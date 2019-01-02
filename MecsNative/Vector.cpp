#include "Vector.h"
#include <stdlib.h>

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

// Desired maximum elements per chunk. This will be reduced if TElement is large (to fit in Arena limit)
// Larger values are significantly faster for big arrays, but more memory-wasteful on small arrays
const int TARGET_ELEMS_PER_CHUNK = 64;

// Maximum size of the skip table.
// This is dynamically sizes, so large values won't use extra memory for small arrays.
// This limits the memory growth of larger arrays. If it's bigger that an arena, everything will fail.
const long SKIP_TABLE_SIZE_LIMIT = 1024;


/*
 * Structure of the element chunk:
 *
 * [Ptr to next chunk, or -1]    <- 8 bytes
 * [Chunk value (if set)]        <- sizeof(TElement)
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

void MaybeRebuildSkipTable(Vector *v); // defined later

// add a new chunk at the end of the chain
void *NewChunk(Vector *v)
{
    auto ptr = malloc(v->ChunkBytes);
    if (ptr == NULL) return NULL;

    ((size_t*)ptr)[0] = 0; // set the continuation pointer of the new chunk to invalid
    if (v->_endChunkPtr != NULL) ((size_t*)v->_endChunkPtr)[0] = (size_t)ptr;  // update the continuation pointer of the old end chunk
    v->_endChunkPtr = ptr;                                    // update the end chunk pointer
    v->_skipTableDirty = true; // updating the skip table wouldn't be wasted

    return ptr;
}


void FindNearestChunk(Vector *v, unsigned int targetIndex, bool *found, void **chunkPtr, unsigned int *chunkIndex) {
    // 1. Calculate desired chunk index
    uint targetChunkIdx = (uint)(targetIndex / v->ElemsPerChunk);
    uint endChunkIdx = (uint)((v->_elementCount - 1) / v->ElemsPerChunk);

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
        var guess = (targetChunkIdx * v->_skipEntries) / endChunkIdx;
        var upper = guess + 2;
        var lower = guess - 2;
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
        chunkHeadPtr = readPtr(chunkHeadPtr, 0);
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

    for (int i = 0; i < entries; i++) {
        FindNearestChunk(v, target, &found, &chunkPtr, &chunkIndex);

        if (!found || chunkPtr < 0) // total fail
        {
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

    if (newSkipEntries < 1) // total fail
    {
        free(newTablePtr);
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

Vector VectorAllocate(int elementSize)
{
    Vector result = Vector{};

    result.ElementByteSize = elementSize;

    // Work out how many elements can fit in an arena
    result.ChunkHeaderSize = PTR_SIZE;
    auto spaceForElements = ARENA_SIZE - result.ChunkHeaderSize; // need pointer space
    result.ElemsPerChunk = (int)(spaceForElements / result.ElementByteSize);

    if (result.ElemsPerChunk <= 1) {
        result.IsValid = false;
        return result;
    }

    if (result.ElemsPerChunk > TARGET_ELEMS_PER_CHUNK)
        result.ElemsPerChunk = TARGET_ELEMS_PER_CHUNK; // no need to go crazy with small items.

    result.ChunkBytes = (unsigned short)(result.ChunkHeaderSize + (result.ElemsPerChunk * result.ElementByteSize));


    // Make a table, which can store a few chunks, and can have a next-chunk-table pointer
    // Each chunk can hold a few elements.
    result._skipEntries = NULL;
    result._skipTable = NULL;
    result._endChunkPtr = NULL;
    result._baseChunkTable = NULL;

    auto baseTable = NewChunk(&result);

    if (baseTable == NULL) {
        result.IsValid = false;
        return result;
    }
    result._baseChunkTable = baseTable;
    result._elementCount = 0;
    RebuildSkipTable(&result);

    // All done
    result.IsValid = true;
    return result;
}

void VectorDeallocate(Vector *v) {
    v->IsValid = false;
    free(v->_skipTable);
    v->_skipTable = NULL;
    // Walk through the chunk chain, removing until we hit an invalid pointer
    var current = v->_baseChunkTable;
    v->_baseChunkTable = NULL;
    v->_endChunkPtr = NULL;
    while (true) {
        var next = readPtr(current, 0);
        free(current);
        writePtr(current, 0, NULL); // just in case we have a loop
        if (next == NULL) return; // end of chunks
        current = next;
    }
}
/*
int VectorLength(Vector *v);
bool VectorPush(Vector *v, void* value);
void* VectorGet(Vector *v, unsigned int index);
void* VectorPop(Vector *v);
void* VectorSet(Vector *v, unsigned int index, void* element);
void VectorPrealloc(Vector *v, unsigned int length);
bool VectorSwap(Vector *v, unsigned int index1, unsigned int index2);
*/