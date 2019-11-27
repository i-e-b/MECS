#include "Scope.h"

typedef HashMap* MapPtr;
typedef uint32_t Name;

RegisterVectorStatics(Vec)
RegisterVectorFor(MapPtr, Vec)
RegisterVectorFor(DataTag, Vec)
RegisterVectorFor(ScopeReference, Vec)
RegisterVectorFor(HashMap_KVP, Vec)

RegisterHashMapStatics(Map)
RegisterHashMapFor(Name, DataTag, HashMapIntKeyHash, HashMapIntKeyCompare, Map)

// Note -- this dynamic scope resolution leads to a lot of inefficiency
// in the interpreter -- especially in nested loops that access variables in outer scopes.
// a compile-time resolution would fix this entirely, but would change some of the call / eval logic.

typedef struct Scope {
    // Vector of (Hashmap of Name->DataTag)
    Vector* _scopes; // this is currently critical in tight loops. Maybe de-vector it?

    // Arena for new allocations
    Arena* _memory;
} Scope;


Scope* ScopeAllocate(Arena* arena) {
    auto result = (Scope*)ArenaAllocateAndClear(arena, sizeof(Scope));
    if (result == NULL) return NULL;

    auto firstMap = (MapPtr)MapAllocateArena_Name_DataTag(64, arena);
    if (firstMap == NULL) { ArenaDereference(arena, result); return NULL; }

    result->_scopes = VecAllocateArena_MapPtr(arena);

    if (result->_scopes == NULL) {
        ScopeDeallocate(result);
        return NULL;
    }

    result->_memory = arena;
    VecPush_MapPtr(result->_scopes, firstMap);
    return result;
}

void ScopeDeallocate(Scope* s) {
    if (s == NULL) return;

    if (s->_scopes != NULL) {
        MapPtr map;
        while (VecPop_MapPtr(s->_scopes, &map)) {
            MapDeallocate(map);
        }
        VecDeallocate(s->_scopes);
        s->_scopes = NULL;
    }
    auto mem = s->_memory;
    ArenaDereference(mem, s);
}

void ScopePush(Scope* s, Vector* parameters) {
    if (s == NULL) return;

    auto newLevel = MapAllocateArena_Name_DataTag(64, s->_memory);
    if (newLevel == NULL) return;

    if (s->_scopes == NULL) {
        s->_scopes = VecAllocateArena_MapPtr(s->_memory); // THIS SHOULD NOT HAPPEN!
    }
    VecPush_MapPtr(s->_scopes, newLevel);

    if (parameters == NULL) return;

    int length = VecLength(parameters);
    for (int i = 0; i < length; i++) {
        auto val = VecGet_DataTag(parameters, i);
        MapPut_Name_DataTag(newLevel, ScopeNameForPosition(i), *val, true);
    }
}

void ScopePush(Scope* s, DataTag* parameters, uint32_t paramCount) {
    if (s == NULL) return;

    auto newLevel = MapAllocateArena_Name_DataTag(64, s->_memory);
    if (newLevel == NULL) return;

    if (s->_scopes == NULL) {
        s->_scopes = VecAllocateArena_MapPtr(s->_memory); // THIS SHOULD NOT HAPPEN!
    }
    VecPush_MapPtr(s->_scopes, newLevel);

    if (parameters == NULL) return;

    for (int i = 0; i < paramCount; i++) {
        MapPut_Name_DataTag(newLevel, ScopeNameForPosition(i), parameters[i], true);
    }
}

void ScopeDrop(Scope* s) {
    if (s == NULL) return;

    int length = VecLength(s->_scopes);
    if (length < 2) {
        return; // refuse to drop the global scope
    }

    MapPtr last = NULL;
    VecPop_MapPtr(s->_scopes, &last);
    if (last == NULL) return;
}

DataTag ScopeResolve(Scope* s, uint32_t crushedName) {
    if (s == NULL) return NonResult();

    int currentScopeIdx = VecLength(s->_scopes) - 1;

    DataTag* found = NULL;
    for (int i = currentScopeIdx; i >= 0; i--) {
        auto scopeMap = VecGet_MapPtr(s->_scopes, i);
        if (MapGet_Name_DataTag(*scopeMap, crushedName, &found)) {
            return *found;
        }
    }

    // fell out of the loop without finding anything
    return NonResult();
}

void ScopeSetValue(Scope* s, uint32_t crushedName, DataTag newValue) {
    if (s == NULL) return;

    // try to find an existing value and change it
    // otherwise, insert a new value

    int currentScopeIdx = VecLength(s->_scopes) - 1;
    
    DataTag* found = NULL;
    for (int i = currentScopeIdx; i >= 0; i--) {
        auto scopeMap = *VecGet_MapPtr(s->_scopes, i);
        if ( ! MapGet_Name_DataTag(scopeMap, crushedName, &found)) { // TODO: Make this more efficient, so we don't have to repeatedly walk up scopes.
            continue; // not in this scope
        }

        if (TagsAreEqual(newValue, *found)) return; // nothing needs doing

        // save the new value in scope and exit
        MapPut_Name_DataTag(scopeMap, crushedName, newValue, true);
        return;
    }

    // Dropped out of the loop without finding an existing value.
    // We now save a new value in the inner-most scope
    MapPtr innerScope = NULL;
    VecPeek_MapPtr(s->_scopes, &innerScope);
    MapPut_Name_DataTag(innerScope, crushedName, newValue, true);
}

bool ScopeCanResolve(Scope* s, uint32_t crushedName) {
    auto tag = ScopeResolve(s, crushedName);
    return IsTagValid(tag);
}

void ScopeRemove(Scope* s, uint32_t crushedName) {
    if (s == NULL) return;
    if (VecLength(s->_scopes) < 1) return;

    // Only works in inner-most or global scope -- global first
    MapPtr scope = *VecGet_MapPtr(s->_scopes, 0);
    if (MapRemove_Name_DataTag(scope, crushedName)) return;
    VecPeek_MapPtr(s->_scopes, &scope);
    MapRemove_Name_DataTag(scope, crushedName);
}

bool InScope(Scope* s, uint32_t crushedName) {
    if (s == NULL) return false;

    MapPtr scope = NULL;
    VecPeek_MapPtr(s->_scopes, &scope);
    return MapGet_Name_DataTag(scope, crushedName, NULL);
}

uint32_t ScopeNameForPosition(int i) {
    // For simplicity, we use a completely artificial hash value here. Hopefully the low odds of collision won't bite us!
    uint32_t h = i;
    h = (h << 16) + i;
    h |= 0x80000001;
    return h;
}

void ScopeMutateNumber(Scope* s, uint32_t crushedName, int8_t increment) {
    if (s == NULL) return;

    int currentScopeIdx = VecLength(s->_scopes) - 1;

    DataTag* found = NULL;
    for (int i = currentScopeIdx; i >= 0; i--) {
        auto scopeMap = VecGet_MapPtr(s->_scopes, i);
        if (MapGet_Name_DataTag(*scopeMap, crushedName, &found)) {
            // we have a pointer direct to the stored value, so can change it in place...
            // NOTE: we are currently assuming the data is an int32_t. In the future, we need to handle number conversion.
            found->data = (int32_t)(found->data) + increment;
            return;
        }
    }

    // fell out of the loop without finding anything
    return;
}

