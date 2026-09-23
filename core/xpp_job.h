#ifndef XPP_JOB_H
#define XPP_JOB_H

/* The cancel token of a running computation (xpp_job.c).

   The core computes on the main thread. A "job" is one command being carried
   out: the JSON front end runs every protocol command as a job, the X11 front
   end an integration or an AUTO run. Abort (and Quit) can come from another
   thread at any moment: the protocol's reader threads call xpp_job_cancel()
   as soon as the line arrives, and the computation sees it at its next
   xpp_job_cancelled() or checkpoint, without the front end having to read
   input first.

   Cancellation is by sequence number, the one xpp_inbox.c gives every input
   line: xpp_job_cancel(n) cancels the job running now and every job whose
   command line came before line n, even one that has not begun yet. So an
   Abort sent right after Run still stops that run, however late the engine
   takes Run from its queue, while a command sent after the Abort runs
   normally.

   This file and xpp_job.c include no front-end header. */

/* A job begins and ends; nested pairs are fine (a counter), the outermost
   one counts. seq is the sequence number of the command line that started
   the job; 0 when there is none (the X11 front end, a command the program
   gives itself): the job then gets a number after every cancel so far. */
void xpp_job_begin(unsigned long seq);
void xpp_job_end(void);

/* 1 while a job runs. Any thread; the protocol's classifier asks it. */
int xpp_job_running(void);

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
   now; start it at 0): the rate limit of polls and flushes in long loops */
int xpp_every(double *last, double seconds);

/* 1 at most every 50 ms (then the clock starts again): whether the front end
   should be polled now. my_abort() and byeauto_() (xpp_ui.c) share it, so a
   tight loop calling them does not hammer the front end. */
int xpp_job_poll_due(void);

/* 1 when the running job is cancelled; otherwise, at most every 50 ms, lets
   the front end look for Escape or Abort (my_abort(), which cancels the job
   when it sees one) and returns 1 when it saw one. Other keys the poll
   returns are dropped. Inside AUTO, byeauto_() is the same checkpoint
   polling through the AUTO window (its Abort button). */
int xpp_job_checkpoint(void);

/* AUTO: 1 while stepbv solves a Newton step (autlib1.c). A cancelled job
   then stops the collocation setup early (setubv2.c), solvbv skips the
   solve and stepbv returns to the last converged point. Other solves, such
   as stdrbv's starting direction, always run to the end. */
extern int xpp_setubv_stop;

#endif
