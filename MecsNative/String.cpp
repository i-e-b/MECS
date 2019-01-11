#include "String.h"
#include "Vector.h"

#include <stdlib.h>

typedef struct String {
    Vector chars; // vector of characters
    uint32_t hashval; // cached hash value. Any time we change the string, this should be set to 0.
} String;

RegisterVectorStatics(V)
RegisterVectorFor(char, V)

String * StringEmpty() {
    auto vec = VAllocate_char();
    if (!vec.IsValid) return NULL;

    auto str = (String*)calloc(1, sizeof(String));
    str->chars = vec;
    str->hashval = 0;

    return str;
}

void StringDeallocate(String *str) {
    if (str == NULL) return;
    if (str->chars.IsValid == true) VDeallocate(&str->chars);
    free(str);
}

String * StringNew(const char * str) {
    auto result = StringEmpty();
    if (result == NULL) return NULL;
    if (result->chars.IsValid == false) return NULL;

    while (*str != 0) {
        VPush_char(&result->chars, *str);
        str++;
    }
    return result;
}

void StringAppend(String *first, String *second) {
    unsigned int len = VLength(&second->chars);
    for (unsigned int i = 0; i < len; i++) {
        VPush_char(&first->chars, *VGet_char(&second->chars, i));
    }
    first->hashval = 0;
}

void StringAppend(String *first, const char *second) {
    while (*second != 0) {
        VPush_char(&first->chars, *second);
        second++;
    }
    first->hashval = 0;
}

unsigned int StringLength(String * str) {
    if (str == NULL) return 0;
    return VLength(&str->chars);
}

char StringCharAtIndex(String *str, int idx) {
    char val = 0;
    if (idx < 0) { // from end
        idx += VLength(&str->chars);
    }
    auto ok = VCopy_char(&str->chars, idx, &val);
    if (!ok) return 0;
    return val;
}

String *StringSlice(String* str, int startIdx, int length) {
    if (str == NULL) return NULL;
    auto len = StringLength(str);
    if (len < 1) return NULL;

    String *result = StringEmpty();
    while (startIdx < 0) { startIdx += len; }

    for (int i = 0; i < length; i++) {
        uint32_t x = (i + startIdx) % len;
        VPush_char(&result->chars, *VGet_char(&str->chars, x));
    }

    return result;
}

String *StringChop(String* str, int startIdx, int length) {
    auto result = StringSlice(str, startIdx, length);
    StringDeallocate(str);
    return result;
}

char *StringToCStr(String *str) {
    auto len = StringLength(str);
    auto result = (char*)malloc(1 + (sizeof(char) * len)); // need extra byte for '\0'
    for (unsigned int i = 0; i < len; i++) {
        result[i] = *VGet_char(&str->chars, i);
    }
    result[len] = 0;
    return result;
}

char *StringToCStr(String *str, int start) {
    auto len = StringLength(str);
    if (start < 0) { // from end
        start += len;
    }
    len -= start;

    auto result = (char*)malloc(1 + (sizeof(char) * len)); // need extra byte for '\0'
    for (unsigned int i = 0; i < len; i++) {
        result[i] = *VGet_char(&str->chars, i + start);
    }
    result[len] = 0;
    return result;
}

uint32_t StringHash(String* str) {
    if (str == NULL) return 0;
    if (str->hashval != 0) return str->hashval;

    uint32_t len = StringLength(str);
    uint32_t hash = len;
    for (uint32_t i = 0; i < len; i++) {
        hash += *VGet_char(&str->chars, i);;
        hash ^= hash >> 16;
        hash *= 0x7feb352d;
        hash ^= hash >> 15;
        hash *= 0x846ca68b;
        hash ^= hash >> 16;
    }
    hash ^= len;
    hash ^= hash >> 16;
    hash *= 0x7feb352d;
    hash ^= hash >> 15;
    hash *= 0x846ca68b;
    hash ^= hash >> 16;
    hash += len;

    if (hash == 0) return 0x800800; // never return zero
    str->hashval = hash;
    return hash;
}

void StringToLower(String *str) {
    if (str == NULL) return;
    str->hashval = 0;
    // Simple 7-bit ASCII only at present
    uint32_t len = VLength(&str->chars);
    for (uint32_t i = 0; i < len; i++) {
        auto chr = VGet_char(&str->chars, i);
        if (*chr >= 'A' && *chr <= 'Z') {
            *chr = *chr + 0x20;
        }
    }
}

void StringToUpper(String *str) {
    if (str == NULL) return;
    str->hashval = 0;
    // Simple 7-bit ASCII only at present
    uint32_t len = VLength(&str->chars);
    for (uint32_t i = 0; i < len; i++) {
        auto chr = VGet_char(&str->chars, i);
        if (*chr >= 'a' && *chr <= 'z') {
            *chr = *chr - 0x20;
        }
    }
}

bool StringStartsWith(String* haystack, String *needle) {
    if (haystack == NULL) return false;
    if (needle == NULL) return true;
    auto len = StringLength(needle);
    if (len > StringLength(haystack)) return false;
    for (uint32_t i = 0; i < len; i++) {
        auto a = VGet_char(&haystack->chars, i);
        auto b = VGet_char(&needle->chars, i);
        if (*a != *b) return false;
    }
    return true;
}
bool StringStartsWith(String* haystack, const char* needle) {
    if (haystack == NULL) return false;
    if (needle == NULL) return true;
    auto limit = StringLength(haystack);
    uint32_t i = 0;
    while (needle[i] != 0) {
        if (i >= limit) return false;
        auto chr = VGet_char(&haystack->chars, i);
        if (*chr != needle[i]) return false;
        i++;
    }
    return true;
}


bool StringEndsWith(String* haystack, String *needle) {
    if (haystack == NULL) return false;
    if (needle == NULL) return true;
    auto len = StringLength(needle);
    auto off = StringLength(haystack);
    if (len > off) return false;
    off -= len;
    for (uint32_t i = 0; i < len; i++) {
        auto a = VGet_char(&haystack->chars, i + off);
        auto b = VGet_char(&needle->chars, i);
        if (*a != *b) return false;
    }
    return true;
}
bool StringEndsWith(String* haystack, const char* needle) {
    auto str2 = StringNew(needle);
    bool match = StringEndsWith(haystack, str2);
    StringDeallocate(str2);
    return match;
}

bool StringAreEqual(String* a, String* b) {
    if (a == NULL) return false;
    if (b == NULL) return false;
    uint32_t len = StringLength(a);
    if (len != StringLength(b)) return false;
    for (uint32_t i = 0; i < len; i++) {
        auto ca = VGet_char(&a->chars, i);
        auto cb = VGet_char(&b->chars, i);
        if (*ca != *cb) return false;
    }
    return true;
}
bool StringAreEqual(String* a, const char* b) {
    if (a == NULL) return false;
    if (b == NULL) return false;
    uint32_t limit = StringLength(a);
    uint32_t i = 0;
    while (b[i] != 0) {
        if (i >= limit) return false;
        auto chr = VGet_char(&a->chars, i);
        if (*chr != b[i]) return false;
        i++;
    }
    return i == limit;
}

bool StringFind(String* haystack, String* needle, unsigned int start, unsigned int* outPosition) {
    // get a few special cases out of the way
    if (haystack == NULL) return false;
    if (outPosition != NULL) *outPosition = 0;
    if (needle == NULL) return true; // treating null as empty

    uint32_t hayLen = VLength(&haystack->chars);
    uint32_t needleLen = VLength(&needle->chars);
    if (needleLen > hayLen) return false;
    if (start < 0) { // from end
        start += hayLen;
    }

    // Rabin–Karp method, but using sum rather than hash (more false positives, but much cheaper on average)
    // get a hash of the 'needle', and try to find somewhere in the haystack that matches.
    // double-check if we find one.
    char *matchStr = StringToCStr(needle);
    char *scanStr = StringToCStr(haystack, start);

    // make sum of the needle, and an initial sum of the scan hash
    int match = 0;
    int scan = 0;
    uint32_t i = 0;
    for (i = 0; i < needleLen; i++) {
        match += matchStr[i];
        scan += scanStr[i];
    }

    // now roll the hash until until we find a match or run out of string
    for (i = needleLen; i < hayLen; i++) {
        if (match == scan) { // possible match, double check
            uint32_t idx = i - needleLen;
            if (outPosition != NULL) *outPosition = idx + start;
            bool found = true;
            for (uint32_t j = 0; j < needleLen; j++) {
                if (matchStr[j] != scanStr[j + idx]) {
                    found = false;
                    break;
                }
            }
            if (!found) {
                scan -= scanStr[i - needleLen];
                scan += scanStr[i];
                continue;
            }

            // OK, this is a match
            free(matchStr);
            free(scanStr);
            return true;
        }

        scan -= scanStr[i - needleLen];
        scan += scanStr[i];
    }

    // Clean up.
    free(matchStr);
    free(scanStr);
    return false;
}
