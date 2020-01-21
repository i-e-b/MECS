#pragma once

#ifndef tree2_h
#define tree2_h

#include "ArenaAllocator.h"
#include "Vector.h"

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

// Allocate a new empty tree in an arena. All allocations will remain in this arena.
DTreePtr DTreeAllocate(ArenaPtr arena, int dataElementSize);
// Deallocate all storage for a tree, including data elements and nodes
void DTreeDeallocate(DTreePtr* tree);



// return false if this node has children, true otherwise
bool DTreeIsLeaf(DTreePtr tree, int nodeId);

// Write an element value to the given node.
// the element is shallow copied into the tree.
bool DTreeSetValue(DTreePtr tree, int nodeId, void* element);

// Add a child to the end of the parent's child chain. The NodeId of the new node is returned (or negative if invalid)
int DTreeAddChild(DTreePtr tree, int parentId, void* element);
// Add a sibling to the given node. If it's part of a longer chain, it gets put at the end of the chain.
// The node ID of the new node is returned, or negative if invalid.
int DTreeAddSibling(DTreePtr tree, int nodeId, void* element);
// Returns a pointer to the node data (in place, no copying)
void* DTreeReadBody(DTreePtr tree, int nodeId);


// Counts the number of children of a node
int DTreeCountChildren(DTreePtr tree, int nodeId);

// Returns the ID of the first child of a node, or negative
int DTreeGetChild(DTreePtr tree, int parentId);
// Returns the next sibling of a node, or negative
int DTreeGetSibling(DTreePtr tree, int olderSiblingId);
// Returns the parent node of a child, or negative if this is the root
int DTreeGetParent(DTreePtr tree, int childId);
// Return the nth child (zero indexed). Returns negative if out of range
int DTreeGetNthChild(DTreePtr tree, int parentId, int childIdx);



// Add a child node into the given index in the child chain. Return the Id of the new nod
int DTreeInsertChild(DTreePtr tree, int parentId, int targetIndex, void* element);
// Remove a child by index and stitch the chain back together
void DTreeRemoveChild(DTreePtr tree, int parentId, int targetIndex);
// Provide a vector of all node data. Order is arbitrary. Caller should dispose
VectorPtr DTreeAllData(DTreePtr tree);

// Create a 'pivot' of a node. The first child is brought up, and it's siblings become children.
// IN: node->[first, second, ...]; OUT: (parent:node)<-first->[second, ...]
// Returns the Id of the first child
int DTreePivot(DTreePtr tree, int nodeId);

// Get the arena being used by a tree node
ArenaPtr DTreeArena(DTreePtr tree);


#endif
