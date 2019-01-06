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
// Only pointers to data are maintained by the hash-map. You must alloc and free as required.
// This is best for larger structures and string keys
// TODO: make another version of this that works with directly stored values ("Dictionary"?)
typedef struct HashMap {
    Vector buckets; // this is a Vector<HashMap_Entry>

    unsigned int count;
    unsigned int countMod;
    unsigned int countUsed;
    unsigned int growAt;
    unsigned int shrinkAt;

    bool IsValid; // if false, the hash map has failed

    // Should return true IFF the two key objects are equal
    bool(*KeyComparer)(void* key_A, void* key_B);

    // Should return a unsigned 32bit hash value for the given key
    unsigned int(*GetHash)(void* key);

} HashMap;

// Create a new hash map with an initial size
HashMap HashMapAllocate(unsigned int size, bool(*keyComparerFunc)(void* key_A, void* key_B), unsigned int(*getHashFunc)(void* key));
// Deallocate internal storage of the hash-map. Does not deallocate the keys or values
void HashMapDeallocate(HashMap *h);
// Returns true if value found. If so, it's pointer is copied to `*outValue`
bool HashMapGet(HashMap *h, void* key, void** outValue);
// Add a key/value pair to the map. If `canReplace` is true, conflicts replace existing data. if false, existing data survives
bool HashMapPut(HashMap *h, void* key, void* value, bool canReplace);
// List all keys in the hash map. The vector must be deallocated by the caller.
Vector HashMapAllEntries(HashMap *h); // returns a Vector<HashMap_KVP>
// Returns true if hashmap has a value stored to the given key
bool HashMapContainsKey(HashMap *h, void* key);
// Remove the entry for the given key, if it exists
bool HashMapRemove(HashMap *h, void* key);
// Remove all entries from the hash-map, but leave the hash-map allocated and valid
void HashMapClear(HashMap *h);
// Return count of entries stored in the hash-map
unsigned int HashMapCount(HashMap *h);
#endif