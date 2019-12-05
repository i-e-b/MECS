
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

#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>

typedef struct Screen {
    //The window we'll be rendering to
    SDL_Window* window;

    //The surface contained by the window
    SDL_Surface* screenSurface;

	// Allocation zone
	ArenaPtr arena;
} Screen;

// Do anything needed to attach to a physical display device
ScreenPtr DisplaySystem_Start(ArenaPtr arena, int width, int height) {
	if (arena == NULL) return NULL;
	if (width < 100 || height < 100) return NULL;

	auto result = (ScreenPtr)ArenaAllocateAndClear(arena, sizeof(Screen));
	if (result == NULL) return NULL;

	result->arena = arena;

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		ArenaDereference(arena, result);
        return NULL;
	}

    result->window = SDL_CreateWindow("MECS for Windows (SDL)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
	
    if (result->window == NULL) {
		ArenaDereference(arena, result);
        return NULL;
	}

	result->screenSurface = SDL_GetWindowSurface(result->window); // Get window surface
    SDL_FillRect(result->screenSurface, NULL, SDL_MapRGB(result->screenSurface->format, 0x70, 0x70, 0x80)); // Fill the surface blue-grey
    SDL_UpdateWindowSurface(result->window); // Update the surface
}

// Release a physical display device
void DisplaySystem_Shutdown(ScreenPtr screen) {
	if (screen != NULL) {
		SDL_DestroyWindow(screen->window);
		ArenaDereference(screen->arena, screen);
	}
    SDL_Quit();
}


void DisplaySystem_PumpIdle(ScreenPtr screen) {
	SDL_PumpEvents(); // Keep Win32 happy
}

#endif

#ifdef RASPI

#endif
