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

// return a tag that is never valid (denotes an error)
DataTag InvalidTag() {
    return DataTag{ (int)DataType::Invalid, 0, 0 };
}
// returns false if the tag is of `Invalid` type, true otherwise
bool IsTagValid(DataTag t) {
    return t.type != (int)DataType::Invalid;
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
        ((uint32_t)codeClass << 8) | (codeAction),
        ((uint32_t)p1 << 16) | ((uint32_t)p2)
    };
}
// Encode an op-code with 1x32 bit param
DataTag EncodeLongOpcode(char codeClass, char codeAction, uint32_t p1) {
    return DataTag{
        (int)DataType::Opcode,
        ((uint32_t)codeClass << 8) | (codeAction),
        p1
    };
}
// Decode an op-code tag that uses up to 2x16 bit params
void DecodeOpcode(DataTag encoded, char* codeClass, char* codeAction, uint16_t* p1, uint16_t* p2) {
    if (codeClass != NULL) {
        *codeClass = encoded.params >> 8;
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
}

// Decode an op-code that uses up to 1x32 bit param
void DecodeLongOpcode(DataTag encoded, char* codeClass, char* codeAction, uint32_t* p1) {
    if (codeClass != NULL) {
        *codeClass = encoded.params >> 8;
    }
    if (codeAction != NULL) {
        *codeAction = encoded.params & 0xFF;
    }
    if (p1 != NULL) {
        *p1 = encoded.data;
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

DataTag EncodePointer(uint32_t ptrTarget, DataType type) {
    DataTag t;
    t.type = (char)type;
    t.params = 0;
    t.data = ptrTarget;
    return t;
}

DataTag EncodeInt32(int32_t original) {
    return DataTag{ (int)DataType::Integer, 0, (uint32_t)original };
}
int32_t DecodeInt32(DataTag encoded) {
    return encoded.data;
}

DataTag EncodeBool(bool b) {
    return DataTag{ (int)DataType::Integer, 0, (b) ? 0xffffffffu : 0u };
}
bool DecodeBool(DataTag encoded) {
    return encoded.data == 0;
}

DataTag EncodeShortStr(String* str) {
    DataTag result = { (int)DataType::SmallString, 0, 0 };

    for (int i = 0; i < 3; i++) {
        result.params |= StringDequeue(str) << (16 - (i*8));
    }
    for (int i = 0; i < 4; i++) {
        result.data |= StringDequeue(str) << (24 - (i * 8));
    }
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

    switch ((DataType)(token.type)) {
    case DataType::Invalid:  StringAppend(target, "Invalid token"); return;
    case DataType::Not_a_Result:  StringAppend(target, "Non value (NAR)"); return;
    case DataType::Void:  StringAppend(target, "Non value (Void)"); return;
    case DataType::Unit:  StringAppend(target, "Non value (Unit)"); return;

    case DataType::Opcode:
        StringAppend(target, "Opcode ");
        DecodeLongOpcode(token, &c1, &c2, NULL);
        StringAppendChar(target, c1);
        StringAppendChar(target, c2);
        if (symbols != NULL && MapGet_int_StringPtr(symbols, token.data, &str)) {
            StringAppendFormat(target, " '\x01' ", *str);
        }
        StringAppendFormat(target, "[\x03]", token.data);
        return;

    case DataType::VariableRef:
        StringAppend(target, "VariableNameRef");
        if (symbols != NULL && MapGet_int_StringPtr(symbols, token.data, &str)) {
            StringAppendFormat(target, " '\x01' ", *str);
        }
        StringAppendFormat(target, "[\x03]", DecodeVariableRef(token));
        return;

    case DataType::Exception:
        StringAppend(target, "Runtime Error at ");
        StringAppendInt32Hex(target, token.data);
        return;

    case DataType::DebugStringPtr:
        StringAppend(target, "Debug string [");
        StringAppendInt32Hex(target, token.data);
        StringAppendChar(target, ']');
        return;

    case DataType::Fraction:
        StringAppend(target, "Fractional number [");
        StringAppendF16(target, token.data);
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
        StringAppendChar(target, ']');
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
