#pragma once

#ifndef serialisation_h
#define serialisation_h

#include "TagData.h"
#include "Vector.h"
#include "ArenaAllocator.h"
#include "TagCodeInterpreter.h"

// Write a data tag `source` in serialised form to the given BYTE vector `target`
// The given interpreter state will be used to resolve contents of containers.
// Any existing data in the target will be deleted
bool FreezeToVector(DataTag source, InterpreterState* state, Vector* target);

// Expand a byte vector that has been filled by `FreezeToVector` into an arena
// The data tag that is the root of the resulting structure is passed through `dest`
// Data in `source` will be consumed during deserialisation.
bool DefrostFromVector(DataTag* dest, Arena* memory, Vector* source);

#endif
