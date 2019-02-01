#pragma once

#ifndef memorymanager_h
#define memorymanager_h

#include "ArenaAllocator.h"

/*
    Exposes replacements for malloc, calloc and free.
    Maintains a stack of memory arenas, where allocations and deallocations can be made; and all deallocated at once.

    Uses the most recently pushed Arena.
    Uses the stdlib versions if no areas have been pushed, or if not set up.
    When an arena is popped from the manager, it is deallocated
*/

// Ensure the memory manager is ready. It starts with an empty stack
void StartManagedMemory();
// Close all arenas and return to stdlib memory
void ShutdownManagedMemory();

// Start a new arena, keeping memory and state of any existing ones
bool MMPush(size_t arenaMemory);

// Deallocate the most recent arena, restoring the previous
void MMPop();

// Deallocate the most recent arena, copying a data item to the next one down (or permanent memory if at the bottom of the stack)
void* MMPopReturn(void* ptr, size_t size);

// Return the current arena, or NULL if none pushed
Arena* MMCurrent();

// Allocate memory
void* mmalloc(size_t size);

// Allocate memory array, cleared to zeros
void* mcalloc(int count, size_t size);

// Free memory
void mfree(void* ptr);


#endif