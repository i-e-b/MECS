

#include "Tree_2.h"

#define INVALID -1

// Note: all siblings in a chain have the same parent ID. To get to the start of a sibling list, you can go up to 
// the parent and back. The root node should NOT have any siblings.
typedef struct DRelation {
	// Index of parent in `Relations` vector, or negative if root.
	int ParentId;

	// Index of first parent in `Relations` vector, or negative if leaf
	int ChildId;

	// Index of next sibling in chain, or negative if end
	int NextSibling;

	// Index into `Data` vector for the data at this node, or negative if empty.
	int DataIndex;
} DRelation;

RegisterVectorFor(DRelation, Vector)

// Diagonal tree stored in arrays
typedef struct DTree {
	ArenaPtr _memory;

	// Vector of `DRelation`
	VectorPtr Relations;

	// Vector of indeterminate type
	VectorPtr Data;
} DTree;

// Allocate a new empty tree in an arena. All allocations will remain in this arena.
DTreePtr DTreeAllocate(ArenaPtr arena, int dataElementSize) {
	if (arena == NULL) return NULL;

	auto result = (DTreePtr)ArenaAllocateAndClear(arena, sizeof(DTree));
	if (result == NULL) return NULL;

	result->_memory = arena;
	result->Relations = VectorAllocateArena_DRelation(arena);
	result->Data = VectorAllocateArena(arena, dataElementSize);

	if (result->Data == NULL || result->Relations == NULL) DTreeDeallocate(&result);

	return result;
}

// Deallocate all storage for a tree, including data elements and nodes
void DTreeDeallocate(DTreePtr *tree) {
	if (tree == NULL || *tree == NULL) return;

	auto arena = (*tree)->_memory;
	if (arena == NULL) return;

	ArenaDereference(arena, (*tree)->Data);
	ArenaDereference(arena, (*tree)->Relations);
	ArenaDereference(arena, *tree);

	*tree = NULL; // break the reference
}

// return false if this node has children, true otherwise
bool DTreeIsLeaf(DTreePtr tree, int nodeId) {
	if (tree == NULL) return false;
	auto rel = VectorGet_DRelation(tree->Relations, nodeId);

	if (rel == NULL) return false;
	return (rel->ChildId) < 0;
}

// Write an element value to the given node.
// the element is shallow copied into the tree.
bool DTreeSetValue(DTreePtr tree, int nodeId, void* element) {
	if (tree == NULL || nodeId < 0 || element == NULL) return false;

	auto rel = VectorGet_DRelation(tree->Relations, nodeId);
	if (rel == NULL) return false;

	int idx = rel->DataIndex;
	if (idx < 0) {
		// need a new data node
		if (!VectorPush(tree->Data, element)) return false;
		rel->DataIndex = VectorLength(tree->Data) - 1;
		return true;
	} else {
		// overwrite existing
		return VectorSet(tree->Data, idx, element, NULL);
	}
}

// Walk to the end of a sibling chain, given any node in that chain
int DTreeEndOfSiblingChain(DTreePtr tree, int nodeId) {
	if (tree == NULL || nodeId < 0) return INVALID;
	int lastIdx = nodeId;

	auto rel = VectorGet_DRelation(tree->Relations, lastIdx);
	while (rel->NextSibling >= 0) {
		lastIdx = rel->NextSibling;
		rel = VectorGet_DRelation(tree->Relations, lastIdx);
	}
	return lastIdx;
}

// Add a child to the end of the parent's child chain. The NodeId of the new node is returned (or negative if invalid)
int DTreeAddChild(DTreePtr tree, int parentId, void* element) {
	if (tree == NULL || parentId < 0 || element == NULL) return INVALID;

	// Insert the data
	if (!VectorPush(tree->Data, element)) return INVALID;
	auto didx = VectorLength(tree->Data) - 1;
	
	// Insert the node
	DRelation crel = {};
	crel.DataIndex = didx;
	crel.ParentId = parentId;
	crel.NextSibling = INVALID;
	crel.ChildId = INVALID;

	VectorPush_DRelation(tree->Relations, crel);
	int childIdx = VectorLength(tree->Relations) - 1;

	// Case 1: no existing child
	auto prel = VectorGet_DRelation(tree->Relations, parentId);
	if (prel->ChildId < 0) {
		prel->ChildId = childIdx;
		return childIdx;
	}

	// Case 2: need to walk sibling chain
	int sibIdx = DTreeEndOfSiblingChain(tree, prel->ChildId);
	auto srel = VectorGet_DRelation(tree->Relations, sibIdx);
	srel->NextSibling = childIdx;
	return childIdx;
}

// Add a sibling to the given node. If it's part of a longer chain, it gets put at the end of the chain.
// The node ID of the new node is returned, or negative if invalid.
int DTreeAddSibling(DTreePtr tree, int nodeId, void* element) {
	if (tree == NULL || nodeId < 0 || element == NULL) return INVALID;

	// Find the node's parent
	auto sibRel = VectorGet_DRelation(tree->Relations, nodeId);
	if (sibRel == NULL) return INVALID;
	int parentId = sibRel->ParentId;

	// Insert the data
	if (!VectorPush(tree->Data, element)) return INVALID;
	auto didx = VectorLength(tree->Data) - 1;

	// Insert the node
	DRelation crel = {};
	crel.DataIndex = didx;
	crel.ParentId = parentId;
	crel.NextSibling = INVALID;
	crel.ChildId = INVALID;

	VectorPush_DRelation(tree->Relations, crel);
	int childIdx = VectorLength(tree->Relations) - 1;

	// walk sibling chain and bind
	int sibIdx = DTreeEndOfSiblingChain(tree, nodeId);
	auto srel = VectorGet_DRelation(tree->Relations, sibIdx);
	srel->NextSibling = childIdx;
	return childIdx;
}

// Returns a pointer to the node data (in place, no copying)
void* DTreeReadBody(DTreePtr tree, int nodeId) {
	if (tree == NULL || nodeId < 0) return NULL;
	auto rel = VectorGet_DRelation(tree->Relations, nodeId);
	if (rel == NULL || rel->DataIndex < 0) return NULL;
	return VectorGet(tree->Data, rel->DataIndex);
}