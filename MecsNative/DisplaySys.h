#pragma once

#ifndef displaysys_h
#define displaysys_h

#include <stdint.h>

// This is basically a copy from the SDL experiments' `ScanBufferDraw.h`

typedef struct ScanBuffer ScanBuffer;

// Create a new scan-buffer, to accept draw commands and be rendered to a buffer
ScanBuffer *InitScanBuffer(int width, int height);
// Dispose if a scan-buffer when no longer needed
void FreeScanBuffer(ScanBuffer *buf);

// Reset all drawing operations in the buffer, ready for next frame
// Do this *after* rendering to pixel buffer
void ClearScanBuffer(ScanBuffer *buf);

// Render a scan buffer to a pixel framebuffer
// This can be done on a different processor core from other draw commands to spread the load
// Do not draw to a buffer while it is rendering (switch buffers if you need to)
void RenderBuffer( ScanBuffer *buf, // source scan buffer
    uint8_t* data, int rowBytes,    // target frame-buffer
    int bufSize                     // size of target buffer
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




#endif
