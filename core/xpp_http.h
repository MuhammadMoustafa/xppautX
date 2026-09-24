#ifndef XPP_HTTP_H
#define XPP_HTTP_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* The browser front end without Node: a small HTTP server inside
   xppautX (xpp_http.cpp). The page and its script are compiled in
   (web_assets.c); events reach the page by Server-Sent Events and commands
   come back by POST. Only 127.0.0.1 is served, and the
   event and command URLs need the random token printed with the address.

   xpp_http_start redirects stdout and stderr (what xppaut prints) into the
   page's log as well as the terminal, and after it the protocol goes to
   the page instead of stdin/stdout; the page's commands go into the inbox
   (xpp_inbox.h). Returns 0 when no port can be opened.

   flags: XPP_HTTP_SHOW prints the address (the "XPP: http://..." line
   tools and the VS Code extension read) and XPP_HTTP_OPEN opens it in the
   default browser; the desktop window (xpp_window.h) passes neither and
   navigates to xpp_http_url() itself, so the token appears nowhere. */
#define XPP_HTTP_SHOW 1
#define XPP_HTTP_OPEN 2
int xpp_http_start(int port, int flags);
/* browser mode after the fact (the window could not open): print the
   address, and open it in the default browser when `open` is set */
void xpp_http_show(int open);
/* the page's address with its token; "" before xpp_http_start */
const char *xpp_http_url(void);
/* whether the core said bye (a Quit) before exiting, as opposed to
   stopping on an error: after an error the server keeps serving, so the
   page can show what xppaut printed, until xpp_http_release() or Ctrl+C */
int xpp_http_said_bye(void);
/* stop that wait (the window showing the page was closed); may come
   before the exit, which then does not wait at all. Any thread. */
void xpp_http_release(void);
int xpp_http_active(void);
void xpp_http_emit(const char *line, size_t len); /* one event, no newline */

#ifdef __cplusplus
}
#endif
#endif
