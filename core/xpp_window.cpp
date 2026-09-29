/* The desktop window (xpp_window.h): web2 in the operating system's web
   view, through the vendored webview library (third_party/webview, built
   as its own object from src/webview.cc; this file sees only its C API).

   Like xpp_http.cpp this file includes no core header but small C APIs
   (xpp_http.h, xpp_inbox.h, xpp_log.h, ui_json.h), so the platform headers it needs
   for the menu bar and the file dialogs (<windows.h> and <shobjidl.h> on
   Windows, GTK on Linux, the Objective-C runtime on macOS) cannot clash
   with core names: the one exception to "Windows API code lives only in
   xpp_win32.cpp", kept behind _WIN32 and out of every header. It calls them
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
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#ifndef XPP_WINDOW

int xpp_window_supported(void) { return 0; }
int xpp_window_run(void (*)(void), const char *) { return 0; }
void xpp_window_set_model(const char *) {}

#else /* XPP_WINDOW */

#include <algorithm>
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
#include <shobjidl.h>
#elif defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
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

/* the core's functions the window calls */
#ifdef XPP_WINDOW_PLUGIN
const XppWindowHost *host; /* the loader's, kept for the process's life */
#else
const XppWindowHost host_table = {XPP_WINDOW_HOST_VERSION, xpp_http_url, xpp_http_release, xpp_http_said_bye,
                                  xpp_inbox_push, json_ui_push_open, xpp_log};
const XppWindowHost *const host = &host_table;
#endif

/* what the window shows first: web2 needs room for its panels */
constexpr int WIDTH = 1280, HEIGHT = 840;

/* Where the window opens (W13f): its size, which the display's scaling can
   make larger than the screen (1280x840 at 125% is 1600x1050), shrunk to
   95% of the work area (the monitor less the taskbar) when it does not
   fit, and centred in it. */
struct Placement {
    int x, y, width, height;
};
[[maybe_unused]] Placement fit_in(int width, int height, int area_x, int area_y, int area_width, int area_height)
{
    width = std::min(width, area_width * 95 / 100);
    height = std::min(height, area_height * 95 / 100);
    return {area_x + (area_width - width) / 2, area_y + (area_height - height) / 2, width, height};
}

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
    std::string error_msg;     /* why the window failed to open (W35e) */
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
        static constexpr std::string_view quit = "{\"cmd\":\"quit\"}";
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
   A save dialog does not ask before replacing a file: the core does, as
   in every front end (open_writer_asking), and one question is enough. */
struct FileDialog {
    bool save = false;
    std::string title;
    std::string dir;               /* the folder shown first ("": the system's choice) */
    std::string file;              /* the name offered (a base name) */
    std::string filter;            /* the filter's name */
    std::vector<std::string> exts; /* ".set" ...; none: no filter but All files */
};

/* the platform's dialog, below, on the window's UI thread, owned by window
   (its native window): the path picked, "" when cancelled, nullopt when it
   could not open */
std::optional<std::string> pick_file(void *window, const FileDialog &d);

/* ---- File > Open model and Reload: the protocol's open and reload -------
   The model picked is loaded in this process (W61): the core asks before
   the model it has goes, offering to save its session, in the page. A
   session file (.snapx, W57) picked here opens that session. */
[[maybe_unused]] constexpr std::string_view RELOAD = "{\"cmd\":\"reload\"}";

[[maybe_unused]] void open_model(void *window)
{
    const FileDialog models{false, "Open model", "", "", "XPP models and sessions (*.ode, *.odex, *.snapx)", {".ode", ".odex", ".snapx"}};
    std::optional<std::string> path = pick_file(window, models);
    if (path && !path->empty()) host->open_model(path->c_str());
}

/* ---- the platform's menu bar, icon and dialogs -------------------------- */

enum MenuId { ID_OPEN = 101, ID_RELOAD, ID_QUIT, ID_MANUAL, ID_KEYS, ID_ABOUT };
[[maybe_unused]] const char *const KEYS_CHAPTER = "05-commands"; /* the hotkeys, from its first paragraph */

#if defined(_WIN32)

constexpr bool HAS_FILE_DIALOG = true;

std::wstring wide(const std::string &s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? static_cast<size_t>(n) : 1, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    w.resize(w.size() - 1);
    return w;
}

/* UTF-16 to UTF-8, wide's inverse */
std::string narrow(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? static_cast<size_t>(n) : 1, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    s.resize(s.size() - 1);
    return s;
}

/* a COM interface, released at the end of its scope */
template <class T> struct Com {
    T *p = nullptr;
    Com() = default;
    Com(const Com &) = delete;
    Com &operator=(const Com &) = delete;
    ~Com()
    {
        if (p) p->Release();
    }
    void **out() { return reinterpret_cast<void **>(&p); }
    T *operator->() const { return p; }
};

/* the Common Item Dialog (IFileOpenDialog, IFileSaveDialog), on the
   window's STA thread */
std::optional<std::string> pick_file(void *window, const FileDialog &d)
{
    Com<IFileDialog> dlg;
    if (FAILED(CoCreateInstance(d.save ? __uuidof(FileSaveDialog) : __uuidof(FileOpenDialog), nullptr,
                                CLSCTX_INPROC_SERVER, __uuidof(IFileDialog), dlg.out())))
        return std::nullopt;
    FILEOPENDIALOGOPTIONS opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST;
    if (d.save)
        opts &= ~static_cast<FILEOPENDIALOGOPTIONS>(FOS_OVERWRITEPROMPT); /* the core asks (FileDialog) */
    else
        opts |= FOS_FILEMUSTEXIST;
    dlg->SetOptions(opts);
    if (!d.title.empty()) dlg->SetTitle(wide(d.title).c_str());
    std::wstring name = wide(d.filter), patterns;
    for (const std::string &e : d.exts) patterns += (patterns.empty() ? L"*" : L";*") + wide(e);
    std::vector<COMDLG_FILTERSPEC> types;
    if (!patterns.empty()) types.push_back({name.c_str(), patterns.c_str()});
    types.push_back({L"All files (*.*)", L"*.*"});
    dlg->SetFileTypes(static_cast<UINT>(types.size()), types.data());
    dlg->SetFileTypeIndex(1);
    if (d.save && !d.exts.empty()) dlg->SetDefaultExtension(wide(d.exts[0].substr(1)).c_str());
    if (!d.dir.empty()) {
        std::wstring dir = wide(d.dir);
        std::replace(dir.begin(), dir.end(), L'/', L'\\');
        if (dir.size() > 3 && dir.back() == L'\\') dir.pop_back();
        Com<IShellItem> folder;
        if (SUCCEEDED(SHCreateItemFromParsingName(dir.c_str(), nullptr, __uuidof(IShellItem), folder.out())))
            dlg->SetFolder(folder.p);
    }
    if (!d.file.empty()) dlg->SetFileName(wide(d.file).c_str());
    HRESULT shown = dlg->Show(static_cast<HWND>(window));
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return std::string();
    Com<IShellItem> item;
    PWSTR path = nullptr;
    if (FAILED(shown) || FAILED(dlg->GetResult(&item.p)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return std::nullopt;
    std::string picked = narrow(path);
    CoTaskMemFree(path);
    return picked;
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
        case ID_RELOAD: host->inbox_push(RELOAD.data(), RELOAD.size()); return 0;
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
    AppendMenuW(file, MF_STRING, ID_RELOAD, L"&Reload");
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

/* fit_in on the monitor the window opened on (the library sized it, frame
   and DPI scaling included, at the system's default position) */
void place_window(webview_t w)
{
    HWND hwnd = static_cast<HWND>(webview_get_window(w));
    RECT r;
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    if (!hwnd || !GetWindowRect(hwnd, &r) ||
        !GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return;
    const RECT &a = mi.rcWork;
    Placement p = fit_in(r.right - r.left, r.bottom - r.top, a.left, a.top, a.right - a.left, a.bottom - a.top);
    SetWindowPos(hwnd, nullptr, p.x, p.y, p.width, p.height, SWP_NOZORDER | SWP_NOACTIVATE);
}

#elif defined(__APPLE__)

/* macOS: no menu bar of ours yet, and the icon comes with the .app bundle
   (W13b) */
void add_menus(webview_t) {}
/* the library centres the window, and Cocoa keeps a window on its screen */
void place_window(webview_t) {}

constexpr bool HAS_FILE_DIALOG = true;

/* an Objective-C message from C++, as the webview library sends them */
template <typename R = id, typename... A> R msg(id self, const char *sel, A... a)
{
    return reinterpret_cast<R (*)(id, SEL, A...)>(objc_msgSend)(self, sel_registerName(sel), a...);
}
id cls(const char *name) { return reinterpret_cast<id>(objc_getClass(name)); }
id ns_string(const std::string &s) { return msg(cls("NSString"), "stringWithUTF8String:", s.c_str()); }

/* NSOpenPanel, NSSavePanel, application-modal on the main thread (not
   tested: written without a Mac). An open panel has no type menu of its
   own, so it filters nothing and every file stays pickable (All files);
   a save panel takes the types, the first added to a name without one. */
std::optional<std::string> pick_file(void *, const FileDialog &d)
{
    id panel = d.save ? msg(cls("NSSavePanel"), "savePanel") : msg(cls("NSOpenPanel"), "openPanel");
    if (!panel) return std::nullopt;
    if (!d.title.empty()) {
        msg<void>(panel, "setTitle:", ns_string(d.title));
        msg<void>(panel, "setMessage:", ns_string(d.title));
    }
    if (!d.dir.empty())
        msg<void>(panel, "setDirectoryURL:",
                  msg(cls("NSURL"), "fileURLWithPath:isDirectory:", ns_string(d.dir), static_cast<BOOL>(YES)));
    if (d.save && !d.file.empty()) msg<void>(panel, "setNameFieldStringValue:", ns_string(d.file));
    if (d.save && !d.exts.empty()) {
        id types = msg(cls("NSMutableArray"), "array");
        for (const std::string &e : d.exts) msg<void>(types, "addObject:", ns_string(e.substr(1)));
        msg<void>(panel, "setAllowedFileTypes:", types);
        msg<void>(panel, "setAllowsOtherFileTypes:", static_cast<BOOL>(YES));
    }
    constexpr long MODAL_RESPONSE_OK = 1; /* NSModalResponseOK */
    if (msg<long>(panel, "runModal") != MODAL_RESPONSE_OK) return std::string();
    id url = msg(panel, "URL");
    const char *path = url ? msg<const char *>(msg(url, "path"), "UTF8String") : nullptr;
    if (!path) return std::nullopt;
    return std::string(path);
}

#else /* Linux: GTK 3 (webkit2gtk-4.1) */

#if GTK_MAJOR_VERSION >= 4
void add_menus(webview_t) {} /* webkitgtk-6.0 (GTK 4) has no GtkMenuBar */
void place_window(webview_t) {} /* nor a work area or a window position */
/* nor gtk_dialog_run: web2 shows its own file dialog */
constexpr bool HAS_FILE_DIALOG = false;
std::optional<std::string> pick_file(void *, const FileDialog &) { return std::nullopt; }
#else
constexpr bool HAS_FILE_DIALOG = true;

webview_t view_of_menu()
{
    std::lock_guard<std::mutex> lk(st->mu);
    return st->view;
}

/* a pattern that matches ext in any case (GTK 3's are case-sensitive):
   ".ode" is "*.[oO][dD][eE]" */
std::string any_case(const std::string &ext)
{
    std::string p = "*";
    for (char c : ext) {
        const char lo = g_ascii_tolower(c), up = g_ascii_toupper(c);
        if (lo == up) p += c;
        else p += std::string("[") + lo + up + "]";
    }
    return p;
}

/* GtkFileChooserDialog, on the GTK thread */
std::optional<std::string> pick_file(void *window, const FileDialog &d)
{
    GtkWidget *dlg = gtk_file_chooser_dialog_new(d.title.empty() ? nullptr : d.title.c_str(), GTK_WINDOW(window),
                                                 d.save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
                                                 "_Cancel", GTK_RESPONSE_CANCEL, d.save ? "_Save" : "_Open",
                                                 GTK_RESPONSE_ACCEPT, nullptr);
    if (!dlg) return std::nullopt;
    GtkFileChooser *fc = GTK_FILE_CHOOSER(dlg);
    gtk_file_chooser_set_local_only(fc, TRUE);
    gtk_file_chooser_set_do_overwrite_confirmation(fc, FALSE); /* the core asks (FileDialog) */
    if (!d.exts.empty()) {
        GtkFileFilter *types = gtk_file_filter_new();
        gtk_file_filter_set_name(types, d.filter.c_str());
        for (const std::string &e : d.exts) gtk_file_filter_add_pattern(types, any_case(e).c_str());
        gtk_file_chooser_add_filter(fc, types);
    }
    GtkFileFilter *all = gtk_file_filter_new();
    gtk_file_filter_set_name(all, "All files");
    gtk_file_filter_add_pattern(all, "*");
    gtk_file_chooser_add_filter(fc, all);
    if (!d.dir.empty()) gtk_file_chooser_set_current_folder(fc, d.dir.c_str());
    if (d.save && !d.file.empty()) gtk_file_chooser_set_current_name(fc, d.file.c_str());
    std::optional<std::string> picked = std::string();
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(fc);
        if (path) picked = std::string(path);
        g_free(path);
    }
    gtk_widget_destroy(dlg);
    return picked;
}

void on_menu(GtkMenuItem *, gpointer id_ptr)
{
    webview_t w = view_of_menu();
    if (!w) return;
    GtkWindow *win = GTK_WINDOW(webview_get_window(w));
    switch (GPOINTER_TO_INT(id_ptr)) {
    case ID_OPEN: open_model(win); break;
    case ID_RELOAD: host->inbox_push(RELOAD.data(), RELOAD.size()); break;
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
        host->log(XPP_LOG_WARN, "xppautX: window icon: %s\n", err ? err->message : "unknown error");
    }
    if (err) g_error_free(err);
    g_object_unref(loader);
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
    menu_item(file, "_Reload", ID_RELOAD);
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

/* fit_in on the primary monitor's work area (GTK sizes in logical pixels,
   so the scaling is already in the area); on Wayland the compositor
   chooses the position and ignores the move */
void place_window(webview_t w)
{
    GtkWindow *win = static_cast<GtkWindow *>(webview_get_window(w));
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = display ? gdk_display_get_primary_monitor(display) : nullptr;
    if (!monitor && display) monitor = gdk_display_get_monitor(display, 0);
    if (!win || !monitor) return;
    GdkRectangle a;
    gdk_monitor_get_workarea(monitor, &a);
    int width, height;
    gtk_window_get_size(win, &width, &height);
    Placement p = fit_in(width, height, a.x, a.y, a.width, a.height);
    gtk_window_resize(win, p.width, p.height);
    gtk_window_move(win, p.x, p.y);
}
#endif /* GTK 3 */

#endif /* platform */

/* the page's __xppFileDialog({mode, title, dir, file, wild, exts}), in
   webview's JSON (xpp_webview.h) */
FileDialog file_dialog_of(const std::string &request)
{
    const std::string o = xpp_webview_json_value(request, nullptr, 0);
    FileDialog d;
    d.save = xpp_webview_json_value(o, "mode", 0) == "write";
    d.title = xpp_webview_json_value(o, "title", 0);
    d.dir = xpp_webview_json_value(o, "dir", 0);
    d.file = xpp_webview_json_value(o, "file", 0);
    d.filter = xpp_webview_json_value(o, "wild", 0);
    const std::string exts = xpp_webview_json_value(o, "exts", 0);
    for (int i = 0;; i++) {
        std::string e = xpp_webview_json_value(exts, nullptr, i);
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
        std::optional<std::string> path = pick_file(webview_get_window(w), file_dialog_of(request ? request : ""));
        if (!path) {
            status = 1;
            reply = xpp_webview_json_quote("the file dialog could not open");
        } else if (!path->empty()) {
            reply = xpp_webview_json_quote(*path);
        }
    } catch (const std::exception &e) {
        status = 1;
        reply = "null";
        host->log(XPP_LOG_WARN, "xppautX: the file dialog failed: %s\n", e.what());
    }
    webview_return(w, id, status, reply.c_str());
}

/* the window, on the thread that runs it; NULL when it cannot open, with
   why in st->error_msg */
webview_t open_view()
{
    int code = 0;
    std::array<char, 512> why{};
    webview_t w = xpp_webview_create(0, nullptr, &code, why.data(), why.size());
    if (!w) {
        st->error_msg = xpp_webview_error_message(code, why.data());
        return nullptr;
    }
    std::string t;
    {
        std::lock_guard<std::mutex> lk(st->mu);
        t = title_of(st->model);
    }
    webview_set_title(w, t.c_str());
    webview_set_size(w, WIDTH, HEIGHT, WEBVIEW_HINT_NONE);
    add_menus(w);
    place_window(w);
    /* before the page loads, so it is there from its first script */
    if (HAS_FILE_DIALOG) webview_bind(w, "__xppFileDialog", file_dialog_cb, w);
    /* the token stays out of sight: the web view has no address bar */
    webview_navigate(w, host->http_url());
    return w;
}

/* the warning when the window does not open (why, when webview said) */
const char *no_view_message()
{
    return st->error_msg.empty() ? "xppautX: the window cannot open; using the browser instead\n" : st->error_msg.c_str();
}

#ifdef __APPLE__
void (*session_fn)(void);

void *session_main(void *)
{
    session_fn();
    return nullptr;
}
#endif

/* xpp_window.h's set_model and run: the library's (XppWindowApi) in the
   plugin, else the public functions below */
void set_model(const char *path)
{
    if (!st || !path) return;
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    std::lock_guard<std::mutex> lk(st->mu);
    st->model = base;
    if (st->view) webview_dispatch(st->view, set_title_cb, nullptr);
}

int run(void (*session)(void), const char *about)
{
    try {
        st = new State;
        st->about = about ? about : "";
#ifdef __APPLE__
        /* Cocoa: the window on the main thread, the session on another,
           with the main thread's 8 MB of stack (a new thread gets 512 KB) */
        webview_t w = open_view();
        if (!w) {
            host->log(XPP_LOG_WARN, "%s", no_view_message());
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
            host->log(XPP_LOG_WARN, "%s", no_view_message());
            return 0;
        }
        std::atexit(on_exit);
#endif
    } catch (const std::exception &e) {
        host->log(XPP_LOG_WARN, "xppautX: the window cannot open (%s); using the browser instead\n", e.what());
        return 0;
    }
    session();
    return 1;
}

} /* namespace */

#ifdef XPP_WINDOW_PLUGIN
extern "C" __attribute__((visibility("default"))) int xpp_window_plugin_init(const XppWindowHost *h,
                                                                             XppWindowApi *api)
{
    if (!h || h->version != XPP_WINDOW_HOST_VERSION || !api) return 0;
    host = h;
    api->run = run;
    api->set_model = set_model;
    return 1;
}
#else
int xpp_window_supported(void) { return 1; }
void xpp_window_set_model(const char *path) { set_model(path); }
int xpp_window_run(void (*session)(void), const char *about) { return run(session, about); }
#endif

#endif /* XPP_WINDOW */
