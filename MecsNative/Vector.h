#pragma once
#ifndef vector_h
#define vector_h

// Generalised auto-sizing vector
// Can be used as a stack or array
typedef struct Vector Vector;

// Create a new dynamic vector with the given element size (must be fixed per vector)
Vector *VectorAllocate(int elementSize);
// Check the vector is correctly allocated
bool VectorIsValid(Vector *v);
// Clear all elements out of the vector, but leave it valid
void VectorClear(Vector *v);
// Deallocate vector (does not deallocate anything held in the elements)
void VectorDeallocate(Vector *v);
// Return number of elements in vector. Allocated capacity may be substantially different
int VectorLength(Vector *v);
// Push a new value to the end of the vector
bool VectorPush(Vector *v, void* value);
// Get a pointer to an element in the vector. This is an in-place pointer -- no copy is made
void* VectorGet(Vector *v, unsigned int index);
// Copy data from an element in the vector to a pointer
bool VectorCopy(Vector *v, unsigned int index, void* outValue);
// Copy data from first element and remove it from the vector
bool VectorDequeue(Vector *v, void* outValue);
// Read and remove an element from the vector. A copy of the element is written into the parameter. If null, only the removal is done.
bool VectorPop(Vector *v, void *target);
// Write a value at a given position. This must be an existing allocated position (with either push or prealloc).
// If not 'prevValue' is not null, the old value is copied there
bool VectorSet(Vector *v, unsigned int index, void* element, void* prevValue);
// Ensure the vector has at least this many elements allocated. Any extras written will be zeroed out.
bool VectorPrealloc(Vector *v, unsigned int length);
// Swaps the values at two positions in the vector
bool VectorSwap(Vector *v, unsigned int index1, unsigned int index2);


// Macros to create type-specific versions of the methods above.
// If you want to use the typed versions, make sure you call `RegisterContainerFor(typeName, namespace)` for EACH type
// Vectors can only hold one kind of fixed-length element per vector instance

// These are invariant on type, but can be namespaced
#define RegisterVectorStatics(nameSpace) \
    inline void nameSpace##Deallocate(Vector *v){ VectorDeallocate(v); }\
    inline int nameSpace##Length(Vector *v){ return VectorLength(v); }\
    inline bool nameSpace##Prealloc(Vector *v, unsigned int length){ return VectorPrealloc(v, length); }\
    inline bool nameSpace##Swap(Vector *v, unsigned int index1, unsigned int index2){ return VectorSwap(v, index1, index2); }\
    inline void nameSpace##Clear(Vector *v) { VectorClear(v); }\

// These must be registered for each type, as they are type variant
#define RegisterVectorFor(typeName, nameSpace) \
    inline Vector* nameSpace##Allocate_##typeName(){ return VectorAllocate(sizeof(typeName)); } \
    inline bool nameSpace##Push_##typeName(Vector *v, typeName valuePtr){ return VectorPush(v, (void*)&valuePtr); } \
    inline typeName * nameSpace##Get_##typeName(Vector *v, unsigned int index){ return (typeName*)VectorGet(v, index); } \
    inline bool nameSpace##Copy_##typeName(Vector *v, unsigned int idx, typeName *target){ return VectorCopy(v, idx, (void*) target); } \
    inline bool nameSpace##Pop_##typeName(Vector *v, typeName *target){ return VectorPop(v, (void*) target); } \
    inline bool nameSpace##Set_##typeName(Vector *v, unsigned int index, typeName* element, typeName* prevValue){ return VectorSet(v, index, (void*)element, (void*)prevValue); } \
    inline bool nameSpace##Dequeue_##typeName(Vector *v, typeName* outValue) { return VectorDequeue(v, (void*)outValue);}\



#endif
