#include "TagCodeInterpreter.h"

#include "HashMap.h"
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

    if (is->Functions != NULL) MapDeallocate(is->Functions);
    if (is->_returnStack != NULL) VecDeallocate(is->_returnStack);
    if (is->_valueStack != NULL) VecDeallocate(is->_valueStack);
    if (is->_input != NULL) StringDeallocate(is->_input);
    if (is->_output != NULL) StringDeallocate(is->_output);

    mfree(is);
}

// Run the interpreter until end or cycle count (whichever comes first)
// Remember to check execution state afterward
ExecutionResult InterpRun(InterpreterState* is, int maxCycles) {

}

// Add IPC messages to an InterpreterState (only when it's not running)
void InterpAddIPC(InterpreterState*is, Vector* ipcMessages) {

}
