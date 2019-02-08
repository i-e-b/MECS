#pragma once

#ifndef compileroptimisations_h
#define compileroptimisations_h

/*
    Checks for compile-time optimisations
*/

#include "Tree.h"
#include "String.h"
#include "TagCodeTypes.h"
#include <stdint.h>

// Is a node an add or subtract that can be encoded as an increment. `node` should be TreeNode<SourceNode>.
bool CO_IsSmallIncrement(TreeNode* node, int8_t* outIncr, String** outVarName);

// Is this a single comparison between two values?
bool CO_IsSimpleComparion(TreeNode* condition, int opCodeCount);

// Pack a comparison between two simple values into a single op-code. Reduces loop condition complexity
TreeNode* CO_ReadSimpleComparison(TreeNode* condition, CmpOp *outCmpOp, uint16_t* outArgCount);

#endif
