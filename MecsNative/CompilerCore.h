#pragma once

#ifndef compilercore_h
#define compilercore_h

/*

Compiles pre-parsed source code into TagCode.
The compiler uses the FileSys functions to access included files.

It it recommended that the parse and compile process is done in its own
memory arena, with the arena being closed AFTER the TagCodeCache has been
serialised to another store (i.e. a different arena, or to disk).

*/

#include "Scope.h"
#include "TagCodeWriter.h"
#include "Tree.h"
#include "HashMap.h"

// The type of code being compiled
// This is used by the compiler to turn on and off certain features and optimisations
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
TagCodeCache* Compile(TreeNode* root, int indent, bool debug, Scope* parameterNames, HashMap* includedFiles, Context compileContext);

#endif
