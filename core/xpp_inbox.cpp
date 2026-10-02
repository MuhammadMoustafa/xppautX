/* The protocol's input queues (xpp_inbox.h).

   The core runs on the main thread and never reads a file descriptor or a
   socket for protocol input: reader threads push lines here and the core
   takes them with next(). That keeps input moving while the core
   computes, so a later classifier can pick out Abort or Quit at once.

   Two locks: push_lock serialises pushers (sequence number, classifier,
   enqueue happen as one step, so sequence order is queue order) and is
   never taken by the core; lock guards the queues and is held only briefly.
   Waiting is on a condition variable, never a polling loop. This file
   includes no core header but the small APIs of xpp_log.h, xpp_io.h,
   xpp_files.h and xpp_mem.h.

   Nothing here throws out of a thread (an allocation that fails ends the
   program: xpp::out_of_memory_now). */
#include "xpp_inbox.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_mem.h"

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
    bool refused = false; /* the classifier's Verdict::refuse */
};

pthread_mutex_t push_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
/* [NORMAL], [CONTROL]. Never destroyed: a reader thread may still push
   while exit() destroys statics. */
std::array<std::deque<Item>, 2> &queues = *new std::array<std::deque<Item>, 2>;
bool closed;
unsigned long next_seq = 1;
xpp::inbox::Verdict (*classify)(const char *line, unsigned long seq);

/* the queues' indices */
constexpr int NORMAL = 0, CONTROL = 1;

/* the queue to take from now, or -1; call with the lock held */
int pick(xpp::inbox::From which)
{
    if (which == xpp::inbox::From::arrival) {
        const std::deque<Item> &n = queues[NORMAL], &c = queues[CONTROL];
        if (n.empty() || c.empty()) return !c.empty() ? CONTROL : !n.empty() ? NORMAL : -1;
        return c.front().seq < n.front().seq ? CONTROL : NORMAL;
    }
    if (which != xpp::inbox::From::normal && !queues[CONTROL].empty()) return CONTROL;
    if (which != xpp::inbox::From::control && !queues[NORMAL].empty()) return NORMAL;
    return -1;
}

} // namespace

namespace xpp::inbox {

void set_classifier(Verdict (*cls)(const char *line, unsigned long seq))
{
    pthread_mutex_lock(&push_lock);
    classify = cls;
    pthread_mutex_unlock(&push_lock);
}

void push(std::string_view line)
{
    Item it;
    int q = NORMAL;
    try {
        it.line.assign(line);
    } catch (...) {
        out_of_memory_now("keeping a line");
    }
    pthread_mutex_lock(&push_lock);
    it.seq = next_seq++;
    const Verdict c = classify ? classify(it.line.c_str(), it.seq) : Verdict::normal;
    if (c == Verdict::drop) {
        pthread_mutex_unlock(&push_lock);
        return;
    }
    if (c == Verdict::control) q = CONTROL;
    it.refused = c == Verdict::refuse;
    pthread_mutex_lock(&lock);
    try {
        queues[q].push_back(std::move(it));
    } catch (...) {
        out_of_memory_now("queueing a line");
    }
    /* broadcast: the core may wait on one queue while a line lands in the other */
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
    pthread_mutex_unlock(&push_lock);
}

void close()
{
    pthread_mutex_lock(&lock);
    closed = true;
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
}

Took next(From which, int wait_ms, std::string &line, unsigned long &seq, bool &refused)
{
    struct timespec until = {};
    int q;
    Took r = Took::nothing;
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
        seq = it.seq;
        refused = it.refused;
        queues[q].pop_front();
        r = Took::line;
    } else if (closed && queues[NORMAL].empty() && queues[CONTROL].empty()) {
        r = Took::end;
    }
    pthread_mutex_unlock(&lock);
    return r;
}

} // namespace xpp::inbox

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
    return xpp::win32::read_stdin({buf, n});
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
            if (!skipping) xpp::inbox::push({start, n});
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
        xpp::out_of_memory_now("reading stdin");
    }
    xpp::inbox::close();
    return nullptr;
}

} // namespace

bool xpp::inbox::start_stdin()
{
    pthread_t t;
    if (pthread_create(&t, nullptr, stdin_main, nullptr) != 0) return false;
    pthread_detach(t);
    return true;
}

/* ---- the --script reader: a file, one line at a time, pulled by the core ----

   The reader keeps one command line read ahead (`ahead`), so the core can
   look at the line after the one it is about to run (script_peek:
   a recorded interruption follows the command it interrupted). */

namespace {

xpp::UniqueFile script_fp;
/* the script's file, and the line last pushed (its number in the file,
   or in a script made as it goes) as written: an error's place */
struct Pushed {
    std::string path;
    int line = 0;
    std::string text;
} pushed;
/* a script made as it goes (start_generated: --silent's) in
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

namespace xpp::inbox {


xpp::Place script_place()
{
    if (pushed.path.empty() || pushed.line <= 0) return {};
    return xpp::Place{pushed.path, pushed.line, 0, pushed.text};
}

bool start_file(std::string_view path)
{
    pushed.path = path;
    script_fp.reset(xpp::files::open_stream(path, "rb"));
    return script_fp != nullptr;
}

void start_generated(std::function<std::optional<std::string>()> next)
{
    script_gen = std::move(next);
}

void script_advance()
{
    if (script_gen) {
        std::optional<std::string> line;
        try {
            line = script_gen();
        } catch (const std::bad_alloc &) {
            out_of_memory_now("making the script");
        }
        if (line) {
            pushed.line++;
            push(*line);
        } else {
            script_gen = nullptr; /* closed once: later calls do nothing */
            close();
        }
        return;
    }
    if (!script_open()) return;
    try {
        read_ahead();
    } catch (const std::bad_alloc &) {
        out_of_memory_now("reading the script");
    }
    if (ahead.valid) {
        ahead.valid = false;
        pushed.line = ahead.line;
        pushed.text = ahead.text;
        push(ahead.text);
        return;
    }
    if (ahead.eof) {
        ahead.eof = false; /* closed once: later calls do nothing */
        close();
    }
}

const char *script_peek(int &line_no)
{
    if (script_gen || !script_open()) return nullptr;
    try {
        read_ahead();
    } catch (const std::bad_alloc &) {
        out_of_memory_now("reading the script");
    }
    if (!ahead.valid) return nullptr;
    line_no = ahead.line;
    return ahead.text.c_str();
}

void script_skip()
{
    int line_no = 0;
    if (script_peek(line_no)) ahead.valid = false;
}

} // namespace xpp::inbox
