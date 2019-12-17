#pragma once

#ifndef serialisation_h
#define serialisation_h

#include "TagData.h"
#include "Vector.h"
#include "HashMap.h"
#include "ArenaAllocator.h"
#include "TagCodeInterpreter.h"

// Write a data tag `source` in serialised form to the given BYTE vector `target`
// The given interpreter state will be used to resolve contents of containers.
// Any existing data in the target will be deleted
bool FreezeToVector(DataTag source, InterpreterState* state, Vector* target);

// Write the contents of a hash map to the given BYTE vector `target`
// This requires no interpreter state, but imposes restrictions on the data.
// The hashmap *MUST* be of type `StringPtr=>DataTag`. The child types must be static (no child hashmaps or vectors, only short strings)
// Any existing data in the target vector will be deleted
bool FreezeToVector(HashMapPtr source, VectorPtr target); // Future idea: use vargs to build vector for events?


// Expand a byte vector that has been filled by `FreezeToVector` into an arena
// The data tag that is the root of the resulting structure is passed through `dest`
// Data in `source` will be consumed during deserialisation.
bool DefrostFromVector(DataTag* dest, Arena* memory, Vector* source);

#endif
