#ifndef XPP_INBOX_H
#define XPP_INBOX_H

#include <stddef.h>

/* The protocol's input, read off the core's thread (xpp_inbox.c).

   Reader threads (the HTTP server in browser mode, a stdin reader with
   --server) push whole command lines; the core, which stays on the main
   thread, takes them with xpp_inbox_next(). Lines go into one of two queues
   as a classifier decides: the control queue is for lines that must reach
   the core while it computes (Abort, Close, Quit), the normal queue for
   everything else. With the default classifier every line is normal, so
   the core sees lines in the order they arrived. Every line gets a sequence
   number that grows by one per line, whichever queue it lands in.

   This file and xpp_inbox.c include no core header. */

#define XPP_INBOX_NORMAL 0  /* a classifier result; next(): the normal queue only */
#define XPP_INBOX_CONTROL 1 /* a classifier result; next(): the control queue only */
#define XPP_INBOX_ANY 2     /* next(): the control queue first, then the normal one */
#define XPP_INBOX_ARRIVAL 3 /* next(): both queues, the older line first (sequence order) */

/* One line, without its newline (it must contain none), stored as given:
   the caller strips line ends. Empty lines are kept. Thread-safe; pushes
   from several threads are serialised, so sequence order is queue order. */
void xpp_inbox_push(const char *line, size_t n);

/* cls(line, seq) returns XPP_INBOX_CONTROL or XPP_INBOX_NORMAL. It runs on
   the pushing reader thread, before the line is queued, with no inbox lock
   held that the core waits on (it may set atomics or signal the core), but
   it must not touch core state or call back into the inbox. The pushing
   thread may hold its own locks (xpp_http.c's) while it runs. NULL restores
   the default: everything normal. */
void xpp_inbox_set_classifier(int (*cls)(const char *line, unsigned long seq));

/* The next line from `which` queue (XPP_INBOX_NORMAL, _CONTROL, _ANY or
   _ARRIVAL), waiting at most wait_ms (< 0: block, 0: poll). Returns 1 with
   *line set to a malloc'd string the caller frees (and *seq, when seq is
   not NULL, to its sequence number); 0 when nothing came in time, or when
   input has ended and only the other queue still holds lines; -1 when
   input has ended (xpp_inbox_close) and both queues are empty. */
int xpp_inbox_next(int which, int wait_ms, char **line, unsigned long *seq);

/* End of input: once both queues are drained, next() returns -1. */
void xpp_inbox_close(void);

/* --server: start a thread that reads stdin, pushes its lines (a '\r'
   before the newline is dropped) and closes the inbox at end of input or
   on a read error; an unterminated last line is dropped, as before.
   It never writes to stdout. Returns 0 when the thread cannot start. */
int xpp_inbox_start_stdin(void);

#endif
