#pragma once

#ifndef displaysys_h
#define displaysys_h

#include <stdint.h>
#include "ArenaAllocator.h"

// Draw commands structures
typedef struct ScanBuffer ScanBuffer;

// Structure for tracking display (window on desktop, raw buffer on embed)
typedef struct Screen Screen;
typedef Screen* ScreenPtr;

//######################### Device management #########################

// Do anything needed to attach to a physical display device
ScreenPtr DisplaySystem_Start(ArenaPtr arena, int width, int height);

// Release a physical display device
void DisplaySystem_Shutdown(ScreenPtr screen);

// Run desktop system events (ignored on embedded systems)
// This should be called frequently, particularly as the system goes idle.
void DisplaySystem_PumpIdle(ScreenPtr screen);

// get a pointer to the raw frame buffer being used by the screen
char* DisplaySystem_GetFrameBuffer(ScreenPtr screen);



// Create a new scan-buffer, to accept draw commands and be rendered to a buffer
ScanBuffer *InitScanBuffer(int width, int height);
// Dispose if a scan-buffer when no longer needed
void FreeScanBuffer(ScanBuffer *buf);

// Reset all drawing operations in the buffer, ready for next frame
// Do this *after* rendering to pixel buffer
void ClearScanBuffer(ScanBuffer *buf);

// Render a scan buffer to a pixel framebuffer
// This can be done on a different processor core from other draw commands to spread the load
// Do not draw to a scan buffer while it is rendering (switch buffers if you need to)
void RenderBuffer(
    ScanBuffer *buf, // source scan buffer
    char* data       // target frame-buffer (must match scanbuffer dimensions)
);

// Fill a triagle with a solid colour
void FillTrangle( ScanBuffer *buf,
    int x0, int y0, 
    int x1, int y1,
    int x2, int y2,
    int z,
    int r, int g, int b);

// Fill an axis aligned rectangle
void FillRect(ScanBuffer *buf,
    int left, int top, int right, int bottom,
    int z,
    int r, int g, int b);

void FillCircle(ScanBuffer *buf,
    int x, int y, int radius,
    int z,
    int r, int g, int b);

void FillEllipse(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z,
    int r, int g, int b);

// Fill a quad given 3 points
void FillTriQuad(ScanBuffer *buf,
    int x0, int y0,
    int x1, int y1,
    int x2, int y2,
    int z,
    int r, int g, int b);

// draw a line with width
void DrawLine(ScanBuffer *buf,
    int x0, int y0,
    int x1, int y1,
    int z, int w, // width
    int r, int g, int b);

// draw the border of an ellipse
void OutlineEllipse(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z, int w, // outline width
    int r, int g, int b);

// Set a background plane
void SetBackground( ScanBuffer *buf,
    int z, // depth of the background. Anything behind this will be invisible
    int r, int g, int b);

// draw everywhere except in the ellipse
void EllipseHole(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z,
    int r, int g, int b);



// Write a glyph at the given position (y is baseline)
// This is done with a very basic default font
void AddGlyph(ScanBuffer *buf, char c, int x, int y, int z, uint32_t color);


#endif
