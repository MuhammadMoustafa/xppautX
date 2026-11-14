/* What xpp_window.cpp (the window's shared logic: its state, the close and
   quit questions, the page's bound calls, the pop-outs) shares with the file
   of each platform, which implements the primitives once: xpp_window_win.cpp
   (Windows: WebView2, the Common Item Dialog), xpp_window_linux.cpp (GTK 3 and
   WebKitGTK; also built into libxppwindow.so) and xpp_window_mac.cpp (Cocoa;
   the Objective-C runtime). The Makefile builds the one of its platform, and
   none with WINDOW=0. Internal to the xpp_window*.cpp files, and built only
   with a web view (XPP_WINDOW). */
#pragma once

#define WEBVIEW_HEADER /* declarations only: the library is webview.o */
#define WEBVIEW_STATIC
#include "webview/webview.h"

#include <optional>
#include <string>
#include <vector>

#include "xpp_window_plugin.h"

namespace xpp::window::platform {

/* ---- what the shared file gives the platform's --------------------------- */

/* the core's functions the window calls (the loader's table in the plugin) */
const XppWindowHost *host_api();

/* where a window opens (W13f): its size, which the display's scaling can
   make larger than the screen (1280x840 at 125% is 1600x1050), shrunk to
   95% of the work area (the monitor less the taskbar) when it does not
   fit, and centred in it; the window is then maximized (W230). */
struct Placement {
    int x, y, width, height;
};
Placement fit_in(int width, int height, int area_x, int area_y, int area_width, int area_height);

/* a native window's close box, on the UI thread (Windows' WM_CLOSE, GTK's
   delete-event, Cocoa's windowShouldClose:): a pop-out's asks its page to pop
   in, which closes it; the main window's is the leave question. True when the
   page was asked and the window stays. */
bool close_asking(void *native);
/* the leave question alone (the main window's close box, Cocoa's Quit): true when it asked */
bool quit_asking();
/* the main window's view while it is up, else nullptr */
webview_t main_view();

/* the file dialog the page asked for (W88): web2's __xppFileDialog */
struct FileDialog {
    bool save = false;
    std::string title;
    std::string dir;               /* the folder shown first ("": the system's choice) */
    std::string file;              /* the name offered (a base name) */
    std::string filter;            /* the filter's name */
    std::vector<std::string> exts; /* ".set" ...; none: no filter but All files */
};

/* the window opened on the thread that runs it (NULL when it cannot, its reason in no_view_message()),
   the core's exit hook (atexit: closes the window after a bye), the window gone, and the one
   run_window_ui's start of a window on a thread of its own is built from */
webview_t open_view();
void set_main_view(webview_t view);
void on_exit();
void window_closed(webview_t w); /* never returns */
const char *no_view_message();
bool run_on_ui_thread();

/* ---- what each platform implements --------------------------------------- */

/* whether a native file dialog exists (GTK 4 has none: web2 shows its own) */
bool has_file_dialog();
/* the platform's dialog, on the window's UI thread, owned by window (its native
   window): the path picked, "" when cancelled, nullopt when it could not open */
std::optional<std::string> pick_file(void *window, const FileDialog &d);
/* the window's close box, icon and menus, once the library has made it */
void prepare_window(webview_t w);
/* a pop-out: the same close box and icon as the main window's, no maximizing and no place of ours */
void prepare_popout(webview_t w);
/* a pop-out brought to the front, restored when minimized */
void raise_window(void *native);
/* fit_in on the monitor the window opened on, and maximized */
void place_window(webview_t w);
/* start the window and, where it keeps the thread, the session: false when the
   window cannot open; true when the caller is to run the session (Cocoa runs it on
   a thread of its own and never returns) */
bool run_window_ui(void (*session)());

} // namespace xpp::window::platform
