#pragma once

#ifndef arenaallocator_h
#define arenaallocator_h

#include <stdint.h>

// Maximum size of a single allocation
#define ARENA_ZONE_SIZE 65535

#define KILOBYTES * 1024UL
#define MEGABYTES * 1048576
#define GIGABYTES * 1073741824UL

#define KILOBYTE * 1024UL
#define MEGABYTE * 1048576
#define GIGABYTE * 1073741824UL

typedef struct Arena Arena;

// Create a new arena for memory management. Size is the maximum size for the whole
// arena. Fragmentation may make the usable size smaller. Size should be a multiple of ARENA_ZONE_SIZE
Arena* NewArena(size_t size);

// Call to drop an arena, deallocating all memory it contains
void DropArena(Arena** a);

// Copy arena data out to system-level memory. Use this for very long-lived data
void* MakePermanent(void* data, size_t length);

// Copy data from one arena to another. Use this for return values
void* CopyToArena(void* srcData, size_t length, Arena* target);

// Returns true if the given pointer is managed by this arena
bool ArenaContainsPointer(Arena* a, void* ptr);

// Allocate memory of the given size
void* ArenaAllocate(Arena* a, size_t byteCount);

// Allocate memory of the given size and set all bytes to zero
void* ArenaAllocateAndClear(Arena* a, size_t byteCount);

// Remove a reference to memory. When no references are left, the memory is deallocated
bool ArenaDereference(Arena* a, void* ptr);

// Add a reference to memory, to delay deallocation. When no references are left, the memory is deallocated
bool ArenaReference(Arena* a, void* ptr);

// Get an offset into the arena for a pointer to memory. Zero is NOT a valid offset value.
uint32_t ArenaPtrToOffset(Arena* a, void* ptr);

// Get a raw memory pointer from an offset into an arena. Zero is NOT a valid offset value.
void* ArenaOffsetToPtr(Arena* a, uint32_t offset);

// Read statistics for this Arena. Pass `NULL` for anything you're not interested in.
void ArenaGetState(Arena* a, size_t* allocatedBytes, size_t* unallocatedBytes, int* occupiedZones, int* emptyZones, int* totalReferenceCount, size_t* largestContiguous);


#endif