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

/* Bound allocation for command input; discard an oversized line through its newline
   and enqueue one error. Pixel answers need several MB. */
constexpr size_t MAX_LINE = size_t(256) << 20;
constexpr size_t CHUNK = 65536; /* Read stdin in bounded chunks rather than one syscall per byte. */

void input_too_large()
{
    xpp::inbox::push(xpp::format(R"({{"input_error":"command exceeds the {} MB input limit"}})", MAX_LINE >> 20));
}

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
            if (!skipping) {
                if (n > MAX_LINE) input_too_large();
                else xpp::inbox::push({start, n});
            }
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
            if (!skipping) input_too_large();
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

/* --silent's internal command source, pulled only when the core is ready. */
namespace xpp::inbox {
namespace {
std::function<std::optional<std::string>()> generated;
} // namespace

void start_generated(std::function<std::optional<std::string>()> next)
{
    generated = std::move(next);
}

void generated_advance()
{
    if (!generated) return;
    std::optional<std::string> line;
    try {
        line = generated();
    } catch (const std::bad_alloc &) {
        out_of_memory_now("making the silent commands");
    }
    if (line) push(*line);
    else {
        generated = nullptr;
        close();
    }
}

} // namespace xpp::inbox
