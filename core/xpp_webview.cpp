/* The vendored webview library, compiled here instead of
   third_party/webview/src/webview.cc (which only includes webview.h), plus
   xpp::webview_create (xpp_webview.h, W35e) and the library's JSON helpers
   for a binding (W88). The Makefile builds this file
   only as webview.o, with the library's standard (WEBVIEW_STD: C++17 with
   libc++, whose C++23 rejects webview), never as a core object. */
#include "webview/webview.h"
#include "xpp_webview.h"

void *xpp::webview_create(bool debug, void *window, int &code, std::string &msg)
{
    try {
        return new webview::webview{debug, window};
    } catch (const webview::exception &e) {
        code = static_cast<int>(e.error().code());
        msg = e.error().message();
        return nullptr;
    } catch (const std::exception &e) {
        code = -1; /* WEBVIEW_ERROR_UNSPECIFIED */
        msg = e.what();
        return nullptr;
    } catch (...) {
        code = -1; /* WEBVIEW_ERROR_UNSPECIFIED */
        msg.clear();
        return nullptr;
    }
}

std::string xpp::webview_json_value(const std::string &json, std::string_view key, int index)
{
    return webview::detail::json_parse(json, std::string(key), index);
}

std::string xpp::webview_json_quote(const std::string &s) { return webview::detail::json_escape(s); }
