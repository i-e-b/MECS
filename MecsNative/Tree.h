#pragma once

#ifndef tree_h
#define tree_h

typedef struct TreeNode TreeNode;
typedef struct Tree {
    int ElementByteSize;
    TreeNode* Root;
    bool IsValid;
} Tree;

// Allocate a tree for a given size of element
Tree TreeAllocate(int elementSize);
// Deallocate all nodes, and the data held
void Deallocate(Tree* tree);

// Write an element value to the given node. If `node` is null, the root element is set
void SetValue(TreeNode* node, void* element);
// Add a child to the end of the parent's child chain
TreeNode* AddChild(TreeNode* parent, void* element);
// Add a sibling to the given node. If it's part of a longer chain, it gets put at the end of the chain
TreeNode* AddSibling(TreeNode* treeNodePtr, void* element);
// Returns a pointer to the node data (in place, no copying)
void* ReadBody(TreeNode* treeNodePtr);
// Returns the first child of a node, or null
TreeNode *Child(TreeNode* parentPtr);
// Returns the next sibling of a node, or null
TreeNode *Sibling(TreeNode* olderSiblingPtr);
// Add a child node into the given index in the child chain
TreeNode *InsertChild(TreeNode* parent, int targetIndex, void* element);
// Remove a child by index and stitch the chain back together
void RemoveChild(TreeNode* parent, int targetIndex);

#endif