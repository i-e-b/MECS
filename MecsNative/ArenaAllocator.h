#pragma once

#ifndef arenaallocator_h
#define arenaallocator_h

#define ARENA_ZONE_SIZE 65535

#define sizehelpers
#define KILOBYTES * 1024UL
#define MEGABYTES * 1048576
#define GIGABYTES * 1073741824UL

#define KILOBYTE * 1024UL
#define MEGABYTE * 1048576
#define GIGABYTE * 1073741824UL

/*

    The arena allocator holds a stack of large chunk of memory.
    Each of these large chunks is managed with `malloc` and `free` as normal.

    To get an allocated chunk of memory, we use the arena `Allocate`.
    We can optionally free memory with `Deallocate` when we know it's not going to be used.

    Once we don't need any of the memory anymore, we can close the arena, which deallocates all
    memory contained in it.

    Return values can either be copied out of the closing arena into a different one,
    or be written as produced to another arena.

    At the moment, the maximum allocated chunk size inside an arena is 64K. Use one of the
    container classes to exceed this.

    General layout:

    Real Memory
     |
     +-- Arena
     |    |
     |    +-[data]
     |    |
     |    +-[ list of zones... ]
     |
     +-- Arena
     .
     .
*/

typedef struct Arena Arena;

// Call to push a new arena to the manager stack. Size is the maximum size for the whole
// arena. Fragmentation may make the usable size smaller. Size should be a multiple of ARENA_ZONE_SIZE
Arena* NewArena(size_t size);
// Call to drop an arena, deallocating all memory it contains
void DropArena(Arena** a);

// Copy arena data out to system-level memory. Use this for very long-lived data
void* MakePermanent(void* data, size_t length);

// Copy data from one arena to another. Use this for return values
void* CopyToArena(void* srcData, size_t length, Arena* target);


// Allocate memory of the given size
void* ArenaAllocate(Arena* a, size_t byteCount);

// Remove a reference to memory. When no references are left, the memory is deallocated
bool ArenaDereference(Arena* a, void* ptr);

// Add a reference to memory, to delay deallocation. When no references are left, the memory is deallocated
bool ArenaReference(Arena* a, void* ptr);

// Read statistics for this Arena. Pass `NULL` for anything you're not interested in.
void ArenaGetState(Arena* a, size_t* allocatedBytes, size_t* unallocatedBytes, int* occupiedZones, int* emptyZones, int* totalReferenceCount, size_t* largestContiguous);


#endif