#include "CompilerOptimisations.h"
#include "SourceCodeTokeniser.h"

typedef SourceNode Node; // 'SourceNode'. Typedef just for brevity

RegisterTreeStatics(T)
RegisterTreeFor(Node, T)


bool IsGet(TreeNode* node, String* target) {
    auto nodeData = TReadBody_Node(node);
    if (nodeData->NodeType == NodeType::Atom && StringAreEqual(nodeData->Text, target)) return true;
    if (StringAreEqual(nodeData->Text, "get") && TCountChildren(node) == 1) {
        auto childData = TReadBody_Node(TChild(node));
        return StringAreEqual(childData->Text, target);
    }

    return false;
}

bool IsGet(TreeNode* node) {
    auto nodeData = TReadBody_Node(node);
    if (nodeData->NodeType == NodeType::Atom && StringLength(nodeData->Text) > 0) return true;
    if (StringAreEqual(nodeData->Text, "get") && TCountChildren(node) == 1) {
        auto childData = TReadBody_Node(TChild(node));
        return StringLength(childData->Text) > 0;
    }

    return false;
}

bool CO_IsSmallIncrement(TreeNode * node, int8_t * outIncr, String ** outVarName) {
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

    auto nodeData = TReadBody_Node(node);
    if (!StringAreEqual(nodeData->Text, "set")) return false;
    if (TCountChildren(node) != 2) return false;

    auto firstChildData = TReadBody_Node(TChild(node));

    auto target = firstChildData->Text;
    auto operation = TSibling(TChild(node));
    if (TCountChildren(operation) != 2) return false;

    auto operationData = TReadBody_Node(operation);
    if (!StringAreEqual(operationData->Text, "+") && !StringAreEqual(operationData->Text, "-")) return false;

    auto opChild1 = TChild(operation);
    auto opChild1Data = TReadBody_Node(opChild1);
    auto opChild2 = TSibling(opChild1);
    auto opChild2Data = TReadBody_Node(opChild2);

    if (StringAreEqual(operationData->Text, "-")) { // the *second* item must be a small constant
        if (!IsGet(opChild1, target)) return false;
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
        TreeNode* incrNode;
        if (IsGet(opChild1, target)) {
            incrNode = opChild2;
        } else if (IsGet(opChild2, target)) {
            incrNode = opChild1;
        } else return false;

        auto incrNodeData = TReadBody_Node(incrNode);
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
bool CO_IsSimpleComparion(TreeNode* condition, int opCodeCount) {
    // TODO
    return false;
}

// Pack a comparison between two simple values into a single op-code. Reduces loop condition complexity
TreeNode* CO_ReadSimpleComparison(TreeNode* condition, CmpOp *outCmpOp, uint16_t* outArgCount) {
    // TODO
    return NULL;
}