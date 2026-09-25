/* xpp_job: which job an Abort cancels. The rule is by sequence number (an
   Abort cancels the running job and every job started by an earlier line,
   never a later one), and getting it wrong shows up end to end only as a
   run that sometimes ignores Abort, or a command that sometimes does
   nothing. */
#include "xpptest.h"
#include "xpp_job.h"

int main(void)
{
    CHECK(!xpp_job_running());
    CHECK(!xpp_job_cancelled()); /* no job, nothing to cancel */

    /* an Abort cancels the running job */
    xpp_job_begin(5);
    CHECK(xpp_job_running());
    CHECK(!xpp_job_cancelled());
    xpp_job_cancel(6);
    CHECK(xpp_job_cancelled());
    /* nested jobs are one job */
    xpp_job_begin(0);
    CHECK(xpp_job_cancelled());
    xpp_job_end();
    CHECK(xpp_job_running());
    xpp_job_end();
    CHECK(!xpp_job_running());
    CHECK(!xpp_job_cancelled());

    /* a command that came before the Abort but begins after it */
    xpp_job_begin(6);
    CHECK(xpp_job_cancelled());
    xpp_job_end();
    /* ... but not one sent after it */
    xpp_job_begin(7);
    CHECK(!xpp_job_cancelled());
    xpp_job_end();

    /* a cancel never goes back: an older Abort changes nothing */
    xpp_job_cancel(3);
    xpp_job_begin(8);
    CHECK(!xpp_job_cancelled());
    /* a prompt answered after an Abort: the answer is the last word */
    xpp_job_cancel(9);
    CHECK(xpp_job_cancelled());
    xpp_job_resume(10);
    CHECK(!xpp_job_cancelled());
    xpp_job_cancel(11);
    CHECK(xpp_job_cancelled());
    xpp_job_end();

    /* a job with no command line (the X11 front end): numbered after every
       cancel so far, and cancelled by Escape seen in the job */
    xpp_job_begin(0);
    CHECK(!xpp_job_cancelled());
    xpp_job_cancel_current();
    CHECK(xpp_job_cancelled());
    xpp_job_end();
    xpp_job_begin(0);
    CHECK(!xpp_job_cancelled());
    xpp_job_end();
    xpp_job_cancel_current(); /* outside a job: nothing */
    xpp_job_begin(0);
    CHECK(!xpp_job_cancelled());
    xpp_job_end();

    /* an unmatched end does not underflow */
    xpp_job_end();
    CHECK(!xpp_job_running());

    /* where a job got to: the last report, reset when a job begins */
    xpp_job_begin(20);
    CHECK(xpp_job_progress().what == XPP_JOB_OTHER);
    xpp_job_rows_stored(10, 0.5);
    CHECK(xpp_job_progress().what == XPP_JOB_INTEGRATE && xpp_job_progress().rows == 10 &&
          xpp_job_progress().t == 0.5);
    xpp_job_end();
    CHECK(xpp_job_progress().rows == 10); /* still there after the job */

    /* a replayed interruption: a stop armed between jobs is the next job's,
       and cancels it when the row counter reaches it, not before */
    xpp_job_stop_at_rows(3);
    CHECK(xpp_job_stop_armed());
    xpp_job_begin(21);
    CHECK(xpp_job_progress().what == XPP_JOB_OTHER);
    xpp_job_rows_stored(2, 0.1);
    CHECK(!xpp_job_cancelled());
    xpp_job_rows_stored(3, 0.2);
    CHECK(xpp_job_cancelled());
    CHECK(!xpp_job_stop_armed());
    xpp_job_end();

    /* AUTO: a stop at point P of branch B is reached when P-1 of B is stored
       (the cancelled run then ends B with point P, an end point) */
    xpp_job_begin(22);
    xpp_job_stop_at_point(2, 5);
    xpp_job_point_stored(1, 4);
    CHECK(!xpp_job_cancelled());
    xpp_job_point_stored(2, 3);
    CHECK(!xpp_job_cancelled());
    xpp_job_point_stored(2, 4);
    CHECK(xpp_job_cancelled());
    CHECK(xpp_job_progress().what == XPP_JOB_AUTO && xpp_job_progress().branch == 2 &&
          xpp_job_progress().point == 4);
    xpp_job_end();

    /* a stop the job never reached is still armed at its end (a script then
       fails), and gone after it */
    xpp_job_begin(23);
    xpp_job_stop_at_rows(100);
    xpp_job_rows_stored(5, 1.0);
    CHECK(xpp_job_stop_armed());
    CHECK(!xpp_job_cancelled());
    xpp_job_end();
    CHECK(!xpp_job_stop_armed());

    /* the poll throttle lets the first poll through (the 50 ms after it
       would make a flaky test) */
    CHECK(xpp_job_poll_due());

    TEST_REPORT("job cancel");
}
