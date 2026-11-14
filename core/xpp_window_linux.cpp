/* The window's platform primitives (xpp_window_internal.h) on Linux: GTK 3
   (webkit2gtk-4.1): the native file dialog, the close box and icon, the
   pop-out's raise and the window's place. Built into libxppwindow.so (the only
   thing linked against GTK/WebKitGTK, with XPP_ICON_ASSET: the icon's PNG is
   embedded), where it logs through the host's table. */
#include "xpp_window_internal.h"
#include "xpp_assets.h"

#include <string>

#include <gtk/gtk.h>

namespace xpp::window::platform {

bool run_window_ui(void (*)()) { return run_on_ui_thread(); }

#if GTK_MAJOR_VERSION >= 4
void prepare_window(webview_t) {} /* webkitgtk-6.0 (GTK 4): nothing to set up */
void place_window(webview_t) {} /* nor a work area or a window position */
void prepare_popout(webview_t) {} /* nor a close box of its own: a pop-out closes by its page's Pop in */
void raise_window(void *native) { gtk_window_present(GTK_WINDOW(native)); }
/* nor gtk_dialog_run: web2 shows its own file dialog */
bool has_file_dialog() { return false; }
std::optional<std::string> pick_file(void *, const FileDialog &) { return std::nullopt; }
#else
bool has_file_dialog() { return true; }

namespace {

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

} // namespace

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
    gtk_file_chooser_set_do_overwrite_confirmation(fc, TRUE);
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

namespace {

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
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    GError *err = nullptr;
    if (gdk_pixbuf_loader_write(loader, xpp::icon_png.data(), xpp::icon_png.size(), &err) &&
        gdk_pixbuf_loader_close(loader, &err)) {
        GdkPixbuf *pix = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pix) gtk_window_set_icon(win, pix);
    } else {
        host_api()->log(XPP_LOG_WARN, "xppautX: window icon: {}\n", err ? err->message : "unknown error");
    }
    if (err) g_error_free(err);
    g_object_unref(loader);
}

/* the close box: the question first (quit_asking) */
gboolean on_delete(GtkWidget *window, GdkEvent *, gpointer) { return close_asking(window) ? TRUE : FALSE; }

} // namespace

void prepare_window(webview_t w)
{
    GtkWidget *win = static_cast<GtkWidget *>(webview_get_window(w));
    if (!win) return;
    g_signal_connect(win, "delete-event", G_CALLBACK(on_delete), nullptr);
    set_window_icon(GTK_WINDOW(win));
}

/* a pop-out: the main window's icon and close box, no maximizing and no place of ours: the window manager's */
void prepare_popout(webview_t w) { prepare_window(w); }

void raise_window(void *native) { gtk_window_present(GTK_WINDOW(native)); }

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
    gtk_window_maximize(win); /* W230: un-maximizing gives the size above */
}

#endif /* GTK 3 */

} // namespace xpp::window::platform
