#pragma once

#ifndef tree_h
#define tree_h

#include "Vector.h"

typedef struct TreeNode TreeNode;
typedef struct Tree Tree;

// Allocate a tree for a given size of element
Tree* TreeAllocate(int elementSize);
// Deallocate all nodes, and the data held
void TreeDeallocate(Tree* tree);

// Get the root node of a tree
TreeNode *TreeRoot(Tree* tree);
// Write an element value to the given node. If `node` is null, the root element is set
void TreeSetValue(TreeNode* node, void* element);
// Add a child to the end of the parent's child chain
TreeNode* TreeAddChild(TreeNode* parent, void* element);
// Add a sibling to the given node. If it's part of a longer chain, it gets put at the end of the chain
TreeNode* TreeAddSibling(TreeNode* treeNodePtr, void* element);
// Returns a pointer to the node data (in place, no copying)
void* TreeReadBody(TreeNode* treeNodePtr);
// Returns the first child of a node, or null
TreeNode *TreeChild(TreeNode* parentPtr);
// Returns the next sibling of a node, or null
TreeNode *TreeSibling(TreeNode* olderSiblingPtr);
// Add a child node into the given index in the child chain
TreeNode *TreeInsertChild(TreeNode* parent, int targetIndex, void* element);
// Remove a child by index and stitch the chain back together
void TreeRemoveChild(TreeNode* parent, int targetIndex);
// Provide a vector of pointers to all node data. Order is depth-first. The vector must be deallocated by the caller
Vector* TreeAllData(Tree *tree);


// Macros to create type-specific versions of the methods above.
// If you want to use the typed versions, make sure you call `RegisterTreeFor(typeName, namespace)` for EACH type

// These are invariant on type, but can be namespaced
#define RegisterTreeStatics(nameSpace) \
    inline TreeNode * nameSpace##Root(Tree* tree){return TreeRoot(tree);}\
    inline void nameSpace##Deallocate(Tree *t){ TreeDeallocate(t); }\
    inline TreeNode *nameSpace##Child(TreeNode *t){ return TreeChild(t); }\
    inline TreeNode *nameSpace##Sibling(TreeNode *t){ return TreeSibling(t); }\
    inline void nameSpace##RemoveChild(TreeNode *t, int idx){ return TreeRemoveChild(t, idx); }\
    inline Vector* nameSpace##AllData(Tree *t){ return TreeAllData(t);}\


// These must be registered for each distinct pair, as they are type variant
#define RegisterTreeFor(elemType, nameSpace) \
    inline elemType* nameSpace##ReadBody_##elemType(TreeNode *node){return (elemType*)TreeReadBody(node);}\
    inline TreeNode* nameSpace##InsertChild_##elemType(TreeNode* parent, int idx, elemType* element){return TreeInsertChild(parent,idx,element);}\
    inline void nameSpace##SetValue_##elemType(TreeNode* node, elemType* element){TreeSetValue(node,element);}\
    inline TreeNode* nameSpace##AddChild_##elemType(TreeNode* parent, elemType* element){return TreeAddChild(parent, element);}\
    inline TreeNode* nameSpace##AddSibling_##elemType(TreeNode* node, elemType* element){return TreeAddSibling(node,element);}\
    inline Tree* nameSpace##Allocate_##elemType(){return TreeAllocate(sizeof(elemType));}\


#endif