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
    path = (char *)malloc((size_t)n + 64);
    if (path == NULL) return NULL;
    for (i = 0; i < 1000; i++) {
        sprintf(path, "%s\\xppautoX-%lu-%d", base, (unsigned long)GetCurrentProcessId(), i);
        if (_mkdir(path) == 0) return path;
    }
    free(path);
    return NULL;
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
