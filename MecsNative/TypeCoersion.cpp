#include "TypeCoersion.h"

#include "Scope.h"

/*
Convert from RuntimeMemoryModel.cs
*/


// Make or return reference to a string version of the given token. `debugSymbols` is Map<crushname->string>
String* Stringify(InterpreterState* is, DataTag token, DataType type, HashMap* debugSymbols);
// Interpret or cast value as a boolean
bool CastBoolean(InterpreterState* is, DataTag encoded);
// null, empty, "false" or "0" are false. All other strings are true.
bool StringTruthyness(InterpreterState* is, String* strVal);
// Interpret, or cast value as double
float CastDouble(InterpreterState* is, DataTag encoded);
// Cast a value to int. If not applicable, returns zero
int CastInt(InterpreterState* is, DataTag  encoded);

String* DereferenceString(InterpreterState* is, uint32_t position) {
    auto original = (String*)InterpreterDeref(is, position);
    return StringProxy(original);
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