#pragma once

#ifndef heap_h
#define heap_h

#include "ArenaAllocator.h"

// A generic heap based on the vector container
typedef struct Heap Heap;
typedef Heap* HeapPtr;

// Allocate and setup a heap structure with a given size
Heap *HeapAllocate(ArenaPtr arena, int elementByteSize);
// Deallocate a heap
void HeapDeallocate(Heap * H);
// Remove all entries without deallocating ( O(n) time )
void HeapClear(Heap * H);
// Add an element ( O(log n) )
// Data from the element is copied into the heap, and can be disposed by the caller
void HeapInsert(Heap * H, int priority, void* element);
// Remove the minimum element, returning its value ( O(log n) )
// Data is copied into the element from the heap. If the element passed is NULL, the delete still happens, but the result is discarded
bool HeapDeleteMin(Heap * H, void* element);
// Returns a pointer to the value of the minimum element. Returns NULL if empty. Does not copy. ( O(1) )
void* HeapPeekMin(Heap* H);
// Copy the value of the minimum element, testing for its existence first ( O(1) )
bool HeapTryFindMin(Heap* H, void * found);
// Return the value of the second-minimum element, if present ( O(1) )
bool HeapTryFindNext(Heap* H, void * found);
// Returns true if heap has no elements
bool HeapIsEmpty(Heap* H);


// Macros to create type-specific versions of the methods above.
// If you want to use the typed versions, make sure you call `RegisterTreeFor(typeName, namespace)` for EACH type

// These are invariant on type, but can be namespaced
#define RegisterHeapStatics(nameSpace) \
    inline void nameSpace##Deallocate(Heap * H){ HeapDeallocate(H); }\
    inline void nameSpace##Clear(Heap * H){ HeapClear(H); }\
    inline bool nameSpace##IsEmpty(Heap * H){ return HeapIsEmpty(H); }\

// These must be registered for each distinct pair, as they are type variant
#define RegisterHeapFor(elemType, nameSpace) \
    inline Heap * nameSpace##Allocate_##elemType(ArenaPtr arena){return HeapAllocate(arena, sizeof(elemType));}\
    inline void nameSpace##Insert_##elemType(Heap* h, int priority, elemType* element){ HeapInsert(h,priority,element);}\
    inline void nameSpace##Insert_##elemType(Heap* h, int priority, const elemType* element){ HeapInsert(h,priority,(void*)element);}\
    inline bool nameSpace##DeleteMin_##elemType(Heap * H, elemType* element){return HeapDeleteMin(H, element);}\
    inline elemType* nameSpace##PeekMin_##elemType(Heap * H){ return (elemType*)HeapPeekMin(H);}\
    inline bool nameSpace##TryFindMin_##elemType(Heap * H, elemType* element){return HeapTryFindMin(H, element);}\
    inline bool nameSpace##TryFindNext_##elemType(Heap * H, elemType* element){return HeapTryFindNext(H,element);}\


#endif
