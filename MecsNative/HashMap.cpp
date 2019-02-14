#include "HashMap.h"
#include "Vector.h"
#include "String.h"
#include "MemoryManager.h"

// Fixed sizes -- these are structural to the code and must not changeo
const unsigned int MAX_BUCKET_SIZE = 1<<30; // safety limit for scaling the buckets
const unsigned int SAFE_HASH = 0x80000000; // just in case you get a zero result

// Tuning parameters: have a play if you have performance or memory issues.
const unsigned int MIN_BUCKET_SIZE = 64; // default size used if none given
const float LOAD_FACTOR = 0.8f; // higher is more memory efficient. Lower is faster, to a point.

// Some helpers going from C# prototype to C
#ifndef NULL
#define NULL 0
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef var
#define var auto
#endif

// Entry in the hash-table
typedef struct HashMap_Entry {
    uint hash; // NOTE: a hash value equal to zero marks an empty slot
    uint key; // index into the keys vector
    uint value; // index into the values vector
} HashMap_Entry;

typedef struct HashMap {
    // Storage and types
    Vector* buckets; // this is a Vector<HashMap_Entry>, referencing the other vectors
    Vector* keys; // these are the stored keys (either data or pointers depending on caller's use)
    Vector* values; // the stored values (similar to keys)

    int KeyByteSize;
    int ValueByteSize;

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

bool ResizeNext(HashMap * h); // defined below

uint DistanceToInitIndex(HashMap * h, uint indexStored) {
    if (!VectorIsValid(h->buckets)) return indexStored + h->count;

    var entry = (HashMap_Entry*)VectorGet(h->buckets, indexStored);
    var indexInit = entry->hash & h->countMod;
    if (indexInit <= indexStored) return indexStored - indexInit;
    return indexStored + (h->count - indexInit);
}

bool Swap(Vector* vec, uint idx, HashMap_Entry* newEntry) {
    if (vec == NULL) return false;

    HashMap_Entry temp;
    if (!VectorCopy(vec, idx, &temp)) return false;

    VectorSet(vec, idx, newEntry, NULL);
    *newEntry = temp;
    return true;
}

bool PutInternal(HashMap * h, HashMap_Entry* entry, bool canReplace, bool checkDuplicates) {
    uint indexInit = entry->hash & h->countMod;
    uint probeCurrent = 0;

    for (uint i = 0; i < h->count; i++) {
        var indexCurrent = (indexInit + i) & h->countMod;

        if (!VectorIsValid(h->buckets)) return false;
        var current = (HashMap_Entry*)VectorGet(h->buckets, indexCurrent);
        if (current == NULL) return false; // internal failure

        if (current->hash == 0) {
            h->countUsed++;
            VectorSet(h->buckets, indexCurrent, entry, NULL);
            return true;
        }

        if (checkDuplicates && (entry->hash == current->hash)
            && h->KeyComparer(VectorGet(h->keys, entry->key), VectorGet(h->keys, current->key))
            ) {
            if (!canReplace) return false;

            if (!VectorIsValid(h->buckets)) return false;
            VectorSet(h->buckets, indexCurrent, entry, NULL);
            return true;
        }

        // Perform the core robin-hood balancing
        var probeDistance = DistanceToInitIndex(h, indexCurrent);
        if (probeCurrent > probeDistance) {
            probeCurrent = probeDistance;
            if (!Swap(h->buckets, indexCurrent, entry)) return false;
        }
        probeCurrent++;
    }
    // need to grow?
    // Trying recursive insert:
    if (!ResizeNext(h)) return false;
    return PutInternal(h, entry, canReplace, checkDuplicates);
}

bool Resize(HashMap * h, uint newSize, bool autoSize) {
    var oldCount = h->count;
    var oldBuckets = h->buckets;
    var oldKeyVec = h->keys;
    var oldValVec = h->values;

    h->count = newSize;
    if (newSize > 0 && newSize < MIN_BUCKET_SIZE) newSize = MIN_BUCKET_SIZE;
    if (newSize > MAX_BUCKET_SIZE) newSize = MAX_BUCKET_SIZE;

    h->countMod = newSize - 1;

    auto newBuckets = VectorAllocate(sizeof(HashMap_Entry));
    if (!VectorIsValid(newBuckets) || !VectorPrealloc(newBuckets, newSize)) return false;

    auto newKeys = VectorAllocate(h->KeyByteSize);
    if (!VectorIsValid(newKeys)) {
        VectorDeallocate(newBuckets); return false;
    }

    auto newValues = VectorAllocate(h->ValueByteSize);
    if (!VectorIsValid(newValues)) {
        VectorDeallocate(newBuckets); VectorDeallocate(newKeys); return false;
    }

    h->buckets = newBuckets;
    h->keys = newKeys;
    h->values = newValues;

    h->growAt = autoSize ? (uint)(newSize*LOAD_FACTOR) : newSize;
    h->shrinkAt = autoSize ? newSize >> 2 : 0;

    h->countUsed = 0;

    // if the old buckets are null or empty, there are no values to copy
    if (!VectorIsValid(oldBuckets)) return true;

    // old values need adding to the new hashmap
    if (newSize > 0) {
        for (uint i = 0; i < oldCount; i++) {
            // Read and validate old bucket entry
            var oldEntry = (HashMap_Entry*)VectorGet(oldBuckets, i);
            if (oldEntry == NULL || oldEntry->hash == 0) continue;

            // Copy key and value data across
            uint keyIdx = VectorLength(h->keys);
            VectorPush(h->keys, VectorGet(oldKeyVec, oldEntry->key));
            uint valueIdx = VectorLength(h->values);
            VectorPush(h->values, VectorGet(oldValVec, oldEntry->value));

            // Put new etry into new bucket vector
            auto newEntry = HashMap_Entry{ oldEntry->hash, keyIdx, valueIdx };
            PutInternal(h, &newEntry, false, false);
        }
    }

    VectorDeallocate(oldBuckets);
    if (VectorIsValid(oldKeyVec)) VectorDeallocate(oldKeyVec); // TODO: this is breaking. I think the chain pointers are being corrupted.
    if (VectorIsValid(oldValVec)) VectorDeallocate(oldValVec);
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
    return Resize(h, (uint)size, true);
}

HashMap* HashMapAllocate(unsigned int size, int keyByteSize, int valueByteSize, bool(*keyComparerFunc)(void *key_A, void *key_B), unsigned int(*getHashFunc)(void *key))
{
    auto result = (HashMap*)mcalloc(1, sizeof(HashMap)); // need to ensure values are zeroed
    result->KeyByteSize = keyByteSize;
    result->ValueByteSize = valueByteSize;
    result->KeyComparer = keyComparerFunc;
    result->GetHash = getHashFunc;
    result->IsValid = Resize(result, (uint)NextPow2(size), false);
    return result;
}

void HashMapDeallocate(HashMap * h)
{
    h->IsValid = false;
    h->count = 0;
    if (h->buckets != NULL) VectorDeallocate(h->buckets);
    if (h->keys != NULL) VectorDeallocate(h->keys);
    if (h->values != NULL) VectorDeallocate(h->values);
}

bool Find(HashMap* h, void* key, uint* index) {
    *index = 0;
    if (h->countUsed <= 0) return false;
    if (!VectorIsValid(h->buckets)) return false;

    uint hash = h->GetHash(key);
    uint indexInit = hash & h->countMod;
    uint probeDistance = 0;

    for (uint i = 0; i < h->count; i++) {
        *index = (indexInit + i) & h->countMod;
        var res = (HashMap_Entry*)VectorGet(h->buckets, *index);
        if (res == NULL) return false; // internal failure

        auto keyData = VectorGet(h->keys, res->key);

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
    uint index = 0;
    if (!Find(h, key, &index)) return false;

    // look up the entry
    var res = (HashMap_Entry*)VectorGet(h->buckets, index);
    if (res == NULL) {
        return false;
    }

    // look up the value
    *outValue = VectorGet(h->values, res->value);
    return true;
}

bool HashMapPut(HashMap* h, void* key, void* value, bool canReplace) {
    // Check to see if we need to grow
    if (h->countUsed >= h->growAt) {
        if (!ResizeNext(h)) return false;
    }

    // Copy key and value bytes into our vectors
    uint valueIdx = VectorLength(h->values);
    VectorPush(h->values, value);
    uint keyIdx = VectorLength(h->keys);
    VectorPush(h->keys, key);

    uint safeHash = h->GetHash(key);
    if (safeHash == 0) safeHash = SAFE_HASH; // can't allow hash of zero
    
    // Write the entry into the hashmap
    auto entry = HashMap_Entry{ safeHash, keyIdx, valueIdx };
    return PutInternal(h, &entry, canReplace, true);
}

Vector *HashMapAllEntries(HashMap* h) {
    var result = VectorAllocate(sizeof(HashMap_KVP));
    if (!VectorIsValid(h->buckets)) return result;

    for (uint i = 0; i < h->count; i++) {
        // Read and validate entry
        var ent = (HashMap_Entry*)VectorGet(h->buckets, i);
        if (ent->hash == 0) continue;

        // Look up pointers to the data
        auto keyPtr = VectorGet(h->keys, ent->key);
        auto valuePtr = VectorGet(h->values, ent->value);

        // Add Key-Value pair to output
        auto kvp = HashMap_KVP { keyPtr, valuePtr };
        VectorPush(result, &kvp);
    }
    return result;
}

bool HashMapContains(HashMap* h, void* key)
{
    uint idx;
    return Find(h, key, &idx);
}

bool HashMapRemove(HashMap* h, void* key) {
    uint index;
    if (!Find(h, key, &index)) return false;

    // Note: we only remove the hashmap entry. The data is retained (mostly to keep the code simple)
    // Data will be purged when a resize happens
    for (uint i = 0; i < h->count; i++) {
        var curIndex = (index + i) & h->countMod;
        var nextIndex = (index + i + 1) & h->countMod;

        var res = (HashMap_Entry*)VectorGet(h->buckets, nextIndex);
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
