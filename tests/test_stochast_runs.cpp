/* stocHast's "Compute" (many runs), "Mean" and "Variance" (markov.cpp
   append_stoch/do_stats, driven by integrate.cpp's do_range() while
   STOCH_FLAG is set): running many independent trials of
   tools/models/stoch_runs.ode, each sampling nsamp=normal(0,1) afresh,
   the per-time mean stocHast accumulates across trials must be near 0 and
   the variance near 1, within a tolerance set by the trial count -- never
   an exact value, so this still passes after W32a's generator swap.

   do_stochast_com()'s "Compute" ('c') calls compute_em(), which prompts
   for the range with set_up_range() (a no-op in headless mode) and then
   do_range(); this test drives do_range() directly with the range fields
   it would have read, which is the same headless path. */
#include "xpptest.h"
#include "session.h"
#include "storage.h"
#include "browse.h"
#include "graphics.h"
#include "integrate.h"
#include "markov.h"
#include "xpp_batch.h"
#include "xpp_io.h"

#include <cmath>

int main(void)
{
    char arg0[] = "test_stochast_runs", arg1[] = "tools/models/stoch_runs.ode";
    char *argv[] = {arg0, arg1, NULL};
    xpp_load_model(2, argv, 1);
    init_browser(xpp::session());
    init_all_graph(xpp::session());

    const int ntrials = 400;
    xpp::session().integrator.range.item = "dummy";
    xpp::session().integrator.range.steps = ntrials - 1;
    xpp::session().integrator.range.plow = 0.0;
    xpp::session().integrator.range.phigh = 0.0;
    xpp::session().integrator.range.reset = 1;
    xpp::session().integrator.range.oldic = 1;
    xpp::session().integrator.range.cycle = 0;
    xpp::session().integrator.range.movie = 0;
    xpp::session().integrator.range.rtype = 0;

    xpp::session().stochastic.flag = 1;
    int ierr = do_range(xpp::session(), xpp::session().data_store.current, 0);
    xpp::session().stochastic.flag = 0;

    CHECK(ierr != -1);
    CHECK(xpp::session().stochastic.n_trials == ntrials);
    CHECK(xpp::session().stochastic.here == 1);
    CHECK(xpp::session().stochastic.len == 11); /* total=10,dt=1: 11 rows per trial */

    int ncol = -1;
    find_variable("nsamp", &ncol);
    CHECK(ncol > 0);

    double meantol = 4.0 / std::sqrt(static_cast<double>(ntrials));
    double vartol = 4.0 * std::sqrt(2.0 / ntrials);
    for (int i = 0; i < xpp::session().stochastic.len; i++) {
        CHECK(std::fabs(xpp::session().stochastic.mean[ncol][i] - 0.0) < meantol);
        CHECK(std::fabs(xpp::session().stochastic.variance[ncol][i] - 1.0) < vartol);
    }

    TEST_REPORT("stochast compute/mean/variance");
}
