#pragma once

#ifndef string_h
#define string_h

#include <stdint.h>

// A mutable variable length string structure
typedef struct String String;

// Create an empty string
String *StringEmpty();
// Create a mutable string from a c-string
String *StringNew(const char *str);
// Clear the contents of a string, but leave it allocated
void StringClear(String *str);

// Deallocate a string
void StringDeallocate(String *str);

// Length of a string
unsigned int StringLength(String* str);
// Get char at index. Returns 0 if invalid. Negative indexes are from end
char StringCharAtIndex(String *str, int idx);

// Add one string to another. `first` is modified.
void StringAppend(String *first, String *second);
void StringAppend(String *first, const char *second);
// Create a new string from a range in an existing string. The existing string is not modified
String *StringSlice(String* str, int startIdx, int length);
// Create a new string from a range in an existing string, and DEALLOCATE the original string
String *StringChop(String* str, int startIdx, int length);
// Generate a hash-code for a string. Guaranteed to be non-zero for valid strings.
uint32_t StringHash(String* str);
// Alloc and copy a new c-string from a mutable string. The result must be deallocated with `free()`
char *StringToCStr(String *str);

// Change all upper-case letters to lower case, in place (existing string is modified)
void StringToLower(String *str);
// Change all lower-case letters to upper case, in place (existing string is modified)
void StringToUpper(String *str);

// Find the position of a substring. If the outPosition is NULL, it is ignored
bool StringFind(String* haystack, String* needle, unsigned int start, unsigned int* outPosition);
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
// Append a fixed-point number as a decimal string (F16.16, passed as int32)
void StringAppendF16(String *str, int32_t value); // TODO...

// Parse an int from a decimal string
bool StringTryParse_int32(String *str, int32_t *dest); // TODO
// Parse a fixed-point number from a decimal string (F16.16, passed as int32)
bool StringTryParse_f16(String *str, int32_t *dest); // TODO

#endif
