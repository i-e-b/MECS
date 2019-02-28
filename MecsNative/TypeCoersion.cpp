#include "TypeCoersion.h"

#include "Scope.h"

/*
Convert from RuntimeMemoryModel.cs
*/


String* DereferenceString(InterpreterState* is, uint32_t position) {
    auto original = (String*)InterpreterDeref(is, position);
    return StringProxy(original);
}

// Make or return reference to a string version of the given token. `debugSymbols` is Map<crushname->string>
String* Stringify(InterpreterState* is, DataTag token, DataType type, HashMap* debugSymbols);

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
float CastDouble(InterpreterState* is, DataTag encoded);

// Cast a value to int. If not applicable, returns zero
int CastInt(InterpreterState* is, DataTag  encoded) {
    int result;
    auto type = encoded.type;
    switch (type) {
    case (int)DataType::VariableRef:
    {
        // Follow scope
        auto next = ScopeResolve(InterpreterScope(is), DecodeVariableRef(encoded));//Variables.Resolve(NanTags.DecodeVariableRef(encoded));
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
        auto ptr = DecodePointer(encoded);
        auto temp = DereferenceString(is, ptr);
        DecodeShortStr(encoded, temp);
        bool ok = StringTryParse_int32(temp, &result);
        StringDeallocate(temp);

        if (!ok) return 0;
        return result;
    }
    case (int)DataType::Integer: return (int)encoded.data;

    default:
        return 0;
    }
}


// Get a resonable string representation from a value.
// This should include stringifying non-string types (numbers, structures etc)
String* CastString(InterpreterState* is, DataTag encoded) {
    // IMPORTANT: this should NEVER send back an original string -- it will get deallocated!
    auto type = encoded.type;
    switch (type) {
    case (int)DataType::Invalid: return StringNew("<invalid value>");
    case (int)DataType::Integer: return StringFromInt32(encoded.data); //encoded.ToString(CultureInfo.InvariantCulture);
    case (int)DataType::Fraction: return StringNew("TODO");
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
        return DereferenceString(is, DecodePointer(encoded));

    case (int)DataType::HashtablePtr:
    case (int)DataType::VectorPtr:
        return StringNew("<complex type>"); // TODO: add stringification later

    default:
        return StringNew("<value out of range>");
    }
}

// Store a new string at the end of memory, and return a string pointer token for it
DataTag StoreStringAndGetReference(InterpreterState* is, String* str);