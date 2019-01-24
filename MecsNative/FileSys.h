#pragma once

#ifndef filesys_h
#define filesys_h

#include "String.h"
#include "Vector.h"
#include <stdint.h>

// Read from the file system. Appends a fragment of a file to a preallocated vector of bytes.
bool FileLoadChunk(String* path, Vector* buffer, uint64_t start, uint64_t end, uint64_t* actual);

// Write a file, replacing any existing. Reads from a vector of bytes
bool FileWriteAll(String* path, Vector* buffer);

// Write a file, appending any existing. Reads from a vector of bytes
bool FileAppendAll(String* path, Vector* buffer);

#endif
