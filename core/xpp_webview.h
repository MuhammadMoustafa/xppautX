#ifndef XPP_WEBVIEW_H
#define XPP_WEBVIEW_H
#include <stddef.h>
#ifdef __cplusplus
#include <string>
#endif

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

/* webview's own JSON, the encoding of a binding's request and reply
   (webview_bind, webview_return; W88): the parser and escaper the library's
   bindings use themselves (webview::detail, which the vendored copy pins;
   its public aliases are deprecated), so the window reads and answers the
   page in the library's own terms rather than with a copy of the core's. */
/* the value of `key` in the JSON object `json`, or with key empty its
   element `index`: a string unescaped, anything else as written, "" when
   there is none */
std::string xpp_webview_json_value(const std::string &json, const char *key, int index);
/* s as a JSON string, quoted and escaped */
std::string xpp_webview_json_quote(const std::string &s);
#endif
#endif
