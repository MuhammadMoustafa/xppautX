/* One logging module for the whole core, quiet by default (issue: W2).

   Levels, most to least severe:
     XPP_LOG_ERROR  a model that does not parse, a file that cannot be
                    opened, an AUTO failure the user must see
     XPP_LOG_WARN   a real problem that is not fatal: a duplicate name,
                    a CLI usage mistake, a numerical warning
     XPP_LOG_INFO   the startup banner, "All formulas are valid!!",
                    parser statistics ("nvar=2 naux=4 ... NEQ=6"),
                    confirmations
     XPP_LOG_DEBUG  integrator/solver chatter, internal traces

   The default threshold is XPP_LOG_WARN: xppautX and xppaut print nothing
   at all for a clean run. --verbose (xppautX) / -verbose (xppaut) raises
   it to INFO; --debug / -debug raises it to DEBUG. A message at or above
   the threshold (numerically <=) is written to stderr; nothing else is
   done with it -- see the "sinks" note below.

   err_msg() (xpp_ui.h) is the existing, already-pervasive API for a
   message the user must see; its headless default now logs at ERROR
   directly, not through plintf()/INFO.

   plintf() (historically declared in ggets.h/xpp_ui.h) is now a thin
   wrapper: it forwards to xpp_log() at INFO. Callers do not need to
   change; only its one definition (core/xpp_ui.c) does.

   Sinks: stderr only. In --web mode xpp_http.c already redirects both
   stdout and stderr into a pipe that becomes the page's log panel; in
   --server mode a plain stderr write never touches the JSON protocol
   (out_line() in ui_json.c writes to a separate fd). Both front ends
   already turn "whatever this process writes to stderr" into something
   the user sees, so xpp_log() does not need a second, protocol-specific
   sink -- see CLAUDE.md "Logging". A message gated below the threshold
   is simply never written, so it does not reach either front end.

   xpp_log_auto() is the one deliberate exception: AUTO's per-point
   continuation table and its startup/warning lines (autlib1.c wrline()/
   headng_(), gogoauto.c, auto_nox.c, auto_x11.c, autlib2.c, autlib3.c)
   always write to stderr, ignoring the threshold. The AUTO window's
   "Output" panel in the browser is fed from the same captured stream
   (web/xpp-client.js: log() forwards every line to autoLog()), so this
   is what keeps the table visible there even though its content is
   otherwise INFO-level; see CLAUDE.md "Logging" for the full reasoning.
   AUTO's fort.7/fort.8/fort.9 files are separate fprintf(fp7/fp9,...)
   calls elsewhere and are untouched: they are data, not log output. */
#ifndef XPP_LOG_H
#define XPP_LOG_H

#include <stdarg.h>

typedef enum {
    XPP_LOG_ERROR = 0,
    XPP_LOG_WARN  = 1,
    XPP_LOG_INFO  = 2,
    XPP_LOG_DEBUG = 3
} XppLogLevel;

/* Default is XPP_LOG_WARN. */
void xpp_log_set_threshold(XppLogLevel level);
XppLogLevel xpp_log_get_threshold(void);

/* printf-style; a no-op when level is below the threshold. Adds a
   trailing '\n' only if fmt does not already end with one. */
void xpp_log(XppLogLevel level, const char *fmt, ...);
void xpp_log_v(XppLogLevel level, const char *fmt, va_list ap);

/* AUTO's console table and its startup/warning lines: always written,
   never gated by the threshold. See the big comment above. */
void xpp_log_auto(const char *fmt, ...);

/* Recognizes "--verbose"/"--debug" (xppautX) and "-verbose"/"-debug"
   (xppaut); returns 1 and applies the threshold if arg matched, else 0. */
int xpp_log_parse_arg(const char *arg);

#endif
