#include "Tree.h"
#include "MemoryManager.h"

#include "RawData.h"


// Header for each tree node. This covers the first bytes of a node
// The actual node pointer is oversized to hold the element data
typedef struct TreeNode {
    TreeNode* ParentPtr;      // Pointer to parent. null means root
    TreeNode* FirstChildPtr;  // Pointer to child linked list. null means leaf node
    TreeNode* NextSiblingPtr; // Pointer to next sibling (linked list of parent's children)
    int ElementByteSize;      // Number of bytes in element
} TreeNode;

const unsigned int POINTER_SIZE = sizeof(void*);
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

#pragma endregion

TreeNode* TreeAllocate(int elementSize) {
    // Make the root node
    auto Root = (TreeNode*)mcalloc(1, NODE_HEAD_SIZE + elementSize); // notice we actually oversize to hold the node data
    if (Root == NULL) {
        return NULL;
    }

    // Set initial values
    Root->FirstChildPtr = NULL;
    Root->NextSiblingPtr = NULL;
    Root->ParentPtr = NULL;
    Root->ElementByteSize = elementSize;

    return Root;
}

bool TreeIsLeaf(TreeNode* node) {
    if (node == NULL) return false;
    return node->FirstChildPtr == NULL;
}

// Write an element value to the given node. If `node` is null, the root element is set
void TreeSetValue(TreeNode* node, void* element) {
    writeValue((void*)node, NODE_HEAD_SIZE, element, node->ElementByteSize);
}

TreeNode* AllocateAndWriteNode(TreeNode* parent, int elementByteSize, void* element) {
    // Allocate new node and header
    auto nodeSize = NODE_HEAD_SIZE + elementByteSize;
    auto newChildHead = (TreeNode*)mcalloc(1, nodeSize);
    if (newChildHead == NULL) {
        return NULL;
    }

    // Write a node head and data into memory
    newChildHead->ParentPtr = parent;
    newChildHead->ElementByteSize = elementByteSize;
    newChildHead->NextSiblingPtr = NULL;
    newChildHead->FirstChildPtr = NULL;

    writeValue((void*)newChildHead, NODE_HEAD_SIZE, element, newChildHead->ElementByteSize);
    return newChildHead;
}

TreeNode* TreeAddSibling(TreeNode* treeNodePtr, void* element) {
    if (treeNodePtr == NULL) return NULL;

    // Walk the sibling link chain until we hit the last element
    var next = treeNodePtr;
    while (next->NextSiblingPtr != NULL) {
        next = next->NextSiblingPtr;
    }

    // Make a new node
    var newChildPtr = AllocateAndWriteNode(next->ParentPtr, treeNodePtr->ElementByteSize, element);
    if (newChildPtr == NULL) return NULL;

    // Write ourself into the chain
    next->NextSiblingPtr = newChildPtr;
    return newChildPtr;
}

TreeNode* TreeAddChild(TreeNode* parent, void* element) {
    if (parent == NULL) return NULL;

    var head = parent;

    if (head->FirstChildPtr != NULL) { // part of a chain. Switch function
        return TreeAddSibling(head->FirstChildPtr, element);
    }

    // This is the first child of this parent
    var newChildPtr = AllocateAndWriteNode(parent, parent->ElementByteSize, element);
    if (newChildPtr == NULL) return NULL;

    // Set ourself as the parent's first child
    head->FirstChildPtr = newChildPtr;
    return newChildPtr;
}

void* TreeReadBody(TreeNode* treeNodePtr) {
    return byteOffset(treeNodePtr, NODE_HEAD_SIZE);
}

TreeNode *TreeChild(TreeNode* parentPtr) {
    if (parentPtr == NULL) return NULL;
    return parentPtr->FirstChildPtr;
}

TreeNode *TreeParent(TreeNode* childPtr) {
    if (childPtr == NULL) return NULL;
    return childPtr->ParentPtr;
}

TreeNode *TreeSibling(TreeNode* olderSiblingPtr) {
    if (olderSiblingPtr == NULL) return NULL;
    return olderSiblingPtr->NextSiblingPtr;
}

TreeNode *TreeInsertChild(TreeNode* parent, int targetIndex, void* element) {
    if (parent == NULL) return NULL;

    // Simplest case: a plain add
    if (parent->FirstChildPtr == NULL) {
        if (targetIndex != 0) return NULL;
        return TreeAddChild(parent, element);
    }

    // Special case: insert at start (need to update parent)
    if (targetIndex == 0) {
        var newRes = AllocateAndWriteNode(parent, parent->ElementByteSize, element);
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
    var newNode = AllocateAndWriteNode(parent, parent->ElementByteSize, element);
    if (newNode == NULL) return NULL;

    // Swap pointers around
    newNode->NextSiblingPtr = prevSibling->NextSiblingPtr; // doesn't matter if this is NULL
    prevSibling->NextSiblingPtr = newNode;

    return newNode;
}


int TreeCountChildren(TreeNode* node) {
    if (node == NULL || node->FirstChildPtr == NULL) return 0;

    int count = 0;
    auto n = node->FirstChildPtr;
    while (n != NULL) {
        count++;
        n = n->NextSiblingPtr;
    }
    return count;
}

// Create a node not connected to a tree
TreeNode* TreeBareNode(int elementSize) {
    // Make the root node
    auto Root = (TreeNode*)mcalloc(1, NODE_HEAD_SIZE + elementSize); // notice we actually oversize to hold the node data
    if (Root == NULL) {
        return NULL;
    }

    // Set initial values
    Root->FirstChildPtr = NULL;
    Root->NextSiblingPtr = NULL;
    Root->ParentPtr = NULL;
    Root->ElementByteSize = elementSize;
    return Root;
}

// Add a bare node into a tree
void TreeAppendNode(TreeNode* parent, TreeNode* child) {
    if (parent == NULL) return;
    if (child == NULL) return;

    var head = parent;

    // Walk the child sibling chain and update the parent
    auto sibChain = child;
    while (sibChain != NULL) {
        sibChain->ParentPtr = parent;
        sibChain = sibChain->NextSiblingPtr;
    }

    if (head->FirstChildPtr != NULL) { // part of a chain.
        // Walk the sibling link chain until we hit the last element
        var next = head->FirstChildPtr;
        while (next->NextSiblingPtr != NULL) {
            next = next->NextSiblingPtr;
        }

        // Write ourself into the chain
        next->NextSiblingPtr = child;
        return;
    }

    // Set ourself as the parent's first child
    head->FirstChildPtr = child;
    return;
}

// Create a 'pivot' of a node. The first child is brought up, and it's siblings become children.
// IN: node->[first, second, ...]; OUT: (parent:node)<-first->[second, ...]
TreeNode* TreePivot(TreeNode *node) {
    if (node == NULL || node->FirstChildPtr == NULL) return NULL;

    void* firstNodeData = TreeReadBody(node->FirstChildPtr);
    auto newRoot = AllocateAndWriteNode(node, node->ElementByteSize, firstNodeData);

    newRoot->FirstChildPtr = TreeSibling(node->FirstChildPtr);

    return newRoot;
}

// Deallocate node and all its children AND siblings, recursively
void RecursiveDelete(TreeNode* treeNodePtr) {
    var current = treeNodePtr;
    while (current != NULL) {
        if (current->FirstChildPtr != NULL) RecursiveDelete(current->FirstChildPtr);
        var next = current->NextSiblingPtr;
        mfree(current);
        current = next;
    }
}

// Deallocate node and all it's children, recursively
void DeleteNode(TreeNode* treeNodePtr) {
    if (treeNodePtr == NULL) return;
    RecursiveDelete(treeNodePtr->FirstChildPtr);
    mfree(treeNodePtr);
}

void TreeRemoveChild(TreeNode* parent, int targetIndex) {
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

void RecursiveFill(TreeNode* treeNodePtr, Vector *nodeDataList) {
    auto current = treeNodePtr;
    while (current != NULL) {
        if (current->FirstChildPtr != NULL) RecursiveFill(current->FirstChildPtr, nodeDataList);

        auto next = current->NextSiblingPtr;
        VectorPush(nodeDataList, TreeReadBody(current));

        current = next;
    }
}

Vector* TreeAllData(TreeNode * tree) {
    auto vec = VectorAllocate(tree->ElementByteSize);
    RecursiveFill(tree, vec);
    return vec;
}

// Deallocate all nodes, and the data held
void TreeDeallocate(TreeNode* tree) {
    DeleteNode(tree);
}
