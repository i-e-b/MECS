#include "TagCodeInterpreter.h"

#include "MemoryManager.h"
#include "HashMap.h"
#include "Scope.h"
#include "TagCodeFunctionTypes.h"
#include "TagCodeReader.h"
#include "TypeCoersion.h"
#include "MathBits.h"

// required only for 'eval'
#include "SourceCodeTokeniser.h"
#include "CompilerCore.h"

#ifndef abs
#define abs(x)  ((x < 0) ? (-(x)) : (x))
#endif
typedef uint32_t Name;

RegisterHashMapStatics(Map)
RegisterHashMapFor(Name, StringPtr, HashMapIntKeyHash, HashMapIntKeyCompare, Map)
RegisterHashMapFor(Name, FunctionDefinition, HashMapIntKeyHash, HashMapIntKeyCompare, Map)

RegisterVectorStatics(Vec)
RegisterVectorFor(DataTag, Vec)
RegisterVectorFor(int, Vec)

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

    // If the `ErrorFlag` is true, the interpreter will stop as soon as it can
    // and return an error result.
    bool ErrorFlag;

    // the string table and opcodes
    Vector* _program; // Vector<DataTag> (read only)
    Scope* _variables; // scoped variable references
    Arena* _memory; // read/write memory (for non-short strings and other 'heap' containers)

    // Functions that have been defined at run-time
    HashMap*  Functions; // Map<CrushName -> FunctionDefinition>
    HashMap* DebugSymbols; // Map<CrushName -> String>

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
    
    add("new-list", FuncDef::NewList); add("push", FuncDef::Push);
    add("pop", FuncDef::Pop); add("dequeue", FuncDef::Dequeue);

    add("()", FuncDef::UnitEmpty); // empty value marker
#undef add;
}

// Start up an interpreter
// tagCode is Vector<DataTag>, debugSymbols in Map<CrushName -> StringPtr>.
InterpreterState* InterpAllocate(Vector* tagCode, size_t memorySize, HashMap* debugSymbols) {
    if (tagCode == NULL) return NULL;
    auto result = (InterpreterState*)mmalloc(sizeof(InterpreterState));
    if (result == NULL) return NULL;

    result->Functions = MapAllocate_Name_FunctionDefinition(100);
    result->DebugSymbols = debugSymbols; // ok if NULL
    result->ErrorFlag = false;

    result->_program = tagCode;
    result->_variables = ScopeAllocate();
    result->_memory = NewArena(memorySize);

    result->_position = 0;
    result->_stepsTaken = 0;
    result->_runningVerbose = false;

    result->_returnStack = VecAllocateArena_int(result->_memory);
    result->_valueStack = VecAllocateArena_DataTag(result->_memory);

    result->_input = StringEmpty();
    result->_output = StringEmpty();

    if ((result->Functions == NULL)
        || (result->_returnStack == NULL)
        || (result->_valueStack == NULL)
        || (result->_input == NULL)
        || (result->_variables == NULL)
        || (result->_memory == NULL)
        || (result->_output == NULL)) {
        InterpDeallocate(result);
        mfree(result);
        return NULL;
    }

    AddBuiltInFunctionSymbols(result->Functions);

    return result;
}

// Close down an interpreter and free all memory (except input tagCode and debugSymbols)
void InterpDeallocate(InterpreterState* is) {
    if (is == NULL) return;
    
    is->ErrorFlag = true;
    if (is->_input != NULL) StringDeallocate(is->_input);
    if (is->_output != NULL) StringDeallocate(is->_output);
    if (is->Functions != NULL) MapDeallocate(is->Functions);
    if (is->_returnStack != NULL) VecDeallocate(is->_returnStack);
    if (is->_valueStack != NULL) VecDeallocate(is->_valueStack);
    if (is->_variables != NULL) ScopeDeallocate(is->_variables);
    if (is->_memory != NULL) DropArena(&(is->_memory));

    mfree(is);
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
void RollBackSubProgram(InterpreterState* is) {
    DataTag tag = {};

    bool found = VecPeek_DataTag(is->_program, &tag);
    // sanity check:
    if (!found || tag.type != (int)DataType::EndOfSubProgram) return;

    /*StringAppend(is->_output, "   /\\ "); // reverse order!!!
    DescribeTag(tag, is->_output, NULL);
    StringAppend(is->_output, "\r\n");*/

    while (true) {
        VecPop_DataTag(is->_program, NULL);
        found = VecPeek_DataTag(is->_program, &tag);

        /*StringAppend(is->_output, "   /\\ "); // reverse order!!!
        DescribeTag(tag, is->_output, NULL);
        StringAppend(is->_output, "\r\n");*/

        if (!found
            || tag.type == (int)DataType::EndOfSubProgram
            || tag.type == (int)DataType::EndOfProgram) return;
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
    return StringEmpty();
}

DataTag* ReadParams(InterpreterState* is, uint16_t nbParams){
    DataTag* param = (DataTag*)ArenaAllocate(is->_memory, nbParams * sizeof(DataTag));
    if (param == NULL) return NULL;

    // Pop values from stack into a param cache, putting into source-code order
    for (int i = nbParams - 1; i >= 0; i--) {
        if (!VecPop_DataTag(is->_valueStack, &(param[i]))) {
            is->ErrorFlag = true;
            StringAppendFormat(is->_output, "\nValue stack underflow at position \x03 (\x02)\n", is->_position, is->_position);
            return NULL;
        }

        if (param[i].type == 0) { // invalid value!
            is->ErrorFlag = true;
            // this can happen if we have an unresolved name. Should be handled earlier.
            StringAppendFormat(is->_output, "\nInvalid value in parameters! Found when calling at position \x03 (\x02)\n", is->_position, is->_position);
            return NULL;
        }
    }

    return param;
}

// Defined at bottom
DataTag EvaluateBuiltInFunction(int* position, FuncDef kind, int nbParams, DataTag* param, InterpreterState* is);

DataTag EvaluateFunctionCall(int* position, int functionNameHash, int nbParams, DataTag* param, InterpreterState* is) {
    FunctionDefinition* fun = NULL;
    
    if (!MapIsValid(is->Functions)) {
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Function hash map has been damaged at position \x02\n", *position);
        return RuntimeError(is->_position);
    }

    bool found = MapGet_Name_FunctionDefinition(is->Functions, functionNameHash, &fun);

    if (!found) {
        is->ErrorFlag = true;
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
DataTag TryPopTag(InterpreterState* is, int position) {
    DataTag tag;
    if (!VecPop_DataTag(is->_valueStack, &tag)) {
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Value stack underflow at position \x02", position);
        return InvalidTag();
    }
    return tag;
}

void DoIndexedGet(InterpreterState* is, uint16_t paramCount) {
    auto target = TryPopTag(is, is->_position);
    auto value = ScopeResolve(is->_variables, DecodeVariableRef(target));

    switch (value.type)
    {
        // Note: Numeric types should read the bit at index
        // Hashes and arrays do the obvious lookup
        // Sets return true/false for occupancy
    case (int)DataType::StringPtr:
    case (int)DataType::StaticStringPtr:
    {
        // get the other indexes. If more than one, build a string out of the bits?
        // What to do with out-of-range?
        auto src = CastString(is, value);
        auto srcLength = StringLength(src);
        auto dst = StringEmpty();
        auto indexes = VecAllocateArena_int(is->_memory);

        for (int i = 0; i < paramCount; i++) { // get indexes from stack order to index order
            auto idx = CastInt(is, TryPopTag(is, is->_position));
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
        auto src = (Vector*)InterpreterDeref(is, value);
        if (src == NULL) {
            VecPush_DataTag(is->_valueStack, NonResult());
            return;
        }
        auto srcLength = VecLength(src);
        // if we've asked for one index, we return the value directly:
        if (paramCount == 1) {
            auto idx = CastInt(is, TryPopTag(is, is->_position));
            if (idx >= 0 && idx < srcLength) VecPush_DataTag(is->_valueStack, *VecGet_DataTag(src, idx));
            return;
        }

        // if more than one, we build a new vector:
        auto list = VecAllocateArena_DataTag(is->_memory);
        auto indexes = VecAllocateArena_int(is->_memory);

        // get indexes from stack order to index order
        for (int i = 0; i < paramCount; i++) {
            auto idx = CastInt(is, TryPopTag(is, is->_position));
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
            is->ErrorFlag = true;
            VecPush_DataTag(is->_valueStack, _Exception(is, "Arena mapping failed in indexed get from list"));
            return;
        }

        VecPush_DataTag(is->_valueStack, EncodePointer(encPtr, DataType::VectorPtr));
        return;
    }
    default:

        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Can't index at position: '\x02'", is->_position);
    }
}

DataType PrepareFunctionCall(int* position, uint16_t nbParams, InterpreterState* is) {
    DataTag nameTag = {};
    VecPop_DataTag(is->_valueStack, &nameTag);
    auto functionNameHash = DecodeVariableRef(nameTag);

    auto param = ReadParams(is, nbParams);
    if (param == NULL) {
        is->ErrorFlag = true;
        VecPush_DataTag(is->_valueStack, RuntimeError(is->_position));
        return DataType::Exception;
    }

    // Evaluate function.
    DataTag evalResult = EvaluateFunctionCall(position, functionNameHash, nbParams, param, is);
    ArenaDereference(is->_memory, param);

    if (evalResult.type == (int)DataType::Exception) {
        is->ErrorFlag = true;
        VecPush_DataTag(is->_valueStack, evalResult);
        return DataType::Exception;
    }
    if (evalResult.type == (int)DataType::MustWait) {
        return DataType::MustWait;
    }

    if (evalResult.type == 0) { // something gave a totally invalid result
        is->ErrorFlag = true;
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
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Value stack underflow during function definition at position \x02", *position);
        return;
    }
    auto functionNameHash = DecodeVariableRef(tag);

    if (MapContains_Name_FunctionDefinition(is->Functions, functionNameHash)) {
        is->ErrorFlag = true;
        FunctionDefinition* original;
        MapGet_Name_FunctionDefinition(is->Functions, functionNameHash, &original);
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

void HandleReturn(int* position, InterpreterState* is) {
    if (is == NULL) return;


    /*StringAppend(is->_output, "\r\nValue stack at end of program:\r\n");
    auto vals = is->_valueStack;
    auto len = VecLength(vals);
    for (int i = 0; i < len; i++) {
        auto strVal = CastString(is, *VecGet_DataTag(vals, i));
        StringAppend(is->_output, strVal);
        StringAppend(is->_output, "\r\n");
    }
    StringAppend(is->_output, "###########################\r\n");*/


    int result;
    if (!VecPop_int(is->_returnStack, &result)) {
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Return stack empty. Check program logic at position \x02", position);
    }

    ScopeDrop(is->_variables);
    *position = result;
}

void HandleControlSignal(int* position, char codeAction, int opCodeCount, InterpreterState* is) {
    switch (codeAction)
    {
    // cmp - relative jump *DOWN* if top of stack is false
    case 'c':
    {
        DataTag tag = TryPopTag(is, *position);
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
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "A function returned without setting a value. Did you miss a 'return' in a function? At position \x02", position);
        return;
    }
    // ret - pop return stack and jump to absolute position
    case 'r':
        HandleReturn(position, is);
        break;

    default:
    {
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Unknown control signal '\x04'", codeAction);
        return;
    }
    }

}

int HandleCompoundCompare(int position, char codeAction, uint16_t argCount, uint16_t opCodeCount, InterpreterState* is) {
    auto param = ReadParams(is, argCount);

    if (param == NULL) {
        is->ErrorFlag = true;
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
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Unknown compound compare at position \x02", position);
        result = -1;
        break;
    }
    }

    ArenaDereference(is->_memory, param);
    return result;
}

void HandleMemoryAccess(int* position, char action, int varRef, uint16_t paramCount, InterpreterState* is) {
    switch (action)
    {
    case 'g': // get (adds a value to the stack, false if not set)
        VecPush_DataTag(is->_valueStack, ScopeResolve(is->_variables, varRef));
        break;

    case 's': // set
    {
        DataTag tag;
        if (!VecPop_DataTag(is->_valueStack, &tag)) {
            is->ErrorFlag = true;
            StringAppendFormat(is->_output, "There were no values to save. Did you forget a `return` in a function? Position:  \x02", *position);
            return;
        }

        ScopeSetValue(is->_variables, varRef, tag);
        break;
    }
    case 'i': // is set? (adds a bool to the stack)
    {
        auto val = EncodeBool(ScopeCanResolve(is->_variables, varRef));
        VecPush_DataTag(is->_valueStack, val);
        break;
    }
    case 'u': // unset
    {
        ScopeRemove(is->_variables, varRef);
        break;
    }
    case 'G': // indexed get
        DoIndexedGet(is, paramCount);
        break;

    default:
    {
        is->ErrorFlag = true;
        StringAppendFormat(is->_output, "Unknown memory opcode: '\x04'", action);
        return;
    }
    }
}

// dispatch for op codes. valueStack is Vector<DataTag>, returnStack is Vector<int>
DataType ProcessOpCode(char codeClass, char codeAction, uint16_t p1, uint16_t p2, int* position, DataTag word, InterpreterState* is) {
    uint32_t varRef;
    switch (codeClass)
    {
    case 'f': // Function operations
        if (codeAction == 'c') {
            return PrepareFunctionCall(position, p1, is);
        } else if (codeAction == 'd') {
            HandleFunctionDefinition(position, p1, p2, is);
        }
        break;

    case 'c': // flow Control -- conditions, jumps etc
    {
        int opCodeCount = p2 + (p1 << 16); // we use 31 bit jumps, in case we have lots of static data
        HandleControlSignal(position, codeAction, opCodeCount, is);
    }
    break;

    case 'C': // compound compare and jump
        *position = HandleCompoundCompare(*position, codeAction, p1, p2, is);
        break;

    case 'm': // Memory access - get|set|isset|unset
        varRef = p2 + (p1 << 16);
        HandleMemoryAccess(position, codeAction, varRef, p1, is);
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


// Add IPC messages to an InterpreterState (only when it's not running)
void InterpAddIPC(InterpreterState*is, Vector* ipcMessages) {

}

String *ConcatList(int nbParams, DataTag* param, int startIndex, InterpreterState* is) {
    auto str = StringEmpty();
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
    auto str = StringEmpty();

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

        auto str = StringEmpty();
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
    { // valueless marker (like an empty object)
        return UnitReturn();
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

        MMPush(1 MEGABYTE);

        auto code = CastString(is, param[0]);
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
            VecPush_DataTag(list, param[i]); // TODO: check this is the correct order
        }

        uint32_t encPtr = ArenaPtrToOffset(is->_memory, list); // allows us to place a 32-bit ptr in 64-bit space
        if (encPtr < 1) { return RuntimeError(is->_position); } // nonsense result from arena allocation

        return EncodePointer(encPtr, DataType::VectorPtr);
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
    if (!IsAllocated(encodedPosition)) return NULL; // should NOT have been called for this type

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
    return DecodeString(is->_program, position, length);
}


// Run the interpreter until end or cycle count (whichever comes first)
// Remember to check execution state afterward
ExecutionResult InterpRun(InterpreterState* is, bool traceExecution, int maxCycles) {
    if (is == NULL) {
        return FailureResult(0);
    }
    DataTag evalResult = {};
    is->_runningVerbose = traceExecution;
    auto programEnd = VectorLength(is->_program);
    int localSteps = 0;

    while (is->_position < programEnd) {
        if (localSteps >= maxCycles) {
            return PausedExecutionResult();
        }
        if (is->ErrorFlag) {
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


        // TODO: Add a range->native array cache and use here. Should help a lot in loops.
        auto wptr = VecGet_DataTag(is->_program, is->_position);
        if (wptr == NULL) break;
        DataTag word = *wptr;

        if (traceExecution) {/* TODO: this stuff, when diagnostic string is done
            _output.WriteLine("          stack :" + string.Join(", ", _valueStack.ToArray().Select(t = > _memory.DiagnosticString(t, DebugSymbols))));
            _output.WriteLine("          #" + _stepsTaken + "; p=" + _position
                + "; w=" + _memory.DiagnosticString(word, DebugSymbols));*/
        }

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
            DecodeOpcode(word, &codeClass, &codeAction, &p1, &p2);
            auto result = ProcessOpCode(codeClass, codeAction, p1, p2, &(is->_position), word, is);
            if (result == DataType::Exception) {
                return FailureResult(is->_position);
            } else if (result == DataType::MustWait) {
                return WaitingExecutionResult();
            }
            programEnd = VectorLength(is->_program); // In case 'eval' changed it
            break;
        }
        case (int)DataType::EndOfSubProgram:
        {
            // Delete back to the 'EndOfProgram' marker
            HandleReturn(&(is->_position), is);
            RollBackSubProgram(is);

            // What is happening with value stack and scope here?
            /*StringAppend(is->_output, "\r\nValue stack at end of eval:\r\n");
            auto vals = is->_valueStack;
            auto len = VecLength(vals);
            for (int i = 0; i < len; i++) {
                auto strVal = CastString(is, *VecGet_DataTag(vals, i));
                StringAppend(is->_output, strVal);
                StringAppend(is->_output, "\r\n");
            }
            StringAppend(is->_output, "###########################\r\n");*/
            break;
        }
        case (int)DataType::EndOfProgram:
            goto GOOD_EXIT;

        default:
            VecPush_DataTag(is->_valueStack, word); // these could be raw doubles, encoded real values, or references/pointers
            break;
        }

        is->_position++;
    }

    // dropped out of the loop without hitting an end-of-program marker.
    // make a note of it:
    StringAppend(is->_output, "Program went out of bounds. Check compiler.");

GOOD_EXIT:
    // Exited the program. Cleanup and return value

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
