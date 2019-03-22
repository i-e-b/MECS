#pragma once

#ifndef tagdata_h
#define tagdata_h

// Tagged data and pointers for 32 bit systems
// 8 bits of type, 24 bits of parameters, 32 bits of data
// Both parameters and and data depend on the type.

#include <stdint.h>
#include "String.h"
#include "HashMap.h"

// Bit flag on DataType that indicates the value is a pointer to GC memory
#define ALLOCATED_TYPE 0x80

// Bit flag on DataType that indicates the value represents a number
#define NUMERIC_TYPE 0x40

// Every variable or opcode in the interpreter must be one of these:
// Range 0-10 are 'core'; Range 11-63 are non-numeric values, Range 64-126 are numeric; Range 128-249 are 'allocated'
enum class DataType {
    Invalid      = 0, // Tag is not initialised, or is a sentinel value

    // ##### Core bytecode types ###########
    VariableRef  = 1,       // Data is a 32-bit hash that refers to a variable name.
    Opcode = 2,             // Params are 2xchar to define opcode, Data is either 1x32 or 2x16 bit params for the opcode
    EndOfProgram = 3,       // A marker for the end of a program. Interpreter should stop.
    EndOfSubProgram = 4,    // A marker for the end of a sub-program. Interpreter should clean-up and continue.


    // ##### Runtime result types ##########
    // A result was expected, but not produced (like trying to read an undefined slot in a collection)
    // Use of NaR will propagate through calculations.
    Not_a_Result = 5, // A nonsense value. Any calculation with NAR can be short-cut into another NAR. 'exists' checks should return false.
    Exception = 6,    // A non-recoverable run-time error occured. The interpreter will stop for inspection. Data contains interpreter location.
    Void = 7,         // Nothing returned
    Unit = 8,         // No result, but as part of a return


    // ##### Runtime data types ############
    Integer     = NUMERIC_TYPE + 1, // Data is a 32-bit signed integer, Params are unused.
    Fraction    = NUMERIC_TYPE + 2, // Data is a 16.16-bit pixed point fractional number. Params are unused (could be an extension to 40.16 or 28.28 in the future)

    HashtablePtr = ALLOCATED_TYPE + 1,  // Data is pointer to HashTable. Params not used. Can be collected by GC
    VectorPtr    = ALLOCATED_TYPE + 2,  // Data is pointer to Vector. Params not used. Can be collected by GC
    VectorIndex  = 12,                  // Data is pointer to Vector, Params is index into vector.

    DebugStringPtr = 20,                           // A string debug pointer, used for symbols / tracing
    SmallString = 21,                              // A small string, no allocation. Params + Data contain up to 7 characters.
    StaticStringPtr = 22,                          // Data is a pointer to a static string in tag code vector. Ignored by GC. Params not used.
    StringPtr = ALLOCATED_TYPE + StaticStringPtr,  // Data is pointer to dynamic string in arena memory. Params not used. Can be collected by GC

    // This special value means the interpreter needs to pause for input.
    // The triggering opcode will be repeated when more input is available
    MustWait = 250,

    Flag = 0xFF // A marker for internal testing
};

// Fixed-size 64-bit operand for the runtime
typedef struct DataTag {
    uint32_t type : 8;
    uint32_t params : 24;

    uint32_t data;
} DataTag;

// Functions to pack and un-pack data tags

// Returns true if this token is a pointer to allocated data, false if this token contains its own value
bool IsAllocated(DataTag token);// { return token.type & ALLOCATED_TYPE > 0; };

// return a tag that is never valid (denotes an error)
DataTag InvalidTag();
// returns false if the tag is of `Invalid` type, true otherwise
bool IsTagValid(DataTag t);
// returns true if the tags are both the same type, params and data. False otherwise.
bool TagsAreEqual(DataTag a, DataTag b);

// Value tagged as an non return type
DataTag VoidReturn();
DataTag UnitReturn();
DataTag NonResult();
DataTag RuntimeError(uint32_t bytecodeLocation);

DataTag MarkEndOfSubProgram();
DataTag MarkEndOfProgram();

// Interpreter has to pause
DataTag MustWait(uint32_t resumePosition);

// Encode an op-code with up to 2x16 bit params
DataTag EncodeOpcode(char codeClass, char codeAction, uint16_t p1, uint16_t p2);
// Encode an op-code with 1x32 bit param
DataTag EncodeLongOpcode(char codeClass, char codeAction, uint32_t p1);
// Decode an op-code tag that uses up to 2x16 bit params
void DecodeOpcode(DataTag encoded, char* codeClass, char* codeAction, uint16_t* p1, uint16_t* p2);
// Decode an op-code that uses up to 1x32 bit param
void DecodeLongOpcode(DataTag encoded, char* codeClass, char* codeAction, uint32_t* p1);

// Crush and encode a name (such as a function or variable name) as a variable ref
DataTag EncodeVariableRef(String* fullName, uint32_t* outCrushedName);
// Encode an already crushed name as a variable ref
DataTag EncodeVariableRef(uint32_t crushedName);
// Extract an encoded reference name from a tag
uint32_t DecodeVariableRef(DataTag encoded);
// Get hash code of names, as created by variable reference op codes
uint32_t GetCrushedName(String* fullName);
// Get hash code of names, as created by variable reference op codes
uint32_t GetCrushedName(const char* fullName);

// Encode a pointer with a type
DataTag EncodePointer(uint32_t ptrTarget, DataType type);
// Decode a previously encoded pointer
uint32_t DecodePointer(DataTag encoded);

DataTag EncodeInt32(int32_t original);
int32_t DecodeInt32(DataTag encoded);

DataTag EncodeBool(bool b);
bool DecodeBool(DataTag encoded);

// append short string contents to a mutable string
void DecodeShortStr(DataTag token, String* target);
// Encode the first 7 characters of a string into a tag. The chars are removed from the original string.
DataTag EncodeShortStr(String* str);

// Encode a single character as a short string
DataTag EncodeShortStr(char c);

// Encode a marker tag for development use
DataTag EncodeVisualMarker();

// append a human-readable summary of the token to a mutable string. Symbol table is optional
void DescribeTag(DataTag token, String* target, HashMap* symbols);

#endif