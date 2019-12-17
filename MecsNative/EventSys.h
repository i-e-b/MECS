#pragma once

#ifndef eventsys_h
#define eventsys_h

#include "Vector.h"
#include "String.h"

// Start the event system. This should be called before the scheduler
bool EventSystem_Start();

// Non-blocking check for events.
// Returns true if events are available.
// If `target` and `data` should be supplied by the caller, and will be populated with event details
// that can be used for the runtime scheduler to drive MECS IPC.
bool EventPoll(StringPtr target, VectorPtr data);

// Non-blocking check for keyboard input events.
// This is used to drive the low-level console, and is not suitable for MECS IPC.
bool EventKeyboardPoll(char *c, bool *down, bool *printable, int* code, bool* shift, bool* ctrl, bool* alt, bool* gui);

#endif
