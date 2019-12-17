#include "EventSys.h"
#include "HashMap.h"
#include "TagData.h"
#include "Serialisation.h"
#include "MemoryManager.h"

RegisterHashMapFor(StringPtr, DataTag, HashMapStringKeyHash, HashMapStringKeyCompare, HashMap)

#ifdef WIN32

#include <SDL.h>

// Start the event system. This should be called before the scheduler
bool EventSystem_Start() {
	// Ensure SDL is up
	uint32_t subsystem_init;
	subsystem_init = SDL_WasInit(SDL_INIT_EVERYTHING);
	if ((subsystem_init & SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
			return false;
		}
	}
	return true;
}

// Non-blocking check for events.
// Returns true if events are available.
// If `target` and `data` should be supplied by the caller, and will be populated with event details
// that can be used for the runtime scheduler.
bool EventPoll(StringPtr target, VectorPtr data) {
	if (target == NULL || data == NULL) return false;

	SDL_Event event;

	// Check for events. We only loop here if we are ignoring the event
	// Otherwise, we return a single event to the caller
	while (SDL_PollEvent(&event) != 0) {
		switch (event.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			MMPush(256 KILOBYTES);
			// Make a hashmap to hold event parameters
			auto evtData = HashMapAllocate_StringPtr_DataTag(2);
			HashMapPut_StringPtr_DataTag(evtData, StringNew("code"), EncodeInt32(event.key.keysym.scancode), true);
			HashMapPut_StringPtr_DataTag(evtData, StringNew("char"), EncodeShortStr((char)(event.key.keysym.sym)), true);
			HashMapPut_StringPtr_DataTag(evtData, StringNew("state"), EncodeBool(event.key.state), true);

			// serialise to data vector
			bool ok = FreezeToVector(evtData, data);
			MMPop();

			if (!ok) return false;

			// set the event type
			StringClear(target);
			StringAppend(target, "keyboard");
			return true;
		}
			/*
		case SDL_MOUSEMOTION:
			//event.motion.x
			StringAppend(target, "mouse event");
			return true;
			*/
		default:
			// Ignore any other event -- loop and try again
			break;
		}
	}
	return false; // no unfiltered event
}


#endif

#ifdef RASPI

#endif