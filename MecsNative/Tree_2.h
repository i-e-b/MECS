#pragma once

#ifndef tree2_h
#define tree2_h

#include "ArenaAllocator.h"
#include "Vector.h"
#include "String.h"

/*

This is a table-based general tree implementation, to replace the pointer-based tree.

Plan:
=====

Option 1 - Maps
---------------

Map 1: Relations. (NodeId) => (ParentId, List<ChildId>)
Map 2: Contents. (NodeId) => (Node data)

The "Tree" object contains these maps, a counter to maintain unique IDs, and the ID of the root node.

Option 2 - Compound Arrays
--------------------------

Array 1: Relations [NodeId -> List<ChildId]
Array 2: Contents [NodeId -> (Node data)]

Option 3 - Diagonal Arrays
--------------------------

Array 1: Relations [NodeId -> (ParentId, First child Id, Next sibling id)]
Array 2: Contents [NodeId -> (Node data)]

Both the array options have trouble deleting node data (subtrees can be disconnected, but not removed from memory)
We'd want a 'cleanup' or 'copy' function that makes a compacted copy of a (sub)tree.

*/


// Diagonal tree stored in arrays
typedef struct DTree DTree;
typedef DTree *DTreePtr;

// A container for tree + a specific node
// TODO: add overrides to use this as an alternative to tree+nodeId in params
typedef struct DTreeNode {
    DTreePtr Tree;
    int NodeId;
} DTreeNode;

// Function to render nodes to strings. Used by `DTreeRenderToString()`
typedef void DTreeNodeRenderFunc(void* nodeData, StringPtr target);
// Function to act on node data in isolation. Used by `DTreeApplyAll()`
typedef void DTreeApplyFunc(void* nodeData);

// Allocate a new empty tree in an arena. All allocations will remain in this arena.
DTreePtr DTreeAllocate(ArenaPtr arena, int dataElementSize);
// Deallocate all storage for a tree, including data elements and nodes
void DTreeDeallocate(DTreePtr* tree);

// Remove all data and nodes, restore an empty root node
void DTreeClear(DTreePtr tree);

// Draw a representation of the tree to the given string
void DTreeRenderToString(DTreePtr tree, StringPtr string, DTreeNodeRenderFunc* nodeRenderFunc);
// Draw a representation of the tree to the given string
StringPtr DTreeRenderToString(DTreePtr tree, DTreeNodeRenderFunc* nodeRenderFunc);

// Make a new normalised copy of the tree, from a given node down.
DTreePtr DTreeCopySubtree(DTreePtr src, int newRootId);

// Copy a subtree from one tree to another. The equivalent of `srcRootId` in the `dst` tree is returned
int DTreeAppendSubtree(DTreePtr dst, int dstParentId, DTreePtr src, int srcRootId);

// return the ID of the root element in the tree
int DTreeRootId(DTreePtr tree);
// return false if this node has children, true otherwise
bool DTreeIsLeaf(DTreePtr tree, int nodeId);

// Write an element value to the given node. The element is shallow copied into the tree.
bool DTreeSetValue(DTreePtr tree, int nodeId, void* element);

// Add a child to the end of the parent's child chain. The NodeId of the new node is returned (or negative if invalid)
int DTreeAddChild(DTreePtr tree, int parentId, void* element);

// Add a sibling to the given node. If it's part of a longer chain, it gets put at the end of the chain.
// The node ID of the new node is returned, or negative if invalid.
int DTreeAddSibling(DTreePtr tree, int nodeId, void* element);

// Returns a pointer to the node data (in place, no copying)
void* DTreeReadBody(DTreePtr tree, int nodeId);


// Counts the number of immediate children of a node (the 1st child and all its siblings, but no grand children)
int DTreeCountChildren(DTreePtr tree, int nodeId);

// Returns the ID of the first child of a node, or negative
int DTreeGetChildId(DTreePtr tree, int parentId);
// Returns the next sibling of a node, or negative
int DTreeGetSiblingId(DTreePtr tree, int olderSiblingId);
// Returns the parent node of a child, or negative if this is the root
int DTreeGetParentId(DTreePtr tree, int childId);
// Return the nth child (zero indexed). Returns negative if out of range
int DTreeGetNthChildId(DTreePtr tree, int parentId, int childIdx);



// Add a child node into the given index in the child chain. Return the Id of the new node
// If the index is off the end of the sibling chain, the node will be added to the end of the chain
int DTreeInsertChild(DTreePtr tree, int parentId, int targetIndex, void* element);
// Remove a child by index and stitch the chain back together
void DTreeRemoveChild(DTreePtr tree, int parentId, int targetIndex);
// Remove a child by ID and stitch the chain back together. If no ID matches, nothing will happen
// returns the index of the child found
int DTreeRemoveChildById(DTreePtr tree, int parentId, int targetId);

// Provide a vector of all node data. Order is arbitrary. Caller should dispose.
// If an arena is not provided, the tree's own storage will be used
VectorPtr DTreeAllData(DTreePtr tree, ArenaPtr arena);
// Apply a function to each data element in turn, independent of position in the tree. This is useful for deallocation too.
void DTreeApplyAll(DTreePtr tree, DTreeApplyFunc* func);

// Create a 'pivot' of a node. The first child is brought up, and it's siblings become children.
// IN: node->[first, second, ...]; OUT: (parent:node)<-first->[second, ...]
// Returns the Id of the first child
int DTreePivot(DTreePtr tree, int nodeId);

// Insert a new node between a parent and all its children
// IN: node->[first, second, ...]; OUT: (parent:node)<-newNode->[first, second, ...]
// Returns the Id of the new node
int DTreeInjectNode(DTreePtr tree, int parentId, void* element);

DTreeNode DTreeMakeNode(DTreePtr tree, int nodeId);
DTreeNode DTreeRootNode(DTreePtr tree);

// Get the arena being used by a tree node
ArenaPtr DTreeArena(DTreePtr tree);


// Macros to create type-specific versions of the methods above.
// If you want to use the typed versions, make sure you call `RegisterDTreeFor(typeName, namespace)` for EACH type

// These are invariant on type, but can be namespaced
#define RegisterDTreeStatics(nameSpace) \
    inline void nameSpace##Deallocate(DTreePtr* tree){ DTreeDeallocate(tree); }\
    inline void nameSpace##Deallocate(DTreeNode node){ DTreeDeallocate(&(node.Tree)); }\
	inline ArenaPtr nameSpace##Arena(DTreePtr tree){return DTreeArena(tree);}\
    inline VectorPtr nameSpace##AllData(DTreePtr tree, ArenaPtr arena){return DTreeAllData(tree, arena);}\
    inline int nameSpace##RootId(DTreePtr tree){ return DTreeRootId( tree);}\
    inline int nameSpace##Pivot(DTreePtr tree, int nodeId){return DTreePivot(tree, nodeId);}\
    inline int nameSpace##Pivot(DTreeNode node){return DTreePivot(node.Tree, node.NodeId);}\
    inline int nameSpace##CountChildren(DTreePtr tree, int nodeId){return DTreeCountChildren(tree, nodeId);}\
    inline int nameSpace##CountChildren(DTreeNode node){return DTreeCountChildren(node.Tree, node.NodeId);}\
    inline void nameSpace##RemoveChild(DTreePtr tree, int parentId, int targetIndex){ DTreeRemoveChild( tree, parentId, targetIndex);}\
    inline void nameSpace##RemoveChild(DTreeNode node, int targetIndex){ DTreeRemoveChild( node.Tree, node.NodeId, targetIndex);}\
    inline int nameSpace##RemoveChildById(DTreePtr tree, int parentId, int targetId){ return DTreeRemoveChildById( tree, parentId, targetId);}\
    inline int nameSpace##RemoveChildById(DTreeNode node, int targetId){ return DTreeRemoveChildById( node.Tree, node.NodeId, targetId);}\
    inline int nameSpace##GetNthChildId(DTreePtr tree, int parentId, int childIdx){ return DTreeGetNthChildId( tree, parentId, childIdx);}\
    inline int nameSpace##GetNthChildId(DTreeNode node, int childIdx){ return DTreeGetNthChildId( node.Tree, node.NodeId, childIdx);}\
    inline int nameSpace##GetParentId(DTreePtr tree, int childId){ return DTreeGetParentId( tree, childId);}\
    inline int nameSpace##GetParentId(DTreeNode node){ return DTreeGetParentId( node.Tree, node.NodeId);}\
    inline int nameSpace##GetSiblingId(DTreePtr tree, int olderSiblingId){ return DTreeGetSiblingId( tree, olderSiblingId);}\
    inline int nameSpace##GetSiblingId(DTreeNode node){ return DTreeGetSiblingId( node.Tree, node.NodeId);}\
    inline int nameSpace##GetChildId(DTreePtr tree, int parentId){ return DTreeGetChildId( tree, parentId);}\
    inline int nameSpace##GetChildId(DTreeNode node){ return DTreeGetChildId( node.Tree, node.NodeId);}\
    inline bool nameSpace##IsLeaf(DTreePtr tree, int nodeId){ return DTreeIsLeaf( tree, nodeId);}\
    inline bool nameSpace##IsLeaf(DTreeNode node){ return DTreeIsLeaf( node.Tree, node.NodeId);}\
    inline DTreePtr nameSpace##CopySubtree(DTreePtr src, int newRootId) { return DTreeCopySubtree(src, newRootId); }\
    inline DTreePtr nameSpace##CopySubtree(DTreeNode node) { return DTreeCopySubtree(node.Tree, node.NodeId); }\
    inline int nameSpace##AppendSubtree(DTreePtr dst, int dstParentId, DTreePtr src, int srcRootId) { return DTreeAppendSubtree(dst,dstParentId,src,srcRootId);}\
    inline int nameSpace##AppendSubtree(DTreeNode dst, DTreeNode src) { return DTreeAppendSubtree(dst.Tree,dst.NodeId,src.Tree,src.NodeId);}\
    inline DTreeNode nameSpace##Node(DTreePtr tree, int nodeId){ auto x = DTreeNode{}; x.Tree = tree; x.NodeId = nodeId; return x; }\
    inline void nameSpace##Clear(DTreePtr tree) { DTreeClear(tree); }\
    inline bool nameSpace##ValidNode(DTreeNode node) { return (node.NodeId >= 0); }\

// These must be registered for each distinct pair, as they are type variant
#define RegisterDTreeFor(elemType, nameSpace) \
    inline DTreePtr nameSpace##Allocate_##elemType(ArenaPtr arena){return DTreeAllocate(arena, sizeof(elemType));}\
    inline void nameSpace##RenderToString_##elemType(DTreePtr tree, StringPtr string, void(*nodeRenderFunc)(elemType* nodeData, StringPtr target)){DTreeRenderToString(tree, string, (DTreeNodeRenderFunc*)nodeRenderFunc);}\
    inline StringPtr nameSpace##RenderToString_##elemType(DTreePtr tree, void(*nodeRenderFunc)(elemType* nodeData, StringPtr target)){ return DTreeRenderToString(tree, (DTreeNodeRenderFunc*)nodeRenderFunc);}\
    inline void nameSpace##ApplyAll_##elemType(DTreePtr tree, void(*func)(elemType* nodeData)){ DTreeApplyAll(tree, (DTreeApplyFunc*)func);}\
    inline int nameSpace##InsertChild_##elemType(DTreePtr tree, int parentId, int targetIndex, elemType element){return DTreeInsertChild(tree, parentId, targetIndex, &element);}\
    inline int nameSpace##InsertChild_##elemType(DTreeNode node, int targetIndex, elemType element){return DTreeInsertChild(node.Tree, node.NodeId, targetIndex, &element);}\
    inline int nameSpace##AddSibling_##elemType(DTreePtr tree, int nodeId, elemType element){return DTreeAddSibling(tree, nodeId, &element);}\
    inline int nameSpace##AddSibling_##elemType(DTreeNode node, elemType element){return DTreeAddSibling(node.Tree, node.NodeId, &element);}\
    inline int nameSpace##AddChild_##elemType(DTreePtr tree, int parentId, elemType element){return DTreeAddChild(tree, parentId, &element);}\
    inline int nameSpace##AddChild_##elemType(DTreeNode node, elemType element){return DTreeAddChild(node.Tree, node.NodeId, &element);}\
    inline bool nameSpace##SetValue_##elemType(DTreePtr tree, int nodeId, elemType element){return DTreeSetValue(tree, nodeId, &element);}\
    inline bool nameSpace##SetValue_##elemType(DTreeNode node, elemType element){return DTreeSetValue(node.Tree, node.NodeId, &element);}\
    inline elemType* nameSpace##ReadBody_##elemType(DTreePtr tree, int nodeId){return (elemType*)DTreeReadBody(tree, nodeId);}\
    inline elemType* nameSpace##ReadBody_##elemType(DTreeNode node){return (elemType*)DTreeReadBody(node.Tree, node.NodeId);}\
    inline int nameSpace##InjectNode_##elemType(DTreePtr tree, int parentId, elemType element){return DTreeInjectNode(tree, parentId, &element);}\
    inline int nameSpace##InjectNode_##elemType(DTreeNode node, elemType element){return DTreeInjectNode(node.Tree, node.NodeId, &element);}\

#endif
