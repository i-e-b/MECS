#include "MemoryManager.h"
#include "Vector.h"

#include <stdlib.h>

static volatile Vector* MEMORY_STACK = NULL;
static volatile int LOCK = 0;

typedef Arena* ArenaPtr;

RegisterVectorStatics(Vec);
RegisterVectorFor(ArenaPtr, Vec);

// Ensure the memory manager is ready. It starts with an empty stack
void StartManagedMemory() {
    if (LOCK != 0) return; // weak protection, yes.
    if (MEMORY_STACK != NULL) return;
    LOCK = 1;

    MEMORY_STACK = VecAllocate_ArenaPtr();

    LOCK = 0;
}
// Close all arenas and return to stdlib memory
void ShutdownManagedMemory() {
    if (LOCK != 0) return;
    if (MEMORY_STACK != NULL) return;
    LOCK = 1;

    Vector* vec = (Vector*)MEMORY_STACK;
    ArenaPtr a = NULL;
    while (VecPop_ArenaPtr(vec, &a)) {
        DropArena(&a);
    }

    LOCK = 0;
}

// Start a new arena, keeping memory and state of any existing ones
bool MMPush(size_t arenaMemory) {
    while (LOCK != 0) {}
    LOCK = 1;

    Vector* vec = (Vector*)MEMORY_STACK;
    auto a = NewArena(arenaMemory);
    bool result = false;
    if (a != NULL) {
        result = VecPush_ArenaPtr(vec, a);
        if (!result) DropArena(&a);
    }

    LOCK = 0;
    return result;
}

// Deallocate the most recent arena, restoring the previous
void MMPop() {
    while (LOCK != 0) {}
    LOCK = 1;

    Vector* vec = (Vector*)MEMORY_STACK;
    ArenaPtr a = NULL;
    if (VecPop_ArenaPtr(vec, &a)) {
        DropArena(&a);
    }

    LOCK = 0;
}

// Deallocate the most recent arena, copying a data item to the next one down (or permanent memory if at the bottom of the stack)
void* MMPopReturn(void* ptr, size_t size) {
    while (LOCK != 0) {}
    LOCK = 1;

    void* result = NULL;
    Vector* vec = (Vector*)MEMORY_STACK;
    ArenaPtr a = NULL;
    if (VecPop_ArenaPtr(vec, &a)) {
        if (VecPeek_ArenaPtr(vec, &a)) { // there is another arena. Copy there
            result = CopyToArena(ptr, size, a);
        } else { // no more arenas. Dump in regular memory
            result = MakePermanent(ptr, size);
        }
        DropArena(&a);
    } else { // nothing to pop. Hopefully this is real memory?
        result = ptr;
    }

    LOCK = 0;
    return result;
}

// Return the current arena, or NULL if none pushed
Arena* MMCurrent() {
    while (LOCK != 0) {}
    LOCK = 1;

    Vector* vec = (Vector*)MEMORY_STACK;
    ArenaPtr result = NULL;
    VecPeek_ArenaPtr(vec, &result);

    LOCK = 0;
    return result;
}

// Allocate memory
void* mmalloc(size_t size) {
    auto a = MMCurrent();
    if (a != NULL) return ArenaAllocate(a, size);
    else return malloc(size);
}

// Allocate memory array, cleared to zeros
void* mcalloc(int count, size_t size) {
    auto a = MMCurrent();
    if (a != NULL) return ArenaAllocate(a, count*size);
    else return calloc(count, size);
}

// Free memory
void mfree(void* ptr) {
    auto a = MMCurrent();
    if (a != NULL) ArenaDereference(a, ptr);
    else free(ptr);
}