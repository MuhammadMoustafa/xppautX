/* Windows replacements for the few POSIX calls the core and the protocol
   front end use. Kept in one file so <windows.h> (whose macros clash with
   core names such as max, MessageBox and VARTYPE) is included nowhere else.
   Empty on other systems. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xpp_win32.h"
#include <array>
#include <string>

namespace xpp::win32 {

/* blocks until stdin has data: xpp_inbox.cpp calls it on its reader thread */
int read_stdin(std::span<char> buf)
{
    DWORD got = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf.data(), static_cast<DWORD>(buf.size()), &got, NULL) || got == 0)
        return -1;
    return static_cast<int>(got);
}

std::optional<std::string> read_stdin_line(int seconds, size_t cap)
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
                const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
                line.resize(static_cast<size_t>(bytes));
                if (bytes) WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), line.data(), bytes, nullptr, nullptr);
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
            if (read_stdin({&c, 1}) != 1) return std::nullopt;
            if (c == '\n') return line;
            if (c != '\r') line += c;
        }
    }
    return std::nullopt;
}

void binary_mode(int fd) { _setmode(fd, _O_BINARY); }

/* xpp_files.cpp: a link is never read or written through */
bool path_is_link(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

/* Another process may hold the target open for a moment, an antivirus or
   the search indexer reading a file just written: the move then fails with
   a sharing, lock or access error, gone a few milliseconds later (W159's
   push: test_lunch's set files, 2 runs in 3 here). Those errors are tried
   again, MOVE_RETRIES times MOVE_RETRY_MS apart (half a second in all,
   longer than such a scan holds a small file); any other fails at once. */
constexpr int MOVE_RETRIES = 50;
constexpr DWORD MOVE_RETRY_MS = 10;

bool move_over(const char *from, const char *to)
{
    for (int attempt = 0;; attempt++) {
        if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        const DWORD e = GetLastError();
        if (attempt == MOVE_RETRIES
            || (e != ERROR_SHARING_VIOLATION && e != ERROR_LOCK_VIOLATION && e != ERROR_ACCESS_DENIED))
            return false;
        Sleep(MOVE_RETRY_MS);
    }
}

std::string temp_folder()
{
    std::array<char, MAX_PATH> base; /* GetTempPathA writes it */
    DWORD n = GetTempPathA(static_cast<DWORD>(base.size()), base.data());
    if (n == 0 || n >= base.size()) return std::string();
    if (base[n - 1] == '\\') n--;
    return std::string(base.data(), n);
}

bool process_running(unsigned long pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
    DWORD code;
    bool running;

    if (h == NULL) return false; /* no such process */
    running = !GetExitCodeProcess(h, &code) || code == STILL_ACTIVE;
    CloseHandle(h);
    return running;
}

} // namespace xpp::win32

/* W13b: xppautX links -mwindows, so no console appears when Explorer or a
   file association starts it; a command-line mode reattaches to a real
   parent console instead (xpp_win32.h). A handle that is already a pipe,
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

void xpp::win32::attach_console()
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

#endif
