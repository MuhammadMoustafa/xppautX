/* The protocol's input queues (xpp_inbox.h).

   The core runs on the main thread and never reads a file descriptor or a
   socket for protocol input: reader threads push lines here and the core
   takes them with xpp_inbox_next(). That keeps input moving while the core
   computes, so a later classifier can pick out Abort or Quit at once.

   Two locks: push_lock serialises pushers (sequence number, classifier,
   enqueue happen as one step, so sequence order is queue order) and is
   never taken by the core; lock guards the queues and is held only briefly.
   Waiting is on a condition variable, never a polling loop. This file
   includes no core header but xpp_mem.h.

   C++ with a C API (xpp_inbox.h is extern "C"). The lines handed out are
   xpp_malloc'd C strings the caller frees with xpp_free; nothing here
   throws into C (xpp_mem.h: an allocation that fails ends the program). */
#include "xpp_inbox.h"
#include "xpp_mem.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#ifdef _WIN32
#include "xpp_win32.h"
#else
#include <unistd.h>
#endif

namespace {

struct Item {
    Item *next;
    unsigned long seq;
    char *line;
};

struct Queue {
    Item *head, *tail;
};

pthread_mutex_t push_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
Queue queues[2]; /* [XPP_INBOX_NORMAL], [XPP_INBOX_CONTROL] */
int closed;
unsigned long next_seq = 1;
int (*classify)(const char *line, unsigned long seq);


/* the queue to take from now, or -1; call with the lock held */
int pick(int which)
{
    if (which == XPP_INBOX_ARRIVAL) {
        Item *n = queues[XPP_INBOX_NORMAL].head, *c = queues[XPP_INBOX_CONTROL].head;
        if (!n || !c) return c ? XPP_INBOX_CONTROL : n ? XPP_INBOX_NORMAL : -1;
        return c->seq < n->seq ? XPP_INBOX_CONTROL : XPP_INBOX_NORMAL;
    }
    if (which != XPP_INBOX_NORMAL && queues[XPP_INBOX_CONTROL].head) return XPP_INBOX_CONTROL;
    if (which != XPP_INBOX_CONTROL && queues[XPP_INBOX_NORMAL].head) return XPP_INBOX_NORMAL;
    return -1;
}

} // namespace

void xpp_inbox_set_classifier(int (*cls)(const char *line, unsigned long seq))
{
    pthread_mutex_lock(&push_lock);
    classify = cls;
    pthread_mutex_unlock(&push_lock);
}

void xpp_inbox_push(const char *line, size_t n)
{
    Item *it = static_cast<Item *>(xpp_malloc(sizeof *it));
    int q = XPP_INBOX_NORMAL;
    it->next = nullptr;
    it->line = static_cast<char *>(xpp_malloc(n + 1));
    std::memcpy(it->line, line, n);
    it->line[n] = 0;
    pthread_mutex_lock(&push_lock);
    it->seq = next_seq++;
    if (classify && classify(it->line, it->seq) == XPP_INBOX_CONTROL) q = XPP_INBOX_CONTROL;
    pthread_mutex_lock(&lock);
    if (queues[q].tail) queues[q].tail->next = it;
    else queues[q].head = it;
    queues[q].tail = it;
    /* broadcast: the core may wait on one queue while a line lands in the other */
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
    pthread_mutex_unlock(&push_lock);
}

void xpp_inbox_close(void)
{
    pthread_mutex_lock(&lock);
    closed = 1;
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
}

int xpp_inbox_next(int which, int wait_ms, char **line, unsigned long *seq)
{
    struct timespec until = {};
    int q, r = 0;
    if (wait_ms > 0) {
        struct timeval now;
        gettimeofday(&now, nullptr);
        until.tv_sec = now.tv_sec + wait_ms / 1000;
        until.tv_nsec = now.tv_usec * 1000L + (wait_ms % 1000) * 1000000L;
        if (until.tv_nsec >= 1000000000L) {
            until.tv_sec++;
            until.tv_nsec -= 1000000000L;
        }
    }
    pthread_mutex_lock(&lock);
    while ((q = pick(which)) < 0 && !closed && wait_ms != 0) {
        if (wait_ms < 0) pthread_cond_wait(&ready, &lock);
        else if (pthread_cond_timedwait(&ready, &lock, &until) == ETIMEDOUT) break;
    }
    if (q < 0) q = pick(which); /* a line may have come with the timeout */
    if (q >= 0) {
        Item *it = queues[q].head;
        if (!(queues[q].head = it->next)) queues[q].tail = nullptr;
        *line = it->line;
        if (seq) *seq = it->seq;
        xpp_free(it);
        r = 1;
    } else if (closed && !queues[0].head && !queues[1].head) {
        r = -1;
    }
    pthread_mutex_unlock(&lock);
    return r;
}

/* ---- the --server reader: stdin, line by line ---------------------------- */

namespace {

/* A line longer than this is dropped up to its newline (the old read_line
   gave up at 256 MB too); pixel answers are a few MB. */
constexpr size_t MAX_LINE = size_t(256) << 20;
constexpr size_t CHUNK = 65536;

/* up to n bytes of stdin, blocking; <= 0 at end of input or on an error */
long read_stdin(char *buf, size_t n)
{
#ifdef _WIN32
    return xpp_read_stdin(buf, (int)n);
#else
    for (;;) {
        ssize_t r = read(0, buf, n);
        if (r < 0 && errno == EINTR) continue;
        return (long)r;
    }
#endif
}

void *stdin_main(void *)
{
    char *buf = nullptr;
    size_t len = 0, cap = 0, scanned = 0;
    bool skipping = false; /* inside an overlong line, until its newline */
    for (;;) {
        if (cap - len < CHUNK) {
            cap = cap * 2 + CHUNK;
            buf = static_cast<char *>(xpp_realloc(buf, cap));
        }
        long r = read_stdin(buf + len, cap - len);
        if (r <= 0) break;
        len += (size_t)r;
        /* push every complete line; search only the bytes not searched yet */
        char *start = buf, *nl;
        while ((nl = static_cast<char *>(std::memchr(buf + scanned, '\n', len - scanned))) != nullptr) {
            size_t n = (size_t)(nl - start);
            if (n > 0 && start[n - 1] == '\r') n--;
            if (!skipping) xpp_inbox_push(start, n);
            skipping = false;
            start = nl + 1;
            scanned = (size_t)(start - buf);
        }
        if (start != buf) {
            len -= (size_t)(start - buf);
            std::memmove(buf, start, len);
        }
        scanned = len;
        if (len > MAX_LINE) {
            skipping = true;
            len = scanned = 0;
        }
    }
    xpp_free(buf);
    xpp_inbox_close();
    return nullptr;
}

} // namespace

int xpp_inbox_start_stdin(void)
{
    pthread_t t;
    if (pthread_create(&t, nullptr, stdin_main, nullptr) != 0) return 0;
    pthread_detach(t);
    return 1;
}

/* ---- the --script reader: a file, one line at a time, pulled by the core ----

   The reader keeps one command line read ahead (`ahead`), so the core can
   look at the line after the one it is about to run (xpp_inbox_script_peek:
   a recorded interruption follows the command it interrupted). */

namespace {

FILE *script_fp;
int script_line; /* of the line last pushed, for error messages */

struct Ahead {
    bool valid = false; /* read, not yet pushed or skipped */
    bool eof = false;   /* nothing left after the lines already handed out */
    std::string text;
    int line = 0; /* its file line number */
} ahead;
int file_line; /* lines of the file read so far */

/* read the next command line of the file into `ahead` (blank lines and
   comments skipped), or mark the end of the file */
void read_ahead()
{
    if (ahead.valid || ahead.eof || !script_fp) return;
    for (;;) {
        std::string s;
        int c;
        file_line++;
        while ((c = std::fgetc(script_fp)) != EOF && c != '\n')
            if (c != '\r') s += (char)c; /* CRLF script files */
        if (c == EOF && s.empty()) { /* nothing left to skip past */
            std::fclose(script_fp);
            script_fp = nullptr;
            ahead.eof = true;
            return;
        }
        size_t p = s.find_first_not_of(" \t");
        if (p == std::string::npos || s[p] == '#') { /* blank or a comment line */
            if (c == EOF) {
                std::fclose(script_fp);
                script_fp = nullptr;
                ahead.eof = true;
                return;
            }
            continue;
        }
        ahead.text = std::move(s);
        ahead.line = file_line;
        ahead.valid = true;
        return;
    }
}

bool script_open() { return script_fp || ahead.valid || ahead.eof; }

} // namespace

int xpp_inbox_script_line(void) { return script_line; }

int xpp_inbox_start_file(const char *path)
{
    script_fp = std::fopen(path, "rb");
    return script_fp != nullptr;
}

void xpp_inbox_script_advance(void)
{
    if (!script_open()) return;
    try {
        read_ahead();
    } catch (const std::bad_alloc &) {
        std::fputs("xppautX: out of memory reading the script\n", stderr);
        std::abort();
    }
    if (ahead.valid) {
        ahead.valid = false;
        script_line = ahead.line;
        xpp_inbox_push(ahead.text.data(), ahead.text.size());
        return;
    }
    if (ahead.eof) {
        ahead.eof = false; /* closed once: later calls do nothing */
        xpp_inbox_close();
    }
}

const char *xpp_inbox_script_peek(int *line_no)
{
    if (!script_open()) return nullptr;
    try {
        read_ahead();
    } catch (const std::bad_alloc &) {
        std::fputs("xppautX: out of memory reading the script\n", stderr);
        std::abort();
    }
    if (!ahead.valid) return nullptr;
    if (line_no) *line_no = ahead.line;
    return ahead.text.c_str();
}

void xpp_inbox_script_skip(void)
{
    if (xpp_inbox_script_peek(nullptr)) ahead.valid = false;
}
