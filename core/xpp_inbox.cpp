/* The protocol's input queues (xpp_inbox.h).

   The core runs on the main thread and never reads a file descriptor or a
   socket for protocol input: reader threads push lines here and the core
   takes them with xpp_inbox_next(). That keeps input moving while the core
   computes, so a later classifier can pick out Abort or Quit at once.

   Two locks: push_lock serialises pushers (sequence number, classifier,
   enqueue happen as one step, so sequence order is queue order) and is
   never taken by the core; lock guards the queues and is held only briefly.
   Waiting is on a condition variable, never a polling loop. This file
   includes no core header but the small C APIs of xpp_log.h, xpp_io.h
   and xpp_files.h.

   C++ with a C API for the pushers (xpp_inbox.h is extern "C"); the core
   takes a line into a std::string (the header's C++ section). Nothing here
   throws into C or out of a thread (an allocation that fails ends the
   program). */
#include "xpp_inbox.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <cstring>
#include <deque>
#include <functional>
#include <new>
#include <optional>
#include <string>
#include <vector>

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
    unsigned long seq = 0;
    std::string line; /* moved out to the caller as it is */
    bool refused = false; /* the classifier's XPP_INBOX_REFUSE */
};

pthread_mutex_t push_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
/* [XPP_INBOX_NORMAL], [XPP_INBOX_CONTROL]. Never destroyed: a reader
   thread may still push while exit() destroys statics. */
std::array<std::deque<Item>, 2> &queues = *new std::array<std::deque<Item>, 2>;
int closed;
unsigned long next_seq = 1;
int (*classify)(const char *line, unsigned long seq);

/* the queue to take from now, or -1; call with the lock held */
int pick(int which)
{
    if (which == XPP_INBOX_ARRIVAL) {
        const std::deque<Item> &n = queues[XPP_INBOX_NORMAL], &c = queues[XPP_INBOX_CONTROL];
        if (n.empty() || c.empty()) return !c.empty() ? XPP_INBOX_CONTROL : !n.empty() ? XPP_INBOX_NORMAL : -1;
        return c.front().seq < n.front().seq ? XPP_INBOX_CONTROL : XPP_INBOX_NORMAL;
    }
    if (which != XPP_INBOX_NORMAL && !queues[XPP_INBOX_CONTROL].empty()) return XPP_INBOX_CONTROL;
    if (which != XPP_INBOX_CONTROL && !queues[XPP_INBOX_NORMAL].empty()) return XPP_INBOX_NORMAL;
    return -1;
}

/* No exception may reach the C callers or leave a thread: a failed
   allocation ends the program, as xpp_mem's do. */
[[noreturn]] void out_of_memory(const char *what)
{
    xpp::log_printf(XPP_LOG_ERROR, "xppautX: out of memory %s\n", what);
    std::abort();
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
    Item it;
    int q = XPP_INBOX_NORMAL, c;
    try {
        it.line.assign(line, n);
    } catch (...) {
        out_of_memory("keeping a line");
    }
    pthread_mutex_lock(&push_lock);
    it.seq = next_seq++;
    c = classify ? classify(it.line.c_str(), it.seq) : XPP_INBOX_NORMAL;
    if (c == XPP_INBOX_DROP) {
        pthread_mutex_unlock(&push_lock);
        return;
    }
    if (c == XPP_INBOX_CONTROL) q = XPP_INBOX_CONTROL;
    it.refused = c == XPP_INBOX_REFUSE;
    pthread_mutex_lock(&lock);
    try {
        queues[q].push_back(std::move(it));
    } catch (...) {
        out_of_memory("queueing a line");
    }
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

int xpp_inbox_next(int which, int wait_ms, std::string &line, unsigned long *seq, bool *refused)
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
        Item &it = queues[q].front();
        line = std::move(it.line); /* no allocation: it takes the item's block */
        if (seq) *seq = it.seq;
        if (refused) *refused = it.refused;
        queues[q].pop_front();
        r = 1;
    } else if (closed && queues[0].empty() && queues[1].empty()) {
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
    return xpp_read_stdin(buf, static_cast<int>(n));
#else
    for (;;) {
        ssize_t r = read(0, buf, n);
        if (r < 0 && errno == EINTR) continue;
        return static_cast<long>(r);
    }
#endif
}

void read_stdin_lines()
{
    std::vector<char> buf;
    size_t len = 0, scanned = 0;
    bool skipping = false; /* inside an overlong line, until its newline */
    for (;;) {
        if (buf.size() - len < CHUNK) buf.resize(buf.size() * 2 + CHUNK);
        long r = read_stdin(buf.data() + len, buf.size() - len);
        if (r <= 0) break;
        len += static_cast<size_t>(r);
        /* push every complete line; search only the bytes not searched yet */
        char *base = buf.data(), *start = base, *nl;
        while ((nl = static_cast<char *>(std::memchr(base + scanned, '\n', len - scanned))) != nullptr) {
            size_t n = static_cast<size_t>(nl - start);
            if (n > 0 && start[n - 1] == '\r') n--;
            if (!skipping) xpp_inbox_push(start, n);
            skipping = false;
            start = nl + 1;
            scanned = static_cast<size_t>(start - base);
        }
        if (start != base) {
            len -= static_cast<size_t>(start - base);
            std::memmove(base, start, len);
        }
        scanned = len;
        if (len > MAX_LINE) {
            skipping = true;
            len = scanned = 0;
        }
    }
}

void *stdin_main(void *)
{
    try {
        read_stdin_lines();
    } catch (...) {
        out_of_memory("reading stdin");
    }
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

xpp::UniqueFile script_fp;
int script_line; /* of the line last pushed, for error messages */
/* a script made as it goes (xpp_inbox_start_generated: -silent's) in
   place of a file: it is never read ahead */
std::function<std::optional<std::string>()> script_gen;

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
        while ((c = std::fgetc(script_fp.get())) != EOF && c != '\n')
            if (c != '\r') s += static_cast<char>(c); /* CRLF script files */
        if (c == EOF && s.empty()) { /* nothing left to skip past */
            script_fp.reset();
            ahead.eof = true;
            return;
        }
        size_t p = s.find_first_not_of(" \t");
        if (p == std::string::npos || s[p] == '#') { /* blank or a comment line */
            if (c == EOF) {
                script_fp.reset();
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
    script_fp.reset(xpp::files::open_stream(path, "rb"));
    return script_fp != nullptr;
}

void xpp_inbox_start_generated(std::function<std::optional<std::string>()> next)
{
    script_gen = std::move(next);
}

void xpp_inbox_script_advance(void)
{
    if (script_gen) {
        std::optional<std::string> line;
        try {
            line = script_gen();
        } catch (const std::bad_alloc &) {
            out_of_memory("making the script");
        }
        if (line) {
            script_line++;
            xpp_inbox_push(line->data(), line->size());
        } else {
            script_gen = nullptr; /* closed once: later calls do nothing */
            xpp_inbox_close();
        }
        return;
    }
    if (!script_open()) return;
    try {
        read_ahead();
    } catch (const std::bad_alloc &) {
        out_of_memory("reading the script");
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
    if (script_gen || !script_open()) return nullptr;
    try {
        read_ahead();
    } catch (const std::bad_alloc &) {
        out_of_memory("reading the script");
    }
    if (!ahead.valid) return nullptr;
    if (line_no) *line_no = ahead.line;
    return ahead.text.c_str();
}

void xpp_inbox_script_skip(void)
{
    if (xpp_inbox_script_peek(nullptr)) ahead.valid = false;
}
