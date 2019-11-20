#include "TagData.h"

RegisterHashMapStatics(Map)
RegisterHashMapFor(int, StringPtr, HashMapIntKeyHash, HashMapIntKeyCompare, Map)

bool IsAllocated(DataTag token) {
    return (token.type & ALLOCATED_TYPE) > 0;
};

DataTag VoidReturn() {
    return DataTag{ (int)DataType::Void, 0, 0 };
};
DataTag UnitReturn() {
    return DataTag{ (int)DataType::Unit, 0, 0 };
};
DataTag NonResult() {
    return DataTag{ (int)DataType::Not_a_Result, 0, 0 };
};
DataTag RuntimeError(uint32_t bytecodeLocation) {
    return DataTag{ (int)DataType::Exception, 0, bytecodeLocation };
};

DataTag MarkEndOfSubProgram() {
    return DataTag{ (int)DataType::EndOfSubProgram, 0, 0 };
}
DataTag MarkEndOfProgram() {
    return DataTag{ (int)DataType::EndOfProgram, 0, 0 };
}
DataTag MustWait(uint32_t resumePosition) {
    return DataTag{ (int)DataType::MustWait, 0, resumePosition };
};

// return a tag that is never valid (denotes an error)
DataTag InvalidTag() {
    return DataTag{ (int)DataType::Invalid, 0, 0 };
}
// returns false if the tag is of Invalid, NaR, Void types; true otherwise
bool IsTagValid(DataTag t) {
    return t.type != (int)DataType::Invalid
        && t.type != (int)DataType::Not_a_Result
        && t.type != (int)DataType::Void;
}

bool TagsAreEqual(DataTag a, DataTag b) {
    if (a.data != b.data) return false;
    if (a.type != b.type) return false;
    if (a.params != b.params) return false;
    return true;
}

// Encode an op-code with up to 2x16 bit params
DataTag EncodeOpcode(char codeClass, char codeAction, uint16_t p1, uint16_t p2) {
    return DataTag{
        (int)DataType::Opcode,
        ((uint32_t)codeClass << 8) | (codeAction), // 16 bits, out of 24
        ((uint32_t)p1 << 16) | ((uint32_t)p2)
    };
}
// Encode an op-code with 1x32 bit param
DataTag EncodeLongOpcode(char codeClass, char codeAction, uint32_t p1) {
    return DataTag{
        (int)DataType::Opcode,
        ((uint32_t)(codeClass & 0xFF) << 8) | (codeAction & 0xFF),
        p1
    };
}

// Encode an op-code with 1x32 bit param, and an extra byte parameter
DataTag EncodeWideLongOpcode(char codeClass, char codeAction, uint32_t p1, uint8_t p3) {
    return DataTag{
        (int)DataType::Opcode,
        ((uint32_t)(p3 & 0xFF) << 16) |((uint32_t)(codeClass & 0xFF) << 8) | (codeAction & 0xFF),
        p1
    };
}

// Decode an op-code tag that uses up to 2x16 bit params
void DecodeOpcode(DataTag encoded, char* codeClass, char* codeAction, uint16_t* p1, uint16_t* p2, uint8_t* p3) {
    if (codeClass != NULL) {
        *codeClass = (encoded.params >> 8) & 0xFF;
    }
    if (codeAction != NULL) {
        *codeAction = encoded.params & 0xFF;
    }
    if (p1 != NULL) {
        *p1 = encoded.data >> 16;
    }
    if (p2 != NULL) {
        *p2 = encoded.data & 0xFFFF;
    }
    if (p3 != NULL) {
        *p3 = (encoded.params >> 16) & 0xFF;
    }
}

// Decode an op-code that uses 1x32 bit param
void DecodeLongOpcode(DataTag encoded, char* codeClass, char* codeAction, uint32_t* p1, uint8_t* p3) {
    if (codeClass != NULL) {
        *codeClass = (encoded.params >> 8) & 0xFF;
    }
    if (codeAction != NULL) {
        *codeAction = encoded.params & 0xFF;
    }
    if (p1 != NULL) {
        *p1 = encoded.data;
    }
    if (p3 != NULL) {
        *p3 = (encoded.params >> 16) & 0xFF;
    }
}

// Crush and encode a name (such as a function or variable name) as a variable ref
DataTag EncodeVariableRef(String* fullName, uint32_t* outCrushedName) {
    if (fullName == NULL) {
        return DataTag{ (int)DataType::Invalid, 0, 0 };
    }

    auto hash = StringHash(fullName);
    if (outCrushedName != NULL) *outCrushedName = hash;
    return DataTag{ (int)DataType::VariableRef, 0, hash };
}

// Encode an already crushed name as a variable ref
DataTag EncodeVariableRef(uint32_t crushedName) {
    return DataTag{ (int)DataType::VariableRef, 0, crushedName };
}
// Extract an encoded reference name from a tag
uint32_t DecodeVariableRef(DataTag encoded) {
    return encoded.data;
}
// Get hash code of names, as created by variable reference op codes
uint32_t GetCrushedName(String* fullName) {
    return StringHash(fullName);
}
uint32_t GetCrushedName(const char* fullName) {
    auto tmp = StringNew(fullName);
    auto res = GetCrushedName(tmp);
    StringDeallocate(tmp);
    return res;
}

DataTag EncodePointer(uint32_t ptrTarget, DataType type) {
    DataTag t;
    t.type = (char)type;
    t.params = 0;
    t.data = ptrTarget;
    return t;
}

// Encode a reference to a value inside a vector
DataTag VectorIndexTag(uint32_t vectorPtrTarget, int index) {
    DataTag t;
    t.params = index;
    t.type = (char)DataType::VectorIndex;
    t.data = vectorPtrTarget;
    return t;
}

DataTag HashTableValue(uint32_t dataTagPtr) {
    DataTag t;
    t.params = 0;
    t.type = (char)DataType::HashtableEntryPtr;
    t.data = dataTagPtr;
    return t;
}

uint32_t DecodePointer(DataTag encoded) {
    return encoded.data;
}

DataTag EncodeInt32(int32_t original) {
    return DataTag{ (int)DataType::Integer, 0, (uint32_t)original };
}
int32_t DecodeInt32(DataTag encoded) {
    return encoded.data;
}


DataTag EncodeDouble(double original) {
    uint64_t* bits = (uint64_t*)(&original);
    uint32_t base = ((*bits) >>  8) & 0xFFFFFFFF;
    uint32_t head = ((*bits) >> 40) & 0x00FFFFFF;
    return DataTag{ (int)DataType::Fraction, head, base };
}

double DecodeDouble(DataTag encoded) {
    uint64_t bits = ((uint64_t)encoded.data << 8) | ((uint64_t)encoded.params << 40);
    double* output = (double*)(&bits);
    return *output;
}

DataTag EncodeBool(bool b) {
    return DataTag{ (int)DataType::Integer, 0, (b) ? 0xffffffffu : 0u };
}
bool DecodeBool(DataTag encoded) {
    return encoded.data == 0;
}

DataTag EncodeShortStr(String* str) {
    DataTag result = { (int)DataType::SmallString, 0, 0 };
    if (str == NULL) return result;

    for (int i = 0; i < 3; i++) {
        result.params |= StringDequeue(str) << (16 - (i*8));
    }
    for (int i = 0; i < 4; i++) {
        result.data |= StringDequeue(str) << (24 - (i * 8));
    }
    return result;
}

DataTag EncodeShortStr(const char* str) {
    DataTag result = { (int)DataType::SmallString, 0, 0 };
    if (str == NULL) return result;
    int j = 0;

    for (int i = 0; i < 3; i++) {
        if (str[j] == 0) break;
        result.params |= str[j] << (16 - (i*8));
        j++;
    }
    for (int i = 0; i < 4; i++) {
        if (str[j] == 0) break;
        result.data |= str[j] << (24 - (i * 8));
        j++;
    }
    return result;
}

DataTag EncodeShortStr(char c) {
    DataTag result = { (int)DataType::SmallString, 0, 0 };
    result.params |= c << 16;
    return result;
}

DataTag EncodeVisualMarker() {
    return DataTag{ 0xff, 0xffffff, 0xffffffff };
}

void DecodeShortStr(DataTag token, String* target) {
    if (target == NULL) return;
    for (int i = 0; i < 3; i++) {
        char c = (token.params >> (16 - (i * 8))) & 0xFF;
        if (c == 0) return;
        StringAppendChar(target, c);
    }
    for (int i = 0; i < 4; i++) {
        char c = (token.data >> (24 - (i * 8))) & 0xFF;
        if (c == 0) return;
        StringAppendChar(target, c);
    }
}

void DescribeTag(DataTag token, String* target, HashMap* symbols) {
    StringPtr *str = NULL;
    char c1, c2;
    uint8_t p3;

    switch ((DataType)(token.type)) {
    case DataType::Invalid:  StringAppend(target, "Invalid token"); return;
    case DataType::Not_a_Result:  StringAppend(target, "Non value (NAR)"); return;
    case DataType::Void:  StringAppend(target, "Non value (Void)"); return;
    case DataType::Unit:  StringAppend(target, "Non value (Unit)"); return;

    case DataType::Opcode:
        StringAppend(target, "Opcode ");
        DecodeLongOpcode(token, &c1, &c2, NULL, &p3);
        if (c1 == 'i') { // increment mode
            StringAppendFormat(target, "incr \x02 ", (signed char)c2);
        } else {
            StringAppendChar(target, c1);
            StringAppendChar(target, c2);
            if (p3 > 0) {
                StringAppendFormat(target, " +\x02 ", p3);
            }
        }
        if (symbols != NULL && MapGet_int_StringPtr(symbols, token.data, &str)) {
            StringAppendFormat(target, " '\x01' ", *str);
        }
        StringAppendFormat(target, "[\x03]", token.data);
        return;

    case DataType::EndOfProgram:
        StringAppend(target, "End Of Program");
        return;
    case DataType::EndOfSubProgram:
        StringAppend(target, "End Of Subprogram");
        return;
    case DataType::Flag:
        StringAppend(target, "Internal testing flag");
        return;

    case DataType::VariableRef:
        StringAppend(target, "VariableNameRef");
        if (symbols != NULL && MapGet_int_StringPtr(symbols, token.data, &str)) {
            StringAppendFormat(target, " '\x01' ", *str);
        }
        StringAppendFormat(target, "[\x03]", DecodeVariableRef(token));
        return;

    case DataType::Exception:
        StringAppendFormat(target, "Runtime Error at \x03 (\x02)", token.data, token.data);
        return;

    case DataType::DebugStringPtr:
        StringAppend(target, "Debug string [");
        StringAppendInt32Hex(target, token.data);
        StringAppendChar(target, ']');
        return;

    case DataType::Fraction:
        StringAppend(target, "Fractional number [");
        StringAppendDouble(target, token.data);
        StringAppendChar(target, ']');
        return;

    case DataType::Integer:
        StringAppend(target, "Integer number [");
        StringAppendInt32(target, token.data);
        StringAppendChar(target, ']');
        return;

    case DataType::HashtablePtr:
        StringAppend(target, "Hashtable ptr [");
        StringAppendInt32Hex(target, token.data);
        StringAppendChar(target, ']');
        return;

    case DataType::VectorPtr:
        StringAppend(target, "Vector ptr [");
        StringAppendInt32Hex(target, token.data);
        StringAppendChar(target, ']');
        return;

    case DataType::StaticStringPtr:
        StringAppend(target, "Static string ptr [");
        StringAppendInt32Hex(target, token.data);
        StringAppend(target, "] \"");

        StringAppendChar(target, '"');
        return;

    case DataType::StringPtr:
        StringAppend(target, "String ptr [");
        StringAppendInt32Hex(target, token.data);
        StringAppendChar(target, ']');
        return;

    case DataType::SmallString:
        StringAppend(target, "Small String [");
        DecodeShortStr(token, target);
        StringAppendChar(target, ']');
        return;

    //15
    default:
        StringAppend(target, "Unknown token type: ");
        StringAppendInt32(target, token.type);
        return;
    }
}
