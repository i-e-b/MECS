#pragma once

#ifndef compileroptimisations_h
#define compileroptimisations_h

/*
    Checks for compile-time optimisations
*/

#include "Tree_2.h"
#include "String.h"
#include "TagCodeFunctionTypes.h"
#include <stdint.h>

// Is a node an add or subtract that can be encoded as an increment. `node` should be TreeNode<SourceNode>.
bool CO_IsSmallIncrement(DTreeNode* node, int8_t* outIncr, String** outVarName);

// Is this a single comparison between two values?
bool CO_IsSimpleComparsion(DTreeNode* condition, int opCodeCount);

// Is this node a set command, where the target is a simple reference
bool CO_IsSimpleSet(DTreeNode* setNode);

// Pack a comparison between two simple values into a single op-code. Reduces loop condition complexity
DTreeNode CO_ReadSimpleComparison(DTreeNode* condition, CmpOp *outCmpOp, uint16_t* outArgCount);

#endif
