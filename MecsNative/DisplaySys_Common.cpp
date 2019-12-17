
#include "DisplaySys.h"
#include "DisplaySys_Font.h"

#include "Heap.h"
#include "Vector.h"


// entry for each 'pixel' in a scan buffer
// 'drawing' involves writing a list of these, sorting by x-position, then filling the scanline
// Notes: 1080p resolution is 1920x1080 = 2'073'600 pixels. 2^22 is 4'194'304; 2^21 -1 = 2'097'151
// Using per-row buffers, we only need 2048, or about 11 bits
typedef struct SwitchPoint {
    uint32_t xpos:11;       // position of switch-point, as (y*width)+x; Limits us to 2048 width. (21 bits left)
    uint32_t id:16;         // the object ID (used for material lookup, 65k limit) (5 bits left)
    uint32_t state:1;       // 1 = 'on' point, 0 = 'off' point.
    uint32_t reserved:4;
} SwitchPoint;

typedef struct ScanLine {
	bool dirty;				// set to `true` when the scanline is updated
    int32_t count;          // number of items in the array
    int32_t length;         // memory length of the array

    SwitchPoint* points;    // When drawing to the buffer, we can just append. Before rendering, this must be sorted by xpos
} ScanLine;

typedef struct Material {
    uint32_t color;         // RGB (24 bit of color).
    int16_t  depth;         // z-position in final image
} Material;

RegisterVectorFor(Material, Vector)

// buffer of switch points.
typedef struct ScanBuffer {
	uint16_t itemCount;      // used to give each switch-point a unique ID. This is critical for the depth-sorting process
	int height;
	int width;
	int32_t expectedScanBufferSize;

	ScanLine* scanLines;     // matrix of switch points.
	VectorPtr materials;     // Vector<Material> : draw properties for each object
	HeapPtr p_heap, r_heap;  // internal heaps for depth sorting
	ArenaPtr screenArena;    // Memory arena we are working in (owned by the display system)
} ScanBuffer;


// This type is used for the ScanBuffer drawing SwitchPoint list
typedef struct SP_Element {
    int depth;      // the 'priority' of our element. Should be greater than zero
    int identifier; // a unique identifier for the element (Note: internally, this is used as a 2ndary priority to add sort stability)
    int lookup;     // any extra information needed. Does not need to be unique.
} SP_Element;


RegisterHeapFor(SP_Element, Heap)

#define ON 0x01
#define OFF 0x00

// if we use a single arena array for the materials, we are limited to:
constexpr int OBJECT_MAX = ARENA_ZONE_SIZE / sizeof(Material); // 8191 with 32-bit color; 10922 with 24 bit color

// NOTES:
// Backgrounds: To set a general background color, the first position (possibly at pos= -1) should be an 'ON' at the furthest depth per scanline.
//              There should be no matching 'OFF'.
//              In areas where there is no fill present, no change to the existing image is made.
// Holes: A CCW winding polygon will have 'OFF's before 'ON's, being inside-out. If a single 'ON' is set before this shape
//        (Same as a background) then we will fill only where the polygon is *not* present -- this makes vignette effects simple

ScanBuffer * DS_InitScanBuffer(ScreenPtr screen, int width, int height)
{
	auto arena = DisplaySystem_GetArena(screen);
	if (arena == NULL) return NULL;

	auto buf = (ScanBuffer*)ArenaAllocateAndClear(arena, sizeof(ScanBuffer));

    if (buf == NULL) return NULL;

    auto sizeEstimate = width * 2;
    buf->expectedScanBufferSize = sizeEstimate;

    // Idea: Have a single list and sort by overall position rather than x (would need a background reset at each scan start?)
    //       Could also do a 'region' like difference-from-last-scanline?

    buf->materials = VectorAllocateArena_Material(arena); //(Material*)ArenaAllocateAndClear(arena, ARENA_ZONE_SIZE/*(OBJECT_MAX + 1) * sizeof(Material)*/); // TODO: make vector?

    if (buf->materials == NULL) { DS_FreeScanBuffer(buf); return NULL; }

	buf->scanLines = (ScanLine*)ArenaAllocateAndClear(arena, (height + 1) * sizeof(ScanLine)); // we use a spare line as sorting temp memory
    if (buf->scanLines == NULL) { DS_FreeScanBuffer(buf); return NULL; }

    for (int i = 0; i < height + 1; i++) {
		auto scanBuf = (SwitchPoint*)ArenaAllocateAndClear(arena, (sizeEstimate + 1) * sizeof(SwitchPoint));
        if (scanBuf == NULL) { DS_FreeScanBuffer(buf); return NULL; }
        buf->scanLines[i].points = scanBuf;
        buf->scanLines[i].count = 0;
        buf->scanLines[i].length = sizeEstimate;
    }

    buf->p_heap = HeapAllocate_SP_Element(arena);
    if (buf->p_heap == NULL) {
        DS_FreeScanBuffer(buf);
        return NULL;
    }

    buf->r_heap = HeapAllocate_SP_Element(arena);
    if (buf->r_heap == NULL) {
        DS_FreeScanBuffer(buf);
        return NULL;
    }

    buf->itemCount = 0;
    buf->height = height;
    buf->width = width;

    return buf;
}

void DS_FreeScanBuffer(ScanBuffer * buf)
{
    if (buf == NULL) return;
	if (buf->screenArena == NULL) return;

	auto arena = buf->screenArena;
    if (buf->scanLines != NULL) {
        for (int i = 0; i < buf->height; i++) {
			if (buf->scanLines[i].points != NULL) ArenaDereference(arena, buf->scanLines[i].points);
        }
        ArenaDereference(arena, buf->scanLines);
    }
    if (buf->materials != NULL) VectorDeallocate(buf->materials);
    if (buf->p_heap != NULL) HeapDeallocate(buf->p_heap);
    if (buf->r_heap != NULL) HeapDeallocate(buf->r_heap);
    ArenaDereference(arena, buf);
}

// Set a point with an exact position, clipped to bounds
// gradient is 0..15; 15 = vertical; 0 = near horizontal.
inline void SetSP(ScanBuffer * buf, int x, int y, uint16_t objectId, uint8_t isOn) {
    if (y < 0 || y > buf->height) return;
    
   // SwitchPoint sp;
    ScanLine* line = &(buf->scanLines[y]);

	if (line->count >= line->length) return; // buffer full. TODO: grow?

    auto idx = line->count;
    auto points = line->points;

    points[idx].xpos = (x < 0) ? 0 : x;
    points[idx].id = objectId;
    points[idx].state = isOn;

	line->dirty = true; // ensure it's marked dirty
	line->count++; // increment pointer
}

inline void SetMaterial(ScanBuffer* buf, uint16_t* objectId, int depth, uint32_t color) {
    //if (objectId > OBJECT_MAX) return;
	Material mat;
	mat.color = color;
	mat.depth = depth;
	VectorPush_Material(buf->materials, mat);
	*objectId = VectorLength(buf->materials) - 1;
}

// INTERNAL: Write scan switch points into buffer for a single line.
//           Used to draw any other polygons
void SetLine(
    ScanBuffer *buf,
    int x0, int y0,
    int x1, int y1,
    int z,
    int r, int g, int b)
{
    if (y0 == y1) {
        return; // ignore: no scanlines would be affected
    }

    uint32_t color = ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
    int h = buf->height;
    int w = buf->width;
    uint8_t isOn;
    
    if (y0 < y1) { // going down
        isOn = OFF;
    } else { // going up
        isOn = ON;
        // swap coords so we can always calculate down (always 1 entry per y coord)
        int tmp;
        tmp = x0; x0 = x1; x1 = tmp;
        tmp = y0; y0 = y1; y1 = tmp;
    }

    int yoff = (y0 < 0) ? -y0 : 0;
    int top = (y0 < 0) ? 0 : y0;
    int bottom = (y1 > h) ? h : y1;
    float grad = (float)(x0 - x1) / (float)(y0 - y1);

    auto objectId = buf->itemCount;
    SetMaterial(buf, &objectId, z, color);

    for (int y = top; y < bottom; y++) // skip the last pixel to stop double-counting
    {
        // add a point.
        int x = (int)(grad * (y-y0) + x0);
        SetSP(buf, x, y, objectId, isOn);
    }

}

// Internal: Fill an axis aligned rectangle
void GeneralRect(ScanBuffer *buf,
    int left, int top, int right, int bottom,
    int z,
    int r, int g, int b)
{
    if (left >= right || top >= bottom) return; //empty
    SetLine(buf,
        left, bottom,
        left, top,
        z, r, g, b);
    SetLine(buf,
        right, top,
        right, bottom,
        z, r, g, b);
}

// Fill an axis aligned rectangle
void DS_FillRect(ScanBuffer *buf,
    int left, int top, int right, int bottom,
    int z,
    int r, int g, int b)
{
    if (z < 0) return; // behind camera
    GeneralRect(buf, left, top, right, bottom, z, r, g, b);

    buf->itemCount++;
}

void DS_FillCircle(ScanBuffer *buf,
    int x, int y, int radius,
    int z,
    int r, int g, int b) {
    DS_FillEllipse(buf,
        x, y, radius * 2, radius * 2,
        z,
        r, g, b);
}


void GeneralEllipse(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z, bool positive,
    int r, int g, int b)
{
    uint32_t color = ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);

    uint8_t left = (positive) ? (ON) : (OFF);
    uint8_t right = (positive) ? (OFF) : (ON);

    int a2 = width * width;
    int b2 = height * height;
    int fa2 = 4 * a2, fb2 = 4 * b2;
    int x, y, ty, sigma;
    
    auto objectId = buf->itemCount;
    SetMaterial(buf, &objectId, z, color);
    int grad = 15; // TODO: calculate (could be based on distance from centre line)

    // Top and bottom (need to ensure we don't double the scanlines)
    for (x = 0, y = height, sigma = 2 * b2 + a2 * (1 - 2 * height); b2*x <= a2 * y; x++) {
        if (sigma >= 0) {
            sigma += fa2 * (1 - y);
            // only draw scan points when we change y
            SetSP(buf, xc - x, yc + y, objectId, left);
            SetSP(buf, xc + x, yc + y, objectId, right);

            SetSP(buf, xc - x, yc - y, objectId, left);
            SetSP(buf, xc + x, yc - y, objectId, right);
            y--;
        }
        sigma += b2 * ((4 * x) + 6);
    }
    ty = y; // prevent overwrite

    // Left and right
    SetSP(buf, xc - width, yc, objectId, left);
    SetSP(buf, xc + width, yc, objectId, right);
    for (x = width, y = 1, sigma = 2 * a2 + b2 * (1 - 2 * width); a2*y < b2 * x; y++) {
        if (y > ty) break; // started to overlap 'top-and-bottom'

        SetSP(buf, xc - x, yc + y, objectId, left);
        SetSP(buf, xc + x, yc + y, objectId, right);

        SetSP(buf, xc - x, yc - y, objectId, left);
        SetSP(buf, xc + x, yc - y, objectId, right);

        if (sigma >= 0) {
            sigma += fb2 * (1 - x);
            x--;
        }
        sigma += a2 * ((4 * y) + 6);
    }
}


void DS_FillEllipse(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z,
    int r, int g, int b)
{
    if (z < 0) return; // behind camera

    GeneralEllipse(buf,
        xc, yc, width, height,
        z, true,
        r, g, b);

    buf->itemCount++;
}

void DS_EllipseHole(ScanBuffer *buf,
    int xc, int yc, int width, int height,
    int z,
    int r, int g, int b) {

    if (z < 0) return; // behind camera

    // set background
    GeneralRect(buf, 0, 0, buf->width, buf->height, z, r, g, b);

    // Same as ellipse, but with on and off flipped to make hole
    GeneralEllipse(buf,
        xc, yc, width, height,
        z, false,
        r, g, b);

    buf->itemCount++;
}

// Fill a quad given 3 points
void DS_FillTriQuad(ScanBuffer *buf,
    int x0, int y0,
    int x1, int y1,
    int x2, int y2,
    int z,
    int r, int g, int b) {
    // Basically the same as triangle, but we also draw a mirror image across the xy1/xy2 plane
    if (buf == NULL) return;
    if (z < 0) return; // behind camera

    if (x2 == x1 && x0 == x1 && y0 == y1 && y1 == y2) return; // empty

    // Cross product (finding only z)
    // this tells us if we are clockwise or ccw.
    int dx1 = x1 - x0; int dx2 = x2 - x0;
    int dy1 = y1 - y0; int dy2 = y2 - y0;
    int dz = dx1 * dy2 - dy1 * dx2;

    if (dz <= 0) { // ccw
        auto tmp = x1; x1 = x2; x2 = tmp;
        tmp = y1; y1 = y2; y2 = tmp;
        dx1 = dx2; dy1 = dy2;
    }
    SetLine(buf, x0, y0, x1, y1, z, r, g, b);
    SetLine(buf, x1, y1, x2 + dx1, y2 + dy1, z, r, g, b);
    SetLine(buf, x2 + dx1, y2 + dy1, x2, y2, z, r, g, b);
    SetLine(buf, x2, y2, x0, y0, z, r, g, b);

    buf->itemCount++;
}

float isqrt(float number) {
	long i;
	float x2, y;
	int j;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y = number;
	i = *(long*)&y;
	i = 0x5f3759df - (i >> 1);
	y = *(float*)&i;
	j = 3;
	while (j--) {	y = y * (threehalfs - (x2 * y * y)); }

	return y;
}

void DS_DrawLine(ScanBuffer * buf, int x0, int y0, int x1, int y1, int z, int w, int r, int g, int b)
{
    if (w < 1) return; // empty

    // TODO: special case for w < 2

    // Use triquad and the gradient's normal to draw
    float ndy = (float)(   x1 - x0  );
    float ndx = (float)( -(y1 - y0) );

    // normalise
    float mag = isqrt((ndy*ndy) + (ndx*ndx));
    ndx *= w * mag;
    ndy *= w * mag;

    int hdx = (int)(ndx / 2);
    int hdy = (int)(ndy / 2);

    // Centre points on line width 
    x0 -= hdx;
    y0 -= hdy;
    x1 -= (int)(ndx - hdx);
    y1 -= (int)(ndy - hdy);

    DS_FillTriQuad(buf, x0, y0, x1, y1,
        x0 + (int)(ndx), y0 + (int)(ndy),
        z, r, g, b);
}

void DS_OutlineEllipse(ScanBuffer * buf, int xc, int yc, int width, int height, int z, int w, int r, int g, int b)
{
    if (z < 0) return; // behind camera

    int w1 = w / 2;
    int w2 = w - w1;

    GeneralEllipse(buf,
        xc, yc, width + w2, height + w2,
        z, true, r, g, b);

    GeneralEllipse(buf,
        xc, yc, width - w1, height - w1,
        z, false, r, g, b);

    buf->itemCount++;
}

// Fill a triagle with a solid colour
// Triangle must be clockwise winding (if dy is -ve, line is 'on', otherwise line is 'off')
// counter-clockwise contours are detected and rearraged
void DS_FillTrangle(
    ScanBuffer *buf, 
    int x0, int y0,
    int x1, int y1,
    int x2, int y2,
    int z,
    int r, int g, int b)
{
    if (buf == NULL) return;
    if (z < 0) return; // behind camera

    if (x0 == x1 && x1 == x2) return; // empty
    if (y0 == y1 && y1 == y2) return; // empty

    // Cross product (finding only z)
    // this tells us if we are clockwise or ccw.
    int dx1 = x1 - x0; int dx2 = x2 - x0;
    int dy1 = y1 - y0; int dy2 = y2 - y0;
    int dz = dx1 * dy2 - dy1 * dx2;

    if (dz > 0) { // cw
        SetLine(buf, x0, y0, x1, y1, z, r, g, b);
        SetLine(buf, x1, y1, x2, y2, z, r, g, b);
        SetLine(buf, x2, y2, x0, y0, z, r, g, b);
    } else { // ccw - switch vertex 1 and 2 to make it clockwise.
        SetLine(buf, x0, y0, x2, y2, z, r, g, b);
        SetLine(buf, x2, y2, x1, y1, z, r, g, b);
        SetLine(buf, x1, y1, x0, y0, z, r, g, b);
    }

    buf->itemCount++;
}

// Set a single 'on' point at the given level on each scan line
void DS_SetBackground(
    ScanBuffer *buf,
    int z, // depth of the background. Anything behind this will be invisible
    int r, int g, int b) {
    if (z < 0) return; // behind camera

    SetLine(buf,
        0, buf->height,
        0, 0,
        z, r, g, b);

    buf->itemCount++;
}


// Clear the rows between top and bottom (inclusive)
void DS_ClearRows(ScanBuffer *buf, int top, int bottom) {

    if (buf == NULL) return;
	if (top < 0) top = 0;
	if (bottom > buf->height) bottom = buf->height;

    HeapClear(buf->p_heap);
    HeapClear(buf->r_heap);

    buf->itemCount = 0; // reset object ids

    // invalidate the marked line. We assume all others are unchanged.
    for (int i = top; i < bottom; i++)
    {
        buf->scanLines[i].count = 0;
        buf->scanLines[i].dirty = true;
    }
}

// Reset all drawing operations in the buffer, ready for next frame
// Do this *after* rendering to pixel buffer
void DS_ClearScanBuffer(ScanBuffer * buf) {
	DS_ClearRows(buf, 0, 10000);
}

// blend two colors, by a proportion [0..255]
// 255 is 100% color1; 0 is 100% color2.
inline uint32_t Blend(int prop1, uint32_t color1, uint32_t color2) {
    if (prop1 >= 255) return color1;
    if (prop1 <= 0) return color2;

    int prop2 = 255 - prop1;
    int r = prop1 * ((color1 & 0x00FF0000) >> 16);
    int g = prop1 * ((color1 & 0x0000FF00) >> 8);
    int b = prop1 * (color1 & 0x000000FF);

    r += prop2 * ((color2 & 0x00FF0000) >> 16);
    g += prop2 * ((color2 & 0x0000FF00) >> 8);
    b += prop2 * (color2 & 0x000000FF);

    // everything needs shifting 8 bits, we've integrated it into the color merge
    return ((r & 0xff00) << 8) + ((g & 0xff00)) + ((b >> 8) & 0xff);
}

// reduce display heap to the minimum by merging with remove heap
inline void CleanUpHeaps(HeapPtr p_heap, HeapPtr r_heap) {
    // clear first rank (ended objects that are on top)
    // while top of p_heap and r_heap match, remove both.
    auto nextRemove = SP_Element{ 0,-1,0 };
    auto top = SP_Element{ 0,-1,0 };
    bool ok = true;
	while (ok &&
        HeapTryFindMin(p_heap, &top) && HeapTryFindMin(r_heap, &nextRemove)
        && top.identifier == nextRemove.identifier
        ) {
        ok &= HeapDeleteMin_SP_Element(r_heap, NULL);
        ok &= HeapDeleteMin_SP_Element(p_heap, NULL);
    }

    if (!ok) return;

    // clear up second rank (ended objects that are behind the top)
    auto nextObj = SP_Element{ 0,-1,0 };

    // clean up the heaps more
    if (HeapTryFindNext(p_heap, &nextObj)) {
		auto elem = HeapPeekMin_SP_Element(r_heap);
        if (elem != NULL && elem->identifier == nextObj.identifier) {
			SP_Element current = {};
            bool ok = HeapDeleteMin_SP_Element(p_heap, &current); // remove the current top (we'll put it back after)
            while (HeapTryFindMin(p_heap, &top) && HeapTryFindMin(r_heap, &nextRemove)
                && top.identifier == nextRemove.identifier) {
                HeapDeleteMin_SP_Element(r_heap, NULL);
                HeapDeleteMin_SP_Element(p_heap, NULL);
            }
            HeapInsert_SP_Element(p_heap, current.depth, &current);
        }
    }
}

int CompareSwitchPoint(void* A, void* B) {
	SwitchPoint* a = (SwitchPoint*)A;
	SwitchPoint* b = (SwitchPoint*)B;

	// sort by position, with `on` to the left of `off`
    auto p1 = (a->xpos << 1) + a->state;
    auto p2 = (b->xpos << 1) + b->state;
    return p1 - p2;
}

void RenderScanLine(
    ScanBuffer *buf,             // source scan buffer
    int lineIndex,               // index of the line we're drawing
    char* data                   // target frame-buffer
) {
	auto scanLine = &(buf->scanLines[lineIndex]);
	if (scanLine->dirty == false) return;
	scanLine->dirty = false;

    auto tmpLine = &(buf->scanLines[buf->height]);

    int yoff = buf->width * lineIndex;
    auto materials = buf->materials;
    auto count = scanLine->count;
    auto width = buf->width;

    // Note: sorting takes a lot of the time up. Anything we can do to improve it will help frame rates
    SwitchPoint* list = (SwitchPoint*)IterativeMergeSort(scanLine->points, tmpLine->points, count, sizeof(SwitchPoint), CompareSwitchPoint);

    
    auto p_heap = buf->p_heap;   // presentation heap
    auto r_heap = buf->r_heap;   // removal heap
    
    HeapClear(p_heap);
    HeapClear(r_heap);

    uint32_t end = buf->width; // end of data in 32bit words

    bool on = false;
    uint32_t p = 0; // current pixel
    uint32_t color = 0; // color of current object
    uint32_t color_under = 0; // antialiasing color
    SwitchPoint current = {}; // top-most object's most recent "on" switch
    for (int i = 0; i < count; i++)
    {
        SwitchPoint sw = list[i];
        if (sw.xpos > end) break; // ran off the end

		Material m = Material{ 0, 0 };
        Material* mptr = VectorGet_Material(materials, sw.id);//materials[sw.id];
		if (mptr != NULL) m = *mptr;

        if (sw.xpos > p) { // render up to this switch point
            if (on) {
                auto max = (sw.xpos > end) ? end : sw.xpos;
                for (; p < max; p++) {
                    // This AA strategy will never work. Needs re-thinking
                    /*if (current.fade < 15) current.fade++;
                    auto c = Blend(15 + (current.fade << 4), color, color_under);
                    ((uint32_t*)data)[p + yoff] = c;*/


                    ((uint32_t*)data)[p + yoff] = color;
                } // draw pixels up to the point
            } else p = sw.xpos; // skip direct to the point
        }

        auto heapElem = SP_Element{ /*depth:*/ m.depth, /*unique id:*/(int)sw.id, /*lookup index:*/ i };
        if (sw.state == ON) { // 'on' point, add to presentation heap
            HeapInsert_SP_Element(p_heap, heapElem.depth, &heapElem);
        } else { // 'off' point, add to removal heap
            HeapInsert_SP_Element(r_heap, heapElem.depth, &heapElem);
        }

        CleanUpHeaps(p_heap, r_heap);
        SP_Element top = { 0,0,0 };
        on = HeapTryFindMin_SP_Element(p_heap, &top);

        if (on) {
            // set color for next run based on top of p_heap
            //color = materials[top.identifier].color;
            current = list[top.lookup];
			
			Material m = Material{ 0, 0 };
			Material* mptr = VectorGet_Material(materials, current.id);//materials[current.id];
			if (mptr != NULL) m = *mptr;
            color = m.color;
        } else {
            color = 0;
        }


#if 0
        // DEBUG: show switch point in black
        int pixoff = ((yoff + sw.xpos - 1) * 4);
        if (pixoff > 0) { data[pixoff + 0] = data[pixoff + 1] = data[pixoff + 2] = 0; }
        // END
#endif
    } // out of switch points

    
    if (on) { // fill to end of data
        for (; p < end; p++) {
            ((uint32_t*)data)[p + yoff] = color;
        }
    }
    
}

// Render a scan buffer to a pixel framebuffer
// This can be done on a different processor core from other draw commands to spread the load
// Do not draw to a scan buffer while it is rendering (switch buffers if you need to)
void DS_RenderBuffer(
    ScanBuffer *buf, // source scan buffer
    ScreenPtr screen // target frame-buffer (must match scanbuffer dimensions)
) {
    if (buf == NULL || screen == NULL) return;

	auto data = DisplaySystem_GetFrameBuffer(screen);
	if (data == NULL) return;

    for (int i = 0; i < buf->height; i++) {
        RenderScanLine(buf, i, data);
    }
}

#define SAFETY_LIMIT 50

inline void insertGlyph(ScanBuffer *buf, int x, int y, uint16_t* points, uint16_t objectId) {
	if (points == NULL) return;
    bool on = true;
    for (int i = 0; i < SAFETY_LIMIT; i++) {
        if (points[i] == 0xFFFF) break;
        int px = x + (points[i] & 0xff);
        int py = y - (points[i] >> 8) + 1;
        SetSP(buf, px, py, objectId, on);
        on = !on;
    }
}

inline uint16_t* glyphForChar(char c) {
    if (c < 33 || c > 126) return NULL;
	return charMap[c - 33];
}

void DS_DrawGlyph(ScanBuffer *buf, char c, int x, int y, int z, uint32_t color) {
    if (buf == NULL) return;
    if (c < 33 || c > 126) return;
    if (x < -7 || x > buf->width) return;
    if (y < -1 || y > buf->height + 8) return;

    // pick a char block
	uint16_t* points = charMap[c - 33];

    // set objectId, color, and depth
    uint16_t objId = buf->itemCount;
	buf->itemCount++;
    SetMaterial(buf, &objId, z, color);

    // draw points
	insertGlyph(buf, x,y, points, objId);
}

// Draw as much of a string as possible between the `left` and `right` sides.
// `y` is the font baseline. glyphs will be rendered above and below this point.
// The string is consumed as it is rendered, so any remaining string was not drawn
// we optimise here by using a single material for the entire row.
// Returns false if drawing can't continue -- either the string is empty or there was an error
bool DS_DrawStringBounded(ScanBuffer* buf, StringPtr str, int left, int right, int* dx, int y, int z, uint32_t color) {
	if (buf == NULL || str == NULL) return false;
	if (right - left < 8) return false;

    uint16_t objId = buf->itemCount;
	buf->itemCount++;
    SetMaterial(buf, &objId, z, color);

	int x = left;
    if (dx != NULL) x += *dx;

	int end = right - 8;
	bool cont = true;
	while (x <= end) {
		char c = StringDequeue(str);
		if (c == 0) { cont = false; break; } // end of string
		if (c == '\n') break; // simple line break
		if (c == '\r') {// possibly complex linebreak
			char next = StringCharAtIndex(str, 0);
			if (next == '\n') StringDequeue(str);
			break;
		}

		// ok, now draw the actual char
		insertGlyph(buf, x, y, glyphForChar(c), objId);
		x += FONT_WIDTH;
	}
	if (dx != NULL) {
		if (x + 8 > right) *dx = left;
		*dx = x - left;
	}
	return cont;
}

