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
    CHECK(xpp_job_checkpoint());
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

    /* the poll throttle lets the first poll through (the 50 ms after it
       would make a flaky test) */
    CHECK(xpp_job_poll_due());

    TEST_REPORT("job cancel");
}
