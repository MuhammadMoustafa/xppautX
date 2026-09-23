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

   The default threshold is XPP_LOG_WARN, so a clean run prints nothing;
   --verbose / -verbose raises it to INFO, --debug / -debug to DEBUG.
   plintf() is xpp_log at INFO; err_msg()'s headless default logs at ERROR.

   Messages follow printf: the caller writes the newline, so a line can be
   built in pieces. They go to -logfile's file when one was given, else to
   stderr. That is enough for both front ends: browser mode turns stderr
   into the page's log panel, and --server mode writes its protocol to a
   separate descriptor.

   xpp_log_auto() is AUTO's console table and its notes. The console gets
   it at INFO, like the rest; in browser mode it is always written
   (xpp_log_set_auto_echo), because the AUTO window's Output panel is fed
   from the log. AUTO's fort.7/8/9 files are data, written elsewhere. */
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
/* 1: AUTO's table is written whatever the threshold (browser mode) */
void xpp_log_set_auto_echo(int on);

/* Recognizes "--verbose"/"--debug" (xppautX) and "-verbose"/"-debug"
   (xppaut); returns 1 and applies the threshold if arg matched, else 0. */
int xpp_log_parse_arg(const char *arg);

#endif
