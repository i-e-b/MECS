#include <iostream>

#include "Vector.h"
#include "HashMap.h"

// So, the idea for general containers, have some basic `void*` and size functionality,
// and some macros to init an inline casting function for each type.
// Like this:

typedef struct exampleElement {
    int a; int b;
} exampleElement;
exampleElement fakeData = exampleElement{ 5,15 };

// Register type specifics
RegisterVectorFor(exampleElement, Vec)
RegisterVectorFor(HashMap_KVP, Vec)

bool IntKeyCompare(void* key_A, void* key_B) {
    auto A = *((int*)key_A);
    auto B = *((int*)key_B);
    return A == B;
}
unsigned int IntKeyHash(void* key) {
    auto A = *((unsigned int*)key);
    return A;
}

int main()
{
    int x = 5, y = 4;
    char _this_hello_is_special[] = "Special value";

    std::cout << "*************** HASH MAP *****************\n";
    std::cout << "Allocating\n";
    auto hmap = HashMapAllocate(64, IntKeyCompare, IntKeyHash);
    std::cout << "HashMap OK? " << hmap.IsValid << "\n";

    std::cout << "Writing entries\n";
    for (int i = 0; i < 100; i++) {
        auto key = (int*)malloc(sizeof(int));
        auto value = (int*)malloc(sizeof(int));
        *key = i;
        *value = 2 * i;
        Put(&hmap, key, value, true);
    }

    std::cout << "Looking up data\n";
    int lukey = 40;
    int* lu_val_ptr = NULL;
    if (!Get(&hmap, &lukey, (void**)&lu_val_ptr)) {
        std::cout << "Get failed!\n";
        return 1;
    }
    std::cout << "Found value " << *lu_val_ptr << " (expected 80)\n";


    std::cout << "Deallocating contents\n";
    //Vector AllEntries(HashMap *h);
    auto allEntriesVec = AllEntries(&hmap);
    HashMap_KVP toClean;
    while (VecPop_HashMap_KVP(&allEntriesVec, &toClean)) {
        free(toClean.Key);
        free(toClean.Value);
    }


    std::cout << "Deallocating map\n";
    Deallocate(&hmap);
    std::cout << "HashMap OK? " << hmap.IsValid << "\n";


    std::cout << "**************** VECTOR *******************\n";
    auto testElement = exampleElement{ 20,5 };

    // See if the container stuff works...
    std::cout << "Allocating\n";
    auto gvec = VecAllocate_exampleElement();
    std::cout << "Vector OK? " << gvec.IsValid << "; base addr = " << gvec._baseChunkTable << "\n";

    // add some entries
    std::cout << "Writing entries with 'push'\n";
    for (int i = 0; i < 1000; i++) {
        if (!VecPush_exampleElement(&gvec, testElement)) {
            std::cout << "Push failed!\n";
            return 255;
        }
    }
    std::cout << "Vector OK? " << gvec.IsValid << "; Elements stored = " << VectorLength(&gvec) << "\n";

    // read a random-access element
    auto r = VecGet_exampleElement(&gvec, 5);
    std::cout << "Element 5 data = " << r->a << ", " << r->b << "\n";

    // resize the array
    VectorPrealloc(&gvec, 5000);
    std::cout << "Vector OK? " << gvec.IsValid << "; Elements stored = " << VectorLength(&gvec) << "\n";

    // Pop a load of values
    std::cout << "Reading and removing entries with 'pop'\n";
    for (int i = 0; i < 4000; i++) {
        exampleElement poppedData;
        if (!VecPop_exampleElement(&gvec, &poppedData)) {
            std::cout << "Pop failed!\n";
            return 254;
        }
    }
    std::cout << "Vector OK? " << gvec.IsValid << "; Elements stored = " << VectorLength(&gvec) << "\n";

    // Set a different element value
    auto newData = exampleElement{ 255,511 };
    exampleElement capturedOldData;
    VecSet_exampleElement(&gvec, 70, &newData, &capturedOldData);
    std::cout << "Replace value at 70. Old data = " << capturedOldData.a << ", " << capturedOldData.b << "\n";
    r = VecGet_exampleElement(&gvec, 70);
    std::cout << "Element 70 new data = " << r->a << ", " << r->b << "\n";

    // Swap elements by index pair
    std::cout << "Swapping\n";
    VectorSwap(&gvec, 60, 70);
    r = VecGet_exampleElement(&gvec, 60);
    std::cout << "Element 60 new data = " << r->a << ", " << r->b << "\n";
    r = VecGet_exampleElement(&gvec, 70);
    std::cout << "Element 70 new data = " << r->a << ", " << r->b << "\n";

    std::cout << "Deallocating\n";
    VectorDeallocate(&gvec);
    std::cout << "Vector OK? " << gvec.IsValid << "; base addr = " << gvec._baseChunkTable << "\n";
}
