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
#include "browse.h"
#include "graphics.h"
#include "integrate.h"
#include "markov.h"
#include "phsplan.h"
#include "xpp_batch.h"
#include "xpp_io.h"

#include <cmath>

#define PARAM 1

extern double MyData[MAXODE];


int main(void)
{
    char arg0[] = "test_stochast_runs", arg1[] = "tools/models/stoch_runs.ode";
    char *argv[] = {arg0, arg1, NULL};
    xpp_load_model(2, argv, 1);
    init_browser();
    init_all_graph();

    const int ntrials = 400;
    XPP_STRCPY(range.item, "dummy");
    range.steps = ntrials - 1;
    range.plow = 0.0;
    range.phigh = 0.0;
    range.reset = 1;
    range.oldic = 1;
    range.cycle = 0;
    range.movie = 0;
    range.rtype = 0;

    STOCH_FLAG = 1;
    int ierr = do_range(MyData, 0);
    STOCH_FLAG = 0;

    CHECK(ierr != -1);
    CHECK(N_TRIALS == ntrials);
    CHECK(STOCH_HERE == 1);
    CHECK(stoch_len == 11); /* total=10,dt=1: 11 rows per trial */

    int ncol = -1;
    find_variable("nsamp", &ncol);
    CHECK(ncol > 0);

    double meantol = 4.0 / std::sqrt(static_cast<double>(ntrials));
    double vartol = 4.0 * std::sqrt(2.0 / ntrials);
    for (int i = 0; i < stoch_len; i++) {
        CHECK(std::fabs(my_mean[ncol][i] - 0.0) < meantol);
        CHECK(std::fabs(my_variance[ncol][i] - 1.0) < vartol);
    }

    TEST_REPORT("stochast compute/mean/variance");
}
