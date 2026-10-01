#ifndef XPP_JOB_H
#define XPP_JOB_H

/* The cancel token of a running computation (xpp_job.cpp), in namespace
   xpp::job (W109f).

   The core computes on the main thread. A "job" is one command being carried
   out: the JSON front end runs every protocol command as a job, the X11 front
   end an integration or an AUTO run. Abort (and Quit) can come from another
   thread at any moment: the protocol's reader threads call job::cancel()
   as soon as the line arrives, and the computation sees it at its next
   job::cancelled() or checkpoint, without the front end having to read
   input first.

   Cancellation is by sequence number, the one xpp_inbox.cpp gives every input
   line: job::cancel(n) cancels the job running now and every job whose
   command line came before line n, even one that has not begun yet. So an
   Abort sent right after Run still stops that run, however late the engine
   takes Run from its queue, while a command sent after the Abort runs
   normally.

   This file and xpp_job.cpp include no front-end header. */

namespace xpp::job {

/* A job begins and ends; nested pairs are fine (a counter), the outermost
   one counts. seq is the sequence number of the command line that started
   the job; 0 when there is none (a command the program gives itself): the
   job then gets a number after every cancel so far. */
void begin(unsigned long seq);
void end();

/* true while a job runs. Any thread; the protocol's classifier asks it. */
bool running();

/* A computation inside the job: an integration (a range of them
   included), Sing pts' and a boundary value problem's iterations, an AUTO
   run -- what Escape and Abort stop. While one runs, the protocol takes
   only what the computation itself acts on and drops everything else
   (core/ui_json.cpp during_run(), docs/protocol.md "Commands during a
   command"). Nested pairs are fine (a counter); main thread. Code uses
   xpp::Computation (below), a scope. */
void compute_begin();
void compute_end();

/* true while a computation runs. Any thread; the protocol's classifier
   asks it. */
bool computing();

/* true when the running job has begun a computation, running or done (a
   Flow's next trajectory, a range's next run): a setting taken now waits
   for the job's end (docs/protocol.md "Commands during a command"). Main
   thread. */
bool computed();

/* hook() is called once per job, when its first computation begins (a job
   whose computations come one after another, a Flow's trajectories, calls
   it once): the front end tells its client that the job computes until
   it ends (docs/protocol.md "computing"). Main thread; nullptr for none. */
void set_compute_hook(void (*hook)());

/* true while the running job has been cancelled and is ending: what
   arrives now is for after it (docs/protocol.md "Commands during a
   command"). Any thread, like cancelled() for the main thread. */
bool stopping();

/* Cancel the running job and every job started by a line up to upto_seq.
   Any thread. */
void cancel(unsigned long upto_seq);

/* Cancel the running job, whatever its number (a front end saw Escape or
   its Abort button). Main thread; nothing when no job runs. */
void cancel_current();

/* true when the running job has been cancelled. One atomic load and a
   compare: fine in the tightest loop. false outside a job. */
bool cancelled();

/* The running job now dates from line seq: a cancel sent before seq no
   longer applies to it. For a prompt the user answered after an Abort:
   the answer, not the Abort, is their last word. Main thread. */
void resume(unsigned long seq);

/* true at most every 50 ms (then the clock starts again): whether the
   front end should be polled now. my_abort() and byeauto_() (xpp_ui.cpp)
   share it, so a tight loop calling them does not hammer the front end. */
bool poll_due();

/* ---- where a job got to, and replaying an interruption ----------------

   Computations report their progress as they store results: an integration
   every row it puts in storage (integrate.cpp), AUTO every point it adds to
   the diagram (autevd.cpp addbif), the animation's Go every frame it shows
   (json_ani.cpp). When a job ends cancelled, the front end
   reads the last report to say where it stopped (docs/protocol.md,
   "stopped"). A script replaying the session arms the same point before
   the job runs, and the job cancels itself exactly there: the stored rows,
   or the diagram, come out as in the recorded session. A key the job read
   itself (/ ending a range, Escape stopping the animation's Go) is
   replayed the same way: the armed stop hands the job that key
   (take_key) instead of cancelling it.

   The progress is reset when a job begins. Main thread only. */

/* what the last report was */
enum class Reported {
    nothing, /* no integration, no AUTO, no animation */
    rows,    /* an integration's rows and t */
    point,   /* AUTO's branch and point */
    frame    /* the animation's frames */
};

struct Progress {
    Reported what = Reported::nothing; /* the last report */
    long rows = 0;                     /* rows in storage */
    double t = 0;                      /* the time of the last row stored */
    int branch = 0, point = 0;         /* the last point AUTO stored */
    int frame = 0;                     /* the animation's frames shown */
};

/* An integration has `rows` rows in storage, the last one at time t: a
   row was just stored, or an integration starts with these. Cancels the
   job when a stop at `rows` is armed. */
void report_rows(long rows, double t);

/* AUTO stored point `point` (> 0) of branch `branch` (> 0). A stop at point
   P is reached here when P-1 is stored: AUTO's next solve sees the cancel
   and ends the branch on point P, an end point (EP) repeating P-1, which is
   how every cancelled AUTO run ends (autlib1.cpp stplae, stplbv). */
void report_point(int branch, int point);

/* The animation's Go has shown `frame` frames (from 1). A stop at frame F
   is reached when frame F has been shown. */
void report_frame(int frame);

/* the running (or, between jobs, the last) job's progress */
Progress progress();

/* Arm a stop for the running job, or the next one to begin when none
   runs: it cancels itself when the integration has stored `rows` rows,
   or when AUTO has stored point `point` of branch `branch` (see above). A
   new arm replaces the old one; the outermost end() disarms. */
void stop_at_rows(long rows);
void stop_at_point(int branch, int point);
void stop_at_frame(int frame);
/* the stop just armed hands the job the key `key` (a key code) when it
   is reached, instead of cancelling it */
void stop_with_key(int key);
/* the key a reached stop hands the job, taken (0 when none): the
   checkpoints (my_abort, the animation's wait) take it before any input */
int take_key();

/* true while a stop is armed and not reached yet */
bool stop_armed();

} // namespace xpp::job

namespace xpp {

/* true when at least `seconds` have passed since `last` (then `last`
   becomes now; start it at 0): the rate limit of polls and flushes in
   long loops. XPP_NO_THROTTLE set in the environment makes it always
   true: every throttled flush then happens, so a check sees each state a
   long loop passes through whatever the machine's speed
   (tools/servercheck.py's autoinfo checks) */
bool every(double &last, double seconds);

/* a computation's extent (job::compute_begin/end) as a scope */
struct Computation {
    Computation() { job::compute_begin(); }
    ~Computation() { job::compute_end(); }
    Computation(const Computation &) = delete;
    Computation &operator=(const Computation &) = delete;
};
/* a job the program gives itself (job::begin(0): no command line) as a
   scope, ended however the scope is left (an AUTO run, an integration) */
struct Job {
    Job() { job::begin(0); }
    ~Job() { job::end(); }
    Job(const Job &) = delete;
    Job &operator=(const Job &) = delete;
};

} // namespace xpp
#endif
