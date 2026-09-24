#ifndef XPP_WINDOW_H
#define XPP_WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

/* The desktop window (xpp_window.cpp, docs/roadmap.md W13a): web2 in the
   operating system's own web view (the vendored third_party/webview:
   WebView2 on Windows, WebKitGTK on Linux, WKWebView on macOS), showing
   the page xpp_http.cpp serves on 127.0.0.1, with the app's own menu bar
   (File: Open model, Quit; Help: Manual, Keyboard shortcuts, About).

   Threads: the core keeps the thread it has (the main thread) and stays
   single-threaded. On Windows and Linux the window runs its own UI loop on
   a thread of its own, which is all WebView2 (an STA thread with a message
   loop) and GTK (one thread that initialises and runs it) ask for. Cocoa
   insists on the main thread, so on macOS the window takes the main thread
   and the session runs on a second thread instead (untested).

   Closing the window pushes {"cmd":"quit"} into the inbox, the protocol's
   Quit (a running job is cancelled; the core exits), and releases
   xpp_http's wait after an error. When the core exits after a Quit the
   window closes; after an error it stays, showing the page's log, until it
   is closed. */

/* 1 when this build has the window (Windows, macOS, and Linux built with
   WebKitGTK: the Makefile's pkg-config test), else 0 and xppautX is
   browser-only */
int xpp_window_supported(void);

/* Open the window on xpp_http_url() and run session() -- the core's whole
   session, which ends the process with exit() -- with it: never returns
   when the window opened. Returns 0 at once, without running session(),
   when the window cannot open (no WebView2 runtime, no display): the
   caller falls back to browser mode. `about` is Help > About's text. */
int xpp_window_run(void (*session)(void), const char *about);

/* The model's file, for the title ("xppautX — lecar.ode"); any thread,
   a no-op without a window */
void xpp_window_set_model(const char *path);

#ifdef __cplusplus
}
#endif
#endif
