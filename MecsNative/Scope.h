#pragma once

#ifndef scope_h
#define scope_h

#include <stdint.h>

#include "HashMap.h"
#include "Vector.h"
#include "TagData.h"

/*

Contains data to store and query both compile-time and runtime scopes
This is basically a list (entry per scope, in order) of hashmaps (crushed name -> DataTag)

In the runtime engine, this will store the actual data (or pointers) being used by the program.
In the compiler, this is used to keep track of name references

*/

typedef struct ScopeReference {
    uint32_t crushedName;
    DataTag value;
} ScopeReferece;

typedef struct Scope Scope;

// Create a new empty scope
Scope* ScopeAllocate();
// Create a new scope, copying data from an existing one.
// All visible values from the source become globals in the result. Shadowed values are not copied
Scope* ScopeClone(Scope* source);
// Delete a scope
void ScopeDeallocate(Scope* s);

// List all reference that have gone out of scope. Hashmap of (crushname -> DataTag)
HashMap* ScopePotentialGarbage(Scope *s);
// List all *visible* references (vector of ScopeReference) any shadowed references are not supplied.
Vector* ScopeAllVisible(Scope* s);

// Start a new inner-most scope. Parameters are specially named by index (like "__p0", "__p1"). The compiler must match this.
// `Parameters` is vector of DataTag
void ScopePush(Scope* s, Vector* parameters);
// Start a new inner-most scope, copying parameters from a static array
void ScopePush(Scope* s, DataTag* parameters, uint32_t paramCount);

// Remove innermost scope, and drop back to the previous one
void ScopeDrop(Scope* s);
// Read a value by name. Returns 'invalid' if not found (remember to check!)
DataTag ScopeResolve(Scope* s, uint32_t crushedName);
// Set a value by name. If no scope has it, then it will be defined in the innermost scope
void ScopeSetValue(Scope* s, uint32_t crushedName, DataTag newValue);
// Does this name exist in any scopes?
bool ScopeCanResolve(Scope* s, uint32_t crushedName);
// Remove this variable. NOTE: this will only work in the local or global scopes, but not any intermediaries.
// It's expected that this will be used mostly on globals. They will be removed in preference to locals.
void ScopeRemove(Scope* s, uint32_t crushedName);

// Get the crushed name for a positional argument
uint32_t ScopeNameForPosition(int i);
// Does this name exist in the inner-most scope?  Will ignore other scopes, including global.
bool InScope(Scope* s, uint32_t crushedName);
// Add an increment to a stored number
void ScopeMutateNumber(Scope* s, uint32_t crushedName, int8_t increment);

// Purge hashmaps of all scopes.
// TODO: This *REALLY* needs to be improved!
void ScopePurge(Scope* s);

#endif
