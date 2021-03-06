#pragma once

#ifndef typecoersion_h
#define typecoersion_h

#include "String.h"
#include "TagData.h"
#include "HashMap.h"
#include "TagCodeInterpreter.h"

/*
    Runtime casting and coersion of unknown types to required types
*/

// Interpret or cast value as a boolean
bool CastBoolean(InterpreterState* is, DataTag encoded);
// null, empty, "false" or "0" are false. All other strings are true.
bool StringTruthyness(String* strVal);
// Interpret, or cast value as double
double CastDouble(InterpreterState* is, DataTag encoded);
// Cast a value to int. If not applicable, returns zero
int CastInt(InterpreterState* is, DataTag  encoded);

// Get a resonable string representation from a value.
// This should include stringifying non-string types (numbers, structures etc)
// The called should always deallocate the resulting string (a proxy will be used if required)
String* CastString(InterpreterState* is, DataTag encoded);

#endif
