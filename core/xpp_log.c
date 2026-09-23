/* See xpp_log.h. */
#include "xpp_log.h"
#include <stdio.h>
#include <string.h>

static XppLogLevel threshold = XPP_LOG_WARN;

void xpp_log_set_threshold(XppLogLevel level) { threshold = level; }
XppLogLevel xpp_log_get_threshold(void) { return threshold; }

/* Reader threads (xpp_http.c, the --server stdin reader) do not log in
   their hot paths (see xpp_log.h); an occasional fprintf from one of them
   racing the main thread's is no worse than any other interleaved writes
   to the same stderr, which is what upstream xppaut already did with
   plintf()/printf() from a single thread. No extra locking here. */
static void emit(FILE *out, const char *fmt, va_list ap)
{
    size_t n = strlen(fmt);
    vfprintf(out, fmt, ap);
    if (n == 0 || fmt[n - 1] != '\n') fputc('\n', out);
    fflush(out);
}

void xpp_log_v(XppLogLevel level, const char *fmt, va_list ap)
{
    if (level > threshold) return;
    emit(stderr, fmt, ap);
}

void xpp_log(XppLogLevel level, const char *fmt, ...)
{
    va_list ap;
    if (level > threshold) return;
    va_start(ap, fmt);
    emit(stderr, fmt, ap);
    va_end(ap);
}

void xpp_log_auto(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
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
