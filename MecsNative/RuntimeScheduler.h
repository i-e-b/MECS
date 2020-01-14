#pragma once

/*
   The run-time scheduler handles execution of multiple programs in parallel,
   and the inter-process communication between them
*/

#include "String.h"
#include "Vector.h"
#include "TagCodeInterpreter.h"

#ifndef runtimescheduler_h
#define runtimescheduler_h

typedef struct RuntimeScheduler RuntimeScheduler;
typedef RuntimeScheduler* RuntimeSchedulerPtr;


enum class SchedulerState {
	// The scheduler has no faults, and at least one program that can still run
	Running = 1,
	// At least one program has entered a faulted state
	Faulted = 2,
	// All programs have run to completion
	Complete = 3
};

// Allocate a new scheduler. The scheduler will create its own memory arenas, and those for the interpreters.
RuntimeSchedulerPtr RTSchedulerAllocate();

// Deallocate a scheduler, and any contained interpreter runtimes (mostly by closing the arenas)
void RTSchedulerDeallocate(RuntimeSchedulerPtr *sched);

// Read, compile and add a program to the execution schedule
// Returns false if there were any errors loading
// If the `processId` string is provided, it will have the process instance unique ID appended to it.
bool RTSchedulerAddProgram(RuntimeSchedulerPtr sched, StringPtr filePath, StringPtr processId);

// Run ONE of the scheduled programs for a given number of rounds.
// Each time you call this, a different program may be given the rounds.
// Will return non-zero if there is a fault or all programs have ended.
// (use `RTSchedulerState` function to get a flag, and the `RTSchedulerProgramStatistics` function to get detailed states)
// `consoleOut` is optional. If supplied, it will be filled with console data from the run program. If not, the program's console will be cleared.
int RTSchedulerRun(RuntimeSchedulerPtr sched, int rounds, StringPtr consoleOut);

// Return a state for the scheduler
SchedulerState RTSchedulerState(RuntimeSchedulerPtr sched);

// Return the index of the last program that ran
int RTSchedulerLastProgramIndex(RuntimeSchedulerPtr sched);

// Write a description of the compiled code to a string
void RTSchedulerDebugDump(RuntimeSchedulerPtr sched, StringPtr target);

#endif
