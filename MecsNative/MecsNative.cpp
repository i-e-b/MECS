#include <iostream>

#include "Vector.h"

// Testing my #define understanding

#define exp2str(expression)  #expression
#define xstr(s) exp2str(s)
#define splatName(name) _this_##name##_is_special

// So, the idea for general containers, have some basic `void*` and size functionality,
// and some macros to init an inline casting function for each type.
// Like this:

typedef struct exampleElement {
    int a; int b;
} exampleElement;
exampleElement fakeData = exampleElement{ 5,15 };

typedef struct general {
    unsigned char *data;
    int elementLength;
    int elementCount;
    int capacity;
} general;

general InitGeneral(int elementByteSize) {
    return general{ NULL, elementByteSize, 0, 0 };
}

void *GeneralLoadElement(general *container, int index) {
    return &fakeData; // would do actual lookup logic to get pointer to bytes.
}

// each general function has a macro to make the specific ones:
#define regLoadElem(typeName) inline typeName *LoadElement_##typeName(general *container, int index){return (typeName *)GeneralLoadElement(container, index);} 
#define regCreate(typeName) inline general Init_##typeName(){return InitGeneral(sizeof(typeName));} 
// and there is a single macro to register all those specialised funcs:
#define RegisterContainerFor(typeName) regCreate(typeName) regLoadElem(typeName)


// Anytime you want to use the specialised container, call the main register macro:
RegisterContainerFor(exampleElement)
// now, you can call `Init_exampleElement()` or `LoadElement_exampleElement()`

int main()
{
    int x = 5, y = 4;
    char _this_hello_is_special[] = "Special value";

    auto container = Init_exampleElement();
    std::cout << "Element size is " << container.elementLength << "\n";
    auto result = LoadElement_exampleElement(&container, 0);
    std::cout << "Element data = " << result->a << ", " << result->b << "\n";

    std::cout << "Hello World!\n"; 
    std::cout << exp2str(x > y) << "\n";          // "x > y"
    std::cout << splatName(hello) << "\n";        // "Special value"
    std::cout << xstr(splatName(hello)) << "\n";  // "_this_hello_is_special" (need to do a double expansion through `xstr` to get string of token)


    // See if the container stuff works...
    std::cout << "Allocating\n";
    auto gvec = VectorAllocate(sizeof(exampleElement));
    std::cout << "Vector OK? " << gvec.IsValid << "; base addr = " << gvec._baseChunkTable << "\n";

    // add some entries
    std::cout << "Writing entries with 'push'\n";
    for (int i = 0; i < 10; i++) {
        if (!VectorPush(&gvec, result)) {
            std::cout << "Push failed!\n";
            return 255;
        }
    }
    std::cout << "Vector OK? " << gvec.IsValid << "; Elements stored = " << VectorLength(&gvec) << "\n";

    // read a random-access element
    auto r = (exampleElement*)VectorGet(&gvec, 5);
    std::cout << "Element 5 data = " << r->a << ", " << r->b << "\n";


    std::cout << "Deallocating\n";
    VectorDeallocate(&gvec);
    std::cout << "Vector OK? " << gvec.IsValid << "; base addr = " << gvec._baseChunkTable << "\n";
}
