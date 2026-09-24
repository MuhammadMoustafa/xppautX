/* The desktop window (xpp_window.h): web2 in the operating system's web
   view, through the vendored webview library (third_party/webview, built
   as its own object from src/webview.cc; this file sees only its C API).

   Like xpp_http.cpp this file includes no core header but small C APIs
   (xpp_http.h, xpp_inbox.h, xpp_log.h), so the platform headers it needs
   for the menu bar (<windows.h> on Windows, GTK on Linux) cannot clash
   with core names: the one exception to "Windows API code lives only in
   xpp_win32.c", kept behind _WIN32 and out of every header.

   Built with XPP_WINDOW defined when the build has a web view (the
   Makefile); without it every function says "no window". */
#include "xpp_window.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_log.h"
#include <cstring>
#include <string>

#ifndef XPP_WINDOW

int xpp_window_supported(void) { return 0; }
int xpp_window_run(void (*session)(void), const char *about)
{
    (void)session;
    (void)about;
    return 0;
}
void xpp_window_set_model(const char *path) { (void)path; }

#else /* XPP_WINDOW */

#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#define WEBVIEW_HEADER /* declarations only: the library is webview.o */
#define WEBVIEW_STATIC
#include "webview/webview.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#elif defined(__APPLE__)
#include <pthread.h>
#include <unistd.h>
#else
#include <gtk/gtk.h>
#include <unistd.h>
#ifdef XPP_ICON_ASSET
/* build/obj/icon_assets.c (tools/embed_bytes.c), a plain C object: declared
   at file scope, not inside the anonymous namespace below, so it keeps C
   linkage instead of being mangled as one of its members */
extern "C" const unsigned char xpp_icon_png[];
extern "C" const unsigned long xpp_icon_png_len;
#endif
#endif

namespace {

/* what the window shows first: web2 needs room for its panels */
constexpr int WIDTH = 1280, HEIGHT = 840;
/* after the window closed, how long the core has to exit on its own */
constexpr auto EXIT_GRACE = std::chrono::seconds(10);

/* The shared state. Allocated once and never freed: the UI thread may
   still run while exit() destroys statics. */
struct State {
    std::mutex mu;
    webview_t view = nullptr;  /* while the window is up */
    bool core_closing = false; /* the core's exit closes it (no quit to send) */
    std::string model;         /* the title's file name */
    std::string about;         /* Help > About's text */
    std::promise<void> done;   /* the window has closed and let go */
    std::shared_future<void> done_f = done.get_future().share();
};
State *st;

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

/* Help > Manual (chapter NULL: where Help was left, as F1) and Help >
   Keyboard shortcuts: web2's hook (web2/src/desktop.ts) */
[[maybe_unused]] void open_help(webview_t w, const char *chapter)
{
    std::string js = "window.__xppOpenHelp && window.__xppOpenHelp(";
    if (chapter) js += std::string("'") + chapter + "'";
    js += ")";
    webview_eval(w, js.c_str());
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
    webview_destroy(w);
    if (!by_core) {
        static const char quit[] = "{\"cmd\":\"quit\"}";
        xpp_http_release();
        xpp_inbox_push(quit, sizeof quit - 1);
    }
    st->done.set_value();
    std::this_thread::sleep_for(EXIT_GRACE);
    xpp_log(XPP_LOG_WARN, "xppautX: the session did not end after its window closed; ending it\n");
    std::_Exit(by_core ? 1 : 0);
}

/* The core's exit (atexit, registered after xpp_http's, so it runs first).
   After a Quit (the core said bye) the window closes; after an error it
   stays open on the page's log, and xpp_http's exit waits until it is
   closed. */
void on_exit()
{
    std::unique_lock<std::mutex> lk(st->mu);
    if (st->view == nullptr) {
        lk.unlock();
        xpp_http_release();
        st->done_f.wait_for(std::chrono::seconds(2));
        return;
    }
    if (!xpp_http_said_bye()) return;
    st->core_closing = true;
    webview_dispatch(st->view, terminate_cb, nullptr);
    lk.unlock();
    st->done_f.wait_for(std::chrono::seconds(3));
}

/* ---- File > Open model: a new xppautX with the file ---------------------
   The core has no way to load another model into a running session, so a
   second model is a second process (its own window), started in the
   model's folder as a double-click would. */

/* ---- the platform's menu bar, icon and dialogs -------------------------- */

enum MenuId { ID_OPEN = 101, ID_QUIT, ID_MANUAL, ID_KEYS, ID_ABOUT };
[[maybe_unused]] const char *const KEYS_CHAPTER = "05-commands"; /* the hotkeys, from its first paragraph */

#if defined(_WIN32)

std::wstring wide(const std::string &s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? (size_t)n : 1, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    w.resize(w.size() - 1);
    return w;
}

void open_model(HWND owner)
{
    wchar_t file[32768] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"XPP models (*.ode)\0*.ode\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof file / sizeof file[0];
    ofn.lpstrTitle = L"Open model";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    std::wstring path(file), dir = path.substr(0, ofn.nFileOffset);
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\" \"" + path + L"\"";
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    /* a console of its own that nobody sees: its window is the web view */
    if (CreateProcessW(exe, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                       dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else
        xpp_log(XPP_LOG_ERROR, "xppautX: cannot start a second xppautX (error %lu)\n", GetLastError());
}

WNDPROC webview_proc; /* the library's own window procedure */

LRESULT CALLBACK menu_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_COMMAND && HIWORD(wp) == 0) {
        webview_t w;
        {
            std::lock_guard<std::mutex> lk(st->mu);
            w = st->view;
        }
        switch (LOWORD(wp)) {
        case ID_OPEN: open_model(hwnd); return 0;
        case ID_QUIT: PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
        case ID_MANUAL: if (w) open_help(w, nullptr); return 0;
        case ID_KEYS: if (w) open_help(w, KEYS_CHAPTER); return 0;
        case ID_ABOUT:
            MessageBoxW(hwnd, wide(st->about).c_str(), L"About xppautX", MB_OK | MB_ICONINFORMATION);
            return 0;
        default: break;
        }
    }
    return CallWindowProcW(webview_proc, hwnd, msg, wp, lp);
}

void add_menus(webview_t w)
{
    HWND hwnd = static_cast<HWND>(webview_get_window(w));
    if (!hwnd) return;
    /* the icon: resource 32512 (IDI_APPLICATION's number) in assets/xppautx.rc,
       which the library already set as the class's large icon */
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HANDLE icon = LoadImageW(inst, MAKEINTRESOURCEW(32512), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                             GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (icon) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));

    HMENU bar = CreateMenu(), file = CreatePopupMenu(), help = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_OPEN, L"&Open model…");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_QUIT, L"&Quit");
    AppendMenuW(help, MF_STRING, ID_MANUAL, L"&Manual");
    AppendMenuW(help, MF_STRING, ID_KEYS, L"&Keyboard shortcuts");
    AppendMenuW(help, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(help, MF_STRING, ID_ABOUT, L"&About xppautX");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Help");
    webview_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(menu_proc)));
    SetMenu(hwnd, bar);
}

#elif defined(__APPLE__)

/* macOS: no menu bar of ours yet, and the icon comes with the .app bundle
   (W13b) */
void add_menus(webview_t) {}

#else /* Linux: GTK 3 (webkit2gtk-4.1) */

#if GTK_MAJOR_VERSION >= 4
void add_menus(webview_t) {} /* webkitgtk-6.0 (GTK 4) has no GtkMenuBar */
#else

webview_t view_of_menu()
{
    std::lock_guard<std::mutex> lk(st->mu);
    return st->view;
}

void open_model(GtkWindow *parent)
{
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Open model", parent, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel",
                                                 GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, nullptr);
    GtkFileFilter *ode = gtk_file_filter_new(), *all = gtk_file_filter_new();
    gtk_file_filter_set_name(ode, "XPP models (*.ode)");
    gtk_file_filter_add_pattern(ode, "*.ode");
    gtk_file_filter_set_name(all, "All files");
    gtk_file_filter_add_pattern(all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), ode);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), all);
    char *path = nullptr;
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
        path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
    gtk_widget_destroy(dlg);
    if (!path) return;
    char *exe = g_file_read_link("/proc/self/exe", nullptr), *dir = g_path_get_dirname(path);
    char *argv[] = {exe, path, nullptr};
    GError *err = nullptr;
    /* the parent's descriptors (the listening socket, the log pipe) are
       closed in the child; its output would land in this page's log */
    if (!exe || !g_spawn_async(dir, argv, nullptr,
                               static_cast<GSpawnFlags>(G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
                               nullptr, nullptr, nullptr, &err)) {
        xpp_log(XPP_LOG_ERROR, "xppautX: cannot start a second xppautX: %s\n", err ? err->message : "no path");
        if (err) g_error_free(err);
    }
    g_free(exe);
    g_free(dir);
    g_free(path);
}

void on_menu(GtkMenuItem *, gpointer id_ptr)
{
    webview_t w = view_of_menu();
    if (!w) return;
    GtkWindow *win = GTK_WINDOW(webview_get_window(w));
    switch (GPOINTER_TO_INT(id_ptr)) {
    case ID_OPEN: open_model(win); break;
    case ID_QUIT: gtk_window_close(win); break;
    case ID_MANUAL: open_help(w, nullptr); break;
    case ID_KEYS: open_help(w, KEYS_CHAPTER); break;
    case ID_ABOUT: {
        GtkWidget *dlg = gtk_message_dialog_new(win, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s",
                                                st->about.c_str());
        gtk_window_set_title(GTK_WINDOW(dlg), "About xppautX");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        break;
    }
    default: break;
    }
}

GtkWidget *menu_item(GtkWidget *menu, const char *label, int id)
{
    GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);
    g_signal_connect(item, "activate", G_CALLBACK(on_menu), GINT_TO_POINTER(id));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    return item;
}

GtkWidget *top_menu(GtkWidget *bar, const char *label)
{
    GtkWidget *item = gtk_menu_item_new_with_mnemonic(label), *menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), item);
    return menu;
}

/* the window icon (item 4, W13b): the installed hicolor theme icon by name
   (tools/associate/install-linux.sh put it there), else the PNG embedded at
   build time (Makefile, XPP_ICON_ASSET) so an unpacked-but-not-installed
   build still has one instead of GTK's generic default. */
void set_window_icon(GtkWindow *win)
{
    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "xppautx")) {
        gtk_window_set_icon_name(win, "xppautx");
        return;
    }
#ifdef XPP_ICON_ASSET
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    GError *err = nullptr;
    if (gdk_pixbuf_loader_write(loader, xpp_icon_png, xpp_icon_png_len, &err) &&
        gdk_pixbuf_loader_close(loader, &err)) {
        GdkPixbuf *pix = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pix) gtk_window_set_icon(win, pix);
    } else {
        xpp_log(XPP_LOG_WARN, "xppautX: window icon: %s\n", err ? err->message : "unknown error");
    }
    if (err) g_error_free(err);
    g_object_unref(loader);
#else
    (void)win;
#endif
}

/* The library puts its web view straight into the window: move it into a
   box under a menu bar. */
void add_menus(webview_t w)
{
    GtkWidget *win = static_cast<GtkWidget *>(webview_get_window(w));
    GtkWidget *view = static_cast<GtkWidget *>(webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
    if (!win || !view) return;
    set_window_icon(GTK_WINDOW(win));
    GtkWidget *bar = gtk_menu_bar_new(), *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *file = top_menu(bar, "_File"), *help = top_menu(bar, "_Help");
    menu_item(file, "_Open model\xe2\x80\xa6", ID_OPEN);
    gtk_menu_shell_append(GTK_MENU_SHELL(file), gtk_separator_menu_item_new());
    menu_item(file, "_Quit", ID_QUIT);
    menu_item(help, "_Manual", ID_MANUAL);
    menu_item(help, "_Keyboard shortcuts", ID_KEYS);
    gtk_menu_shell_append(GTK_MENU_SHELL(help), gtk_separator_menu_item_new());
    menu_item(help, "_About xppautX", ID_ABOUT);
    g_object_ref(view);
    gtk_container_remove(GTK_CONTAINER(win), view);
    gtk_box_pack_start(GTK_BOX(box), bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), view, TRUE, TRUE, 0);
    g_object_unref(view);
    gtk_container_add(GTK_CONTAINER(win), box);
    gtk_widget_show_all(box);
}
#endif /* GTK 3 */

#endif /* platform */

/* the window, on the thread that runs it; NULL when it cannot open */
webview_t open_view()
{
    webview_t w = webview_create(0, nullptr);
    if (!w) return nullptr;
    std::string t;
    {
        std::lock_guard<std::mutex> lk(st->mu);
        t = title_of(st->model);
    }
    webview_set_title(w, t.c_str());
    webview_set_size(w, WIDTH, HEIGHT, WEBVIEW_HINT_NONE);
    add_menus(w);
    /* the token stays out of sight: the web view has no address bar */
    webview_navigate(w, xpp_http_url());
    return w;
}

const char NO_VIEW[] = "xppautX: the window cannot open (no web view: on Windows the WebView2 runtime, on Linux a "
                       "display); using the browser instead\n";

#ifdef __APPLE__
void (*session_fn)(void);

void *session_main(void *)
{
    session_fn();
    return nullptr;
}
#endif

} /* namespace */

int xpp_window_supported(void) { return 1; }

void xpp_window_set_model(const char *path)
{
    if (!st || !path) return;
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    std::lock_guard<std::mutex> lk(st->mu);
    st->model = base;
    if (st->view) webview_dispatch(st->view, set_title_cb, nullptr);
}

int xpp_window_run(void (*session)(void), const char *about)
{
    try {
        st = new State;
        st->about = about ? about : "";
#ifdef __APPLE__
        /* Cocoa: the window on the main thread, the session on another,
           with the main thread's 8 MB of stack (a new thread gets 512 KB) */
        webview_t w = open_view();
        if (!w) {
            xpp_log(XPP_LOG_WARN, "%s", NO_VIEW);
            return 0;
        }
        {
            std::lock_guard<std::mutex> lk(st->mu);
            st->view = w;
        }
        std::atexit(on_exit);
        pthread_attr_t attr;
        pthread_t core;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 8u << 20);
        session_fn = session;
        pthread_create(&core, &attr, session_main, nullptr);
        webview_run(w);
        window_closed(w); /* never returns */
        return 1;
#else
        auto ready = std::make_shared<std::promise<webview_t>>();
        std::future<webview_t> up = ready->get_future();
        std::thread([ready] {
            webview_t w = open_view();
            if (w) {
                std::lock_guard<std::mutex> lk(st->mu);
                st->view = w;
            }
            ready->set_value(w);
            if (!w) return;
            webview_run(w);
            window_closed(w);
        }).detach();
        if (!up.get()) {
            xpp_log(XPP_LOG_WARN, "%s", NO_VIEW);
            return 0;
        }
        std::atexit(on_exit);
#endif
    } catch (const std::exception &e) {
        xpp_log(XPP_LOG_WARN, "xppautX: the window cannot open (%s); using the browser instead\n", e.what());
        return 0;
    }
    session();
    return 1;
}

#endif /* XPP_WINDOW */
