#pragma once

#ifndef compilercore_h
#define compilercore_h

#include "TagCodeWriter.h"
#include "Tree.h"
#include "HashMap.h"

// The type of code being compiled
enum class Context {
    // Normal code
    Default,

    // In a memory access function
    MemoryAccess,

    // Compiling an import
    External,

    // A loop block
    Loop,

    // A condition block
    Condition
};


// Compile source code from a syntax tree into a tag code cache
TagCodeCache* CompileRoot(TreeNode* root, bool debug);

// Function/Program compiler. This is called recursively when subroutines are found
TagCodeCache* Compile(TreeNode* root, int indent, bool debug, Scope parameterNames, HashMap* includedFiles, Context compileContext);

#endif
