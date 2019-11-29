#pragma once

#ifndef hashmap_h
#define hashmap_h
#include "Vector.h"

// key-value-pair data structure, for reading back value sets
typedef struct HashMap_KVP {
    void* Key;
    void* Value;
} HashMap_KVP;

// A generalised hash-map using the robin-hood strategy and our own Vector class
// Users must supply their own hashing and equality function pointers
typedef struct HashMap HashMap;
typedef HashMap* HashMapPtr;

// Create a new hash map with an initial size
HashMap* HashMapAllocate(unsigned int size, int keyByteSize, int valueByteSize, bool(*keyComparerFunc)(void* key_A, void* key_B), unsigned int(*getHashFunc)(void* key));
// Create a new hash map with an initial size, pinned to a specific arena
HashMap* HashMapAllocateArena(Arena* a, unsigned int size, int keyByteSize, int valueByteSize, bool(*keyComparerFunc)(void* key_A, void* key_B), unsigned int(*getHashFunc)(void* key));

// Deallocate internal storage of the hash-map. Does not deallocate the keys or values
void HashMapDeallocate(HashMap *h);

// Do some basic sanity checks on the hash map
bool HashMapIsValid(HashMap *h);

// Returns true if value found. If so, it's pointer is copied to `*outValue`. If outValue is null, no value is copied.
bool HashMapGet(HashMap *h, void* key, void** outValue);
// Add a key/value pair to the map. If `canReplace` is true, conflicts replace existing data. if false, existing data survives
bool HashMapPut(HashMap *h, void* key, void* value, bool canReplace);
// List all keys in the hash map. The vector must be deallocated by the caller.
Vector *HashMapAllEntries(HashMap *h); // returns a Vector<HashMap_KVP>
// Remove the entry for the given key, if it exists
bool HashMapRemove(HashMap *h, void* key);
// Remove all entries from the hash-map, but leave the hash-map allocated and valid
void HashMapClear(HashMap *h);
// Return count of entries stored in the hash-map
unsigned int HashMapCount(HashMap *h);

// Resize the hash map and its internal buffers to suit the currently held data
// Note: The hash map doesn't clean up after key removal/replacement until it is cleared or resized
// If you are doing lots or remove and replace, call this occasionally to prevent memory growth
void HashMapPurge(HashMap *h);


// Some common compare and hash functions.
bool         HashMapStringKeyCompare(void* key_A, void* key_B);
unsigned int HashMapStringKeyHash(void* key);
bool         HashMapIntKeyCompare(void* key_A, void* key_B);
unsigned int HashMapIntKeyHash(void* key);

// Macros to create type-specific versions of the methods above.
// If you want to use the typed versions, make sure you call `RegisterHashMapFor(typeName, namespace,...)` for EACH type

// These are invariant on type, but can be namespaced
#define RegisterHashMapStatics(nameSpace) \
    inline void nameSpace##Deallocate(HashMap *h){ HashMapDeallocate(h); }\
    inline Vector* nameSpace##AllEntries(HashMap *h)/*<! returns a Vector<HashMap_KVP> */{ return HashMapAllEntries(h); }\
    inline void nameSpace##Clear(HashMap *h){ HashMapClear(h); }\
    inline unsigned int nameSpace##Count(HashMap *h){ return HashMapCount(h); }\
    inline bool nameSpace##IsValid(HashMap *h){return HashMapIsValid(h);}\


// These must be registered for each distinct pair, as they are type variant
#define RegisterHashMapFor(keyType, valueType, hashFuncPtr, compareFuncPtr, nameSpace) \
    inline HashMap* nameSpace##Allocate_##keyType##_##valueType(unsigned int size){ return HashMapAllocate(size, sizeof(keyType), sizeof(valueType), compareFuncPtr, hashFuncPtr); } \
    inline HashMap* nameSpace##AllocateArena_##keyType##_##valueType(unsigned int size, Arena* a){ return HashMapAllocateArena(a, size, sizeof(keyType), sizeof(valueType), compareFuncPtr, hashFuncPtr); } \
    inline bool nameSpace##Get##_##keyType##_##valueType(HashMap *h, keyType key, valueType** outValue){return HashMapGet(h, &key, (void**)(outValue));}\
    inline bool nameSpace##Put##_##keyType##_##valueType(HashMap *h, keyType key, valueType value, bool replace){return HashMapPut(h, &key, &value, replace); }\
    inline bool nameSpace##Remove##_##keyType##_##valueType(HashMap *h, keyType key){ return HashMapRemove(h, &key); }\
    typedef struct nameSpace##_KVP_##keyType##_##valueType { keyType* Key; valueType* Value; } nameSpace##_KVP_##keyType##_##valueType ; \


#endif