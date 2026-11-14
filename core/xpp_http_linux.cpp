/* xpp_http_internal.h's browser opener on Linux (the rest is xpp_http_posix.cpp). */
#include "xpp_http_internal.h"

namespace xpp::http {

const char *browser_opener() { return "xdg-open"; }

} // namespace xpp::http
