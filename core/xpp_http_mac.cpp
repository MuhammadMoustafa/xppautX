/* xpp_http_internal.h's browser opener on macOS (the rest is xpp_http_posix.cpp). */
#include "xpp_http_internal.h"

namespace xpp::http {

const char *browser_opener() { return "open"; }

} // namespace xpp::http
