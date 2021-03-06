#pragma once

#ifndef tree_h
#define tree_h

#include "Vector.h"

/*

This is a Linked-node general tree implementation (using pointers everywhere)

I plan to replace it.

*/



typedef struct TreeNode TreeNode;

// Allocate a tree for a given size of element
TreeNode* TreeAllocate(ArenaPtr arena, int elementSize);
// Deallocate all nodes, and the data held
void TreeDeallocate(TreeNode* tree);

// return false if this node has children, true otherwise
bool TreeIsLeaf(TreeNode* node);

// Write an element value to the given node. If `node` is null, the root element is set
void TreeSetValue(TreeNode* node, void* element);
// Add a child to the end of the parent's child chain
TreeNode* TreeAddChild(TreeNode* parent, void* element);
// Add a sibling to the given node. If it's part of a longer chain, it gets put at the end of the chain
TreeNode* TreeAddSibling(TreeNode* treeNodePtr, void* element);
// Returns a pointer to the node data (in place, no copying)
void* TreeReadBody(TreeNode* treeNodePtr);

// Counts the number of children of a node
int TreeCountChildren(TreeNode* node);

// Returns the first child of a node, or null
TreeNode *TreeChild(TreeNode* parentPtr);
// Returns the next sibling of a node, or null
TreeNode *TreeSibling(TreeNode* olderSiblingPtr);
// Returns the parent node of a child, or null if this is the root
TreeNode *TreeParent(TreeNode* childPtr);
// Return the nth child (zero indexed). Returns NULL if out of range
TreeNode *TreeNthChild(TreeNode* parent, int childIdx);


// Add a child node into the given index in the child chain
TreeNode *TreeInsertChild(TreeNode* parent, int targetIndex, void* element);
// Remove a child by index and stitch the chain back together
void TreeRemoveChild(TreeNode* parent, int targetIndex);
// Provide a vector of pointers to all node data. Order is depth-first. The vector must be deallocated by the caller
Vector* TreeAllData(TreeNode *tree);

// Create a 'pivot' of a node. The first child is brought up, and it's siblings become children.
// IN: node->[first, second, ...]; OUT: (parent:node)<-first->[second, ...]
TreeNode* TreePivot(TreeNode *node);
// Create a node not connected to a tree
TreeNode* TreeBareNode(ArenaPtr arena, int elementSize);
// Add a bare node into a tree, at the end of the parent's child chain
void TreeAppendNode(TreeNode* parent, TreeNode* child);

// Get the arena being used by a tree node
ArenaPtr TreeArena(TreeNode* node);

// Macros to create type-specific versions of the methods above.
// If you want to use the typed versions, make sure you call `RegisterTreeFor(typeName, namespace)` for EACH type

// These are invariant on type, but can be namespaced
#define RegisterTreeStatics(nameSpace) \
    inline void nameSpace##Deallocate(TreeNode *t){ TreeDeallocate(t); }\
    inline TreeNode *nameSpace##Child(TreeNode *t){ return TreeChild(t); }\
    inline TreeNode *nameSpace##Parent(TreeNode *t){ return TreeParent(t); }\
    inline TreeNode *nameSpace##Sibling(TreeNode *t){ return TreeSibling(t); }\
    inline void nameSpace##RemoveChild(TreeNode *t, int idx){ return TreeRemoveChild(t, idx); }\
    inline void nameSpace##AppendNode(TreeNode *parent, TreeNode *child){ return TreeAppendNode(parent, child); }\
    inline Vector* nameSpace##AllData(TreeNode *t){ return TreeAllData(t);}\
    inline bool nameSpace##IsLeaf(TreeNode* node){return TreeIsLeaf(node);}\
    inline TreeNode* nameSpace##Pivot(TreeNode *node){return TreePivot(node);}\
	inline ArenaPtr nameSpace##Arena(TreeNode *node){return TreeArena(node);}\
    inline int nameSpace##CountChildren(TreeNode* node){return TreeCountChildren(node);}\


// These must be registered for each distinct pair, as they are type variant
#define RegisterTreeFor(elemType, nameSpace) \
    inline elemType* nameSpace##ReadBody_##elemType(TreeNode *node){return (elemType*)TreeReadBody(node);}\
    inline TreeNode* nameSpace##InsertChild_##elemType(TreeNode* parent, int idx, elemType* element){return TreeInsertChild(parent,idx,element);}\
    inline TreeNode* nameSpace##BareNode_##elemType(ArenaPtr arena){return TreeBareNode(arena, sizeof(elemType));}\
    inline void nameSpace##SetValue_##elemType(TreeNode* node, elemType* element){TreeSetValue(node,element);}\
    inline TreeNode* nameSpace##AddChild_##elemType(TreeNode* parent, elemType* element){return TreeAddChild(parent, element);}\
    inline TreeNode* nameSpace##AddSibling_##elemType(TreeNode* node, elemType* element){return TreeAddSibling(node,element);}\
    inline TreeNode* nameSpace##Allocate_##elemType(ArenaPtr arena){return TreeAllocate(arena, sizeof(elemType));}\


#endif