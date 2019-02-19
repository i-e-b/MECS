#include "TypeCoersion.h"

/*
Convert from RuntimeMemoryModel.cs
*/


// Make or return reference to a string version of the given token. `debugSymbols` is Map<crushname->string>
String* Stringify(InterpreterState* is, DataTag token, DataType type, HashMap* debugSymbols);
// Interpret or cast value as a boolean
bool CastBoolean(InterpreterState* is, DataTag encoded);
// null, empty, "false" or "0" are false. All other strings are true.
bool StringTruthyness(InterpreterState* is, String* strVal);
// Interpret, or cast value as double
float CastDouble(InterpreterState* is, DataTag encoded);
// Cast a value to int. If not applicable, returns zero
int CastInt(InterpreterState* is, DataTag  encoded);

// Get a resonable string representation from a value.
// This should include stringifying non-string types (numbers, structures etc)
String* CastString(InterpreterState* is, DataTag encoded) {
    // IMPORTANT: this should NEVER send back the original string -- it will get deallocated!
    // TODO: make a proxy string variant, that points to another and can be deallocated safely
}
// Store a new string at the end of memory, and return a string pointer token for it
DataTag StoreStringAndGetReference(InterpreterState* is, String* str);