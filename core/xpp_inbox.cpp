/* The protocol's input queues (xpp_inbox.h).

   The core runs on the main thread and never reads a file descriptor or a
   socket for protocol input: reader threads push lines here and the core
   takes them with next(). That keeps input moving while the core
   computes, so a later classifier can pick out Abort or Quit at once.

   Two locks: push_lock serialises pushers (sequence number, classifier,
   enqueue happen as one step, so sequence order is queue order) and is
   never taken by the core; lock guards the queues and is held only briefly.
   Waiting is on a condition variable, never a polling loop. This file
   includes no core header but the small APIs of xpp_log.h, xpp_mem.h and xpp_text.h (the JSON string escape of push_open).

   Nothing here throws out of a thread (an allocation that fails ends the
   program: xpp::out_of_memory_now). */
#include "xpp_inbox.h"
#include "xpp_log.h"
#include "xpp_text.h"
#include <cstdio>
#include <cstdlib>
#include "xpp_mem.h"

#include <cerrno>
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
#include <sys/types.h>
#include <utility>

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
struct Queues : std::array<std::deque<Item>, 2> {
    int save_ask = 0;
    std::string save_path; /* one native choice, consumed by its exact prompt */
    std::string own_open_line; /* the line push_open pushed, until the command that reads it takes it */
    size_t normal_bytes = 0; /* the bytes of the normal queue's lines */
    size_t kept_lines = 0, kept_bytes = 0; /* the command loop's own (set_kept) */
};
Queues &queues = *new Queues;
bool closed;
unsigned long next_seq = 1;
xpp::inbox::Verdict (*classify)(const char *line, unsigned long seq);
void (*busy_hook)(const char *line, std::string *reply);

/* whether a normal line of n bytes would pass the bound; call with the lock held */
bool room_for(size_t n)
{
    return queues[0].size() + queues.kept_lines < xpp::inbox::MAX_PENDING_LINES
           && queues.normal_bytes + queues.kept_bytes + n <= xpp::inbox::MAX_PENDING_BYTES;
}

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

void set_hooks(Verdict (*cls)(const char *line, unsigned long seq), void (*busy)(const char *line, std::string *reply))
{
    pthread_mutex_lock(&push_lock);
    classify = cls;
    busy_hook = busy;
    pthread_mutex_unlock(&push_lock);
}

void set_kept(size_t lines, size_t bytes)
{
    pthread_mutex_lock(&lock);
    queues.kept_lines = lines;
    queues.kept_bytes = bytes;
    pthread_mutex_unlock(&lock);
}

namespace {

/* own: the program's line, never refused (xpp_inbox.h push_own) */
bool push_line(std::string_view line, std::string *reply, bool own)
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
        return true;
    }
    if (c == Verdict::control) q = CONTROL;
    it.refused = c == Verdict::refuse;
    pthread_mutex_lock(&lock);
    if (!own && q == NORMAL && !room_for(it.line.size())) {
        pthread_mutex_unlock(&lock);
        void (*busy)(const char *, std::string *) = busy_hook;
        pthread_mutex_unlock(&push_lock); /* a client slow to read its reply holds up no other reader */
        /* answered here, not queued: the reader goes on, and a Stop behind this line is read */
        busy(it.line.c_str(), reply);
        return false;
    }
    if (q == NORMAL) queues.normal_bytes += it.line.size();
    try {
        queues[q].push_back(std::move(it));
    } catch (...) {
        out_of_memory_now("queueing a line");
    }
    /* broadcast: the core may wait on one queue while a line lands in the other */
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
    pthread_mutex_unlock(&push_lock);
    return true;
}

} // namespace

bool push(std::string_view line, std::string *reply) { return push_line(line, reply, false); }

void push_own(std::string_view line) { push_line(line, nullptr, true); }

void push_open(const char *path) noexcept
{
    try {
        std::string line = R"({"cmd":"session.open_model","file":)";
        xpp::json_append_string(line, path);
        line += '}';
        pthread_mutex_lock(&lock);
        queues.own_open_line = line;
        pthread_mutex_unlock(&lock);
        push_own(line);
    } catch (...) {
        xpp::out_of_memory("opening a model");
    }
}

bool take_own_open(std::string_view line)
{
    pthread_mutex_lock(&lock);
    const bool own = !queues.own_open_line.empty() && queues.own_open_line == line;
    if (own) queues.own_open_line.clear();
    pthread_mutex_unlock(&lock);
    return own;
}

void authorize_save(std::string_view prompt, std::string_view path)
{
    std::string choice(path);
    int ask = 0;
    const int id = parse_int(prompt, ask) && ask > 0 ? ask : 0;
    pthread_mutex_lock(&lock);
    queues.save_ask = id;
    queues.save_path = std::move(choice);
    pthread_mutex_unlock(&lock);
}

bool consume_save(int ask, std::string_view path)
{
    pthread_mutex_lock(&lock);
    const bool allowed = ask > 0 && queues.save_ask == ask && queues.save_path == path;
    queues.save_ask = 0;
    queues.save_path.clear();
    pthread_mutex_unlock(&lock);
    return allowed;
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
        if (q == NORMAL) queues.normal_bytes -= line.size();
        queues[q].pop_front();
        r = Took::line;
    } else if (closed && queues[NORMAL].empty() && queues[CONTROL].empty()) {
        r = Took::end;
    }
    pthread_mutex_unlock(&lock);
    return r;
}

} // namespace xpp::inbox

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
