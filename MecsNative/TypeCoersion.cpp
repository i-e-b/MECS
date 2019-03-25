#include "TypeCoersion.h"

#include "Scope.h"
#include "Fix16.h"


RegisterVectorFor(DataTag, Vector)


String* DereferenceString(InterpreterState* is, DataTag stringTag) {
    if (stringTag.type == (int)DataType::StringPtr) {
        // A runtime string. We just need the pointer
        auto original = (String*)InterpreterDeref(is, stringTag);
        return StringProxy(original);
    } else {
        // a static string. We need to read out of RO data into memory.

        // a static string is [NanTag(UInt32): byte length] [string bytes, padded to 8 byte chunks]
        auto position = DecodePointer(stringTag);
        //  1) read the byte length

        auto header = GetOpcodeAtIndex(is, position);
        if (header.type != (int)DataType::Integer) return NULL; //throw new Exception("String header was not a UInt32");
        auto length = DecodeInt32(header);

        return ReadStaticString(is, position + 1, length);
    }
}

// Interpret or cast value as a boolean
bool CastBoolean(InterpreterState* is, DataTag encoded) {
    auto type = encoded.type;

    switch (type) {
    case (int)DataType::Fraction:
        return false; //TODO... Math.Abs(encoded) > double.Epsilon;

    case (int)DataType::Integer:
        return encoded.data != 0;

    case (int)DataType::SmallString:
    case (int)DataType::StringPtr:
    case (int)DataType::StaticStringPtr:
    {
        auto str = CastString(is, encoded);
        bool res = StringTruthyness(str);
        StringDeallocate(str);
        return res;
    }

    case (int)DataType::VariableRef:
    {   // Follow scope
        auto next = ScopeResolve(InterpreterScope(is), encoded.data);
        return CastBoolean(is, next);
    }

    case (int)DataType::VectorIndex:
    {
        // dereference the vector index, and call CastInt again:
        auto idx = encoded.params; // grab the index
        encoded.type = (int)DataType::VectorPtr; // make this into a reference to the actual vector
        auto src = (Vector*)InterpreterDeref(is, encoded); // get the vector
        if (src == NULL) return false;

        auto tag = VectorGet_DataTag(src, idx);
        if (tag == NULL) return false;
        return CastBoolean(is, *tag);
    }

    // All the things that can't be meaningfully cast are 'false'
    default: return false;
    }
}

// null, empty, "false" or "0" are false. All other strings are true.
bool StringTruthyness(String* strVal) {
    if (strVal == NULL) return false;
    if (StringLength(strVal) < 1) return false;
    if (StringAreEqual(strVal, "false")) return false; // TODO: case insensitivity
    if (StringAreEqual(strVal, "0")) return false;
    return true;
}

// Interpret, or cast value as double
float CastDouble(InterpreterState* is, DataTag encoded) {
    auto type = encoded.type;
    float result;
    switch (type) {
    case (int)DataType::Integer:
        return encoded.data;


    case (int)DataType::VariableRef:
    {
        // Follow scope
        auto next = ScopeResolve(InterpreterScope(is), DecodeVariableRef(encoded));
        return CastDouble(is, next);
    }
    case (int)DataType::SmallString:
    {
        String* temp = StringEmpty();
        DecodeShortStr(encoded, temp);
        fix16_t dest; // TODO: move over to float
        bool ok = StringTryParse_f16(temp, &dest);
        StringDeallocate(temp);
        return (ok) ? (fix16_to_float(dest)) : 0;
    }
    case (int)DataType::StaticStringPtr:
    case (int)DataType::StringPtr:
    {
        auto ptr = DecodePointer(encoded);
        auto temp = DereferenceString(is, encoded);
        fix16_t dest; // TODO: move over to float
        bool ok = StringTryParse_f16(temp, &dest);
        StringDeallocate(temp);

        return (ok) ? (fix16_to_float(dest)) : 0;
    }

    case (int)DataType::VectorIndex:
    {
        // dereference the vector index, and call CastInt again:
        auto idx = encoded.params; // grab the index
        encoded.type = (int)DataType::VectorPtr; // make this into a reference to the actual vector
        auto src = (Vector*)InterpreterDeref(is, encoded); // get the vector
        if (src == NULL) return 0;

        auto tag = VectorGet_DataTag(src, idx);
        if (tag == NULL) return 0;
        return CastDouble(is, *tag);
    }

    // All the things that can't be meaningfully cast
    default: return 0.0;
    }
}

// Cast a value to int. If not applicable, returns zero
int CastInt(InterpreterState* is, DataTag  encoded) {
    int result;
    auto type = encoded.type;
    switch (type) {
    case (int)DataType::VariableRef:
    {
        // Follow scope
        auto next = ScopeResolve(InterpreterScope(is), DecodeVariableRef(encoded));
        return CastInt(is, next);
    }
    case (int)DataType::SmallString:
    {
        int32_t result;
        String* temp = StringEmpty();
        DecodeShortStr(encoded, temp);
        bool ok = StringTryParse_int32(temp, &result);
        StringDeallocate(temp);

        if (!ok) return 0;
        return result;
    }
    case (int)DataType::StaticStringPtr:
    case (int)DataType::StringPtr:
    {
        auto temp = DereferenceString(is, encoded);
        bool ok = StringTryParse_int32(temp, &result);
        StringDeallocate(temp);

        if (!ok) return 0;
        return result;
    }
    case (int)DataType::Integer: return (int)encoded.data;

    case (int)DataType::VectorIndex:
    {
        // dereference the vector index, and call CastInt again:
        auto idx = encoded.params; // grab the index
        encoded.type = (int)DataType::VectorPtr; // make this into a reference to the actual vector
        auto src = (Vector*)InterpreterDeref(is, encoded); // get the vector
        if (src == NULL) return 0;

        auto tag = VectorGet_DataTag(src, idx);
        if (tag == NULL) return 0;
        return CastInt(is, *tag);
    }
    default:
        return 0;
    }
}

String* StringifyVector(InterpreterState* is, DataTag encoded) {
    auto original = (Vector*)InterpreterDeref(is, encoded);

    if (original == NULL) return StringNew("<null>");

    auto output = StringNew('[');
    auto length = VectorLength(original);
    for (int i = 0; i < length; i++) {
        auto str = CastString(is, *VectorGet_DataTag(original, i));
        StringAppend(output, str);
        StringDeallocate(str);

        if (i < length - 1) {
            StringAppend(output, ", ");
        }
    }

    StringAppendChar(output, ']');
    return output;
}


// Get a resonable string representation from a value.
// This should include stringifying non-string types (numbers, structures etc)
String* CastString(InterpreterState* is, DataTag encoded) {
    // IMPORTANT: this should NEVER send back an original string -- it will get deallocated!
    auto type = encoded.type;
    switch (type) {
    case (int)DataType::Invalid: return StringEmpty();
    case (int)DataType::Integer: return StringFromInt32(encoded.data); //encoded.ToString(CultureInfo.InvariantCulture);
    case (int)DataType::Fraction: return StringNew("<fractional number>");
    case (int)DataType::Opcode: return StringNew("<Op Code>");

    case (int)DataType::Not_a_Result: return StringEmpty();
    case (int)DataType::Unit: return StringEmpty();
    case (int)DataType::Void: return StringEmpty();

    case (int)DataType::VariableRef:
    {   // Follow scope
        auto next = ScopeResolve(InterpreterScope(is), encoded.data); //Variables.Resolve(NanTags.DecodeVariableRef(encoded));
        return CastString(is, next);
    }

    case (int)DataType::SmallString:
    {
        auto str = StringEmpty();
        DecodeShortStr(encoded, str);
        return str;
    }

    case (int)DataType::StaticStringPtr:
    case (int)DataType::StringPtr:
        return DereferenceString(is, encoded);

    case (int)DataType::VectorPtr:
        return StringifyVector(is, encoded);

    case (int)DataType::VectorIndex:
    {
        // dereference the vector index, and call CastInt again:
        auto idx = encoded.params; // grab the index
        encoded.type = (int)DataType::VectorPtr; // make this into a reference to the actual vector
        auto src = (Vector*)InterpreterDeref(is, encoded); // get the vector
        if (src == NULL) return false;

        auto tag = VectorGet_DataTag(src, idx);
        if (tag == NULL) return false;
        return CastString(is, *tag);
    }

    case (int)DataType::HashtablePtr:
        return StringNew("<complex type>"); // TODO: add stringification later

    default:
        return StringNew("<value out of range>");
    }
}

