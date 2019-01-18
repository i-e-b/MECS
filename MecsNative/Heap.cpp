#include "Heap.h"
#include "Vector.h"

#include "RawData.h"

#include <stdlib.h>
#include <stdint.h>


typedef struct Heap {
    Vector *Elements;
    int elementSize;
} Heap;

Heap * HeapAllocate(int elementByteSize) {
    auto vec = VectorAllocate(elementByteSize + sizeof(int)); // int for priority, stored inline
    if (vec == NULL) return NULL;
    Heap *h = (Heap*)malloc(sizeof(Heap));
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
    VectorDeallocate(H->Elements);
    free(H);
}

void HeapClear(Heap * H) {
    if (H == NULL) return;
    VectorClear(H->Elements);

    // place a super-minimum value at the start of the vector
    auto temp = calloc(1, H->elementSize + sizeof(int));
    if (temp == NULL) { return; }
    writeInt(temp, INT32_MIN);
    VectorPush(H->Elements, temp);
    free(temp);
}

inline int ElementPriority(Heap *H, int index) {
    return readInt(VectorGet(H->Elements, index));
}

void HeapInsert(Heap * H, int priority, void * element) {
    if (H == NULL)  return;
    auto temp = malloc(H->elementSize + sizeof(int));
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
    free(temp);
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

    auto MinElement = malloc(H->elementSize + sizeof(int));
    if (MinElement == NULL) return false;
    auto LastElement = malloc(H->elementSize + sizeof(int));
    if (LastElement == NULL) { free(MinElement); return false; }

    VectorCopy(H->Elements, 1, MinElement); // the first element is always minimum
    if (element != NULL) readIntPrefixValue(element, MinElement, H->elementSize); // so copy it out
    free(MinElement);

    // Now re-enforce the heap property
    VectorPop(H->Elements, LastElement);
    //LastElement = H->Elements[H->Size--];
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
        //if (Compare(LastElement, H->Elements[Child])) {
        if (endPrio > ElementPriority(H, Child)) {
            VectorSwap(H->Elements, i, Child);
            //H->Elements[i] = H->Elements[Child];
        }
        else break;
    }

    VectorSet(H->Elements, i, LastElement, NULL);
    //H->Elements[i] = LastElement;
    return MinElement;
}