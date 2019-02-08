#include "TimingSys.h"


#ifdef WIN32

#include <time.h>

uint64_t SystemTime() {
    time_t rawtime;
    time(&rawtime);

    return rawtime;
}

#endif

#ifdef RASPI

uint64_t SystemTime() {
    // TODO !
    return uint64_t();
}

#endif