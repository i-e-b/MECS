

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
	int _elementSize;

	// Relation index of root node
	int RootIndex;

	// Vector of `DRelation`
	VectorPtr Relations;

	// Vector of indeterminate type
	VectorPtr Data;
} DTree;

// Allocate a new empty tree in an arena. All allocations will remain in this arena.
DTreePtr DTreeAllocate(ArenaPtr arena, int dataElementSize) {
	if (arena == NULL || dataElementSize < 1) return NULL;

	auto result = (DTreePtr)ArenaAllocateAndClear(arena, sizeof(DTree));
	if (result == NULL) return NULL;

	result->_memory = arena;
	result->_elementSize = dataElementSize;
	result->Relations = VectorAllocateArena_DRelation(arena);
	result->Data = VectorAllocateArena(arena, dataElementSize);

	if (result->Data == NULL || result->Relations == NULL) DTreeDeallocate(&result);
	
	// Insert a root node
	DRelation crel = {};
	crel.DataIndex = INVALID;
	crel.ParentId = INVALID;
	crel.NextSibling = INVALID;
	crel.ChildId = INVALID;

	VectorPush_DRelation(result->Relations, crel);
	result->RootIndex = VectorLength(result->Relations) - 1;

	return result;
}

int DTreeRootId(DTreePtr tree) {
	if (tree == NULL) return INVALID;
	return tree->RootIndex;
}

// Deallocate all storage for a tree, including data elements and nodes
void DTreeDeallocate(DTreePtr *tree) {
	if (tree == NULL || *tree == NULL) return;

	auto arena = (*tree)->_memory;
	if (arena == NULL) return;

	VectorDeallocate((*tree)->Data);
	VectorDeallocate((*tree)->Relations);
	ArenaDereference(arena, (*tree));

	*tree = NULL; // break the reference
}

// Get the arena being used by a tree node
ArenaPtr DTreeArena(DTreePtr tree) {
	if (tree == NULL) return NULL;
	return tree->_memory;
}

// Provide a vector of all node data. Order is arbitrary. Caller should dispose.
// If an arena is not provided, the tree's own storage will be used
VectorPtr DTreeAllData(DTreePtr tree, ArenaPtr arena) {
	if (tree == NULL) return NULL;
	if (arena == NULL) arena = tree->_memory;

	return VectorClone(tree->Data, arena);
}

// return false if this node has children, true otherwise
bool DTreeIsLeaf(DTreePtr tree, int nodeId) {
	if (tree == NULL) return false;
	auto rel = VectorGet_DRelation(tree->Relations, nodeId);

	if (rel == NULL) return false;
	return (rel->ChildId) < 0;
}

// Add a new node, and return it's relation data
// This will be a floating node, and requires binding into the tree
DRelation* InsertNode(DTreePtr tree, int parentId, void* element, int* childIdx) {
	// Insert the data
	if (!VectorPush(tree->Data, element)) return NULL;
	auto didx = VectorLength(tree->Data) - 1;
	
	// Insert the node
	DRelation crel = {};
	crel.DataIndex = didx;
	crel.ParentId = parentId;
	crel.NextSibling = INVALID;
	crel.ChildId = INVALID;

	VectorPush_DRelation(tree->Relations, crel);
	*childIdx = VectorLength(tree->Relations) - 1;

	// Get pointer to the node data (in place, so it can be edited)
	return VectorGet_DRelation(tree->Relations, *childIdx);
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

	int childIdx = INVALID;
	DRelation* rel =  InsertNode(tree, parentId, element, &childIdx);

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

	int childIdx = INVALID;
	DRelation* rel =  InsertNode(tree, parentId, element, &childIdx);

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



// Returns the ID of the first child of a node, or negative
int DTreeGetChildId(DTreePtr tree, int parentId) {
	if (tree == NULL || parentId < 0) return INVALID;

	auto rel = VectorGet_DRelation(tree->Relations, parentId);
	if (rel == NULL) return INVALID;
	return rel->ChildId;
}

// Returns the next sibling of a node, or negative
int DTreeGetSiblingId(DTreePtr tree, int olderSiblingId) {
	if (tree == NULL || olderSiblingId < 0) return INVALID;

	auto rel = VectorGet_DRelation(tree->Relations, olderSiblingId);
	if (rel == NULL) return INVALID;
	return rel->NextSibling;
}

// Returns the parent node of a child, or negative if this is the root
int DTreeGetParentId(DTreePtr tree, int childId) {
	if (tree == NULL || childId < 0) return INVALID;

	auto rel = VectorGet_DRelation(tree->Relations, childId);
	if (rel == NULL) return INVALID;
	return rel->ParentId;
}


// Counts the number of immediate children of a node (the 1st child and all its siblings, but no grand children)
int DTreeCountChildren(DTreePtr tree, int nodeId) {
	if (tree == NULL || nodeId < 0) return 0;

	auto first = VectorGet_DRelation(tree->Relations, nodeId);
	if (first == NULL || first->ChildId < 0) return 0;

	int found = 1;
	int lastIdx = first->ChildId;

	auto rel = VectorGet_DRelation(tree->Relations, lastIdx);
	while (rel->NextSibling >= 0) {
		found++;
		lastIdx = rel->NextSibling;
		rel = VectorGet_DRelation(tree->Relations, lastIdx);
	}
	return found;
}


// Return the nth child (zero indexed). Returns negative if out of range
int DTreeGetNthChildId(DTreePtr tree, int parentId, int childIdx) {
	if (tree == NULL || parentId < 0 || childIdx < 0) return INVALID;

	auto rel = VectorGet_DRelation(tree->Relations, parentId);
	if (rel == NULL) return INVALID;

	int count = 0;
	int lastIdx = rel->ChildId;

	while (count < childIdx) {
		lastIdx = rel->NextSibling;
		if (lastIdx < 0) return INVALID;
		rel = VectorGet_DRelation(tree->Relations, lastIdx);
		if (rel == NULL) return INVALID;
		count++;
	}
	return lastIdx;
}


// Add a child node into the given index in the child chain. Return the Id of the new node
// If the index is off the end of the sibling chain, the node will be added to the end of the chain
int DTreeInsertChild(DTreePtr tree, int parentId, int targetIndex, void* element) {
	if (tree == NULL || parentId < 0 || targetIndex < 0 || element == NULL) return INVALID;

	int childIdx = INVALID;
	DRelation* nrel = InsertNode(tree, parentId, element, &childIdx);

	// Case 1: no existing child
	auto prel = VectorGet_DRelation(tree->Relations, parentId);
	if (prel->ChildId < 0) {
		prel->ChildId = childIdx;
		return childIdx;
	}

	// Case 2: existing child and index is zero (replace child)
	if (targetIndex == 0) {
		auto oldChildRel = VectorGet_DRelation(tree->Relations, prel->ChildId);
		nrel->NextSibling = prel->ChildId;
		prel->ChildId = childIdx;
		return childIdx;
	}

	// Case 3: walk sibling chain
	auto srel = VectorGet_DRelation(tree->Relations, prel->ChildId);
	int count = 1;
	int lastIdx = srel->NextSibling;

	while (count < targetIndex) {
		lastIdx = srel->NextSibling;
		if (lastIdx < 0) break; // off the end of the chain
		
		srel = VectorGet_DRelation(tree->Relations, lastIdx);
		if (srel == NULL) return INVALID;
		count++;
	}

	// found link, insert child
	nrel->NextSibling = srel->NextSibling;
	srel->NextSibling = childIdx;

	return childIdx;
}

// Remove a child by index and stitch the chain back together
void DTreeRemoveChild(DTreePtr tree, int parentId, int targetIndex) {
	if (tree == NULL || parentId < 0 || targetIndex < 0) return;

	auto prel = VectorGet_DRelation(tree->Relations, parentId);

	// Case 1: no existing child
	if (prel->ChildId < 0) { return; }
	
	// Case 2: existing child and index is zero (remove 1st child)
	if (targetIndex == 0) {
		auto oldChildRel = VectorGet_DRelation(tree->Relations, prel->ChildId);
		prel->ChildId = oldChildRel->NextSibling;
		return;
	}
	
	// Case 3: walk sibling chain
	auto srel = VectorGet_DRelation(tree->Relations, prel->ChildId);
	int count = 1;
	int lastIdx = srel->NextSibling;

	while (count < targetIndex) {
		lastIdx = srel->NextSibling;
		if (lastIdx < 0) return; // off the end of the chain
		
		srel = VectorGet_DRelation(tree->Relations, lastIdx);
		if (srel == NULL) return; // broken tree
		count++;
	}

	// found link, skip over it in sibling chain
	if (srel->NextSibling < 0) return;
	auto targetRel = VectorGet_DRelation(tree->Relations, srel->NextSibling);
	srel->NextSibling = targetRel->NextSibling;
}

// Create a 'pivot' of a node. The first child is brought up, and it's siblings become children.
// IN: node->[first, second, ...]; OUT: (parent:node)<-first->[second, ...]
// If the 'first' child has its own children, the pivoted nodes ('second'...) are added to the end of that sibling chain.
// Returns the Id of the original first child
int DTreePivot(DTreePtr tree, int nodeId) {
	if (tree == NULL || nodeId < 0) return INVALID;

	auto prel = VectorGet_DRelation(tree->Relations, nodeId);

	// Case 1: No child
	if (prel->ChildId < 0) return INVALID;

	// Case 2: Pivot with no siblings
	int pivotIndex = prel->ChildId;
	auto frel = VectorGet_DRelation(tree->Relations, pivotIndex);
	if (frel->NextSibling < 0) return pivotIndex;

	// Case 3: Pivot with siblings but no children
	if (frel->ChildId < 0) {
		frel->ChildId = frel->NextSibling;
		frel->NextSibling = INVALID;
		return pivotIndex;
	}

	// Case 4: Pivot with both siblings and children
	int endIdx = DTreeEndOfSiblingChain(tree, frel->ChildId);
	auto endRel = VectorGet_DRelation(tree->Relations, endIdx);
	endRel->NextSibling = frel->NextSibling;
	frel->NextSibling = INVALID;
	return pivotIndex;
}

void RenderToStringRec(DTreePtr tree, int targetNode, StringPtr str, int depth, DTreeNodeRenderFunc* render) {
	auto rel = VectorGet_DRelation(tree->Relations, targetNode);
	// indent
	StringAppendChar(str, ' ', depth * 2);

	// render node content
	if (rel->DataIndex < 0) StringAppend(str, "< >"); // empty node
	else {
		auto data = VectorGet(tree->Data, rel->DataIndex);
		if (data == NULL) StringAppend(str, "[null]");
		else render(data, str);
	}

	// new line
	StringAppend(str, "\n");

	// recurse children
	if (rel->ChildId >= 0) {
		RenderToStringRec(tree, rel->ChildId, str, depth + 1, render);
	}

	// recurse next sibling
	int sibIdx = rel->NextSibling;
	if (sibIdx < 0) return;
	// this might be quite stressful on big trees. We could clean it up.
	RenderToStringRec(tree, rel->NextSibling, str, depth, render); // is our compiler good enough to optimise this?
}

// Draw a representation of the tree to the given string
void DTree_RenderToString(DTreePtr tree, StringPtr string, DTreeNodeRenderFunc* nodeRenderFunc) {
	if (tree == NULL || string == NULL || nodeRenderFunc == NULL) return;

	RenderToStringRec(tree, tree->RootIndex, string, 0, nodeRenderFunc);
}
