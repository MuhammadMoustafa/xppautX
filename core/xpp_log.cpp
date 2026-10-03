/* See xpp_log.h. */
#include "xpp_log.h"
#include "xpp_files.h"
#include "xpp_mem.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

namespace xpp {

namespace {
XppLogLevel threshold = XPP_LOG_WARN;
bool auto_echo;
/* the LogCapture that keeps this thread's messages (the latest made), if
   any */
thread_local xpp::LogCapture *capture = nullptr;

/* where messages go: --logfile's file when one was given, else stderr
   (stdout is the protocol's in --server mode; the file's default is stdout) */
FILE *sink()
{
    return log_settings.file != nullptr && log_settings.file != stdout ? log_settings.file : stderr;
}

/* Every delivery uses the same sink and flush, including captured text. */
void write_text(std::string_view text) noexcept
{
    FILE *out = sink();
    std::fwrite(text.data(), 1, text.size(), out);
    std::fflush(out);
}

/* exit() while a LogCapture keeps messages (a command-line mistake or
   running out of memory during a load) unwinds no stack: what it keeps
   is written here, as it is, so that no message is lost (the latest
   made's: a load's) */
struct WriteKeptAtExit {
    WriteKeptAtExit() = default;
    WriteKeptAtExit(const WriteKeptAtExit &) = delete;
    WriteKeptAtExit &operator=(const WriteKeptAtExit &) = delete;
    ~WriteKeptAtExit()
    {
        if (capture != nullptr) capture->write(capture->text());
    }
};
const WriteKeptAtExit write_kept_at_exit;

/* a log file of its own is closed (never stdout or stderr) */
void close_log_file()
{
    if (log_settings.file != nullptr && log_settings.file != stdout && log_settings.file != stderr)
        std::fclose(log_settings.file);
}
} // namespace

LogSettings log_settings = {nullptr, 1, 0, 0};

bool log_open_file(std::string_view path)
{
    close_log_file();
    log_settings.file = xpp::files::open_stream(path, "w");
    return log_settings.file != nullptr;
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

LogCapture::~LogCapture()
{
    write(text_);
    capture = outer_;
}

void LogCapture::write(std::string_view text) noexcept
{
    if (!text.empty() && log_enabled(XPP_LOG_WARN)) {
        write_text(text);
    }
    text_.clear();
}

void LogCapture::keep(const char *message) noexcept
{
    try {
        text_ += message;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("a load's messages");
    }
}

void log_note_error() { ++log_settings.errors; }
int log_exit_code() { return log_settings.errors.load() != 0 ? 1 : 0; }
void log_exit() { std::exit(log_exit_code()); }

void log_message(XppLogLevel level, std::string_view fmt, std::format_args args) noexcept
{
    if (!log_enabled(level)) return; /* no formatting for a filtered message */
    std::string message = xpp::vformat(fmt, args);
    if (level == XPP_LOG_ERROR) log_note_error();
    if (capture != nullptr && level <= XPP_LOG_WARN) {
        capture->keep(message.c_str());
        return;
    }
    write_text(message);
}

bool log_auto_enabled() { return auto_echo || threshold >= XPP_LOG_INFO; }

void log_auto_message(std::string_view fmt, std::format_args args) noexcept
{
    if (!log_auto_enabled()) return;
    std::string message = xpp::vformat(fmt, args);
    write_text(message);
}

bool log_parse_arg(std::string_view arg)
{
    if (arg == "--verbose") {
        log_set_threshold(XPP_LOG_INFO);
        return true;
    }
    if (arg == "--debug") {
        log_set_threshold(XPP_LOG_DEBUG);
        return true;
    }
    return false;
}

} // namespace xpp
