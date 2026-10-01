/* See xpp_log.h. */
#include "xpp_log.h"
#include "xpp_files.h"
#include "xpp_mem.h"
#include <cstdio>
#include <new>
#include <string>

namespace xpp {

namespace {
XppLogLevel threshold = XPP_LOG_WARN;
bool auto_echo;
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

LogSettings log_settings = {nullptr, 1, 0, 0};

void log_open_file(std::string_view path)
{
    close_log_file();
    log_settings.file = xpp::files::open_stream(path, "w");
}

void log_new_model()
{
    if (!log_settings.file_from_command_line) {
        close_log_file();
        log_settings.file = stdout;
    }
    if (!log_settings.quiet_from_command_line) log_settings.verbose = 1;
}

void log_set_threshold(XppLogLevel level) { threshold = level; }
void log_set_auto_echo(bool on) { auto_echo = on; }

/* printf semantics: the caller writes the newline, so a line can be built
   in pieces */
bool log_enabled(XppLogLevel level)
{
    /* the model's own "@ quiet=1" (log_settings.verbose==0) silences just
       its INFO-level messages, as plintf() did; WARN/ERROR/DEBUG are
       unaffected */
    return level <= threshold && (level != XPP_LOG_INFO || log_settings.verbose);
}

LogCapture::LogCapture() : outer_(capture)
{
    capture = this;
}

LogCapture::~LogCapture() { capture = outer_; }

void LogCapture::keep(const char *message) noexcept
{
    try {
        text_ += message;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("a load's messages");
    }
}

void log_vprintf(XppLogLevel level, const char *fmt, va_list ap)
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
            xpp::out_of_memory("a log message");
        }
        std::vsnprintf(message.data(), message.size() + 1, fmt, ap);
        capture->keep(message.c_str());
        if (!log_enabled(level)) return;
        std::fputs(message.c_str(), out);
        fflush(out);
        return;
    }
    if (!log_enabled(level)) return;
    std::vfprintf(out, fmt, ap);
    fflush(out);
}

void log_printf(XppLogLevel level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_vprintf(level, fmt, ap);
    va_end(ap);
}

bool log_auto_enabled() { return auto_echo || threshold >= XPP_LOG_INFO; }

void log_auto_printf(const char *fmt, ...)
{
    va_list ap;
    FILE *out = sink();
    if (!log_auto_enabled()) return;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fflush(out);
}

bool log_parse_arg(std::string_view arg)
{
    if (arg == "--verbose" || arg == "-verbose") {
        log_set_threshold(XPP_LOG_INFO);
        return true;
    }
    if (arg == "--debug" || arg == "-debug") {
        log_set_threshold(XPP_LOG_DEBUG);
        return true;
    }
    return false;
}

} // namespace xpp
