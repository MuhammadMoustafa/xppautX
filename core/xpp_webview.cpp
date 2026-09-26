/* Glue for the vendored webview library (W35e): the implementation of
   xpp_webview_create, which wraps webview::webview with exception handling.
   This replaces third_party/webview/src/webview.cc as the source of webview.o
   in all builds, ensuring consistent C++17 compilation across platforms
   (macOS and Windows clang need C++17 for libc++ compatibility).
   The glue file catches webview::exception to extract detailed error
   information (code, message) when window creation fails, preventing the
   loss of error details that the C API webview_create normally discards.
   WEBVIEW_STATIC is defined in compiler flags (-DWEBVIEW_STATIC). */
#include "webview/webview.h"
#include "xpp_webview.h"

webview_t xpp_webview_create(int debug, void *window, int *code,
                             char *msg, size_t msg_size)
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
