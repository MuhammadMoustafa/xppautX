/* xpp_job: which job an Abort cancels. The rule is by sequence number (an
   Abort cancels the running job and every job started by an earlier line,
   never a later one), and getting it wrong shows up end to end only as a
   run that sometimes ignores Abort, or a command that sometimes does
   nothing. */
#include "xpptest.h"
#include "xpp_job.h"

#include <atomic>
#include <thread>

int main(void)
{
    CHECK(!xpp::job::running());
    CHECK(!xpp::job::cancelled()); /* no job, nothing to cancel */

    /* an Abort cancels the running job */
    xpp::job::begin(5);
    CHECK(xpp::job::running());
    CHECK(!xpp::job::cancelled());
    xpp::job::cancel(6);
    CHECK(xpp::job::cancelled());
    /* nested jobs are one job */
    xpp::job::begin(0);
    CHECK(xpp::job::cancelled());
    xpp::job::end();
    CHECK(xpp::job::running());
    xpp::job::end();
    CHECK(!xpp::job::running());
    CHECK(!xpp::job::cancelled());

    /* a command that came before the Abort but begins after it */
    xpp::job::begin(6);
    CHECK(xpp::job::cancelled());
    xpp::job::end();
    /* ... but not one sent after it */
    xpp::job::begin(7);
    CHECK(!xpp::job::cancelled());
    xpp::job::end();

    /* a cancel never goes back: an older Abort changes nothing */
    xpp::job::cancel(3);
    xpp::job::begin(8);
    CHECK(!xpp::job::cancelled());
    /* a prompt answered after an Abort: the answer is the last word */
    xpp::job::cancel(9);
    CHECK(xpp::job::cancelled());
    xpp::job::resume(10);
    CHECK(!xpp::job::cancelled());
    xpp::job::cancel(11);
    CHECK(xpp::job::cancelled());
    xpp::job::end();

    /* a job with no command line (the X11 front end): numbered after every
       cancel so far, and cancelled by Escape seen in the job */
    xpp::job::begin(0);
    CHECK(!xpp::job::cancelled());
    xpp::job::cancel_current();
    CHECK(xpp::job::cancelled());
    xpp::job::end();
    xpp::job::begin(0);
    CHECK(!xpp::job::cancelled());
    xpp::job::end();
    xpp::job::cancel_current(); /* outside a job: nothing */
    xpp::job::begin(0);
    CHECK(!xpp::job::cancelled());
    xpp::job::end();

    /* an unmatched end does not underflow */
    xpp::job::end();
    CHECK(!xpp::job::running());

    /* where a job got to: the last report, reset when a job begins */
    xpp::job::begin(20);
    CHECK(xpp::job::progress().what == xpp::job::Reported::nothing);
    xpp::job::report_rows(10, 0.5);
    CHECK(xpp::job::progress().what == xpp::job::Reported::rows && xpp::job::progress().rows == 10 &&
          xpp::job::progress().t == 0.5);
    xpp::job::end();
    CHECK(xpp::job::progress().rows == 10); /* still there after the job */

    /* a replayed interruption: a stop armed between jobs is the next job's,
       and cancels it when the row counter reaches it, not before */
    xpp::job::stop_at_rows(3);
    CHECK(xpp::job::stop_armed());
    xpp::job::begin(21);
    CHECK(xpp::job::progress().what == xpp::job::Reported::nothing);
    xpp::job::report_rows(2, 0.1);
    CHECK(!xpp::job::cancelled());
    xpp::job::report_rows(3, 0.2);
    CHECK(xpp::job::cancelled());
    CHECK(!xpp::job::stop_armed());
    xpp::job::end();

    /* AUTO: a stop at point P of branch B is reached when P-1 of B is stored
       (the cancelled run then ends B with point P, an end point) */
    xpp::job::begin(22);
    xpp::job::stop_at_point(2, 5);
    xpp::job::report_point(1, 4);
    CHECK(!xpp::job::cancelled());
    xpp::job::report_point(2, 3);
    CHECK(!xpp::job::cancelled());
    xpp::job::report_point(2, 4);
    CHECK(xpp::job::cancelled());
    CHECK(xpp::job::progress().what == xpp::job::Reported::point && xpp::job::progress().branch == 2 &&
          xpp::job::progress().point == 4);
    xpp::job::end();

    /* a stop the job never reached is still armed at its end (a script then
       fails), and gone after it */
    xpp::job::begin(23);
    xpp::job::stop_at_rows(100);
    xpp::job::report_rows(5, 1.0);
    CHECK(xpp::job::stop_armed());
    CHECK(!xpp::job::cancelled());
    xpp::job::end();
    CHECK(!xpp::job::stop_armed());

    /* an upload lands outside a computation (W134): held when none runs,
       several at once; refused while one runs */
    {
        xpp::job::OutsideComputation a, b;
        CHECK(a && b && !xpp::job::computing());
    }
    xpp::job::compute_begin();
    CHECK(xpp::job::computing());
    {
        xpp::job::OutsideComputation c;
        CHECK(!c);
    }
    xpp::job::compute_end();
    {
        xpp::job::OutsideComputation d;
        CHECK(d);
    }
    /* ... and a computation begins only once an upload held on another
       thread has ended: compute_begin returns after the release, never
       before, however the threads are scheduled */
    {
        std::atomic<bool> ready{false}, held{false}, go{false}, released{false};
        std::thread upload([&] {
            xpp::job::OutsideComputation e;
            held = static_cast<bool>(e);
            ready = true;
            while (!go) std::this_thread::yield();
            released = true; /* still held: the scope ends below */
        });
        while (!ready) std::this_thread::yield();
        CHECK(held);
        go = true;
        xpp::job::compute_begin();
        CHECK(released);
        xpp::job::compute_end();
        upload.join();
    }

    /* the poll throttle lets the first poll through (the 50 ms after it
       would make a flaky test) */
    CHECK(xpp::job::poll_due());

    TEST_REPORT("job cancel");
}
