#ifndef XPP_WEBVIEW_H
#define XPP_WEBVIEW_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* webview_create, but saying why it failed (W35e): the web view (a
   webview_t), or NULL with webview's error code (webview_error_t) in *code
   and its message in msg (msg_size bytes). webview_create itself catches
   the exception and drops both. */
void *xpp_webview_create(int debug, void *window, int *code, char *msg, size_t msg_size);

#ifdef __cplusplus
}
#endif
#endif
