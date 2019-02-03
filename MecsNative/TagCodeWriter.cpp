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

typedef struct TagCodeCache {
    // Literal strings to be written into data section of code
    Vector* _stringTable; // Vector of string

    // Nan-boxed opcodes and values
    Vector* _opcodes; // Vector of DataTag

    // Names that we've hashed
    HashMap* _symbols; // Map of uint32_t -> String (the crush hash to the original symbol name)
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


void TCW_AddSymbols(TagCodeCache* tcc, HashMap* sym) {
    // TODO
}

void TCW_LiteralString(TagCodeCache* tcc, String* s) {
    // TODO
}
