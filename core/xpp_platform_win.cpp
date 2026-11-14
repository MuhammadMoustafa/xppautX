/* xpp_platform.h for Windows: the console, pipe or file of stdin, a descriptor's line ends, and the parent's
   console (the launch stubs are xpp_platform.h's own). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include "xpp_platform.h"
#include "xpp_win.h"

namespace xpp::platform {

/* blocks until stdin has data: xpp_inbox.cpp calls it on its reader thread */
/* blocks until stdin has data: xpp_inbox.cpp calls it on its reader thread */
long read_input(char *buf, std::size_t n)
{
    DWORD got = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, static_cast<DWORD>(n), &got, NULL) || got == 0)
        return -1;
    return static_cast<long>(got);
}

std::optional<std::string> read_input_line(int seconds, std::size_t cap)
{
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
    DWORD mode;
    const bool console = GetConsoleMode(input, &mode) != 0;
    const bool pipe = GetFileType(input) == FILE_TYPE_PIPE;
    std::string line;
    std::wstring wide;
    /* Anonymous pipes have no data-ready event; observe available bytes without a blocking read. */
    constexpr DWORD PIPE_POLL_MS = 10;
    constexpr wchar_t CONSOLE_EOF = 26; /* Ctrl+Z is the console's end-of-input character. */
    while (line.size() < cap && wide.size() < cap) {
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) return std::nullopt;
        const DWORD left = static_cast<DWORD>(deadline - now);
        if (console) {
            if (WaitForSingleObject(input, left) != WAIT_OBJECT_0) return std::nullopt;
            INPUT_RECORD event;
            DWORD got;
            if (!ReadConsoleInputW(input, &event, 1, &got) || got != 1) return std::nullopt;
            if (event.EventType != KEY_EVENT || !event.Event.KeyEvent.bKeyDown) continue;
            const wchar_t c = event.Event.KeyEvent.uChar.UnicodeChar;
            if (!c) continue;
            if (c == CONSOLE_EOF) return std::nullopt;
            if (c == '\r') {
                line = win::narrow(wide);
                WriteConsoleW(GetStdHandle(STD_ERROR_HANDLE), L"\r\n", 2, &got, nullptr);
                return line;
            }
            if (c == '\b') {
                if (!wide.empty()) {
                    wide.pop_back();
                    WriteConsoleW(GetStdHandle(STD_ERROR_HANDLE), L"\b \b", 3, &got, nullptr);
                }
            } else {
                wide += c;
                WriteConsoleW(GetStdHandle(STD_ERROR_HANDLE), &c, 1, &got, nullptr);
            }
        } else {
            if (pipe) {
                DWORD available;
                if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) return std::nullopt;
                if (!available) { Sleep(left < PIPE_POLL_MS ? left : PIPE_POLL_MS); continue; }
            }
            char c;
            if (read_input(&c, 1) != 1) return std::nullopt;
            if (c == '\n') return line;
            if (c != '\r') line += c;
        }
    }
    return std::nullopt;
}

void set_binary_mode(int fd) { _setmode(fd, _O_BINARY); /* "\n" line ends, not "\r\n" */ }

/* W13b: xppautX links -mwindows, so no console appears when Explorer or a
   file association starts it; a command-line mode reattaches to a real
   parent console instead. A handle that is already a pipe,
   a file or NUL (piped --server, redirected output, `< NUL`) is a real inherited
   handle regardless of subsystem (redirected() below), and it is left
   alone. Only a missing handle or one that is already a console (rare, but
   harmless to redo) is worth an AttachConsole call. */
namespace {

/* whether a standard handle is already real input or output that the
   console must not replace (`cmds | xppautX --server` pipes stdin only):
   a pipe, a file, or a character device that is not a console. The last is
   NUL above all (W18): a test's stdin=DEVNULL, `< NUL`, a CI runner's
   stdin. GetFileType calls NUL a character device like a console; taking
   it for "no stdin" reopened stdin on the parent's console (CONIN$), and
   --server waited at that console instead of seeing the end of input.
   GetConsoleMode tells the two apart: it succeeds only on a console. */
bool redirected(DWORD which)
{
    HANDLE h = GetStdHandle(which);
    if (h == NULL || h == INVALID_HANDLE_VALUE) return false;
    DWORD mode;
    switch (GetFileType(h)) {
    case FILE_TYPE_UNKNOWN: return false;
    case FILE_TYPE_CHAR: return !GetConsoleMode(h, &mode);
    default: return true;
    }
}

} // namespace

void attach_parent_console()
{
    bool in = redirected(STD_INPUT_HANDLE), out = redirected(STD_OUTPUT_HANDLE), err = redirected(STD_ERROR_HANDLE);
    if (in && out && err) return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return; /* no console to attach to (Explorer): stay quiet */
    /* freopen can only fail here if the console itself is gone; there is no
       better fallback than leaving the stream as it was */
    auto reopen = [](const char *name, const char *mode, FILE *f) { return freopen(name, mode, f) != nullptr; };
    if (!out) reopen("CONOUT$", "w", stdout);
    if (!err) reopen("CONOUT$", "w", stderr);
    if (!in) reopen("CONIN$", "r", stdin);
}

} // namespace xpp::platform
