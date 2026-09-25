/* See xpp_log.h. */
#include "xpp_log.h"
#include <cstdio>
#include <cstring>

namespace {
XppLogLevel threshold = XPP_LOG_WARN;
int auto_echo;

/* where messages go: -logfile's file when one was given, else stderr
   (stdout is the protocol's in --server mode; the file's default is stdout) */
FILE *sink()
{
    return log_settings.file != nullptr && log_settings.file != stdout ? log_settings.file : stderr;
}
} // namespace

XppLogSettings log_settings = {NULL, 1, 0, 0};

void xpp_log_set_threshold(XppLogLevel level) { threshold = level; }
void xpp_log_set_auto_echo(int on) { auto_echo = on; }

/* printf semantics: the caller writes the newline, so a line can be built
   in pieces */
int xpp_log_enabled(XppLogLevel level)
{
    /* the model's own "@ quiet=1" (log_settings.verbose==0) silences just
       its INFO-level messages, as plintf() did; WARN/ERROR/DEBUG are
       unaffected */
    return level <= threshold && (level != XPP_LOG_INFO || log_settings.verbose);
}

void xpp_log_v(XppLogLevel level, const char *fmt, va_list ap)
{
    FILE *out = sink();
    if (!xpp_log_enabled(level)) return;
    std::vfprintf(out, fmt, ap);
    fflush(out);
}

void xpp_log(XppLogLevel level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xpp_log_v(level, fmt, ap);
    va_end(ap);
}

void xpp_log_auto(const char *fmt, ...)
{
    va_list ap;
    FILE *out = sink();
    if (!auto_echo && threshold < XPP_LOG_INFO) return;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fflush(out);
}

int xpp_log_parse_arg(const char *arg)
{
    if (strcmp(arg, "--verbose") == 0 || strcmp(arg, "-verbose") == 0) {
        xpp_log_set_threshold(XPP_LOG_INFO);
        return 1;
    }
    if (strcmp(arg, "--debug") == 0 || strcmp(arg, "-debug") == 0) {
        xpp_log_set_threshold(XPP_LOG_DEBUG);
        return 1;
    }
    return 0;
}
