#pragma once

#ifndef console_h
#define console_h

#include "String.h"
#include "DisplaySys.h"

typedef struct Console Console;
typedef Console* ConsolePtr;

// Create a new console. `screen` is required, but `scanBuffer` is optional.
ConsolePtr AttachConsole(ScreenPtr screen, ScanBufferPtr scanBuffer);

// Close down a console
void DeallocateConsole(ConsolePtr console);

// TODO: shorten the naming to "Log" instead of "Console_Write"?

// Write a string to the console
void Log(ConsolePtr cons, StringPtr msg);
// Write a string to the console
void Log(ConsolePtr cons, const char* msg);

// Start a new console line
void LogLine(ConsolePtr cons);

// Write string, and start a new line
void LogLine(ConsolePtr cons, StringPtr msg);
// Write string, and start a new line
void LogLine(ConsolePtr cons, const char* msg);			

// Write to console, and dispose of source string
void LogDealloc(ConsolePtr cons, StringPtr msg);

// Write a formatted string to the console
//'\x01'=(String*); '\x02'=int as dec; '\x03'=int as hex; '\x04'=char; '\x05'=C string (const char*); '\x06'=bool
void LogFmt(ConsolePtr cons, const char* fmt, ...);

// Write a single character to the console
void Log(ConsolePtr cons, char c);


#endif console_h
