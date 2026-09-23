/* The cancel token of a running computation (xpp_job.h).

   State the reader threads touch is two atomics (no locks): cancel_upto,
   the highest sequence number any cancel has named, and running.
   Everything else belongs to the main thread. A job is cancelled when the
   number it started from is <= cancel_upto; cancel_upto only grows.

   C++ with a C API (xpp_job.h is extern "C"); nothing here throws. */
#include "xpp_job.h"

#include <atomic>
#include <chrono>

extern "C" int my_abort(void); /* xpp_ui.c: polls the front end, cancels on Escape */

namespace {

std::atomic<unsigned long> cancel_upto{0};
std::atomic<bool> running{false}; /* depth > 0, for other threads */

int depth;             /* nesting of begin/end */
unsigned long job_seq; /* the outermost job's number */
unsigned long last_seq;

unsigned long load_upto() { return cancel_upto.load(std::memory_order_acquire); }

} // namespace

void xpp_job_begin(unsigned long seq)
{
    if (depth++ > 0) return;
    if (seq == 0) { /* no command line: a number no cancel so far covers */
        seq = last_seq > load_upto() ? last_seq : load_upto();
        seq++;
    }
    job_seq = seq;
    if (seq > last_seq) last_seq = seq;
    running.store(true, std::memory_order_release);
}

void xpp_job_end(void)
{
    if (depth == 0 || --depth > 0) return;
    running.store(false, std::memory_order_release);
}

int xpp_job_running(void) { return running.load(std::memory_order_acquire); }

void xpp_job_cancel(unsigned long upto_seq)
{
    unsigned long cur = load_upto();
    while (upto_seq > cur &&
           !cancel_upto.compare_exchange_strong(cur, upto_seq, std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
    }
}

void xpp_job_cancel_current(void)
{
    if (depth > 0) xpp_job_cancel(job_seq);
}

int xpp_job_cancelled(void) { return depth > 0 && job_seq <= load_upto(); }

void xpp_job_resume(unsigned long seq)
{
    if (depth > 0 && seq > job_seq) {
        job_seq = seq;
        if (seq > last_seq) last_seq = seq;
    }
}

int xpp_every(double *last, double seconds)
{
    using seconds_d = std::chrono::duration<double>;
    const double now = seconds_d(std::chrono::system_clock::now().time_since_epoch()).count();
    if (now - *last < seconds && now >= *last) return 0; /* a clock set back also passes */
    *last = now;
    return 1;
}

int xpp_job_poll_due(void)
{
    static double last;
    return xpp_every(&last, 0.05);
}

int xpp_job_checkpoint(void)
{
    if (xpp_job_cancelled()) return 1;
    return my_abort() == 27;
}
