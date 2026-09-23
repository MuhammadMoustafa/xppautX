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

/* blocks until stdin has data: xpp_inbox.c calls it on its reader thread */
int xpp_read_stdin(char *buf, int n)
{
    DWORD got = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, (DWORD)n, &got, NULL) || got == 0) return -1;
    return (int)got;
}

void xpp_binary_mode(int fd) { _setmode(fd, _O_BINARY); }
#else
typedef int xpp_win32_unused;
#endif
