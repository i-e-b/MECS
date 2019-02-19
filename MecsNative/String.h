#pragma once

#ifndef string_h
#define string_h

#include "Vector.h"
#include <stdint.h>

// A mutable variable length string structure
typedef struct String String;
typedef String* StringPtr;

// Create an empty string
String *StringEmpty();
// Create a mutable string from a c-string
String *StringNew(const char *str);
// Create a mutable string from a single character
String *StringNew(char c);
// Append, somewhat like sprintf. `fmt` is taken literally, except for these low ascii chars: '\x01'=(String*); '\x02'=int as dec; '\x03'=int as hex;
String * StringNewFormat(const char* fmt, ...);
// Clear the contents of a string, but leave it allocated
void StringClear(String *str);

// Make a shallow copy of a string. Can be deallocated without affecting the original.
// But all other operations affect the original
String* StringProxy(String* original);
// Deallocate a string. Ignores if you pass NULL
void StringDeallocate(String *str);
// Returns true if the string is valid, false if it has been deallocated or damaged
bool StringIsValid(String *str);

// Length of a string
unsigned int StringLength(String* str);
// Get char at index. Returns 0 if invalid. Negative indexes are from end
char StringCharAtIndex(String *str, int idx);

// Add a newline character
void StringNL(String *str);
// Add one string to another. `first` is modified.
void StringAppend(String *first, String *second);
// Add one string to another. `first` is modified.
void StringAppend(String *first, const char *second);
// Add a character to the end of a string
void StringAppendChar(String *str, char c);
// Add a character to the end of a string, that character repeated a number of times
void StringAppendChar(String *str, char c, int count);
// Append, somewhat like sprintf. `fmt` is taken literally, except for these low ascii chars: '\x01'=(String*); '\x02'=int as dec; '\x03'=int as hex;
void StringAppendFormat(String *str, const char* fmt, ...);


// Create a new string from a range in an existing string. The existing string is not modified
String *StringSlice(String* str, int startIdx, int length);
// Create a new string from a range in an existing string, and DEALLOCATE the original string
String *StringChop(String* str, int startIdx, int length);

// remove and return the first char of the string. If string is empty, returns '\0'
char StringDequeue(String* str);

// Generate a hash-code for a string. Guaranteed to be non-zero for valid strings.
uint32_t StringHash(String* str);
// Alloc and copy a new c-string from a mutable string. The result must be deallocated with `free()`
char *StringToCStr(String *str);
// Access the underlying byte vector of the string
Vector* StringGetByteVector(String* str);

// Change all upper-case letters to lower case, in place (existing string is modified)
void StringToLower(String *str);
// Change all lower-case letters to upper case, in place (existing string is modified)
void StringToUpper(String *str);

// Find the position of a substring. If the outPosition is NULL, it is ignored
bool StringFind(String* haystack, String* needle, unsigned int start, unsigned int* outPosition);
// Find the position of a substring. If the outPosition is NULL, it is ignored
bool StringFind(String* haystack, const char * needle, unsigned int start, unsigned int* outPosition);
// Find the next position of a character. If the outPosition is NULL, it is ignored
bool StringFind(String* haystack, char needle, unsigned int start, unsigned int* outPosition);

// Find any number of instances of a substring. Each one is replaced with a new substring
// in the output string.
String* StringReplace(String* haystack, String* needle, String* replacement);

// Test if one string starts with another
bool StringStartsWith(String* haystack, String *needle);
bool StringStartsWith(String* haystack, const char* needle);
// Test if one string ends with another
bool StringEndsWith(String* haystack, String *needle);
bool StringEndsWith(String* haystack, const char* needle);
// Test if two strings are equal
bool StringAreEqual(String* a, String* b);
bool StringAreEqual(String* a, const char* b);


// Append an integer as a decimal string
void StringAppendInt32(String *str, int32_t value);
// Append an integer as a fixed length hex string
void StringAppendInt32Hex(String *str, uint32_t value);
// Append an long integer as a fixed length hex string
void StringAppendInt64Hex(String *str, uint64_t value);
// Append a fixed-point number as a decimal string (F16.16, passed as int32)
void StringAppendF16(String *str, int32_t value); // TODO...

// Parse an int from a decimal string
bool StringTryParse_int32(String *str, int32_t *dest);
// Parse a fixed-point number from a decimal string (F16.16, passed as int32)
bool StringTryParse_f16(String *str, int32_t *dest); // TODO

#endif
