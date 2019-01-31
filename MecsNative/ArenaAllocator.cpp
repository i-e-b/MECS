#include "ArenaAllocator.h"
#include "Vector.h"

#include "RawData.h"

#include <stdlib.h>
#include <stdint.h>


#define ZONE_MAX_REFS 65000

typedef struct Arena {
    // Bottom of free memory (after arena management is taken up)
    void* _start;

    // Top of memory
    void* _limit;

    // Pointer to array of ushort, length is equal to _arenaCount.
    // Each element is offset of next pointer to allocate. Zero indicates an empty arena.
    // This is also the base of allocated memory
    uint16_t* _headsPtr;

    // Pointer to array of ushort, length is equal to _arenaCount.
    // Each element is number of references claimed against the arena.
    uint16_t* _refCountsPtr;

    // The most recent arena that had a successful alloc or clear
    int _currentArena;

    // Count of available arenas. This is the limit of memory
    int _arenaCount;

    // Index in the memory manager of this arena
    int _mgrIndex;
} Arena;

// Call to push a new arena to the manager stack. Size is the maximum size for the whole
// arena. Fragmentation may make the usable size smaller. Size should be a multiple of ARENA_ZONE_SIZE
Arena* NewArena(size_t size) {
    auto realMemory = calloc(1, size);
    if (realMemory == NULL) return NULL;

    auto result = (Arena*)calloc(1, sizeof(Arena));
    if (result == NULL) {
        free(realMemory);
        return NULL;
    }

    result->_start = realMemory;
    result->_limit = byteOffset(realMemory, size - 1);

    auto sizeOfTables = sizeof(uint16_t) * result->_arenaCount;
    auto availableBytes = ((size_t)(result->_limit) - (size_t)(result->_start)) - (2 * sizeOfTables);

    // with 64KB arenas (ushort) and 1GB of RAM, we get 16384 arenas.
    // recording only heads and refs would take 64KB of management space
    // recording heads, refs and back-step (an optimisation for very short-lived items) would use 96KB of management space.
    // This seems pretty reasonable.
    result->_arenaCount = (int)(availableBytes / ARENA_ZONE_SIZE);
    result->_currentArena = 0;

    // Allow space for arena tables, store adjusted base
    result->_headsPtr = (uint16_t*)result->_start;
    result->_refCountsPtr = (uint16_t*)byteOffset(result->_start, sizeOfTables);

    // shrink space for headers
    result->_start = byteOffset(result->_start, sizeOfTables * 2);

    // zero-out the tables
    auto zptr = result->_headsPtr;
    while (zptr < result->_start) {
        writeUshort(zptr, 0, 0);
        zptr += sizeof(uint16_t);
    }

    return result;
}

// Call to drop an arena, deallocating all memory it contains
void DropArena(Arena** a) {
    if (a == NULL) return;

    auto ptr = *a;
    if (ptr == NULL) return;

    if (ptr->_headsPtr != NULL) { // delete contained memory
        free(ptr->_headsPtr);
        ptr->_headsPtr = NULL;
        ptr->_start = NULL;
        ptr->_limit = NULL;
    }

    free(*a); // Free the arena reference itself
    *a = NULL; // kill the arena reference
}

// Copy arena data out to system-level memory. Use this for very long-lived data
void* MakePermanent(void* data, size_t length) {
    if (length < 1) return NULL;
    if (data == NULL) return NULL;

    void* perm = malloc(length);
    if (perm == NULL) return NULL;

    copyAnonArray(perm, 0, data, 0, length);

    return NULL;
}

// Copy data from one arena to another. Use this for return values
void* CopyToArena(void* srcData, size_t length, Arena* target) {
    if (srcData == NULL) return NULL;
    if (length < 1) return NULL;
    if (target == NULL) return NULL;

    auto dstData = Allocate(target, length);
    if (dstData == NULL) return NULL;

    copyAnonArray(dstData, 0, srcData, 0, length);

    return NULL;
}


uint16_t GetHead(Arena* a, int arenaIndex) {
    return readUshort(a->_headsPtr, arenaIndex * sizeof(uint16_t));
}
uint16_t GetRefCount(Arena* a, int arenaIndex) {
    return readUshort(a->_refCountsPtr, arenaIndex * sizeof(uint16_t));
}
void SetHead(Arena* a, int arenaIndex, uint16_t val) {
    writeUshort(a->_headsPtr, arenaIndex * sizeof(uint16_t), val);
}
void SetRefCount(Arena* a, int arenaIndex, uint16_t val) {
    writeUshort(a->_refCountsPtr, arenaIndex * sizeof(uint16_t), val);
}

// Allocate memory of the given size
void* Allocate(Arena* a, size_t byteCount) {
    if (byteCount > ARENA_ZONE_SIZE) return NULL; // Invalid allocation -- beyond max size.
    if (a == NULL) return NULL;

    auto maxOff = ARENA_ZONE_SIZE - byteCount;
    auto arenaCount = a->_arenaCount;

    // scan for first arena where there is enough room
    // we can either start from scratch each time, start from last success, or last emptied
    for (int seq = 0; seq < arenaCount; seq++) {
        auto i = (seq + a->_currentArena) % arenaCount; // simple scan from last active, looping back if needed

        if (GetHead(a, i) > maxOff) continue; // no room in this slot

        // found a slot where it will fit
        a->_currentArena = i;
        uint16_t result = GetHead(a, i); // new pointer
        SetHead(a, i, result + byteCount); // advance pointer to end of allocated data

        auto oldRefs = GetRefCount(a, i);
        SetRefCount(a, i, oldRefs + 1); // increase arena ref count

        return byteOffset(a->_start, result + (i * ARENA_ZONE_SIZE)); // turn the offset into an absolute position
    }

    // found nothing -- out of memory!
    return NULL;
}


int ZoneForPtr(Arena* a, void* ptr) {
    if (ptr < a->_start || ptr > a->_limit) return -1;

    ptrdiff_t rawOffset = (ptrdiff_t)ptr - (ptrdiff_t)a->_start;
    ptrdiff_t arena = rawOffset / ARENA_ZONE_SIZE;
    if (arena < 0 || arena >= a->_arenaCount) return -1;
    return (int)arena;
}

// Remove a reference to memory. When no references are left, the memory is deallocated
bool Dereference(Arena* a, void* ptr) {
    if (a == NULL) return false;
    if (ptr == NULL) return false;

    auto zone = ZoneForPtr(a, ptr);
    if (zone < 0) return false;

    auto refCount = GetRefCount(a, zone);
    if (refCount == 0) return false; // Overfree. Fix your code.

    refCount--;
    SetRefCount(a, zone, refCount);

    // If no more references, free the block
    if (refCount == 0) {
        SetHead(a, zone, 0);
        if (zone < a->_currentArena) a->_currentArena = zone; // keep allocations packed in low memory. Is this worth it?
    }
    return true;
}

// Add a reference to memory, to delay deallocation. When no references are left, the memory is deallocated
bool Reference(Arena* a, void* ptr) {
    if (a == NULL) return false;
    if (ptr == NULL) return false;

    auto zone = ZoneForPtr(a, ptr);
    if (zone < 0) return false;

    auto oldRefs = GetRefCount(a, zone);
    if (oldRefs >= ZONE_MAX_REFS) return false; // saturated references. Fix your code.

    SetRefCount(a, zone, oldRefs + 1);
    return true;
}

// Read statistics for this Arena. Pass `NULL` for anything you're not interested in.
void GetState(Arena* a, size_t* allocatedBytes, size_t* unallocatedBytes,
    int* occupiedZones, int* emptyZones, int* totalReferenceCount, size_t* largestContiguous) {

    size_t allocated = 0;
    size_t unallocated = 0;
    int occupied = 0;
    int empty = 0;
    int totalReferences = 0;
    size_t largestFree = 0;

    auto zoneCount = a->_arenaCount;

    for (int i = 0; i < zoneCount; i++) {
        auto arenaRefCount = GetRefCount(a, i);
        auto arenaHead = GetHead(a, i);
        totalReferences += arenaRefCount;

        if (arenaHead > 0) occupied++;
        else empty++;

        auto free = ARENA_ZONE_SIZE - arenaHead;
        allocated += arenaHead;
        unallocated += free;
        if (free > largestFree) largestFree = free;
    }

    if (allocatedBytes != NULL) *allocatedBytes = allocated;
    if (unallocatedBytes != NULL) *unallocatedBytes = unallocated;
    if (occupiedZones != NULL) *occupiedZones = occupied;
    if (emptyZones != NULL) *emptyZones = empty;
    if (totalReferenceCount != NULL) *totalReferenceCount = totalReferences;
    if (largestContiguous != NULL) *largestContiguous = largestFree;
}
