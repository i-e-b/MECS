#pragma once

#ifndef tagcodewriter_h
#define tagcodewriter_h

#include "TagData.h"
#include "TagCodeFunctionTypes.h"

#include "HashMap.h"
#include "Vector.h"
#include "String.h"

#include <stdint.h>

/*
    Manages the ordering and output of interpreter codes.
    This is driven by the compiler
*/

typedef struct TagCodeCache TagCodeCache;

// Management methods:

TagCodeCache* TCW_Allocate();
void TCW_Deallocate(TagCodeCache* tcc);

// True if any errors were recorded
bool TCW_HasErrors(TagCodeCache* tcc);
// Errors during writing, or NULL if successful
Vector* TCW_ErrorList(TagCodeCache* tcc);
// Add an error to the writer
void TCW_AddError(TagCodeCache* tcc, String* ErrorMessage);

// Returns true if we expect this code fragment to return a value
bool TCW_ReturnsValues(TagCodeCache* tcc);
// Set if this code fragment is expected to return a value. 
void TCW_SetReturnsValues(TagCodeCache* tcc);

// Read the data tag in the cache at a zero-based index
DataTag TCW_OpCodeAtIndex(TagCodeCache* tcc, int index);

// Current written opcode position (relative only, for calculating relative jumps)
int TCW_Position(TagCodeCache* tcc);
// Count of opcodes written
int TCW_OpCodeCount(TagCodeCache* tcc);

// Inject a compiled sub-unit into this writer. References to string constants will be recalculated
// NOTE: The `srcFragment` parameter will be deallocted!
void TCW_Merge(TagCodeCache* dest, TagCodeCache* srcFragment);

// Write opcodes and data section to a BYTE vector. References to string constants will be recalculated
Vector* TCW_WriteToStream(TagCodeCache* tcc);

// Adds opcodes and data section to a BYTE vector. References to string constants will be recalculated. Returns start of code index.
int TCW_AppendToStream(TagCodeCache* tcc, Vector* existing);

// Adds opcodes and data section to a `DataTag` vector. References to string constants will be recalculated
// this is mainly for use with 'eval' runtime code generation. Returns start of code index.
int TCW_AppendToVector(TagCodeCache* tcc, Vector* existing);

// Add a symbol set to the known symbols table
bool TCW_AddSymbols(TagCodeCache* tcc, HashMap* sym);

// Return the original names of variable references we've hashed. Keys are the Variable Ref byte codes
HashMap* TCW_GetSymbols(TagCodeCache* tcc);

// Adds a symbol map to a BYTE vector.
bool TCW_WriteSymbolsToStream(TagCodeCache* tcc, Vector* existing);


// Generation methods:

// Add an inline comment into the stream
void TCW_Comment(TagCodeCache* tcc, String* s);
// Reference a value name
void TCW_VariableReference(TagCodeCache* tcc, String* valueName);
// Make a memory command with a string name
void TCW_Memory(TagCodeCache* tcc, char action, String* targetName, int paramCount);
// Make a memory command with a crushed name
void TCW_Memory(TagCodeCache* tcc, char action, uint32_t crushed);
// Direct numeric manipulation
void TCW_Increment(TagCodeCache* tcc, int8_t incr, String* targetName);
// Function call
void TCW_FunctionCall(TagCodeCache* tcc, String* functionName, int parameterCount);
// Add a define-and-skip set of opcodes *before* merging in the compiled function opcodes.
void TCW_FunctionDefine(TagCodeCache* tcc, String* functionName, int argCount, int tokenCount);
// Add a single symbol reference
bool TCW_AddSymbol(TagCodeCache* tcc, uint32_t crushed, String* name);
// Mark a function return that should not happen
void TCW_InvalidReturn(TagCodeCache* tcc);
// Return from a function, with a certain number of values
void TCW_Return(TagCodeCache* tcc, int pCount);
// Encode a compound compare (,compare to do, number of value-stack args to compare, opcodes to jump relative down if top if comparison result is false)
void TCW_CompoundCompareJump(TagCodeCache* tcc, CmpOp operation, uint16_t argCount, uint16_t opCodeCount);
// Jump relative down if top of value-stack is false
void TCW_CompareJump(TagCodeCache* tcc, int opCodeCount);
// Jump relative up, always
void TCW_UnconditionalJump(TagCodeCache* tcc, int opCodeCount);
// Encode a numeric value
void TCW_LiteralNumber(TagCodeCache* tcc, int32_t d);
// Write a static string. Static strings aren't seen by the GC and exist in memory outside of the normal allocation space. Returns `true` iff the string is already present
bool TCW_LiteralString(TagCodeCache* tcc, String* s);
// Add a pre-encoded string
void TCW_RawToken(TagCodeCache* tcc, DataTag value);


#endif
