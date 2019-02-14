#pragma once

#ifndef tagcodereader_h
#define tagcodereader_h

#include "TagData.h"
#include "TagCodeTypes.h"

#include "HashMap.h"
#include "Vector.h"
#include "String.h"

#include <stdint.h>

/*
    Manages the input and byte ordering of interpreter codes.
    This is driven by the runtime interpreter
*/

// Sort the byte order from file-system (network order) to memory (machine order)
void TCR_FixByteOrder(Vector* v);

#endif