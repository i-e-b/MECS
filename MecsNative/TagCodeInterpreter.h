#pragma once

#ifndef tagcodeinterpreter_h
#define tagcodeinterpreter_h

#include "TagData.h"
#include "Vector.h"
#include "Scope.h"

enum class ExecutionState {
    // Program could continue, but stopped by request (debug, step, etc.)
    Paused,
    // Program stopped to wait for console input
    Waiting,
    // Program ran to completion
    Complete,
    // Program is currently running
    Running,
    // Program failed with an error
    ErrorState,

	// Program is waiting for an IPC message (InterpreterState.IPC_Queue_WaitFlags should be populated)
	IPC_Wait,
	// Program wants to send an IPC message (ExecutionResult.IPC_Out_Target and ExecutionResult.IPC_Out_Data should be populated)
	IPC_Send,
	// Program can continue after IPC_Wait state
	IPC_Ready,

    // Program would like to run another program in the background. (ExecutionResult.IPC_Out_Target should contain the path to the source code file)
    IPC_Spawn
};
typedef struct ExecutionResult {
    // If/how the interpreter stopped
    ExecutionState State;
    // If completed, the result of execution
    DataTag Result;

	// To send a message, the execution state must be `IPC_Send`, and both `IPC_Out_Target` and `IPC_Out_Data` must be set.
	// If not null, the target name for an outgoing message.
	String* IPC_Out_Target;
    // If not null, the serialised data to be send to other programs
    Vector* IPC_Out_Data;
} ExecutionResult;

// Runtime state and stack for the interpreter
typedef struct InterpreterState InterpreterState;
typedef InterpreterState* InterpreterStatePtr;

// Start up an interpreter.
// tagCode is Vector<DataTag>, debugSymbols in Map<CrushName -> StringPtr>.
// memory size must be enough for value and return stack, but does not include tagCode size
InterpreterState* InterpAllocate(Vector* tagCode, size_t memorySize, HashMap* debugSymbols);

// Close down an interpreter and free all memory
void InterpDeallocate(InterpreterState* is);

// Return the most recent interpreter state
ExecutionState InterpreterCurrentState(InterpreterState* is);

// Run the interpreter until end or cycle count (whichever comes first)
// Remember to check execution state afterward
ExecutionResult InterpRun(InterpreterState* is, int maxCycles);

// Set an ID for this interpreter. Used by the scheduler.
void InterpSetId(InterpreterState* is, int id);

// Try to add an incoming IPC message to an InterpreterState.
// Only call when the program is in a wait state.
// The call will ignore the request if it is not interested in the target type
// returns false if there was not enough memory to store the event.
bool InterpAddIPC(InterpreterState* is, String* targetName, Vector* ipcMessageData);

// Return a vector of IPC targets the interpreter is waiting for. Caller should dealloc the vector but not the strings.
// Returns Vector<StringPtr>
Vector* InterpWaitingIPC(InterpreterState* is);

// Add string data to the waiting input stream
void WriteInput(InterpreterState* is, String* str);

// Move output data to the supplied string
// If the supplied string is null, the programs output will be cleared.
void ReadOutput(InterpreterState* is, String* receiver);

// Convert a tag code offset into a physical memory location
void* InterpreterDeref(InterpreterState* is, DataTag encodedPosition);

// Get the variables scope of the interpreter instance
Scope* InterpreterScope(InterpreterState* is);

// Push a value onto the interpreter's value stack. This should only be done
// when the interpreter is paused. Used by the scheduler to feed process IDs.
bool InterpreterPushValue(InterpreterState* is, DataTag value);

// Store a new string in the RW memory, and return a string pointer token for it
// The original string parameter is deallocated
DataTag StoreStringAndGetReference(InterpreterState* is, String* str);

// Read and return a copy of the opcode at the given index
DataTag GetOpcodeAtIndex(InterpreterState* is, uint32_t index);

// Read RO tag code memory as a string
String* ReadStaticString(InterpreterState* is, int position, int length);

// ONLY FOR DEBUG USE!!
Arena* InterpInternalMemory(InterpreterState* is);

// Append a string description of opcodes to the target
void InterpDescribeCode(InterpreterState* is, String* target);

#endif
