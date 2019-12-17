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
// that can be used for the runtime scheduler.
bool EventPoll(StringPtr target, VectorPtr data);

#endif
