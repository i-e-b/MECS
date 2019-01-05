#pragma once
#ifndef vector_h
#define vector_h

// Generalised auto-sizing vector
// Can be used as a stack or array
typedef struct Vector {
    bool IsValid; // if this is false, creation failed

    // Calculated parts
    int ElemsPerChunk;
    int ElementByteSize;
    int ChunkHeaderSize;
    unsigned short ChunkBytes;

    // dynamic parts
    unsigned int _elementCount;     // how long is the logical array
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

// Create a new dynamic vector with the given element size (must be fixed per vector)
Vector VectorAllocate(int elementSize);
// Deallocate vector (does not deallocate anything held in the elements)
void VectorDeallocate(Vector *v);
// Return number of elements in vector. Allocated capacity may be substantially different
int VectorLength(Vector *v);
// Push a new value to the end of the vector
bool VectorPush(Vector *v, void* value);
// Get a pointer to an element in the vector. This is an in-place pointer -- no copy is made
void* VectorGet(Vector *v, unsigned int index);
// Read and remove an element from the vector. A copy of the element is written into the parameter. If null, only the removal is done.
bool VectorPop(Vector *v, void *target);
// Write a value at a given position. This must be an existing allocated position (with either push or prealloc).
// If not 'prevValue' is not null, the old value is copied there
bool VectorSet(Vector *v, unsigned int index, void* element, void* prevValue);
// Ensure the vector has at least this many elements allocated. Any extras written will be zeroed out.
bool VectorPrealloc(Vector *v, unsigned int length);
// Swaps the values at two positions in the vector
bool VectorSwap(Vector *v, unsigned int index1, unsigned int index2);

#endif
