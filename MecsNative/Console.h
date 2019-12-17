#pragma once

#ifndef console_h
#define console_h

#include "String.h"
#include "DisplaySys.h"


// Comment out to use only graphic console:
#define ECHO_TO_STDOUT 1

typedef struct Console Console;
typedef Console* ConsolePtr;

// Create a new console. `screen` is required, but `scanBuffer` is optional.
ConsolePtr AttachConsole(ScreenPtr screen, ScanBufferPtr scanBuffer);

// Close down a console
void DeallocateConsole(ConsolePtr console);

// Block execution until a printable character is typed.
// The console will NOT display a prompt, and will NOT echo the input.
char Console_ReadChar(ConsolePtr console);

// Block execution until a line of input is supplied.
// The console WILL display a prompt and WILL echo the input.
void Console_ReadLine(ConsolePtr console, StringPtr dest);

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
//'\x01'=(String*); '\x02'=int as dec; '\x03'=int as hex; '\x04'=char; '\x05'=C string (const char*); '\x06'=bool; '\x07'=byte as hex
void LogFmt(ConsolePtr cons, const char* fmt, ...);

// Write a single character to the console
void Log(ConsolePtr cons, char c);


#endif console_h
