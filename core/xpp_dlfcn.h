#ifndef XPP_DLFCN_H
#define XPP_DLFCN_H

/* dlopen/dlsym for extra.c (dll_lib= and dll_fun= in an .ode file). On
   Windows xpp_win32.c maps them to LoadLibrary/GetProcAddress, so a model
   can load a .dll the way it loads a .so elsewhere. */
#ifdef _WIN32
#define RTLD_LAZY 1
#define RTLD_NOW 2
void *dlopen(const char *name, int flag);
void *dlsym(void *handle, const char *name);
int dlclose(void *handle);
char *dlerror(void);
#else
#include <dlfcn.h>
#endif

#endif
