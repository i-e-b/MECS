#pragma once

#ifndef heap_h
#define heap_h

// A generic heap based on the vector container
typedef struct Heap Heap;

// Allocate and setup a heap structure with a given size
Heap *HeapAllocate(int elementByteSize);
// Deallocate a heap
void HeapDeallocate(Heap * H);
// Remove all entries without deallocating ( O(n) time )
void HeapClear(Heap * H);
// Add an element ( O(log n) )
void HeapInsert(Heap * H, int priority, void* element);
// Remove the minimum element, returning its value ( O(log n) )
bool HeapDeleteMin(Heap * H, void* element);
// Returning the value of the minimum element ( O(1) )
void HeapPeekMin(Heap* H, void *element);
// Returning the value of the minimum element, testing for its existence first ( O(1) )
bool HeapTryFindMin(Heap* H, void * found);
// Return the value of the second-minimum element, if present ( O(1) )
bool HeapTryFindNext(Heap* H, void * found);
// Returns true if heap has no elements
bool HeapIsEmpty(Heap* H);


#endif
