#ifndef XPP_JOB_H
#define XPP_JOB_H
#ifdef __cplusplus
extern "C" {
#endif

/* The cancel token of a running computation (xpp_job.cpp).

   The core computes on the main thread. A "job" is one command being carried
   out: the JSON front end runs every protocol command as a job, the X11 front
   end an integration or an AUTO run. Abort (and Quit) can come from another
   thread at any moment: the protocol's reader threads call xpp_job_cancel()
   as soon as the line arrives, and the computation sees it at its next
   xpp_job_cancelled() or checkpoint, without the front end having to read
   input first.

   Cancellation is by sequence number, the one xpp_inbox.cpp gives every input
   line: xpp_job_cancel(n) cancels the job running now and every job whose
   command line came before line n, even one that has not begun yet. So an
   Abort sent right after Run still stops that run, however late the engine
   takes Run from its queue, while a command sent after the Abort runs
   normally.

   This file and xpp_job.cpp include no front-end header. */

/* A job begins and ends; nested pairs are fine (a counter), the outermost
   one counts. seq is the sequence number of the command line that started
   the job; 0 when there is none (the X11 front end, a command the program
   gives itself): the job then gets a number after every cancel so far. */
void xpp_job_begin(unsigned long seq);
void xpp_job_end(void);

/* 1 while a job runs. Any thread; the protocol's classifier asks it. */
int xpp_job_running(void);

/* A computation inside the job: an integration (a range of them
   included), Sing pts' and a boundary value problem's iterations, an AUTO
   run -- what Escape and Abort stop. While one runs, the protocol takes
   only what the computation itself acts on and drops everything else
   (core/ui_json.cpp during_run(), docs/protocol.md "Commands during a
   command"). Nested pairs are fine (a counter); main thread. C++ code
   uses xpp::Computation (below), a scope. */
void xpp_job_compute_begin(void);
void xpp_job_compute_end(void);

/* 1 while a computation runs. Any thread; the protocol's classifier asks it. */
int xpp_job_computing(void);

/* 1 when the running job has begun a computation, running or done (a
   Flow's next trajectory, a range's next run): a setting taken now waits
   for the job's end (docs/protocol.md "Commands during a command"). Main
   thread. */
int xpp_job_computed(void);

/* hook() is called once per job, when its first computation begins (a job
   whose computations come one after another, a Flow's trajectories, calls
   it once): the front end tells its client that the job computes until
   it ends (docs/protocol.md "computing"). Main thread; NULL for none. */
void xpp_job_set_compute_hook(void (*hook)(void));

/* 1 while the running job has been cancelled and is ending: what arrives
   now is for after it (docs/protocol.md "Commands during a command"). Any
   thread, like xpp_job_cancelled() for the main thread. */
int xpp_job_stopping(void);

/* Cancel the running job and every job started by a line up to upto_seq.
   Any thread. */
void xpp_job_cancel(unsigned long upto_seq);

/* Cancel the running job, whatever its number (a front end saw Escape or
   its Abort button). Main thread; nothing when no job runs. */
void xpp_job_cancel_current(void);

/* 1 when the running job has been cancelled. One atomic load and a compare:
   fine in the tightest loop. 0 outside a job. */
int xpp_job_cancelled(void);

/* The running job now dates from line seq: a cancel sent before seq no
   longer applies to it. For a prompt the user answered after an Abort:
   the answer, not the Abort, is their last word. Main thread. */
void xpp_job_resume(unsigned long seq);

/* 1 when at least `seconds` have passed since *last (then *last becomes
   now; start it at 0): the rate limit of polls and flushes in long loops.
   XPP_NO_THROTTLE set in the environment makes it always 1: every throttled
   flush then happens, so a check sees each state a long loop passes through
   whatever the machine's speed (tools/servercheck.py's autoinfo checks) */
int xpp_every(double *last, double seconds);

/* 1 at most every 50 ms (then the clock starts again): whether the front end
   should be polled now. my_abort() and byeauto_() (xpp_ui.cpp) share it, so a
   tight loop calling them does not hammer the front end. */
int xpp_job_poll_due(void);

/* ---- where a job got to, and replaying an interruption ----------------

   Computations report their progress as they store results: an integration
   every row it puts in storage (integrate.c), AUTO every point it adds to
   the diagram (autevd.c addbif). When a job ends cancelled, the front end
   reads the last report to say where it stopped (docs/protocol.md,
   "stopped"). A script replaying the session arms the same point before
   the job runs, and the job cancels itself exactly there: the stored rows,
   or the diagram, come out as in the recorded session.

   The progress is reset when a job begins. Main thread only. */

#define XPP_JOB_OTHER 0     /* nothing reported: no integration, no AUTO */
#define XPP_JOB_INTEGRATE 1 /* rows, t */
#define XPP_JOB_AUTO 2      /* branch, point */

typedef struct {
    int what;          /* XPP_JOB_OTHER, _INTEGRATE or _AUTO: the last report */
    long rows;         /* rows in storage */
    double t;          /* the time of the last row stored */
    int branch, point; /* the last point AUTO stored */
} XppJobProgress;

/* An integration has `rows` rows in storage, the last one at time t: a
   row was just stored, or an integration starts with these. Cancels the
   job when a stop at `rows` is armed. */
void xpp_job_rows_stored(long rows, double t);

/* AUTO stored point `point` (> 0) of branch `branch` (> 0). A stop at point
   P is reached here when P-1 is stored: AUTO's next solve sees the cancel
   and ends the branch on point P, an end point (EP) repeating P-1, which is
   how every cancelled AUTO run ends (autlib1.c stplae, stplbv). */
void xpp_job_point_stored(int branch, int point);

/* the running (or, between jobs, the last) job's progress */
XppJobProgress xpp_job_progress(void);

/* Arm a stop for the running job, or the next one to begin when none
   runs: it cancels itself when the integration has stored `rows` rows,
   or when AUTO has stored point `point` of branch `branch` (see above). A
   new arm replaces the old one; the outermost xpp_job_end() disarms. */
void xpp_job_stop_at_rows(long rows);
void xpp_job_stop_at_point(int branch, int point);

/* 1 while a stop is armed and not reached yet */
int xpp_job_stop_armed(void);

#ifdef __cplusplus
}

namespace xpp {
/* a computation's extent (xpp_job_compute_begin/end) as a scope */
struct Computation {
    Computation() { xpp_job_compute_begin(); }
    ~Computation() { xpp_job_compute_end(); }
    Computation(const Computation &) = delete;
    Computation &operator=(const Computation &) = delete;
};
} // namespace xpp
#endif
#endif
