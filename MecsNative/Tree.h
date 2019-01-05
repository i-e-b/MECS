#pragma once

#ifndef tree_h
#define tree_h

typedef struct TreeNode;
typedef struct Tree;

// Allocate a tree for a given size of element
Tree TreeAllocate(int elementSize);
// Deallocate all nodes, and the data held
void Deallocate();

void SetRootValue(void* element);
TreeNode* AddChild(TreeNode* parent, void* element);
TreeNode* AddSibling(TreeNode* treeNodePtr, void* element);
void* ReadBody(TreeNode* treeNodePtr);
TreeNode *Child(TreeNode* parentPtr);
TreeNode *Sibling(TreeNode *olderSiblingPtr);
TreeNode *InsertChild(TreeNode * parent, int targetIndex, void* element);
void RemoveChild(TreeNode *  parent, int targetIndex);

#endif