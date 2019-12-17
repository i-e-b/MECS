#include <stdlib.h>
#include <iostream>

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
#include "DisplaySys.h"
#include "EventSys.h"
#include "Console.h"

// The MECS compiler and runtime
#include "SourceCodeTokeniser.h"
#include "CompilerCore.h"
#include "TagCodeInterpreter.h"
#include "TypeCoersion.h"
#include "RuntimeScheduler.h"

ScreenPtr OutputScreen;
ConsolePtr cnsl;

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

int TestHashMap() {
    Log(cnsl,"*************** HASH MAP *****************\n");
    Log(cnsl,"Allocating\n");
    // start small enough that we will go through a grow cycle when adding
    auto hmap = MapAllocate_int_int(64);

    Log(cnsl,"Writing entries\n");
    for (int i = 0; i < 100; i++) {
        int key = i;
        int value = 2 * i;
        MapPut_int_int(hmap, key, value, true);
    }

    Log(cnsl,"Looking up data\n");
    int lukey = 40;
    int* lu_val_ptr = NULL;
    if (!MapGet_int_int(hmap, lukey, &lu_val_ptr)) {
        Log(cnsl,"Get failed!\n");
        return 1;
    }
    LogFmt(cnsl,"Found value \x02 (expected 80)\n", *lu_val_ptr);

    auto has50 = MapGet_int_int(hmap, 50, NULL);
    auto hasNeg1 = MapGet_int_int(hmap, -1, NULL);
    LogFmt(cnsl,"Has 50? \x05; Has -1? \x05\n", (has50 ? "yes" : "no"), (hasNeg1 ? "yes" : "no"));

    MapRemove_int_int(hmap, 50);
    has50 = MapGet_int_int(hmap, 50, NULL);
    LogFmt(cnsl,"Has 50 after removal? \x05\n", (has50 ? "yes" : "no"));

	LogFmt(cnsl,"Count before clear = \x02\n", MapCount(hmap));
    MapClear(hmap);
	LogFmt(cnsl,"Count after clear = \x02\n", MapCount(hmap));


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


    Log(cnsl,"Deallocating map\n");
    MapDeallocate(hmap);
    return 0;
}

int TestVector() {
    Log(cnsl,"**************** VECTOR *******************\n");
    auto testElement = exampleElement{ 20,5 };

    // See if the container stuff works...
    Log(cnsl,"Allocating\n");
    auto gvec = VecAllocate_exampleElement();
    Log(cnsl,"Vector OK? ");
	Log(cnsl,VectorIsValid(gvec) ? "true\n" : "false\n");

    // add some entries
    Log(cnsl,"Writing entries with 'push'\n");
    for (int i = 0; i < 1000; i++) {
        if (!VecPush_exampleElement(gvec, testElement)) {
            Log(cnsl,"Push failed!\n");
            return 255;
        }
    }
	Log(cnsl,"Vector ");Log(cnsl,VectorIsValid(gvec)?"OK":"Bad");
	LogFmt(cnsl,"; Elements stored = \x02\n", VectorLength(gvec));

    // read a random-access element
    auto r = VecGet_exampleElement(gvec, 5);
    LogFmt(cnsl,"Element 5 data = \x02, \x02\n", r->a, r->b);

    // resize the array
    VectorPrealloc(gvec, 5000);
	LogFmt(cnsl,"Vector \x05; Elements stored = \x02\n", VectorIsValid(gvec)?"OK":"BAD", VectorLength(gvec));

    // Pop a load of values
    Log(cnsl,"Reading and removing entries with 'pop'\n");
    for (int i = 0; i < 4000; i++) {
        exampleElement poppedData;
        if (!VecPop_exampleElement(gvec, &poppedData)) {
            Log(cnsl,"Pop failed!\n");
            return 254;
        }
    }
	LogFmt(cnsl,"Vector \x05; Elements stored = \x02\n", VectorIsValid(gvec)?"OK":"BAD", VectorLength(gvec));

    // Set a different element value
    auto newData = exampleElement{ 255,511 };
    exampleElement capturedOldData;
    VecSet_exampleElement(gvec, 70, newData, &capturedOldData);
    LogFmt(cnsl,"Replace value at 70. Old data = \x02, \x02\n", capturedOldData.a, capturedOldData.b);
    r = VecGet_exampleElement(gvec, 70);
    LogFmt(cnsl,"Element 70 new data = \x02, \x02 (should be 255,511)\n", r->a, r->b);

    // Swap elements by index pair
    Log(cnsl,"Swapping 60 and 70\n");
    VectorSwap(gvec, 60, 70);
    r = VecGet_exampleElement(gvec, 60);
    LogFmt(cnsl,"Element 60 new data = \x02, \x02 (255,511)\n", r->a, r->b);
    r = VecGet_exampleElement(gvec, 70);
    LogFmt(cnsl,"Element 70 new data = \x02, \x02 (20,5)\n", r->a, r->b);

    Log(cnsl,"Deallocating\n");
    VectorDeallocate(gvec);
    Log(cnsl,VectorIsValid(gvec)? "Vector OK\n" : "Vector gone\n");

    // Test sorting
    gvec = VecAllocate_exampleElement();
    int sal = 70;
    for (int i = 0; i < sal; i++) {
        VecPush_exampleElement(gvec, exampleElement{ ((i * 6543127) % sal) - 10, i }); // a very crappy random
    }
    Log(cnsl,"Before sort:\n");
    for (int i = 0; i < sal; i++) {
		LogFmt(cnsl,"\x02, ", VecGet_exampleElement(gvec, i)->a);
    }
    Log(cnsl,"\n");

    Log(cnsl,"After sort:\n");
    VecSort_exampleElement(gvec, CompareExampleElement); // sort into ascending order (reverse the compare to reverse the order)
    for (int i = 0; i < sal; i++) {
		LogFmt(cnsl,"\x02, ", VecGet_exampleElement(gvec, i)->a);
    }
    Log(cnsl,"\n");
    VectorDeallocate(gvec);

	// Queue/Dequeue chain
	Log(cnsl,"Dequeue chain ");
	auto q = VecAllocate_char();
	char c = '0';
	for (int i = 0; i < 5; i++) { VecPush_char(q, (char)(i+48)); }
	for (int i = 5; i < 78; i++)
	{
		VecPush_char(q, (char)(i+48));
		VecDequeue_char(q, &c);
		Log(cnsl,c);
	}
	while (VecDequeue_char(q, &c)) { Log(cnsl,c); }
    Log(cnsl,"\n              ");
	for (int i = 0; i < 78; i++)
	{
		VecPush_char(q, (char)(i+48));
		VecDequeue_char(q, &c);
		Log(cnsl,c);
	}
    Log(cnsl,"\n");
	VecDeallocate(q);

    // Check that vectors pin to the arena they were created in:
    size_t beforeOuter, afterInner, afterOuter, finalOuter;
    ArenaGetState(MMCurrent(), &beforeOuter, NULL, NULL, NULL, NULL, NULL);
    auto pinv = VecAllocate_char();
    if (!MMPush(256 KILOBYTES)) {
        Log(cnsl,"Arena allcation failed");
        return 253;
    }
    for (int i = 0; i < (100 KILOBYTES); i++) {
        VecPush_char(pinv, (char)((i % 100) + 33));
    }
    ArenaGetState(MMCurrent(), &afterInner, NULL, NULL, NULL, NULL, NULL);

    // delete the arena, check we can still access data
    MMPop();
    for (int i = 0; i < (100 KILOBYTES); i++) {
        VecPop_char(pinv, &c);
    }

    int refs = 0;
    ArenaGetState(MMCurrent(), &afterOuter, NULL, NULL, NULL, &refs, NULL);
    VecDeallocate(pinv);
    ArenaGetState(MMCurrent(), &finalOuter, NULL, NULL, NULL, NULL, NULL);

	LogFmt(cnsl,"Memory arena pinning: Outer before=\x02; after=\x02; final=\x02\n", beforeOuter, afterOuter, finalOuter);
	LogFmt(cnsl,"                      Inner after=\x02 (should be zero)\n", afterInner);
	LogFmt(cnsl,"                      References after pops=\x02\n", refs);

    return 0;
}

int TestQueue() {
    Log(cnsl,"**************** QUEUE (VECTOR) *******************\n");
    // queues are a feature of vectors (makes things a bit complex)

    auto q = VecAllocate_char();
    auto str = "This is a string of chars to fill our vector. It has to be pretty long to cross chunk boundaries. Neque porro quisquam est qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit.";
    // fill up vector
    auto ptr = str;
    while (*ptr != 0) { VecPush_char(q, *ptr++); }

    // empty it back into std-out
    char c;
    while (VecDequeue_char(q, &c)) { Log(cnsl,c); }
    Log(cnsl,"\n");

    // Fill up again and empty from both ends...
    auto pal = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzzyxwvutsrqponmlkjihgfedcba9876543210ZYXWVUTSRQPONMLKJIHGFEDCBA";
    ptr = pal;
    while (*ptr != 0) { VecPush_char(q, *ptr++); }
    while (VecLength(q) > 0) {
        VecDequeue_char(q, &c); Log(cnsl,c);
        VecPop_char(q, &c); Log(cnsl,c);
    }
    Log(cnsl,"\n");

    // Fill up again and empty from both ends, but using indexes
    ptr = pal;
    while (*ptr != 0) { VecPush_char(q, *ptr++); }
    while (VecLength(q) > 0) {
        VecCopy_char(q, 0, &c); Log(cnsl,c);
        VecCopy_char(q, VecLength(q) - 1, &c); Log(cnsl,c);
        VecDequeue_char(q, &c);
        VecPop_char(q, &c);
    }
    Log(cnsl,"\n");

    return 0;
}

int TestTree() {
    Log(cnsl,"**************** TREE *******************\n");

    Log(cnsl,"Allocating\n");
    auto tree = TAllocate_exampleElement();

    Log(cnsl,"Adding elements\n");
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


    Log(cnsl,"Reading elements\n");
    // find elem5 the long way...
    auto find = tree;
    find = TChild(find);
    find = TSibling(find);
    find = TChild(find);
    find = TSibling(find);
    auto found = TReadBody_exampleElement(find);
    LogFmt(cnsl,"Element 5 data (expecting 2,5) = \x02, \x02\n", found->a, found->b);

    Log(cnsl,"Deallocating\n");
    TDeallocate(tree);
    return 0;
}

int TestString() {
    Log(cnsl,"*************** MUTABLE STRING *****************\n");

    String* str1 = StringNew("Hello, ");
    String* str2 = StringNew("World");

    LogFmt(cnsl,"Hashes of original strings: \x03, \x03\n", StringHash(str1), StringHash(str2));
    LogFmt(cnsl,"String lengths before append: \x02, \x02\n", StringLength(str1), StringLength(str2));
    StringAppend(str1, str2);
    StringAppend(str1, "!");
    StringDeallocate(str2);
    LogFmt(cnsl,"String length after appends: \x02\n", StringLength(str1));
    LogFmt(cnsl,"Hashes of result string: \x02\n", StringHash(str1));

    auto cstr = StringToCStr(str1);
    LogLine(cnsl,cstr);
    mfree(cstr);
    LogFmt(cnsl,"First char = '\x04'\n", StringCharAtIndex(str1, 0));
    LogFmt(cnsl,"Last char = '\x04'\n", StringCharAtIndex(str1, -1));

    StringToUpper(str1);
    Log(cnsl,"Upper case: ");
    LogLine(cnsl,str1);

    StringToLower(str1);
	Log(cnsl,"Lower case: ");
    LogLine(cnsl,str1);

    // Search
    str2 = StringNew("lo,");
    unsigned int pos;
    if (StringFind(str1, str2, 0, &pos)) {
        LogFmt(cnsl,"Found at \x02\n", pos);
    } else {
        Log(cnsl,"Didn't find a string I was expecting!?\n");
    }
    if (StringFind(str1, str2, 4, NULL)) {
        Log(cnsl,"Found a string I wasn't expecting\n");
    }
    StringDeallocate(str2);

    str2 = StringNew("l,o");
    if (StringFind(str1, str2, 0, &pos)) { // this should trigger the 'add' method to a false positive, but still return not found
        Log(cnsl,"Found a string I wasn't expecting\n");
    }
    StringDeallocate(str2);

    // Slicing and chopping
    str2 = StringChop(StringSlice(str1, -2, 2), 0, 5); // last 2 chars = 'd!', then get 5 chars from that str = 'd!d!d'
    Log(cnsl,str2);

    // Comparison
    LogLine(cnsl,StringStartsWith(str1, "hello") ? "cmp 1 OK" : "cmp 1 failed");
    LogLine(cnsl,StringStartsWith(str1, "fish") ? "cmp 2 failed" : "cmp 2 OK");
    LogLine(cnsl,StringStartsWith(str1, str1) ? "cmp 3 OK" : "cmp 3 failed");
    LogLine(cnsl,StringEndsWith(str1, "world!") ? "cmp 4 OK" : "cmp 4 failed");
    LogLine(cnsl,StringEndsWith(str1, "fish") ? "cmp 5 failed" : "cmp 5 OK");
    LogLine(cnsl,StringEndsWith(str1, str1) ? "cmp 6 OK" : "cmp 6 failed");
    LogLine(cnsl,StringAreEqual(str1, "fish") ? "cmp 7 failed" : "cmp 7 OK");
    LogLine(cnsl,StringAreEqual(str1, str2) ? "cmp 8 failed" : "cmp 8 OK");
    LogLine(cnsl,StringAreEqual(str1, str1) ? "cmp 9 OK" : "cmp 9 failed");

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

    LogLine(cnsl,str1);
    Log(cnsl,"1000, 1234, -4567, 0, 2147483647, 0123ABCD\n");

    // Parsing strings
    // INTEGERS
    StringClear(str1);
    StringAppend(str1, "1000");
    int32_t int32res = 0;
    bool ok = StringTryParse_int32(str1, &int32res);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendInt32(str1, int32res);
    Log(cnsl,str1);

    StringClear(str1);
    StringAppend(str1, "0001234000");
    ok = StringTryParse_int32(str1, &int32res);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendInt32(str1, int32res);
    Log(cnsl,str1);

    StringClear(str1);
    StringAppend(str1, "-123");
    ok = StringTryParse_int32(str1, &int32res);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendInt32(str1, int32res);
    Log(cnsl,str1);

    // Parsing strings
    // FLOATING POINT
    StringClear(str1);
    StringAppend(str1, "-110.001");
    double fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    Log(cnsl,str1);

    StringClear(str1);
    StringAppend(str1, "110.01");
    fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    Log(cnsl,str1);

    StringClear(str1);
    StringAppend(str1, "-110");
    fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    Log(cnsl,str1);

    StringClear(str1);
    StringAppend(str1, "3000.0123");
    fix = 0;
    ok = StringTryParse_double(str1, &fix);
    StringAppend(str1, ok ? " (ok)" : " (fail)");
    StringAppend(str1, " = ");
    StringAppendDouble(str1, fix);
    Log(cnsl,str1);

    // String formatting
    StringClear(str1);
    StringClear(str2);
    StringAppend(str2, "'This is from a (String*)'");
    // Simple append
    StringAppendFormat(str1, "Formatted string, with literal (this), included mutable strings: \x01, decimal: \x02, and hex: \x03.", str2, 1234, 1234);
    Log(cnsl,str1);
    // Create from format
    StringDeallocate(str2);
    str2 = StringNewFormat("String inception: \"\x01\"", str1);
    Log(cnsl,str2);

    StringDeallocate(str1);
    StringDeallocate(str2);

    // Test search-and-replace:
    str1 = StringNew("This is a line in the sand, and will stand as a pillar of our hopes and dreams.");
    auto needle = StringNew("and");
    auto replacement = StringNew("but also");
    str2 = StringReplace(str1, needle, replacement);

    Log(cnsl,str1);
    Log(cnsl,str2); // This is a line in the sbut also, but also will stbut also as a pillar of our hopes but also dreams.

    StringDeallocate(str1);
    StringDeallocate(str2);
    StringDeallocate(needle);
    StringDeallocate(replacement);

    return 0;
}

int TestHeaps() {
    Log(cnsl,"*************** BINARY HEAP (Priority Queue) *****************\n");
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
    Log(cnsl,str1);
    Log(cnsl,str2);
    Log(cnsl,str3);
    StringDeallocate(str1);
    StringDeallocate(str2);
    StringDeallocate(str3);
    
    HDeallocate(heap);

    return 0;
}

int TestTagData() {
    Log(cnsl,"***************** TAG DATA ******************\n");

    auto tag = DataTag { (int)DataType::VectorPtr, ALLOCATED_TYPE + 2, 0x82 }; // these should be the same

    auto str = StringEmpty();
    StringAppend(str, "Tag: ");
    StringAppendInt32(str, tag.type);
    StringAppend(str, ", Params: ");
    StringAppendInt32(str, tag.params);
    StringAppend(str, ", Data: ");
    StringAppendInt32(str, tag.data);
    Log(cnsl,str);
    StringClear(str);

    tag = EncodeOpcode('x', 'a', 1, 1);

    char p1, p2;
    uint8_t p3;
    uint32_t param;
    DecodeLongOpcode(tag, &p1, &p2, &param, &p3);

    if (p1 == 'x' && p2 == 'a' && param == 0x00010001 && p3 == 0) { StringAppend(str, "OpCodes OK;"); }
    else { StringAppend(str, "OPCODES FAILED;"); }
    Log(cnsl,str);

    StringClear(str);
    StringAppend(str, "ShrtStr");
    tag = EncodeShortStr(str);
    StringAppend(str, "decoded: '");
    DecodeShortStr(tag, str);
    StringAppend(str, "' (expected 'ShrtStr')");
    Log(cnsl,str);

    StringClear(str);
    tag = EncodeShortStr("Hello!");
    StringAppend(str, "decoded: '");
    DecodeShortStr(tag, str);
    StringAppend(str, "' (expected 'Hello!')");
    Log(cnsl,str);

    double origd = 123450.098765;
    StringClear(str);
    StringAppendDouble(str, origd);
    tag = EncodeDouble(origd);
    StringAppend(str, "; decoded: '");
    double resd = DecodeDouble(tag);
    StringAppendDouble(str, resd);
    StringAppend(str, "' (expected approx 123450.098765)");
    Log(cnsl,str);


    StringClear(str);
    DescribeTag(EncodeOpcode('j', 'F', 1, 1), str, NULL);
    StringNL(str);
    DescribeTag(RuntimeError(0xDEAD), str, NULL);
    StringNL(str);
    DescribeTag(EncodeVariableRef(str, NULL), str, NULL);
    StringNL(str);
    Log(cnsl,str);


    StringDeallocate(str);

    return 0;
}

int TestFileSystem() {
    Log(cnsl,"***************** FILE SYS ******************\n");
    auto vec = VecAllocate_char();

    auto path = StringNew("Test.txt");
    uint64_t read = 0;
    bool ok = FileLoadChunk(path, vec, 0, 1000, &read);

    LogFmt(cnsl,"Existing file read OK = \x06; Bytes read: \x02\n", ok, read);
    Log(cnsl,"File contents:\n");
    char c = 0;
    while (VecDequeue_char(vec, &c)) {
		Log(cnsl,c);
    }
    Log(cnsl,"\n");

    for (int i = 32; i < 255; i++) {
        VecPush_char(vec, i);
    }
    StringClear(path);
    StringAppend(path, "output.txt");
    ok = FileWriteAll(path, vec);
	LogFmt(cnsl,"Trunc & Write OK = \x06; Bytes not written: \x02n", ok, VectorLength(vec));

    for (int i = 32; i < 255; i++) {
        VecPush_char(vec, i);
    }
    ok = FileAppendAll(path, vec);
	LogFmt(cnsl,"append Write OK = \x06; Bytes not written: \x02n", ok, VectorLength(vec));

    ok = FileLoadChunk(path, vec, 0, 1000, &read);
	LogFmt(cnsl,"Read OK = \x06; Bytes read: \x02\n", ok, read);
    Log(cnsl,"File contents:\n");
    c = 0;
    while (VecDequeue_char(vec, &c)) {
        Log(cnsl,c);
    }
    Log(cnsl,"\n");

    StringDeallocate(path);
    VectorDeallocate(vec);

    return 0;
}

int TestArenaAllocator() {
    Log(cnsl,"***************** ARENA ALLOCATOR ******************\n");
    size_t allocatedBytes;
    size_t unallocatedBytes;
    int occupiedZones;
    int emptyZones;
    int totalReferenceCount;
    size_t largestContiguous;


    auto arena1 = NewArena(10 MEGABYTES);

    ArenaGetState(arena1, &allocatedBytes, &unallocatedBytes, &occupiedZones, &emptyZones, &totalReferenceCount, &largestContiguous);
	LogFmt(cnsl,"Empty 10MB Arena: alloc=\x02; free=\x02; frgs used=\x02; frgs empty=\x02; refs=\x02; max chunk=\x02\n",
		allocatedBytes, unallocatedBytes, occupiedZones,emptyZones,totalReferenceCount,largestContiguous);

    // allocate some bits
    for (int i = 0; i < 100; i++) {
        auto ptr = ArenaAllocate(arena1, ARENA_ZONE_SIZE / 5);
        if (ptr == NULL) {
			LogFmt(cnsl,"Failed to allocate at \x02\n", i); break;
        }
    }
    // and happily ignore them all, as long as we keep the arena reference

    ArenaGetState(arena1, &allocatedBytes, &unallocatedBytes, &occupiedZones, &emptyZones, &totalReferenceCount, &largestContiguous);
	LogFmt(cnsl," Used 10MB Arena: alloc=\x02; free=\x02; frgs used=\x02; frgs empty=\x02; refs=\x02; max chunk=\x02\n",
		allocatedBytes, unallocatedBytes, occupiedZones,emptyZones,totalReferenceCount,largestContiguous);

    DropArena(&arena1);
	LogLine(cnsl,arena1 == NULL ? "Arena was destroyed" : "Arena was NOT corrected dropped");
    
    // Test referencing and de-referencing
    auto arena2 = NewArena(256 KILOBYTES);

    ArenaGetState(arena2, &allocatedBytes, NULL, &occupiedZones, NULL, &totalReferenceCount, NULL);
	LogFmt(cnsl,"Empty Arena: alloc=\x02; frgs used=\x02; refs=\x02\n",
		allocatedBytes, occupiedZones, totalReferenceCount );

    void* bits[20];
    for (int i = 0; i < 20; i++) { bits[i] = ArenaAllocate(arena2, 256); }
    ArenaGetState(arena2, &allocatedBytes, NULL, &occupiedZones, NULL, &totalReferenceCount, NULL);
	LogFmt(cnsl," Used Arena: alloc=\x02; frgs used=\x02; refs=\x02\n",
		allocatedBytes, occupiedZones, totalReferenceCount );

    for (int i = 1; i < 20; i+=2) { ArenaReference(arena2, bits[i]); }
    for (int i = 0; i < 20; i+=2) { ArenaDereference(arena2, bits[i]); }
    for (int i = 0; i < 20; i++) { // this will double-dereference some bits, which should be ignored
        ArenaDereference(arena2, bits[i]);
        ArenaDereference(arena2, bits[i]);
    }

    ArenaGetState(arena2, &allocatedBytes, NULL, &occupiedZones, NULL, &totalReferenceCount, NULL);
	LogFmt(cnsl, "Used and dereferenced Arena: alloc=\x02; frgs used=\x02; refs=\x02\n",
		allocatedBytes, occupiedZones, totalReferenceCount );
    DropArena(&arena2);

    // TODO: expand this when String/Vector/Hashtable etc can use the arena allocator

    return 0;
}

int TestSerialisation() {
    Log(cnsl,"***************** SERIALISATION ******************\n");
    
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
        Log(cnsl,"Serialisation failed\n");
        return -1;
    }

	Log(cnsl,"Result bytes = ");
    int length = VecLength(vec);
    for (int i = 0; i < length; i++) {
		LogFmt(cnsl,"\x03 ",(int)((uint8_t)*VecGet_char(vec, i)));
    }
    Log(cnsl,"\nSerialisation OK, trying deserialisation...\n");

    // Try restore
    
    auto targetInterp = InterpAllocate(tagCode, 1 MEGABYTE, NULL);
    auto targetArena = InterpInternalMemory(targetInterp);
    DataTag dest = {};
    ok = DefrostFromVector(&dest, targetArena, vec);
    if (!ok) {
        Log(cnsl,"Deserialisation failed\n");
        return -2;
    }

    // Show final result
    auto human = CastString(targetInterp, dest);
    Log(cnsl,human);
    StringDeallocate(human);

    //////////////////////////////////////////////////
    // More complex stuff.
    // We use a short program to help

    // Run the program:
    auto result = InterpRun(interp, 5000);
    if (result.State != ExecutionState::Complete) {
        Log(cnsl,"Test program did not complete:\n\n");
        auto outp = StringEmpty();
        ReadOutput(interp, outp);
        Log(cnsl,outp);
        StringDeallocate(outp);
        return -3;
    }

    // Show what we are expecting:
    human = CastString(interp, result.Result);
    Log(cnsl,"Serialiser input = "); 
    LogLine(cnsl,human);
    StringDeallocate(human);

    // the output should be loaded into `result.Result`
    // serialise it...
    ok = FreezeToVector(result.Result, /*InterpreterState*/interp, /*Vector<byte>*/vec);
    if (!ok) { Log(cnsl,"Serialisation failed\n"); return -1; }

    // display serialised data
    Log(cnsl,"Result bytes = ");
    length = VecLength(vec);
    for (int i = 0; i < length; i++) {
		LogFmt(cnsl,"\x03 ",(int)((uint8_t)*VecGet_char(vec, i)));
	}
    Log(cnsl,"\nSerialisation OK.\n");

	
    Log(cnsl,"Check repeated serialisation (should not damage orignal data)\n");
    ok = FreezeToVector(result.Result, /*InterpreterState*/interp, /*Vector<byte>*/vec);
    if (!ok) { Log(cnsl,"Serialisation failed\n"); return -1; }
    Log(cnsl,"Result bytes = ");
    length = VecLength(vec);
    for (int i = 0; i < length; i++) {
		LogFmt(cnsl,"\x03 ",(int)((uint8_t)*VecGet_char(vec, i)));
	}
    Log(cnsl,"\nSerialisation OK. Should be exact same data\n");

    // deserialise
    ok = DefrostFromVector(&dest, targetArena, vec);
    if (!ok) { Log(cnsl,"Deserialisation failed\n"); return -2; }

    // Show final result
    human = CastString(targetInterp, dest);
    Log(cnsl,"Deserialisation result = "); 
    LogLine(cnsl,human);
    StringDeallocate(human);

    // Clean up interpreters and their arenas
    InterpDeallocate(interp);
    InterpDeallocate(targetInterp);

    return 0;
}

int TestCompiler() {
    Log(cnsl,"***************** COMPILER ******************\n");

    auto code = StringEmpty();
    auto pathOfInvalid = StringNew("Test.txt"); // not valid source
    //auto pathOfValid = StringNew("demo_program.ecs"); // should be valid source
    auto pathOfValid = StringNew("demo_program.ecs"); // should be valid source

    auto vec = StringGetByteVector(code);
    uint64_t read = 0;
    LogLine(cnsl,pathOfInvalid);
    if (!FileLoadChunk(pathOfInvalid, vec, 0, 10000, &read)) {
        Log(cnsl,"Failed to read file. Test inconclusive.\n");
        return -1;
    }
    
    Log(cnsl,"Reading a non-source code file: ");
    auto syntaxTree = ParseSourceCode(code, false);
    StringClear(code); // also clears the underlying vector
    if (syntaxTree == NULL) { Log(cnsl,"Parser failed entirely"); return -2; }

    SourceNode *result = (SourceNode*)TreeReadBody(syntaxTree);

    if (result->IsValid) {
        Log(cnsl,"The source file was parsed correctly!? It should not have been!\n");
        return -3;
    }

    Log(cnsl,"The source file was not valid (this is ok)\n");

    StringDeallocate(pathOfInvalid);
    DeallocateAST(syntaxTree);

    //###################################################################
    Log(cnsl,"Reading a valid source code file: ");
	LogLine(cnsl,pathOfValid);
    if (!FileLoadChunk(pathOfValid, vec, 0, 10000, &read)) {
        Log(cnsl,"Failed to read file. Test inconclusive.\n");
        return -4;
    }
    auto compilableSyntaxTree = ParseSourceCode(code, false); // Compiler doesn't like metadata!
    syntaxTree = ParseSourceCode(code, true);
    StringDeallocate(code);
    if (syntaxTree == NULL) { Log(cnsl,"Parser failed entirely"); return -5; }

    result = (SourceNode*)TreeReadBody(syntaxTree);

    if (!result->IsValid) {
        Log(cnsl,"The source file was not valid (FAIL!)\n");
    } else {
        Log(cnsl,"The source file was parsed correctly:\n\n");
    }

    auto nstr = RenderAstToSource(syntaxTree); // render it even if bad -- as it contains error details
    if (nstr == NULL) {
        Log(cnsl,"Failed to render AST\n");
        return -8;
    }
    LogLine(cnsl,nstr);

    Log(cnsl,"Attempting to compile:\n");
    auto tagCode = CompileRoot(compilableSyntaxTree, false, false);

    if (TCW_HasErrors(tagCode)) {
        Log(cnsl,"COMPILE FAILED!\n");
        auto errs = TCW_ErrorList(tagCode);
        String* msg = NULL;
        while (VecPop_StringPtr(errs, &msg)) {
            LogLine(cnsl,msg);
        }
        return -1;
    } else {
        Log(cnsl,"Compile OK\n");
        Log(cnsl,"Listing tag-code (excluding strings)\n\n");
        
        int opCount = TCW_OpCodeCount(tagCode);
        auto tagStr = StringEmpty();
        for (int i = 0; i < opCount; i++) {
            DataTag opcode = TCW_OpCodeAtIndex(tagCode, i);
            DescribeTag(opcode, tagStr, TCW_GetSymbols(tagCode));
            StringAppendChar(tagStr, '\n');
        }
        LogLine(cnsl,tagStr);
        StringDeallocate(tagStr);
        
        Log(cnsl,"\n\nWriting to code file...");
        auto buf = TCW_WriteToStream(tagCode);
		LogFmt(cnsl," \x02 bytes...", VecLength(buf));
        FileWriteAll(StringNew("tagcode.dat"), buf);

        Log(cnsl,"\n\nWriting to symbols file...");
        TCW_WriteSymbolsToStream(tagCode, buf);
		LogFmt(cnsl," \x02 bytes...", VecLength(buf));
        FileWriteAll(StringNew("tagsymb.dat"), buf);
        Log(cnsl,"Done\n");

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
	LogFmt(cnsl,"Compiling leaked \x02 bytes across \x02 objects. \x02 free.\n",
		alloc, objects, unalloc);

    return 0;
}

int TestRuntimeExec() {
    // This relies on the 'tagcode.dat' file created in the test compiler step
    Log(cnsl,"***************** RUNTIME ******************\n");

    // Read code file --------------------------------
    auto tagCode = VecAllocate_DataTag();
    auto path = StringNew("tagcode.dat");
    uint64_t actual;
    bool ok = FileLoadChunk(path, tagCode, 0, FILE_LOAD_ALL, &actual);
    if (!ok || actual < 10) { Log(cnsl,"Failed to read tagcode file\n"); return -1; }

	LogFmt(cnsl,"Read file OK. Loaded \x02 elements\n", VecLength(tagCode));

    uint32_t startOfCode, startOfMem;
    if (!TCR_Read(tagCode, &startOfCode, &startOfMem)) {
        Log(cnsl,"Failed to read incoming byte code\n");
        return -1;
    }

    // Read symbols file --------------------------------
    Log(cnsl,"Trying to read symbol file\n");
    StringClear(path);
    StringAppend(path, "tagsymb.dat");
    HashMap* symbolMap = NULL;
    auto rawSymb = VecAllocate_char();

    ok = FileLoadChunk(path, rawSymb, 0, FILE_LOAD_ALL, &actual);
    if (!ok || actual < 10) { Log(cnsl,"Failed to read symbol file (ignoring)\n"); }
    else { symbolMap = TCR_ReadSymbols(rawSymb); }
    VecDeallocate(rawSymb);

    // Write a summary of the program to be executed ----------
    auto str = TCR_Describe(tagCode, symbolMap);
    LogLine(cnsl,str);
    StringDeallocate(str);
    StringDeallocate(path);
    
    // Prepare a runtime task ------------------------------------
    auto interp = InterpAllocate(tagCode, 1 MEGABYTE, symbolMap);

    // run a few cycles and print any output
    Log(cnsl,"Executing...\n");
    auto startTime = SystemTime();
    auto result = InterpRun(interp, 5000);
    while (result.State == ExecutionState::Paused) {
        str = StringEmpty();
        ReadOutput(interp, str);
        Log(cnsl,str);
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
	case ExecutionState::IPC_Send:
		StringAppend(str, "\r\nProgram wants to send '");
		StringAppend(str, result.IPC_Out_Target);
		StringAppendFormat(str, "' with \x02 bytes of data", VectorLength(result.IPC_Out_Data));
		break;
	case ExecutionState::IPC_Wait:
		{
			StringAppend(str, "\r\nProgram wants is waiting for IPC data: ");
			auto waits = InterpWaitingIPC(interp);
			StringPtr targ;
			while (VecPop_StringPtr(waits, &targ)) {
				StringAppend(str, targ);
				StringAppend(str, "; ");
			}
		}
		break;
    case ExecutionState::Running:
        StringAppend(str, "\r\nProgram still running?");
		resultState = 2;
        break;
	default:
		StringAppendFormat(str, "\r\nUNKNOWN STOP STATE \x03", (int)result.State);
		resultState = 3;
		break;
    }
    LogLine(cnsl,str);
    StringDeallocate(str);

	LogFmt(cnsl,"Execution took \x02 seconds\n", (endTime - startTime));
    size_t alloc;
    size_t unalloc;
    int objects;

    // Check memory state
    ArenaGetState(InterpInternalMemory(interp), &alloc, &unalloc, NULL, NULL, &objects, NULL);
	LogFmt(cnsl,"Runtime used in internal memory:\x02 bytes across \x02 objects. \x02 free.\n", alloc, objects, unalloc);
	
	// clean up
    InterpDeallocate(interp);
    MapDeallocate(symbolMap);
    VecDeallocate(tagCode);
    auto arena = MMCurrent();
    ArenaGetState(arena, &alloc, &unalloc, NULL, NULL, &objects, NULL);
	
	LogFmt(cnsl,"Runtime used in external memory:\x02 bytes across \x02 objects. \x02 free. (should all be zero)\n",
		alloc, objects, unalloc);

    return resultState;
}

int AppendFinishState (InterpreterState* is, ExecutionResult result, String* str) {
	int errState = 0;
	auto state = result.State;
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
	case ExecutionState::IPC_Send:
		StringAppend(str, "\r\nProgram wants to send '");
		StringAppend(str, result.IPC_Out_Target);
		StringAppendFormat(str, "' with \x02 bytes of data", VectorLength(result.IPC_Out_Data));
		break;
	case ExecutionState::IPC_Wait:
		{
			StringAppend(str, "\r\nProgram wants is waiting for IPC data: ");
			auto waits = InterpWaitingIPC(is);
			StringPtr targ;
			while (VecPop_StringPtr(waits, &targ)) {
				StringAppend(str, targ);
				StringAppend(str, "; ");
			}
		}
		break;

	default:
		StringAppendFormat(str, "\r\nUNKNOWN STOP STATE \x03", (int)result.State);
        break;
    }
	return errState;
}

int RunProgram(const char* filename) {
    // Load, compile and  run a program inside an arena

    size_t alloc, unalloc;
    int objects;
	LogFmt(cnsl,"########## Attempting program: \x05 #########\n", filename);

    MMPush(1 MEGABYTE); // this is arena is used to read out the results to the console. Most memory allocations should stay inside the interpreter.
    auto program = VecAllocate_DataTag();

    // Compile and load
    MMPush(10 MEGABYTES);
        auto code = StringEmpty();
        auto fileName = StringNew(filename);
        auto vec = StringGetByteVector(code);
        uint64_t read = 0;
        if (!FileLoadChunk(fileName, vec, 0, 10000, &read)) {
            Log(cnsl,"Failed to read file. Test inconclusive.\n");
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
        Log(cnsl,str);
        StringDeallocate(str);
        TraceArena(MMCurrent(), true);
        result = InterpRun(is, 5000);
    }
    auto endTime = SystemTime();

    TraceArena(MMCurrent(), false);

    auto str = StringEmpty();
	int errState = AppendFinishState(is, result, str);

    ReadOutput(is, str);
    LogLine(cnsl,str);
    StringDeallocate(str);
	LogFmt(cnsl,"Execution took \x02 seconds\n", (endTime - startTime));


    // Check memory state
    ArenaGetState(InterpInternalMemory(is), &alloc, &unalloc, NULL, NULL, &objects, NULL);
	LogFmt(cnsl,"Runtime used in internal memory:\x02 bytes across \x02 objects. \x02 free.\n", alloc, objects, unalloc);
    ArenaGetState(MMCurrent(), &alloc, &unalloc, NULL, NULL, &objects, NULL);
	LogFmt(cnsl,"Runtime used in external memory:\x02 bytes across \x02 objects. \x02 free.\n", alloc, objects, unalloc);

    InterpDeallocate(is);

    MMPop();
    return errState;
}

int TestProgramSuite() {
    // Run a range of programs.
    // These should be loaded into the 'jail' folder from the repo's samples

    int errs = 0;

#ifndef _DEBUG
    //errs += RunProgram("stressTest.ecs"); // really slow in debug mode
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
	
	LogFmt(cnsl,"########## Error count = \x02 #########\n", errs);
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
		Log(cnsl,"Failed to read file. Test inconclusive.\n");
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
	
    Log(cnsl,"***************** MULTIPLE RUNTIMES ******************\n");

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
			Log(cnsl,"1");
		}
		if (result1.State != ExecutionState::Paused && result1.State != ExecutionState::Waiting) run1 = false;
		
		if (run2) {
			result2 = InterpRun(prog2, 5);
			Log(cnsl,"2");
		}
		if (result2.State != ExecutionState::Paused && result2.State != ExecutionState::Waiting) run2 = false;
	}
	
    auto str = StringEmpty();
	StringAppend(str, "\r\n Program 1:\r\n");
	int errState = AppendFinishState(prog1, result1, str);
    ReadOutput(prog1, str);

	StringAppend(str, "\r\n Program 2:\r\n");
	errState += AppendFinishState(prog2, result2, str);
    ReadOutput(prog2, str);

    LogLine(cnsl,str);
    StringDeallocate(str);

	
    InterpDeallocate(prog1);
    InterpDeallocate(prog2);

	return 0;
}

int TestIPC() {
	int result = 0;
	
    Log(cnsl,"***************** INTERPROCESS MESSAGING ******************\n");

	
    auto consoleOut = StringEmpty();
	auto sched = RTSchedulerAllocate();

	RTSchedulerAddProgram(sched, StringNew("ipc_prog1.ecs"));
	RTSchedulerAddProgram(sched, StringNew("ipc_prog2.ecs"));

	int32_t safetyLatch = 50; // programs should complete in 2500 steps
	int faultLine = 0;
	while ((faultLine = RTSchedulerRun(sched, 50, consoleOut)) == 0) { // more rounds = less overhead, but coarser time slicing

		//std::cout << "\n#" << RTSchedulerLastProgramIndex(sched) << "\n";
		if (StringLength(consoleOut) > 0) {
			Log(cnsl,consoleOut);
			StringClear(consoleOut);
		}

		// system time is fun time
		if (--safetyLatch < 0) {
			Log(cnsl,"\n########## Schedule ran too long. Abandoning. ##########");
			break;
		}
	}

	if (StringLength(consoleOut) > 0) {
		LogLine(cnsl,consoleOut);
		StringClear(consoleOut);
	}

	// now check to see if there was an error
	auto endState = RTSchedulerState(sched);
	switch (endState) {
	case SchedulerState::Complete:
		Log(cnsl,"\nSchedule completed OK!\n");
		break;

	case SchedulerState::Faulted:
		LogFmt(cnsl,"\nSchedule encountered a fault; LINE = \x02\nIn program#\x02\n", faultLine, RTSchedulerLastProgramIndex(sched));
		RTSchedulerDebugDump(sched, consoleOut);
		LogLine(cnsl,consoleOut);
		StringClear(consoleOut);
		result = 1;
		break;

	case SchedulerState::Running:
		LogFmt(cnsl,"\nScheduler didn't finish running; \x02\n", faultLine);
		break;

	default:
		LogFmt(cnsl,"\nUNEXPECTED STATE: \x02\n", (int)endState);
		result = 2;
		break;
	}

	RTSchedulerDeallocate(&sched);
	StringDeallocate(consoleOut);

    size_t alloc, unalloc;
    int objects;
    ArenaGetState(MMCurrent(), &alloc, &unalloc, NULL, NULL, &objects, NULL);
	LogFmt(cnsl,"\nRuntime used in external memory:\x02 bytes across \x02 objects. \x02 free.\n", alloc, objects, unalloc);

	return result;
}



void ShowConsoleWindow() {
	// SETUP
	OutputScreen = DisplaySystem_Start(NewArena(10 MEGABYTE), 800, 600, 0x70, 0x70, 0x80);
	EventSystem_Start();

	cnsl = AttachConsole(OutputScreen, NULL);

	if (OutputScreen == NULL || cnsl == NULL) exit(1);

	LogLine(cnsl, "Welcome to MECS system");
	LogLine(cnsl, "A set of diagnostic tests will now run");
}

void CloseConsoleWindow() {
	DisplaySystem_Shutdown(OutputScreen);
}


int main() {
    auto suiteStartTime = SystemTime();

    StartManagedMemory();
    MMPush(1 MEGABYTE);
	ShowConsoleWindow();
    EventSystem_Start();
	
    auto aares = TestArenaAllocator();
    if (aares != 0) return aares;

    MMPush(10 MEGABYTE);
    auto vres = TestVector();
    if (vres != 0) return vres;
    MMPop();

    MMPush(10 MEGABYTE);
    auto qres = TestQueue();
    if (qres != 0) return qres;
    MMPop();

    MMPush(10 MEGABYTE);
    auto hmres = TestHashMap();
    if (hmres != 0) return hmres;
    MMPop();

    MMPush(10 MEGABYTE);
    auto tres = TestTree();
    if (tres != 0) return tres;
    MMPop();

    MMPush(10 MEGABYTE);
    auto sres = TestString();
    if (sres != 0) return sres;
    MMPop();

    MMPush(10 MEGABYTE);
    auto hres = TestHeaps();
    if (hres != 0) return hres;
    MMPop();

    MMPush(10 MEGABYTE);
    auto tagres = TestTagData();
    if (tagres != 0) return tagres;
    MMPop();
    
    MMPush(10 MEGABYTE);
    auto serdsres = TestSerialisation();
    if (serdsres != 0) return serdsres;
    MMPop();

    MMPush(10 MEGABYTE);
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
	
    MMPush(10 MEGABYTES);
    auto ipct = TestIPC();
    if (ipct != 0) return ipct;
    MMPop();

	
    auto suiteEndTime = SystemTime();

	LogFmt(cnsl,"\n\nTest suite finished in \x02s. Press any key to exit", (suiteEndTime - suiteStartTime));


    // Use the eventsys to wait for a key press

    MMPush(10 MEGABYTES);
    StringPtr eventTarget = StringNew("x");
    VectorPtr eventData = VectorAllocate(1);
    
    // Looking for a particular event type:
    while (!StringAreEqual(eventTarget, "keyboard")){
        // get any event
		while (!EventPoll(eventTarget, eventData)) {
			DisplaySystem_PumpIdle(OutputScreen);
		}
		Log(cnsl, eventTarget);
        char cdat = 0;
        while (VectorPop(eventData, &cdat)) {
            LogFmt(cnsl, "\x07", cdat);
        }

        // BUG: something in this loop is causing memory to run out.
    }
    MMPop();


    // wait again to inspect...
	char dummy;
    std::cout << "Input a character to quit: ";
	std::cin.get(dummy); 

	CloseConsoleWindow();
    ShutdownManagedMemory();
}

