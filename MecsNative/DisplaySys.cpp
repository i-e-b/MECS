
#include "DisplaySys.h"

// Most of this sub-system is common. Only the render targets (i.e. buffers) need to be platform specific

#ifdef WIN32

#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>

typedef struct Screen {
    //The window we'll be rendering to
    SDL_Window* window;

    //The surface contained by the window
    //SDL_Surface* screenSurface;

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

	auto screenSurface = SDL_GetWindowSurface(result->window); // Get window surface
    SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0x70, 0x70, 0x80)); // Fill the surface blue-grey
    SDL_UpdateWindowSurface(result->window); // Update the surface
	return result;
}

// Release a physical display device
void DisplaySystem_Shutdown(ScreenPtr screen) {
	if (screen != NULL) {
		SDL_DestroyWindow(screen->window);
		ArenaDereference(screen->arena, screen);
	}
    SDL_Quit();
}


// get a pointer to the raw frame buffer being used by the screen
char* DisplaySystem_GetFrameBuffer(ScreenPtr screen) {
	if (screen == NULL) return NULL;

	auto screenSurface = SDL_GetWindowSurface(screen->window); // Get window surface
	return (char*)screenSurface->pixels;
}


void DisplaySystem_PumpIdle(ScreenPtr screen) {
	if (screen == NULL || screen->window == NULL) return;
	SDL_UpdateWindowSurface(screen->window); // Update the surface
	SDL_PumpEvents(); // Keep Win32 happy
}

#endif

#ifdef RASPI

#endif
