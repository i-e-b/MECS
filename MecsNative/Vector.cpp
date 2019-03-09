#include "Vector.h"
#include "MemoryManager.h"

#include "RawData.h"

#include <stdint.h>

typedef struct Vector {
    bool IsValid; // if this is false, creation failed

    Arena* _arena; // the arena this vector should be pinned to (the active one when the vector was created)

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
#define uint uint32_t
#endif
#ifndef var
#define var auto
#endif

// Fixed sizes -- these are structural to the code and must not change
const int PTR_SIZE = sizeof(void*); // system pointer equivalent
const int INDEX_SIZE = sizeof(unsigned int); // size of index data
const int SKIP_ELEM_SIZE = INDEX_SIZE + PTR_SIZE; // size of skip list entries

// Tuning parameters: have a play if you have performance or memory issues.
const int ARENA_SIZE = 65535; // number of bytes for each chunk (limit -- should match any restriction in the allocator)

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

void MaybeRebuildSkipTable(Vector *v); // defined later

// abstract over alloc/free to help us pin to one arena
void* VecCAlloc(Vector *v, int count, int size) {
    if (v->_arena == NULL) return mcalloc(count, size);
    return ArenaAllocateAndClear(v->_arena, count*size);
}
void VecFree(Vector *v, void* ptr) {
    if (v->_arena == NULL) mfree(ptr);
    else ArenaDereference(v->_arena, ptr);
}
void* VecAlloc(Vector *v, int size) {
    if (v->_arena == NULL) return mmalloc(size);
    return ArenaAllocate(v->_arena,size);
}

// add a new chunk at the end of the chain
void *NewChunk(Vector *v) {
    auto ptr = VecCAlloc(v, 1, v->ChunkBytes); // calloc to avoid garbage data in the chunks
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
    { // lands on end-of-chain
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
        if (v->_skipTable != NULL) VecFree(v, v->_skipTable);
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
    auto newTablePtr = VecAlloc(v, SKIP_ELEM_SIZE * entries);
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
            VecFree(v,newTablePtr);
            v->_rebuilding = false;
            return;
        }

        var iptr = byteOffset(newTablePtr, (SKIP_ELEM_SIZE * i)); // 443
        writeUint(iptr, 0, chunkIndex);
        writePtr(iptr, INDEX_SIZE, chunkPtr); // TODO: !!! This fails at large sizes!
        newSkipEntries++;
        target += stride;
    }

    if (newSkipEntries < 1) {
        VecFree(v, newTablePtr); // failed to build
        v->_rebuilding = false;
        return;
    }

    v->_skipEntries = newSkipEntries;
    if (v->_skipTable != NULL) VecFree(v, v->_skipTable);
    v->_skipTable = newTablePtr;
    v->_rebuilding = false;
}

void MaybeRebuildSkipTable(Vector *v) {
    if (v->_rebuilding) return;

    // If we've added a few chunks since last update, then refresh the skip table
    if (v->_skipTableDirty) RebuildSkipTable(v);
}

void * PtrOfElem(Vector *v, uint index) {
    if (v == NULL) return NULL;
    if (index >= v->_elementCount) return NULL;

    var entryIdx = (index + v->_baseOffset) % v->ElemsPerChunk;

    bool found;
    void *chunkPtr = NULL;
    FindNearestChunk(v, index, &found, &chunkPtr, &index);
    if (!found) return NULL;

    return byteOffset(chunkPtr, v->ChunkHeaderSize + (v->ElementByteSize * entryIdx));
}

Vector *VectorAllocate(int elementSize) {
    auto result = (Vector*)mcalloc(1, sizeof(Vector));
    if (result == NULL) return NULL;

    result->_arena = MMCurrent();
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

// Create a new dynamic vector with the given element size (must be fixed per vector) in a specific memory arena
Vector *VectorAllocateArena(Arena* a, int elementSize) {
    if (a == NULL) return NULL;
    auto result = (Vector*)ArenaAllocateAndClear(a, sizeof(Vector));//(Vector*)mcalloc(1, sizeof(Vector));

    result->_arena = a;
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
        VecFree(v, v->_skipTable);
        v->_skipTable = NULL;
    }

    // Walk through the chunk chain, removing until we hit an invalid pointer
    var current = readPtr(v->_baseChunkTable, 0); // read from *second* chunk, if present
    v->_endChunkPtr = v->_baseChunkTable;
    writePtr(v->_baseChunkTable, 0, NULL); // write a null into the chain link

    while (current != NULL) {
        var next = readPtr(current, 0);
        writePtr(current, 0, NULL); // just in case we have a loop
        VecFree(v, current);
        current = next;
    }
}

void VectorDeallocate(Vector *v) {
    if (v == NULL) return;
    v->IsValid = false;
    if (v->_skipTable != NULL) VecFree(v, v->_skipTable);
    v->_skipTable = NULL;
    // Walk through the chunk chain, removing until we hit an invalid pointer
    var current = v->_baseChunkTable;
    while (true) {
        if (current == NULL) break; // end of chunks

        var next = readPtr(current, 0);
        writePtr(current, 0, NULL); // just in case we have a loop
        VecFree(v, current);

        if (current == v->_endChunkPtr) break; // sentinel
        current = next;
    }
    v->_baseChunkTable = NULL;
    v->_endChunkPtr = NULL;
    v->_elementCount = 0;
    v->ElementByteSize = 0;
    v->ElemsPerChunk = 0;

    // arena aware clean-up
    auto a = v->_arena;
    if (a == NULL) {
        mfree(v);
    } else {
        ArenaDereference(a, v);
    }
}

int VectorLength(Vector *v) {
    if (v == NULL) return 0;
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
    if (outValue != NULL) {
        auto ptr = byteOffset(v->_baseChunkTable, v->ChunkHeaderSize + (v->_baseOffset * v->ElementByteSize));
        writeValue(outValue, 0, ptr, v->ElementByteSize);
    }
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
    VecFree(v, oldChunk);

    if (v->_skipTable == NULL) return true; // don't need to fix the table

    // Make a truncated version of the skip table
    v->_skipEntries--;
    if (v->_skipEntries < 4) { // no point having a table
        VecFree(v, v->_skipTable);
        v->_skipEntries = 0;
        v->_skipTable = NULL;
        return true;
    }
    // Copy of the skip table with first element gone
    uint length = SKIP_ELEM_SIZE * v->_skipEntries;
    auto newTablePtr = (char*)VecAlloc(v, length);
    auto oldTablePtr = (char*)v->_skipTable;
    for (uint i = 0; i < length; i++) {
        newTablePtr[i] = oldTablePtr[i + SKIP_ELEM_SIZE];
    }
    v->_skipTable = newTablePtr;
    VecFree(v, oldTablePtr);

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
    if (entryIdx < 1 && v->_elementCount > 1) {
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
        VecFree(v, v->_endChunkPtr);
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

bool VectorPeek(Vector *v, void* target) {
    if (v->_elementCount == 0) return false;

    var index = v->_elementCount - 1;
    var entryIdx = (index + v->_baseOffset) % v->ElemsPerChunk;

    // Get the value
    var result = byteOffset(v->_endChunkPtr, v->ChunkHeaderSize + (v->ElementByteSize * entryIdx));
    if (result == NULL) return false;
    if (target != NULL) {
        writeValue(target, 0, result, v->ElementByteSize);
    }
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
    var tmp = VecAlloc(v, bytes);
    if (tmp == NULL) return false;

    writeValue(tmp, 0, A, bytes); // tmp = A
    writeValue(A, 0, B, bytes); // A = B
    writeValue(B, 0, tmp, bytes); // B = tmp

    VecFree(v, tmp);

    return true;
}

// Merge with minimal copies. Input values should be in arr1. Returns the array that the final result is in.
// The input array should have had the first set of swaps done (partition size = 1)
// Compare should return 0 if the two values are equal, negative if A should be before B, and positive if B should be before A.
void* iterativeMergeSort(void* arr1, void* arr2, int n, int elemSize, int(*compareFunc)(void* A, void* B)) {
    bool anySwaps = false;

    auto A = arr2; // we will be flipping the array pointers around
    auto B = arr1;

    for (int stride = 2; stride < n; stride *= 2) { // doubling merge width

        // swap A and B pointers after each merge set
        { auto tmp = A; A = B; B = tmp; }

        int t = 0; // incrementing point in target array
        for (int left = 0; left < n; left += 2 * stride) {
            int right = left + stride;
            int end = right + stride;
            if (end > n) end = n; // some merge windows will run off the end of the data array
            if (right > n) right = n; // some merge windows will run off the end of the data array
            int l = left, r = right; // the point we are scanning though the two sets to be merged.

            // copy the lowest candidate across from A to B
            while (l < right && r < end) {
                int lower = (compareFunc(byteOffset(A, l * elemSize), byteOffset(A, r * elemSize)) < 0)
                          ? (l++) : (r++);
                copyAnonArray(B, t++, A, lower, elemSize); // B[t++] = A[lower];
            } // exhausted at least one of the merge sides

            while (l < right) { // run down left if anything remains
                copyAnonArray(B, t++, A, l++, elemSize); // B[t++] = A[l++];
            }

            while (r < end) { // run down right side if anything remains
                copyAnonArray(B, t++, A, r++, elemSize); // B[t++] = A[r++];
            }
        }
    }

    // let the caller know which array has the result
    return B;
}

void VectorSort(Vector *v, int(*compareFunc)(void* A, void* B)) {
    // Available Plans:

    // A - normal merge: (extra space ~N + chunksize)
    // 1. in each chunk, run the simple array-based merge-sort
    // 2. merge-sort between pairs of chunks
    // 3. keep merging pairs into longer chains until all sorted
    //
    // B - tide-front (insertion sort) (extra space ~N)
    // 1. in each chunk, run the simple array-based sort
    // 2. build a 'tide' array, one element per chunk
    // 3. pick the lowest element in the array, and replace with next element from that chunk
    // 4. repeat until all empty
    //
    // C - compare-exchange (extra space ~chunksize)
    // 1. each chunk merged
    // 2. we pick a chunk and merge it in turn with each other chunk (so one has values always smaller than other)
    // 3. repeat until sorted (see notebook)
    //
    // D - empty and re-fill (extra space ~2N) -- the optimised nature of push/pop might make this quite good.
    // 1. make 2 arrays big enough for the vector
    // 2. empty the vector into one of them (can do this as first part of sort [2-pairs])
    // 3. sort normally
    // 4. push back into vector
    //
    // E - proxy sort (extra space ~N)
    // 1. Change the compare function to a 'make-ordinal'
    // 2. Write a proxy array of current position and ordinal
    // 3. Sort that
    // 4. swap elements in the vector to match

    // Going to start with 'D' as a base-line

    // Allocate the temporary structures
    uint32_t n = VectorLength(v);
    auto size = v->ElementByteSize;
    void* arr1 = VecAlloc(v, (n+1) * size); // extra space for swapping
    if (arr1 == NULL) return;
    void* arr2 = VecAlloc(v, n * size);
    if (arr2 == NULL) { VecFree(v, arr1); return; }

    // write into array in pairs, doing the scale=1 merge
    uint32_t i = 0;
    while (true) {
        void* a = byteOffset(arr1, i * size);
        void* b = byteOffset(arr1, (i+1) * size);
        bool _a = VectorDequeue(v, a);
        bool _b = VectorDequeue(v, b);

        if (_a && _b) { // wrote 2: compare and swap if required
            if (compareFunc(a, b) > 0) swapMem(a, b, size);
        } else { // empty vector
            break;
        }
        i += 2;
    }

    // do the sort
    auto result = iterativeMergeSort(arr1, arr2, n, size, compareFunc);

    // push the result back into the vector
    for (i = 0; i < n; i++) {
        void* src = byteOffset(result, i * size);
        VectorPush(v, src);
    }

    // clean up
    VecFree(v, arr1);
    VecFree(v, arr2);
}

int VectorElementSize(Vector * v) {
    if (v == NULL) return 0;
    return v->ElementByteSize;
}
