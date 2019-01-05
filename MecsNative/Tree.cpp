#include "Tree.h"
#include <stdlib.h>

// Header for each tree node. This covers the first bytes of a node
typedef struct TreeNode {
    TreeNode* ParentPtr;      // Pointer to parent. -1 means root
    TreeNode* FirstChildPtr;  // Pointer to child linked list. -1 mean leaf node
    TreeNode* NextSiblingPtr; // Pointer to next sibling (linked list of parent's children)
} TreeNode;

typedef struct Tree {
    size_t NodeSize; // size of each node (based on element size)
    TreeNode* Root;
    bool IsValid;
} Tree;

const unsigned int POINTER_SIZE = sizeof(size_t);
const unsigned int NODE_HEAD_SIZE = sizeof(TreeNode);


// Some helpers going from C# prototype to C
#ifndef NULL
#define NULL 0
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef var
#define var auto
#endif

// A bunch of little memory helpers
inline void * byteOffset(void *ptr, int byteOffset) {
    char* x = (char*)ptr;
    x += byteOffset;
    return (void*)x;
}
inline uint readUint(void* ptr, int byteOffset) {
    char* x = (char*)ptr;
    x += byteOffset;
    return ((uint*)x)[0];
}
inline void* readPtr(void* ptr, int byteOffset) {
    char* x = (char*)ptr;
    x += byteOffset;
    return ((void**)x)[0];
}
inline void writeUint(void *ptr, int byteOffset, uint data) {
    char* x = (char*)ptr;
    x += byteOffset;
    ((uint*)x)[0] = data;
}
inline void writePtr(void *ptr, int byteOffset, void* data) {
    char* x = (char*)ptr;
    x += byteOffset;
    ((size_t*)x)[0] = (size_t)data;
}
inline void writeValue(void *ptr, int byteOffset, void* data, int length) {
    char* dst = (char*)ptr;
    dst += byteOffset;
    char* src = (char*)data;

    for (int i = 0; i < length; i++) {
        *(dst++) = *(src++);
    }
}

Tree TreeAllocate(int elementSize)
{
    auto result = Tree();

    result.NodeSize = NODE_HEAD_SIZE + elementSize;

    // Make the root node
    result.Root = (TreeNode*)calloc(1, result.NodeSize); // notice we actually oversize to hold the node data
    if (result.Root == NULL) {
        result.IsValid = false;
        return result;
    }

    // Set initial values
    result.Root->FirstChildPtr = NULL;
    result.Root->NextSiblingPtr = NULL;
    result.Root->ParentPtr = NULL;

    result.IsValid = true;
    return result;
}
