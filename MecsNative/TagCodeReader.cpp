#include "TagCodeReader.h"

#define BYTE unsigned char

RegisterHashMapStatics(Map)
RegisterHashMapFor(int, StringPtr, HashMapIntKeyHash, HashMapIntKeyCompare, Map)

RegisterVectorStatics(Vec)
RegisterVectorFor(DataTag, Vec)
RegisterVectorFor(BYTE, Vec)
RegisterVectorFor(char, Vec)


void TCR_Swizzle(Vector* v, int i) {
    BYTE* res = NULL;
    DataTag swiz = {};
    BYTE* tag = (BYTE*)VectorGet(v, i);

    // re-order the bytes using math (implicitly machine order)
    swiz.type = tag[0]; // single byte
    swiz.params = (tag[1] << 16) | (tag[2] << 8) | (tag[3]);
    swiz.data = (tag[4] << 24) | (tag[5] << 16) | (tag[6] << 8) | (tag[7]);

    // TODO: need to skip the string table, but not the length headers

    res = (BYTE*)&swiz;
    for (int j = 0; j < 8; j++) {
        tag[j] = res[j];
    }
}

bool TCR_FixByteOrder(Vector* v) {
    if (v == NULL) return false;
    int length = VectorLength(v);
    if (length < 1) return false;
    char codeClass, codeAction;
    uint32_t offset;

    // Check we haven't already corrected:
    DecodeLongOpcode(*VecGet_DataTag(v, 0), &codeClass, &codeAction, &offset);
    if (codeClass == 'c' && codeAction == 's') { return true; }
    
    // Correct and read the header
    TCR_Swizzle(v, 0); // Get the string jump

    DecodeLongOpcode(*VecGet_DataTag(v, 0), &codeClass, &codeAction, &offset);

    if (codeClass != 'c' || codeAction != 's') {
        return false; // not valid
    }

    // Jump through the string table, swizzling only the header values. The strings are in byte order
    int i = 1;
    while (i < offset) {
        TCR_Swizzle(v, i); // this should be a string head
        auto raw = *VecGet_DataTag(v, i);
        i++;

        if (raw.type != (int)DataType::Integer) {
            return false; // string table header is wrong -- should be an integer op-code
        }
        auto len = DecodeInt32(raw);
        auto step = (len / 8) + ((len % 8 == 0) ? 0 : 1);

        i += step;
    }

    // swizzle every opcode after the string table
    for (i = offset + 1; i < length; i++) {
        TCR_Swizzle(v, i); 
    }
    return true;
}

String* DecodeString(Vector* data, int position, int length) {
    int block = 0;
    char c = 0;

    int chunk = position;
    auto str = StringEmpty();
    char* raw = (char*)VecGet_DataTag(data, chunk);
    while (length-- > 0) {
        c = *raw;
        if (c == 0) StringAppendChar(str, '$');
        else StringAppendChar(str, *raw);

        raw++;
        block++;

        if (block > 7) {
            block = 0;
            chunk++;
            raw = (char*)VecGet_DataTag(data, chunk);
        }
    }
    return str;
}

String* TCR_Describe(Vector* data, HashMap* symbols) {
    if (data == NULL) return NULL;
    if (!TCR_FixByteOrder(data)) return NULL;

    // Read first code, and handle the string table specially

    char codeClass, codeAction;
    uint32_t offset;
    DecodeLongOpcode(*VecGet_DataTag(data, 0), &codeClass, &codeAction, &offset);

    if (codeClass != 'c' || codeAction != 's') {
        return StringNew("Invalid file: TagCode did not start with a data header.\n");
    }

    auto tagStr = StringEmpty();

    // Write string table (using string lengths to hop)
    int i = 1;
    while (i < offset) {
        auto raw = *VecGet_DataTag(data, i);
        i++;

        if (raw.type != (int)DataType::Integer) { // string table header is wrong
            StringAppendFormat(tagStr, "    - type error at \x02\n", i);
            break;
        }
        auto len = DecodeInt32(raw);
        auto step = (len / 8) + ((len % 8 == 0) ? 0 : 1);

        if (step > 0) {
            auto str = DecodeString(data, i, len);
            StringAppendFormat(tagStr, "    \x02: (\x02) [[\x01]]\n", i-1, len, str);
            StringDeallocate(str);
            i += step;
        } else {
            StringAppendFormat(tagStr, "    \x02: (\x02) <empty>\n", i-1, len);
        }
    }

    // Output the opcodes
    int opCount = VecLength(data);
    for (i = offset + 1; i < opCount; i++) {
        StringAppendFormat(tagStr, "\x02  ", i);
        DataTag opcode = *VecGet_DataTag(data, i);
        DescribeTag(opcode, tagStr, symbols);
        StringAppendChar(tagStr, '\n');
    }
    return tagStr;
}

bool GetUint32(Vector* v, uint32_t* outVal) {
    if (outVal == NULL) return false;

    uint8_t a, b, c, d;
    if (!VecDequeue_BYTE(v, &a)) return false;
    if (!VecDequeue_BYTE(v, &b)) return false;
    if (!VecDequeue_BYTE(v, &c)) return false;
    if (!VecDequeue_BYTE(v, &d)) return false;

    *outVal = (a << 24) | (b << 16) | (c << 8) | (d);
}

// Build a Map<uint32 -> string> reading from a BYTE vector
// This should also add the default (built-in) symbols
HashMap* TCR_ReadSymbols(Vector* v) {
    auto outp = MapAllocate_int_StringPtr(256);

    uint32_t crush;
    uint32_t strLength;
    char c;
    for (;;) {
        if (!GetUint32(v, &crush)) break;
        if (!GetUint32(v, &strLength)) break;

        if (strLength < 1) continue;
        auto str = StringEmpty();
        while (strLength-- > 0) {
            if (!VecDequeue_char(v, &c)) break;
            StringAppendChar(str, c);
        }

        MapPut_int_StringPtr(outp, crush, str, true);
    }

    // the default symbols
    // TODO: put these and the ones in the interpreter somewhere central
#define add(name)  MapPut_int_StringPtr(outp, GetCrushedName(name), StringNew(name), true);
    // This should be kept in sync with TagCodeInterpreter.cpp -> AddBuiltInFunctionSymbols()
    add("="); add("equals"); add(">"); add("<"); add("<>"); add("not-equal");
    add("assert"); add("random"); add("eval"); add("call"); add("not"); add("or");
    add("and"); add("readkey"); add("readline"); add("print"); add("substring");
    add("length"); add("replace"); add("concat"); add("+"); add("-"); add("*");
    add("/"); add("%"); add("()"); add("new-list"); add("push"); add("pop"); add("dequeue");
#undef add;

    return outp;
}



bool TCR_Read(Vector* v, uint32_t* outStartOfCode, uint32_t* outStartOfMemory) {
    if (v == NULL || outStartOfCode == NULL || outStartOfMemory == NULL) return false;
    if (!TCR_FixByteOrder(v)) return false;


    DecodeLongOpcode(*VecGet_DataTag(v, 0), NULL, NULL, outStartOfCode);
    *outStartOfMemory = VecLength(v); // the vector can be extended by the interpreter-- this represents working memory

    return true;
}