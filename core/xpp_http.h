#ifndef XPP_HTTP_H
#define XPP_HTTP_H

#include <string_view>
#include "xpp_error.h"

/* The browser front end without Node: a small HTTP server inside
   xppautX (xpp_http.cpp), in namespace xpp::http (W109f). The page and
   its script are compiled in (web_assets.cpp); events reach the page by
   Server-Sent Events and commands come back by POST. Only 127.0.0.1 is
   served, and the event and command URLs need the random token printed
   with the address. */

namespace xpp::http {

constexpr int DEFAULT_PORT = 8765; /* stable default address for local browser sessions */

/* Serve on `port` (another free one when it is taken). This redirects
   stdout and stderr (what xppaut prints) into the page's log as well as
   the terminal, and after it the protocol goes to the page instead of
   stdin/stdout; the page's commands go into the inbox (xpp_inbox.h).
   false when no port can be opened.

   show prints the address (the "XPP: http://..." line tools and the VS
   Code extension read) and open opens it in the default browser; the
   desktop window (xpp_window.h) asks for neither and navigates to url()
   itself, so the token appears nowhere. */
bool start(int port, bool show, bool open);
/* browser mode after the fact (the window could not open): print the
   address, and open it in the default browser when `open` is set */
void show(bool open);
/* the page's address with its token; "" before start. NUL-terminated:
   the window hands it to the web view's C API. */
const char *url();
/* whether the core said bye (a Quit) before exiting, as opposed to
   stopping on an error: after an error the server keeps serving, so the
   page can show what xppaut printed, until release() or Ctrl+C */
bool said_bye();
/* stop that wait (the window showing the page was closed); may come
   before the exit, which then does not wait at all. Any thread. */
void release();
/* true once start() has succeeded: the protocol goes to the page */
bool active();
/* W13c: only this repository's release pages, never an arbitrary opener argument. */
xpp::Result<> open_release_page(std::string_view url);
/* one event, no newline */
void emit(std::string_view line);

} // namespace xpp::http
#endif
