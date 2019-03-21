#pragma once

#ifndef tagcodereader_h
#define tagcodereader_h

#include "TagData.h"
#include "TagCodeFunctionTypes.h"

#include "HashMap.h"
#include "Vector.h"
#include "String.h"

#include <stdint.h>

/*
    Manages the input and byte ordering of interpreter codes.
    This is driven by the runtime interpreter
*/

// Read a Vector<DataTag>, correcting the byte order if required. Returns `true` if successful.
// Passes back an index for the start of code (after the string table) and the start
// of read-write memory (everything before this should be read-only)
bool TCR_Read(Vector* v, uint32_t* outStartOfCode, uint32_t* outStartOfMemory);

// Build a Map<uint32 -> string> reading from a BYTE vector
// This should also add the default (built-in) symbols
HashMap* TCR_ReadSymbols(Vector* v);

// Generate a string representation of the tag code data
String* TCR_Describe(Vector* data);

// Read a string out of the tag code data
String* DecodeString(Vector* data, int position, int length);

#endif