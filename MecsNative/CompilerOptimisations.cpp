#include "CompilerOptimisations.h"

bool CO_IsSmallIncrement(TreeNode * node, int8_t * outIncr, String ** outVarName) {
    // TODO
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