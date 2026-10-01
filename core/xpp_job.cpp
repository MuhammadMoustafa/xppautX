/* The cancel token of a running computation (xpp_job.h).

   State the reader threads touch is three atomics (no locks): cancel_upto,
   the highest sequence number any cancel has named, running and computing.
   Everything else belongs to the main thread. A job is cancelled when the
   number it started from is <= cancel_upto; cancel_upto only grows.

   Nothing here throws. */
#include "xpp_job.h"

#include <atomic>
#include <chrono>
#include <cstdlib>

namespace {

std::atomic<unsigned long> cancel_upto{0};
std::atomic<bool> job_running{false}; /* depth > 0, for other threads */
std::atomic<bool> job_computing{false}; /* compute_depth > 0, for other threads */
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
    job_running.store(false, std::memory_order_release);
}

void report_rows(long rows, double t)
{
    reported.what = Reported::rows;
    reported.rows = rows;
    reported.t = t;
    if (stop == Stop::rows && rows == stop_count) stop_reached();
}

void report_point(int branch, int point)
{
    reported.what = Reported::point;
    reported.branch = branch;
    reported.point = point;
    if (stop == Stop::point && branch == stop_branch && point + 1 == stop_count) stop_reached();
}

void report_frame(int frame)
{
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
    job_computing.store(true, std::memory_order_release);
    if (depth > 0) job_computed = true;
    if (depth > 0 && !compute_told && compute_hook) {
        compute_told = true;
        compute_hook();
    }
}

void set_compute_hook(void (*hook)()) { compute_hook = hook; }

void compute_end()
{
    if (compute_depth > 0 && --compute_depth == 0) job_computing.store(false, std::memory_order_release);
}

bool computing() { return job_computing.load(std::memory_order_acquire); }

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
    using seconds_d = std::chrono::duration<double>;
    const double now = seconds_d(std::chrono::system_clock::now().time_since_epoch()).count();
    if (now - last < seconds && now >= last) return false; /* a clock set back also passes */
    last = now;
    return true;
}

} // namespace xpp

bool xpp::job::poll_due()
{
    static double last;
    return every(last, 0.05);
}

