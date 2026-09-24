#ifndef XPP_WINDOW_PLUGIN_H
#define XPP_WINDOW_PLUGIN_H

#include <stddef.h>
#include "xpp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The seam between xppautX and its window when the window is a library of
   its own (Linux, W13e): xpp_window.cpp and third_party/webview built into
   libxppwindow.so against GTK and WebKitGTK, embedded in xppautX and loaded
   from memory by xpp_window_loader.cpp only when the window opens, so
   xppautX itself needs neither library to start.

   The library sees nothing of the core: what it calls there comes in this
   table, filled by the loader. The static builds (Windows, macOS) fill it
   with the same functions at compile time, so the window's code is one. */
#define XPP_WINDOW_HOST_VERSION 1
typedef struct XppWindowHost {
    int version; /* XPP_WINDOW_HOST_VERSION */
    const char *(*http_url)(void);
    void (*http_release)(void);
    int (*http_said_bye)(void);
    void (*inbox_push)(const char *line, size_t n);
    void (*log)(XppLogLevel level, const char *fmt, ...)
#ifdef __GNUC__
        __attribute__((format(printf, 2, 3)))
#endif
        ;
} XppWindowHost;

/* what the library gives back: xpp_window.h's run and set_model */
typedef struct XppWindowApi {
    int (*run)(void (*session)(void), const char *about);
    void (*set_model)(const char *path);
} XppWindowApi;

/* The library's one exported function (every other symbol is local: its
   version script): keeps host (it must outlive the library), fills api;
   0 when host's version is not the library's. */
typedef int (*XppWindowPluginInit)(const XppWindowHost *host, XppWindowApi *api);
#define XPP_WINDOW_PLUGIN_INIT "xpp_window_plugin_init"

#ifdef __cplusplus
}
#endif
#endif
