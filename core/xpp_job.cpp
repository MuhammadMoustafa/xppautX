/* The cancel token of a running computation (xpp_job.h).

   State the reader threads touch is atomics (no locks): cancel_upto,
   the highest sequence number any cancel has named, running, and the
   computation gate (computing, or the uploads landing outside one).
   Everything else belongs to the main thread. A job is cancelled when the
   number it started from is <= cancel_upto; cancel_upto only grows.

   Nothing here throws. */
#include "xpp_job.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace {

std::atomic<unsigned long> cancel_upto{0};
std::atomic<bool> job_running{false}; /* depth > 0, for other threads */
/* -1 while a computation runs (compute_depth > 0), else how many
   OutsideComputation scopes hold it: the one word compute_begin and an
   upload's rename agree through, so neither can start inside the other */
std::atomic<int> compute_gate{0};
int compute_depth; /* nesting of compute_begin/end */
bool compute_told;  /* the hook was called for this job */
bool job_computed;  /* a computation began in this job */
void (*compute_hook)(void);
std::atomic<unsigned long> shared_seq{0}; /* job_seq, for other threads (stopping) */

int depth;             /* nesting of begin/end */
unsigned long job_seq; /* the outermost job's number */
unsigned long last_seq;

unsigned long load_upto() { return cancel_upto.load(std::memory_order_acquire); }

/* where the job got to (progress()), and the armed stop */
xpp::job::Progress reported;

enum class Stop { none, rows, point, frame };
Stop stop = Stop::none;
long stop_count; /* the rows, the point or the frame it stops at */
int stop_branch;
int stop_key;    /* the key the armed stop hands the job; 0: it cancels it */
int reached_key; /* a reached stop's key, not taken yet */

/* the armed stop is reached: the job is cancelled, or handed its key */
/* The throttles' clock reading (every(), below), shared by one stored
   row: an integration step passes up to this many throttled checkpoints
   right after it stores its row (its live append, plot_data.cpp; its
   progress, abort poll and live state, xpp_ui.cpp), microseconds apart, so the
   first of them reads the clock and the others take that reading. A
   checkpoint the step does not pass leaves its use to the next every()
   call, which is then at most one step late.

   On a cheap right-hand side a row takes microseconds and the clock reading
   is a few per cent of the run (W514): the reading is then kept for the
   next rows too (the kept reading), as many as fit in step_clock_seconds by the
   rate the last reading measured, so a throttle is late by about that and
   by at most step_clock_max_rows rows; a row that takes longer than
   step_clock_seconds reads the clock itself, as before. Stop and Abort
   do not wait for it: they are atomics the reader thread sets, seen by
   cancelled() at the next checkpoint. */
constexpr int step_throttles = 4;
constexpr double step_clock_seconds = xpp::INPUT_POLL_SECONDS / 4; /* the lateness a kept reading may add */
constexpr long step_clock_max_rows = 1024; /* the most rows one reading serves, whatever the rate */
int step_uses;   /* every() calls left that take step_now (report_rows sets it) */
double step_now; /* the first of them reads it */
struct KeptReading {
    bool cached;     /* this row takes step_now, a reading of an earlier row */
    long to_read;    /* rows still served by the last reading */
    long since_read; /* rows reported since it */
    bool have;       /* step_now is a reading of this job */
} kept;

double clock_now()
{
    using seconds_d = std::chrono::duration<double>;
    return seconds_d(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void stop_reached()
{
    stop = Stop::none;
    if (stop_key) reached_key = stop_key;
    else xpp::job::cancel_current();
}

/* a stop of that kind at count armed, cancelling (stop_with_key may give
   it a key after) */
void arm(Stop kind, long count)
{
    stop = kind;
    stop_count = count;
    stop_key = 0;
}

} // namespace

namespace xpp::job {

void begin(unsigned long seq)
{
    if (depth++ > 0) return;
    reported = Progress{};
    compute_told = false;
    job_computed = false;
    if (seq == 0) { /* no command line: a number no cancel so far covers */
        seq = last_seq > load_upto() ? last_seq : load_upto();
        seq++;
    }
    job_seq = seq;
    shared_seq.store(seq, std::memory_order_relaxed);
    if (seq > last_seq) last_seq = seq;
    job_running.store(true, std::memory_order_release);
}

void end()
{
    if (depth == 0 || --depth > 0) return;
    stop = Stop::none;
    reached_key = 0;
    kept.cached = false;
    kept.have = false;
    job_running.store(false, std::memory_order_release);
}

void report_rows(long rows, double t)
{
    ++kept.since_read;
    kept.cached = kept.to_read > 0 && kept.have;
    if (kept.cached) {
        --kept.to_read;
        step_uses = 0;
    } else {
        step_uses = step_throttles;
    }
    reported.what = Reported::rows;
    reported.rows = rows;
    reported.t = t;
    if (stop == Stop::rows && rows == stop_count) stop_reached();
}

void report_point(int branch, int point)
{
    kept.cached = false;
    reported.what = Reported::point;
    reported.branch = branch;
    reported.point = point;
    if (stop == Stop::point && branch == stop_branch && point + 1 == stop_count) stop_reached();
}

void report_frame(int frame)
{
    kept.cached = false;
    reported.what = Reported::frame;
    reported.frame = frame;
    if (stop == Stop::frame && frame == stop_count) stop_reached();
}

Progress progress() { return reported; }

void stop_at_rows(long rows) { arm(Stop::rows, rows); }

void stop_at_point(int branch, int point)
{
    arm(Stop::point, point);
    stop_branch = branch;
}

void stop_at_frame(int frame) { arm(Stop::frame, frame); }

void stop_with_key(int key) { stop_key = key; }

int take_key()
{
    const int k = reached_key;
    reached_key = 0;
    return k;
}

bool stop_armed() { return stop != Stop::none; }

bool running() { return job_running.load(std::memory_order_acquire); }

void compute_begin()
{
    if (compute_depth++ > 0) return;
    /* an upload landing now (OutsideComputation) finishes its rename first */
    for (int idle = 0; !compute_gate.compare_exchange_weak(idle, -1, std::memory_order_acq_rel, std::memory_order_acquire);
         idle = 0)
        std::this_thread::yield();
    if (depth > 0) job_computed = true;
    if (depth > 0 && !compute_told && compute_hook) {
        compute_told = true;
        compute_hook();
    }
}

void set_compute_hook(void (*hook)()) { compute_hook = hook; }

void compute_end()
{
    if (compute_depth > 0 && --compute_depth == 0) compute_gate.store(0, std::memory_order_release);
}

bool computing() { return compute_gate.load(std::memory_order_acquire) < 0; }

OutsideComputation::OutsideComputation()
{
    int n = compute_gate.load(std::memory_order_acquire);
    while (n >= 0 && !compute_gate.compare_exchange_weak(n, n + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
    }
    held_ = n >= 0;
}

OutsideComputation::~OutsideComputation()
{
    if (held_) compute_gate.fetch_sub(1, std::memory_order_release);
}

bool computed() { return depth > 0 && job_computed; }

bool stopping()
{
    return job_running.load(std::memory_order_acquire) && shared_seq.load(std::memory_order_relaxed) <= load_upto();
}

void cancel(unsigned long upto_seq)
{
    unsigned long cur = load_upto();
    while (upto_seq > cur &&
           !cancel_upto.compare_exchange_strong(cur, upto_seq, std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
    }
}

void cancel_current()
{
    if (depth > 0) cancel(job_seq);
}

bool cancelled() { return depth > 0 && job_seq <= load_upto(); }

void resume(unsigned long seq)
{
    if (depth > 0 && seq > job_seq) {
        job_seq = seq;
        shared_seq.store(seq, std::memory_order_relaxed);
        if (seq > last_seq) last_seq = seq;
    }
}

} // namespace xpp::job

namespace xpp {

bool every(double &last, double seconds)
{
    static const bool always = std::getenv("XPP_NO_THROTTLE") != nullptr; /* a check's hook (xpp_job.h) */
    if (always) return true;
    double now;
    if (kept.cached) {
        now = step_now;
    } else if (step_uses > 0) {
        if (step_uses == step_throttles) {
            const double before = step_now, read = clock_now();
            /* the rows that fit in step_clock_seconds at the rate measured since the last reading; none
               before there is one */
            const double rate = !kept.have ? 0.0
                                : read > before ? static_cast<double>(kept.since_read) * step_clock_seconds / (read - before)
                                                : static_cast<double>(step_clock_max_rows);
            kept.to_read = std::max(static_cast<long>(std::min(rate, static_cast<double>(step_clock_max_rows))) - 1, 0L);
            kept.since_read = 0;
            kept.have = true;
            step_now = read;
        }
        step_uses--;
        now = step_now;
    } else {
        now = clock_now();
    }
    if (now - last < seconds) return false;
    last = now;
    return true;
}

} // namespace xpp

bool xpp::job::poll_due()
{
    static double last;
    return every(last, INPUT_POLL_SECONDS);
}

