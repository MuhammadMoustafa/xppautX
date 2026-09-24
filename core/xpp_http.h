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
   (xpp_inbox.h). Returns 0 when no port can be opened. */
int xpp_http_start(int port, int open_browser);
int xpp_http_active(void);
void xpp_http_emit(const char *line, size_t len); /* one event, no newline */

#ifdef __cplusplus
}
#endif
#endif
