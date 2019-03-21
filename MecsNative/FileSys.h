#pragma once

#ifndef filesys_h
#define filesys_h

#include "String.h"
#include "Vector.h"
#include <stdint.h>

// Largest file supported (not full 64-bits)
#define FILE_LOAD_ALL 0xFFFFFFFF

// Read from the file system. Appends a fragment of a file to a preallocated vector of bytes.
bool FileLoadChunk(String* path, Vector* buffer, uint64_t start, uint64_t end, uint64_t* actual);

// Write a file, replacing any existing. Reads from a vector of bytes
// This removes entries from the buffer, but does not deallocate it.
bool FileWriteAll(String* path, Vector* buffer);

// Write a file, appending any existing. Reads from a vector of bytes
// This removes entries from the buffer, but does not deallocate it.
bool FileAppendAll(String* path, Vector* buffer);

#endif
