#include "open_model.h"
/* The desktop window (xpp_window.h): web2 in the operating system's web
   view, through the vendored webview library (third_party/webview, built
   as its own object from src/webview.cc; this file sees only its C API).

   This is the window's shared logic; what differs by platform (the file
   dialogs, the close box and the icon: <windows.h> and <shobjidl.h> on
   Windows, GTK on Linux, the Objective-C runtime on macOS) is the
   primitives of xpp_window_internal.h, implemented in xpp_window_win.cpp,
   xpp_window_linux.cpp and xpp_window_mac.cpp, so those headers never
   reach this file. Like xpp_http.cpp it includes no core header but small
   APIs (xpp_http.h, xpp_inbox.h, xpp_log.h, ui_json.h), and it calls the core
   through an XppWindowHost table (xpp_window_plugin.h).

   Built with XPP_WINDOW defined when the build has a web view (the
   Makefile); without it every function says "no window". On Linux it is
   also built with XPP_WINDOW_PLUGIN, into libxppwindow.so: then its one
   export is xpp_window_plugin_init, which xpp_window_loader.cpp (in
   xppautX) calls with the table, and xpp_window.h's functions are the
   loader's. */
#include "xpp_window.h"
#include "xpp_window_plugin.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_log.h"
#include "ui_json.h"
#include "xpp_webview.h"
#include "xpp_window_hint.h"
#include <array>
#include <cctype>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#if !defined(XPP_WINDOW_PLUGIN) && (!defined(XPP_WINDOW) || !defined(__APPLE__))
bool xpp::window::adopt_launch_document(std::vector<std::string> &) { return false; }
#endif

#ifndef XPP_WINDOW

bool xpp::window::supported() { return false; }
bool xpp::window::run(void (*)()) { return false; }
void xpp::window::set_model(const char *) {}

#else /* XPP_WINDOW */

#include "xpp_window_internal.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

namespace xpp::window::platform {

namespace {

/* the core's functions the window calls */
#ifdef XPP_WINDOW_PLUGIN
const XppWindowHost *host; /* the loader's, kept for the process's life */
#else
const XppWindowHost host_table = {XPP_WINDOW_HOST_VERSION,
                                  xpp::http::url,
                                  xpp::http::release,
                                  xpp::http::said_bye,
                                  [](const char *line, size_t n) { xpp::inbox::push({line, n}); },
                                  xpp::json_ui_push_open,
                                  xpp::log_message,
                                  xpp::http::open_release_page,
                                  xpp::http::open_about_link,
                                  xpp::inbox::authorize_save};
const XppWindowHost *const host = &host_table;
#endif

} // namespace

const XppWindowHost *host_api() { return host; }

/* the restored size (W230: the window opens maximized, and un-maximizing
   gives this): web2 needs room for its panels */
constexpr int WIDTH = 1280, HEIGHT = 840;

Placement fit_in(int width, int height, int area_x, int area_y, int area_width, int area_height)
{
    width = std::min(width, area_width * 95 / 100);
    height = std::min(height, area_height * 95 / 100);
    return {area_x + (area_width - width) / 2, area_y + (area_height - height) / 2, width, height};
}

/* A view in a window of its own (W645): the page's pop-out, the same page
   as the main window's at its address with `#popout=<key>`, showing that one
   view. `key` is what the page named it, kept to find the window again. */
struct PopOut {
    std::string key;
    webview_t view;
};

/* after the window closed, how long the core has to exit on its own */
constexpr auto EXIT_GRACE = std::chrono::seconds(10);

/* The shared state. Allocated once and never freed: the UI thread may
   still run while exit() destroys statics. */
struct State {
    std::mutex mu;
    webview_t view = nullptr;  /* while the window is up */
    bool core_closing = false; /* the core's exit closes it (no quit to send) */
    bool core_exiting = false; /* the core is exiting: a close box closes the window */
    std::string model;         /* the title's file name */
    std::string error_msg;     /* why the window failed to open (W35e) */
    std::vector<PopOut> popouts; /* the views in windows of their own: the UI thread's only */
    std::promise<void> done;   /* the window has closed and let go */
    std::shared_future<void> done_f = done.get_future().share();
};
namespace { State *st; }

std::string title_of(const std::string &model)
{
    return model.empty() ? std::string("xppautX") : "xppautX \xe2\x80\x94 " + model; /* an em dash */
}

/* webview_dispatch's callbacks run on the UI thread */
void set_title_cb(webview_t w, void *)
{
    std::string t;
    {
        std::lock_guard<std::mutex> lk(st->mu);
        t = title_of(st->model);
    }
    webview_set_title(w, t.c_str());
}

void terminate_cb(webview_t w, void *) { webview_terminate(w); }

/* The close box and File > Quit (W59d, W110), on the UI thread: while the
   core serves the session, the page's leave question (web2's __xppQuit,
   web2/src/desktop.ts), and the window stays. The page asks it itself
   while a command runs, the run going on (its Don't save closes the
   window: __xppCloseWindow, below), and else sends the protocol's quit
   that asks, which the core asks as File > Quit does; the core's exit
   after its bye closes the window (on_exit). A page without the hook (it
   did not load) closes the window: the plain quit (window_closed). True
   when it asked; false once the core is exiting (an error left the window
   open on its log), when the window closes. */
bool quit_asking()
{
    webview_t w;
    {
        std::lock_guard<std::mutex> lk(st->mu);
        if (st->core_closing || st->core_exiting || !st->view) return false;
        w = st->view;
    }
    webview_eval(w, "window.__xppQuit ? window.__xppQuit() : window.__xppCloseWindow()");
    return true;
}

webview_t main_view()
{
    std::lock_guard<std::mutex> lk(st->mu);
    return st->view;
}

void set_main_view(webview_t view)
{
    std::lock_guard<std::mutex> lk(st->mu);
    st->view = view;
}

/* A native window's close box, on the UI thread (Windows' WM_CLOSE, GTK's
   delete-event, Cocoa's windowShouldClose:): a pop-out's asks its page to pop
   in, which closes it (web2's __xppPopIn, then __xppClosePopOut; a page
   without the hook is closed at once); the main window's is the leave
   question. True when the page was asked and the window stays. */
bool close_asking(void *native)
{
    for (const PopOut &p : st->popouts) {
        if (webview_get_window(p.view) != native) continue;
        webview_eval(p.view, "window.__xppPopIn ? window.__xppPopIn() : window.__xppClosePopOut()");
        return true;
    }
    /* a window that is neither (a pop-out already taken off the list, being destroyed) just closes */
    const webview_t main = main_view();
    return main && webview_get_window(main) == native && quit_asking();
}

/* __xppCloseWindow (W110): the leave question's Don't save in the window,
   which closes as before W59d: window_closed sends the plain quit, which
   ends even a computation that never reaches a checkpoint (EXIT_GRACE).
   On the UI thread; called from the library: nothing may throw. */
void close_window_cb(const char *id, const char *, void *arg)
{
    webview_t w = static_cast<webview_t>(arg);
    webview_return(w, id, 0, "null");
    webview_terminate(w);
}


/* The window is gone (closed by the user, File > Quit, or the core's exit).
   On the window's own thread. Unless the core closed it, Quit the session:
   the protocol's quit (the running job is cancelled, the core exits), and
   xpp_http stops waiting for Ctrl+C after an error (nothing shows the page
   any more). The core then has EXIT_GRACE to end the process. */
void window_closed(webview_t w)
{
    bool by_core;
    {
        std::lock_guard<std::mutex> lk(st->mu);
        st->view = nullptr;
        by_core = st->core_closing;
    }
    for (const PopOut &p : st->popouts) webview_destroy(p.view); /* the main window's closing closes its pop-outs */
    st->popouts.clear();
    webview_destroy(w);
    if (!by_core) {
        static constexpr std::string_view quit = "{\"cmd\":\"session.quit\",\"save\":false}";
        host->http_release();
        host->inbox_push(quit.data(), quit.size());
    }
    st->done.set_value();
    std::this_thread::sleep_for(EXIT_GRACE);
    host->log(XPP_LOG_WARN, "xppautX: the session did not end after its window closed; ending it\n");
    std::_Exit(by_core ? 1 : 0);
}

/* The core's exit (atexit, registered after xpp_http's, so it runs first).
   After a Quit (the core said bye) the window closes; after an error it
   stays open on the page's log, and xpp_http's exit waits until it is
   closed. */
void on_exit()
{
    std::unique_lock<std::mutex> lk(st->mu);
    st->core_exiting = true;
    if (st->view == nullptr) {
        lk.unlock();
        host->http_release();
        st->done_f.wait_for(std::chrono::seconds(2));
        return;
    }
    if (!host->http_said_bye()) return;
    st->core_closing = true;
    webview_dispatch(st->view, terminate_cb, nullptr);
    lk.unlock();
    st->done_f.wait_for(std::chrono::seconds(3));
}

/* ---- the native file dialog (W88) ---------------------------------------
   A `file` ask shown in the window is answered from the operating system's
   own open or save dialog, with the true path picked (docs/ui-v2.md section
   4): web2 calls the page's __xppFileDialog, bound below, instead of showing
   its own dialog, and answers the ask with what it resolves to. What the
   dialog filters by (exts) is web2's wildExtensions (web2/src/pickers.ts),
   the one reading of an ask's pattern; File > Open model's is its own.
   A save dialog confirms an existing destination itself. Its file answer
   carries replace:1, so the core writes the chosen path without another
   question. Browser pickers choose a separate delivery destination. */

/* the page's __xppFileDialog({mode, title, dir, file, wild, exts}), in
   webview's JSON (xpp_webview.h) */
FileDialog file_dialog_of(const std::string &request)
{
    const std::string o = xpp::webview_json_value(request, "", 0);
    FileDialog d;
    d.save = xpp::webview_json_value(o, "mode", 0) == "write";
    d.title = xpp::webview_json_value(o, "title", 0);
    d.dir = xpp::webview_json_value(o, "dir", 0);
    d.file = xpp::webview_json_value(o, "file", 0);
    d.filter = xpp::webview_json_value(o, "wild", 0);
    const std::string exts = xpp::webview_json_value(o, "exts", 0);
    for (int i = 0;; i++) {
        std::string e = xpp::webview_json_value(exts, "", i);
        if (e.empty()) break;
        d.exts.push_back(std::move(e));
    }
    return d;
}

/* __xppFileDialog's call, on the UI thread (webview dispatches it there,
   out of the web view's own event): resolves with the path picked, null
   when cancelled, and rejects when the dialog could not open. Called from
   the library: nothing may throw. */
void file_dialog_cb(const char *id, const char *request, void *arg)
{
    webview_t w = static_cast<webview_t>(arg);
    int status = 0;
    std::string reply = "null";
    try {
        const std::string input = request ? request : "";
        const FileDialog dialog = file_dialog_of(input);
        std::optional<std::string> path = pick_file(webview_get_window(w), dialog);
        if (!path) {
            status = 1;
            reply = xpp::webview_json_quote("the file dialog could not open");
        } else if (!path->empty()) {
            if (dialog.save) {
                const std::string object = xpp::webview_json_value(input, "", 0);
                host->authorize_save(xpp::webview_json_value(object, "ask", 0), *path);
            }
            reply = xpp::webview_json_quote(*path);
        }
    } catch (const std::exception &e) {
        status = 1;
        reply = "null";
        host->log(XPP_LOG_WARN, "xppautX: the file dialog failed: {}\n", e.what());
    }
    webview_return(w, id, status, reply.c_str());
}

/* A binding that opens a URL in the system browser. It may be called by a
   tampered page: `open` is a core function that checks the URL before opening it. */
void open_url_cb(const char *id, const char *request, void *arg, xpp::Result<> (*open)(std::string_view url),
                 std::string_view what, std::string_view where)
{
    webview_t w = static_cast<webview_t>(arg);
    constexpr size_t REQUEST_LIMIT = 1024; /* JSON escaping of a bounded URL */
    xpp::Result<> result = xpp::fail(std::string(what), "request refused", xpp::Place{std::string(where), 1});
    try {
        if (request && std::strlen(request) <= REQUEST_LIMIT) result = open(xpp::webview_json_value(request, "", 0));
    } catch (const std::exception &e) {
        result = xpp::fail(std::string(what), e.what(), xpp::Place{std::string(where), 1});
    } catch (...) {
        result = xpp::fail(std::string(what), "request failed", xpp::Place{std::string(where), 1});
    }
    const std::string reply = result ? "null" : xpp::webview_json_quote(result.error().text());
    webview_return(w, id, result ? 0 : 1, reply.c_str());
}

void open_release_cb(const char *id, const char *request, void *arg)
{
    open_url_cb(id, request, arg, host->open_release_page, "update check", "Check for updates");
}

/* the page's About links, in the desktop window (a click on an anchor would navigate the web view away) */
void open_about_link_cb(const char *id, const char *request, void *arg)
{
    open_url_cb(id, request, arg, host->open_about_link, "About", "About");
}

/* ---- pop-outs (W645) ----------------------------------------------------
   The page's "Pop out" on a view (a graph, an autoX view, a dock tab) calls
   __xppPopOut(key, title), bound in the main window only: a second native
   window in this process, on the UI thread that runs the main one (a
   webview is one window with its own engine; the library counts windows
   process-wide and runs one loop per thread, which serves every window made
   on it: GTK's main context, Windows' message loop, Cocoa's one NSApplication
   loop). Its page is the main window's at the same tokened address with
   `#popout=<key>` (a fragment never reaches the server), bound with no more
   than __xppFileDialog (as in the main window: the same picker, the same
   authorisation of a save) and __xppClosePopOut (this window alone: the
   main window's __xppCloseWindow ends the session, a pop-out's never can).
   The key is the page's own percent-encoded name of the view; a tampered
   page can only ask for a window on our own address with a fragment made of
   unreserved characters, and no more than MAX_POPOUTS of them. */
constexpr int POPOUT_WIDTH = 900, POPOUT_HEIGHT = 640; /* the restored size; the system resizes and places it */
constexpr size_t MAX_POPOUTS = 8; /* each is an event stream of the 16 the server holds, beside the main window's and a browser's */
constexpr size_t POPOUT_KEY_MAX = 4096; /* a name of 1024 bytes, every one percent-encoded, and its kind */
constexpr size_t POPOUT_TITLE_MAX = 2048; /* a name of 1024 bytes and a few words, escaped in JSON */
constexpr std::string_view POPOUT_FRAGMENT = "#popout=";

/* a key is encodeURIComponent's output: unreserved characters and %XX only */
bool popout_key_ok(const std::string &key)
{
    if (key.empty() || key.size() > POPOUT_KEY_MAX) return false;
    return std::all_of(key.begin(), key.end(), [](unsigned char c) { return std::isalnum(c) || std::strchr("%-_.~", c); });
}

/* __xppClosePopOut: this window is closed, on the UI thread, out of the library's own callback; the page that
   called it gets no reply (it is going), and the destruction is queued on the main window's loop behind whatever
   this window still has queued (webview_destroy inside the window's own callback would free what the library
   still uses). Called from the library: nothing may throw. */
void destroy_popout_cb(webview_t, void *arg)
{
    webview_t popout = static_cast<webview_t>(arg);
    auto &list = st->popouts;
    auto it = std::find_if(list.begin(), list.end(), [popout](const PopOut &p) { return p.view == popout; });
    if (it == list.end()) return;
    list.erase(it);
    webview_destroy(popout);
}

void close_popout_cb(const char *, const char *, void *arg)
{
    const webview_t main = main_view();
    if (main) webview_dispatch(main, destroy_popout_cb, arg);
}

/* the pop-out's window, the page loading in it; NULL with why */
webview_t open_popout_view(const std::string &key, const std::string &title, std::string &why)
{
    int code = 0;
    std::string detail;
    webview_t w = xpp::webview_create(false, nullptr, code, detail);
    if (!w) {
        why = xpp::webview_error_message(code, detail);
        return nullptr;
    }
    webview_set_title(w, title.c_str());
    webview_set_size(w, POPOUT_WIDTH, POPOUT_HEIGHT, WEBVIEW_HINT_NONE);
    prepare_popout(w);
    if (has_file_dialog()) webview_bind(w, "__xppFileDialog", file_dialog_cb, w);
    webview_bind(w, "__xppClosePopOut", close_popout_cb, w);
    webview_navigate(w, (std::string(host->http_url()) + std::string(POPOUT_FRAGMENT) + key).c_str());
    return w;
}

/* __xppPopOut(key, title): the view's window opened, or brought to the front when it is open already; rejects with
   why when it cannot be. On the UI thread; called from the library: nothing may throw. */
void popout_cb(const char *id, const char *request, void *arg)
{
    webview_t w = static_cast<webview_t>(arg);
    int status = 0;
    std::string reply = "null";
    try {
        const std::string input = request ? request : "";
        const std::string key = xpp::webview_json_value(input, "", 0);
        std::string title = xpp::webview_json_value(input, "", 1);
        std::string why;
        auto &list = st->popouts;
        auto open = std::find_if(list.begin(), list.end(), [&key](const PopOut &p) { return p.key == key; });
        if (!popout_key_ok(key) || title.size() > POPOUT_TITLE_MAX) {
            why = "the view's name is not one a window can be made for";
        } else if (open != list.end()) {
            raise_window(webview_get_window(open->view));
        } else if (list.size() >= MAX_POPOUTS) {
            why = std::to_string(MAX_POPOUTS) + " views are in windows of their own already: pop one in first";
        } else {
            webview_t popout = open_popout_view(key, title.empty() ? "xppautX" : title, why);
            if (popout) list.push_back({key, popout});
        }
        if (!why.empty()) {
            status = 1;
            reply = xpp::webview_json_quote(why);
        }
    } catch (const std::exception &e) {
        status = 1;
        reply = xpp::webview_json_quote(e.what());
    }
    webview_return(w, id, status, reply.c_str());
}

/* the window, on the thread that runs it; NULL when it cannot open, with
   why in st->error_msg */
webview_t open_view()
{
    int code = 0;
    std::string why;
    webview_t w = xpp::webview_create(false, nullptr, code, why);
    if (!w) {
        st->error_msg = xpp::webview_error_message(code, why);
        return nullptr;
    }
    std::string t;
    {
        std::lock_guard<std::mutex> lk(st->mu);
        t = title_of(st->model);
    }
    webview_set_title(w, t.c_str());
    webview_set_size(w, WIDTH, HEIGHT, WEBVIEW_HINT_NONE);
    prepare_window(w);
    place_window(w);
    /* before the page loads, so it is there from its first script */
    if (has_file_dialog()) webview_bind(w, "__xppFileDialog", file_dialog_cb, w);
    webview_bind(w, "__xppCloseWindow", close_window_cb, w);
    webview_bind(w, "__xppPopOut", popout_cb, w);
    webview_bind(w, "__xppOpenRelease", open_release_cb, w);
    webview_bind(w, "__xppOpenAboutLink", open_about_link_cb, w);
    /* the token stays out of sight: the web view has no address bar */
    webview_navigate(w, host->http_url());
    return w;
}

/* the warning when the window does not open (why, when webview said) */
const char *no_view_message()
{
    if (st->error_msg.empty()) st->error_msg = xpp::data_message();
    return st->error_msg.c_str();
}

/* xpp_window.h's set_model and run: the library's (XppWindowApi) in the
   plugin, else the public functions below */
void set_window_model(const char *path)
{
    if (!st || !path) return;
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    std::lock_guard<std::mutex> lk(st->mu);
    st->model = base;
    if (st->view) webview_dispatch(st->view, set_title_cb, nullptr);
}

/* the window on a thread of its own (Windows' message loop wants an STA thread, GTK one thread that
   initialises and runs it), the session on the caller's: false when the window cannot open */
bool run_on_ui_thread()
{
    auto ready = std::make_shared<std::promise<webview_t>>();
    std::future<webview_t> up = ready->get_future();
    std::thread([ready] {
        webview_t w = open_view();
        if (w) set_main_view(w);
        ready->set_value(w);
        if (!w) return;
        webview_run(w);
        window_closed(w);
    }).detach();
    if (!up.get()) {
        host->log(XPP_LOG_WARN, "{}", no_view_message());
        return false;
    }
    std::atexit(on_exit);
    return true;
}

bool run_window(void (*session)())
{
    try {
        st = new State;
        if (!run_window_ui(session)) return false;
    } catch (const std::exception &e) {
        host->log(XPP_LOG_WARN, "{}", xpp::data_message(e.what()));
        return false;
    }
    session();
    return true;
}

} // namespace xpp::window::platform

#ifdef XPP_WINDOW_PLUGIN
extern "C" __attribute__((visibility("default"))) int xpp_window_plugin_init(const XppWindowHost *h,
                                                                             XppWindowApi *api)
{
    if (!h || h->version != XPP_WINDOW_HOST_VERSION || !api) return 0;
    xpp::window::platform::host = h;
    api->run = xpp::window::platform::run_window;
    api->set_model = xpp::window::platform::set_window_model;
    return 1;
}
#else
bool xpp::window::supported() { return true; }
void xpp::window::set_model(const char *path) { xpp::window::platform::set_window_model(path); }
bool xpp::window::run(void (*session)()) { return xpp::window::platform::run_window(session); }
#endif

#endif /* XPP_WINDOW */
