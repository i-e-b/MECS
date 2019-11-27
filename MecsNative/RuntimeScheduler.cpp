#include "RuntimeScheduler.h"

// Compiler & runtime requirements
#include "MemoryManager.h"
#include "String.h"
#include "FileSys.h"
#include "SourceCodeTokeniser.h"
#include "CompilerCore.h"
#include "TagCodeInterpreter.h"
#include "TypeCoersion.h"
#include "RuntimeScheduler.h"

typedef uint32_t Name;
RegisterHashMapStatics(Map)
RegisterHashMapFor(Name, StringPtr, HashMapIntKeyHash, HashMapIntKeyCompare, Map)

RegisterVectorFor(InterpreterStatePtr, Vector)
RegisterVectorFor(DataTag, Vector)

typedef struct RuntimeScheduler {
	// Vector<InterpreterState*>
	Vector* interpreters;

	// Outer arena used for scheduler details
	// This holds debug symbors, the vector of interpreters, but not the interpreter working memory.
	Arena* baseMemory;

	// Next interpreter slot that should be used. This may be off the end, and need reset.
	int roundRobin;

	// Last recorded run state
	SchedulerState state;
} RuntimeScheduler;

// Allocate a new scheduler. The scheduler will create its own memory arenas, and those for the interpreters.
RuntimeSchedulerPtr RTSchedulerAllocate(){
	auto coreMem = NewArena(1 MEGABYTE);

	auto result = (RuntimeScheduler*)ArenaAllocateAndClear(coreMem, sizeof(RuntimeScheduler));
	if (coreMem == NULL || result == NULL) return NULL;

	auto intVec = VectorAllocateArena_InterpreterStatePtr(coreMem);
	if (intVec == NULL) {
		DropArena(&coreMem);
		return NULL;
	}

	result->baseMemory = coreMem;
	result->roundRobin = -1;
	result->interpreters = intVec;
	result->state = SchedulerState::Running;

	return result;
}

// Deallocate a scheduler, and any contained interpreter runtimes (mostly by closing the arenas)
void RTSchedulerDeallocate(RuntimeSchedulerPtr* schedHndl) {
	if (schedHndl == NULL) return;

	auto sched = *schedHndl;
	if (sched == NULL) return;

	if (sched->interpreters != NULL) {
		InterpreterState *interp;
		while (VectorPop_InterpreterStatePtr(sched->interpreters, &interp)) {
			InterpDeallocate(interp);
		}
	}
	
	DropArena(&(sched->baseMemory));
	schedHndl = NULL;
}


Vector* RTS_Compile(StringPtr filename, HashMap *symbols, Arena* symbolStringStorage) {
	// Load and compile
	auto program = VectorAllocate_DataTag();

	// Compile and load
	MMPush(10 MEGABYTES);
	auto code = StringEmpty();
	auto vec = StringGetByteVector(code);
	uint64_t read = 0;
	if (!FileLoadChunk(filename, vec, 0, 10000, &read)) {
		return NULL;
	}

	auto compilableSyntaxTree = ParseSourceCode(code, false);
	auto tagCode = CompileRoot(compilableSyntaxTree, false, false);

	auto nextPos = TCW_AppendToVector(tagCode, program);
	if (symbols != NULL) {
		TCW_GetSymbolsTo(tagCode, symbols, symbolStringStorage);
	}

	StringDeallocate(code);
	DeallocateAST(compilableSyntaxTree);
	TCW_Deallocate(tagCode);
	MMPop();

	return program;
}


// Read, compile and add a program to the execution schedule
// Returns false if there were any errors loading
bool RTSchedulerAddProgram(RuntimeSchedulerPtr sched, StringPtr filePath){
	if (sched == NULL || filePath == NULL) return false;
	
	auto symbols = MapAllocateArena_Name_StringPtr(16, sched->baseMemory);
	auto code = RTS_Compile(filePath, symbols, sched->baseMemory); // compile to bytecode in the ambient Memory Manager space
	if (code == NULL) return false;
    auto prog = InterpAllocate(code, 10 MEGABYTE, symbols); // copy bytecode into a new interpreter
    VectorDeallocate(code); // deallocate from ambient memory
	if (prog == NULL) return false;

	// Store the interpreter
	VectorPush_InterpreterStatePtr(sched->interpreters, prog);

	return true;
}

// Helper for returning fault states
int Fault(RuntimeSchedulerPtr sched, int line) {
	if (sched != NULL) {
		sched->state = SchedulerState::Faulted;
	}
	return line;
}

constexpr auto OK = 0;
constexpr auto ALL_COMPLETE = -1;
constexpr auto DROP_THRU = -2;

// Run ONE of the scheduled programs for a given number of rounds.
// Each time you call this, a different program may be given the rounds.
// Will return false if there is a fault or all programs have ended.
// (use `RTSchedulerState` function to get a flag, and the `RTSchedulerProgramStatistics` function to get detailed states)
int RTSchedulerRun(RuntimeSchedulerPtr sched, int rounds, StringPtr consoleOut) {
	if (sched == NULL) return Fault(sched, __LINE__);

	// Advance the schedule
	sched->roundRobin++;

	// ensure we're in bounds
	int max = VectorLength(sched->interpreters);
	if (max < 1) return Fault(sched, __LINE__);
	if (sched->roundRobin >= max) { sched->roundRobin = 0; }

	// find the interpreter
	auto isp = VectorGet_InterpreterStatePtr(sched->interpreters, sched->roundRobin);
	if (isp == NULL || *isp == NULL) return Fault(sched, __LINE__);
	auto is = *isp;

	// check state and run if appropriate
	// if not, we do nothing and return true (expecting the scheduler to be run endlessly)
	auto state = InterpreterCurrentState(is);
	ExecutionResult result;
	switch (state) {
		// Valid run states
		case ExecutionState::Paused: // normal stop for time-slice
		case ExecutionState::Waiting: // waiting for console data. Will loop if not enough data
		case ExecutionState::IPC_Ready: // was waiting, now has data
		case ExecutionState::IPC_Send: // requested a send, can now continue
			result = InterpRun(is, rounds);
			break;

		// Valid wait states
		case ExecutionState::Complete:
		case ExecutionState::Running:
		case ExecutionState::IPC_Wait:
			return OK;

		// Fail states
		default:
			return Fault(sched, __LINE__);
	}

	// Move output
	ReadOutput(is, consoleOut);

	// check the result state
	// if there is IPC data, dish it out
	switch (result.State) {
		// Invalid stop states
		case ExecutionState::ErrorState:
			return Fault(sched, __LINE__);

		case ExecutionState::Running:
			return Fault(sched, __LINE__);

		// Broadcast IPC to all programs (including self)
		case ExecutionState::IPC_Send:
		{
			if (result.IPC_Out_Target == NULL || result.IPC_Out_Data == NULL) return Fault(sched, __LINE__); // invalid IPC call
			int length = VectorLength(sched->interpreters);
			for (int i = 0; i < length; i++) {
				auto target = VectorGet_InterpreterStatePtr(sched->interpreters, i);
				if (target == NULL || *target == NULL) return Fault(sched, __LINE__);

				bool ok = InterpAddIPC(*target, result.IPC_Out_Target, result.IPC_Out_Data);
				if (!ok) return Fault(sched, __LINE__);
			}

			// deallocate IPC data
			VectorDeallocate(result.IPC_Out_Data);
			StringDeallocate(result.IPC_Out_Target);
			return OK;
		}
		break;

		// The program ended. Check to see if any others are running
		case ExecutionState::Complete:
		{
			// Check to see if all programs have finished.
			// If so, return non-success.
			int length = VectorLength(sched->interpreters);
			for (int i = 0; i < length; i++) {
				auto target = VectorGet_InterpreterStatePtr(sched->interpreters, i);
				if (target == NULL || *target == NULL) return Fault(sched, __LINE__);
				bool done = InterpreterCurrentState(*target) == ExecutionState::Complete;
				if (!done) return OK; // at least one more to run
			}
			sched->state = SchedulerState::Complete;
			return ALL_COMPLETE;
		}
		break;

		// Valid stop states
		default:
			return OK;
	}

	return DROP_THRU; // Shouldn't actually hit this
}

// Write a description of the compiled code to a string
void RTSchedulerDebugDump(RuntimeSchedulerPtr sched, StringPtr target) {
	if (sched == NULL || target == NULL) return;

	int length = VectorLength(sched->interpreters);
	for (int i = 0; i < length; i++) {
		auto interp = VectorGet_InterpreterStatePtr(sched->interpreters, i);
		if (interp == NULL || *interp == NULL) continue;

		StringAppend(target, "\nCode for program #");
		StringAppendInt32(target, i);
		StringAppend(target, "\n\n");

		InterpDescribeCode(*interp, target);
	}
}

int RTSchedulerLastProgramIndex(RuntimeSchedulerPtr sched) {
	if (sched == NULL) return -1;
	return sched->roundRobin;
}

// Return a state for the scheduler
SchedulerState RTSchedulerState(RuntimeSchedulerPtr sched){
	if (sched == NULL) return SchedulerState::Faulted;
	return sched->state;
}

