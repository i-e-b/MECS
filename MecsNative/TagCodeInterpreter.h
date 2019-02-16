#pragma once

#ifndef tagcodeinterpreter_h
#define tagcodeinterpreter_h

#include "TagData.h"
#include "Vector.h"

enum class ExecutionState {
    // Program could continue, but stopped by request (debug, step, etc.)
    Paused,
    // Program ran to completion
    Complete,
    // Program is currently running
    Running,
    // Program failed with an error
    ErrorState
};
typedef struct ExecutionResult {
    // If/how the interpreter stopped
    ExecutionState State;
    // If completed, the result of execution
    DataTag Result;
    // If not null, the execution sent messages in this vector (TODO)
    Vector* IPC;
} ExecutionResult;

// Runtime state and stack for the interpreter
typedef struct InterpreterState InterpreterState;

// Start up an interpreter.
// tagCode is Vector<DataTag>, debugSymbols in Map<CrushName -> StringPtr>.
InterpreterState* InterpAllocate(Vector* tagCode, HashMap* debugSymbols);

// Close down an interpreter and free all memory
void InterpDeallocate(InterpreterState* is);

// Run the interpreter until end or cycle count (whichever comes first)
// Remember to check execution state afterward
ExecutionResult InterpRun(InterpreterState* is, int maxCycles);

// Add IPC messages to an InterpreterState (only when it's not running)
void InterpAddIPC(InterpreterState* is, Vector* ipcMessages);

// Add string data to the waiting input stream
void WriteInput(InterpreterState* is, String* str);

// Move output data to the supplied string
void ReadOutput(InterpreterState* is, String* receiver);

#endif