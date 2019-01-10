#include "String.h"
#include "Vector.h"

#include <stdlib.h>

typedef struct String {
    Vector chars; // vector of characters
    uint32_t hashval; // cached hash value
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
}

unsigned int StringLength(String * str) {
    return VLength(&str->chars);
}

char StringCharAtIndex(String *str, unsigned int idx) {
    char val = 0;
    auto ok = VCopy_char(&str->chars, idx, &val);
    if (!ok) return 0;
    return val;
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
