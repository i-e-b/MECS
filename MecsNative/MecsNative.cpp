#include <iostream>
#include <stdio.h>

// Containers:
#include "Vector.h"
#include "HashMap.h"
#include "Tree.h"
#include "String.h"
#include "Heap.h"
#include "ArenaAllocator.h"
#include "MemoryManager.h"

// Encoding:
#include "TagData.h"
#include "TagCodeReader.h"
#include "Serialisation.h"

// System abstractions
#include "FileSys.h"
#include "TimingSys.h"

// The MECS compiler and runtime
#include "SourceCodeTokeniser.h"
#include "CompilerCore.h"
#include "TagCodeInterpreter.h"
#include "TypeCoersion.h"


typedef struct exampleElement {
    int a; int b;
} exampleElement;


int CompareExampleElement(exampleElement *left, exampleElement *right) {
    return left->a - right->a;
}

// Register type specifics
RegisterVectorStatics(Vec)
RegisterVectorFor(exampleElement, Vec)
RegisterVectorFor(HashMap_KVP, Vec)
RegisterVectorFor(char, Vec)
RegisterVectorFor(StringPtr, Vec)
RegisterVectorFor(DataTag, Vec)

RegisterHashMapStatics(Map)
RegisterHashMapFor(int, int, HashMapIntKeyHash, HashMapIntKeyCompare, Map)
RegisterHashMapFor(int, float, HashMapIntKeyHash, HashMapIntKeyCompare, Map)

RegisterTreeStatics(T)
RegisterTreeFor(exampleElement, T)

RegisterHeapStatics(H)
RegisterHeapFor(char, H)

void WriteStr(String *str) {
    auto cstr = StringToCStr(str);
    std::cout << cstr << "\n";
    mfree(cstr);
}
void WriteStrInline(String *str) {
    auto cstr = StringToCStr(str);
    std::cout << cstr;
    mfree(cstr);
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

    auto has50 = MapGet_int_int(hmap, 50, NULL);
    auto hasNeg1 = MapGet_int_int(hmap, -1, NULL);
    std::cout << "Has 50? " << (has50 ? "yes" : "no") << "; Has -1? " << (hasNeg1 ? "yes" : "no") << "\n";

    MapRemove_int_int(hmap, 50);
    has50 = MapGet_int_int(hmap, 50, NULL);
    std::cout << "Has 50 after removal? " << (has50 ? "yes" : "no") << "\n";

    std::cout << "Count before clear = " << (MapCount(hmap)) << "\n";
    MapClear(hmap);
    std::cout << "Count after clear = " << (MapCount(hmap)) << "\n";


    // Check we can replace one entry an unlimited number of times:
    /*std::cout << "Row hammering...";
    for (int i = 0; i < 1000000; i++) {
        int key = 20;
        int value = 20;
        if(!MapPut_int_int(hmap, key, value, true)) {
            std::cout << "FAILED at " << i << "\n";
            break;
        }
    }
    std::cout << "done\n";*/


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
    VecSet_exampleElement(gvec, 70, newData, &capturedOldData);
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



    // Check that vectors pin to the arena they were created in:
    size_t beforeOuter, afterInner, afterOuter, finalOuter;
    ArenaGetState(MMCurrent(), &beforeOuter, NULL, NULL, NULL, NULL, NULL);
    auto pinv = VecAllocate_char();
    if (!MMPush(256 KILOBYTES)) {
        std::cout << "Arena allcation failed";
        return 253;
    }
    for (int i = 0; i < (100 KILOBYTES); i++) {
        VecPush_char(pinv, (char)((i % 100) + 33));
    }
    ArenaGetState(MMCurrent(), &afterInner, NULL, NULL, NULL, NULL, NULL);

    // delete the arena, check we can still access data
    MMPop();
    char c;
    for (int i = 0; i < (100 KILOBYTES); i++) {
        VecPop_char(pinv, &c);
    }

    int refs = 0;
    ArenaGetState(MMCurrent(), &afterOuter, NULL, NULL, NULL, &refs, NULL);
    VecDeallocate(pinv);
    ArenaGetState(MMCurrent(), &finalOuter, NULL, NULL, NULL, NULL, NULL);

    std::cout << "Memory arena pinning: Outer before=" << beforeOuter << "; after=" << afterOuter << "; final=" << finalOuter << "\n";
    std::cout << "                      Inner after=" << afterInner << " (should be zero)\n";
    std::cout << "                      References after pops=" << refs << "\n";

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
    TSetValue_exampleElement(tree, &elem1);

    auto elem2 = exampleElement{ 1,2 };
    auto node2 = TAddChild_exampleElement(tree, &elem2); // child of root

    auto elem3 = exampleElement{ 1,3 };
    auto node3 = TAddChild_exampleElement(tree, &elem3); // child of root, sibling of node2

    auto elem4 = exampleElement{ 2,4 };
    auto node4 = TAddChild_exampleElement(node3, &elem4); // child of node3

    auto elem5 = exampleElement{ 2,5 };
    auto node5 = TAddSibling_exampleElement(node4, &elem5); // child of node3, sibling of node4


    std::cout << "Reading elements\n";
    // find elem5 the long way...
    auto find = tree;
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
    mfree(cstr);
    std::cout << "First char = '" << StringCharAtIndex(str1, 0) << "'\n";
    std::cout << "Last char = '" << StringCharAtIndex(str1, -1) << "'\n";

    StringToUpper(str1);
    std::cout << "Upper case: ";
    WriteStr(str1);

    StringToLower(str1);
    std::cout << "Lower case: ";
    WriteStr(str1);

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
    WriteStr(str2);

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

    StringDeallocate(str1);

    // number strings
    str1 = StringEmpty();
    StringAppendInt32(str1, 1000);
    StringAppend(str1, ", ");
    StringAppendInt32(str1, 1234);
    StringAppend(str1, ", ");
    StringAppendInt32(str1, -4567);
    StringAppend(str1, ", ");
    StringAppendInt32(str1, 0);
    StringAppend(str1, ", ");
    StringAppendInt32(str1, 2147483647);
    StringAppend(str1, ", ");
    StringAppendInt32Hex(str1, 0x0123ABCD);

    WriteStr(str1);
    std::cout << "1000, 1234, -4567, 0, 2147483647, 0123ABCD\n";

    // Parsing strings
    // INTEGERS
    StringClear(str1);
    StringAppend(str1, "1000");
    int32_t int32res = 0;
    bool ok = StringTryParse_int32(str1, &int32res);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendInt32(str1, int32res);
    WriteStr(str1);

    StringClear(str1);
    StringAppend(str1, "0001234000");
    ok = StringTryParse_int32(str1, &int32res);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendInt32(str1, int32res);
    WriteStr(str1);

    StringClear(str1);
    StringAppend(str1, "-123");
    ok = StringTryParse_int32(str1, &int32res);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendInt32(str1, int32res);
    WriteStr(str1);

    // Parsing strings
    // FLOATING POINT
    StringClear(str1);
    StringAppend(str1, "-110.001");
    double fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    WriteStr(str1);

    StringClear(str1);
    StringAppend(str1, "110.01");
    fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    WriteStr(str1);

    StringClear(str1);
    StringAppend(str1, "-110");
    fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    WriteStr(str1);

    StringClear(str1);
    StringAppend(str1, "3000.0123");
    fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    WriteStr(str1);

    // String formatting
    StringClear(str1);
    StringClear(str2);
    StringAppend(str2, "'This is from a (String*)'");
    // Simple append
    StringAppendFormat(str1, "Formatted string, with literal (this), included mutable strings: \x01, decimal: \x02, and hex: \x03.", str2, 1234, 1234);
    WriteStr(str1);
    // Create from format
    StringDeallocate(str2);
    str2 = StringNewFormat("String inception: \"\x01\"", str1);
    WriteStr(str2);

    StringDeallocate(str1);
    StringDeallocate(str2);

    // Test search-and-replace:
    str1 = StringNew("This is a line in the sand, and will stand as a pillar of our hopes and dreams.");
    auto needle = StringNew("and");
    auto replacement = StringNew("but also");
    str2 = StringReplace(str1, needle, replacement);

    WriteStr(str1);
    WriteStr(str2); // This is a line in the sbut also, but also will stbut also as a pillar of our hopes but also dreams.

    StringDeallocate(str1);
    StringDeallocate(str2);
    StringDeallocate(needle);
    StringDeallocate(replacement);

    return 0;
}

int TestHeaps() {
    std::cout << "*************** BINARY HEAP (Priority Queue) *****************\n";
    auto str1 = StringEmpty();
    auto str2 = StringEmpty();
    auto str3 = StringEmpty();
    auto heap = HAllocate_char();
    // add out-of-order, but with alphabetical priority
    HInsert_char(heap, 1, "A");
    HInsert_char(heap, 6, "F");
    HInsert_char(heap, 7, "G");
    HInsert_char(heap, 5, "E");
    HInsert_char(heap, 2, "B");
    HInsert_char(heap, 4, "D");
    HInsert_char(heap, 3, "C");

    // Now loop until empty in order
    char c;
    while (!HIsEmpty(heap)) {
        if (HTryFindNext_char(heap, &c)) { // find the 2nd lowest priority
            StringAppendChar(str2, c);
        }

        StringAppendChar(str3, *HPeekMin_char(heap));

        HDeleteMin_char(heap, &c);
        StringAppendChar(str1, c);
    }
    StringAppend(str1, " (expected ABCDEFG)");
    StringAppend(str2, " (expected BCDEFG)");
    StringAppend(str3, " (expected ABCDEFG)");
    WriteStr(str1);
    WriteStr(str2);
    WriteStr(str3);
    StringDeallocate(str1);
    StringDeallocate(str2);
    StringDeallocate(str3);
    
    HDeallocate(heap);

    return 0;
}

int TestTagData() {
    std::cout << "***************** TAG DATA ******************\n";

    auto tag = DataTag { (int)DataType::VectorPtr, ALLOCATED_TYPE + 2, 0x82 }; // these should be the same

    auto str = StringEmpty();
    StringAppend(str, "Tag: ");
    StringAppendInt32(str, tag.type);
    StringAppend(str, ", Params: ");
    StringAppendInt32(str, tag.params);
    StringAppend(str, ", Data: ");
    StringAppendInt32(str, tag.data);
    WriteStr(str);
    StringClear(str);

    tag = EncodeOpcode('x', 'a', 1, 1);

    char p1, p2;
    uint8_t p3;
    uint32_t param;
    DecodeLongOpcode(tag, &p1, &p2, &param, &p3);

    if (p1 == 'x' && p2 == 'a' && param == 0x00010001 && p3 == 0) { StringAppend(str, "OpCodes OK;"); }
    else { StringAppend(str, "OPCODES FAILED;"); }
    WriteStr(str);

    StringClear(str);
    StringAppend(str, "ShrtStr");
    tag = EncodeShortStr(str);
    StringAppend(str, "decoded: '");
    DecodeShortStr(tag, str);
    StringAppend(str, "' (expected 'ShrtStr')");
    WriteStr(str);

    StringClear(str);
    tag = EncodeShortStr("Hello!");
    StringAppend(str, "decoded: '");
    DecodeShortStr(tag, str);
    StringAppend(str, "' (expected 'Hello!')");
    WriteStr(str);

    double origd = 123450.098765;
    StringClear(str);
    StringAppendDouble(str, origd);
    tag = EncodeDouble(origd);
    StringAppend(str, "; decoded: '");
    double resd = DecodeDouble(tag);
    StringAppendDouble(str, resd);
    StringAppend(str, "' (expected approx 123450.098765)");
    WriteStr(str);


    StringClear(str);
    DescribeTag(EncodeOpcode('j', 'F', 1, 1), str, NULL);
    StringNL(str);
    DescribeTag(RuntimeError(0xDEAD), str, NULL);
    StringNL(str);
    DescribeTag(EncodeVariableRef(str, NULL), str, NULL);
    StringNL(str);
    WriteStr(str);


    StringDeallocate(str);

    return 0;
}

int TestFileSystem() {
    std::cout << "***************** FILE SYS ******************\n";
    auto vec = VecAllocate_char();

    auto path = StringNew("Test.txt");
    uint64_t read = 0;
    bool ok = FileLoadChunk(path, vec, 0, 1000, &read);

    std::cout << "Existing file read OK = " << ok << "; Bytes read: " << read << "\n";
    std::cout << "File contents:\n";
    char c = 0;
    while (VecDequeue_char(vec, &c)) {
        std::cout << c;
    }
    std::cout << "\n";

    for (int i = 32; i < 255; i++) {
        VecPush_char(vec, i);
    }
    StringClear(path);
    StringAppend(path, "output.txt");
    ok = FileWriteAll(path, vec);
    std::cout << "Trunc & Write OK = " << ok << "; Bytes not written: " << VectorLength(vec) << "\n";

    for (int i = 32; i < 255; i++) {
        VecPush_char(vec, i);
    }
    ok = FileAppendAll(path, vec);
    std::cout << " append Write OK = " << ok << "; Bytes not written: " << VectorLength(vec) << "\n";

    ok = FileLoadChunk(path, vec, 0, 1000, &read);
    std::cout << "Read OK = " << ok << "; Bytes read: " << read << "\n";
    std::cout << "File contents:\n";
    c = 0;
    while (VecDequeue_char(vec, &c)) {
        std::cout << c;
    }
    std::cout << "\n";

    StringDeallocate(path);
    VectorDeallocate(vec);

    return 0;
}

int TestArenaAllocator() {
    std::cout << "***************** ARENA ALLOCATOR ******************\n";
    size_t allocatedBytes;
    size_t unallocatedBytes;
    int occupiedZones;
    int emptyZones;
    int totalReferenceCount;
    size_t largestContiguous;


    auto arena1 = NewArena(10 MEGABYTES);

    ArenaGetState(arena1, &allocatedBytes, &unallocatedBytes, &occupiedZones, &emptyZones, &totalReferenceCount, &largestContiguous);
    std::cout << "Empty 10MB Arena: alloc=" << allocatedBytes << "; free=" << unallocatedBytes << "; frgs used=" << occupiedZones << "; frgs empty=" << emptyZones << "; refs=" << totalReferenceCount << "; max chunk=" << largestContiguous << "\n";

    // allocate some bits
    for (int i = 0; i < 100; i++) {
        auto ptr = ArenaAllocate(arena1, ARENA_ZONE_SIZE / 5);
        if (ptr == NULL) {
            std::cout << "Failed to allocate at " << i << "\n"; break;
        }
    }
    // and happily ignore them all, as long as we keep the arena reference

    ArenaGetState(arena1, &allocatedBytes, &unallocatedBytes, &occupiedZones, &emptyZones, &totalReferenceCount, &largestContiguous);
    std::cout << " Used 10MB Arena: alloc=" << allocatedBytes << "; free=" << unallocatedBytes << "; frgs used=" << occupiedZones << "; frgs empty=" << emptyZones << "; refs=" << totalReferenceCount << "; max chunk=" << largestContiguous << "\n";

    DropArena(&arena1);
    std::cout << "Arena was killed: " << (arena1 == NULL ? "yes" : "no") << "\n";
    
    // Test referencing and de-referencing
    auto arena2 = NewArena(256 KILOBYTES);

    ArenaGetState(arena2, &allocatedBytes, NULL, &occupiedZones, NULL, &totalReferenceCount, NULL);
    std::cout << "Empty Arena: alloc=" << allocatedBytes << "; frgs used=" << occupiedZones << "; refs=" << totalReferenceCount << "\n";
    void* bits[20];
    for (int i = 0; i < 20; i++) { bits[i] = ArenaAllocate(arena2, 256); }
    ArenaGetState(arena2, &allocatedBytes, NULL, &occupiedZones, NULL, &totalReferenceCount, NULL);
    std::cout << "Used Arena: alloc=" << allocatedBytes << "; frgs used=" << occupiedZones << "; refs=" << totalReferenceCount << "\n";

    for (int i = 1; i < 20; i+=2) { ArenaReference(arena2, bits[i]); }
    for (int i = 0; i < 20; i+=2) { ArenaDereference(arena2, bits[i]); }
    for (int i = 0; i < 20; i++) { // this will double-dereference some bits, which should be ignored
        ArenaDereference(arena2, bits[i]);
        ArenaDereference(arena2, bits[i]);
    }

    ArenaGetState(arena2, &allocatedBytes, NULL, &occupiedZones, NULL, &totalReferenceCount, NULL);
    std::cout << "Used and dereferenced Arena: alloc=" << allocatedBytes << "; frgs used=" << occupiedZones << "; refs=" << totalReferenceCount << "\n";
    DropArena(&arena2);

    // TODO: expand this when String/Vector/Hashtable etc can use the arena allocator

    return 0;
}

int TestSerialisation() {
    std::cout << "***************** SERIALISATION ******************\n";
    
    // Compile some code and load it into an interpreter.
    auto tagCode = VecAllocate_DataTag();
    
    // sample outputs for below
    //auto code = StringNew("return('Hello, world!')"); // pointer to static string
    //auto code = StringNew("return(new-list(1 2 3 4 'Can I have a little more?' 5 6 7 8 9))"); // list of numbers
    auto code = StringNew("return(new-map('a' 1, 'b' new-list(1 2 'x'), 'c' 'Hello, world!', 'd' 2))"); // map with heterogeneous list


    auto compilableSyntaxTree = ParseSourceCode(code, false); // Compiler doesn't like metadata!
    auto compiled = CompileRoot(compilableSyntaxTree, false, false);
    TCW_AppendToVector(compiled, tagCode);

    auto interp = InterpAllocate(tagCode, 1 MEGABYTE, NULL);
    auto arena = InterpInternalMemory(interp);

    // a new vector to write into. This is inside the interpreter's memory.
    auto vec = VecAllocateArena_char(arena);

    //////////////////////////////////////////////////
    // A simple example of self-contained data
    DataTag source = EncodeShortStr("Hello"); //EncodeInt32(123645); //EncodeDouble(2.718281828459045);
    auto ok = FreezeToVector(source, /*InterpreterState*/interp, /*Vector<byte>*/vec);

    if (!ok) {
        std::cout << "Serialisation failed\n";
        return -1;
    }

    std::cout << "Result bytes = " << std::hex;
    int length = VecLength(vec);
    for (int i = 0; i < length; i++) {
        std::cout << (int)((uint8_t)*VecGet_char(vec, i)) << " ";
    }
    std::cout << std::dec << "\nSerialisation OK, trying deserialisation...\n";

    // Try restore
    
    auto targetInterp = InterpAllocate(tagCode, 1 MEGABYTE, NULL);
    auto targetArena = InterpInternalMemory(targetInterp);
    DataTag dest = {};
    ok = DefrostFromVector(&dest, targetArena, vec);
    if (!ok) {
        std::cout << "Deserialisation failed\n";
        return -2;
    }

    // Show final result
    auto human = CastString(targetInterp, dest);
    WriteStr(human);
    StringDeallocate(human);

    //////////////////////////////////////////////////
    // More complex stuff.
    // We use a short program to help

    // Run the program:
    auto result = InterpRun(interp, 5000);
    if (result.State != ExecutionState::Complete) {
        std::cout << "Test program did not complete:\n\n";
        auto outp = StringEmpty();
        ReadOutput(interp, outp);
        WriteStrInline(outp);
        StringDeallocate(outp);
        return -3;
    }

    // Show what we are expecting:
    human = CastString(interp, result.Result);
    std::cout << "Serialiser input = "; 
    WriteStr(human);
    StringDeallocate(human);

    // the output should be loaded into `result.Result`
    // serialise it...
    ok = FreezeToVector(result.Result, /*InterpreterState*/interp, /*Vector<byte>*/vec);
    if (!ok) { std::cout << "Serialisation failed\n"; return -1; }

    // display serialised data
    std::cout << "Result bytes = " << std::hex;
    length = VecLength(vec);
    for (int i = 0; i < length; i++) { std::cout << (int)((uint8_t)*VecGet_char(vec, i)) << " "; }
    std::cout << std::dec << "\nSerialisation OK.\n";

    // deserialise
    ok = DefrostFromVector(&dest, targetArena, vec);
    if (!ok) { std::cout << "Deserialisation failed\n"; return -2; }

    // Show final result
    human = CastString(targetInterp, dest);
    std::cout << "Deserialisation result = "; 
    WriteStr(human);
    StringDeallocate(human);

    // Clean up interpreters and their arenas
    InterpDeallocate(interp);
    InterpDeallocate(targetInterp);

    return 0;
}

int TestCompiler() {
    std::cout << "***************** COMPILER ******************\n";

    auto code = StringEmpty();
    auto pathOfInvalid = StringNew("Test.txt"); // not valid source
    //auto pathOfValid = StringNew("demo_program.ecs"); // should be valid source
    auto pathOfValid = StringNew("msg_sendMessage.ecs"); // should be valid source

    auto vec = StringGetByteVector(code);
    uint64_t read = 0;
    WriteStr(pathOfInvalid);
    if (!FileLoadChunk(pathOfInvalid, vec, 0, 10000, &read)) {
        std::cout << "Failed to read file. Test inconclusive.\n";
        return -1;
    }
    
    std::cout << "Reading a non-source code file: ";
    auto syntaxTree = ParseSourceCode(code, false);
    StringClear(code); // also clears the underlying vector
    if (syntaxTree == NULL) { std::cout << "Parser failed entirely"; return -2; }

    SourceNode *result = (SourceNode*)TreeReadBody(syntaxTree);

    if (result->IsValid) {
        std::cout << "The source file was parsed correctly!? It should not have been!\n";
        return -3;
    }

    std::cout << "The source file was not valid (this is ok)\n";

    StringDeallocate(pathOfInvalid);
    DeallocateAST(syntaxTree);

    //###################################################################
    std::cout << "Reading a valid source code file: ";
    WriteStr(pathOfValid);
    if (!FileLoadChunk(pathOfValid, vec, 0, 10000, &read)) {
        std::cout << "Failed to read file. Test inconclusive.\n";
        return -4;
    }
    auto compilableSyntaxTree = ParseSourceCode(code, false); // Compiler doesn't like metadata!
    syntaxTree = ParseSourceCode(code, true);
    StringDeallocate(code);
    if (syntaxTree == NULL) { std::cout << "Parser failed entirely"; return -5; }

    result = (SourceNode*)TreeReadBody(syntaxTree);

    if (!result->IsValid) {
        std::cout << "The source file was not valid (FAIL!)\n";
    } else {
        std::cout << "The source file was parsed correctly:\n\n";
    }

    auto nstr = RenderAstToSource(syntaxTree); // render it even if bad -- as it contains error details
    if (nstr == NULL) {
        std::cout << "Failed to render AST\n";
        return -8;
    }
    WriteStr(nstr);

    std::cout << "Attempting to compile:\n";
    auto tagCode = CompileRoot(compilableSyntaxTree, false, false);

    if (TCW_HasErrors(tagCode)) {
        std::cout << "COMPILE FAILED!\n";
        auto errs = TCW_ErrorList(tagCode);
        String* msg = NULL;
        while (VecPop_StringPtr(errs, &msg)) {
            WriteStr(msg);
        }
        return -1;
    } else {
        std::cout << "Compile OK\n";
        std::cout << "Listing tag-code (excluding strings)\n\n";
        
        int opCount = TCW_OpCodeCount(tagCode);
        auto tagStr = StringEmpty();
        for (int i = 0; i < opCount; i++) {
            DataTag opcode = TCW_OpCodeAtIndex(tagCode, i);
            DescribeTag(opcode, tagStr, TCW_GetSymbols(tagCode));
            StringAppendChar(tagStr, '\n');
        }
        WriteStr(tagStr);
        StringDeallocate(tagStr);
        
        std::cout << "\n\nWriting to code file...";
        auto buf = TCW_WriteToStream(tagCode);
        std::cout << VecLength(buf) << " bytes...";
        FileWriteAll(StringNew("tagcode.dat"), buf);

        std::cout << "\n\nWriting to symbols file...";
        TCW_WriteSymbolsToStream(tagCode, buf);
        std::cout << VecLength(buf) << " bytes...";
        FileWriteAll(StringNew("tagsymb.dat"), buf);
        std::cout << "Done\n";

        VecDeallocate(buf);
    }

    StringDeallocate(nstr);
    StringDeallocate(pathOfValid);
    DeallocateAST(syntaxTree);
    DeallocateAST(compilableSyntaxTree);

    auto arena = MMCurrent();
    size_t alloc;
    size_t unalloc;
    int objects;
    ArenaGetState(arena, &alloc, &unalloc, NULL, NULL, &objects, NULL);

    // The compiler currently leaks like a sieve, and relies heavily on arenas to clean up.
    std::cout << "Compiling leaked " << alloc << " bytes across " << objects << " objects. " << unalloc << " free.\n";

    return 0;
}

int TestRuntimeExec() {
    // This relies on the 'tagcode.dat' file created in the test compiler step
    std::cout << "***************** RUNTIME ******************\n";

    // Read code file --------------------------------
    auto tagCode = VecAllocate_DataTag();
    auto path = StringNew("tagcode.dat");
    uint64_t actual;
    bool ok = FileLoadChunk(path, tagCode, 0, FILE_LOAD_ALL, &actual);
    if (!ok || actual < 10) { std::cout << "Failed to read tagcode file\n"; return -1; }

    std::cout << "Read file OK. Loaded " << VecLength(tagCode) << " elements\n";

    uint32_t startOfCode, startOfMem;
    if (!TCR_Read(tagCode, &startOfCode, &startOfMem)) {
        std::cout << "Failed to read incoming byte code\n";
        return -1;
    }

    // Read symbols file --------------------------------
    std::cout << "Trying to read symbol file\n";
    StringClear(path);
    StringAppend(path, "tagsymb.dat");
    HashMap* symbolMap = NULL;
    auto rawSymb = VecAllocate_char();

    ok = FileLoadChunk(path, rawSymb, 0, FILE_LOAD_ALL, &actual);
    if (!ok || actual < 10) { std::cout << "Failed to read symbol file (ignoring)\n"; }
    else { symbolMap = TCR_ReadSymbols(rawSymb); }
    VecDeallocate(rawSymb);

    // Write a summary of the program to be executed ----------
    auto str = TCR_Describe(tagCode, symbolMap);
    WriteStr(str);
    StringDeallocate(str);
    StringDeallocate(path);
    
    // Prepare a runtime task ------------------------------------
    auto interp = InterpAllocate(tagCode, 1 MEGABYTE, symbolMap);

    // run a few cycles and print any output
    std::cout << "Executing...\n";
    auto startTime = SystemTime();
    auto result = InterpRun(interp, 5000);
    while (result.State == ExecutionState::Paused) {
        str = StringEmpty();
        ReadOutput(interp, str);
        WriteStrInline(str);
        StringDeallocate(str);
        result = InterpRun(interp, 5000);
    }

    // Write a status report ------------------------------
    auto endTime = SystemTime();
    str = StringEmpty();
    ReadOutput(interp, str);
	int resultState = 0;
    switch (result.State) {
    case ExecutionState::Complete:
        StringAppend(str, "\r\nProgram Complete");
        break;
    case ExecutionState::Paused:
        StringAppend(str, "\r\nProgram paused without finishing");
        break;
    case ExecutionState::Waiting:
        StringAppend(str, "\r\nProgram waiting for input");
        break;
    case ExecutionState::ErrorState:
        StringAppend(str, "\r\nProgram ERRORED: ");
        DescribeTag(result.Result, str, symbolMap);
		resultState = 1;
        break;
    case ExecutionState::Running:
        StringAppend(str, "\r\nProgram still running?");
		resultState = 2;
        break;
    }
    WriteStr(str);
    StringDeallocate(str);

    std::cout << "Execution took " << (endTime - startTime) << " seconds\n";
    size_t alloc;
    size_t unalloc;
    int objects;

    // Check memory state
    ArenaGetState(InterpInternalMemory(interp), &alloc, &unalloc, NULL, NULL, &objects, NULL);
    std::cout << "Runtime used in internal memory: " << alloc << " bytes across " << objects << " objects. " << unalloc << " free.\n";

    // clean up
    InterpDeallocate(interp);
    MapDeallocate(symbolMap);
    VecDeallocate(tagCode);
    auto arena = MMCurrent();
    ArenaGetState(arena, &alloc, &unalloc, NULL, NULL, &objects, NULL);
    std::cout << "Runtime used in external memory: " << alloc << " bytes across " << objects << " objects. " << unalloc << " free.\n";


    return resultState;
}



int AppendFinishState (ExecutionState state, String* str) {
	int errState = 0;
    switch (state) {
    case ExecutionState::Complete:
        StringAppend(str, "\r\nProgram Complete\r\n");
        break;
    case ExecutionState::Paused:
        StringAppend(str, "\r\nProgram paused without finishing\r\n");
        break;
    case ExecutionState::Waiting:
        StringAppend(str, "\r\nProgram waiting for input\r\n");
        break;
    case ExecutionState::ErrorState:
        errState = 1;
        StringAppend(str, "\r\nProgram ERRORED\r\n");
        break;
    case ExecutionState::Running:
        errState = 1;
        StringAppend(str, "\r\nProgram still running?\r\n");
        break;
    default:
        errState = 1;
        StringAppend(str, "\r\nUnknown State!\r\n");
        break;
        break;
    }
	return errState;
}

int RunProgram(const char* filename) {
    // Load, compile and  run a program inside an arena

    size_t alloc, unalloc;
    int objects;
    std::cout << "########## Attempting program: " << filename << " #########\n";

    MMPush(1 MEGABYTE); // this is arena is used to read out the results to the console. Most memory allocations should stay inside the interpreter.
    auto program = VecAllocate_DataTag();

    // Compile and load
    MMPush(10 MEGABYTES);
        auto code = StringEmpty();
        auto fileName = StringNew(filename);
        auto vec = StringGetByteVector(code);
        uint64_t read = 0;
        if (!FileLoadChunk(fileName, vec, 0, 10000, &read)) {
            std::cout << "Failed to read file. Test inconclusive.\n";
            return 1;
        }
        StringDeallocate(fileName);
        auto compilableSyntaxTree = ParseSourceCode(code, false);
        auto tagCode = CompileRoot(compilableSyntaxTree, false, false);

        auto nextPos = TCW_AppendToVector(tagCode, program);

        StringDeallocate(code);
        DeallocateAST(compilableSyntaxTree);
        TCW_Deallocate(tagCode);
    MMPop();

    // set-up
    auto is = InterpAllocate(program, 1 MEGABYTE, NULL);
    VecDeallocate(program);

    ArenaGetState(MMCurrent(), &alloc, &unalloc, NULL, NULL, &objects, NULL);

    auto inp = StringNew("xhello, world\nLine2\nLine3\n"); // some sample input
    WriteInput(is, inp);
    StringDeallocate(inp);

    TraceArena(MMCurrent(), true); // we shouldn't be allocating outside the interpreter

    // run
    auto startTime = SystemTime();
    auto result = InterpRun(is, 5000);
    while (result.State == ExecutionState::Paused) {
        TraceArena(MMCurrent(), false);
        auto str = StringEmpty();
        ReadOutput(is, str);
        WriteStrInline(str);
        StringDeallocate(str);
        TraceArena(MMCurrent(), true);
        result = InterpRun(is, 5000);
    }
    auto endTime = SystemTime();

    TraceArena(MMCurrent(), false);

    auto str = StringEmpty();
	int errState = AppendFinishState(result.State, str);

    ReadOutput(is, str);
    WriteStr(str);
    StringDeallocate(str);
    std::cout << "Execution took " << (endTime - startTime) << " seconds\n";


    // Check memory state
    ArenaGetState(InterpInternalMemory(is), &alloc, &unalloc, NULL, NULL, &objects, NULL);
    std::cout << "Runtime used in internal memory: " << alloc << " bytes across " << objects << " objects. " << unalloc << " free.\n";
    ArenaGetState(MMCurrent(), &alloc, &unalloc, NULL, NULL, &objects, NULL);
    std::cout << "Runtime used in external memory: " << alloc << " bytes across " << objects << " objects. " << unalloc << " free.\n";

    InterpDeallocate(is);

    MMPop();
    return errState;
}

int TestProgramSuite() {
    // Run a range of programs.
    // These should be loaded into the 'jail' folder from the repo's samples

    int errs = 0;

#ifndef _DEBUG
    errs += RunProgram("stressTest.ecs"); // really slow in debug mode
#endif

    errs += RunProgram("Importer.ecs");
    errs += RunProgram("demo_program2.ecs");
    errs += RunProgram("demo_program3.ecs");
    errs += RunProgram("fib.ecs");
    errs += RunProgram("getWithIndex.ecs");
    errs += RunProgram("hashmaps.ecs");
    errs += RunProgram("listMath.ecs");
    errs += RunProgram("lists.ecs");
    errs += RunProgram("nestedLoops.ecs");
    errs += RunProgram("pick.ecs");
    errs += RunProgram("pick2.ecs");
    errs += RunProgram("stringSearch.ecs");
    errs += RunProgram("strings.ecs");
	
    std::cout << "########## Error count = " << errs << " #########\n";
    return errs;
}

Vector* Compile(const char* filename) {
	// Load and compile
	auto program = VecAllocate_DataTag();

	// Compile and load
	MMPush(10 MEGABYTES);
	auto code = StringEmpty();
	auto fileName = StringNew(filename);
	auto vec = StringGetByteVector(code);
	uint64_t read = 0;
	if (!FileLoadChunk(fileName, vec, 0, 10000, &read)) {
		std::cout << "Failed to read file. Test inconclusive.\n";
		return NULL;
	}
	StringDeallocate(fileName);
	auto compilableSyntaxTree = ParseSourceCode(code, false);
	auto tagCode = CompileRoot(compilableSyntaxTree, false, false);

	auto nextPos = TCW_AppendToVector(tagCode, program);

	StringDeallocate(code);
	DeallocateAST(compilableSyntaxTree);
	TCW_Deallocate(tagCode);
	MMPop();

	return program;
}

int TestMultipleRuntimes () {
	
    std::cout << "***************** MULTIPLE RUNTIMES ******************\n";

	auto code1 = Compile("demo_program2.ecs");
	auto code2 = Compile("demo_program3.ecs");
	
    auto prog1 = InterpAllocate(code1, 1 MEGABYTE, NULL);
    auto prog2 = InterpAllocate(code2, 1 MEGABYTE, NULL);

    VecDeallocate(code1);
    VecDeallocate(code2);


	// alternate between running two programs
	ExecutionResult result1, result2;
	bool run1 = true;
	bool run2 = true;
	while (run1 || run2) {
		
		if (run1) {
			result1 = InterpRun(prog1, 5);
			std::cout << "1";
		}
		if (result1.State != ExecutionState::Paused && result1.State != ExecutionState::Waiting) run1 = false;
		
		if (run2) {
			result2 = InterpRun(prog2, 5);
			std::cout << "2";
		}
		if (result2.State != ExecutionState::Paused && result2.State != ExecutionState::Waiting) run2 = false;
	}
	
    auto str = StringEmpty();
	StringAppend(str, "\r\n Program 1:\r\n");
	int errState = AppendFinishState(result1.State, str);
    ReadOutput(prog1, str);

	StringAppend(str, "\r\n Program 2:\r\n");
	errState += AppendFinishState(result2.State, str);
    ReadOutput(prog2, str);

    WriteStr(str);
    StringDeallocate(str);

	
    InterpDeallocate(prog1);
    InterpDeallocate(prog2);

	return 0;
}

int main() {
    auto aares = TestArenaAllocator();
    if (aares != 0) return aares;

    StartManagedMemory();

    MMPush(1 MEGABYTE);
    auto vres = TestVector();
    if (vres != 0) return vres;
    MMPop();

    MMPush(1 MEGABYTE);
    auto qres = TestQueue();
    if (qres != 0) return qres;
    MMPop();

    MMPush(1 MEGABYTE);
    auto hmres = TestHashMap();
    if (hmres != 0) return hmres;
    MMPop();

    MMPush(1 MEGABYTE);
    auto tres = TestTree();
    if (tres != 0) return tres;
    MMPop();

    MMPush(1 MEGABYTE);
    auto sres = TestString();
    if (sres != 0) return sres;
    MMPop();

    MMPush(1 MEGABYTE);
    auto hres = TestHeaps();
    if (hres != 0) return hres;
    MMPop();

    MMPush(1 MEGABYTE);
    auto tagres = TestTagData();
    if (tagres != 0) return tagres;
    MMPop();
    
    MMPush(1 MEGABYTE);
    auto serdsres = TestSerialisation();
    if (serdsres != 0) return serdsres;
    MMPop();

    MMPush(1 MEGABYTE);
    auto fsres = TestFileSystem();
    if (fsres != 0) return fsres;
    MMPop();

    MMPush(10 MEGABYTES);
    auto bigone = TestCompiler();
    if (bigone != 0) return bigone;
    MMPop();

    MMPush(10 MEGABYTES);
    auto runit = TestRuntimeExec();
    if (runit != 0) return runit;
    MMPop();

    auto suite = TestProgramSuite();
    if (suite != 0) return suite;
	
    MMPush(10 MEGABYTES);
    auto multi = TestMultipleRuntimes();
    if (multi != 0) return multi;
    MMPop();

    ShutdownManagedMemory();
}
