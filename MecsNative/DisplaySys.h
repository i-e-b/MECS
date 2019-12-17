#pragma once

#ifndef displaysys_h
#define displaysys_h

#include <stdint.h>
#include "ArenaAllocator.h"
#include "String.h"

// Draw commands structures
typedef struct ScanBuffer ScanBuffer;
typedef ScanBuffer* ScanBufferPtr;

// Structure for tracking display (window on desktop, raw buffer on embed)
typedef struct Screen Screen;
typedef Screen* ScreenPtr;

//######################### Device management #########################

// Do anything needed to attach to a physical display device
// The screen will be cleared with the given color
ScreenPtr DisplaySystem_Start(ArenaPtr arena, int width, int height, int r, int g, int b);

// Release a physical display device
void DisplaySystem_Shutdown(ScreenPtr screen);

// Run desktop system events (ignored on embedded systems)
// This should be called frequently, particularly as the system goes idle.
void DisplaySystem_PumpIdle(ScreenPtr screen);

// get a pointer to the raw frame buffer being used by the screen
char* DisplaySystem_GetFrameBuffer(ScreenPtr screen);

// Return the arena being used by the display system
ArenaPtr DisplaySystem_GetArena(ScreenPtr screen);

// move all pixels on the screen in by a vertical number of pixels.
// negative values will move the image up, positive will move it down.
// invalidated pixels will be written with the given color
void DS_VScrollScreen(ScreenPtr screen, int distance, int r, int g, int b);

int DisplaySystem_GetWidth(ScreenPtr screen);
int DisplaySystem_GetHeight(ScreenPtr screen);



//######################### Scan buffer management #########################

// Create a new scan-buffer, to accept draw commands and be rendered to a buffer
ScanBuffer *DS_InitScanBuffer(ScreenPtr screen, int width, int height);
// Dispose if a scan-buffer when no longer needed
void DS_FreeScanBuffer(ScanBuffer *buf);

// Reset all drawing operations in the buffer, ready for next frame
// Do this *after* rendering to pixel buffer
void DS_ClearScanBuffer(ScanBuffer *buf);

// Clear the rows between top and bottom (inclusive)
void DS_ClearRows(ScanBuffer *buf, int top, int bottom);

// Render a scan buffer to a pixel framebuffer
// This can be done on a different processor core from other draw commands to spread the load
// Do not draw to a scan buffer while it is rendering (switch buffers if you need to)
void DS_RenderBuffer(
    ScanBuffer *buf, // source scan buffer
    ScreenPtr screen // target frame-buffer (must match scanbuffer dimensions)
);

//######################### Scan buffer drawing #########################

// Fill a triagle with a solid colour
void DS_FillTrangle( ScanBuffer *buf,
    int x0, int y0, 
    int x1, int y1,
    int x2, int y2,
    int z,
    int r, int g, int b);

// Fill an axis aligned rectangle
void DS_FillRect(ScanBuffer *buf,
    int left, int top, int right, int bottom,
    int z,
    int r, int g, int b);

// Fill a circle given a point and radius
void DS_FillCircle(ScanBuffer *buf,
    int x, int y, int radius,
    int z,
    int r, int g, int b);

// Fill an ellipse given a centre point, height and width
void DS_FillEllipse(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z,
    int r, int g, int b);

// Fill a quad given 3 points
void DS_FillTriQuad(ScanBuffer *buf,
    int x0, int y0,
    int x1, int y1,
    int x2, int y2,
    int z,
    int r, int g, int b);

// draw a line with width
void DS_DrawLine(ScanBuffer *buf,
    int x0, int y0,
    int x1, int y1,
    int z, int w, // width
    int r, int g, int b);

// draw the border of an ellipse
void DS_OutlineEllipse(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z, int w, // outline width
    int r, int g, int b);

// Set a background plane
void DS_SetBackground( ScanBuffer *buf,
    int z, // depth of the background. Anything behind this will be invisible
    int r, int g, int b);

// draw everywhere except in the ellipse
void DS_EllipseHole(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z,
    int r, int g, int b);


// Write a single glyph at the given position (y is baseline)
// This is done with a very basic default font
// characters fit on an 8x8 grid
void DS_DrawGlyph(ScanBuffer *buf, char c, int x, int y, int z, uint32_t color);

// Draw as much of a string as possible between the `left` and `right` sides.
// `y` is the font baseline. glyphs will be rendered above and below this point.
// if `dx` is not null, it will be given the offset from `left` where drawing stopped
// The string is consumed as it is rendered, so any remaining string was not drawn
// Returns false if drawing can't continue -- either the string is empty or there was an error
bool DS_DrawStringBounded(ScanBuffer* buf, StringPtr str, int left, int right, int* dx, int y, int z, uint32_t color);

#endif
