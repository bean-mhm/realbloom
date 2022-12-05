#include "ConsoleColors.h"

#ifdef _WIN32
void activateVirtualTerminal()
{
    HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(handleOut, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    consoleMode |= DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(handleOut, consoleMode);
}
#endif

const char* consoleColor(int font, int back, int style)
{
    static char code[32];

    if (font >= 0)
        font += 30;
    else
        font = 0;
    if (back >= 0)
        back += 40;
    else
        back = 0;

    if (back > 0 && style > 0)
    {
        sprintf_s(code, "\033[%d;%d;%dm", font, back, style);
    } else if (back > 0)
    {
        sprintf_s(code, "\033[%d;%dm", font, back);
    } else
    {
        sprintf_s(code, "\033[%dm", font);
    }

    return code;
}
