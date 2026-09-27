#ifndef XPP_WINDOW_HINT_H
#define XPP_WINDOW_HINT_H

#include <string>
#include <string_view>

/* C++ only: xpp_window_loader.cpp and its unit test use it, and the Linux
   window library (which links no core file) the inline part.

   What xppautX says when its window library (Linux, W13e:
   xpp_window_loader.cpp) does not load, before it falls back to the
   browser: from dl_error, dlerror()'s text, and os_release, the text of
   /etc/os-release (NULL or "" when there is none). A missing library
   ("libwebkit2gtk-4.1.so.0: cannot open shared object file") becomes the
   command that installs WebKitGTK on that system -- apt, dnf, pacman or
   zypper by its ID and ID_LIKE, else the library's name -- and anything
   else is quoted as it is; an empty dl_error is "unknown error". One
   line ending in '\n'. May throw std::bad_alloc. */
std::string xpp_window_load_message(std::string_view os_release, std::string_view dl_error);

/* The warning when the web view cannot start (W35e), from webview's error
   code (webview_error_t) and message: the runtime hint only when webview
   says a dependency is missing (WEBVIEW_ERROR_MISSING_DEPENDENCY, -5),
   otherwise what webview said. Inline: the Linux window library, which
   links no core file, uses it too. */
inline std::string xpp_webview_error_message(int code, const char *msg)
{
    if (code == -5)
        return "xppautX: the window cannot open (no web view: on Windows the WebView2 runtime, on Linux a "
               "display); using the browser instead\n";
    return "xppautX: the window cannot open (webview error " + std::to_string(code) + ": " +
           (msg && *msg ? std::string(msg) : std::string("no details")) + "); using the browser instead\n";
}

#endif
