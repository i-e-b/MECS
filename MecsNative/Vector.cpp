#include "Vector.h"
#include <stdlib.h>

typedef struct Vector {
    bool IsValid; // if this is false, creation failed

    // Calculated parts
    int ElemsPerChunk;
    int ElementByteSize;
    int ChunkHeaderSize;
    unsigned short ChunkBytes;

    // dynamic parts
    unsigned int _elementCount;     // how long is the logical array
    unsigned int _baseOffset;       // how many elements should be ignored from first chunk (for de-queueing)
    int  _skipEntries;              // how long is the logical skip table
    bool _skipTableDirty;           // does the skip table need updating?
    bool _rebuilding;               // are we in the middle of rebuilding the skip table?

    // Pointers to data
    // Start of the chunk chain
    void* _baseChunkTable;

    // End of the chunk chain
    void* _endChunkPtr;

    // Pointer to skip table
    void* _skipTable;
} Vector;


#ifndef NULL
#define NULL 0
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef var
#define var auto
#endif

// Fixed sizes -- these are structural to the code and must not change
const int PTR_SIZE = sizeof(void*); // system pointer equivalent
const int INDEX_SIZE = sizeof(unsigned int); // size of index data
const int SKIP_ELEM_SIZE = INDEX_SIZE + PTR_SIZE; // size of skip list entries

// Tuning parameters: have a play if you have performance or memory issues.
const int ARENA_SIZE = 65535; // number of bytes for each chunk (limit)

// Desired maximum elements per chunk. This will be reduced if element is large (to fit in Arena limit)
// Larger values are significantly faster for big arrays, but more memory-wasteful on small arrays
const int TARGET_ELEMS_PER_CHUNK = 64;

// Maximum size of the skip table.
// This is dynamically sizes, so large values won't use extra memory for small arrays.
// This limits the memory growth of larger arrays. If it's bigger than an arena, everything will fail.
const long SKIP_TABLE_SIZE_LIMIT = 1024;

/*
 * Structure of the element chunk:
 *
 * [Ptr to next chunk, or -1]    <- sizeof(void*)
 * [Chunk value (if set)]        <- sizeof(Element)
 * . . .
 * [Chunk value]                 <- ... up to ChunkBytes
 *
 */

 /*
  * Structure of skip table
  *
  * [ChunkIdx]      <-- 4 bytes (uint)
  * [ChunkPtr]      <-- 8 bytes (ptr)
  * . . .
  * [ChunkIdx]
  * [ChunkPtr]
  *
  * Recalculate that after a prealloc, or after a certain number of chunks
  * have been added. This table could be biased, but for simplicity just
  * evenly distribute for now.
  */

// A bunch of little memory helpers
inline void * byteOffset(void *ptr, int byteOffset) {
    char* x = (char*)ptr;
    x += byteOffset;
    return (void*)x;
}
inline uint readUint(void* ptr, int byteOffset) {
    char* x = (char*)ptr;
    x += byteOffset;
    return ((uint*)x)[0];
}
inline void* readPtr(void* ptr, int byteOffset) {
    char* x = (char*)ptr;
    x += byteOffset;
    return ((void**)x)[0];
}
inline void writeUint(void *ptr, int byteOffset, uint data) {
    char* x = (char*)ptr;
    x += byteOffset;
    ((uint*)x)[0] = data;
}
inline void writePtr(void *ptr, int byteOffset, void* data) {
    char* x = (char*)ptr;
    x += byteOffset;
    ((size_t*)x)[0] = (size_t)data;
}
inline void writeValue(void *ptr, int byteOffset, void* data, int length) {
    char* dst = (char*)ptr;
    dst += byteOffset;
    char* src = (char*)data;

    for (int i = 0; i < length; i++) {
        *(dst++) = *(src++);
    }
}

void MaybeRebuildSkipTable(Vector *v); // defined later

// add a new chunk at the end of the chain
void *NewChunk(Vector *v)
{
    auto ptr = calloc(1, v->ChunkBytes); // avoid garbage data in the chunks (help when we pre-alloc)
    if (ptr == NULL) return NULL;

    ((size_t*)ptr)[0] = 0; // set the continuation pointer of the new chunk to invalid
    if (v->_endChunkPtr != NULL) ((size_t*)v->_endChunkPtr)[0] = (size_t)ptr;  // update the continuation pointer of the old end chunk
    v->_endChunkPtr = ptr;                                    // update the end chunk pointer
    v->_skipTableDirty = true; // updating the skip table wouldn't be wasted

    return ptr;
}

void FindNearestChunk(Vector *v, unsigned int targetIndex, bool *found, void **chunkPtr, unsigned int *chunkIndex) {
    // 0. Apply offsets
    uint realIndex = targetIndex + v->_baseOffset;
    uint realElemCount = v->_elementCount + v->_baseOffset;

    // 1. Calculate desired chunk index
    uint targetChunkIdx = (uint)(realIndex / v->ElemsPerChunk);
    uint endChunkIdx = (uint)((realElemCount - 1) / v->ElemsPerChunk);

    // 2. Optimise for start- and end- of chain (small lists & very likely for Push & Pop)
    if (targetChunkIdx == 0)
    { // start of chain
        *found = true;
        *chunkPtr = v->_baseChunkTable;
        *chunkIndex = targetChunkIdx;
        return;
    }
    if (v->_elementCount == 0 || targetChunkIdx == endChunkIdx)
    { // lands in a chunk
        *found = true;
        *chunkPtr = v->_endChunkPtr;
        *chunkIndex = targetChunkIdx;
        return;
    }
    if (targetIndex >= v->_elementCount)
    { // lands outside a chunk -- off the end
        *found = false;
        *chunkPtr = v->_endChunkPtr;
        *chunkIndex = targetChunkIdx;
        return;
    }

    // All the simple optimal paths failed. Make sure the skip list is good...
    MaybeRebuildSkipTable(v);

    // 3. Use the skip table to find a chunk near the target
    //    By ensuring the skip table is fresh, we can calculate the correct location
    uint startChunkIdx = 0;
    void* chunkHeadPtr = v->_baseChunkTable;

    if (v->_skipEntries > 1)
    {
        // guess search bounds
        int guess = (targetChunkIdx * v->_skipEntries) / endChunkIdx;
        int upper = guess + 2;
        int lower = guess - 2;
        if (upper > v->_skipEntries) upper = v->_skipEntries;
        if (lower < 0) lower = 0;

        // binary search for the best chunk
        while (lower < upper) {
            var mid = ((upper - lower) / 2) + lower;
            if (mid == lower) break;

            var midChunkIdx = readUint(v->_skipTable, (SKIP_ELEM_SIZE * mid));
            if (midChunkIdx == targetChunkIdx) break;

            if (midChunkIdx < targetChunkIdx) lower = mid;
            else upper = mid;
        }

        var baseAddr = byteOffset(v->_skipTable, (SKIP_ELEM_SIZE * lower)); // pointer to skip table entry
        startChunkIdx = readUint(baseAddr, 0);
        chunkHeadPtr = readPtr(baseAddr, INDEX_SIZE);
    }

    var walk = targetChunkIdx - startChunkIdx;
    if (walk > 5 && v->_skipEntries < SKIP_TABLE_SIZE_LIMIT) {
        v->_skipTableDirty = true; // if we are walking too far, try builing a better table
    }

    // 4. Walk the chain until we find the chunk we want
    for (; startChunkIdx < targetChunkIdx; startChunkIdx++)
    {
        auto next = readPtr(chunkHeadPtr, 0);
        if (next == NULL) {
            // walk chain failed! We will store the last step in case it's useful.
            *found = false;
            *chunkPtr = chunkHeadPtr;
            *chunkIndex = targetChunkIdx;
            return;
        }
        chunkHeadPtr = next;
    }

    *found = true;
    *chunkPtr = chunkHeadPtr;
    *chunkIndex = targetChunkIdx;
}

void RebuildSkipTable(Vector *v)
{
    v->_rebuilding = true;
    v->_skipTableDirty = false;
    auto chunkTotal = v->_elementCount / v->ElemsPerChunk;
    if (chunkTotal < 4) { // not worth having a skip table
        if (v->_skipTable != NULL) free(v->_skipTable);
        v->_skipEntries = 0;
        v->_skipTable = NULL;
        v->_rebuilding = false;
        return;
    }

    // Guess a reasonable size for the skip table
    auto entries = (chunkTotal < SKIP_TABLE_SIZE_LIMIT) ? chunkTotal : SKIP_TABLE_SIZE_LIMIT;

    // General case: not every chunk will fit in the skip table
    // Find representative chunks using the existing table.
    // (finding will be a combination of search and scan)
    auto newTablePtr = malloc(SKIP_ELEM_SIZE * entries);
    if (newTablePtr == NULL) { v->_rebuilding = false; return; } // live with the old one

    auto stride = v->_elementCount / entries;
    if (stride < 1) stride = 1;

    long target = 0;
    auto newSkipEntries = 0;
    bool found;
    void *chunkPtr;
    unsigned int chunkIndex;

    for (uint i = 0; i < entries; i++) {
        FindNearestChunk(v, target, &found, &chunkPtr, &chunkIndex);

        if (!found || chunkPtr == NULL) { // total fail
            free(newTablePtr);
            v->_rebuilding = false;
            return;
        }

        var iptr = byteOffset(newTablePtr, (SKIP_ELEM_SIZE * i));
        writeUint(iptr, 0, chunkIndex);
        writePtr(iptr, INDEX_SIZE, chunkPtr);
        newSkipEntries++;
        target += stride;
    }

    if (newSkipEntries < 1) {
        free(newTablePtr); // failed to build
        v->_rebuilding = false;
        return;
    }

    v->_skipEntries = newSkipEntries;
    if (v->_skipTable != NULL) free(v->_skipTable);
    v->_skipTable = newTablePtr;
    v->_rebuilding = false;
}

void MaybeRebuildSkipTable(Vector *v) {
    if (v->_rebuilding) return;

    // If we've added a few chunks since last update, then refresh the skip table
    if (v->_skipTableDirty) RebuildSkipTable(v);
}

void * PtrOfElem(Vector *v, uint index) {
    if (index >= v->_elementCount) return NULL;

    var entryIdx = (index + v->_baseOffset) % v->ElemsPerChunk;

    bool found;
    void *chunkPtr = NULL;
    FindNearestChunk(v, index, &found, &chunkPtr, &index);
    if (!found) return NULL;

    return byteOffset(chunkPtr, v->ChunkHeaderSize + (v->ElementByteSize * entryIdx));
}

Vector *VectorAllocate(int elementSize) {
    auto result = (Vector*)calloc(1, sizeof(Vector));

    result->ElementByteSize = elementSize;

    // Work out how many elements can fit in an arena
    result->ChunkHeaderSize = PTR_SIZE;
    auto spaceForElements = ARENA_SIZE - result->ChunkHeaderSize; // need pointer space
    result->ElemsPerChunk = (int)(spaceForElements / result->ElementByteSize);

    if (result->ElemsPerChunk <= 1) {
        result->IsValid = false;
        return result;
    }

    if (result->ElemsPerChunk > TARGET_ELEMS_PER_CHUNK)
        result->ElemsPerChunk = TARGET_ELEMS_PER_CHUNK; // no need to go crazy with small items.

    result->ChunkBytes = (unsigned short)(result->ChunkHeaderSize + (result->ElemsPerChunk * result->ElementByteSize));


    // Make a table, which can store a few chunks, and can have a next-chunk-table pointer
    // Each chunk can hold a few elements.
    result->_skipEntries = NULL;
    result->_skipTable = NULL;
    result->_endChunkPtr = NULL;
    result->_baseChunkTable = NULL;

    auto baseTable = NewChunk(result);

    if (baseTable == NULL) {
        result->IsValid = false;
        return result;
    }
    result->_baseChunkTable = baseTable;
    result->_elementCount = 0;
    result->_baseOffset = 0;
    RebuildSkipTable(result);

    // All done
    result->IsValid = true;
    return result;
}

bool VectorIsValid(Vector *v) {
    if (v == NULL) return false;
    return v->IsValid;
}

void VectorClear(Vector *v) {
    if (v == NULL) return;
    if (v->IsValid == false) return;

    v->_elementCount = 0;
    v->_baseOffset = 0;
    v->_skipEntries = 0;

    // empty out the skip table, if present
    if (v->_skipTable != NULL) {
        free(v->_skipTable);
        v->_skipTable = NULL;
    }

    // Walk through the chunk chain, removing until we hit an invalid pointer
    var current = readPtr(v->_baseChunkTable, 0); // read from *second* chunk, if present
    v->_endChunkPtr = v->_baseChunkTable;
    writePtr(v->_baseChunkTable, 0, NULL); // write a null into the chain link

    while (current != NULL) {
        var next = readPtr(current, 0);
        writePtr(current, 0, NULL); // just in case we have a loop
        free(current);
        current = next;
    }
}

void VectorDeallocate(Vector *v) {
    if (v == NULL) return;
    v->IsValid = false;
    if (v->_skipTable != NULL) free(v->_skipTable);
    v->_skipTable = NULL;
    // Walk through the chunk chain, removing until we hit an invalid pointer
    var current = v->_baseChunkTable;
    v->_baseChunkTable = NULL;
    v->_endChunkPtr = NULL;
    while (true) {
        var next = readPtr(current, 0);
        writePtr(current, 0, NULL); // just in case we have a loop
        free(current);
        if (next == NULL) return; // end of chunks
        current = next;
    }
    free(v);
}

int VectorLength(Vector *v) {
    return v->_elementCount;
}

bool VectorPush(Vector *v, void* value) {
    var entryIdx = (v->_elementCount + v->_baseOffset) % v->ElemsPerChunk;

    bool found;
    void *chunkPtr = NULL;
    uint index;
    FindNearestChunk(v, v->_elementCount, &found, &chunkPtr, &index);
    if (!found) // need a new chunk, write at start
    {
        var ok = NewChunk(v);
        if (ok == NULL) return false;
        writeValue(v->_endChunkPtr, v->ChunkHeaderSize, value, v->ElementByteSize);
        v->_elementCount++;
        return true;
    }
    if (chunkPtr == NULL) return false;

    // Writing value into existing chunk
    writeValue(chunkPtr, v->ChunkHeaderSize + (v->ElementByteSize * entryIdx), value, v->ElementByteSize);
    v->_elementCount++;

    return true;
}

void* VectorGet(Vector *v, unsigned int index) {
    return PtrOfElem(v, index);
}

bool VectorCopy(Vector * v, unsigned int index, void * outValue)
{
    var ptr = PtrOfElem(v, index);
    if (ptr == NULL) return false;

    writeValue(outValue, 0, ptr, v->ElementByteSize);
    return true;
}

bool VectorDequeue(Vector * v, void * outValue) {
    // Special cases:
    if (v == NULL) return false;
    if (!v->IsValid) return false;
    if (v->_elementCount < 1) return false;

    // read the element at index `_baseOffset`, then increment `_baseOffset`.
    auto ptr = byteOffset(v->_baseChunkTable, v->ChunkHeaderSize + (v->_baseOffset * v->ElementByteSize));
    writeValue(outValue, 0, ptr, v->ElementByteSize);
    v->_baseOffset++;
    v->_elementCount--;

    // If there's no clean-up to be done, jump out now.
    if (v->_baseOffset < v->ElemsPerChunk) return true;

    // If `_baseOffset` is equal to chunk length, deallocate the first chunk.
    // When we we deallocate a chunk, copy a trunated version of the skip table
    // if we're on the last chunk, don't deallocate, but just reset the base offset.

    v->_baseOffset = 0;
    auto nextChunk = readPtr(v->_baseChunkTable, 0);
    if (nextChunk == NULL || v->_baseChunkTable == v->_endChunkPtr) { // this is the only chunk
        return true;
    }
    // Advance the base and free the old
    auto oldChunk = v->_baseChunkTable;
    v->_baseChunkTable = nextChunk;
    free(oldChunk);

    if (v->_skipTable == NULL) return true; // don't need to fix the table

    // Make a truncated version of the skip table
    v->_skipEntries--;
    if (v->_skipEntries < 4) { // no point having a table
        free(v->_skipTable);
        v->_skipEntries = 0;
        v->_skipTable = NULL;
        return true;
    }
    // Copy of the skip table with first element gone
    uint length = SKIP_ELEM_SIZE * v->_skipEntries;
    auto newTablePtr = (char*)malloc(length);
    auto oldTablePtr = (char*)v->_skipTable;
    for (uint i = 0; i < length; i++) {
        newTablePtr[i] = oldTablePtr[i + SKIP_ELEM_SIZE];
    }
    v->_skipTable = newTablePtr;
    free(oldTablePtr);

    return true;
}

bool VectorPop(Vector *v, void *target) {
    if (v->_elementCount == 0) return false;

    var index = v->_elementCount - 1;
    var entryIdx = (index + v->_baseOffset) % v->ElemsPerChunk;

    // Get the value
    var result = byteOffset(v->_endChunkPtr, v->ChunkHeaderSize + (v->ElementByteSize * entryIdx));
    if (result == NULL) return false;
    if (target != NULL) { // need to copy element, as we might dealloc the chunk it lives in
        writeValue(target, 0, result, v->ElementByteSize);
    }

    // Clean up if we've emptied a chunk that isn't the initial one
    if (entryIdx < 1 && v->_elementCount > 0) {
        // need to dealloc end chunk
        bool found;
        void *prevChunkPtr = NULL;
        uint deadChunkIdx = 0;
        FindNearestChunk(v, index - 1, &found, &prevChunkPtr, &deadChunkIdx);
        if (!found || prevChunkPtr == v->_endChunkPtr) {
            // damaged references!
            v->IsValid = false;
            return false;
        }
        free(v->_endChunkPtr);
        v->_endChunkPtr = prevChunkPtr;
        writePtr(prevChunkPtr, 0, NULL); // remove the 'next' pointer from the new end chunk

        if (v->_skipEntries > 0) {
            // Check to see if we've made the skip list invalid
            var skipTableEnd = readUint(v->_skipTable, SKIP_ELEM_SIZE * (v->_skipEntries - 1));

            // knock the last element off if it's too big. 
            // The walk limit in FindNearestChunk set the dirty flag if needed
            if (skipTableEnd >= deadChunkIdx) {
                v->_skipEntries--;
            }
        }
    }

    v->_elementCount--;
    return true;
}

bool VectorSet(Vector *v, unsigned int index, void* element, void* prevValue) {
    // push in the value, returning previous value
    var ptr = PtrOfElem(v, index);
    if (ptr == NULL) return false;

    if (prevValue != NULL) {
        writeValue(prevValue, 0, ptr, v->ElementByteSize);
    }

    writeValue(ptr, 0, element, v->ElementByteSize);
    return true;
}

bool VectorPrealloc(Vector *v, unsigned int length) {
    var remain = length - v->_elementCount;
    if (remain < 1) return true;

    var newChunkIdx = length / v->ElemsPerChunk;

    // Walk through the chunk chain, adding where needed
    var chunkHeadPtr = v->_baseChunkTable;
    for (int i = 0; i < newChunkIdx; i++)
    {
        var nextChunkPtr = readPtr(chunkHeadPtr, 0);
        if (nextChunkPtr == NULL) {
            // need to alloc a new chunk
            nextChunkPtr = NewChunk(v);
            if (nextChunkPtr == NULL) return false;
        }
        chunkHeadPtr = nextChunkPtr;
    }

    v->_elementCount = length;

    RebuildSkipTable(v); // make sure we're up to date

    return true;
}

bool VectorSwap(Vector *v, unsigned int index1, unsigned int index2) {
    var A = PtrOfElem(v, index1);
    var B = PtrOfElem(v, index2);

    if (A == NULL || B == NULL) return false;

    var bytes = v->ElementByteSize;
    var tmp = malloc(bytes);
    if (tmp == NULL) return false;

    writeValue(tmp, 0, A, bytes); // tmp = A
    writeValue(A, 0, B, bytes); // A = B
    writeValue(B, 0, tmp, bytes); // B = tmp

    free(tmp);

    return true;
}
