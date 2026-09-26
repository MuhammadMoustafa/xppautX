#ifndef XPP_WINDOW_HINT_H
#define XPP_WINDOW_HINT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What xppautX says when its window library (Linux, W13e:
   xpp_window_loader.cpp) does not load, before it falls back to the
   browser: from dl_error, dlerror()'s text, and os_release, the text of
   /etc/os-release (NULL or "" when there is none). A missing library
   ("libwebkit2gtk-4.1.so.0: cannot open shared object file") becomes the
   command that installs WebKitGTK on that system -- apt, dnf, pacman or
   zypper by its ID and ID_LIKE, else the library's name -- and anything
   else is quoted as it is. Written into out (size bytes, cut to fit),
   one line ending in '\n'. */
void xpp_window_load_message(char *out, size_t size, const char *os_release, const char *dl_error);

#ifdef __cplusplus
}

#include <string>

/* Translate a webview error into user-friendly message (W35e). Only show
   the "install WebView2 runtime" hint for error code -5 (MISSING_DEPENDENCY);
   other errors report the actual code name and webview's message. */
inline std::string xpp_webview_error_message(int code, const char *msg)
{
    /* WEBVIEW_ERROR_MISSING_DEPENDENCY is -5; only show the runtime hint for it */
    if (code == -5) {
        return "xppautX: the window cannot open (no web view: on Windows the WebView2 runtime, on Linux a "
               "display); using the browser instead\n";
    }
    /* For other errors, report the code and the message from the exception */
    const char *code_name;
    switch (code) {
    case 0: code_name = "OK"; break;
    case -1: code_name = "UNSPECIFIED"; break;
    case -2: code_name = "INVALID_ARGUMENT"; break;
    case -3: code_name = "INVALID_STATE"; break;
    case -4: code_name = "CANCELED"; break;
    case 1: code_name = "DUPLICATE"; break;
    case 2: code_name = "NOT_FOUND"; break;
    default: code_name = "unknown"; break;
    }
    std::string actual_msg = msg && msg[0] ? std::string(msg) : "(no details)";
    return "xppautX: the window cannot open (webview " + std::string(code_name) + ": " +
           actual_msg + "); using the browser instead\n";
}

#endif
#endif
