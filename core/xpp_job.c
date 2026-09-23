/* The cancel token of a running computation (xpp_job.h).

   State the reader threads touch is two words, read and written with the
   GCC/Clang __atomic builtins (no locks, no pthreads): cancel_upto, the
   highest sequence number any cancel has named, and running. Everything
   else belongs to the main thread. A job is cancelled when the number it
   started from is <= cancel_upto; cancel_upto only grows. */
#include "xpp_job.h"
#include <sys/time.h>
#include <stddef.h>

int my_abort(void); /* xpp_ui.c: polls the front end, cancels on Escape */

static unsigned long cancel_upto; /* atomic */
static int running;               /* atomic: depth > 0, for other threads */

static int depth;             /* nesting of begin/end */
static unsigned long job_seq; /* the outermost job's number */
static unsigned long last_seq;

static unsigned long load_upto(void) { return __atomic_load_n(&cancel_upto, __ATOMIC_ACQUIRE); }

void xpp_job_begin(unsigned long seq)
{
    if (depth++ > 0) return;
    if (seq == 0) { /* no command line: a number no cancel so far covers */
        seq = last_seq > load_upto() ? last_seq : load_upto();
        seq++;
    }
    job_seq = seq;
    if (seq > last_seq) last_seq = seq;
    __atomic_store_n(&running, 1, __ATOMIC_RELEASE);
}

void xpp_job_end(void)
{
    if (depth == 0 || --depth > 0) return;
    __atomic_store_n(&running, 0, __ATOMIC_RELEASE);
}

int xpp_job_running(void) { return __atomic_load_n(&running, __ATOMIC_ACQUIRE); }

void xpp_job_cancel(unsigned long upto_seq)
{
    unsigned long cur = load_upto();
    while (upto_seq > cur &&
           !__atomic_compare_exchange_n(&cancel_upto, &cur, upto_seq, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
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
    struct timeval tv;
    double now;
    gettimeofday(&tv, NULL);
    now = tv.tv_sec + tv.tv_usec * 1e-6;
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
