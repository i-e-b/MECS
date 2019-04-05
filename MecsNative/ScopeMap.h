#pragma once

#ifndef scopemap_h
#define scopemap_h

#include "TagData.h"

/*
    This is a specialisation of the 'HashMap' code
    purely for the 'Scope' container. This only allows 32-bit int keys
    and 64-bit int values (as DataTag).

    The functions are basically the same as hash-map, but only what's required for scope
*/

typedef struct ScopeMap_KVP {
    uint32_t* Key;
    DataTag* Value;
} ScopeMap_KVP;

typedef struct ScopeMap ScopeMap;

// Create a new scope map
// This has a default fixed size
ScopeMap* ScopeMapAllocate();
// Deallocate internal storage of the scope-map.
void ScopeMapDeallocate(ScopeMap *h);

// Returns true if value found. If so, it's pointer is copied to `*outValue`
bool ScopeMapGet(ScopeMap *h, uint32_t key, DataTag** outValue);
// Add a key/value pair to the map. If there is an existing value, it will be replaced
bool ScopeMapPut(ScopeMap *h, uint32_t key, DataTag value);

// Remove the entry for the given key, if it exists
bool ScopeMapRemove(ScopeMap *h, uint32_t key);


// List all keys in the hash map. The vector must be deallocated by the caller.
Vector *ScopeMapAllEntries(ScopeMap *h); // returns a Vector<ScopeMap_KVP>

#endif