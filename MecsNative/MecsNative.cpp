#include <iostream>

// Containers:
#include "Vector.h"
#include "HashMap.h"
#include "Tree.h"
#include "String.h"
#include "Heap.h"

// Math:
#include "Fix16.h"

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
int CompareExampleElement(exampleElement *left, exampleElement *right) {
    return left->a - right->a;
}

// Register type specifics
RegisterVectorStatics(Vec)
RegisterVectorFor(exampleElement, Vec)
RegisterVectorFor(HashMap_KVP, Vec)
RegisterVectorFor(char, Vec)

RegisterHashMapStatics(Map)
RegisterHashMapFor(int, int, IntKeyHash, IntKeyCompare, Map)
RegisterHashMapFor(int, float, IntKeyHash, IntKeyCompare, Map)

RegisterTreeStatics(T)
RegisterTreeFor(exampleElement, T)

void WriteStr(String *str) {
    auto cstr = StringToCStr(str);
    std::cout << cstr << "\n";
    free(cstr);
}

int TestHashMap() {
    std::cout << "*************** HASH MAP *****************\n";
    std::cout << "Allocating\n";
    // start small enough that we will go through a grow cycle when adding
    auto hmap = MapAllocate_int_int(64);

    std::cout << "Writing entries\n";
    for (int i = 0; i < 100; i++) {
        int key = i;
        int value = 2 * i;
        MapPut_int_int(hmap, key, value, true);
    }

    std::cout << "Looking up data\n";
    int lukey = 40;
    int* lu_val_ptr = NULL;
    if (!MapGet_int_int(hmap, lukey, &lu_val_ptr)) {
        std::cout << "Get failed!\n";
        return 1;
    }
    std::cout << "Found value " << *lu_val_ptr << " (expected 80)\n";

    auto has50 = MapContains_int_int(hmap, 50);
    auto hasNeg1 = MapContains_int_int(hmap, -1);
    std::cout << "Has 50? " << (has50 ? "yes" : "no") << "; Has -1? " << (hasNeg1 ? "yes" : "no") << "\n";

    MapRemove_int_int(hmap, 50);
    has50 = MapContains_int_int(hmap, 50);
    std::cout << "Has 50 after removal? " << (has50 ? "yes" : "no") << "\n";

    std::cout << "Count before clear = " << (MapCount(hmap)) << "\n";
    MapClear(hmap);
    std::cout << "Count after clear = " << (MapCount(hmap)) << "\n";

    std::cout << "Deallocating map\n";
    MapDeallocate(hmap);
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
    std::cout << "Element 70 new data = " << r->a << ", " << r->b << " (should be 255,511)\n";

    // Swap elements by index pair
    std::cout << "Swapping 60 and 70\n";
    VectorSwap(gvec, 60, 70);
    r = VecGet_exampleElement(gvec, 60);
    std::cout << "Element 60 new data = " << r->a << ", " << r->b << " (255,511)\n";
    r = VecGet_exampleElement(gvec, 70);
    std::cout << "Element 70 new data = " << r->a << ", " << r->b << " (20,5)\n";

    std::cout << "Deallocating\n";
    VectorDeallocate(gvec);
    std::cout << "Vector OK? " << VectorIsValid(gvec) << "\n";

    // Test sorting
    gvec = VecAllocate_exampleElement();
    int sal = 70;
    for (int i = 0; i < sal; i++) {
        VecPush_exampleElement(gvec, exampleElement{ ((i * 6543127) % sal) - 10, i }); // a very crappy random
    }
    std::cout << "Before sort:\n";
    for (int i = 0; i < sal; i++) {
        std::cout << VecGet_exampleElement(gvec, i)->a << ", ";
    }
    std::cout << "\n";

    std::cout << "After sort:\n";
    VecSort_exampleElement(gvec, CompareExampleElement); // sort into ascending order (reverse the compare to reverse the order)
    for (int i = 0; i < sal; i++) {
        std::cout << VecGet_exampleElement(gvec, i)->a << ", ";
    }
    std::cout << "\n";
    VectorDeallocate(gvec);


    return 0;
}

int TestQueue() {
    std::cout << "**************** QUEUE (VECTOR) *******************\n";
    // queues are a feature of vectors (makes things a bit complex)

    auto q = VecAllocate_char();
    auto str = "This is a string of chars to fill our vector. It has to be pretty long to cross chunk boundaries. Neque porro quisquam est qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit.";
    // fill up vector
    auto ptr = str;
    while (*ptr != 0) { VecPush_char(q, *ptr++); }

    // empty it back into std-out
    char c;
    while (VecDequeue_char(q, &c)) { std::cout << c; }
    std::cout << "\n";

    // Fill up again and empty from both ends...
    auto pal = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzzyxwvutsrqponmlkjihgfedcba9876543210ZYXWVUTSRQPONMLKJIHGFEDCBA";
    ptr = pal;
    while (*ptr != 0) { VecPush_char(q, *ptr++); }
    while (VecLength(q) > 0) {
        VecDequeue_char(q, &c); std::cout << c;
        VecPop_char(q, &c); std::cout << c;
    }
    std::cout << "\n";

    // Fill up again and empty from both ends, but using indexes
    ptr = pal;
    while (*ptr != 0) { VecPush_char(q, *ptr++); }
    while (VecLength(q) > 0) {
        VecCopy_char(q, 0, &c); std::cout << c;
        VecCopy_char(q, VecLength(q) - 1, &c); std::cout << c;
        VecDequeue_char(q, &c);
        VecPop_char(q, &c);
    }
    std::cout << "\n";

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

    // number strings
    str1 = StringEmpty();
    StringAppendInt32(str1, 1234);
    StringAppend(str1, ", ");
    StringAppendInt32(str1, -4567);
    StringAppend(str1, ", ");
    StringAppendInt32(str1, 0);
    StringAppend(str1, ", ");
    StringAppendInt32(str1, 2147483647);
    StringAppend(str1, ", ");
    StringAppendInt32Hex(str1, 0x0123ABCD);

    cstr = StringToCStr(str1);
    std::cout << cstr << "\n";
    std::cout << "1234, -4567, 0, 2147483647, 0123ABCD\n";
    free(cstr);
    StringDeallocate(str1);
    return 0;
}

int TestFixedPoint() {
    std::cout << "*************** FIXED POINT *****************\n";

    // Test basic operations against constant
    fix16_t fexpected = FOUR_DIV_PI;
    fix16_t fpi = fix16_pi;
    fix16_t f4 = fix16_from_int(4);
    fix16_t fresult = fix16_div(f4, fpi);

    fix16_t diff = fix16_abs(fix16_sub(fresult, fexpected));

    auto str1 = StringNew("Raw difference: ");
    StringAppendInt32Hex(str1, diff);
    WriteStr(str1);

    // Test constants and conversions with string generation
    StringClear(str1);
    StringAppend(str1, "Pi: ");
    StringAppendF16(str1, fix16_pi);
    StringAppend(str1, ", e: ");
    StringAppendF16(str1, fix16_e);
    StringAppend(str1, ", 1.0: ");
    StringAppendF16(str1, fix16_one);
    StringAppend(str1, ", FIX16.16 maximum: ");
    StringAppendF16(str1, fix16_maximum);
    WriteStr(str1);

    StringClear(str1);
    StringAppend(str1, "1.03: ");
    StringAppendF16(str1, fix16_from_float(1.03));
    StringAppend(str1, ", 100.001: ");
    StringAppendF16(str1, fix16_from_float(100.001));
    StringAppend(str1, ", 0.9999: ");
    StringAppendF16(str1, fix16_from_float(0.9999));

    WriteStr(str1);

    // Test of saturating math
    StringClear(str1);
    auto big = fix16_from_float(30000.1234);
    auto bigger = fix16_sadd(big, big);
    StringAppend(str1, "Sat add: ");
    StringAppendF16(str1, bigger);
    StringAppend(str1, " (expecting max F16 ~ 32767.9999)");
    WriteStr(str1);


    StringDeallocate(str1);


    return 0;
}

int TestHeaps() {
    std::cout << "*************** BINARY HEAP (Priority Queue) *****************\n";
    auto str1 = StringEmpty();
    auto heap = HeapAllocate(sizeof(char));
    // add out-of-order, but with alphabetical priority
    HeapInsert(heap, 1, (void*)"A");
    HeapInsert(heap, 6, (void*)"F");
    HeapInsert(heap, 7, (void*)"G");
    HeapInsert(heap, 5, (void*)"E");
    HeapInsert(heap, 2, (void*)"B");
    HeapInsert(heap, 4, (void*)"D");
    HeapInsert(heap, 3, (void*)"C");

    // Now loop until empty in order
    char c;
    while (!HeapIsEmpty(heap)) {
        HeapDeleteMin(heap, &c);
        StringAppendChar(str1, c);
    }
    WriteStr(str1);
    StringDeallocate(str1);

    return 0;
}

int main() {
    auto hmres = TestHashMap();
    if (hmres != 0) return hmres;

    auto vres = TestVector();
    if (vres != 0) return vres;

    auto qres = TestQueue();
    if (qres != 0) return qres;

    auto tres = TestTree();
    if (tres != 0) return tres;

    auto sres = TestString();
    if (sres != 0) return sres;

    auto fpres = TestFixedPoint();
    if (fpres != 0) return fpres;

    auto hres = TestHeaps();
    if (hres != 0) return hres;
}
