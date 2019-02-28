#pragma once

#ifndef tagcodeinterpreter_h
#define tagcodeinterpreter_h

#include "TagData.h"
#include "Vector.h"
#include "Scope.h"

enum class ExecutionState {
    // Program could continue, but stopped by request (debug, step, etc.)
    Paused,
    // Program stopped its own to wait for input (messages, console in, etc.)
    Waiting,
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
InterpreterState* InterpAllocate(Vector* tagCode, size_t memorySize, HashMap* debugSymbols);

// Close down an interpreter and free all memory
void InterpDeallocate(InterpreterState* is);

// Run the interpreter until end or cycle count (whichever comes first)
// Remember to check execution state afterward
ExecutionResult InterpRun(InterpreterState* is, bool traceExecution, int maxCycles);

// Add IPC messages to an InterpreterState (only when it's not running)
void InterpAddIPC(InterpreterState* is, Vector* ipcMessages);

// Add string data to the waiting input stream
void WriteInput(InterpreterState* is, String* str);

// Move output data to the supplied string
void ReadOutput(InterpreterState* is, String* receiver);

// Convert a tag code offset into a physical memory location
void* InterpreterDeref(InterpreterState* is, uint32_t position);

// Get the variables scope of the interpreter instance
Scope* InterpreterScope(InterpreterState* is);

// Store a new string at the end of memory, and return a string pointer token for it
DataTag StoreStringAndGetReference(InterpreterState* is, String* str);

// Store a new string at the end of memory, and return a string pointer token for it
DataTag StoreStringAndGetReference(InterpreterState* is, char c);

#endif
