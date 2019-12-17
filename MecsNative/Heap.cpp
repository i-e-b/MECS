#include "Heap.h"
#include "Vector.h"

#include "RawData.h"

#include <stdint.h>


typedef struct Heap {
    Vector *Elements;
    int elementSize;
} Heap;

Heap * HeapAllocate(ArenaPtr arena, int elementByteSize) {
    if (arena == NULL || elementByteSize < 1 || elementByteSize >= ARENA_ZONE_SIZE) return NULL;

    auto vec = VectorAllocateArena(arena, elementByteSize + sizeof(int)); // int for priority, stored inline
    if (vec == NULL) return NULL;

    Heap *h = (Heap*)ArenaAllocate(arena, sizeof(Heap));
    if (h == NULL) {
        VectorDeallocate(vec);
        return NULL;
    }
    h->Elements = vec;
    h->elementSize = elementByteSize;

    HeapClear(h);

    return h;
}

void HeapDeallocate(Heap * H) {
    if (H == NULL) return;
    auto arena = VectorArena(H->Elements);
    VectorDeallocate(H->Elements);
    ArenaDereference(arena, H);    
}

void HeapClear(Heap * H) {
    if (H == NULL) return;
    VectorClear(H->Elements);
    auto arena = VectorArena(H->Elements);

    // place a super-minimum value at the start of the vector
	auto temp = ArenaAllocateAndClear(arena, H->elementSize + sizeof(int));
    if (temp == NULL) { return; }
    writeInt(temp, INT32_MIN);
    VectorPush(H->Elements, temp);
    ArenaDereference(arena, temp);
}

inline int ElementPriority(Heap *H, int index) {
    return readInt(VectorGet(H->Elements, index));
}

void HeapInsert(Heap * H, int priority, void * element) {
    if (H == NULL)  return;
    
    auto arena = VectorArena(H->Elements);
    auto temp = ArenaAllocateAndClear(arena, H->elementSize + sizeof(int));
    if (temp == NULL) return;
    
    writeIntPrefixValue(temp, priority, element, H->elementSize);

    VectorPush(H->Elements, temp); // this is a dummy value, we will overwrite it
    uint32_t size = VectorLength(H->Elements);
    int i;

    // Percolate down to make room for the new element
    for (i = size - 1; ElementPriority(H, i >> 1) > priority; i >>= 1) {
        VectorSwap(H->Elements, i, i >> 1); //H->Elements[i] = H->Elements[i >> 1];
    }

    VectorSet(H->Elements, i, temp, NULL);
    ArenaDereference(arena, temp);
}

// Returns true if heap has no elements
bool HeapIsEmpty(Heap* H) {
    if (H == NULL) return true;
    return VectorLength(H->Elements) < 2; // first element is the reserved super-minimum
}

bool HeapDeleteMin(Heap * H, void* element) {
    if (HeapIsEmpty(H)) {
        return false; // our empty value
    }

    int i, Child;

    auto arena = VectorArena(H->Elements);

    auto MinElement = ArenaAllocateAndClear(arena, H->elementSize + sizeof(int));
    if (MinElement == NULL) return false; // TODO: BUG--- this can cause us to infinite loop if we run out of memory
    auto LastElement = ArenaAllocateAndClear(arena, H->elementSize + sizeof(int));
    if (LastElement == NULL) { ArenaDereference(arena, MinElement); return false; }

    VectorCopy(H->Elements, 1, MinElement); // the first element is always minimum
    if (element != NULL) readIntPrefixValue(element, MinElement, H->elementSize); // so copy it out
    ArenaDereference(arena, MinElement);

    // Now re-enforce the heap property
    VectorPop(H->Elements, LastElement);
    auto endPrio = readInt(LastElement);
    auto size = VectorLength(H->Elements) - 1;

    for (i = 1; i * 2 <= size; i = Child) {
        // Find smaller child
        Child = i * 2;

        if (Child != size &&
            ElementPriority(H, Child) > ElementPriority(H, Child + 1)) {
            Child++;
        }

        // Percolate one level
        if (endPrio > ElementPriority(H, Child)) {
            VectorSwap(H->Elements, i, Child);
        }
        else break;
    }

    VectorSet(H->Elements, i, LastElement, NULL);
    return MinElement;
}

void* HeapPeekMin(Heap* H) {
    if (!HeapIsEmpty(H)) return byteOffset(VectorGet(H->Elements, 1), sizeof(int));

    return NULL;
}

bool HeapTryFindMin(Heap* H, void * found) {
    if (!HeapIsEmpty(H)) {
        readIntPrefixValue(found, VectorGet(H->Elements, 1), H->elementSize);
        return true;
    }
    return false;
}

// find the 2nd least element
bool HeapTryFindNext(Heap* H, void * found) {
    if (H == NULL) return false;
    auto size = VectorLength(H->Elements);
    if (size < 3) return false;

    if (size == 3) { // exactly 2 real elements
        readIntPrefixValue(found, VectorGet(H->Elements, 2), H->elementSize);
        return true;
    }

    // inspect top two and pick the smallest
    if (ElementPriority(H, 2) > ElementPriority(H, 3)) {
        readIntPrefixValue(found, VectorGet(H->Elements, 3), H->elementSize);
    } else {
        readIntPrefixValue(found, VectorGet(H->Elements, 2), H->elementSize);
    }

    return true;
}