#include "TagCodeInterpreter.h"

#include "MemoryManager.h"
#include "HashMap.h"
#include "Scope.h"
#include "TagCodeFunctionTypes.h"
#include "TagCodeReader.h"
#include "TypeCoersion.h"
#include "MathBits.h"
#include "Serialisation.h"

// required only for 'eval'
#include "SourceCodeTokeniser.h"
#include "CompilerCore.h"

#define CODE_POS_ERR_STR(s)  __FILE__ s
// ^ use this like:  StringAppendFormat(is->_output, CODE_POS_ERR_STR("; Line \x02 - My error description"), __LINE__ );

#ifndef abs
#define abs(x)  ((x < 0) ? (-(x)) : (x))
#endif
typedef uint32_t Name;

RegisterHashMapStatics(Map)
RegisterHashMapFor(Name, StringPtr, HashMapIntKeyHash, HashMapIntKeyCompare, Map)
RegisterHashMapFor(Name, FunctionDefinition, HashMapIntKeyHash, HashMapIntKeyCompare, Map)
RegisterHashMapFor(StringPtr, DataTag, HashMapStringKeyHash, HashMapStringKeyCompare, Map)
RegisterHashMapFor(StringPtr, VectorPtr, HashMapStringKeyHash, HashMapStringKeyCompare, Map)
RegisterHashMapFor(StringPtr, bool, HashMapStringKeyHash, HashMapStringKeyCompare, Map)

RegisterVectorStatics(Vec)
RegisterVectorFor(DataTag, Vec)
RegisterVectorFor(int, Vec)
RegisterVectorFor(char, Vec)
RegisterVectorFor(VectorPtr, Vec)
RegisterVectorFor(StringPtr, Vec)
RegisterVectorFor(HashMap_KVP, Vec)

// the smallest difference considered for float equality
const float ComparisonPrecision = 1e-10;

/*
    Our interpreter is a two-stack model
*/
typedef struct InterpreterState {
    Vector* _valueStack; // Stack<DataTag>
    Vector* _returnStack; // Stack<int>

    String* _input;
    String* _output;

	// The last known state of the interpreter. This is used for IPC and signalling errors.
    // If the value is `ErrorState`, the interpreter should stop as soon as it can.
	ExecutionState State;

    // the string table and opcodes
    Vector* _program; // Vector<DataTag> (read only)
    Scope* _variables; // scoped variable references
    Arena* _memory; // read/write memory (for non-short strings and other 'heap' containers)

    // Functions that have been defined at run-time
    HashMap* Functions; // Map<CrushName -> FunctionDefinition>
    HashMap* DebugSymbols; // Map<CrushName -> String>

	// Inter-Program Communication
	HashMap* IPC_Queues; // Map<TargetName -> Vector< datavec > >; where datavec is Vector<byte>
	HashMap* IPC_Queue_WaitFlags; // Map<TargetName -> bool>; `true` means the interpreter is waiting for this message

    // number of byte codes interpreted (also used for random number generation)
    int _stepsTaken;
    // if `true`, write lots of output
    bool _runningVerbose;
    // the PC. This is in TOKEN positions, not bytes
    int _position;
} InterpreterState;


DataTag _Exception(InterpreterState* is, const char* msg) {
    StringAppend(is->_output, msg);
    return RuntimeError(is->_position);
}
DataTag _Exception(InterpreterState* is, const char* msg, String* details) {
    StringAppend(is->_output, msg);
    StringAppend(is->_output, details);
    return RuntimeError(is->_position);
}

Arena* InterpInternalMemory(InterpreterState* is) {
    return is->_memory;
}

// map built-in functions for the massive switch
void AddBuiltInFunctionSymbols(HashMap* fd) {
    if (fd == NULL) return;
#define add(name,type)  MapPut_Name_FunctionDefinition(fd, GetCrushedName(name), FunctionDefinition{type}, true);
    // this should be kept in sync with TagCodeReader.cpp -> MapPut_int_StringPtr()
    add("=", FuncDef::Equal); add("equals", FuncDef::Equal); add(">", FuncDef::GreaterThan);
    add("<", FuncDef::LessThan); add("<>", FuncDef::NotEqual); add("not-equal", FuncDef::NotEqual);
    add("not", FuncDef::LogicNot); add("or", FuncDef::LogicOr); add("and", FuncDef::LogicAnd);

    add("assert", FuncDef::Assert); add("random", FuncDef::Random); add("eval", FuncDef::Eval);
    add("call", FuncDef::Call);

    add("readkey", FuncDef::ReadKey); add("readline", FuncDef::ReadLine);
    add("print", FuncDef::Print); add("substring", FuncDef::Substring);
    add("length", FuncDef::Length); add("replace", FuncDef::Replace); add("concat", FuncDef::Concat);

    add("+", FuncDef::MathAdd); add("-", FuncDef::MathSub); add("*", FuncDef::MathProd);
    add("/", FuncDef::MathDiv); add("%", FuncDef::MathMod);
    
    add("new-map", FuncDef::NewMap); 
    add("new-list", FuncDef::NewList); add("push", FuncDef::Push);
    add("pop", FuncDef::Pop); add("dequeue", FuncDef::Dequeue);

	add("listen", FuncDef::Listen); add("wait", FuncDef::Wait); add("send", FuncDef::Send);

    add("()", FuncDef::UnitEmpty); // empty value marker
#undef add;
}

// Start up an interpreter
// tagCode is Vector<DataTag>, debugSymbols in Map<CrushName -> StringPtr>.
InterpreterState* InterpAllocate(Vector* tagCode, size_t memorySize, HashMap* debugSymbols) {
    if (tagCode == NULL) return NULL;
    auto memory = NewArena(memorySize);
    
    auto result = (InterpreterState*)ArenaAllocateAndClear(memory, sizeof(InterpreterState));
    if (result == NULL) return NULL;

    result->_memory = memory;

    result->Functions = MapAllocateArena_Name_FunctionDefinition(100, result->_memory);
    result->DebugSymbols = debugSymbols; // ok if NULL
    result->State = ExecutionState::Paused;
    result->_variables = ScopeAllocate(memory);

    //result->_program = tagCode;
    // Copy program into this interpreter (non-destructively)
    result->_program = VecClone(tagCode, memory);

    result->_position = 0;
    result->_stepsTaken = 0;
    result->_runningVerbose = false;

    result->_returnStack = VecAllocateArena_int(result->_memory);
    result->_valueStack = VecAllocateArena_DataTag(result->_memory);

    result->_input = StringEmptyInArena(result->_memory);
    result->_output = StringEmptyInArena(result->_memory);

    if ((result->Functions == NULL)
        || (result->_returnStack == NULL)
        || (result->_valueStack == NULL)
        || (result->_input == NULL)
        || (result->_variables == NULL)
        || (result->_memory == NULL)
        || (result->_output == NULL)) {
        InterpDeallocate(result);
        return NULL;
    }

    AddBuiltInFunctionSymbols(result->Functions);

    return result;
}

// Close down an interpreter and free all memory (except input tagCode and debugSymbols)
void InterpDeallocate(InterpreterState* is) {
    if (is == NULL) return;
    
    // All the allocations should be contained in the interpreter state's internal arena.
    if (is->_memory != NULL) {
        auto mr = is->_memory;
        DropArena(&mr);
    }
}

// Add string data to the waiting input stream
void WriteInput(InterpreterState* is, String* str) {
    if (is == NULL || str == NULL) return;
    StringAppend(is->_input, str);
}

// Move output data to the supplied string
void ReadOutput(InterpreterState* is, String* receiver) {
    if (is == NULL || receiver == NULL) return;

    int limit = StringLength(is->_output);
    char c;
    for (int i = 0; i < limit; i++) {
        c = StringDequeue(is->_output);
        if (c != 0) StringAppendChar(receiver, c);
    }
}

// pop items from the interpreter program until we see either EndOfProgram or EndOfSubProgram
int RollBackSubProgram(InterpreterState* is) {
    DataTag tag = {};
    int rollBackCount = 0;

    bool found = VecPeek_DataTag(is->_program, &tag);
    // sanity check:
    if (!found || tag.type != (int)DataType::EndOfSubProgram) {
        StringAppend(is->_output, "Tried to rollback a sub program, but failed\n");
        return rollBackCount;
    }

    /*StringAppend(is->_output, "   /\\ "); // reverse order!!!
    DescribeTag(tag, is->_output, NULL);
    StringAppend(is->_output, "\r\n");*/

    int s = VecLength(is->_program);

    while (true) {
        rollBackCount++;
        if (!VecPop_DataTag(is->_program, NULL)) {
			is->State = ExecutionState::ErrorState;
            StringAppend(is->_output, "Tried to rollback a sub program. Never found the end marker.\n");
            return rollBackCount;
        }
        found = VecPeek_DataTag(is->_program, &tag);

        /*StringAppend(is->_output, "   /\\ "); // reverse order!!!
        DescribeTag(tag, is->_output, NULL);
        StringAppend(is->_output, "\r\n");*/

        if (!found
            || tag.type == (int)DataType::EndOfSubProgram
            || tag.type == (int)DataType::EndOfProgram) {
            return rollBackCount;
        }
    }
}

ExecutionResult FailureResult(uint32_t position) {
    ExecutionResult r = {};
    r.Result = RuntimeError(position);
    r.State = ExecutionState::ErrorState;
    return r;
}
ExecutionResult CompleteExecutionResult(DataTag retVal) {
    ExecutionResult r = {};
    r.Result = retVal;
    r.State = ExecutionState::Complete;
    return r;
}
ExecutionResult PausedExecutionResult() {
    ExecutionResult r = {};
    r.Result = NonResult();
    r.State = ExecutionState::Paused;
    return r;
}
ExecutionResult WaitingExecutionResult() {
    ExecutionResult r = {};
    r.Result = NonResult();
    r.State = ExecutionState::Waiting;
    return r;
}
ExecutionResult IPCWaitExecutionResult() {
    ExecutionResult r = {};
    r.Result = NonResult();
    r.State = ExecutionState::IPC_Wait;
    return r;
}
ExecutionResult IPCSendExecutionResult() {
    ExecutionResult r = {};
    r.Result = NonResult();
    r.State = ExecutionState::IPC_Send;
    return r;
}

String* DbgStr(InterpreterState* is, uint32_t hash)
{
    if (is->DebugSymbols == NULL) {
        return StringNewFormat("\x03", hash);
    }

    StringPtr *symbolName = NULL;
    if (!MapGet_Name_StringPtr(is->DebugSymbols, hash, &symbolName)) {
        return StringNewFormat("<unknown> \x03", hash);
    }
    return StringNewFormat("\x01 (\x03)", *symbolName, hash);
}

String* DiagnosticString(DataTag tag, InterpreterState* is) {
    // TODO: Diagnostic string
    return StringEmptyInArena(is->_memory);
}

DataTag* ReadParams(InterpreterState* is, uint16_t nbParams){
    DataTag* param = (DataTag*)ArenaAllocate(is->_memory, nbParams * sizeof(DataTag));
    if (param == NULL) return NULL;

    
    // Pop values from stack into a param cache, putting into source-code order
    for (int i = nbParams - 1; i >= 0; i--) {
        if (!VecPop_DataTag(is->_valueStack, &(param[i]))) {
			is->State = ExecutionState::ErrorState;
            StringAppendFormat(is->_output, "\nValue stack underflow at position \x03 (\x02)\n", is->_position, is->_position);
            return NULL;
        }

        // TODO: hunt down where invalid values are coming in.
        // Either fix, or replace with NaR. Then put this code back in.
        if (param[i].type == 0) { // invalid value!
			is->State = ExecutionState::ErrorState;
            // this can happen if we have an unresolved name. Should be handled earlier.
            StringAppendFormat(is->_output, "\nInvalid value in parameters! Found when calling at position \x03 (\x02)\n", is->_position, is->_position);
            //StringAppendFormat(is->_output, "Param index p \x02; Data = \x03", i, param[i].data);
            return NULL;
        }
    }

    return param;
}

// Defined at bottom
DataTag EvaluateBuiltInFunction(int* position, FuncDef kind, int nbParams, DataTag* param, InterpreterState* is);

// Read a value out of a container based on index parameters
DataTag DecomposeContainer(InterpreterState* is, DataTag container, int nbParams, DataTag* param) {
    if (nbParams != 1) return NonResult();

    switch (container.type) {
    case (int)DataType::VectorPtr:
    {
        // Return an encoding for the index. We don't actually resolve anything from the vector at this point (a get/set or use will do that)
        return VectorIndexTag(container.data, CastInt(is, param[0]));
    }
    case (int)DataType::HashtablePtr:
    {
        // get table and key;
        // look up pointer to value
        // map to arena pointer
        
        auto offset = DecodePointer(container);
        auto src = (HashMap*)ArenaOffsetToPtr(is->_memory, offset);
        if (src == NULL) return NonResult();

        auto key = CastString(is, param[0]);
        
        DataTag* ptr = NULL;
        auto found = HashMapGet(src, &key, (void**)&ptr);
        StringDeallocate(key);

        if (!found) return NonResult();

        auto value_offset = ArenaPtrToOffset(is->_memory, ptr);
        if (value_offset < 1) return NonResult();
        return HashTableValue(value_offset);
    }
    default: return NonResult();
    }
}

// Try to resolve a variable name being called as if it were a function
// for vectors and hashmaps, this gets a proxy to the held value (or a NaR)
// for everything else, it's an error
DataTag ResolveValueAsFunction(InterpreterState* is, uint32_t functionNameHash, int nbParams, DataTag* param) {
    auto tag = ScopeResolve(is->_variables, functionNameHash);

	return DecomposeContainer(is, tag, nbParams, param);
}

// Check to see if the tag is a vector-index or hash-entry.
// if so, we resolve it to the stored value. Otherwise, we do nothing.
// returns true iff an index was resolved
bool ResolveIndexIfRequired(InterpreterState* is, DataTag* tag) {
    if (tag == NULL) return false;
    switch (tag->type) {
    case (int)DataType::VectorIndex:
    {
        // dereference the vector index, and call CastInt again:
        auto idx = tag->params; // grab the index

        auto offset = DecodePointer(*tag);
        auto src = (Vector*)ArenaOffsetToPtr(is->_memory, offset);
        if (src == NULL) {// broken reference. Switch value to NaR
            tag->type = (int)DataType::Not_a_Result;
            tag->params = 0;
            tag->data = 0;
            return false; 
        }

        auto newtag = VecGet_DataTag(src, idx);
        if (tag == NULL) {// broken reference. Switch value to NaR
            tag->type = (int)DataType::Not_a_Result;
            tag->params = 0;
            tag->data = 0;
            return false;
        }

        tag->type = newtag->type;
        tag->params = newtag->params;
        tag->data = newtag->data;
        return true;
    }

    case (int)DataType::HashtableEntryPtr:
    {
        // unpack pointer to tag into the actual tag
        auto realTag = (DataTag*)InterpreterDeref(is, *tag);
        tag->type = realTag->type;
        tag->params = realTag->params;
        tag->data = realTag->data;
        return true;

    }

    default: return false;
    }
}

// returns true if the tag is a vector-index or hash-entry.
bool IsContainerIndex(DataTag tag) {
    return (tag.type == (int)DataType::VectorIndex || tag.type == (int)DataType::HashtableEntryPtr);
}

// returns true if the tag is a vector or hash-map.
bool IsContainerType(DataTag tag) {
    return (tag.type == (int)DataType::VectorPtr || tag.type == (int)DataType::HashtablePtr);
}

inline DataTag EvaluateFunctionCall(int* position, uint32_t functionNameHash, int nbParams, DataTag* param, InterpreterState* is) {
    FunctionDefinition* fun = NULL;
    
    if (!MapIsValid(is->Functions)) {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Function hash map has been damaged at position \x02\n", *position);
        return RuntimeError(is->_position);
    }

    bool found = MapGet_Name_FunctionDefinition(is->Functions, functionNameHash, &fun);

    if (!found) { // No function with that name (neither built-in or runtime-defined)
        // It could be a variable de-ref (for vector or hash), or a true error
        if (ScopeCanResolve(is->_variables, functionNameHash)) {
            auto result = ResolveValueAsFunction(is, functionNameHash, nbParams, param);
            if (result.type != 0) return result;
        }

        // Nope, just an error
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output,
            "Tried to call an undefined function '\x01' at position \x02\n", DbgStr(is, functionNameHash), *position);
        return RuntimeError(is->_position);
        // TODO: "Did you mean?"
        /*
        throw new Exception("Tried to call an undefined function '"
            + DbgStr(functionNameHash)
            + "' at position " + position
            + "\r\nKnown functions: " + string.Join(", ", Functions.Keys.Select(DbgStr))
            + "\r\nAs a string: " + TryDeref(functionNameHash) + "?"
            + "\r\nKnown symbols: " + string.Join(", ", DebugSymbols.Keys.Select(DbgStr)));*/
    }

    if (fun->Kind != FuncDef::Custom)
        return EvaluateBuiltInFunction(position, fun->Kind, nbParams, param, is);

    // handle functions that are defined in the program
    ScopePush(is->_variables, param, nbParams); // write parameters into new scope
    VecPush_int(is->_returnStack, *position); // set position for 'cret' call
    *position = fun->StartPosition; // move pointer to start of function
    return VoidReturn(); // return no value, continue execution elsewhere

}


// List equals follows the logic: `true` if *any* of the values is equal to the first, `false` otherwise.
bool ListEquals(int nbParams, DataTag* param, InterpreterState* is) {
    if (nbParams < 1) return false;
    auto type = param[0].type;
    switch (type) {
        // Non-comparable types
    case (int)DataType::Invalid:
    case (int)DataType::Not_a_Result:
    case (int)DataType::Exception:
    case (int)DataType::Void:
    case (int)DataType::Unit:
    case (int)DataType::Opcode:
        return false;

        // Numeric types
    case (int)DataType::Integer:
    case (int)DataType::Fraction:
    {
        auto target = CastDouble(is, param[0]);
        for (int i = 1; i < nbParams; i++) {
            auto diff = target - CastDouble(is, param[i]);
            if (abs(diff) <= ComparisonPrecision) return true;
        }
        return false;
    }

    // String types
    case (int)DataType::SmallString:
    case (int)DataType::StaticStringPtr:
    case (int)DataType::StringPtr:
    {
        auto target = CastString(is, param[0]);
        for (int i = 1; i < nbParams; i++) {
            auto pair = CastString(is, param[i]);
            bool match = StringAreEqual(target, pair);
            StringDeallocate(pair);
            if (match) {
                StringDeallocate(target);
                return true;
            }
        }
        return false;
    }

    // Pointer equality
    case (int)DataType::VariableRef:
    case (int)DataType::HashtablePtr:
    case (int)DataType::VectorPtr:
    {
        auto target = param[0];
        for (int i = 1; i < nbParams; i++) {
            if (target.type == param[i].type && target.data == param[i].data) return true;
        }
        return false;
    }

    default:
        return false;
    }
}

// True iff each element is larger than the previous
bool FoldGreaterThan(int nbParams, DataTag* param, InterpreterState* is) {
    bool first = true;
    double prev = 0;
    for (int i = 0; i < nbParams; i++) {
        auto encoded = param[i];
        if (first)
        {
            prev = CastDouble(is, encoded);
            first = false;
            continue;
        }

        auto current = CastDouble(is, encoded);
        if (prev <= current) return false; // inverted for short-circuit
        prev = current;
    }

    return true;
}

// True iff each element is smaller than the previous
bool FoldLessThan(int nbParams, DataTag* param, InterpreterState* is) {
    bool first = true;
    double prev = 0;
    for (int i = 0; i < nbParams; i++) {
        auto encoded = param[i];
        if (first)
        {
            prev = CastDouble(is, encoded);
            first = false;
            continue;
        }

        auto current = CastDouble(is, encoded);
        if (prev >= current) return false; // inverted for short-circuit
        prev = current;
    }

    return true;
}

// Try to read a tag from the value stack
inline DataTag TryPopFromValueStack(InterpreterState* is, int position) {
    DataTag tag;
    if (!VecPop_DataTag(is->_valueStack, &tag)) {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Value stack underflow at position \x02", position);
        return InvalidTag();
    }
    return tag;
}

void DoIndexedGet(InterpreterState* is, uint32_t varRef, uint16_t paramCount) {
    auto value = ScopeResolve(is->_variables, varRef);

    switch (value.type)
    {
    case (int)DataType::StringPtr:
    case (int)DataType::StaticStringPtr:
    {
        // get the other indexes. If more than one, build a string out of the bits?
        // out-of-range is ignored
        auto src = CastString(is, value);
        auto srcLength = StringLength(src);
        auto dst = StringEmptyInArena(is->_memory);
        auto indexes = VecAllocateArena_int(is->_memory);

        for (int i = 0; i < paramCount; i++) { // get indexes from stack order to index order
            auto idx = CastInt(is, TryPopFromValueStack(is, is->_position));
            if (idx >= 0 && idx < srcLength) VecPush_int(indexes, idx);
        }

        int idx = 0;
        while (VecPop_int(indexes, &idx)) {
            char c = StringCharAtIndex(src, idx);
            if (idx >= 0 && idx < srcLength) StringAppendChar(dst, c);
        }
        VecDeallocate(indexes);

        auto result = StoreStringAndGetReference(is, dst);
        StringDeallocate(src);
        VecPush_DataTag(is->_valueStack, result);
        return;
    }

    case (int)DataType::VectorPtr:
    {
        
        if (paramCount < 1) { // Compiler Error: indexed get with no indexes.
			is->State = ExecutionState::ErrorState;
            StringAppendFormat(is->_output, "Compiler error? Tried to get a vector entry with no indicies. Position: '\x02'", is->_position);
        }

        auto src = (Vector*)InterpreterDeref(is, value);
        if (src == NULL) {
            VecPush_DataTag(is->_valueStack, NonResult());
            return;
        }
        auto srcLength = VecLength(src);
        // if we've asked for one index, we return the value directly:
        if (paramCount == 1) {
            auto idx = CastInt(is, TryPopFromValueStack(is, is->_position));
            if (idx >= 0 && idx < srcLength) VecPush_DataTag(is->_valueStack, *VecGet_DataTag(src, idx));
            return;
        }

        // if more than one, we build a new vector:
        auto list = VecAllocateArena_DataTag(is->_memory);
        auto indexes = VecAllocateArena_int(is->_memory);

        // get indexes from stack order to index order
        for (int i = 0; i < paramCount; i++) {
            auto idx = CastInt(is, TryPopFromValueStack(is, is->_position));
            if (idx >= 0 && idx < srcLength) VecPush_int(indexes, idx);
        }

        // Pick source indexes and copy into output list
        int idx = 0;
        while (VecPop_int(indexes, &idx)) {
            if (idx >= 0 && idx < srcLength) {
                VecPush_DataTag(list, *VecGet_DataTag(src, idx));
            }
        }

        uint32_t encPtr = ArenaPtrToOffset(is->_memory, list); // allows us to place a 32-bit ptr in 64-bit space
        if (encPtr < 1) { // nonsense result from arena allocation
			is->State = ExecutionState::ErrorState;
            VecPush_DataTag(is->_valueStack, _Exception(is, "Arena mapping failed in indexed get from list"));
            return;
        }

        VecPush_DataTag(is->_valueStack, EncodePointer(encPtr, DataType::VectorPtr));
        return;
    }

    case (int)DataType::HashtablePtr:
    {
        if (paramCount < 1) { // Compiler Error: indexed get with no indexes.
			is->State = ExecutionState::ErrorState;
            StringAppendFormat(is->_output, "Compiler error? Tried to get a hash entry with no keys. Position: '\x02'", is->_position);
        }

        auto src = (HashMap*)InterpreterDeref(is, value);
        if (src == NULL) {
            StringAppend(is->_output, "Failed to read target hashmap during an index get");
            VecPush_DataTag(is->_valueStack, NonResult());
            return;
        }

        // if we've asked for one index, we return the value directly:
        if (paramCount == 1) {
            auto key = CastString(is, TryPopFromValueStack(is, is->_position));
            //StringAppendFormat(is->_output, "Indexing hashmap with '\x01' as the key\n", key);
            DataTag *tag = NULL;
            if (!MapGet_StringPtr_DataTag(src, key, &tag) || tag == NULL) {
                // no such key or value
                VecPush_DataTag(is->_valueStack, NonResult());
                return;
            }
            VecPush_DataTag(is->_valueStack, *tag);
            StringDeallocate(key);
            return;
        }

        // More than one key. We build a new vector from the lookups. Any missing keys are not added.
        
        auto list = VecAllocateArena_DataTag(is->_memory);

        // get indexes from stack order to index order
        for (int i = 0; i < paramCount; i++) {
            auto key = CastString(is, TryPopFromValueStack(is, is->_position));
            //StringAppendFormat(is->_output, "Indexing hashmap with '\x01' as the key\n", key);
            DataTag *tag = NULL;
            if (!MapGet_StringPtr_DataTag(src, key, &tag) || tag == NULL) {
                // no such key or value
                continue;
            }
            VecPush_DataTag(list, *tag);
            StringDeallocate(key);
        }

        // vector is in reverse order due to stack, so flip it
        VecReverse(list);

        uint32_t encPtr = ArenaPtrToOffset(is->_memory, list); // allows us to place a 32-bit ptr in 64-bit space
        if (encPtr < 1) { // nonsense result from arena allocation
			is->State = ExecutionState::ErrorState;
            VecPush_DataTag(is->_valueStack, _Exception(is, "Arena mapping failed in indexed get from hash table"));
            return;
        }

        VecPush_DataTag(is->_valueStack, EncodePointer(encPtr, DataType::VectorPtr));

        return;
    }

    default:

		is->State = ExecutionState::ErrorState;
        auto info = DbgStr(is, varRef);
        StringAppendFormat(is->_output, "Tried to index the wrong kind of thing (\x01). position: '\x02'", info, is->_position);
        StringDeallocate(info);
    }
}

void DoIndexedSet(InterpreterState* is, uint32_t varRef, uint16_t paramCount) {
    /*
this code:
    set(fullList(1) 3)

compiles to this:
    1  Integer number [1]
    2  Integer number [3]
    3  VariableNameRef 'fullList' [8B563F61]
    4  Opcode mS[00020000]
    */

    if (paramCount != 2) {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Index set with wrong number of parameters: \x02 at position '\x02'", paramCount, is->_position);
        return;
    }
    auto valueToSet = TryPopFromValueStack(is, is->_position);
    ResolveIndexIfRequired(is, &valueToSet); // if this is an index reference, resolve it before continuing
    auto indexValue = TryPopFromValueStack(is, is->_position);
    
    auto container = ScopeResolve(is->_variables, varRef);

    switch (container.type)
    {
    case (int)DataType::VectorPtr:
    {
        auto src = (Vector*)InterpreterDeref(is, container);
        if (src == NULL) {
            return;
        }
        auto srcLength = VecLength(src);
        auto idx = CastInt(is, indexValue);
        if (idx >= 0 && idx < srcLength) {
            // all ok, set the value
            VecSet_DataTag(src, idx, valueToSet, NULL);
        }

        return;
    }
    case (int)DataType::HashtablePtr:
    {
        auto src = (HashMap*)InterpreterDeref(is, container);
        if (src == NULL) {
            return;
        }
        auto key = CastString(is, indexValue); // this string becomes owned by the hashmap, so we don't deallocate it.
        auto ok = MapPut_StringPtr_DataTag(src, key, valueToSet, true);
        return;
    }
    default:

		is->State = ExecutionState::ErrorState;
        auto info = DbgStr(is, varRef);
        StringAppendFormat(is->_output, "Tried to set-by-index on the wrong kind of thing (\x01). position: '\x02'", info, is->_position);
        StringDeallocate(info);
    }
    // so var stack should have the target, then the value, then indexes in reverse order (should be exactly 1 at the moment)
}

inline DataType PrepareFunctionCall(int* position, uint32_t nameHash, uint16_t nbParams, InterpreterState* is) {
    auto functionNameHash = nameHash;

    auto param = ReadParams(is, nbParams);
    if (param == NULL) {
		is->State = ExecutionState::ErrorState;
        VecPush_DataTag(is->_valueStack, RuntimeError(is->_position));
        return DataType::Exception;
    }

    // Evaluate function.
    DataTag evalResult = EvaluateFunctionCall(position, functionNameHash, nbParams, param, is);
    ArenaDereference(is->_memory, param);

    if (evalResult.type == (int)DataType::Exception) {
		is->State = ExecutionState::ErrorState;
        VecPush_DataTag(is->_valueStack, evalResult);
        return DataType::Exception;
    }
    if (evalResult.type == (int)DataType::MustWait) {
        return DataType::MustWait;
    }

    if (evalResult.type == 0) { // something gave a totally invalid result
		is->State = ExecutionState::ErrorState;
        VecPush_DataTag(is->_valueStack, RuntimeError(is->_position));
        return DataType::Exception;
    }

    // Add result on stack as a value.
    if (evalResult.type != (int)DataType::Void) { // NaR is added so it can propagate
        VecPush_DataTag(is->_valueStack, evalResult);
    }

    return (DataType)evalResult.type;
}

void HandleFunctionDefinition(int* position, uint16_t argCount, uint16_t tokenCount, InterpreterState* is) {
    if (is == NULL || position == NULL) { return; }

    DataTag tag;
    if (!VecPop_DataTag(is->_valueStack, &tag)) {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Value stack underflow during function definition at position \x02", *position);
        return;
    }
    auto functionNameHash = DecodeVariableRef(tag);

    FunctionDefinition* original;
    if (MapGet_Name_FunctionDefinition(is->Functions, functionNameHash, &original)) {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Function '\x01' redefined at \x02. Original at \x02.", functionNameHash, *position, original->StartPosition);
        return;
    }

    FunctionDefinition newF = {};
    newF.Kind = FuncDef::Custom;
    newF.ParamCount = argCount;
    newF.StartPosition = *position;
    MapPut_Name_FunctionDefinition(is->Functions, functionNameHash, newF, true);

    *position = *position + tokenCount + 1; // start + definition length + terminator token
}

inline DataType HandleReturn(InterpreterState* is) {
    if (is == NULL) return DataType::Exception;

    int result;
    if (!VecPop_int(is->_returnStack, &result)) {
        // This could be an error, but we're going to assume it's a return from inside the root of a program (or actor, etc.)
        return DataType::EndOfProgram;
    }

    ScopeDrop(is->_variables);
    is->_position = result;
}

inline DataType HandleControlSignal(int* position, char codeAction, int opCodeCount, InterpreterState* is) {
    switch (codeAction)
    {
    // cmp - relative jump *DOWN* if top of stack is false
    case 'c':
    {
        DataTag tag = TryPopFromValueStack(is, *position);
        auto condition = CastBoolean(is, tag);

        if (condition == false) { *position += opCodeCount; }
        break;
    }
    // jmp - unconditional relative jump *UP*
    case 'j':
    {
        int newPosition = (*position) - opCodeCount;
        *position = newPosition;
        break;
    }
    // skip - unconditional relative jump *DOWN*
    case 's':
    {
        *position += opCodeCount;
        break;
    }
    // ct - call term - a function that returns values ended without returning
    case 't':
    {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "A function returned without setting a value. Did you miss a 'return' in a function? At position \x02", position);
        return DataType::Exception;
    }
    // ret - pop return stack and jump to absolute position
    case 'r':
        return HandleReturn(is);

    default:
    {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Unknown control signal '\x04'", codeAction);
        return DataType::Exception;
    }
    }
    return DataType::Void;
}

inline int HandleCompoundCompare(int position, char codeAction, uint16_t argCount, uint16_t opCodeCount, InterpreterState* is) {
    auto param = ReadParams(is, argCount);

    if (param == NULL) {
		is->State = ExecutionState::ErrorState;
        StringAppend(is->_output, "Out of memory?");
        return -1;
    }

    int result;
    auto cmp = (CmpOp)codeAction;
    switch (cmp) {
    case CmpOp::Equal:
        result = ListEquals(argCount, param, is) ? position : position + opCodeCount;
        break;
    case CmpOp::NotEqual:
        result = ListEquals(argCount, param, is) ? position + opCodeCount : position;
        break;
    case CmpOp::Less:
        result = FoldLessThan(argCount, param, is) ? position : position + opCodeCount;
        break;
    case CmpOp::Greater:
        result = FoldGreaterThan(argCount, param, is) ? position : position + opCodeCount;
        break;
    default:
    {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Unknown compound compare at position \x02", position);
        result = -1;
        break;
    }
    }

    ArenaDereference(is->_memory, param);
    return result;
}

inline void HandleMemoryAccess(int* position, char action, uint32_t varRef, uint8_t paramCount, InterpreterState* is) {
    switch (action)
    {
    case 'g': // get (adds a value to the stack, false if not set)
    {
        if (paramCount > 0) {
            DoIndexedGet(is, varRef, paramCount);
            break;
        }
        auto tag = ScopeResolve(is->_variables, varRef);
        ResolveIndexIfRequired(is, &tag); // if this is an index reference, resolve it before continuing
        VecPush_DataTag(is->_valueStack, tag);
        break;
    }
    case 's': // set
    {
        if (paramCount > 1) {
            DoIndexedSet(is, varRef, paramCount);
            break;
        }
        DataTag tag;
        if (!VecPop_DataTag(is->_valueStack, &tag)) {
			is->State = ExecutionState::ErrorState;
            StringAppendFormat(is->_output, "There were no values to save. Did you forget a `return` in a function? Position:  \x02", *position);
            return;
        }
        ResolveIndexIfRequired(is, &tag); // if this is an index reference, resolve it before continuing

        ScopeSetValue(is->_variables, varRef, tag);
        break;
    }
    case 'i': // is set? (adds a bool to the stack)
    {
		if (paramCount < 1) { // check a reference is in scope
			auto val = EncodeBool(ScopeCanResolve(is->_variables, varRef));
			VecPush_DataTag(is->_valueStack, val);
		} else {
			// check if a key is present in a hash-map
            auto target = TryPopFromValueStack(is, is->_position);
            auto container = ScopeResolve(is->_variables, varRef);
            ResolveIndexIfRequired(is, &target); // if this is an index reference, resolve it before continuing
			auto src = (HashMap*)InterpreterDeref(is, container);
            if (src == NULL) {
				VecPush_DataTag(is->_valueStack, EncodeBool(false)); // not a valid container
                return;
            }
            auto key = CastString(is, target);
			auto found = MapGet_StringPtr_DataTag(src, key, NULL);
			VecPush_DataTag(is->_valueStack, EncodeBool(found));
		}
		break;
    }
    case 'u': // unset
    {
        //StringAppendFormat(is->_output, "\nRequest to delete '\x01' with \x02 parameters." , DbgStr(is, varRef), p3);
        if (paramCount < 1) { // remove a reference
            ScopeRemove(is->_variables, varRef);
        } else { // requesting to remove entries from a hash-map?
            auto target = TryPopFromValueStack(is, is->_position);
            auto container = ScopeResolve(is->_variables, varRef);
            ResolveIndexIfRequired(is, &target); // if this is an index reference, resolve it before continuing
            auto src = (HashMap*)InterpreterDeref(is, container);
            if (src == NULL) {
                return;
            }
            auto key = CastString(is, target);
            auto ok = MapRemove_StringPtr_DataTag(src, key);
            StringDeallocate(key);
        }
        break;
    }
    default:
    {
		is->State = ExecutionState::ErrorState;
        StringAppendFormat(is->_output, "Unknown memory opcode: '\x04'", action);
        return;
    }
    }
}

// dispatch for op codes. valueStack is Vector<DataTag>, returnStack is Vector<int>
inline DataType ProcessOpCode(char codeClass, char codeAction, uint16_t p1, uint16_t p2, uint8_t p3, int* position, DataTag word, InterpreterState* is) {
    uint32_t varRef;
    switch (codeClass)
    {
    case 'f': // Function operations
        if (codeAction == 'c') {
            varRef = p2 + (p1 << 16);
            return PrepareFunctionCall(position, varRef, p3, is);
        } else if (codeAction == 'd') {
            HandleFunctionDefinition(position, p1, p2, is);
        }
        break;

    case 'c': // flow Control -- conditions, jumps etc
    {
        int opCodeCount = p2 + (p1 << 16); // we use 31 bit jumps, in case we have lots of static data
        return HandleControlSignal(position, codeAction, opCodeCount, is);
    }
    break;

    case 'C': // compound compare and jump
        *position = HandleCompoundCompare(*position, codeAction, p1, p2, is);
        break;

    case 'm': // Memory access - get|set|isset|unset
        varRef = p2 + (p1 << 16);
        HandleMemoryAccess(position, codeAction, varRef, p3, is);
        break;

    case 'i': // special 'increment' operator. Uses the 'codeAction' slot to hold a small signed number
        varRef = p2 + (p1 << 16);
        ScopeMutateNumber(is->_variables, varRef, (int8_t)codeAction);
        break;

    case 's': // reserved for System operation.
        StringAppendFormat(is->_output, "Unimplemented System operation op code at \x02 : '\x01'\n", position, DiagnosticString(word, is));
        return DataType::Exception;

    default:
        StringAppendFormat(is->_output, "Unexpected op code at \x02 : '\x01'\n", position, DiagnosticString(word, is));
        return DataType::Exception;
    }
    return DataType::Void;
}


// Try to add an incoming IPC message to an InterpreterState.
// Only call when the program is in a wait state.
// The call will ignore the request if it is not interested in the target type
// Returns false iff there is an error storing the message. Successful stores AND ignored messages return true.
bool InterpAddIPC(InterpreterState* is, String* targetName, Vector* ipcMessageData) {
	if (is == NULL || targetName == NULL || ipcMessageData == NULL) return true; // invalid
	if (is->IPC_Queues == NULL) return true; // no bindings
	
	VectorPtr *queue;
	bool mapped = MapGet_StringPtr_VectorPtr(is->IPC_Queues, targetName, &queue);
	if (!mapped) return true; // this one not bound

	// `queue` is a vector of byte-vectors. We want to import a *copy* of the incoming data
	// migrated into the interpreter's arena.

	auto newMsg = VecClone(ipcMessageData, is->_memory); // what to do if we run out of memory?
	if (newMsg == NULL) return false;
	auto ok = VectorPush(*queue, newMsg);

	// set a ready state if we're waiting for a message we have.
	if (is->State == ExecutionState::IPC_Wait) {
		bool *flag;
		if (MapGet_StringPtr_bool(is->IPC_Queue_WaitFlags, targetName, &flag)) {
			if (flag != NULL && *flag == true) {
				is->State = ExecutionState::IPC_Ready;
			}
		}
	}

	return ok;
}

// Clear all wait flags for IPC targets
void ResetIPCWaits(InterpreterState* is) {
	if (is == NULL || is->IPC_Queue_WaitFlags == NULL) return;

	// deallocate all the string keys
	auto ptrs = MapAllEntries(is->IPC_Queue_WaitFlags); // Vector<HashMap_KVP>
	HashMap_KVP target;
	while (VecPop_HashMap_KVP(ptrs, &target)) {
		StringDeallocate(*((StringPtr*)target.Key));
	}

	// wipe the map
	MapClear(is->IPC_Queue_WaitFlags);
}

// Add an IPC target to our wait state, and ensure any data structures are in place.
void AddWaitFlag(InterpreterState* is, String* target) {
	if (is == NULL || target == NULL) return;

	// Check we have a queue map ready:
	if (is->IPC_Queues == NULL) { is->IPC_Queues = MapAllocateArena_StringPtr_VectorPtr(5, is->_memory); }

	// check there is a message queue for the specific target:
	VectorPtr *msgQueue;
	if (!MapGet_StringPtr_VectorPtr(is->IPC_Queues, target, &msgQueue)) {
		auto newQueue = VecAllocateArena_VectorPtr(is->_memory);
		MapPut_StringPtr_VectorPtr(is->IPC_Queues, target, newQueue, true);
		msgQueue = &newQueue;
	}

	// check we have a queue wait map:
	if (is->IPC_Queue_WaitFlags == NULL) { is->IPC_Queue_WaitFlags = MapAllocateArena_StringPtr_bool(5, is->_memory); }

	// Finally, set the wait flag:
	MapPut_StringPtr_bool(is->IPC_Queue_WaitFlags, target, true, true);
}

Vector* InterpWaitingIPC(InterpreterState* is) {
	auto result = VecAllocateArena_StringPtr(is->_memory);

	if (is->IPC_Queue_WaitFlags != NULL) {
		auto ptrs = MapAllEntries(is->IPC_Queue_WaitFlags); // Vector<HashMap_KVP>
		HashMap_KVP target;
		while (VecPop_HashMap_KVP(ptrs, &target)) {
			VecPush_StringPtr(result, *((StringPtr*)target.Key));
		}
	}

	return result;
}

String *ConcatList(int nbParams, DataTag* param, int startIndex, InterpreterState* is) {
    auto str = StringEmptyInArena(is->_memory);
    for (int i = startIndex; i < nbParams; i++) {
        StringAppend(str, CastString(is, param[i]));
    }
    return str;
}

DataTag ChainSum(InterpreterState* is, int nbParams, DataTag* param) {
    double sum = 0.0;
    for (int i = 0; i < nbParams; i++) {
        sum += CastInt(is, param[i]);
    }
    return EncodeInt32(sum);
}
DataTag ChainDifference(InterpreterState* is, int nbParams, DataTag* param) {
    double sum = CastInt(is, param[0]);
    for (int i = 1; i < nbParams; i++) {
        sum -= CastInt(is, param[i]);
    }
    return EncodeInt32(sum);
}
DataTag ChainProduct(InterpreterState* is, int nbParams, DataTag* param) {
    double sum = CastInt(is, param[0]);
    for (int i = 1; i < nbParams; i++) {
        sum *= CastInt(is, param[i]);
    }
    return EncodeInt32(sum);
}
DataTag ChainDivide(InterpreterState* is, int nbParams, DataTag* param) {
    double sum = CastInt(is, param[0]);
    for (int i = 1; i < nbParams; i++) {
        sum /= CastInt(is, param[i]);
    }
    return EncodeInt32(sum);
}
DataTag ChainRemainder(InterpreterState* is, int nbParams, DataTag* param) {
    int sum = CastInt(is, param[0]);
    for (int i = 1; i < nbParams; i++) {
        sum = sum % CastInt(is, param[i]);
    }
    return EncodeInt32(sum);
}

DataTag ConcatStrings(int nbParams, InterpreterState * is, DataTag * param)
{
    auto str = StringEmptyInArena(is->_memory);

    for (int i = 0; i < nbParams; i++) {
        auto s = CastString(is, param[i]);
        StringAppend(str, s);
        StringDeallocate(s);
    }

    return StoreStringAndGetReference(is, str);
}

DataTag ConcatVectors(int nbParams, InterpreterState * is, DataTag * param) {
    auto list = VecAllocateArena_DataTag(is->_memory);

    // unpack each vector, and copy its data across
    for (int i = 0; i < nbParams; i++) {
        auto src = (Vector*)InterpreterDeref(is, param[i]);
        if (src == NULL) continue;

        int len = VecLength(src);
        for (int j = 0; j < len; j++) {
            VecPush_DataTag(list, *VecGet_DataTag(src, j));
        }
    }

    uint32_t encPtr = ArenaPtrToOffset(is->_memory, list); // allows us to place a 32-bit ptr in 64-bit space
    if (encPtr < 1) { return RuntimeError(is->_position); } // nonsense result from arena allocation

    return EncodePointer(encPtr, DataType::VectorPtr);
}

bool AllVectors(int nbParams, DataTag * param) {
    for (int i = 0; i < nbParams; i++)
    {
        if (param[i].type != (int)DataType::VectorPtr) return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This is where all the code for the built-in functions are
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DataTag EvaluateBuiltInFunction(int* position, FuncDef kind, int nbParams, DataTag* param, InterpreterState* is){
    switch (kind) {
        // each element equal to the first
    case FuncDef::Equal:
        if (nbParams < 2) return _Exception(is, "equals ( = ) must have at least two things to compare");
        return EncodeBool(ListEquals(nbParams, param, is));

        // Each element smaller than the last
    case FuncDef::GreaterThan:
        if (nbParams < 2) return _Exception(is, "greater than ( > ) must have at least two things to compare");
        return EncodeBool(FoldGreaterThan(nbParams, param, is));

        // Each element larger than the last
    case FuncDef::LessThan:
        if (nbParams < 2) return _Exception(is, "less than ( < ) must have at least two things to compare");
        return EncodeBool(FoldLessThan(nbParams, param, is));

        // Each element DIFFERENT TO THE FIRST (does not check set uniqueness!)
    case FuncDef::NotEqual:
        if (nbParams < 2) return _Exception(is, "not-equal ( <> ) must have at least two things to compare");
        return EncodeBool(!ListEquals(nbParams, param, is));

    case FuncDef::Assert:
    {
        if (nbParams < 1) return VoidReturn(); // assert nothing passes
        auto condition = param[0];
        if (CastBoolean(is, condition) == false) {
            auto msg = ConcatList(nbParams, param, 1, is);
            return _Exception(is, "Assertion failed: ", msg);
        }
        return VoidReturn();
    }

    case FuncDef::Random:
        // TODO: the 0 param should give a float?
        if (nbParams < 1) return EncodeInt32(int_random(is->_stepsTaken)); // 0 params - any size
        if (nbParams < 2) return EncodeInt32(random_at_most(is->_stepsTaken, param[0].data)); // 1 param  - max size
        return EncodeInt32(ranged_random(is->_stepsTaken, param[0].data, param[1].data)); // 2 params - range

    case FuncDef::LogicNot:
    {
        if (nbParams != 1) return _Exception(is, "'not' should be called with one argument");
        auto bval = CastBoolean(is, param[0]);
        return EncodeBool(!bval);
    }
    case FuncDef::LogicOr:
    {
        bool more = nbParams > 0;
        int i = 0;
        while (more) {
            auto bresult = CastBoolean(is, param[i]);
            if (bresult) return EncodeBool(true);

            i++;
            more = i < nbParams;
        }

        return EncodeBool(false);
    }

    case FuncDef::LogicAnd:
    {
        bool more = nbParams > 0;
        int i = 0;
        while (more) {
            auto bresult = CastBoolean(is, param[i]);
            if (!bresult) return EncodeBool(false);

            i++;
            more = i < nbParams;
        }

        return EncodeBool(true);
    }

    case FuncDef::ReadKey:
    {
        // if there isn't enough data, we should set the ExecutionState to waiting and exit.
        if (StringLength(is->_input) < 1) return MustWait(is->_position);

        // otherwise read the character
        return EncodeShortStr(StringDequeue(is->_input));
    }

    case FuncDef::ReadLine:
    {
        // if there isn't enough data, we should set the ExecutionState to waiting and exit.
        uint32_t pos = 0;
        if (!StringFind(is->_input, '\n', 0, &pos)) return MustWait(is->_position);

        auto str = StringEmptyInArena(is->_memory);
        for (int i = 0; i < pos; i++) {
            StringAppendChar(str, StringDequeue(is->_input));
        }
        // otherwise read the character
        return StoreStringAndGetReference(is, str);
    }

    case FuncDef::Print:
    {
        bool emptyEnd = false;
        for (int i = 0; i < nbParams; i++) {
            auto str = CastString(is, param[i]);
            emptyEnd = StringLength(str) == 0;
            StringAppend(is->_output, str);
            StringDeallocate(str);
        }

        if (!emptyEnd) StringNL(is->_output);
        return VoidReturn();
    }

    case FuncDef::Substring:
    {
        if (nbParams == 2) {
            auto newString = CastString(is, param[0]);
            int origLen = StringLength(newString);
            int offset = CastInt(is, param[1]);
            StringChop(newString, offset, origLen - offset);
            return StoreStringAndGetReference(is, newString);

        } else if (nbParams == 3) {
            auto newString = CastString(is, param[0]);
            int offset = CastInt(is, param[1]);
            int len = CastInt(is, param[2]);
            StringChop(newString, offset, len);
            return StoreStringAndGetReference(is, newString);

        } else {
            return _Exception(is, "'Substring' should be called with 2 or 3 parameters");
        }
    }

    case FuncDef::Length:
    {
        if (param[0].type == (int)DataType::VectorPtr) {
            auto vec = (Vector*)InterpreterDeref(is, param[0]);
            if (vec == NULL) return EncodeInt32(0);
            return EncodeInt32(VecLength(vec));
        } else if (param[0].type == (int)DataType::HashtablePtr) {
            auto map = (HashMap*)InterpreterDeref(is, param[0]);
            if (map == NULL) return EncodeInt32(0);
            return EncodeInt32(MapCount(map));
        }


        // anything else is stringified
        auto str = CastString(is, param[0]);
        int v = StringLength(str);
        StringDeallocate(str);
        return EncodeInt32(v);
    }

    case FuncDef::Replace:
    {
        if (nbParams != 3) return _Exception(is, "'Replace' should be called with 3 parameters");
        auto src = CastString(is, param[0]);
        auto oldValue = CastString(is, param[1]);
        auto newValue = CastString(is, param[2]);

        auto out = StringReplace(src, oldValue, newValue);
        auto v = StoreStringAndGetReference(is, out);
        StringDeallocate(src);
        StringDeallocate(oldValue);
        StringDeallocate(newValue);
        return v;
    }

    case FuncDef::Concat:
    {
        // TODO: extend this:
        // if all params are vectors, we make a new vector of their contents (copying)
        // if all params are hash-maps, we make a new map with all the KVPs
        // else we stringify everything and concat the results
        if (param[0].type == (int)DataType::VectorPtr) {
            if (AllVectors(nbParams, param)) {
                return ConcatVectors(nbParams, is, param);
            }
        }

        return ConcatStrings(nbParams, is, param);
    }

    case FuncDef::UnitEmpty:
	{
		if (nbParams > 0) {
			// decompose the last value on the stack (for container types)
            auto target = TryPopFromValueStack(is, is->_position);
			ResolveIndexIfRequired(is, &target);

			if (!IsContainerType(target)) {
				return _Exception(is, "Attempted to decompose a non-container type. ", StringNewFormat("passed a '\x02' at \x02\n", target.type, position));
			}

			return DecomposeContainer(is, target, nbParams, param);
		} else {
			// valueless marker (like an empty object)
			return UnitReturn();
		}
    }


    case FuncDef::MathAdd:
        if (nbParams == 1) return EncodeInt32(CastInt(is, param[0])); // todo: check for number type, do promotion
        return ChainSum(is, nbParams, param);

    case FuncDef::MathSub:
        if (nbParams == 1) return EncodeInt32(-CastInt(is, param[0]));
        return ChainDifference(is, nbParams, param);

    case FuncDef::MathProd:
        if (nbParams == 1) return _Exception(is, "Uniary '*' is not supported");
        return ChainProduct(is, nbParams, param);

    case FuncDef::MathDiv:
        if (nbParams == 1) return _Exception(is, "Uniary '/' is not supported");
        return ChainDivide(is, nbParams, param);

    case FuncDef::MathMod:
        if (nbParams == 1) return EncodeInt32(CastInt(is, param[0]) % 2);
        return ChainRemainder(is, nbParams, param);

    case FuncDef::Eval:
    {
        // Add a new block to our code base and jump to it.
        // that keeps our stepping mechanism working.
        // Static string pointers need an offset mechanism,
        // handled by the tag code writer.

        MMPush(1 MEGABYTE); // Arena for COMPILING, not for running.

        auto code = CastString(is, param[0]);
        //StringAppendFormat(is->_output, "\nEvaluating: [[\x01]]\n", code);
        auto compilableSyntaxTree = ParseSourceCode(code, false);

        auto tagCode = CompileRoot(compilableSyntaxTree, false, true); // a variant that 'EndOfSubProgram' instead of 'EndOfProgram'

        auto nextPos = TCW_AppendToVector(tagCode, is->_program); // These opcodes should be removed when 'EndOfSubProgram' is reached
        if (nextPos < 0) return RuntimeError(is->_position);
        // NOTE: If `eval` code uses `return` to exit, we will probably leak.
        // Because `eval` is greedy, it should be ok to nest evals inside other evals. Not a good idea, but possible.

        StringDeallocate(code);
        DeallocateAST(compilableSyntaxTree);
        TCW_Deallocate(tagCode);

        MMPop();

        ScopePush(is->_variables, param, nbParams); // write parameters into new scope
        VecPush_int(is->_returnStack, *position); // set position for 'cret' call
        *position = nextPos; // move pointer to start of function, taking into account the interpreter's auto-advance
        return VoidReturn(); // return no value, continue execution elsewhere
    }
    case FuncDef::Call:
    {
        auto type = param[0].type;
        if (type != (int)DataType::StringPtr && type != (int)DataType::StaticStringPtr)
            return _Exception(is, "Tried to call a function by name, but was not a string", StringNewFormat("passed a '\x02' at \x02\n", type, position));

        // this should be a string, but we need a function name hash -- so calculate it:
        auto strName = CastString(is, param[0]);
        auto functionNameHash = GetCrushedName(strName);
        StringDeallocate(strName);
        return EvaluateFunctionCall(position, functionNameHash, nbParams - 1, param + 1, is);
    }
    case FuncDef::NewList:
    {
        auto list = VecAllocateArena_DataTag(is->_memory);

        // lists store datatags directly
        for (int i = 0; i < nbParams; i++) {
            VecPush_DataTag(list, param[i]);
        }

        uint32_t encPtr = ArenaPtrToOffset(is->_memory, list); // allows us to place a 32-bit ptr in 64-bit space
        if (encPtr < 1) { return _Exception(is, "Failed to find list in memory"); } // nonsense result from arena allocation

        return EncodePointer(encPtr, DataType::VectorPtr);
    }
    case FuncDef::NewMap:
    {
        auto theMap = MapAllocateArena_StringPtr_DataTag(64, is->_memory);

        // Read initial data
        for (int i = 0; i < nbParams; i+=2) {
            auto key = CastString(is, param[i]);
            auto value = param[i+1];
            MapPut_StringPtr_DataTag(theMap, key, value, true);
        }
        
        // map from RAM to arena offset
        uint32_t encPtr = ArenaPtrToOffset(is->_memory, theMap); // allows us to place a 32-bit ptr in 64-bit space
        if (encPtr < 1) { return _Exception(is, "Failed to find map in memory"); } // nonsense result from arena allocation

        return EncodePointer(encPtr, DataType::HashtablePtr);
    }
    case FuncDef::Push:
    {
        if (nbParams < 2) return _Exception(is, "`push` needs a list and at least one value");
        if (param[0].type != (int)DataType::VectorPtr) return _Exception(is, "First parameter to `push` must be a list");

        auto vec = (Vector*)InterpreterDeref(is, param[0]);
        if (vec == NULL) return _Exception(is, "The list you tried to `push` to was invalid");

        for (int i = 1; i < nbParams; i++) {
            if (param[i].type == (int)DataType::Not_a_Result) continue;
            VecPush_DataTag(vec, param[i]);
        }
        return VoidReturn(); // maybe push could return something useful?
    }
    case FuncDef::Pop:
    {
        if (nbParams != 1) return _Exception(is, "`pop` needs a single list");
        if (param[0].type != (int)DataType::VectorPtr) return _Exception(is, "First parameter to `pop` must be a list");

        auto vec = (Vector*)InterpreterDeref(is, param[0]);
        if (vec == NULL) return _Exception(is, "The list you tried to `pop` from was invalid");

        DataTag result;
        if (!VecPop_DataTag(vec, &result)) {
            return NonResult();
        }
        return result;
    }
    case FuncDef::Dequeue:
    {
        if (nbParams != 1) return _Exception(is, "`dequeue` needs a single list");
        if (param[0].type != (int)DataType::VectorPtr) return _Exception(is, "First parameter to `dequeue` must be a list");

        auto vec = (Vector*)InterpreterDeref(is, param[0]);
        if (vec == NULL) return _Exception(is, "The list you tried to `dequeue` from was invalid");

        DataTag result;
        if (!VecDequeue_DataTag(vec, &result)) { return NonResult(); }
        return result;
    }

	case FuncDef::Listen:
	{
		// Here we would add a hook into our `is` IPC map
		// The 'listen' call replaces any other previous calls.
		// So if you call `listen("a" "b")` and then `listen("a" "c")`
		// The second call would destroy the 'b' queue and add a new 'c' queue.

		// Check we have a queue map ready:
		if (is->IPC_Queues == NULL) { is->IPC_Queues = MapAllocateArena_StringPtr_VectorPtr(5, is->_memory); }

		// check there is a message queue for each specific target:
		for (int i = 0; i < nbParams; i++) {
			auto target = CastString(is, param[i]);
			if (StringLength(target) < 1) continue; // ignore empty strings

			VectorPtr* msgQueue;
			if (!MapGet_StringPtr_VectorPtr(is->IPC_Queues, target, &msgQueue)) {
				auto newQueue = VecAllocateArena_VectorPtr(is->_memory);
				MapPut_StringPtr_VectorPtr(is->IPC_Queues, target, newQueue, true);
				msgQueue = &newQueue;
			} else {
				StringDeallocate(target);
			}
		}

		// TODO: remove the old queues.
		// Maybe it would be better to make a new queue set and move any still active ones over.

        //return _Exception(is, "Built-in not implemented: `listen`");
	}

	case FuncDef::Wait:
	{
		if (is->IPC_Queues == NULL) return _Exception(is, "Tried to `wait`, but you didn't say `listen` first.");

		// Kick back to the interpreter with a special status code, and set an IPC flag for ourselves.
		// When we resume, we should be able to read our IPC map into the value stack before continuing
		ResetIPCWaits(is);
		if (is->IPC_Queue_WaitFlags == NULL) { is->IPC_Queue_WaitFlags = MapAllocateArena_StringPtr_bool(5, is->_memory); }
		
		// Re-add every requested target
		// also, check we have an appropriate queue, otherwise fail.
		for (int i = 0; i < nbParams; i++) {
			auto target = CastString(is, param[i]);
			if (StringLength(target) < 1) continue; // ignore empty strings

			bool listening = MapGet_StringPtr_VectorPtr(is->IPC_Queues, target, NULL);
			if (!listening) return _Exception(is, "Tried to `wait` for a message you didn't add to `listen`");

			if (!MapPut_StringPtr_bool(is->IPC_Queue_WaitFlags, target, true, true)) {
				return _Exception(is, "`wait` failed: could not store wait states");
			}
		}
        return IPCWaitRequest();
	}

	case FuncDef::Send:
	{
		// Serialise object on the stack, and place it in our IPC outbox.
		// Then kick back to the interpreter with a special status code. The interpreter should then have
		// access to that data and the target name. It should then distribute the message to all running
		// programs with a matching key in their IPC map.
		// This program can be resumed at any time without special warm-up.
		// Senders have no idea how many (if any) programs got their message.
        return _Exception(is, "Built-in not implemented: `send`");
	}

    default:
        return _Exception(is, "Unrecognised built-in!", StringNewFormat(" Type = \x02\n", ((int)kind)));
    }
}


// Read and return a copy of the opcode at the given index
DataTag GetOpcodeAtIndex(InterpreterState* is, uint32_t index) {
    if (is == NULL) return InvalidTag();
    if (index >= VecLength(is->_program)) return InvalidTag();

    auto p = VecGet_DataTag(is->_program, index);
    return *p;
}

// Convert a tag code offset into a physical memory location
void* InterpreterDeref(InterpreterState* is, DataTag encodedPosition) {
    auto offset = DecodePointer(encodedPosition);
    return ArenaOffsetToPtr(is->_memory, offset);
}

// Get the variables scope of the interpreter instance
Scope* InterpreterScope(InterpreterState* is) {
    if (is == NULL) return NULL;
    return is->_variables;
}

// Store a new string at the end of memory, and return a string pointer token for it
// The original string parameter is deallocated
DataTag StoreStringAndGetReference(InterpreterState* is, String* str) {
    if (str == NULL) return RuntimeError(is->_position);

    // short strings are stack/scope values
    if (StringLength(str) <= 6) {
        return EncodeShortStr(str);
    }

    // The C# version of this piled stuff into the end of the bytecode data.
    // In the C++ version, all the runtime container data goes into the interpreter's `_memory` arena.
    // The arena is overall better, but the de-referencing has to be smarter (esp. between static and non-static)

    String* result = StringEmptyInArena(is->_memory);
    auto length = StringLength(str);
    for (uint32_t i = 0; i < length; i++) {
        StringAppendChar(result, StringDequeue(str));
    }
    StringDeallocate(str);
    
    uint32_t encPtr = ArenaPtrToOffset(is->_memory, result); // allows us to place a 32-bit ptr in 64-bit space
    if (encPtr < 1) {
        return RuntimeError(is->_position); // nonsense result from arena allocation
    }

    return EncodePointer(encPtr, DataType::StringPtr);
}


String* ReadStaticString(InterpreterState* is, int position, int length) {
    if (is == NULL) return NULL;
    return DecodeString(is->_program, position, length, is->_memory);
}

inline bool CheckProgramWindow(InterpreterState* is, DataTag** programWindow, int* low, int* high) {
    int pos = is->_position;
    if (*programWindow != NULL && pos >= *low && pos <= *high) return true;

    // out of cached range. Re-cache
    if (*programWindow != NULL) VecFreeCache(is->_program, programWindow);
    *low = pos - 200;
    *high = pos + 400; // bias forward of current position
    *programWindow = VecCacheRange_DataTag(is->_program, low, high);
    if (*programWindow == NULL) {
        // total failure. Out of memory?
        return false;
    }
    if (pos < *low || pos > *high) {
        // still out of range. We're out of bounds.
        return false;
    }
    return true;
}

// Read a byte vector into an interpreter object, and push that to the value stack.
bool DeserialiseIPCData(InterpreterState* is, VectorPtr ipcData, StringPtr target) {
	DataTag dest; // ref we will put in the map
    bool ok = DefrostFromVector(&dest, is->_memory, ipcData);
	if (!ok) return false;

	// Make a map to store it in
	auto map = MapAllocateArena_StringPtr_DataTag(1, is->_memory);
	if (map == NULL) return false;

	// add the data
	ok = MapPut_StringPtr_DataTag(map, target, dest, true);
	if (!ok) return false;
	
	// Encode a pointer to the map
	uint32_t encPtr = ArenaPtrToOffset(is->_memory, map); // allows us to place a 32-bit ptr in 64-bit space
	if (encPtr < 1) { return false; } // nonsense result from arena allocation
	DataTag final = EncodePointer(encPtr, DataType::HashtablePtr);

	// push into the value stack
	return VecPush_DataTag(is->_valueStack, final);
}

// Load a message from program's queues based on that programs wait state
bool LoadIPCData(InterpreterState *is) {
	// We match any keys in `is->IPC_Queue_WaitFlags` with data available in `is->IPC_Queues`
	// The first match we find, we deserialise the data into `is->_valueStack`, wrapped in a
	// map, whose key is the matching IPC target.

	if (is->IPC_Queues == NULL || is->IPC_Queue_WaitFlags == NULL) return false;

	VectorPtr vecWait = MapAllEntries(is->IPC_Queue_WaitFlags); // Vector<HashMap_KVP>

	bool ok = false;
	// For each message we're waiting for...
	HashMap_KVP waitEntry;
	while (VecPop_HashMap_KVP(vecWait, &waitEntry)) {
		StringPtr target = *((StringPtr*)waitEntry.Key);
		bool set = *((bool*)waitEntry.Value);

		// ... see if we have data ...
		VectorPtr *vecData;
		bool found = MapGet_StringPtr_VectorPtr(is->IPC_Queues, target, &vecData);
		if (!found || vecData == NULL) continue; // no queue?
		if (VecLength(*vecData) < 1) continue; // empty queue

		VectorPtr ipcObject;
		if (!VecDequeue_VectorPtr(*vecData, &ipcObject)) continue; // failed to read object?

		ok = DeserialiseIPCData(is, ipcObject, target);
		VecDeallocate(ipcObject); // we will have copied the data by deserialising.
		break; // only load one message at a time
	}
	VecDeallocate(vecWait);

	return ok;
}

// Run the interpreter until end or cycle count (whichever comes first)
// This is the internal call. The public one is below (it sets the last state of the interpreter)
ExecutionResult InterpRunInternal(InterpreterState* is, int maxCycles) {
    if (is == NULL) {
        return FailureResult(0);
    }

    DataTag evalResult = {};
    auto programEnd = VectorLength(is->_program);
    int localSteps = 0;


	// If we are coming out of an IPC wait state, we need to load the message data onto the value stack here.
	if (is->State == ExecutionState::IPC_Ready) {
		auto ok = LoadIPCData(is);
		if (!ok) {
			_Exception(is, "Failed to load IPC data");
			return FailureResult(0);
		}
	}

    DataTag* programWindow = NULL;
    int lowIndex = -1, highIndex = -1;
	is->State = ExecutionState::Running;

    while (true){
        if (localSteps >= maxCycles) {
            VecFreeCache(is->_program, programWindow);
            return PausedExecutionResult();
        }
        if (is->State == ExecutionState::ErrorState) {
            uint32_t errPos = 0;
            // try to read exception position
            if (VecPop_DataTag(is->_valueStack, &evalResult)) { if (evalResult.type == (int)DataType::Exception) errPos = evalResult.data; }
            else { errPos = is->_position; }
            return FailureResult(errPos);
        }

        is->_stepsTaken++;
        localSteps++;

        // Prevent stackoverflow.
        // Ex: if(true 1 10 20)
        /*if ((is->_stepsTaken & 127) == 0 && VecLength(is->_valueStack) > 100) { // TODO: improve this mess. Maybe add sentinels and dequeue on returns?
            for (int i = 0; i < 100; i++) {
                VectorDequeue(is->_valueStack, NULL); // knock values off the far side of the stack.
            }
        }*/


        if (!CheckProgramWindow(is, &programWindow, &lowIndex, &highIndex)) break;
        auto word = programWindow[is->_position - lowIndex];


        switch (word.type) {
        case (int)DataType::Invalid:
        {
            StringAppendFormat(is->_output, "Unknown code point at position \x02\n", is->_position);
            break;
        }

        case (int)DataType::Opcode:
        {
            // decode opcode and do stuff
            char codeClass, codeAction;
            uint16_t p1, p2;
            uint8_t p3;
            DecodeOpcode(word, &codeClass, &codeAction, &p1, &p2, &p3);
            auto result = ProcessOpCode(codeClass, codeAction, p1, p2, p3, &(is->_position), word, is);
			
			// TODO: make a flag bit for these lot...
            if (result == DataType::Exception) { // program failed
                return FailureResult(is->_position);
            } else if (result == DataType::MustWait) { // program is waiting for input, and is yielding
                return WaitingExecutionResult();
			} else if (result == DataType::IPCWait) {
				return IPCWaitExecutionResult();
            } else if (result == DataType::EndOfProgram) { // program is exiting early (based on logic)
                goto GOOD_EXIT;
            }
            break;
        }
        case (int)DataType::EndOfSubProgram:
        {
            // Delete back to the 'EndOfProgram' marker
            HandleReturn(is);
            highIndex -= RollBackSubProgram(is);
            programEnd = VectorLength(is->_program);
            break;
        }
        case (int)DataType::EndOfProgram:
            goto GOOD_EXIT;

        default:
            VecPush_DataTag(is->_valueStack, word); // encoded values, or references/pointers
            break;
        }

        is->_position++;
    }

    // dropped out of the loop without hitting an end-of-program marker.
    // make a note of it:
    StringAppend(is->_output, "Program went out of bounds. Check compiler.");

GOOD_EXIT:
    // Exited the program. Cleanup and return value

    VecFreeCache(is->_program, programWindow);

    if (VecLength(is->_valueStack) != 0) {
        VecPop_DataTag(is->_valueStack, &evalResult);
    } else {
        evalResult = VoidReturn();
    }

    // reset state, just incase we get run again
    // it's up to the caller to clear input and output if required.
    VecClear(is->_returnStack);
    VecClear(is->_valueStack);
    is->_position = 0;

	return CompleteExecutionResult(evalResult);
}

// Run the interpreter until end or cycle count (whichever comes first)
// Remember to check execution state afterward
ExecutionResult InterpRun(InterpreterState* is, int maxCycles) {
	auto result = InterpRunInternal(is, maxCycles);
	is->State = result.State;
	return result;
}

