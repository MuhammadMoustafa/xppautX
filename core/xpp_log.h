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
   err_msg()'s headless default logs at ERROR. There is no plintf() any
   more (retired at W25): call xpp_log(level, fmt, ...) directly from a
   .c file, or xpp::log(level, fmt, args...) (below, .cpp files, a
   std::format-checked wrapper) where the format string converts
   mechanically. An INFO message additionally honours the model's own
   "@ quiet=1" (log_settings.verbose), same as plintf() used to.

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
#ifdef __cplusplus
#include <cstdio> /* first, so MinGW's libstdc++ picks its C99 printf */
#else
#include <stdio.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    XPP_LOG_ERROR = 0,
    XPP_LOG_WARN  = 1,
    XPP_LOG_INFO  = 2,
    XPP_LOG_DEBUG = 3
} XppLogLevel;

/* Where the log goes and whether the model may silence it. The model's
   @ logfile= and @ quiet= options set file and verbose unless the command
   line's -logfile / -quiet did first (they win over .xpprc and the model). */
typedef struct {
    FILE *file;                  /* -logfile's file; NULL or stdout: stderr */
    int verbose;                 /* 0: an INFO message prints nothing (@ quiet=1) */
    int quiet_from_command_line; /* -quiet was given: @ quiet= is ignored */
    int file_from_command_line;  /* -logfile was given: @ logfile= is ignored */
} XppLogSettings;
extern XppLogSettings log_settings;

/* The log goes to the file path from now on (@ logfile=): the one it
   went to before is closed, unless that was stdout or stderr. */
void xpp_log_open_file(const char *path);

/* Default is XPP_LOG_WARN. */
void xpp_log_set_threshold(XppLogLevel level);
/* 1 when a message at level would be printed now (the threshold, and
   "@ quiet=1" for INFO) */
int xpp_log_enabled(XppLogLevel level);

/* printf-style; a no-op when !xpp_log_enabled(level). The caller writes
   the newline. */
void xpp_log(XppLogLevel level, const char *fmt, ...);
void xpp_log_v(XppLogLevel level, const char *fmt, va_list ap);

/* AUTO's console table and its startup/warning lines: always written,
   never gated by the threshold. See the big comment above. */
void xpp_log_auto(const char *fmt, ...);
/* 1 when xpp_log_auto() writes (browser mode, or a threshold of INFO or
   more) */
int xpp_log_auto_enabled(void);
/* 1: AUTO's table is written whatever the threshold (browser mode) */
void xpp_log_set_auto_echo(int on);

/* Recognizes "--verbose"/"--debug" (xppautX) and "-verbose"/"-debug"
   (xppaut); returns 1 and applies the threshold if arg matched, else 0. */
int xpp_log_parse_arg(const char *arg);

#ifdef __cplusplus
} /* extern "C" */

#include <string>
#include "xpp_io.h" /* xpp::vformat */
#ifdef XPP_IO_HAVE_STD_FORMAT
#define XPP_LOG_HAVE_STD_FORMAT 1
#endif

namespace xpp {
/* While one lives, the ERROR and WARN messages logged on the thread that
   made it are also kept here, as well as written as ever: a load's
   diagnostic (session.h, xpp::Load) takes what was logged about the line
   that failed as its cause. The latest made keeps them; the one before
   again once it goes. */
class LogCapture {
public:
    LogCapture();
    ~LogCapture();
    LogCapture(const LogCapture &) = delete;
    LogCapture &operator=(const LogCapture &) = delete;
    /* what was kept since it was made or last cleared */
    const std::string &text() const noexcept { return text_; }
    void clear() noexcept { text_.clear(); }
    /* an ERROR or WARN message logged (xpp_log_v) */
    void keep(const char *message) noexcept;

private:
    std::string text_;
    LogCapture *outer_;
};

#ifdef XPP_LOG_HAVE_STD_FORMAT
/* Compile-time checked counterpart of xpp_log() for the .cpp files (see
   core/xpp_io.h's xpp::format, same idea): a bad "{}" against the
   argument types is a compile error, not a run-time surprise. Formats
   with std::format, then calls the C xpp_log() with "%s" so the sink,
   threshold and log_settings.verbose gating stay in the one place. No
   exception may cross into C: a formatting failure (out of memory) is
   loud and final, like xpp::format_failed. Prefer this over plain
   xpp_log() in .cpp files whenever the format string converts
   mechanically (most do); a dynamic width/precision (`%*s`, `%.*s`) or a
   pointer destination stay on plain xpp_log. */
template <class... Args>
void log(XppLogLevel level, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    if (!xpp_log_enabled(level)) return; /* no formatting for a filtered message */
    /* xpp::vformat (xpp_io.h): the formatting compiled once, not here */
    std::string s = xpp::vformat(fmt.get(), std::make_format_args(args...));
    xpp_log(level, "%s", s.c_str());
}

/* the same for AUTO's table and notes: xpp_log_auto() with a checked
   format */
template <class... Args>
void log_auto(std::format_string<Args...> fmt, Args &&...args) noexcept
{
    if (!xpp_log_auto_enabled()) return;
    std::string s = xpp::vformat(fmt.get(), std::make_format_args(args...));
    xpp_log_auto("%s", s.c_str());
}
#endif
} // namespace xpp

#endif /* __cplusplus */
#endif
