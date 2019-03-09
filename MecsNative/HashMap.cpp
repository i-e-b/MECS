#include "HashMap.h"
#include "Vector.h"
#include "String.h"
#include "MemoryManager.h"
#include "RawData.h"

// Fixed sizes -- these are structural to the code and must not changeo
const unsigned int MAX_BUCKET_SIZE = 1<<30; // safety limit for scaling the buckets
const unsigned int SAFE_HASH = 0x80000000; // just in case you get a zero result

// Tuning parameters: have a play if you have performance or memory issues.
const unsigned int MIN_BUCKET_SIZE = 64; // default size used if none given
const float LOAD_FACTOR = 0.8f; // higher is more memory efficient. Lower is faster, to a point.

// Entry in the hash-table
// The actual entries are tagged on the end of the entry
typedef struct HashMap_Entry {
    uint32_t hash; // NOTE: a hash value equal to zero marks an empty slot
} HashMap_Entry;

typedef struct HashMap {
    // Storage and types
    Vector* buckets; // this is a Vector<HashMap_Entry + keydata + valuedata>

    int KeyByteSize; // byte length of the key
    int ValueByteSize; // byte length of the value

    // Hashmap metrics
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


bool HashMapIsValid(HashMap *h) {
    if (h == NULL) return false;
    if (!VectorIsValid(h->buckets)) return false;
    return h->IsValid;
}

bool ResizeNext(HashMap * h); // defined below

uint32_t DistanceToInitIndex(HashMap * h, uint32_t indexStored) {
    if (!VectorIsValid(h->buckets)) return indexStored + h->count;

    auto entry = (HashMap_Entry*)VectorGet(h->buckets, indexStored);
    auto indexInit = entry->hash & h->countMod;
    if (indexInit <= indexStored) return indexStored - indexInit;
    return indexStored + (h->count - indexInit);
}

void* KeyPtr(HashMap_Entry* e) {
    return byteOffset(e, sizeof(HashMap_Entry));
}
void* ValuePtr(HashMap* h, HashMap_Entry* e) {
    return byteOffset(e, sizeof(HashMap_Entry) + h->KeyByteSize);
}

bool Swap(Vector* vec, uint32_t idx, HashMap_Entry* newEntry) {
    if (vec == NULL) return false;

    HashMap_Entry temp;
    if (!VectorCopy(vec, idx, &temp)) return false;

    VectorSet(vec, idx, newEntry, NULL);
    *newEntry = temp;
    return true;
}

bool PutInternal(HashMap * h, HashMap_Entry* entry, bool canReplace, bool checkDuplicates) {
    uint32_t indexInit = entry->hash & h->countMod;
    uint32_t probeCurrent = 0;

    for (uint32_t i = 0; i < h->count; i++) {
        auto indexCurrent = (indexInit + i) & h->countMod;

        if (!VectorIsValid(h->buckets))
            return false;
        auto current = (HashMap_Entry*)VectorGet(h->buckets, indexCurrent);
        if (current == NULL) return false; // internal failure

        if (current->hash == 0) {
            h->countUsed++;
            VectorSet(h->buckets, indexCurrent, entry, NULL);
            return true;
        }

        if (checkDuplicates && (entry->hash == current->hash)
            && h->KeyComparer(KeyPtr(entry), KeyPtr(current))
            ) {
            if (!canReplace) return false;

            if (!VectorIsValid(h->buckets))
                return false;
            VectorSet(h->buckets, indexCurrent, entry, NULL);
            return true;
        }

        // Perform the core robin-hood balancing
        auto probeDistance = DistanceToInitIndex(h, indexCurrent);
        if (probeCurrent > probeDistance) {
            probeCurrent = probeDistance;
            if (!Swap(h->buckets, indexCurrent, entry))
                return false;
        }
        probeCurrent++;
    }
    // need to grow?
    // Trying recursive insert:
    if (!ResizeNext(h)) return false;
    return PutInternal(h, entry, canReplace, checkDuplicates);
}

bool Resize(HashMap * h, uint32_t newSize, bool autoSize) {
    auto oldCount = h->count;
    auto oldBuckets = h->buckets;

    h->count = newSize;
    if (newSize > 0 && newSize < MIN_BUCKET_SIZE) newSize = MIN_BUCKET_SIZE;
    if (newSize > MAX_BUCKET_SIZE) newSize = MAX_BUCKET_SIZE;

    h->countMod = newSize - 1;

    auto newBuckets = VectorAllocate(sizeof(HashMap_Entry) + h->KeyByteSize + h->ValueByteSize);
    if (!VectorIsValid(newBuckets) || !VectorPrealloc(newBuckets, newSize)) return false;

    h->buckets = newBuckets;

    h->growAt = autoSize ? (uint32_t)(newSize * LOAD_FACTOR) : newSize;
    h->shrinkAt = autoSize ? newSize >> 2 : 0;

    h->countUsed = 0;


    // if the old buckets are null or empty, there are no values to copy
    if (!VectorIsValid(oldBuckets)) { return true; }

    // old values need adding to the new hashmap
    if (newSize > 0) {
        for (uint32_t i = 0; i < oldCount; i++) {
            // Read and validate old bucket entry
            auto oldEntry = (HashMap_Entry*)VectorGet(oldBuckets, i);
            if (oldEntry == NULL || oldEntry->hash == 0) continue;

            // Copy the old entry into the new bucket vector
            PutInternal(h, oldEntry, false, false);
        }
    }

    VectorDeallocate(oldBuckets);
    return true;
}

unsigned long long NextPow2(unsigned long long c) {
    c--;
    c |= c >> 1;
    c |= c >> 2;
    c |= c >> 4;
    c |= c >> 8;
    c |= c >> 16;
    c |= c >> 32;
    return ++c;
}

void HashMapPurge(HashMap *h) {
    auto size = NextPow2((h->countUsed) / LOAD_FACTOR);
    Resize(h, size, true);
}

bool ResizeNext(HashMap * h) {
    // mild scaling can save memory, but resizing is very expensive -- so the default is an aggressive algorithm
    // Mild scaling
    //return Resize(count == 0 ? 32 : count*2);

    // Aggressive scaling
    unsigned long size = (unsigned long)h->count * 2;
    if (h->count < 8192) size = (unsigned long)h->count * h->count;
    if (size < MIN_BUCKET_SIZE) size = MIN_BUCKET_SIZE;
    return Resize(h, (uint32_t)size, true);
}

HashMap* HashMapAllocate(uint32_t size, int keyByteSize, int valueByteSize, bool(*keyComparerFunc)(void *key_A, void *key_B), unsigned int(*getHashFunc)(void *key)) {
    auto result = (HashMap*)mcalloc(1, sizeof(HashMap)); // need to ensure values are zeroed
    result->KeyByteSize = keyByteSize;
    result->ValueByteSize = valueByteSize;
    result->KeyComparer = keyComparerFunc;
    result->GetHash = getHashFunc;
    result->buckets = NULL; // created in `Resize`
    result->IsValid = Resize(result, (uint32_t)NextPow2(size), false);
    return result;
}

void HashMapDeallocate(HashMap * h) {
    h->IsValid = false;
    h->count = 0;
    if (h->buckets != NULL) VectorDeallocate(h->buckets);
}

bool Find(HashMap* h, void* key, uint32_t* index) {
    *index = 0;
    if (h->countUsed <= 0) return false;
    if (!VectorIsValid(h->buckets)) return false;

    uint32_t hash = h->GetHash(key);
    uint32_t indexInit = hash & h->countMod;
    uint32_t probeDistance = 0;

    for (uint32_t i = 0; i < h->count; i++) {
        *index = (indexInit + i) & h->countMod;
        auto res = (HashMap_Entry*)VectorGet(h->buckets, *index);
        if (res == NULL) return false; // internal failure

        auto keyData = KeyPtr(res);//VectorGet(h->keys, res->key);

        // found?
        if ((hash == res->hash) && h->KeyComparer(key, keyData)) return true;

        // not found, probe further
        if (res->hash != 0) probeDistance = DistanceToInitIndex(h, *index);

        if (i > probeDistance) break;
    }

    return false;
}

bool HashMapGet(HashMap* h, void* key, void** outValue) {
    // Find the entry index
    uint32_t index = 0;
    if (!Find(h, key, &index)) return false;

    // look up the entry
    auto res = (HashMap_Entry*)VectorGet(h->buckets, index);
    if (res == NULL) {
        return false;
    }

    // look up the value
    *outValue = ValuePtr(h, res);//VectorGet(h->values, res->value);
    return true;
}

bool HashMapPut(HashMap* h, void* key, void* value, bool canReplace) {
    // Check to see if we need to grow
    if (h->countUsed >= h->growAt) {
        if (!ResizeNext(h)) return false;
    }

    uint32_t safeHash = h->GetHash(key);
    if (safeHash == 0) safeHash = SAFE_HASH; // can't allow hash of zero
    
    // Write the entry into the hashmap
    auto entry = (HashMap_Entry*)mmalloc(sizeof(HashMap_Entry) + h->KeyByteSize + h->ValueByteSize);
    entry->hash = safeHash;
    writeValue(KeyPtr(entry), 0, key, h->KeyByteSize);
    writeValue(ValuePtr(h, entry), 0, value, h->ValueByteSize);

    auto OK = PutInternal(h, entry, canReplace, true);

    mfree(entry);
    return OK;
}

Vector *HashMapAllEntries(HashMap* h) {
    auto result = VectorAllocate(sizeof(HashMap_KVP));
    if (!VectorIsValid(h->buckets)) return result;

    for (uint32_t i = 0; i < h->count; i++) {
        // Read and validate entry
        auto ent = (HashMap_Entry*)VectorGet(h->buckets, i);
        if (ent->hash == 0) continue;

        // Look up pointers to the data
        auto keyPtr = KeyPtr(ent);
        auto valuePtr = ValuePtr(h, ent);

        // Add Key-Value pair to output
        auto kvp = HashMap_KVP { keyPtr, valuePtr };
        VectorPush(result, &kvp);
    }
    return result;
}

bool HashMapContains(HashMap* h, void* key) {
    uint32_t idx;
    return Find(h, key, &idx);
}

bool HashMapRemove(HashMap* h, void* key) {
    uint32_t index;
    if (!Find(h, key, &index)) return false;

    // Note: we only remove the hashmap entry. The data is retained (mostly to keep the code simple)
    // Data will be purged when a resize happens
    for (uint32_t i = 0; i < h->count; i++) {
        auto curIndex = (index + i) & h->countMod;
        auto nextIndex = (index + i + 1) & h->countMod;

        auto res = (HashMap_Entry*)VectorGet(h->buckets, nextIndex);
        if (res == NULL) return false; // internal failure

        if ((res->hash == 0) || (DistanceToInitIndex(h, nextIndex) == 0))
        {
            auto empty = HashMap_Entry();
            VectorSet(h->buckets, curIndex, &empty, NULL);

            if (--(h->countUsed) == h->shrinkAt) Resize(h, h->shrinkAt, true);

            return true;
        }

        VectorSwap(h->buckets, curIndex, nextIndex);
    }

    return false;
}

void HashMapClear(HashMap * h) {
    Resize(h, 0, true);
}

unsigned int HashMapCount(HashMap * h) {
    return h->countUsed;
}

// Default comparers and hashers:

bool HashMapStringKeyCompare(void* key_A, void* key_B) {
    auto A = *((StringPtr*)key_A);
    auto B = *((StringPtr*)key_B);
    return StringAreEqual(A, B);
}
unsigned int HashMapStringKeyHash(void* key) {
    auto A = *((StringPtr*)key);
    return StringHash(A);
}
bool HashMapIntKeyCompare(void* key_A, void* key_B) {
    auto A = *((int*)key_A);
    auto B = *((int*)key_B);
    return A == B;
}
unsigned int HashMapIntKeyHash(void* key) {
    auto A = *((unsigned int*)key);
    return A | 0xA0000000;
}
