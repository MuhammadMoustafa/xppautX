/* See xpp_log.h. */
#include "xpp_log.h"
#include "xpp_files.h"
#include "xpp_mem.h"
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

namespace {
XppLogLevel threshold = XPP_LOG_WARN;
int auto_echo;
/* the LogCapture that keeps this thread's messages (the latest made), if
   any */
thread_local xpp::LogCapture *capture = nullptr;

/* where messages go: -logfile's file when one was given, else stderr
   (stdout is the protocol's in --server mode; the file's default is stdout) */
FILE *sink()
{
    return log_settings.file != nullptr && log_settings.file != stdout ? log_settings.file : stderr;
}

/* a log file of its own is closed (never stdout or stderr) */
void close_log_file()
{
    if (log_settings.file != nullptr && log_settings.file != stdout && log_settings.file != stderr)
        std::fclose(log_settings.file);
}
} // namespace

XppLogSettings log_settings = {NULL, 1, 0, 0};

void xpp_log_open_file(const char *path)
{
    close_log_file();
    log_settings.file = xpp_files_open_stream(path, "w");
}

void xpp_log_new_model(void)
{
    if (!log_settings.file_from_command_line) {
        close_log_file();
        log_settings.file = stdout;
    }
    if (!log_settings.quiet_from_command_line) log_settings.verbose = 1;
}

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

xpp::LogCapture::LogCapture() : outer_(capture)
{
    capture = this;
}

xpp::LogCapture::~LogCapture() { capture = outer_; }

void xpp::LogCapture::keep(const char *message) noexcept
{
    try {
        text_ += message;
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("a load's messages");
    }
}

void xpp_log_v(XppLogLevel level, const char *fmt, va_list ap)
{
    FILE *out = sink();
    if (capture != nullptr && level <= XPP_LOG_WARN) {
        /* the message once as text, for the capture and the log */
        va_list again;
        va_copy(again, ap);
        const int n = std::vsnprintf(nullptr, 0, fmt, again);
        va_end(again);
        if (n < 0) return;
        std::string message;
        try {
            message.resize(static_cast<size_t>(n));
        } catch (const std::bad_alloc &) {
            xpp_out_of_memory("a log message");
        }
        std::vsnprintf(message.data(), message.size() + 1, fmt, ap);
        capture->keep(message.c_str());
        if (!xpp_log_enabled(level)) return;
        std::fputs(message.c_str(), out);
        fflush(out);
        return;
    }
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

int xpp_log_auto_enabled(void) { return auto_echo || threshold >= XPP_LOG_INFO; }

void xpp_log_auto(const char *fmt, ...)
{
    va_list ap;
    FILE *out = sink();
    if (!xpp_log_auto_enabled()) return;
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
