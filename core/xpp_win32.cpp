/* Windows replacements for the few POSIX calls the core and the protocol
   front end use. Kept in one file so <windows.h> (whose macros clash with
   core names such as max, MessageBox and VARTYPE) is included nowhere else.
   Empty on other systems. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xpp_dlfcn.h"
#include "xpp_win32.h"
#include "xpp_util.h"
#include "xpp_mem.h"
#include "xpp_io.h"

static const char *dl_error;

void *dlopen(const char *name, int flag)
{
    HMODULE h = LoadLibraryA(name);
    (void)flag;
    dl_error = h ? NULL : "LoadLibrary failed";
    return (void *)h;
}

void *dlsym(void *handle, const char *name)
{
    FARPROC f = GetProcAddress((HMODULE)handle, name);
    dl_error = f ? NULL : "GetProcAddress failed";
    return (void *)f;
}

int dlclose(void *handle) { return FreeLibrary((HMODULE)handle) ? 0 : -1; }

char *dlerror(void)
{
    const char *e = dl_error;
    dl_error = NULL;
    return (char *)e;
}

/* blocks until stdin has data: xpp_inbox.cpp calls it on its reader thread */
int xpp_read_stdin(char *buf, int n)
{
    DWORD got = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, (DWORD)n, &got, NULL) || got == 0) return -1;
    return (int)got;
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

/* the Windows side of xpp_util.c's AUTO scratch directory */
char *xpp_make_temp_dir(void)
{
    char base[MAX_PATH];
    char *path;
    DWORD n;
    int i;

    n = GetTempPathA(sizeof(base), base);
    if (n == 0 || n >= sizeof(base)) return NULL;
    if (n > 0 && base[n - 1] == '\\') base[--n] = 0;
    path = (char *)xpp_malloc((size_t)n + 64);
    for (i = 0; i < 1000; i++) {
        /* path is a pointer, allocated n+64 bytes just above. */
        xpp_snprintf(path, (size_t)n + 64, "%s\\xppautoX-%lu-%d", base, (unsigned long)GetCurrentProcessId(), i);
        if (_mkdir(path) == 0) return path;
    }
    xpp_free(path);
    return NULL;
}

static int scratch_pid_running(unsigned long pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    DWORD code;
    int running;

    if (h == NULL) return 0; /* no such process */
    running = !GetExitCodeProcess(h, &code) || code == STILL_ACTIVE;
    CloseHandle(h);
    return running;
}

/* issue #32: see xpp_util.c's POSIX twin for why. Windows names by the
   same "xppautoX-<pid>-N" pattern; OpenProcess fails when pid no longer
   names a process (or GetExitCodeProcess says it already exited). */
void xpp_cleanup_stale_scratch_dirs(void)
{
    char base[MAX_PATH];
    char pattern[MAX_PATH + 16];
    char path[2 * MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    DWORD n = GetTempPathA(sizeof(base), base);

    if (n == 0 || n >= sizeof(base)) return;
    if (n > 0 && base[n - 1] == '\\') base[--n] = 0;
    snprintf(pattern, sizeof(pattern), "%s\\xppautoX-*", base);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        unsigned long pid;
        int idx, consumed = -1;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (sscanf(fd.cFileName, "xppautoX-%lu-%d%n", &pid, &idx, &consumed) != 2) continue;
        if (consumed < 0 || fd.cFileName[consumed] != '\0') continue;
        if (scratch_pid_running(pid)) continue;
        snprintf(path, sizeof(path), "%s\\%s", base, fd.cFileName);
        xpp_remove_temp_dir(path);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
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
    if (!out) (void)freopen("CONOUT$", "w", stdout);
    if (!err) (void)freopen("CONOUT$", "w", stderr);
    if (!in) (void)freopen("CONIN$", "r", stdin);
}

void xpp_remove_temp_dir(const char *dir)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pattern[2 * MAX_PATH];
    char path[2 * MAX_PATH];

    if (dir == NULL) return;
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
            DeleteFileA(path);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}
#else
typedef int xpp_win32_unused;
#endif
