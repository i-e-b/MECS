#pragma once

/*
   The run-time scheduler handles execution of multiple programs in parallel,
   and the inter-process communication between them
*/

#ifndef runtimescheduler_h
#define runtimescheduler_h

#include "String.h"
#include "Vector.h"
#include "TagCodeInterpreter.h"

typedef struct RuntimeScheduler RuntimeScheduler;
typedef RuntimeScheduler* RuntimeSchedulerPtr;

typedef struct ProgramState {
	ExecutionState runState;
	InterpreterState *interpreter;
} ProgramState;


enum class SchedulerState {
	// The scheduler has no faults, and at least one program that can still run
	Running,
	// At least one program has entered a faulted state
	Faulted,
	// All programs have run to completion
	Complete
};

// Allocate a new scheduler. The scheduler will create its own memory arenas, and those for the interpreters.
RuntimeSchedulerPtr RTSchedulerAllocate();

// Deallocate a scheduler, and any contained interpreter runtimes (mostly by closing the arenas)
void RTSchedulerDeallocate(RuntimeSchedulerPtr *sched);

// Read, compile and add a program to the execution schedule
// Returns false if there were any errors loading
bool RTSchedulerAddProgram(RuntimeSchedulerPtr sched, StringPtr filePath);

// Run ONE of the scheduled programs for a given number of rounds.
// Each time you call this, a different program may be given the rounds.
// Will return false if there is a fault or all programs have ended.
// (use `RTSchedulerState` function to get a flag, and the `RTSchedulerProgramStatistics` function to get detailed states)
bool RTSchedulerRun(RuntimeSchedulerPtr sched, int rounds);

// Return a state for the scheduler
SchedulerState RTSchedulerState(RuntimeSchedulerPtr sched);

// Return a Vector<ProgramState>, with entries for each program in the schedule
VectorPtr RTSchedulerProgramStatistics();

#endif
