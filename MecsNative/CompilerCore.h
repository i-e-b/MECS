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
#include "Tree_2.h"
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
    Condition,
    // An `eval` statement at run-time
    RuntimeEval
};


// Compile source code from a syntax tree into a tag code cache
// Setting `debug` to true will cause additional symbol output, and may turn off some optimisations
// Setting `isSubprogram` to true will set a return-like marker at the end of the program. If false, it will set a termination marker.
TagCodeCache* CompileRoot(DTreePtr root, bool debug, bool isSubprogram);

// Function/Program compiler. This is called recursively when subroutines are found
TagCodeCache* Compile(DTreePtr root, int indent, bool debug, Scope* parameterNames, HashMap* includedFiles, Context compileContext);

#endif
