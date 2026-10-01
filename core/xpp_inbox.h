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

/* --script FILE: open FILE for script_advance() (below); no thread and
   nothing pushed yet, unlike start_stdin(). A script is one client
   talking to itself in order, so nothing needs to race the core to catch
   an Abort: the core thread pulls one line at a time, only when it is
   ready for it (see core/ui_json.cpp: after a command's idle, and when an
   ask is pending). false when FILE cannot be opened. */
bool start_file(std::string_view path);

/* A script made as it goes, in place of --script's file (-silent's
   built-in script, core/json_silent.cpp): script_advance() pushes what
   next() gives, or closes the inbox when it gives nothing, and asks it
   only then, so each line can depend on what the lines before it did. It
   is never peeked at (script_peek gives nullptr: it holds no recorded
   interruption). next() runs on the core's thread. */
void start_generated(std::function<std::optional<std::string>()> next);

/* Push the file source's next command line (blank lines and lines whose
   first non-blank character is '#' are skipped, a '\r' before the newline
   dropped), or close the inbox at end of file, ending a line with no
   newline too. A no-op once no file is open (start_file was never called,
   or already reached end of file). */
void script_advance();

/* the file line number of the script line pushed last (1-based) */
int script_line();

/* The command line the next script_advance() pushes, without pushing it,
   with its file line number in line_no; nullptr at the end of the file or
   when no file is open. Valid until the next advance or skip. The core
   looks at it to see what follows the line it is about to run (a
   recorded interruption, core/ui_json.cpp). NUL-terminated, for the
   protocol's JSON reader. */
const char *script_peek(int &line_no);

/* Drop the line script_peek() shows: the next advance pushes the one
   after it. */
void script_skip();

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
