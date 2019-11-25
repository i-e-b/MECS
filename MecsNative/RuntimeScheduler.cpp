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
	result->roundRobin = 0;
	result->interpreters = intVec;

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
	if (prog == NULL) return NULL;

	return true;
}

// Run ONE of the scheduled programs for a given number of rounds.
// Each time you call this, a different program may be given the rounds.
// Will return false if there is a fault or all programs have ended.
// (use `RTSchedulerState` function to get a flag, and the `RTSchedulerProgramStatistics` function to get detailed states)
bool RTSchedulerRun(RuntimeSchedulerPtr sched, int rounds){

}

// Return a state for the scheduler
SchedulerState RTSchedulerState(RuntimeSchedulerPtr sched){

}

// Return a Vector<ProgramState>, with entries for each program in the schedule
VectorPtr RTSchedulerProgramStatistics(){

}

