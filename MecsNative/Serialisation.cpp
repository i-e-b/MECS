#include "Serialisation.h"

#define byte uint8_t
RegisterVectorStatics(Vec)
RegisterVectorFor(byte, Vec)


/*

The format:

[type:8]
    if unit/nar/void:  [empty:0]
    if simple:         [data: 56]
    if string:         [entry count: 32] [data: n]
    if map:            [entry count: 32] n*{ [key length:32][key data:n] [value type:8][value data:n] }
    if vector:         [entry count: 32] n*{ [value type:8][value data:n] }

    Values in the containers (map and vector) can be any of the above

*/


bool RecursiveWrite(DataTag source, InterpreterState* state, Vector* target) {
    // this switch should get pulled out to a recursion friendly internal function
    switch ((DataType)source.type) {
        // Simple small types
    case DataType::Integer:
    case DataType::Fraction:
    case DataType::SmallString:
        // write the tag into the vector
        VecPush_byte(target, source.type);
        for (int i = 2; i >= 0; i--) {
            VecPush_byte(target, (source.params >> (i*8)) & 0xff);
        }
        for (int i = 3; i >= 0; i--) {
            VecPush_byte(target, (source.data >> (i*8)) & 0xff);
        }
        break;

        // String types
    case DataType::DebugStringPtr:
    case DataType::StaticStringPtr: // this one needs us to read from program memory. Arena won't be enough.
    case DataType::StringPtr:
        // get the string bytes, and write into vector as length+bytes
        break;

        // Containers (here begins the recursion)
    case DataType::HashtablePtr:
        // write a header, then each element. Keys are always strings.
        break;
    case DataType::VectorPtr:
        // write a header, then each element is handled
        break;

        // empty types
    case DataType::Not_a_Result:
        // write a byte for the type, and nothing else.
        VecPush_byte(target, source.type);
        break;

        // indirect types
    case DataType::VariableRef:
        // resolve to a real datatag, and use that. There is nothing written for the var-ref itself
        break;

    default: // nothing else is supported yet
        return false;
    }

    return true;
}

// Write a data tag `source` in serialised form to the given BYTE vector `target`
// The given interpreter state will be used to resolve contents of containers.
// Any existing data in the target will be deleted
bool FreezeToVector(DataTag source, InterpreterState* state, Vector* target) {
    if (target == NULL) return false;
    if (state == NULL) return false;

    if (VectorElementSize(target) != 1) return false;
    VectorClear(target);

    return RecursiveWrite(source, state, target);
}

// Expand a byte vector that has been filled by `FreezeToVector` into an arena
// The data tag that is the root of the resulting structure is passed through `dest`
bool DefrostFromVector(DataTag* dest, Arena* memory, Vector* source) {
    if (source == NULL) return false;
    if (memory == NULL) return false;
    if (dest == NULL) return false;

    // This needs to take the raw bytes and put everything back together
    // Strings will be rolled out to their full containers (including static strings -- we don't assume code will be same)
    // Hash and Vector types will get built out.

    // initially, just dealing with things as they are tested:
    uint8_t type = 0;
    uint8_t b=0;
    auto ok = VecDequeue_byte(source, &type);
    switch ((DataType)type) {
        // Simple small types (don't need to write to arena)
    case DataType::Integer:
    case DataType::Fraction:
    case DataType::SmallString:
        // Unpack from serial form

        dest->type = type;
        for (int i = 2; i >= 0; i--) {
            if (!VecDequeue_byte(source, &b)) return false; // vector too short
            dest->params |= ((uint32_t)b) << (i*8);
        }
        for (int i = 3; i >= 0; i--) {
            if (!VecDequeue_byte(source, &b)) return false; // vector too short
            dest->data |= ((uint32_t)b) << (i*8);
        }
        break;
    }

    return false;
}