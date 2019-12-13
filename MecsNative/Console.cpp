#include "Console.h"
#include <stdarg.h>

#include <iostream>

const int LINE_HEIGHT = 9;
typedef struct Console {
	ScreenPtr OutputScreen;
	ScanBufferPtr OutputGraphics;

	// arena used for string mangling
	ArenaPtr arena;
	int ConsoleX;
	int y;
	int left, right;
} Console;


ConsolePtr AttachConsole(ScreenPtr screen, ScanBufferPtr scanBuffer) {
	if (screen == NULL) return NULL;

	int w = DisplaySystem_GetWidth(screen);
	int h = DisplaySystem_GetHeight(screen);

	if (w < (3 * LINE_HEIGHT)) return NULL;
	if (h < (3 * LINE_HEIGHT)) return NULL;

	auto arena = NewArena(1 MEGABYTE);
	if (arena == NULL) return NULL;

	auto result = (ConsolePtr)ArenaAllocate(arena, sizeof(Console));
	if (result == NULL) {
		DropArena(&arena);
		return NULL;
	}
	
	result->OutputScreen = screen;
	result->arena = arena;
	result->ConsoleX = 0;
	result->left = LINE_HEIGHT; 
	result->right = w - LINE_HEIGHT;
	result->y = h - LINE_HEIGHT;

	if (scanBuffer != NULL) {
		result->OutputGraphics = scanBuffer;
	} else {
		result->OutputGraphics = DS_InitScanBuffer(screen, w, h);
		if (result->OutputGraphics == NULL) {
			DropArena(&arena);
			return NULL;
		}
	}

	return result;
}

// Close down a console
void DeallocateConsole(ConsolePtr console) {
	if (console == NULL) return;
	if (console->arena == NULL) return;
	auto tgt = console->arena;
	DropArena(&tgt);
}


// Write a formatted string to the console
//'\x01'=(String*); '\x02'=int as dec; '\x03'=int as hex; '\x04'=char; '\x05'=C string (const char*); '\x06'=bool
void Console_WriteFmt(ConsolePtr cons, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    auto str = StringEmptyInArena(cons->arena);
    vStringAppendFormat(str, fmt, args);
	Console_Write(cons, str);
	StringDeallocate(str);
    va_end(args);
}


void Console_WriteLine(ConsolePtr cons, StringPtr msg) {
	auto clone = StringClone(msg, cons->arena);
	Console_Write(cons, clone);
	Console_Newline(cons);
	StringDeallocate(clone);
}
// Write to console, and dispose of source string
void Console_WriteD(ConsolePtr cons, StringPtr msg) {
	Console_Write(cons, msg);
	StringDeallocate(msg);
}
void Console_WriteChar(ConsolePtr cons, char c) {
	auto msg = StringEmptyInArena(cons->arena);
	StringAppendChar(msg, c);
	Console_Write(cons, msg);
	StringDeallocate(msg);
}

void Console_Write(ConsolePtr cons, const char* msg) {
	auto tmp = StringNewInArena(msg, cons->arena);
	Console_Write(cons, tmp);
	StringDeallocate(tmp);
}
void Console_WriteLine(ConsolePtr cons, const char* msg) {
	Console_Write(cons, msg);
	Console_Newline(cons);
}

void Console_Newline(ConsolePtr cons) {
	if (cons == NULL) return;

	cons->ConsoleX = 0;
	DS_VScrollScreen(cons->OutputScreen, -LINE_HEIGHT, /*background color: */ 0x70, 0x70, 0x80);
	DisplaySystem_PumpIdle(cons->OutputScreen);
}


void Console_Write(ConsolePtr cons, StringPtr msg) {
	if (cons == NULL || msg == NULL) return;
	// bottom first, scrolling up
	int y = cons->y;
	int right = cons->right;

	int left = cons->left + cons->ConsoleX;
	if (left + LINE_HEIGHT >= right) left = cons->left;

	while (DS_DrawStringBounded(cons->OutputGraphics, msg, left, right, &(cons->ConsoleX), y, 1, /* Text color: */ 0xFfFfFf)){
		cons->ConsoleX = 0; // only keep the offset if we didn't wrap
		left = cons->left;
		// Draw line and scroll up
		DS_RenderBuffer(cons->OutputGraphics, cons->OutputScreen);
		DS_ClearRows(cons->OutputGraphics, y - LINE_HEIGHT, y + LINE_HEIGHT);
		DS_VScrollScreen(cons->OutputScreen, -LINE_HEIGHT, /*background color: */ 0x70, 0x70, 0x80);
	}
	
	// draw any remaining lines and flush buffer
	DS_RenderBuffer(cons->OutputGraphics, cons->OutputScreen);
	DS_ClearRows(cons->OutputGraphics, y - LINE_HEIGHT, y + LINE_HEIGHT);
	DisplaySystem_PumpIdle(cons->OutputScreen);
}