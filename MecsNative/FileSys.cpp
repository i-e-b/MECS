
#include "FileSys.h"
#include "MemoryManager.h"
// Using MM restricts us to 64K path lengths. Should be OK for now.
//#include <stdlib.h>

#ifdef WIN32

#include <stdio.h>

bool fileWriteModeWindows(String* path, Vector* buffer, const char* mode) {
    if (path == NULL || buffer == NULL) return false;
    if (VectorElementSize(buffer) != 1) return false; // not a byte-size vector

    auto realPath = StringNew("C:\\Temp\\MECS\\"); // jail for testing on Windows
    StringAppend(realPath, path);

    auto cpath = StringToCStr(realPath);
    auto file = fopen(cpath, mode);

    StringDeallocate(realPath);
    mfree(cpath);

    char c = 0;
    while (VectorDequeue(buffer, &c)) {
        if (fputc(c, file) == EOF) break;
    }

    fclose(file);

    return VectorLength(buffer) == 0;
}

bool FileWriteAll(String* path, Vector* buffer) {
    return fileWriteModeWindows(path, buffer, "wb"); // truncate and write binary
}

// Write a file, appending any existing. Reads from a vector of bytes
bool FileAppendAll(String* path, Vector* buffer) {
    return fileWriteModeWindows(path, buffer, "ab"); // create or append binary
}

bool FileLoadChunk(String* path, Vector* buffer, uint64_t start, uint64_t end, uint64_t* actual) {
    if (path == NULL || buffer == NULL) return false;
    if (VectorElementSize(buffer) < 1 || !VectorIsValid(buffer)) return false; // not valid destination

    auto realPath = StringNew("C:\\Temp\\MECS\\"); // jail for testing on Windows
    StringAppend(realPath, path);

    auto cpath = StringToCStr(realPath);
    auto file = fopen(cpath, "rb");

    StringDeallocate(realPath);
    mfree(cpath);

    if (file == NULL) {
        return false;
    }

    int err = _fseeki64(file, start, SEEK_SET);
    if (err) {
        fclose(file);
        return false;
    }

    int elemSize = VectorElementSize(buffer);
    char* elemBuffer = (char*)mmalloc(elemSize);
    int idx = 0;

    auto len = end - start;
    uint64_t readBytes = 0;
    while (len --> 0) {
        int r = fgetc(file);
        if (r < 0) break;
        //char c = r;
        elemBuffer[idx] = r;
        readBytes++;

        idx++;
        if (idx == elemSize) {
            idx = 0;
            VectorPush(buffer, elemBuffer);
        }
    }

    if (actual != NULL) *actual = readBytes;

    mfree(elemBuffer);
    return true;
}

#endif

#ifdef RASPI

bool FileLoadChunk(String* path, Vector* buffer, uint64_t start, uint64_t end, uint64_t* actual) {
    // TODO: SD card reading routines here
}

bool FileWriteAll(String* path, Vector* buffer) {
    // TODO: SD card reading routines here
}

#endif