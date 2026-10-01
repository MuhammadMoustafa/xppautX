#ifndef XPP_WEBVIEW_H
#define XPP_WEBVIEW_H

#include <string>
#include <string_view>

/* xpp_webview.cpp, the vendored webview library's own object (webview.o),
   built with the library's standard (C++17 with libc++): what the window
   (xpp_window.cpp) asks of it beyond webview's C API, in namespace xpp
   (W109f). */

namespace xpp {

/* webview_create, but saying why it failed (W35e): the web view (a
   webview_t), or nullptr with webview's error code (webview_error_t) in
   code and its message in msg. webview_create itself catches the
   exception and drops both. */
void *webview_create(bool debug, void *window, int &code, std::string &msg);

/* webview's own JSON, the encoding of a binding's request and reply
   (webview_bind, webview_return; W88): the parser and escaper the library's
   bindings use themselves (webview::detail, which the vendored copy pins;
   its public aliases are deprecated), so the window reads and answers the
   page in the library's own terms rather than with a copy of the core's. */
/* the value of `key` in the JSON object `json`, or with key empty its
   element `index`: a string unescaped, anything else as written, "" when
   there is none */
std::string webview_json_value(const std::string &json, std::string_view key, int index);
/* s as a JSON string, quoted and escaped */
std::string webview_json_quote(const std::string &s);

} // namespace xpp
#endif
