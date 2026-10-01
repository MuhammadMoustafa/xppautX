#ifndef XPP_WINDOW_H
#define XPP_WINDOW_H

/* The desktop window (xpp_window.cpp, docs/roadmap.md W13a), in namespace
   xpp::window (W109f): web2 in the operating system's own web view (the
   vendored third_party/webview: WebView2 on Windows, WebKitGTK on Linux,
   WKWebView on macOS), showing the page xpp_http.cpp serves on
   127.0.0.1, with the app's own menu bar (File: Open model, Quit; Help:
   Manual, Keyboard shortcuts, About).

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
   is closed.

   On Linux the window is a library of its own (libxppwindow.so, W13e):
   these functions load it (xpp_window_loader.cpp) and call it through the
   C table of xpp_window_plugin.h. */

namespace xpp::window {

/* true when this build has the window (Windows, macOS, and Linux built
   with WebKitGTK: the Makefile's pkg-config test), else false and xppautX
   is browser-only */
bool supported();

/* Open the window on http::url() and run session() -- the core's whole
   session, which ends the process with exit() -- with it: never returns
   when the window opened. Returns false at once, without running
   session(), when the window cannot open (no WebView2 runtime, no
   display): the caller falls back to browser mode. `about` is Help >
   About's text. */
bool run(void (*session)(), const char *about);

/* The model's file, for the title ("xppautX — lecar.ode"); any thread,
   a no-op without a window */
void set_model(const char *path);

#ifdef __APPLE__
/* The document Finder (a double-click, a drop on the app) or `open` asked
   xppautX.app to open when it launched it, which macOS sends as an
   open-documents Apple Event rather than an argument (W91): the model to
   load, from the session run() runs; nullptr when there was none.
   One that comes while the app runs is opened as File > Open model opens
   one (W61). */
const char *launch_document();
#endif

} // namespace xpp::window
#endif
