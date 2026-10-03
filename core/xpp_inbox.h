#ifndef XPP_INBOX_H
#define XPP_INBOX_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>

/* The protocol's input, read off the core's thread (xpp_inbox.cpp), in
   namespace xpp::inbox (W109f).

   Reader threads (the HTTP server in browser mode, a stdin reader with
   --server) push whole command lines; the core, which stays on the main
   thread, takes them with next(). Lines go into one of two queues as a
   classifier decides: the control queue is for lines that must reach the
   core while it computes (Abort, Close, Quit), the normal queue for
   everything else. With the default classifier every line is normal, so
   the core sees lines in the order they arrived. Every line gets a sequence
   number that grows by one per line, whichever queue it lands in.

   This file and xpp_inbox.cpp include no core header but small APIs
   (xpp_log.h, xpp_io.h, xpp_files.h, xpp_mem.h). */

namespace xpp::inbox {

/* what the classifier makes of a line */
enum class Verdict {
    normal,  /* the normal queue */
    control, /* the control queue */
    drop,    /* discarded, never queued (its sequence number is used up) */
    refuse   /* queued as a normal line, marked refused (next()) */
};

/* which queue next() takes from */
enum class From {
    normal,  /* the normal queue only */
    control, /* the control queue only */
    any,     /* the control queue first, then the normal one */
    arrival  /* both queues, the older line first (sequence order) */
};

/* One line, without its newline (it must contain none), stored as given:
   the caller strips line ends. Empty lines are kept. Thread-safe; pushes
   from several threads are serialised, so sequence order is queue order. */
void push(std::string_view line);

/* cls(line, seq) says where a line goes (Verdict). It runs on the pushing
   reader thread, before the line is queued, with no inbox lock held that
   the core waits on (it may set atomics, log or signal the core), but it
   must not touch core state or call back into the inbox. The pushing
   thread may hold its own locks (xpp_http.cpp's) while it runs. The line
   is NUL-terminated (the protocol's JSON reader takes a C string).
   nullptr restores the default: everything normal. */
void set_classifier(Verdict (*cls)(const char *line, unsigned long seq));

/* End of input: once both queues are drained, next() says Took::end. */
void close();

/* --server: start a thread that reads stdin, pushes its lines (a '\r'
   before the newline is dropped) and closes the inbox at end of input or
   on a read error; an unterminated last line is dropped, as before.
   It never writes to stdout. false when the thread cannot start. */
bool start_stdin();

/* --silent's internal command list (json_silent.cpp). generated_advance
   asks next on the core's thread, after the preceding command or at its
   prompt, so each line can depend on what the previous lines did. */
void start_generated(std::function<std::optional<std::string>()> next);
/* Push its next line, or close the inbox when the list ends. */
void generated_advance();

/* what next() got */
enum class Took {
    line,    /* a line, in `line` */
    nothing, /* nothing in time, or input has ended and only the other
                queue still holds lines */
    end      /* input has ended (close()) and both queues are empty */
};

/* The next line from `which` queue, waiting at most wait_ms (< 0: block,
   0: poll). Took::line with the line in `line`, its sequence number in
   `seq` and in `refused` whether the classifier refused it; `line`,
   `seq` and `refused` are left alone otherwise. Nothing is thrown. */
Took next(From which, int wait_ms, std::string &line, unsigned long &seq, bool &refused);

} // namespace xpp::inbox
#endif
