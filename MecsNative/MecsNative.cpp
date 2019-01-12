#include <iostream>

#include "Vector.h"
#include "HashMap.h"
#include "Tree.h"
#include "String.h"

// So, the idea for general containers, have some basic `void*` and size functionality,
// and some macros to init an inline casting function for each type.
// Like this:

typedef struct exampleElement {
    int a; int b;
} exampleElement;
exampleElement fakeData = exampleElement{ 5,15 };


bool IntKeyCompare(void* key_A, void* key_B) {
    auto A = *((int*)key_A);
    auto B = *((int*)key_B);
    return A == B;
}
unsigned int IntKeyHash(void* key) {
    auto A = *((unsigned int*)key);
    return A;
}

// Register type specifics
RegisterVectorStatics(Vec)
RegisterVectorFor(exampleElement, Vec)
RegisterVectorFor(HashMap_KVP, Vec)

RegisterHashMapStatics(Map)
RegisterHashMapFor(int, int, IntKeyHash, IntKeyCompare, Map)
RegisterHashMapFor(int, float, IntKeyHash, IntKeyCompare, Map)

RegisterTreeStatics(T)
RegisterTreeFor(exampleElement, T)

int TestHashMap() {
    std::cout << "*************** HASH MAP *****************\n";
    std::cout << "Allocating\n";
    // start small enough that we will go through a grow cycle when adding
    auto hmap = MapAllocate_int_int(64);
    std::cout << "HashMap OK? " << hmap.IsValid << "\n";

    std::cout << "Writing entries\n";
    for (int i = 0; i < 100; i++) {
        int key = i;
        int value = 2 * i;
        MapPut_int_int(&hmap, key, value, true);
    }

    std::cout << "Looking up data\n";
    int lukey = 40;
    int* lu_val_ptr = NULL;
    if (!MapGet_int_int(&hmap, lukey, &lu_val_ptr)) {
        std::cout << "Get failed!\n";
        return 1;
    }
    std::cout << "Found value " << *lu_val_ptr << " (expected 80)\n";

    auto has50 = MapContains_int_int(&hmap, 50);
    auto hasNeg1 = MapContains_int_int(&hmap, -1);
    std::cout << "Has 50? " << (has50 ? "yes" : "no") << "; Has -1? " << (hasNeg1 ? "yes" : "no") << "\n";

    MapRemove_int_int(&hmap, 50);
    has50 = MapContains_int_int(&hmap, 50);
    std::cout << "Has 50 after removal? " << (has50 ? "yes" : "no") << "\n";

    std::cout << "Count before clear = " << (MapCount(&hmap)) << "\n";
    MapClear(&hmap);
    std::cout << "Count after clear = " << (MapCount(&hmap)) << "\n";

    std::cout << "Deallocating map\n";
    MapDeallocate(&hmap);
    std::cout << "HashMap OK? " << hmap.IsValid << "\n";
    return 0;
}

int TestVector() {
    std::cout << "**************** VECTOR *******************\n";
    auto testElement = exampleElement{ 20,5 };

    // See if the container stuff works...
    std::cout << "Allocating\n";
    auto gvec = VecAllocate_exampleElement();
    std::cout << "Vector OK? " << VectorIsValid(gvec) << "\n";

    // add some entries
    std::cout << "Writing entries with 'push'\n";
    for (int i = 0; i < 1000; i++) {
        if (!VecPush_exampleElement(gvec, testElement)) {
            std::cout << "Push failed!\n";
            return 255;
        }
    }
    std::cout << "Vector OK? " << VectorIsValid(gvec) << "; Elements stored = " << VectorLength(gvec) << "\n";

    // read a random-access element
    auto r = VecGet_exampleElement(gvec, 5);
    std::cout << "Element 5 data = " << r->a << ", " << r->b << "\n";

    // resize the array
    VectorPrealloc(gvec, 5000);
    std::cout << "Vector OK? " << VectorIsValid(gvec) << "; Elements stored = " << VectorLength(gvec) << "\n";

    // Pop a load of values
    std::cout << "Reading and removing entries with 'pop'\n";
    for (int i = 0; i < 4000; i++) {
        exampleElement poppedData;
        if (!VecPop_exampleElement(gvec, &poppedData)) {
            std::cout << "Pop failed!\n";
            return 254;
        }
    }
    std::cout << "Vector OK? " << VectorIsValid(gvec) << "; Elements stored = " << VectorLength(gvec) << "\n";

    // Set a different element value
    auto newData = exampleElement{ 255,511 };
    exampleElement capturedOldData;
    VecSet_exampleElement(gvec, 70, &newData, &capturedOldData);
    std::cout << "Replace value at 70. Old data = " << capturedOldData.a << ", " << capturedOldData.b << "\n";
    r = VecGet_exampleElement(gvec, 70);
    std::cout << "Element 70 new data = " << r->a << ", " << r->b << "\n";

    // Swap elements by index pair
    std::cout << "Swapping\n";
    VectorSwap(gvec, 60, 70);
    r = VecGet_exampleElement(gvec, 60);
    std::cout << "Element 60 new data = " << r->a << ", " << r->b << "\n";
    r = VecGet_exampleElement(gvec, 70);
    std::cout << "Element 70 new data = " << r->a << ", " << r->b << "\n";

    std::cout << "Deallocating\n";
    VectorDeallocate(gvec);
    std::cout << "Vector OK? " << VectorIsValid(gvec) << "\n";
    return 0;
}

int TestTree() {
    std::cout << "**************** TREE *******************\n";

    std::cout << "Allocating\n";
    auto tree = TAllocate_exampleElement();

    std::cout << "Adding elements\n";
    auto elem1 = exampleElement{ 0,1 };
    TSetValue_exampleElement(TRoot(tree), &elem1);

    auto elem2 = exampleElement{ 1,2 };
    auto node2 = TAddChild_exampleElement(TRoot(tree), &elem2); // child of root

    auto elem3 = exampleElement{ 1,3 };
    auto node3 = TAddChild_exampleElement(TRoot(tree), &elem3); // child of root, sibling of node2

    auto elem4 = exampleElement{ 2,4 };
    auto node4 = TAddChild_exampleElement(node3, &elem4); // child of node3

    auto elem5 = exampleElement{ 2,5 };
    auto node5 = TAddSibling_exampleElement(node4, &elem5); // child of node3, sibling of node4


    std::cout << "Reading elements\n";
    // find elem5 the long way...
    auto find = TRoot(tree);
    find = TChild(find);
    find = TSibling(find);
    find = TChild(find);
    find = TSibling(find);
    auto found = TReadBody_exampleElement(find);
    std::cout << "Element 5 data (expecting 2,5) = " << found->a << ", " << found->b << "\n";

    std::cout << "Deallocating\n";
    TDeallocate(tree);
    return 0;
}

int TestString() {
    std::cout << "*************** MUTABLE STRING *****************\n";

    String* str1 = StringNew("Hello, ");
    String* str2 = StringNew("World");

    std::cout << "Hashes of original strings: " << StringHash(str1) << ", " << StringHash(str2) << "\n";
    std::cout << "String lengths before append: " << StringLength(str1) << ", " << StringLength(str2) << "\n";
    StringAppend(str1, str2);
    StringAppend(str1, "!");
    StringDeallocate(str2);
    std::cout << "String length after appends: " << StringLength(str1) << "\n";
    std::cout << "Hashes of result string: " << StringHash(str1) << "\n";

    auto cstr = StringToCStr(str1);
    std::cout << cstr << "\n";
    free(cstr);
    std::cout << "First char = '" << StringCharAtIndex(str1, 0) << "'\n";
    std::cout << "Last char = '" << StringCharAtIndex(str1, -1) << "'\n";

    StringToUpper(str1);
    cstr = StringToCStr(str1);
    std::cout << "Upper case: " << cstr << "\n";
    free(cstr);

    StringToLower(str1);
    cstr = StringToCStr(str1);
    std::cout << "Lower case: " << cstr << "\n";
    free(cstr);

    // Search
    str2 = StringNew("lo,");
    unsigned int pos;
    if (StringFind(str1, str2, 0, &pos)) {
        std::cout << "Found at " << pos << "\n";
    } else {
        std::cout << "Didn't find a string I was expecting!?\n";
    }
    if (StringFind(str1, str2, 4, NULL)) {
        std::cout << "Found a string I wasn't expecting\n";
    }
    StringDeallocate(str2);

    str2 = StringNew("l,o");
    if (StringFind(str1, str2, 0, &pos)) { // this should trigger the 'add' method to a false positive, but still return not found
        std::cout << "Found a string I wasn't expecting\n";
    }
    StringDeallocate(str2);

    // Slicing and chopping
    str2 = StringChop(StringSlice(str1, -2, 2), 0, 5); // last 2 chars = 'd!', then get 5 chars from that str = 'd!d!d'
    cstr = StringToCStr(str2);
    std::cout << cstr << "\n";
    free(cstr);

    // Comparison
    std::cout << (StringStartsWith(str1, "hello") ? "cmp 1 OK" : "cmp 1 failed") << "\n";
    std::cout << (StringStartsWith(str1, "fish") ? "cmp 2 failed" : "cmp 2 OK") << "\n";
    std::cout << (StringStartsWith(str1, str1) ? "cmp 3 OK" : "cmp 3 failed") << "\n";
    std::cout << (StringEndsWith(str1, "world!") ? "cmp 4 OK" : "cmp 4 failed") << "\n";
    std::cout << (StringEndsWith(str1, "fish") ? "cmp 5 failed" : "cmp 5 OK") << "\n";
    std::cout << (StringEndsWith(str1, str1) ? "cmp 6 OK" : "cmp 6 failed") << "\n";
    std::cout << (StringAreEqual(str1, "fish") ? "cmp 7 failed" : "cmp 7 OK") << "\n";
    std::cout << (StringAreEqual(str1, str2) ? "cmp 8 failed" : "cmp 8 OK") << "\n";
    std::cout << (StringAreEqual(str1, str1) ? "cmp 9 OK" : "cmp 9 failed") << "\n";

    StringDeallocate(str2);
    StringDeallocate(str1);
    return 0;
}

int main() {
    auto hmres = TestHashMap();
    if (hmres != 0) return hmres;

    auto vres = TestVector();
    if (vres != 0) return vres;

    auto tres = TestTree();
    if (tres != 0) return tres;

    auto sres = TestString();
    if (sres != 0) return sres;
}
