
#include "DisplaySys.h"

// Most of this sub-system is common. Only the render targets (i.e. buffers) need to be platform specific

#ifdef WIN32

#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>

typedef struct Screen {
    //The window we'll be rendering to
    SDL_Window* window;

	int height;
	int width;
	int bpp; // bits per pixel
    
	// Allocation zone
	ArenaPtr arena;
} Screen;


int DisplaySystem_GetWidth(ScreenPtr screen) {
	if (screen == NULL) return -1;
	return screen->width;
}
int DisplaySystem_GetHeight(ScreenPtr screen) {
	if (screen == NULL) return -1;
	return screen->height;
}

ArenaPtr DisplaySystem_GetArena(ScreenPtr screen) {
	if (screen == NULL) return NULL;
	return screen->arena;
}

// Do anything needed to attach to a physical display device
ScreenPtr DisplaySystem_Start(ArenaPtr arena, int width, int height, int r, int g, int b) {
	if (arena == NULL) return NULL;
	if (width < 100 || height < 100) return NULL;

	// Ensure SDL is up
	uint32_t subsystem_init;
	subsystem_init = SDL_WasInit(SDL_INIT_EVERYTHING);
	if ((subsystem_init & SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
			return NULL;
		}
	}

	auto result = (ScreenPtr)ArenaAllocateAndClear(arena, sizeof(Screen));
	if (result == NULL) return NULL;
	result->arena = arena;

    result->window = SDL_CreateWindow("MECS for Windows (SDL)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
	
    if (result->window == NULL) {
		ArenaDereference(arena, result);
        return NULL;
	}

	result->height = height;
	result->width = width;

	auto screenSurface = SDL_GetWindowSurface(result->window); // Get window surface
    SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, r, g, b)); // Fill the surface
	result->bpp = screenSurface->format->BitsPerPixel;
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


// move all pixels on the screen in by a vertical number of pixels.
// negative values will move the image up, positive will move it down.
void DS_VScrollScreen(ScreenPtr screen, int distance, int r, int g, int b) {
	if (screen == NULL || screen->window == NULL || distance == 0) return;

	auto screenSurface = SDL_GetWindowSurface(screen->window); // Get window surface
	auto buf = (char*)screenSurface->pixels;

	int dir = (distance > 0) ? -1 : 1;
	int start = (distance > 0) ? distance : 0;
	int end = (distance > 0) ? screen->height : (screen->height + distance);
	
	int upper = (start > end) ? start : end;
	int lower = (start > end) ? end : start;

	int rowbytes = screen->width * (screen->bpp >> 3);

	if (dir < 0) { auto tmp = start; start = end - 1; end = tmp; } // swap endpoints


	for (int y = start; (y >= lower) && (y < upper); y += dir) {
		int sy = y - distance;

		int src_y = sy * rowbytes;
		int dst_y = y * rowbytes;

		for (int x = 0; x < rowbytes; x++) {
			buf[dst_y+x] = buf[src_y+x];
		}
	}

	for (int y = 0; y < lower; y++)
	{
		int dst_y = y * rowbytes;
		for (int x = 0; x < rowbytes; x+=4) {
			buf[dst_y+x+0] = b;
			buf[dst_y+x+1] = g;
			buf[dst_y+x+2] = r;
			buf[dst_y+x+3] = 0; // NA
		}
	}
	for (int y = upper; y < screen->height; y++)
	{
		int dst_y = y * rowbytes;
		for (int x = 0; x < rowbytes; x+=4) {
			buf[dst_y+x+0] = b;
			buf[dst_y+x+1] = g;
			buf[dst_y+x+2] = r;
			buf[dst_y+x+3] = 0; // NA
		}
	}
}

// Erase a rectangle on the screen. This ignores the scan buffer renderer.
void DS_Erase(ScreenPtr screen, int left, int top, int right, int bottom, int r, int g, int b) {
	if (screen == NULL || screen->window == NULL) return;

	if (left < 0) left = 0;
	if (right > screen->width) right = screen->width;
	if (top < 0) top = 0;
	if (bottom > screen->height) bottom = screen->height;
	if (left >= right || top >= bottom) return;
	
	auto screenSurface = SDL_GetWindowSurface(screen->window); // Get window surface
	auto buf = (char*)screenSurface->pixels;

	int pixbytes = (screen->bpp >> 3);
	int rowbytes = screen->width * pixbytes;
	left *= pixbytes;
	right *= pixbytes;
	
	for (int y = top; y < bottom; y++)
	{
		int dst_y = y * rowbytes;
		for (int x = left; x < right; x+=4) {
			buf[dst_y+x+0] = b;
			buf[dst_y+x+1] = g;
			buf[dst_y+x+2] = r;
			buf[dst_y+x+3] = 0; // NA
		}
	}
}

#endif

#ifdef RASPI

#endif
