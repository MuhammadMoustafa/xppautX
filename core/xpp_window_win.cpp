/* The window's platform primitives (xpp_window_internal.h) on Windows: the
   native file dialog (the Common Item Dialog), the close box and icon (a window
   procedure over the library's), the pop-out's raise and the window's place.
   The one window file that includes <windows.h> (with xpp_win.cpp and the
   other *_win.cpp files): its macros clash with core names such as max,
   MessageBox and VARTYPE, so it includes no core header but xpp_window_internal.h. */
#include "xpp_window_internal.h"
#include "xpp_window.h"

#include <algorithm>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#include <commctrl.h>

namespace xpp::window {

const char *launch_document() { return nullptr; } /* only macOS's Finder opens a document with the app */

} // namespace xpp::window

namespace xpp::window::platform {

namespace {

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

} // namespace

bool has_file_dialog() { return true; }

bool run_window_ui(void (*)()) { return run_on_ui_thread(); }

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
        opts |= FOS_OVERWRITEPROMPT;
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

namespace {

WNDPROC webview_proc; /* the library's own window procedure */

LRESULT CALLBACK close_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CLOSE && close_asking(hwnd)) return 0;
    return CallWindowProcW(webview_proc, hwnd, msg, wp, lp);
}

} // namespace

void prepare_window(webview_t w)
{
    HWND hwnd = static_cast<HWND>(webview_get_window(w));
    if (!hwnd) return;
    /* the icon: resource 32512 (IDI_APPLICATION's number) in assets/xppautx.rc,
       which the library already set as the class's large icon */
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HANDLE icon = LoadImageW(inst, MAKEINTRESOURCEW(32512), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                             GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (icon) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));

    webview_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(close_proc)));
}

/* a pop-out: the same close box and icon as the main window's (webview_proc is the library's one window
   procedure, already kept by prepare_window), no maximizing and no place of ours: the system's */
void prepare_popout(webview_t w)
{
    HWND hwnd = static_cast<HWND>(webview_get_window(w));
    if (!hwnd) return;
    HANDLE icon = LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(32512), IMAGE_ICON,
                             GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (icon) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(close_proc));
}

/* a pop-out brought to the front, restored when minimized */
void raise_window(void *native)
{
    HWND hwnd = static_cast<HWND>(native);
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
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
    ShowWindow(hwnd, SW_MAXIMIZE); /* W230: title bar and taskbar stay */
}


} // namespace xpp::window::platform
