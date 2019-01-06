#include "Tree.h"
#include <stdlib.h>

// Header for each tree node. This covers the first bytes of a node
typedef struct TreeNode {
    TreeNode* ParentPtr;      // Pointer to parent. null means root
    TreeNode* FirstChildPtr;  // Pointer to child linked list. null means leaf node
    TreeNode* NextSiblingPtr; // Pointer to next sibling (linked list of parent's children)
    int ElementByteSize;      // Number of bytes in element
} TreeNode;


const unsigned int POINTER_SIZE = sizeof(size_t);
const unsigned int NODE_HEAD_SIZE = sizeof(TreeNode);

#pragma region helperjunk
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

#pragma endregion

Tree TreeAllocate(int elementSize) {
    auto result = Tree();

    result.ElementByteSize = elementSize;

    // Make the root node
    result.Root = (TreeNode*)calloc(1, NODE_HEAD_SIZE + result.ElementByteSize); // notice we actually oversize to hold the node data
    if (result.Root == NULL) {
        result.IsValid = false;
        return result;
    }

    // Set initial values
    result.Root->FirstChildPtr = NULL;
    result.Root->NextSiblingPtr = NULL;
    result.Root->ParentPtr = NULL;
    result.Root->ElementByteSize = elementSize;

    result.IsValid = true;
    return result;
}


// Write an element value to the given node. If `node` is null, the root element is set
void SetValue(TreeNode* node, void* element) {
    writeValue((void*)node, NODE_HEAD_SIZE, element, node->ElementByteSize);
}

TreeNode* AllocateAndWriteNode(TreeNode* parent, void* element) {
    // Allocate new node and header
    auto nodeSize = NODE_HEAD_SIZE + parent->ElementByteSize;
    auto newChildHead = (TreeNode*)calloc(1, nodeSize);
    if (newChildHead == NULL) {
        return NULL;
    }

    // Write a node head and data into memory
    newChildHead->ParentPtr = parent;
    newChildHead->ElementByteSize = parent->ElementByteSize;

    writeValue((void*)newChildHead, NODE_HEAD_SIZE, element, newChildHead->ElementByteSize);
    return newChildHead;
}

TreeNode* AddSibling(TreeNode* treeNodePtr, void* element) {
    // Walk the sibling link chain until we hit the last element
    var next = treeNodePtr;
    while (next->NextSiblingPtr != NULL) {
        next = next->NextSiblingPtr;
    }

    // Make a new node
    var newChildPtr = AllocateAndWriteNode(next->ParentPtr, element);
    if (newChildPtr == NULL) return NULL;

    // Write ourself into the chain
    next->NextSiblingPtr = newChildPtr;
    return newChildPtr;
}

TreeNode* AddChild(TreeNode* parent, void* element) {
    if (parent == NULL) return NULL;

    var head = parent;

    if (head->FirstChildPtr != NULL) { // part of a chain. Switch function
        return AddSibling(head->FirstChildPtr, element);
    }

    // This is the first child of this parent
    var newChildPtr = AllocateAndWriteNode(parent, element);
    if (newChildPtr == NULL) return NULL;

    // Set ourself as the parent's first child
    head->FirstChildPtr = newChildPtr;
    return newChildPtr;
}

void* ReadBody(TreeNode* treeNodePtr) {
    return byteOffset(treeNodePtr, NODE_HEAD_SIZE);
}

TreeNode *Child(TreeNode* parentPtr) {
    if (parentPtr == NULL) return NULL;
    return parentPtr->FirstChildPtr;
}

TreeNode *Sibling(TreeNode* olderSiblingPtr) {
    if (olderSiblingPtr == NULL) return NULL;
    return olderSiblingPtr->NextSiblingPtr;
}

TreeNode *InsertChild(TreeNode* parent, int targetIndex, void* element) {

    // Simplest case: a plain add
    if (parent->FirstChildPtr == NULL) {
        if (targetIndex != 0) return NULL;
        return AddChild(parent, element);
    }

    // Special case: insert at start (need to update parent)
    if (targetIndex == 0) {
        var newRes = AllocateAndWriteNode(parent, element);
        if (newRes == NULL) return NULL;
        var next = parent->FirstChildPtr;
        parent->FirstChildPtr = newRes;

        newRes->NextSiblingPtr = next;

        return newRes;
    }

    // Main case: walk the chain, keeping track of our index
    var idx = 1;
    var prevSibling = parent->FirstChildPtr;
    while (idx < targetIndex) {
        var nextRes = prevSibling->NextSiblingPtr;
        if (nextRes == NULL) {
            if (idx == targetIndex) break; // writing to end of chain, OK
            return NULL; // tried to write off the end of chain
        }
        prevSibling = nextRes;
        idx++;
    }
    // Got the predecessor in sibling chain

    // Inject into chain.
    // New node
    var newNode = AllocateAndWriteNode(parent, element);
    if (newNode == NULL) return NULL;

    // Swap pointers around
    newNode->NextSiblingPtr = prevSibling->NextSiblingPtr; // doesn't matter if this is NULL
    prevSibling->NextSiblingPtr = newNode;

    return newNode;
}


// Deallocate node and all its children AND siblings, recursively
void RecursiveDelete(TreeNode* treeNodePtr) {
    var current = treeNodePtr;
    while (current != NULL) {
        if (current->FirstChildPtr != NULL) RecursiveDelete(current->FirstChildPtr);
        var next = current->NextSiblingPtr;
        free(current);
        current = next;
    }
}

// Deallocate node and all it's children, recursively
void DeleteNode(TreeNode* treeNodePtr) {
    if (treeNodePtr == NULL) return;
    RecursiveDelete(treeNodePtr->FirstChildPtr);
    free(treeNodePtr);
}

void RemoveChild(TreeNode* parent, int targetIndex) {
    TreeNode*  deleteTargetPtr;

    if (parent->FirstChildPtr == NULL) return; // empty parent

    // 1. If targetIndex == 0, short the parent into the next
    if (targetIndex == 0) {
        deleteTargetPtr = parent->FirstChildPtr;
        if (deleteTargetPtr == NULL) return; // there are no children

        // Skip over this item
        parent->FirstChildPtr = deleteTargetPtr->NextSiblingPtr;

        DeleteNode(deleteTargetPtr);
        return;
    }

    // 2. Scan through sibling chain. Short-circuit when we find the index
    // Main case: walk the chain, keeping track of our index
    var idx = 1;
    var leftSibling = parent->FirstChildPtr;
    while (idx < targetIndex) {
        var nextRes = leftSibling->NextSiblingPtr;
        if (nextRes == NULL) {
            return; // tried to delete off the end of chain: ignore
        }
        leftSibling = nextRes;
        idx++;
    }

    // Got the predecessor in sibling chain
    if (leftSibling->NextSiblingPtr == NULL) return; // deleting at end of chain: ignore

    // Set `leftSiblingHead` to point at its target's target
    //  [left] --> [to del] --> [whatever]
    //  [left] ---------------> [whatever]
    deleteTargetPtr = leftSibling->NextSiblingPtr;
    leftSibling->NextSiblingPtr = deleteTargetPtr->NextSiblingPtr;

    DeleteNode(deleteTargetPtr); // clean up
}

// Deallocate all nodes, and the data held
void Deallocate(Tree* tree) {
    DeleteNode(tree->Root);
}
