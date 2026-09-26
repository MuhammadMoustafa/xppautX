/* The vendored webview library, compiled here instead of
   third_party/webview/src/webview.cc (which only includes webview.h), plus
   xpp_webview_create (xpp_webview.h, W35e). The Makefile builds this file
   only as webview.o, with the library's standard (WEBVIEW_STD: C++17 with
   libc++, whose C++23 rejects webview), never as a core object. */
#include "webview/webview.h"
#include "xpp_webview.h"

void *xpp_webview_create(int debug, void *window, int *code, char *msg, size_t msg_size)
{
    if (!code || !msg || msg_size == 0) return nullptr;

    try {
        return new webview::webview{static_cast<bool>(debug), window};
    } catch (const webview::exception &e) {
        *code = static_cast<int>(e.error().code());
        const auto &error_msg = e.error().message();
        size_t n = error_msg.copy(msg, msg_size - 1);
        msg[n] = '\0';
        return nullptr;
    } catch (const std::exception &e) {
        *code = -1;  /* WEBVIEW_ERROR_UNSPECIFIED */
        const char *what = e.what();
        size_t n = what ? std::string(what).copy(msg, msg_size - 1) : 0;
        msg[n] = '\0';
        return nullptr;
    } catch (...) {
        *code = -1;  /* WEBVIEW_ERROR_UNSPECIFIED */
        msg[0] = '\0';
        return nullptr;
    }
}
