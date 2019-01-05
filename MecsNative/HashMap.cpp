#include "HashMap.h"
#include "Vector.h"
#include <stdlib.h>


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
    void* key;
    void* value;
} HashMap_Entry;

bool ResizeNext(HashMap * h); // defined below

uint DistanceToInitIndex(HashMap * h, uint indexStored) {
    if (!h->buckets.IsValid) return indexStored + h->count;

    var entry = (HashMap_Entry*)VectorGet(&(h->buckets), indexStored);
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

bool PutInternal(HashMap * h, HashMap_Entry* entry, bool canReplace, bool checkDuplicates)
{
    uint indexInit = entry->hash & h->countMod;
    uint probeCurrent = 0;

    for (uint i = 0; i < h->count; i++) {
        var indexCurrent = (indexInit + i) & h->countMod;

        if (!h->buckets.IsValid) return false;
        var current = (HashMap_Entry*)VectorGet(&(h->buckets), indexCurrent);
        if (current == NULL) return false; // internal failure

        if (current->hash == 0) {
            h->countUsed++;
            VectorSet(&(h->buckets), indexCurrent, entry, NULL);
            return true;
        }

        if (checkDuplicates && (entry->hash == current->hash) && h->KeyComparer(entry->key, current->key)) {
            if (!canReplace) return false;

            if (!h->buckets.IsValid) return false;
            VectorSet(&(h->buckets), indexCurrent, entry, NULL);
            return true;
        }

        // Perform the core robin-hood balancing
        var probeDistance = DistanceToInitIndex(h, indexCurrent);
        if (probeCurrent > probeDistance) {
            probeCurrent = probeDistance;
            if (!Swap(&(h->buckets), indexCurrent, entry)) return false;
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

    h->count = newSize;
    if (newSize > 0 && newSize < MIN_BUCKET_SIZE) newSize = MIN_BUCKET_SIZE;
    if (newSize > MAX_BUCKET_SIZE) newSize = MAX_BUCKET_SIZE;

    h->countMod = newSize - 1;

    h->buckets = VectorAllocate(sizeof(HashMap_Entry));//new Vector<Entry>(_alloc, _mem);
    if (!h->buckets.IsValid) return false;
    if (!VectorPrealloc(&h->buckets, newSize)) return false;

    h->growAt = autoSize ? (uint)(newSize*LOAD_FACTOR) : newSize;
    h->shrinkAt = autoSize ? newSize >> 2 : 0;

    h->countUsed = 0;

    // if the old buckets are null or empty, there are no values to copy
    if (!oldBuckets.IsValid) return true;
    if ((oldCount <= 0) || (newSize == 0)) {
        VectorDeallocate(&oldBuckets);
        return true;
    }

    // old values need adding to the new hashmap
    for (uint i = 0; i < oldCount; i++) {
        var res = (HashMap_Entry*)VectorGet(&oldBuckets, i);
        if (res == NULL) continue;
        if (res->hash != 0) PutInternal(h, res, false, false);
    }

    VectorDeallocate(&oldBuckets);
    return true;
}

unsigned long NextPow2(unsigned long c) {
    c--;
    c |= c >> 1;
    c |= c >> 2;
    c |= c >> 4;
    c |= c >> 8;
    c |= c >> 16;
    c |= c >> 32;
    return ++c;
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

HashMap HashMapAllocate(unsigned int size, bool(*keyComparerFunc)(void *key_A, void *key_B), unsigned int(*getHashFunc)(void *key))
{
    auto result = HashMap(); // need to ensure values are zeroed
    result.KeyComparer = keyComparerFunc;
    result.GetHash = getHashFunc;
    result.IsValid = Resize(&result, (uint)NextPow2(size), false);
    return result;
}

void Deallocate(HashMap * h)
{
    h->count = 0;
    if(h->buckets.IsValid) VectorDeallocate(&(h->buckets));
    h->IsValid = false;
}

bool Find(HashMap * h, void* key, uint* index) {
    *index = 0;
    if (h->countUsed <= 0) return false;
    if (!h->buckets.IsValid) return false;

    uint hash = h->GetHash(key);
    uint indexInit = hash & h->countMod;
    uint probeDistance = 0;

    for (uint i = 0; i < h->count; i++) {
        *index = (indexInit + i) & h->countMod;
        var res = (HashMap_Entry*)VectorGet(&(h->buckets), *index);
        if (res == NULL) return false; // internal failure

        // found?
        if ((hash == res->hash) && h->KeyComparer(key, res->key)) return true;

        // not found, probe further
        if (res->hash != 0) probeDistance = DistanceToInitIndex(h, *index);

        if (i > probeDistance) break;
    }

    return false;
}

bool Get(HashMap * h, void * key, void ** outValue) {
    uint index = 0;
    if (!Find(h, key, &index)) return false;

    var res = (HashMap_Entry*)VectorGet(&(h->buckets), index);
    if (res == NULL) {
        return false;
    }

    *outValue = res->value;
    return true;
}

bool Put(HashMap * h, void * key, void * value, bool canReplace) {
    if (h->countUsed >= h->growAt) {
        if (!ResizeNext(h)) return false;
    }
    
    auto entry = HashMap_Entry{ h->GetHash(key), key, value };

    return PutInternal(h, &entry, canReplace, true);
}

Vector AllEntries(HashMap * h) {
    if (!h->buckets.IsValid) return Vector();

    var result = VectorAllocate(sizeof(HashMap_KVP));

    for (uint i = 0; i < h->count; i++) {
        var ent = (HashMap_Entry*)VectorGet(&(h->buckets), i);
        if (ent->hash == 0) continue;

        auto kvp = HashMap_KVP { ent->key, ent->value };
        VectorPush(&result, &kvp);
    }
    return result;
}

bool ContainsKey(HashMap * h, void * key)
{
    uint idx;
    return Find(h, key, &idx);
}

bool Remove(HashMap * h, void * key) {
    uint index;
    if (!Find(h, key, &index)) return false;

    for (uint i = 0; i < h->count; i++) {
        var curIndex = (index + i) & h->countMod;
        var nextIndex = (index + i + 1) & h->countMod;

        var res = (HashMap_Entry*)VectorGet(&(h->buckets), nextIndex);
        if (res == NULL) return false; // internal failure

        if ((res->hash == 0) || (DistanceToInitIndex(h, nextIndex) == 0))
        {
            auto empty = HashMap_Entry();
            VectorSet(&(h->buckets), curIndex, &empty, NULL);

            if (--(h->countUsed) == h->shrinkAt) Resize(h, h->shrinkAt, true);

            return true;
        }

        VectorSwap(&(h->buckets), curIndex, nextIndex);
    }

    return false;
}

void Clear(HashMap * h) {
    Resize(h, 0, true);
}

unsigned int Count(HashMap * h) {
    return h->countUsed;
}
