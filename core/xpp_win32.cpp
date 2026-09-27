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
#include "xpp_dlfcn.h"
#include "xpp_win32.h"
#include <array>
#include <string>

static const char *dl_error;

void *dlopen(const char *name, int)
{
    HMODULE h = LoadLibraryA(name);
    dl_error = h ? NULL : "LoadLibrary failed";
    return reinterpret_cast<void *>(h);
}

void *dlsym(void *handle, const char *name)
{
    FARPROC f = GetProcAddress(static_cast<HMODULE>(handle), name);
    dl_error = f ? NULL : "GetProcAddress failed";
    return reinterpret_cast<void *>(f);
}

int dlclose(void *handle) { return FreeLibrary(static_cast<HMODULE>(handle)) ? 0 : -1; }

char *dlerror(void)
{
    const char *e = dl_error;
    dl_error = NULL;
    return const_cast<char *>(e); /* POSIX's type; the text is never written */
}

/* blocks until stdin has data: xpp_inbox.cpp calls it on its reader thread */
int xpp_read_stdin(char *buf, int n)
{
    DWORD got = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, static_cast<DWORD>(n), &got, NULL) || got == 0) return -1;
    return static_cast<int>(got);
}

void xpp_binary_mode(int fd) { _setmode(fd, _O_BINARY); }

/* xpp_files.cpp: a link is never read or written through */
int xpp_path_is_link(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

int xpp_replace_file(const char *from, const char *to)
{
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}

std::string xpp_temp_folder(void)
{
    std::array<char, MAX_PATH> base; /* GetTempPathA writes it */
    DWORD n = GetTempPathA(static_cast<DWORD>(base.size()), base.data());
    if (n == 0 || n >= base.size()) return std::string();
    if (base[n - 1] == '\\') n--;
    return std::string(base.data(), n);
}

int xpp_process_running(unsigned long pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, static_cast<DWORD>(pid));
    DWORD code;
    int running;

    if (h == NULL) return 0; /* no such process */
    running = !GetExitCodeProcess(h, &code) || code == STILL_ACTIVE;
    CloseHandle(h);
    return running;
}

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

void xpp_win32_attach_console(void)
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
