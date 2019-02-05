#include "TagCodeWriter.h"
#include "HashMap.h"

#include "MemoryManager.h"



bool TCW_IntKeyCompare(void* key_A, void* key_B) {
    auto A = *((uint32_t*)key_A);
    auto B = *((uint32_t*)key_B);
    return A == B;
}
unsigned int TCW_IntKeyHash(void* key) {
    auto A = *((uint32_t*)key);
    return A;
}

typedef String* StringPtr;

RegisterHashMapStatics(Map)
RegisterHashMapFor(int, StringPtr, TCW_IntKeyHash, TCW_IntKeyCompare, Map)
RegisterHashMapFor(int, int, TCW_IntKeyHash, TCW_IntKeyCompare, Map)

RegisterVectorStatics(Vec)
RegisterVectorFor(StringPtr, Vec)
RegisterVectorFor(DataTag, Vec)
RegisterVectorFor(char, Vec)
RegisterVectorFor(HashMap_KVP, Vec)

typedef struct TagCodeCache {
    // Literal strings to be written into data section of code
    Vector* _stringTable; // Vector of string

    // Nan-boxed opcodes and values
    Vector* _opcodes; // Vector of DataTag

    // Names that we've hashed
    HashMap* _symbols; // Map of uint32_t -> String (the crush hash to the original symbol name)

    // any errors that have been found
    Vector* _errors; // Vector of string

    // True, if this tcc is a fragment that returns non-void
    bool _returnsValues; // set externally
} TagCodeCache;

TagCodeCache * TCW_Allocate() {
    auto result = (TagCodeCache*)mmalloc(sizeof(TagCodeCache));
    if (result == NULL) return NULL;

    result->_opcodes = VecAllocate_DataTag();
    result->_stringTable = VecAllocate_StringPtr();
    result->_symbols = MapAllocate_int_StringPtr(1024);

    if (result->_opcodes == NULL || result->_stringTable == NULL || result->_symbols == NULL) {
        TCW_Deallocate(result);
        return NULL;
    }

    return result;
}

void TCW_Deallocate(TagCodeCache* tcc) {
    if (tcc == NULL) return;

    if (tcc->_opcodes != NULL) mfree(tcc->_opcodes);
    if (tcc->_stringTable != NULL) mfree(tcc->_stringTable);
    if (tcc->_symbols != NULL) mfree(tcc->_symbols);

    mfree(tcc);
}

bool TCW_ReturnsValues(TagCodeCache* tcc) {
    if (tcc == NULL) return false;
    return tcc->_returnsValues;
}

void TCW_ReturnsValues(TagCodeCache* tcc, bool v) {
    if (tcc == NULL) return;
    tcc->_returnsValues = v;
}

bool TCW_HasErrors(TagCodeCache* tcc) {
    if (tcc == NULL) return false;
    if (tcc->_errors == NULL) return false;
    return true;
}

Vector* TCW_ErrorList(TagCodeCache* tcc) {
    if (tcc == NULL) return NULL;
    return tcc->_errors;
}

DataTag InvalidTag() { return DataTag{0,0,0}; }

DataTag TCW_OpCodeAtIndex(TagCodeCache* tcc, int index) {
    if (tcc == NULL) return InvalidTag();
    if (VecLength(tcc->_opcodes) >= index) return InvalidTag();

    DataTag result;
    VecCopy_DataTag(tcc->_opcodes, index, &result);

    return result;
}

int TCW_Position(TagCodeCache* tcc) {
    if (tcc == NULL) return -1;
    return VecLength(tcc->_opcodes);
}

int TCW_OpCodeCount(TagCodeCache* tcc) {
    if (tcc == NULL) return -1;
    return VecLength(tcc->_opcodes);
}

void TCW_Merge(TagCodeCache* dest, TagCodeCache* fragment) {
    if (dest == NULL || fragment == NULL) return;

    auto codes = fragment->_opcodes;
    int srcLength = VecLength(codes);
    auto strings = fragment->_stringTable;

    TCW_AddSymbols(dest, fragment->_symbols);

    for (int i = 0; i < srcLength; i++) {
        auto code = VecGet_DataTag(codes, i);
        switch ((DataType)(code->type)) {
        case DataType::Invalid:
            VecPush_DataTag(dest->_opcodes, *code);
            break;

        case DataType::DebugStringPtr:
        case DataType::StaticStringPtr:
        case DataType::StringPtr:
            TCW_LiteralString(dest, *VecGet_StringPtr(strings, code->data));
            break;

        default:
            VecPush_DataTag(dest->_opcodes, *code);
            break;
        }
    }
}

// Number of 8-byte chunks required to store this string
int CalculatePaddedSize(String* s) {
    int bytes = StringLength(s);
    return bytes / 8 + (bytes % 8 != 0);
}

// storage size of a vector of strings
int SumOfPaddedSize(Vector* v) {
    auto len = VectorLength(v);
    int sum = 0;
    for (int i = 0; i < len; i++) {
        sum += CalculatePaddedSize(*VecGet_StringPtr(v, i));
    }
    return sum;
}

void WriteCode(Vector* output, DataTag tag) {
    auto ptr = (char*)(&tag);

    for (int i = 0; i < 8; i++) {
        VecPush_char(output, *ptr);
        ptr++;
    }
}
void WriteData(Vector* output, void* data, int length) {
    auto ptr = (char*)(data);

    for (int i = 0; i < length; i++) {
        VecPush_char(output, *ptr);
        ptr++;
    }
}
// Append a string data to the output vector, consuming the string
void WriteString(Vector* output, String* staticStr) {
    char c = StringDequeue(staticStr);

    while (c != '\0') {
        VecPush_char(output, c);
        c = StringDequeue(staticStr);
    }
}

Vector* TCW_WriteToStream(TagCodeCache* tcc) {
    auto output = VecAllocate_char();
    // a string is [Integer Tag: byte length] [string bytes, padded to 8 byte chunks]

    // 1) Calculate the string table size
    int dataLength = SumOfPaddedSize(tcc->_stringTable) + VecLength(tcc->_stringTable);

    // 2) Write a jump command to skip the table
    auto jumpCode = EncodeLongOpcode('c', 's', dataLength);
    WriteCode(output, jumpCode);

    // 3) Write the strings, with a mapping dictionary
    long location = 1; // counting initial jump as 0
    int stringTableCount = VecLength(tcc->_stringTable);
    auto mapping = MapAllocate_int_int(1024);
    for (int index = 0; index < stringTableCount; index++) {
        String* staticStr = NULL;
        VecDequeue_StringPtr(tcc->_stringTable, &staticStr);

        auto bytes = StringLength(staticStr);
        auto chunks = bytes / 8 + (bytes % 8 != 0);
        auto padSize = (chunks * 8) - bytes;

        MapPut_int_int(mapping, index, location, true);

        auto headerOpCode = EncodeInt32(bytes);
        WriteCode(output, headerOpCode);
        location++;

        WriteString(output, staticStr);
        location += chunks;
        StringDeallocate(staticStr);

        for (int p = 0; p < padSize; p++) { VecPush_char(output, 0); }
    }

    // 4) Write the op-codes
    int opCodeCount = VecLength(tcc->_opcodes);
    for (int index = 0; index < stringTableCount; index++) {
        DataTag code = {};
        VecDequeue_DataTag(tcc->_opcodes, &code);

        switch ((DataType)(code.type)) {
        case DataType::DebugStringPtr:
        case DataType::StringPtr:
        case DataType::StaticStringPtr:
        {
            // Re-write to new location
            auto original = code.data;
            int* final = NULL;
            MapGet_int_int(mapping, original, &final);
            WriteCode(output, EncodePointer(*final, DataType::StaticStringPtr));
            break;
        }

        default:
            WriteCode(output, code);
            break;
        }
    }
    return output;
}


bool TCW_AddSymbols(TagCodeCache* tcc, HashMap* sym) {
    if (tcc == NULL || sym == NULL) return false;

    auto content = MapAllEntries(sym);

    bool ok = true;
    HashMap_KVP entry;
    while (VecPeek_HashMap_KVP(content, &entry)) {
        ok = ok && TCW_AddSymbol(tcc, *(int32_t*)entry.Key, *(StringPtr*)entry.Value);
    }
    VecDeallocate(content);
    return ok;
}

HashMap* TCW_GetSymbols(TagCodeCache* tcc) {
    if (tcc == NULL) return NULL;
    return tcc->_symbols;
}

void TCW_Comment(TagCodeCache* tcc, String* s) {
    // TODO: write this out a symbols/docs file
}

void TCW_VariableReference(TagCodeCache* tcc, String* valueName) {
    if (tcc == NULL) return;

    uint32_t crush;
    DataTag tag = EncodeVariableRef(valueName, &crush);
    VecPush_DataTag(tcc->_opcodes, tag);
    TCW_AddSymbol(tcc, crush, valueName);
}

void TCW_Memory(TagCodeCache* tcc, char action, String* targetName, int paramCount) {
    if (tcc == NULL) return;

    // TODO: direct memory access if we can know we have scope / param name etc. (save a hash lookup)
    uint32_t crush;
    switch (action) {
    case 's':
    {
        if (paramCount > 1) { // set with index (for arrays, strings, maps, etc)
            VecPush_DataTag(tcc->_opcodes, EncodeVariableRef(targetName, &crush));  // reference the name
            TCW_AddSymbol(tcc, crush, targetName);                                  // add to symbols
            VecPush_DataTag(tcc->_opcodes, EncodeOpcode('m', 'S', paramCount, 0));  // add memory action
            break;
        } else goto defaultCase;
    }

    case 'g':
    {
        if (paramCount > 0) { // get with index (for arrays, strings, maps, etc)
            VecPush_DataTag(tcc->_opcodes, EncodeVariableRef(targetName, &crush));  // reference the name
            TCW_AddSymbol(tcc, crush, targetName);                                  // add to symbols
            VecPush_DataTag(tcc->_opcodes, EncodeOpcode('m', 'G', paramCount, 0));  // add memory action
            break;
        } else goto defaultCase;
    }

    defaultCase:
    default: // for simple get/set, we encode the reference directly into the opcode
        EncodeVariableRef(targetName, &crush);                                  // get the crushed name
        TCW_AddSymbol(tcc, crush, targetName);                                  // add to symbols
        VecPush_DataTag(tcc->_opcodes, EncodeLongOpcode('m', action, crush));   // add memory action
        break;
    }
}

void TCW_Increment(TagCodeCache* tcc, int8_t incr, String* targetName) {
    if (tcc == NULL) return;

    uint32_t crush;
    EncodeVariableRef(targetName, &crush);
    TCW_AddSymbol(tcc, crush, targetName);
    VecPush_DataTag(tcc->_opcodes, EncodeLongOpcode('i', incr, crush));
}

void TCW_FunctionCall(TagCodeCache* tcc, String* functionName, int parameterCount) {
    if (tcc == NULL) return;

    VecPush_DataTag(tcc->_opcodes, EncodeVariableRef(functionName, NULL));  // reference the function name
    VecPush_DataTag(tcc->_opcodes, EncodeOpcode('f', 'c', (uint16_t)parameterCount, 0));  // call function
}

void TCW_FunctionDefine(TagCodeCache* tcc, String* functionName, int argCount, int tokenCount) {
    if (tcc == NULL) return;

    uint32_t crush;
    VecPush_DataTag(tcc->_opcodes, EncodeVariableRef(functionName, &crush));  // reference the function name
    TCW_AddSymbol(tcc, crush, functionName);
    VecPush_DataTag(tcc->_opcodes, EncodeOpcode('f', 'd', argCount, tokenCount));  // call function
}

void TCW_Exception(TagCodeCache* tcc, String* message) {
    if (tcc->_errors == NULL) {
        tcc->_errors = VecAllocate_StringPtr();
    }
    VecPush_StringPtr(tcc->_errors, message);
}

bool TCW_AddSymbol(TagCodeCache* tcc, uint32_t crushed, String* name) {
    if (tcc == NULL || name == NULL) return false;

    auto ok = MapPut_int_StringPtr(tcc->_symbols, crushed, name, false);
    if (ok == true) return true;
        
    // otherwise, we have a collision
    StringPtr* str = NULL;
    ok = MapGet_int_StringPtr(tcc->_symbols, crushed, &str);

    if (StringAreEqual(name, *str)) return true; // same name as before

    // If we het to here, the compiler name crushing has failed.
    // We try to let the programmer know how to work around our limitations
    auto errMsg = StringNew("Hash collision between symbols!  This is a compiler limitation, sorry. Try renaming '");
    StringAppend(errMsg, *str);
    StringAppend(errMsg, "' or '");
    StringAppend(errMsg, name);
    StringAppend(errMsg, "'");
    TCW_Exception(tcc, errMsg);
    return false;
}

void TCW_InvalidReturn(TagCodeCache* tcc) {
    if (tcc == NULL) return;
    VecPush_DataTag(tcc->_opcodes, EncodeOpcode('c', 't', 0, 0));  // always fail
}

void TCW_Return(TagCodeCache* tcc, int pCount) {
    if (tcc == NULL) return;
    VecPush_DataTag(tcc->_opcodes, EncodeOpcode('c', 'r', 0, pCount));
}

void TCW_CompoundCompareJump(TagCodeCache* tcc, CmpOp operation, uint16_t argCount, uint16_t opCodeCount) {
    if (tcc == NULL) return;
    VecPush_DataTag(tcc->_opcodes, EncodeOpcode('C', (char)operation, argCount, opCodeCount));
}

void TCW_CompareJump(TagCodeCache* tcc, int opCodeCount) {
    if (tcc == NULL) return;
    VecPush_DataTag(tcc->_opcodes, EncodeLongOpcode('c', 'c', opCodeCount));
}

void TCW_UnconditionalJump(TagCodeCache* tcc, int opCodeCount) {
    if (tcc == NULL) return;
    VecPush_DataTag(tcc->_opcodes, EncodeLongOpcode('c', 'j', opCodeCount));
}

void TCW_LiteralNumber(TagCodeCache* tcc, double d) {
    if (tcc == NULL) return;
    // TODO: better number support, using the 42 bits rather than 32.
    VecPush_DataTag(tcc->_opcodes, EncodeInt32(d));
}

void TCW_LiteralString(TagCodeCache* tcc, String* s) {
    if (tcc == NULL || s == NULL) return;

    // duplication check (only need 1 copy of any given static literal)
    int len = VecLength(tcc->_stringTable);
    for (int i = 0; i < len; i++) {
        auto match = StringAreEqual(s, *VecGet_StringPtr(tcc->_stringTable, i));
        // found duplicate. Reference and leave
        VecPush_DataTag(tcc->_opcodes, EncodePointer(i, DataType::StaticStringPtr));
        return;
    }

    // no existing matches
    VecPush_StringPtr(tcc->_stringTable, s);
    VecPush_DataTag(tcc->_opcodes, EncodePointer(len, DataType::StaticStringPtr));
}

void TCW_RawToken(TagCodeCache* tcc, DataTag value) {
    if (tcc == NULL) return;
    VecPush_DataTag(tcc->_opcodes, value);
}
