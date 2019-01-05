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

Vector VectorAllocate(int elementSize);
void VectorDeallocate(Vector *v);

int VectorLength(Vector *v);
bool VectorPush(Vector *v, void* value);
void* VectorGet(Vector *v, unsigned int index);
void* VectorPop(Vector *v);
bool VectorSet(Vector *v, unsigned int index, void* element, void* prevValue);
bool VectorPrealloc(Vector *v, unsigned int length);
bool VectorSwap(Vector *v, unsigned int index1, unsigned int index2);

#endif
