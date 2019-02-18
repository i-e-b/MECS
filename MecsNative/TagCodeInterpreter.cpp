#include "TagCodeInterpreter.h"

#include "HashMap.h"
#include "Scope.h"
#include "TagCodeTypes.h"
#include "MemoryManager.h"

typedef uint32_t Name;

RegisterHashMapStatics(Map)
RegisterHashMapFor(Name, StringPtr, HashMapIntKeyHash, HashMapIntKeyCompare, Map)
RegisterHashMapFor(Name, FunctionDefinition, HashMapIntKeyHash, HashMapIntKeyCompare, Map)

RegisterVectorStatics(Vec)
RegisterVectorFor(DataTag, Vec)
RegisterVectorFor(int, Vec)

/*
    Our interpreter is a two-stack model
*/
typedef struct InterpreterState {
    Vector* _valueStack; // Stack<DataTag>
    Vector* _returnStack; // Stack<int>

    String* _input;
    String* _output;

    // the string table and opcodes
    Vector* _program; // Vector<DataTag>
    Scope* _memory;

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

// map built-in functions for the massive switch
void AddBuiltInFunctionSymbols(HashMap* fd) {
    if (fd == NULL) return;
#define add(name,type)  MapPut_Name_FunctionDefinition(fd, GetCrushedName(name), FunctionDefinition{type}, true);

    add("=", FuncDef::Equal); add("equals", FuncDef::Equal); add(">", FuncDef::GreaterThan);
    add("<", FuncDef::LessThan); add("<>", FuncDef::NotEqual); add("not-equal", FuncDef::NotEqual);
    add("assert", FuncDef::Assert); add("random", FuncDef::Random); add("eval", FuncDef::Eval);
    add("call", FuncDef::Call); add("not", FuncDef::LogicNot); add("or", FuncDef::LogicOr);
    add("and", FuncDef::LogicAnd); add("readkey", FuncDef::ReadKey); add("readline", FuncDef::ReadLine);
    add("print", FuncDef::Print); add("substring", FuncDef::Substring);
    add("length", FuncDef::Length); add("replace", FuncDef::Replace); add("concat", FuncDef::Concat);
    add("+", FuncDef::MathAdd); add("-", FuncDef::MathSub); add("*", FuncDef::MathProd);
    add("/", FuncDef::MathDiv); add("%", FuncDef::MathMod);
    add("()", FuncDef::UnitEmpty); // empty value marker
#undef add;
}

// Start up an interpreter
// tagCode is Vector<DataTag>, debugSymbols in Map<CrushName -> StringPtr>.
InterpreterState* InterpAllocate(Vector* tagCode, HashMap* debugSymbols) {
    if (tagCode == NULL) return NULL;
    auto result = (InterpreterState*)mmalloc(sizeof(InterpreterState));
    if (result == NULL) return NULL;

    result->Functions = MapAllocate_Name_FunctionDefinition(128);
    result->DebugSymbols = debugSymbols; // ok if NULL

    result->_program = tagCode;
    result->_memory = ScopeAllocate();

    result->_position = 0;
    result->_stepsTaken = 0;
    result->_runningVerbose = false;

    result->_returnStack = VecAllocate_int();
    result->_valueStack = VecAllocate_DataTag();

    result->_input = StringEmpty();
    result->_output = StringEmpty();

    if ((result->Functions == NULL)
        || (result->_returnStack == NULL)
        || (result->_valueStack == NULL)
        || (result->_input == NULL)
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

    if (is->_memory != NULL) ScopeDeallocate(is->_memory);
    if (is->Functions != NULL) MapDeallocate(is->Functions);
    if (is->_returnStack != NULL) VecDeallocate(is->_returnStack);
    if (is->_valueStack != NULL) VecDeallocate(is->_valueStack);
    if (is->_input != NULL) StringDeallocate(is->_input);
    if (is->_output != NULL) StringDeallocate(is->_output);

    mfree(is);
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
    // TODO!!
    return StringEmpty();
}

DataTag* ReadParams(int position, uint16_t nbParams, Vector* valueStack) {
    DataTag* param = (DataTag*)mcalloc(nbParams, sizeof(DataTag)); //new double[nbParams];
    // Pop values from stack into a param cache
    for (int i = nbParams - 1; i >= 0; i--) {
        VecPop_DataTag(valueStack, &(param[i]));
    }

    return param;
}

// Defined at bottom
DataTag EvaluateBuiltInFunction(int* position, FuncDef kind, int nbParams, DataTag* param, InterpreterState* is);

DataTag EvaluateFunctionCall(int* position, int functionNameHash, int nbParams, DataTag* param, InterpreterState* is) {
    FunctionDefinition* fun = NULL;
    bool found = MapGet_Name_FunctionDefinition(is->Functions, functionNameHash, &fun);

    if (!found) {

        StringAppendFormat(is->_output,
            "Tried to call an undefined function '\x01' at position \x02\n", DbgStr(is, functionNameHash), position);
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
    ScopePush(is->_memory, param, nbParams); // write parameters into new scope
    VecPush_int(is->_returnStack, *position); // set position for 'cret' call
    *position = fun->StartPosition; // move pointer to start of function
    return VoidReturn(); // return no value, continue execution elsewhere

}

void PrepareFunctionCall(int* position, uint16_t nbParams, InterpreterState* is) {
    DataTag nameTag = {};
    VecPop_DataTag(is->_valueStack, &nameTag);
    auto functionNameHash = DecodeVariableRef(nameTag);

    auto param = ReadParams(is->_position, nbParams, is->_valueStack);

    // Evaluate function.
    DataTag evalResult = EvaluateFunctionCall(position, functionNameHash, nbParams, param, is);

    // Add result on stack as a value.
    if (evalResult.type != (int)DataType::Not_a_Result) {
        VecPush_DataTag(is->_valueStack, evalResult);
    }
}
void HandleFunctionDefinition(int* position, uint16_t p1, uint16_t p2, InterpreterState* is) {
    // TODO
}
void HandleControlSignal(int* position, char codeAction, int opCodeCount, InterpreterState* is) {
    // TODO
}
void HandleCompoundCompare(int* position, char  codeAction, uint16_t p1, uint16_t p2, InterpreterState* is) {
    // TODO
}
void HandleMemoryAccess(int* position, char codeAction, int varRef, uint16_t p1, InterpreterState* is) {
    // TODO
}

// dispatch for op codes. valueStack is Vector<DataTag>, returnStack is Vector<int>
bool ProcessOpCode(char codeClass, char codeAction, uint16_t p1, uint16_t p2, int* position, DataTag word, InterpreterState* is) {
    uint32_t varRef;
    switch (codeClass)
    {
    case 'f': // Function operations
        if (codeAction == 'c') {
            PrepareFunctionCall(position, p1, is);
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
        HandleCompoundCompare(position, codeAction, p1, p2, is);
        break;

    case 'm': // Memory access - get|set|isset|unset
        varRef = p2 + (p1 << 16);
        HandleMemoryAccess(position, codeAction, varRef, p1, is);
        break;

    case 'i': // special 'increment' operator. Uses the 'codeAction' slot to hold a small signed number
        varRef = p2 + (p1 << 16);
        ScopeMutateNumber(is->_memory, varRef, (int8_t)codeAction);
        break;

    case 's': // reserved for System operation.
        StringAppendFormat(is->_output, "Unimplemented System operation op code at \x02 : '\x01'\n", position, DiagnosticString(word, is));
        return false;

    default:
        StringAppendFormat(is->_output, "Unexpected op code at \x02 : '\x01'\n", position, DiagnosticString(word, is));
        return false;
    }
    return true;
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
        if (localSteps > maxCycles) {
            return PausedExecutionResult();
        }

        is->_stepsTaken++;
        localSteps++;

        // Prevent stackoverflow.
        // Ex: if(true 1 10 20)
        if ((is->_stepsTaken & 127) == 0 && VecLength(is->_valueStack) > 100) { // TODO: improve this mess. Might be able to use void returns (and add them to loops?)
            for (int i = 0; i < 100; i++) {
                VectorDequeue(is->_valueStack, NULL); // knock values off the far side of the stack.
            }
        }

        DataTag word = *VecGet_DataTag(is->_program, is->_position);

        if (traceExecution) {/* TODO: this stuff!
            _output.WriteLine("          stack :" + string.Join(", ", _valueStack.ToArray().Select(t = > _memory.DiagnosticString(t, DebugSymbols))));
            _output.WriteLine("          #" + _stepsTaken + "; p=" + _position
                + "; w=" + _memory.DiagnosticString(word, DebugSymbols));*/
        }

        switch (word.type) {
        case (int)DataType::Invalid:
        {
            StringAppendFormat(is->_output, "Unknown code point at \x02\n", is->_position);
        }

        case (int)DataType::Opcode:
            // decode opcode and do stuff
            char codeClass, codeAction;
            uint16_t p1, p2;
            DecodeOpcode(word, &codeClass, &codeAction, &p1, &p2);
            if (!ProcessOpCode(codeClass, codeAction, p1, p2, &(is->_position), word, is)) {
                return FailureResult(is->_position);
            }
            break;

        default:
           VecPush_DataTag(is->_valueStack, word); // these could be raw doubles, encoded real values, or references/pointers
            break;
        }

        is->_position++;
    }

    // Exited the program. Cleanup and return value

    if (VecLength(is->_valueStack) != 0) {
        VecPop_DataTag(is->_valueStack, &evalResult);
    } else {
        evalResult = VoidReturn();
    }

    // reset state, just incase we get run again
    VecClear(is->_returnStack);
    VecClear(is->_valueStack);
    StringClear(is->_input);
    StringClear(is->_output);
    is->_position = 0;

    return CompleteExecutionResult(evalResult);
}

// Add IPC messages to an InterpreterState (only when it's not running)
void InterpAddIPC(InterpreterState*is, Vector* ipcMessages) {

}

DataTag _Exception(InterpreterState* is, const char* msg) {
    StringAppend(is->_output, msg);
    return RuntimeError(is->_position);
}

bool ListEquals(int nbParams, DataTag* param) {
    if (nbParams < 1) return false;
    auto type = param[0].type;
    switch (type)
    {
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
        var target = _memory.CastDouble(list[0]);
        for (int i = 1; i < list.Length; i++)
        {
            if (Math.Abs(target - _memory.CastDouble(list[i])) <= ComparisonPrecision) return true;
        }
        return false;
    }

    // String types
    case (int)DataType::SmallString:
    case (int)DataType::StaticStringPtr:
    case (int)DataType::StringPtr:
    {
        var target = _memory.CastString(list[0]);
        for (int i = 1; i < list.Length; i++)
        {
            if (target == _memory.CastString(list[i])) return true;
        }
        return false;
    }

    // Pointer equality
    case (int)DataType::VariableRef:
    case (int)DataType::HashtablePtr:
    case (int)DataType::VectorPtr:
    {
        var target = NanTags.DecodeRaw(list[0]);
        for (int i = 1; i < list.Length; i++)
        {
            if (target == NanTags.DecodeRaw(list[i])) return true;
        }
        return false;
    }

    default:
        return false;
    }
}


DataTag EvaluateBuiltInFunction(int* position, FuncDef kind, int nbParams, DataTag* param, InterpreterState* is) {
    switch (kind)
    {
        // each element equal to the first
    case FuncDef::Equal:
        if (nbParams < 2) return _Exception(is, "equals ( = ) must have at least two things to compare");
        return EncodeBool(ListEquals(nbParams, param));

        // Each element smaller than the last
    case FuncDef::GreaterThan:
        if (nbParams < 2) return _Exception(is, "greater than ( > ) must have at least two things to compare");
        return EncodeBool(FoldGreaterThan(param));

        // Each element larger than the last
    case FuncDef::LessThan:
        if (nbParams < 2) return _Exception(is, "less than ( < ) must have at least two things to compare");
        return EncodeBool(FoldLessThan(param));

        // Each element DIFFERENT TO THE FIRST (does not check set uniqueness!)
    case FuncDef::NotEqual:
        if (nbParams < 2) return _Exception(is, "not-equal ( <> ) must have at least two things to compare");
        return EncodeBool(!ListEquals(param));

    case FuncDef::Assert:
        if (nbParams < 1) return VoidReturn(); // assert nothing passes
        var condition = param.ElementAt(0);
        if (_memory.CastBoolean(condition) == false)
        {
            var msg = ConcatList(param, 1);
            return _Exception(is, "Assertion failed: " + msg);
        }
        return VoidReturn();

    case FuncDef::Random:
        if (nbParams < 1) return rnd.NextDouble(); // 0 params - any size
        if (nbParams < 2) return rnd.Next(_memory.CastInt(param.ElementAt(0))); // 1 param  - max size
        return rnd.Next(_memory.CastInt(param.ElementAt(0)), _memory.CastInt(param.ElementAt(1))); // 2 params - range

    case FuncDef::Eval:
        var reader = new SourceCodeTokeniser();
        var statements = _memory.CastString(param.ElementAt(0));
        var programTmp = reader.Read(statements, false);
        var bin = ToNanCodeCompiler.CompileRoot(programTmp, false);
        var interpreter = new ByteCodeInterpreter();
        interpreter.Init(new RuntimeMemoryModel(bin, _memory.Variables), _input, _output, DebugSymbols); // todo: optional other i/o for eval?
        return interpreter.Execute(false, _runningVerbose, false).Result;

    case FuncDef::Call:
        DecodePointer(param.ElementAt(0), out var target, out var type);
        if (type != DataType.PtrString && type != DataType.PtrStaticString)
            return _Exception(is, "Tried to call a function by name, but passed a '" + type + "' at " + position);
        // this should be a string, but we need a function name hash -- so calculate it:
        var strName = _memory.DereferenceString(target);
        var functionNameHash = NanTags.GetCrushedName(strName);
        nbParams--;
        var newParam = param.Skip(1).ToArray();
        return EvaluateFunctionCall(ref position, functionNameHash, nbParams, newParam, returnStack, valueStack);

    case FuncDef::LogicNot:
        if (nbParams != 1) return _Exception(is, "'not' should be called with one argument");
        var bval = _memory.CastBoolean(param.ElementAt(0));
        return EncodeBool(!bval);

    case FuncDef::LogicOr:
    {
        bool more = nbParams > 0;
        int i = 0;
        while (more)
        {
            var bresult = _memory.CastBoolean(param.ElementAt(i));
            if (bresult) return NanTags.EncodeBool(true);

            i++;
            more = i < nbParams;
        }

        return NanTags.EncodeBool(false);
    }

    case FuncDef::LogicAnd:
    {
        bool more = nbParams > 0;
        int i = 0;
        while (more)
        {
            var bresult = _memory.CastBoolean(param.ElementAt(i));
            if (!bresult) return NanTags.EncodeBool(false);

            i++;
            more = i < nbParams;
        }

        return NanTags.EncodeBool(true);
    }

    case FuncDef::ReadKey:
        return _memory.StoreStringAndGetReference(((char)_input.Read()).ToString());

    case FuncDef::ReadLine:
        return _memory.StoreStringAndGetReference(_input.ReadLine());

    case FuncDef::Print:
    {
        string lastStr = null;
        foreach(var v in param)
        {
            lastStr = _memory.CastString(v);
            _output.Write(lastStr);
        }

        if (lastStr != "") _output.WriteLine();
    }
    return NanTags.VoidReturn();

    case FuncDef::Substring:
        if (nbParams == 2)
        {

            var newString = _memory.CastString(param.ElementAt(0)).Substring(_memory.CastInt(param.ElementAt(1)));
            return _memory.StoreStringAndGetReference(newString);
        } else if (nbParams == 3)
        {
            int start = _memory.CastInt(param.ElementAt(1));
            int length = _memory.CastInt(param.ElementAt(2));

            string s = _memory.CastString(param.ElementAt(0)).Substring(start, length);
            return _memory.StoreStringAndGetReference(s);
        } else {
            return _Exception(is, "'Substring' should be called with 2 or 3 parameters");
        }

    case FuncDef::Length:
        return _memory.CastString(param.ElementAt(0)).Length; // TODO: lengths of other things

    case FuncDef::Replace:
        if (nbParams != 3) return _Exception(is, "'Replace' should be called with 3 parameters");
        string exp = _memory.CastString(param.ElementAt(0));
        string oldValue = _memory.CastString(param.ElementAt(1));
        string newValue = _memory.CastString(param.ElementAt(2));
        exp = exp.Replace(oldValue, newValue);
        return _memory.StoreStringAndGetReference(exp);

    case FuncDef::Concat:
        var builder = new StringBuilder();

        foreach(var v in param)
        {
            builder.Append(_memory.CastString(v));
        }

        return _memory.StoreStringAndGetReference(builder.ToString());

    case FuncDef::UnitEmpty:
    { // valueless marker (like an empty object)
        return NanTags.EncodeNonValue(NonValueType.Unit);
    }


    case FuncDef::MathAdd:
        if (nbParams == 1) return param[0];
        return param.ChainSum();

    case FuncDef::MathSub:
        if (nbParams == 1) return -param[0];
        return param.ChainDifference();

    case FuncDef::MathProd:
        if (nbParams == 1) return _Exception(is, "Uniary '*' is not supported");
        return param.ChainProduct();

    case FuncDef::MathDiv:
        if (nbParams == 1) return _Exception(is, "Uniary '/' is not supported");
        return param.ChainDivide();

    case FuncDef::MathMod:
        if (nbParams == 1) return param[0] % 2;
        return param.ChainRemainder();

    default:
        return _Exception(is, "Unrecognised built-in! Type = " + ((int)kind));
    }
}