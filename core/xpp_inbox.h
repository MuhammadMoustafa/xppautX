#pragma once

#include <cstddef>
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

/* How many normal lines wait for the core, in the queue and kept by the
   command loop (set_kept) together, and their bytes (W389): a client that
   floods the input while the core computes is refused with busy at the
   bound, never queued without end. The count (1024) is far more than a
   client that waits for each command's idle sends at once; the bytes
   (64 MB) hold the largest ordinary line, a pixel answer of a 4K picture
   (about 33 MB as base64), and are also the longest line any input takes. */
constexpr size_t MAX_PENDING_LINES = 1024;
constexpr size_t MAX_PENDING_BYTES = size_t(64) << 20;
/* what the busy error begins with: a --server client tells a refusal by it (HTTP by its 503) */
constexpr const char *BUSY = "Busy";

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

/* The core's command loop tells how many lines (and bytes) it keeps for
   after the command that runs, since they count against the bound. */
void set_kept(size_t lines, size_t bytes);

/* One line, without its newline (it must contain none), stored as given:
   the caller strips line ends. Empty lines are kept. Thread-safe; pushes
   from several threads are serialised, so sequence order is queue order.
   False when the line was refused: a normal line while MAX_PENDING_* lines
   wait. Control lines (the classifier's) are never refused, and nothing
   here blocks, so a Stop sent after a flood is still read. A refused line
   is answered by the busy hook, into *reply if given. */
bool push(std::string_view line, std::string *reply = nullptr);
/* A line the program itself sends (a recording's step, a model the OS
   asks to open): as push, but never refused, since it is one at a time
   and a refusal would lose it. */
void push_own(std::string_view line);
/* Native UI only: a single-use grant for the path actually selected by the
   OS save picker, bound to the prompt id. JSON replies cannot mint it. */
void authorize_save(std::string_view prompt, std::string_view path);
bool consume_save(int ask, std::string_view path);

/* cls(line, seq) says where a line goes (Verdict). It runs on the pushing
   reader thread, before the line is queued, with no inbox lock held that
   the core waits on (it may set atomics, log or signal the core), but it
   must not touch core state or call back into the inbox. The pushing
   thread may hold its own locks (xpp_http.cpp's) while it runs. The line
   is NUL-terminated (the protocol's JSON reader takes a C string).
   nullptr restores the default: everything normal.
   busy(line, reply) is what a push answers when the normal queue is full:
   the client's error for `line`, as one event line, stored in *reply when
   the caller wants it (HTTP's 503 body), else sent to the client. It runs
   on the pushing thread with no inbox lock held, and must not touch core
   state. */
void set_hooks(Verdict (*cls)(const char *line, unsigned long seq), void (*busy)(const char *line, std::string *reply));

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
