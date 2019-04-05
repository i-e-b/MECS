#include "ScopeMap.h"
#include "Vector.h"
#include "String.h"
#include "MemoryManager.h"
#include "RawData.h"

const unsigned int DEFAULT_SCOPEMAP_SIZE = 64; // default size used if none given
const float SCOPEMAP_LOAD_FACTOR = 0.5f; // higher is more memory efficient. Lower is faster, to a point.

// Entry in the map
typedef struct ScopeMap_Entry {
    uint32_t hash;
    DataTag data;
} ScopeMap_Entry;

typedef struct ScopeMap {
    // Storage and types
    ScopeMap_Entry* buckets; // this is a contiguous array of entries

    // Hashmap metrics
    unsigned int count;
    unsigned int countMod;
    unsigned int countUsed;
    unsigned int growAt;
    unsigned int shrinkAt;

    bool IsValid; // if false, the hash map has failed
} ScopeMap;


inline uint32_t DistanceToInitIndex(ScopeMap * h, uint32_t indexStored, ScopeMap_Entry* entry) {
    auto indexInit = entry->hash & h->countMod;
    if (indexInit <= indexStored) return indexStored - indexInit;
    return indexStored + (h->count - indexInit);
}

inline void SwapOut(ScopeMap_Entry* vec, uint32_t idx, ScopeMap_Entry* newEntry) {
    ScopeMap_Entry temp = vec[idx];
    vec[idx] = *newEntry;
    *newEntry = temp;
}

bool ResizeNext(ScopeMap* h); // defined below

bool PutInternal(ScopeMap * h, ScopeMap_Entry* entry) {
    uint32_t indexInit = entry->hash & h->countMod;
    uint32_t probeCurrent = 0;

    for (uint32_t i = 0; i < h->count; i++) {
        auto indexCurrent = (indexInit + i) & h->countMod;

        if (h->buckets == NULL) return false;

        auto current = h->buckets[indexCurrent];

        if (current.hash == 0) {
            h->countUsed++;
            h->buckets[indexCurrent] = *entry;
            return true;
        }

        if (entry->hash == current.hash) {
            h->buckets[indexCurrent] = *entry;
            return true;
        }

        // Perform the core robin-hood balancing
        auto probeDistance = DistanceToInitIndex(h, indexCurrent, &current);
        if (probeCurrent > probeDistance) {
            probeCurrent = probeDistance;
            SwapOut(h->buckets, indexCurrent, entry);
        }
        probeCurrent++;
    }
    // need to grow?
    // Trying recursive insert:
    if (!ResizeNext(h)) return false;
    return PutInternal(h, entry);
}

bool Resize(ScopeMap * h, uint32_t newSize, bool autoSize) {
    auto oldCount = h->count;
    auto oldBuckets = h->buckets;

    h->count = newSize;
    if (newSize > 0 && newSize < 32) newSize = 32;
    if (newSize > ARENA_ZONE_SIZE) newSize = ARENA_ZONE_SIZE;

    h->countMod = newSize - 1;

    //(ScopeMap*)mcalloc(1, sizeof(ScopeMap));
    ScopeMap_Entry* newBuckets = (ScopeMap_Entry*)mcalloc(newSize, sizeof(ScopeMap_Entry));
    if (newBuckets == NULL) return false;

    h->buckets = newBuckets;

    h->growAt = autoSize ? (uint32_t)(newSize * SCOPEMAP_LOAD_FACTOR) : newSize;
    h->shrinkAt = autoSize ? newSize >> 2 : 0;

    h->countUsed = 0;

    // if the old buckets are null or empty, there are no values to copy
    if (oldBuckets == NULL) { return true; }

    // old values need adding to the new hashmap
    if (newSize > 0) {
        for (uint32_t i = 0; i < oldCount; i++) {
            // Read and validate old bucket entry
            auto oldEntry = oldBuckets[i];
            if (oldEntry.hash == 0) continue;

            // Copy the old entry into the new bucket vector
            PutInternal(h, &oldEntry);
        }
    }

    mfree(oldBuckets);
    return true;
}

ScopeMap* ScopeMapAllocate() {
    auto result = (ScopeMap*)mcalloc(1, sizeof(ScopeMap)); // need to ensure values are zeroed
    if (result == NULL) return NULL;
    result->buckets = NULL; // created in `Resize`
    result->IsValid = Resize(result, (uint32_t)NextPow2(DEFAULT_SCOPEMAP_SIZE), false);
    return result;
}

void ScopeMapDeallocate(ScopeMap* h) {
    h->IsValid = false;
    h->count = 0;
    if (h->buckets != NULL) mfree(h->buckets);
    mfree(h);
}

bool ResizeNext(ScopeMap * h) {
    return Resize(h, h->count == 0 ? 32 : h->count * 2, true);
}

bool ScopeMapPut(ScopeMap *h, uint32_t key, DataTag value) {
    if (h == NULL) return false;

    // Check to see if we need to grow
    if (h->countUsed >= h->growAt) {
        if (!ResizeNext(h)) return false;
    }

    uint32_t safeHash = key;
    if (safeHash == 0) safeHash = 0x80000000; // can't allow hash of zero

    ScopeMap_Entry entry = ScopeMap_Entry{};
    entry.data = value;
    entry.hash = key;

    auto OK = PutInternal(h, &entry);

    return OK;
}


bool Find(ScopeMap* h, uint32_t key, uint32_t* index, ScopeMap_Entry** found) {
    *index = 0;
    if (h == NULL) return false;
    if (h->countUsed <= 0) return false;
    if (h->buckets == NULL) return false;

    uint32_t hash = key;
    uint32_t indexInit = hash & h->countMod;
    uint32_t probeDistance = 0;

    for (uint32_t i = 0; i < h->count; i++) {
        *index = (indexInit + i) & h->countMod;
        auto res = &h->buckets[*index];

        // found?
        if (hash == res->hash) {
            if (found != NULL) *found = res;
            return true;
        }

        // not found, probe further
        if (res->hash != 0) probeDistance = DistanceToInitIndex(h, *index, res);

        if (i > probeDistance) break;
    }

    return false;
}

bool ScopeMapRemove(ScopeMap* h, uint32_t key) {
    uint32_t index;
    if (!Find(h, key, &index, NULL)) return false;

    for (uint32_t i = 0; i < h->count; i++) {
        auto curIndex = (index + i) & h->countMod;
        auto nextIndex = (index + i + 1) & h->countMod;

        auto res = h->buckets[nextIndex];

        if ((res.hash == 0) || (DistanceToInitIndex(h, nextIndex, &res) == 0))
        {
            auto empty = ScopeMap_Entry();
            h->buckets[curIndex] = empty;

            if (--(h->countUsed) == h->shrinkAt) Resize(h, h->shrinkAt, true);

            return true;
        }

        // swap robin hood
        auto temp = h->buckets[curIndex];
        h->buckets[curIndex] = h->buckets[nextIndex];
        h->buckets[nextIndex] = temp;
    }

    return false;
}

bool ScopeMapGet(ScopeMap *h, uint32_t key, DataTag** outValue) {
        if (h == NULL) return false;
    // Find the entry index
    uint32_t index = 0;
    ScopeMap_Entry* res = NULL;
    if (!Find(h, key, &index, &res)) return false;

    // expose the value in place
    if (outValue != NULL) *outValue = &(res->data);

    return true;
}