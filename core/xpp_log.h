/* One logging module for the whole core, quiet by default (issue: W2).

   Levels, most to least severe:
     XPP_LOG_ERROR  a model that does not parse, a file that cannot be
                    opened, an autoX failure the user must see
     XPP_LOG_WARN   a real problem that is not fatal: a duplicate name,
                    a CLI usage mistake, a numerical warning
     XPP_LOG_INFO   the startup banner, "All formulas are valid!!",
                    parser statistics ("nvar=2 naux=4 ... NEQ=6"),
                    confirmations
     XPP_LOG_DEBUG  integrator/solver chatter, internal traces

   The default threshold is XPP_LOG_WARN, so a clean run prints nothing;
   --verbose / --verbose raises it to INFO, --debug / --debug to DEBUG.
   err_msg()'s headless default logs at ERROR. There is no plintf() any
   more (retired at W25): call xpp::log(level, fmt, args...) (a
   std::format-checked call, below). Dynamic widths and precisions use
   nested replacement fields (`{:{}}`, `{:.{}}`). What the log shows and where
   it goes are the user's, not a model's (W619): the preferences' log_level and
   log_file, or --verbose, --debug, --quiet and --logfile.

   Messages follow printf: the caller writes the newline, so a line can be
   built in pieces. They go to --logfile's file when one was given, else to
   stderr. That is enough for both front ends: browser mode turns stderr
   into the page's log panel, and --server mode writes its protocol to a
   separate descriptor. With a page, the terminal gets only what the page
   cannot show (log_set_terminal below): WARN and ERROR, and everything
   at --verbose or --debug.

   xpp::log_autox() is autoX's console table and its notes. The console gets
   it at INFO, like the rest; in browser mode it is always written
   (xpp::log_set_autox_echo), because the autoX window's Output panel is fed
   from the log. autoX's fort.7/8/9 files are data, written elsewhere.

   C++ linkage in namespace xpp (W109a); the level stays the plain enum
   XppLogLevel, which the window library's C table names too
   (xpp_window_plugin.h). */
#pragma once

#include <array>
#include <atomic>
#include <cstdio> /* first, so MinGW's libstdc++ picks its C99 printf */
#include <string>
#include <string_view>

#include "xpp_io.h" /* xpp::vformat */

typedef enum {
    XPP_LOG_ERROR = 0,
    XPP_LOG_WARN  = 1,
    XPP_LOG_INFO  = 2,
    XPP_LOG_DEBUG = 3
} XppLogLevel;

namespace xpp {

/* the words of the levels, as the preferences' log_level names them (in the enum's order) */
inline constexpr std::array<std::string_view, 4> log_level_names{"error", "warn", "info", "debug"};

/* Where the log goes. */
struct LogSettings {
    FILE *file;                  /* the log file (preferences, --logfile); NULL or stdout: stderr */
    FILE *terminal;              /* the terminal's copy while a page takes the log (log_set_terminal); NULL: none */
    std::atomic<unsigned> errors{0}; /* errors reported, even when output is suppressed */
};
extern LogSettings log_settings;

/* One accounting path for errors logged or sent through the UI. */
void log_note_error();
int log_exit_code();
[[noreturn]] void log_exit();

/* The log goes to the file path from now on: the one it
   went to before is closed, unless that was stdout or stderr. An empty path
   is stderr again. */
bool log_open_file(std::string_view path);

/* Default is XPP_LOG_WARN. */
void log_set_threshold(XppLogLevel level);
/* true when a message at level would be printed now (the threshold) */
bool log_enabled(XppLogLevel level);

/* true when log_autox() writes (browser mode, or a threshold of INFO
   or more) */
bool log_autox_enabled();
/* true: autoX's table is written whatever the threshold (browser mode) */
void log_set_autox_echo(bool on);

/* With a page taking the log (browser and window modes: stderr is its
   pipe), `terminal` is the terminal's own copy of stderr. A WARN or ERROR
   message is written to it as well, since the page may be closed or have
   crashed, and so is every message at --verbose or --debug. NULL (the
   default, and in every mode with no page): the log is written once, to
   its sink. */
void log_set_terminal(FILE *terminal);

/* While one lives, the ERROR and WARN messages logged on the thread that
   made it are kept here instead of written: a load (session.h,
   xpp::Load) writes them with the place in the model they are about
   (write), or takes those about the line that failed as its error's
   what. The latest made keeps them; the one before again once it goes.
   What it still keeps when it goes, or when the program exits, is
   written as it is. */
class LogCapture {
public:
    LogCapture();
    ~LogCapture();
    LogCapture(const LogCapture &) = delete;
    LogCapture &operator=(const LogCapture &) = delete;
    /* what was kept since it was made or last cleared */
    const std::string &text() const noexcept { return text_; }
    void clear() noexcept { text_.clear(); }
    /* text (what was kept, as its owner renders it) written where the
       log goes, and what was kept forgotten */
    void write(std::string_view text) noexcept;
    /* an ERROR or WARN message logged */
    void keep(const char *message) noexcept;

private:
    std::string text_;
    LogCapture *outer_;
};

/* While one lives, what this thread logs with log_autox() (autoX's table and its notes) is kept in `table` instead of
   written, and what it logs at WARN (a branch's stop, a note) in `warnings`: a job (autox_jobs.cpp) keeps its lines,
   which the join writes in the jobs' order, numbered as the diagram is, so the Output reads as the runs in turn. A sweep's run
   (integrate.cpp, W434) keeps only its warnings: it gives a table it never reads. */
class LogHold {
public:
    LogHold(std::string &table, std::string &warnings);
    ~LogHold();
    LogHold(const LogHold &) = delete;
    LogHold &operator=(const LogHold &) = delete;

private:
    std::string *outer_table_, *outer_warnings_;
};

/* The checked calls below and the window library's callback share this
   formatting and delivery path. The caller writes the newline. */
void log_message(XppLogLevel level, std::string_view fmt, std::format_args args) noexcept;
void log_autox_message(std::string_view fmt, std::format_args args) noexcept;

/* Compile-time checked logging (see core/xpp_io.h's xpp::format, same
   idea): a bad "{}" against the argument types is a compile error, not a
   run-time surprise. A formatting failure is loud and final, like
   xpp::format_failed. */
template <class... Args>
void log(XppLogLevel level, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    xpp::with_format_args([&fmt, level](std::format_args a) { log_message(level, fmt.get(), a); },
                          std::forward<Args>(args)...);
}

/* the same checked call for autoX's table and notes */
template <class... Args>
void log_autox(std::format_string<Args...> fmt, Args &&...args) noexcept
{
    xpp::with_format_args([&fmt](std::format_args a) { log_autox_message(fmt.get(), a); },
                          std::forward<Args>(args)...);
}
} // namespace xpp

