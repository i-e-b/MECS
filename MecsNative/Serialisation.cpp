#include "Serialisation.h"
#include "TypeCoersion.h"
#include "HashMap.h"


#define byte uint8_t
RegisterVectorStatics(Vec)
RegisterVectorFor(byte, Vec)
RegisterVectorFor(DataTag, Vec)

RegisterHashMapStatics(Map);
RegisterHashMapFor(StringPtr, DataTag, HashMapStringKeyHash, HashMapStringKeyCompare, Map)

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

// Push the bytes of a string to a byte vector, without destroying the original string
void PushStringBytes(String* str, Vector* target) {
	auto bvec = VecClone(StringGetByteVector(str), VectorArena(target));
	uint8_t b;
	while (VecDequeue_byte(bvec, &b)) VecPush_byte(target, b);
	VecDeallocate(bvec);
}

bool RecursiveWrite(DataTag source, InterpreterState* state, Vector* target) {
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
        return true;
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
		PushStringBytes(str, target);
        return true;
    }

        // Containers (here begins the recursion)
    case DataType::HashtablePtr:
    {
        // write a header, then each element. Keys are always strings.
        auto original = (HashMap*)InterpreterDeref(state, source); // should always be <string => data tag>
        if (original == NULL) return false;

        VecPush_byte(target, (int)DataType::HashtablePtr);
        auto allKeys = HashMapAllEntries(original); // Vector<  HashMap_KVP<string => data tag>  >

        //[entry count: uint 32] n*{ [key length: uint 32][key characters:n] [value type:8][value data:n] }
        auto length = VecLength(allKeys);
        PushUInt32(length, target);

        HashMap_KVP entry;
        for (uint32_t i = 0; i < length; i++)
        {
            if (!VectorPop(allKeys, &entry)) return false; //internal failure
            String* str =  *(String **)(entry.Key);
            DataTag* valTag = (DataTag*)(entry.Value);

            if (str == NULL || valTag == NULL) return false; // invalid hash map

            // Write key (no type tag, it's always a string)
            PushUInt32(StringLength(str), target);
			PushStringBytes(str, target);

            // Write the value (recursive)
            auto ok = RecursiveWrite(*valTag, state, target);
            if (!ok) return false; // error somewhere deeper
        }

        VecDeallocate(allKeys);
        return true;
    }

    case DataType::VectorPtr:
    {
        // write a header, then each element is handled
        // [entry count: uint 32] n*{ [value type:8][value data:n] }
        VecPush_byte(target, (int)DataType::VectorPtr);
        auto original = (Vector*)InterpreterDeref(state, source);
        if (original == NULL) return false;

        auto length = VectorLength(original);
        PushUInt32(length, target);
        for (uint32_t i = 0; i < length; i++) {
            // each item, recurse
            auto vecEntryPtr = VecGet_DataTag(original, i);
            if (vecEntryPtr == NULL) return false; // broken vector entry
            auto ok = RecursiveWrite(*vecEntryPtr, state, target);
            if (!ok) return false; // error somewhere deeper
        }
        return true;
    }

        // empty types
    case DataType::Not_a_Result:
        // write a byte for the type, and nothing else.
        VecPush_byte(target, source.type);
        return true;
        break;

        // indirect types
    case DataType::VariableRef:
        // resolve to a real datatag, and use that. There is nothing written for the var-ref itself
        auto next = ScopeResolve(InterpreterScope(state), source.data);
        return RecursiveWrite(next, state, target);

    default: // nothing else is supported yet
        return false;
    }
    return false;
}

bool WriteStateless(DataTag source, Vector* target) {
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
        return true;
    }

        // String types
    case DataType::DebugStringPtr:
    case DataType::StaticStringPtr:
    case DataType::StringPtr:
        return false; // long strings are not supported in stateless mode

        // Containers (here begins the recursion)
    case DataType::HashtablePtr:
    case DataType::VectorPtr:
        return false; // containers are not supported in stateless mode

        // empty types
    case DataType::Not_a_Result:
        // write a byte for the type, and nothing else.
        VecPush_byte(target, source.type);
        return true;
        break;

    default: // nothing else is supported in stateless mode
        return false;
    }
    return false;
}

bool RecursiveRead(DataTag* dest, Arena* memory, Vector* source) {

    // This needs to take the raw bytes and put everything back together
    // Strings will be rolled out to their full containers (including static strings -- we don't assume code will be same)
    // Hash and Vector types will get built out.

    // read the type header:
    uint8_t type = 0;
    uint8_t b=0;
    auto ok = VecDequeue_byte(source, &type);
    if (!ok) return false;

    switch ((DataType)type) {
    case DataType::Not_a_Result:
    {
        dest->type = (int)DataType::Not_a_Result;
        dest->params = 0;
        dest->data = 0;
        return true;
    }

        // Simple small types (don't need to write to arena)
    case DataType::Integer:
    case DataType::Fraction:
    case DataType::SmallString:
    {
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
    }

    case DataType::StringPtr:
    {
        // Read string into arena, get offset, encode to output.
        uint32_t len;
        if (!DequeueUInt32(&len, source)) return false; // vector too short
        auto str = StringEmptyInArena(memory);
        auto offset = ArenaPtrToOffset(memory, str);
        if (offset < 1) return false; // out of memory

        // set outgoing tag first (helps tracing in case of an error)
        auto result = EncodePointer(offset, DataType::StringPtr);
        dest->type = result.type;
        dest->params = result.params;
        dest->data = result.data;

        // fill the string
        for (uint32_t i = 0; i < len; i++) {
            if (!VecDequeue_byte(source, &b)) return false; // vector too short
            StringAppendChar(str, b);
        }

        return true;
    }

    case DataType::VectorPtr:
    {
        // [entry count: uint 32] n*{ [value type:8][value data:n] }
        uint32_t length = 0;
        auto ok = DequeueUInt32(&length, source);
        if (!ok) return false;

        // create new vector in target memory
        auto container = VecAllocateArena_DataTag(memory);
        auto offset = ArenaPtrToOffset(memory, container);
        
        // bind output
        auto result = EncodePointer(offset, DataType::VectorPtr);
        dest->type = result.type;
        dest->params = result.params;
        dest->data = result.data;

        // Fill the vector
        for (uint32_t i = 0; i < length; i++) {
            DataTag element = {};
            ok = RecursiveRead(&element, memory, source);
            if (!ok) return false;
            VecPush_DataTag(container, element);
        }
        return true;
    }

    case DataType::HashtablePtr:
    {
        //[entry count: uint 32] n*{ [key length: uint 32][key characters:n] [value type:8][value data:n] }
        uint32_t length = 0, keyLen = 0;
        auto ok = DequeueUInt32(&length, source);
        if (!ok) return false;

        // create new map in target memory
        auto container = MapAllocateArena_StringPtr_DataTag(length, memory);
        auto offset = ArenaPtrToOffset(memory, container);

        // read entries
        uint32_t i,j;
        String* keyStr = StringEmptyInArena(memory);
        for (i = 0; i < length; i++) {
            // read key
            ok = DequeueUInt32(&keyLen, source);
            if (!ok) return false; // invalid serialised form
            if (keyLen < 1) return false; // invalid key
            StringClear(keyStr);
            for (j = 0; j < keyLen; j++) {
                if (!VecDequeue_byte(source, &b)) return false; // vector too short
                StringAppendChar(keyStr, b);
            }

            // read value
            DataTag data = {};
            ok = RecursiveRead(&data, memory, source);
            if (!ok) return false;
            
            // write into map
            ok = MapPut_StringPtr_DataTag(container, StringClone(keyStr), data, false);
            if (!ok) return false; // internal error / out of memory
        }
        StringDeallocate(keyStr);
        
        // bind output
        auto result = EncodePointer(offset, DataType::HashtablePtr);
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


bool FreezeToVector(HashMapPtr source, VectorPtr target) {
    if (target == NULL) return false;
    if (source == NULL) return false;
    
	if (VectorElementSize(target) != 1) return false;
	VectorClear(target);

    // Write the hashtable header, then read the keys
	VecPush_byte(target, (int)DataType::HashtablePtr);
	auto allKeys = HashMapAllEntries(source); // Vector<  HashMap_KVP<string => data tag>  >

	//[entry count: uint 32] n*{ [key length: uint 32][key characters:n] [value type:8][value data:n] }
	auto length = VecLength(allKeys);
	PushUInt32(length, target);

	HashMap_KVP entry;
	for (uint32_t i = 0; i < length; i++)
	{
		if (!VectorPop(allKeys, &entry)) return false; //internal failure
		String* str = *(String**)(entry.Key);
		DataTag* valTag = (DataTag*)(entry.Value);

		if (str == NULL || valTag == NULL) return false; // invalid hash map

		// Write key (no type tag, it's always a string)
		PushUInt32(StringLength(str), target);
		PushStringBytes(str, target);

		// Write the value (NON-recursive)
        auto ok = WriteStateless(*valTag, target);
		if (!ok) return false; // error somewhere deeper
	}

    return true;
}

// Expand a byte vector that has been filled by `FreezeToVector` into an arena
// The data tag that is the root of the resulting structure is passed through `dest`
bool DefrostFromVector(DataTag* dest, Arena* memory, Vector* source) {
    if (source == NULL) return false;
    if (memory == NULL) return false;
    if (dest == NULL) return false;

    return RecursiveRead(dest, memory, source);
}

