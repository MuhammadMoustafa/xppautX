#pragma once
/* One GET over HTTPS through the system's own client, the one module for it (W622): WinHTTP on Windows
   (xpp_https_win.cpp), libcurl loaded at run time on Linux (xpp_https_linux.cpp: dlopen, only when something is
   downloaded, so xppautX links no TLS and one binary starts everywhere). No TLS stack of ours is
   vendored and no process is started. Certificates are the system's, always verified.

   A request follows no redirect: the caller (xpp_update.cpp) checks every address before it is asked
   for. The body goes to the chunk callback as it arrives (never kept whole here); the callback is also
   called with no bytes about once a second while nothing arrives, so a stop is seen on a stalled
   connection. A body that stalls for STALL_SECONDS ends the request with an error. C++ only. */
#include <functional>
#include <string>
#include <string_view>

#include "xpp_error.h"

namespace xpp::https {

/* how long a connection may give no data before the request is abandoned (a slow body is never allowed to hang) */
inline constexpr int STALL_SECONDS = 30;
/* how long a connection may take to open */
inline constexpr int CONNECT_SECONDS = 15;

/* the place of an error of this module: what the user was doing (an error from a request names no file) */
inline constexpr std::string_view PLACE = "update download";
/* what the errors of the Linux and macOS clients say failed (Windows' are xpp_win32.cpp's) */
inline constexpr std::string_view WHERE = "https";

struct Reply {
    int status = 0;
    /* the Location of a 3xx answer, exactly as the server gave it ("" for any other answer) */
    std::string location;
};

/* a piece of the 200 answer's body (or an empty one, while nothing arrives); false stops the request */
using Chunk = std::function<bool(std::string_view)>;

/* one request for url (an https address the caller has checked): the answer's status, and for a 200 its body through
   chunk. A request chunk stopped is not an error here: it returns the reply as far as it got. An error says why the
   system's client could not ask (a missing libcurl names the library), what it said, or that the connection stalled. */
Result<Reply> get(std::string_view url, const Chunk &chunk);

} // namespace xpp::https
