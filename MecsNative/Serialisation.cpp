#include "Serialisation.h"
#include "TypeCoersion.h"

#define byte uint8_t
RegisterVectorStatics(Vec)
RegisterVectorFor(byte, Vec)


/*

The format:

[type:8]
    if unit/nar/void:  [empty:0]
    if simple:         [data: 56]
    if string:         [entry count: uint 32] [characters: n]
    if map:            [entry count: uint 32] n*{ [key length: uint 32][key characters:n] [value type:8][value data:n] }
    if vector:         [entry count: uint 32] n*{ [value type:8][value data:n] }

    Values in the containers (map and vector) can be any of the above

*/

inline void PushUInt32(uint32_t value, Vector* target) {
    for (int i = 3; i >= 0; i--) {
        VecPush_byte(target, (value >> (i * 8)) & 0xff);
    }
}

inline bool DequeueUInt32(uint32_t* value, Vector* target) {
    //if (value == NULL) return false;
    uint32_t tmp = 0;
    uint8_t b;
    for (int i = 3; i >= 0; i--) {
        if (!VecDequeue_byte(target, &b)) return false;
        tmp |= ((uint32_t)b) << (i * 8);
    }
    *value = tmp;
    return true;
}

bool RecursiveWrite(DataTag source, InterpreterState* state, Vector* target) {
    // this switch should get pulled out to a recursion friendly internal function
    switch ((DataType)source.type) {
        // Simple small types
    case DataType::Integer:
    case DataType::Fraction:
    case DataType::SmallString:
    {
        // write the tag into the vector
        VecPush_byte(target, source.type);
        for (int i = 2; i >= 0; i--) {
            VecPush_byte(target, (source.params >> (i*8)) & 0xff);
        }
        PushUInt32(source.data, target);
        break;
    }

        // String types
    case DataType::DebugStringPtr:
    case DataType::StaticStringPtr: // this one needs us to read from program memory. Arena won't be enough.
    case DataType::StringPtr:
    {
        // get the string bytes, and write into vector as length+bytes
        VecPush_byte(target, (int)DataType::StringPtr); // always a general string pointer once it's serialised
        auto str = CastString(state, source);
        PushUInt32(StringLength(str), target);
        auto bvec = StringGetByteVector(str);
        uint8_t b;
        while (VecDequeue_byte(bvec, &b)) VecPush_byte(target, b);
        break;
    }

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
        return true;

    case DataType::StringPtr:
    {
        // Read string into arena, get offset, encode to output.
        uint32_t len;
        if (!DequeueUInt32(&len, source)) return false; // vector too short
        auto str = StringEmptyInArena(memory);
        auto offset = ArenaPtrToOffset(memory, str);
        for (uint32_t i = 0; i < len; i++) {
            if (!VecDequeue_byte(source, &b)) return false; // vector too short
            StringAppendChar(str, b);
        }
        if (offset < 1) return false; // out of memory

        auto result = EncodePointer(offset, DataType::StringPtr);
        dest->type = result.type;
        dest->params = result.params;
        dest->data = result.data;

        return true;
    }

        
    default: // nothing else is supported yet
        return false;
    }

    return false;
}