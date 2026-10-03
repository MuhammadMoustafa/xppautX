#ifndef XPP_WINDOW_PLUGIN_H
#define XPP_WINDOW_PLUGIN_H

#include <stddef.h>
#include "xpp_log.h"

/* The seam between xppautX and its window when the window is a library of
   its own (Linux, W13e): xpp_window.cpp and third_party/webview built into
   libxppwindow.so against GTK and WebKitGTK, embedded in xppautX and loaded
   from memory by xpp_window_loader.cpp only when the window opens, so
   xppautX itself needs neither library to start.

   The library sees nothing of the core: what it calls there comes in this
   table, filled by the loader. The static builds (Windows, macOS) fill it
   with the same functions at compile time, so the window's code is one. */
/* The checked-format callback replaces the varargs callback (W172). */
#define XPP_WINDOW_HOST_VERSION 4
typedef struct XppWindowHost {
    int version; /* XPP_WINDOW_HOST_VERSION */
    const char *(*http_url)(void);
    void (*http_release)(void);
    bool (*http_said_bye)(void);
    void (*inbox_push)(const char *line, size_t n);
    /* File > Open model's file, as the protocol's open command (ui_json.h) */
    void (*open_model)(const char *path);
    /* Checked at the call site; formatted and delivered by the core. */
    void (*log_message)(XppLogLevel level, std::string_view fmt, std::format_args args) noexcept;
    template <class... Args>
    void log(XppLogLevel level, std::format_string<Args...> fmt, Args &&...args) const noexcept
    {
        log_message(level, fmt.get(), std::make_format_args(args...));
    }
} XppWindowHost;

/* what the library gives back: xpp_window.h's run and set_model */
typedef struct XppWindowApi {
    bool (*run)(void (*session)(void), const char *about);
    void (*set_model)(const char *path);
} XppWindowApi;

/* The library's one exported function (every other symbol is local: its
   version script): keeps host (it must outlive the library), fills api;
   0 when host's version is not the library's. */
typedef int (*XppWindowPluginInit)(const XppWindowHost *host, XppWindowApi *api);
#define XPP_WINDOW_PLUGIN_INIT "xpp_window_plugin_init"

#endif
