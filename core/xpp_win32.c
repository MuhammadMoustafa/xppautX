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
#include "xpp_dlfcn.h"
#include "xpp_win32.h"

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

int xpp_read_stdin(char *buf, int n, int wait_ms)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD avail = 0, got = 0;
    int waited = 0;
    if (GetFileType(h) == FILE_TYPE_PIPE) {
        for (;;) {
            if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) return -1; /* closed */
            if (avail > 0 || wait_ms < 0) break;
            if (waited >= wait_ms) return 0;
            Sleep(5);
            waited += 5;
        }
        if (avail > 0 && avail < (DWORD)n) n = (int)avail;
    } else if (wait_ms >= 0 && WaitForSingleObject(h, (DWORD)wait_ms) != WAIT_OBJECT_0) {
        return 0;
    }
    if (!ReadFile(h, buf, (DWORD)n, &got, NULL) || got == 0) return -1;
    return (int)got;
}

void xpp_binary_mode(int fd) { _setmode(fd, _O_BINARY); }
#else
typedef int xpp_win32_unused;
#endif
