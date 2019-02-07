#include "Scope.h"

#include "MemoryManager.h"

bool Scope_IntKeyCompare(void* key_A, void* key_B) {
    auto A = *((uint32_t*)key_A);
    auto B = *((uint32_t*)key_B);
    return A == B;
}
unsigned int Scope_IntKeyHash(void* key) {
    auto A = *((uint32_t*)key);
    return A;
}

typedef HashMap* MapPtr;
typedef uint32_t Name;

RegisterHashMapStatics(Map)
RegisterHashMapFor(Name, DataTag, Scope_IntKeyHash, Scope_IntKeyCompare, Map)
RegisterHashMapFor(Name, bool, Scope_IntKeyHash, Scope_IntKeyCompare, Map)

RegisterVectorStatics(Vec)
RegisterVectorFor(MapPtr, Vec)
RegisterVectorFor(DataTag, Vec)
RegisterVectorFor(ScopeReference, Vec)
RegisterVectorFor(Map_KVP_Name_DataTag, Vec)


typedef struct Scope {
    // Hashmap of Name->DataTag
    HashMap* PotentialGarbage;
    // Vector of (Hashmap of Name->DataTag)
    Vector* _scopes;
    // The current head of the `_scopes` vector
    int _currentScopeIdx;
} Scope;


Scope* ScopeAllocate() {
    auto result = (Scope*)mmalloc(sizeof(Scope));
    if (result == NULL) return NULL;

    auto firstMap = (MapPtr)MapAllocate_Name_DataTag(127);
    if (firstMap == NULL) { mfree(result); return NULL; }

    result->PotentialGarbage = MapAllocate_Name_DataTag(1024);
    result->_scopes = VecAllocate_MapPtr();
    result->_currentScopeIdx = 0;

    if (result->PotentialGarbage == NULL || result->_scopes == NULL) {
        ScopeDeallocate(result);
        return NULL;
    }

    VecPush_MapPtr(result->_scopes, firstMap);
}

void ScopeDeallocate(Scope* s) {
    if (s == NULL) return;
    if (s->PotentialGarbage != NULL) MapDeallocate(s->PotentialGarbage);
    if (s->_scopes != NULL) {
        MapPtr map;
        while (VecPop_MapPtr(s->_scopes, &map)) {
            MapDeallocate(map);
        }
    }
    mfree(s);
}

Scope* ScopeClone(Scope* source) {
    auto result = ScopeAllocate();
    if (source == NULL) return result;

    auto toCopy = ScopeAllVisible(source);
    if (toCopy == NULL) return result;

    auto globalScope = *VecGet_MapPtr(result->_scopes, 0);
    ScopeReference entry;
    while (VecPop_ScopeReference(toCopy, &entry)) {
        MapPut_Name_DataTag(globalScope, entry.crushedName, entry.value, true);
    }

    return result;
}


HashMap* ScopePotentialGarbage(Scope *s) {
    if (s == NULL) return NULL;
    return s->PotentialGarbage;
}

// List all *visible* references (vector of ScopeReference) any shadowed references are not supplied.
Vector* ScopeAllVisible(Scope* s) {
    if (s == NULL) return NULL;

    auto seen = MapAllocate_Name_bool(1024);// to record shadowing
    auto result = VecAllocate_ScopeReference();
    for (int i = s->_currentScopeIdx; i >= 0; i--) {
        auto scope = VecGet_MapPtr(s->_scopes, i); //var scope = _scopes[i];
        auto values = MapAllEntries(*VecGet_MapPtr(s->_scopes, i));

        Map_KVP_Name_DataTag entry; 
        while (VecPop_Map_KVP_Name_DataTag(values, &entry)) {
            if (MapGet_Name_bool(seen, *entry.Key, NULL)) continue; // already seen in a closer scope (shadowed)
            MapPut_Name_bool(seen, *entry.Key, true, true); // add to the 'seen' list

            ScopeReference sr;
            sr.crushedName = *entry.Key;
            sr.value = *entry.Value;
            VecPush_ScopeReference(result, sr);
        }
        VecDeallocate(values);
    }
    MapDeallocate(seen);
    return result;
}

void ScopePush(Scope* s, Vector* parameters) {
    if (s == NULL) return;

    auto newLevel = MapAllocate_Name_DataTag(64);
    if (newLevel == NULL) return;

    VecPush_MapPtr(s->_scopes, newLevel);
    s->_currentScopeIdx++;

    if (parameters == NULL) return;

    int length = VecLength(parameters);
    for (int i = 0; i < length; i++) {
        auto val = VecGet_DataTag(parameters, i);
        MapPut_Name_DataTag(newLevel, ScopeNameForPosition(i), *val, true);
    }
}

void ScopeDrop(Scope* s) {
    if (s == NULL) return;

    int length = VecLength(s->_scopes);
    if (length < 2) return; // refuse to drop the global scope

    MapPtr last = NULL;
    VecPop_MapPtr(s->_scopes, &last);
    s->_currentScopeIdx--;
    if (last == NULL) return;

    // add lost references to the potential garbage
    // this could be done out-of-band to speed up function returns
    auto refs = MapAllEntries(last);
    Map_KVP_Name_DataTag lostReference;
    while (VecPop_Map_KVP_Name_DataTag(refs, &lostReference)) {
        if (IsAllocated(*lostReference.Value)) {
            MapPut_Name_DataTag(s->PotentialGarbage, *lostReference.Key, *lostReference.Value, false);
        }
    }
    VecDeallocate(refs);
}





uint32_t ScopeNameForPosition(int i) {
    // For simplicity, we use a completely artificial hash value here. Hopefully the low odds of collision won't bite us!
    // The C# version made a name like `"__p" + i` and hashed that.
    uint32_t h = i;
    h = (h << 16) + i;
    h |= 0x80000001;
    return h;
}

