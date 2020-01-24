#pragma once

#ifndef desugar_h
#define desugar_h

/*
    Language features that are implemented by decomposing into simpler elements
*/

#include "String.h"
#include "Tree_2.h"
#include "Scope.h"
#include "TagCodeWriter.h"

// If true, the original AST should be expanded
bool NeedsDesugaring(String* funcName);

// Expand input nodes
DTreeNode DesugarProcessNode(String* funcName, Scope* parameterNames, DTreeNode node, TagCodeCache* tcc);

#endif
