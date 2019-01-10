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
// Deallocate a string
void StringDeallocate(String *str);
// Length of a string
unsigned int StringLength(String* str);
// Get char at index. Returns 0 if invalid
char StringCharAtIndex(String *str, unsigned int idx);

// Add one string to another. `first` is modified.
void StringAppend(String *first, String *second);
// Create a new string from a range in an existing string. The existing string is not modified
String *StringSlice(String* str, int startIdx, int length);
// Create a new string from a range in an existing string, and deallocate the original string
String *StringChop(String* str, int startIdx, int length);
// Generate a hash-code for a string. Guaranteed to be non-zero
uint32_t StringHash(String* str);
// Alloc and copy a new c-string from a mutable string. The result must be deallocated with `free()`
char *StringToCStr(String *str);

// Change all upper-case letters to lower case
void StringToLower(String *str);
// Change all lower-case letters to upper case
void StringToUpper(String *str);

// Find the position of a substring. If the outPosition is NULL, it is ignored
bool StringFind(String* haystack, String* needle, unsigned int start, unsigned int* outPosition);
// Test if one string starts with another
bool StringStartsWith(String* haystack, String *needle);
// Test if one string ends with another
bool StringEndsWith(String* haystack, String *needle);
// Test if two strings are equal
bool StringAreEqual(String* a, String* b);

#endif
