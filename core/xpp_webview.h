#ifndef XPP_WEBVIEW_H
#define XPP_WEBVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/* Glue for the vendored webview library (W35e): creates a webview with
   detailed error information. Unlike webview_create (which returns NULL
   and loses the error), this captures the webview::exception's code and
   message when construction fails, allowing xpp_window.cpp to report the
   actual error instead of a generic "WebView2 runtime missing" message. */

typedef void *webview_t;

webview_t xpp_webview_create(int debug, void *window, int *code,
                             char *msg, size_t msg_size);

#ifdef __cplusplus
}
#endif
#endif
