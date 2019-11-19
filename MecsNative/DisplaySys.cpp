
#include "DisplaySys.h"

//#include <stdlib.h>


// entry for each 'pixel' in a scan buffer
// 'drawing' involves writing a list of these, sorting by x-position, then filling the scanline
typedef struct SwitchPoint {
    uint16_t id;                // itemCount when this point was drawn, the object ID (limit of 65k objects per frame)
    int16_t  depth;             // z-position in final image
    uint32_t pos;               // position of switch-point, as (y*width)+x;
    uint32_t material;          // RGB (24 bit of color). Could also be used to look up a texture or other style later.
    uint8_t  meta;              // metadata/flags. TODO: better alignment, merge into material?
                                // 0x01: set = 'on' point, unset = 'off' point
} SwitchPoint;

// buffer of switch points.
typedef struct ScanBuffer {
    uint16_t itemCount;      // used to give each switch-point a unique ID. This is critical for the depth-sorting process
    int height;
    int width;

    int count;              // number of items in the array
    int length;             // memory length of the array
    SwitchPoint *list;      // array of switch points. When drawing to the buffer, we can just append. Before rendering, this must be sorted by abs(pos)
    void *p_heap, *r_heap;  // internal heaps for depth sorting
} ScanBuffer;

// Most of this sub-system is common. Only the render targets (i.e. buffers) need to be platform specific



#ifdef WIN32

#endif

#ifdef RASPI

#endif
