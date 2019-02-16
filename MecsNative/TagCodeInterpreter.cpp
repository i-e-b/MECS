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

ExecutionResult FailureResult() {
    ExecutionResult r = {};
    r.Result = InvalidTag();
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

String* DiagnosticString(DataTag tag, InterpreterState* is) {
    // TODO!!
    return StringEmpty();
}

Vector* ReadParams(int position, uint16_t nbParams, Vector* valueStack) {
    auto param = new double[nbParams];
    // Pop values from stack into a param cache
    try {
        for (int i = nbParams - 1; i >= 0; i--) {
            param[i] = valueStack.Pop();
        }
    } catch (Exception ex) {
        throw new Exception("Stack underflow. Ran out of values before function call at " + position, ex);
    }

    return param;
}

void PrepareFunctionCall(int* position, uint16_t p1, InterpreterState* is) {
    DataTag nameTag = {};
    VecPop_DataTag(is->_valueStack, &nameTag);
    auto functionNameHash = DecodeVariableRef(nameTag);

    auto param = ReadParams(position, nbParams, valueStack);

    // Evaluate function.
    var evalResult = EvaluateFunctionCall(ref position, functionNameHash, nbParams, param, returnStack, valueStack);

    // Add result on stack as a value.
    if (NanTags.TypeOf(evalResult) != DataType.NoValue)
    {
        valueStack.Push(evalResult);
    } else if (NanTags.DecodeNonValue(evalResult) == NonValueType.Unit) {
        valueStack.Push(evalResult);
    }

    return position;
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
        return FailureResult();
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
                return FailureResult();
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
