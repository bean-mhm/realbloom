#pragma once

#include <iostream>

#ifdef _WIN32
#include <Windows.h>

#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define DISABLE_NEWLINE_AUTO_RETURN  0x0008

void activateVirtualTerminal();
#endif

constexpr int CONSOLE_NC = -1;
constexpr int CONSOLE_BLACK = 0;
constexpr int CONSOLE_RED = 1;
constexpr int CONSOLE_GREEN = 2;
constexpr int CONSOLE_YELLOW = 3;
constexpr int CONSOLE_BLUE = 4;
constexpr int CONSOLE_MAGENTA = 5;
constexpr int CONSOLE_CYAN = 6;
constexpr int CONSOLE_WHITE = 7;

constexpr int COL_PRI = CONSOLE_YELLOW;
constexpr int COL_SEC = CONSOLE_CYAN;
constexpr int COL_ERR = CONSOLE_RED;

/**
* Colorize terminal colors ANSI escape sequences.
*
* @param font font color (-1 to 7), see CONSOLE enum
* @param back background color (-1 to 7), see COLORS enum
* @param style font style (1==bold, 4==underline)
**/
const char* consoleColor(int font = -1, int back = -1, int style = -1);
