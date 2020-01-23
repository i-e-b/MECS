#include "CompilerOptimisations.h"
#include "SourceCodeTokeniser.h"

typedef SourceNode Node; // 'SourceNode'. Typedef just for brevity

RegisterDTreeStatics(T)
RegisterDTreeFor(Node, T)

//#define DISABLE_OPTIMISATIONS 1


bool IsGet(DTreeNode node, String* target) {
    auto nodeData = TReadBody_Node(node);
    if (nodeData->NodeType == NodeType::Atom && StringAreEqual(nodeData->Text, target)) return true;
    if (StringAreEqual(nodeData->Text, "get") && TCountChildren(node) == 1) {
        auto childData = TReadBody_Node(node.Tree, TGetChildId(node));
        return StringAreEqual(childData->Text, target);
    }

    return false;
}


bool IsGet(DTreeNode node) {
    auto nodeData = TReadBody_Node(node);
    if (nodeData->NodeType == NodeType::Atom && StringLength(nodeData->Text) > 0) return true;
    if (StringAreEqual(nodeData->Text, "get") && TCountChildren(node) == 1) {
        auto childData = TReadBody_Node(node.Tree, TGetChildId(node));
        return StringLength(childData->Text) > 0;
    }

    return false;
}

bool IsSimpleType(DTreeNode node) {
    auto data = TReadBody_Node(node);
    switch (data->NodeType) {
    case NodeType::StringLiteral:
    case NodeType::Numeric:
    case NodeType::Atom:
        return true;
    default:
        return false;
    }
}


bool CO_IsSimpleSet(DTreeNode setNode) {
#ifdef DISABLE_OPTIMISATIONS
    return false;
#endif
    auto nodeData = TReadBody_Node(setNode);
    return (StringAreEqual(nodeData->Text, "set") && TCountChildren(setNode.Tree, TGetChildId(setNode)) == 0);
}

//bool CO_IsSmallIncrement(TreeNode * node, int8_t * outIncr, String ** outVarName) {
bool CO_IsSmallIncrement(DTreeNode* node, int8_t* outIncr, String** outVarName) {
#ifdef DISABLE_OPTIMISATIONS
    return false;
#endif
    if (node == NULL || outIncr == NULL || outVarName == NULL) return false;

    // These cases to be handled:
    // 1)  set(x +(x n))
    // 2)  set(x +(get(x) n))
    // 3)  set(x +(n x))
    // 4)  set(x +(n get(x)))
    // 5)  set(x -(x n))
    // 6)  set(x -(get(x) n))
    //
    // Where `x` is any reference name, and n is a literal integer between -100 and 100

    *outIncr = 0;
    *outVarName = NULL;

    auto tree = node->Tree;
    auto nodeData = TReadBody_Node(*node);
    if (!StringAreEqual(nodeData->Text, "set")) return false;
    if (TCountChildren(*node) != 2) return false;

    auto firstChildData = TReadBody_Node(tree, TGetChildId(*node));

    auto target = firstChildData->Text;
    auto operation = TGetSiblingId(tree, TGetChildId(*node));
    if (TCountChildren(tree, operation) != 2) return false;

    auto operationData = TReadBody_Node(tree, operation);
    if (!StringAreEqual(operationData->Text, "+") && !StringAreEqual(operationData->Text, "-")) return false;

    auto opChild1 = TGetChildId(tree, operation);
    auto opChild1Data = TReadBody_Node(tree, opChild1);
    auto opChild2 = TGetSiblingId(tree, opChild1);
    auto opChild2Data = TReadBody_Node(tree, opChild2);

    if (StringAreEqual(operationData->Text, "-")) { // the *second* item must be a small constant
        if (!IsGet(TNode(tree, opChild1), target)) return false;
        auto incrNode = opChild2Data;
        if (incrNode->NodeType != NodeType::Numeric) return false;

        int incrV;
        if (!StringTryParse_int32(incrNode->Text, &incrV)) return false;
        if (incrV < -100 || incrV > 100 || incrV == 0) return false;

        *outIncr = (-incrV); // negate to account for subtraction
        *outVarName = target;
        return true;
    }

    if (StringAreEqual(operationData->Text, "+")) { // either item must be a small constant
        int incrNode;
        if (IsGet(TNode(tree, opChild1), target)) {
            incrNode = opChild2;
        } else if (IsGet(TNode(tree, opChild2), target)) {
            incrNode = opChild1;
        } else return false;

        auto incrNodeData = TReadBody_Node(tree, incrNode);
        if (incrNodeData->NodeType != NodeType::Numeric) return false;
        int incrV;
        if (!StringTryParse_int32(incrNodeData->Text, &incrV)) return false;
        if (incrV < -100 || incrV > 100 || incrV == 0) return false;

        *outIncr = incrV;
        *outVarName = target;
        return true;
    }

    return false;
}

// Is this a single comparison between two values?
// opCodeCount is the jump length of the condition
//bool CO_IsSimpleComparsion(TreeNode* condition, int opCodeCount) {
bool CO_IsSimpleComparsion(DTreeNode* condition, int opCodeCount) {
#ifdef DISABLE_OPTIMISATIONS
    return false;
#endif
    if (opCodeCount >= 32767) return false; // can't be encoded

    auto tree = condition->Tree;
    auto target = TGetChildId(*condition);
    if (TCountChildren(tree, target) != 2) return false;

	auto leftId = TGetChildId(tree, target);
	auto rightId = TGetSiblingId(tree, leftId);

    auto left = TNode(tree, leftId);
    auto right = TNode(tree, rightId);

    auto leftIsSimple = IsGet(left) || IsSimpleType(left);
    auto rightIsSimple = IsGet(right) || IsSimpleType(right);

    if (!leftIsSimple || !rightIsSimple) return false;

    auto targetData = TReadBody_Node(tree, target);
    auto txt = targetData->Text;

    if (StringAreEqual(txt, "=")) return true;
    if (StringAreEqual(txt, "equals")) return true;

    if (StringAreEqual(txt, "<>")) return true;
    if (StringAreEqual(txt, "not-equals")) return true;

    if (StringAreEqual(txt, "<")) return true;
    if (StringAreEqual(txt, ">")) return true;

    return false;
}


/*
DTreeNode Repack(DTreeNode parent) {
    auto root = Node();
    root.NodeType = NodeType::Root;
    root.SourceLocation = 0;
    root.IsValid = true;
    root.Text = StringEmpty();
    root.Unescaped = NULL;
    root.FormattedLocation = 0;

    auto tree = TAllocate_Node(TArena(parent));
    TSetValue_Node(tree, root);

    TAppendNode(tree, TChild(parent));

    return tree;
}*/

// Pack a comparison between two simple values into a single op-code. Reduces loop condition complexity
DTreeNode CO_ReadSimpleComparison(DTreeNode* condition, CmpOp *outCmpOp, uint16_t* outArgCount) {
//TreeNode* CO_ReadSimpleComparison(TreeNode* condition, CmpOp *outCmpOp, uint16_t* outArgCount) {
    auto target = TGetChildId(*condition);
    *outArgCount = TCountChildren(condition->Tree, target);


    auto targetData = TReadBody_Node(condition->Tree, target);
    auto txt = targetData->Text;
    if (StringAreEqual(txt, "=")) *outCmpOp = CmpOp::Equal;
    else if (StringAreEqual(txt, "equals")) *outCmpOp = CmpOp::Equal;

    else if (StringAreEqual(txt, "<>")) *outCmpOp = CmpOp::NotEqual;
    else if (StringAreEqual(txt, "not-equals")) *outCmpOp = CmpOp::NotEqual;

    else if (StringAreEqual(txt, "<")) *outCmpOp = CmpOp::Less;
    else if (StringAreEqual(txt, ">")) *outCmpOp = CmpOp::Greater;

    else return;// NULL;

    //return Repack(target); // this is weird and tricky. Figure out a nicer way of doing it.
}