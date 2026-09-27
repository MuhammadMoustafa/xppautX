/* stocHast's model-level RNG functions (ran, normal, wiener) as used from
   an .ode file, plus the 1D/2D histogram and spike-time-autocorrelation
   features (histogram.cpp new_hist/two_d_hist), all computed on the one
   run of tools/models/stoch_rng.ode: x never moves, so every stored row is
   an independent sample of ran(1), normal(2,3) and the wiener "n" with a
   fixed seed (4001 rows). Checks use statistical tolerances (a few sigma),
   never exact values, so they still pass after W32a swaps the generator
   for std::mt19937_64. The spike-time-autocorrelation check instead uses
   the "t" column, whose pairwise differences are exactly known (a
   triangular distribution over an arithmetic sequence), so it is checked
   exactly. */
#include "xpptest.h"
#include "session.h"
#include "storage.h"
#include "browse.h"
#include "graphics.h"
#include "histogram.h"
#include "integrate.h"
#include "xpp_batch.h"

#include <cmath>

namespace {

double column_mean(int col, int n)
{
    double s = 0;
    for (int i = 0; i < n; i++) s += xpp::session().data_store.col[col][i];
    return s / n;
}

double column_var(int col, int n, double mean)
{
    double s = 0;
    for (int i = 0; i < n; i++) {
        double d = xpp::session().data_store.col[col][i] - mean;
        s += d * d;
    }
    return s / n;
}

} // namespace

int main(void)
{
    char arg0[] = "test_stochast_rng", arg1[] = "tools/models/stoch_rng.ode";
    char *argv[] = {arg0, arg1, NULL};
    xpp_load_model(2, argv, 1);
    init_browser();
    init_all_graph();
    batch_integrate();

    int n = xpp::session().data_store.rows;
    CHECK(n > 3900); /* total=4000, dt=1: ~4001 rows */

    int rcol = -1, ncol = -1, wcol = -1;
    find_variable("rsamp", &rcol);
    find_variable("nsamp", &ncol);
    find_variable("wsamp", &wcol);
    CHECK(rcol > 0 && ncol > 0 && wcol > 0);

    /* ran(1): uniform on [0,1), mean 0.5, var 1/12 */
    double rmean = column_mean(rcol, n), rvar = column_var(rcol, n, rmean);
    double rtol = 4.0 * std::sqrt(1.0 / 12.0) / std::sqrt(static_cast<double>(n));
    CHECK(std::fabs(rmean - 0.5) < rtol);
    CHECK(std::fabs(rvar - 1.0 / 12.0) < 0.02); /* loose: 4th-moment tail */

    /* normal(2,3): mean 2, var 9 */
    double nmean = column_mean(ncol, n), nvar = column_var(ncol, n, nmean);
    double ntol = 4.0 * 3.0 / std::sqrt(static_cast<double>(n));
    CHECK(std::fabs(nmean - 2.0) < ntol);
    CHECK(std::fabs(nvar - 9.0) < 1.5);

    /* wiener n: normal(0,1)/sqrt(dt), dt=1 here, so N(0,1) */
    double wmean = column_mean(wcol, n), wvar = column_var(wcol, n, wmean);
    double wtol = 4.0 * 1.0 / std::sqrt(static_cast<double>(n));
    CHECK(std::fabs(wmean - 0.0) < wtol);
    CHECK(std::fabs(wvar - 1.0) < 0.3);

    /* 1D histogram of ran(1) over [0,1) in 10 bins: each bin's count is a
       binomial(n, 0.1) draw; check against 5 std of that. */
    new_hist(10, 0.0, 1.0, rcol, 0, "", 0);
    CHECK(xpp::session().histogram.hist_here == 1);
    double expect = static_cast<double>(n) / 10.0;
    double binsd = std::sqrt(static_cast<double>(n) * 0.1 * 0.9);
    double total1d = 0;
    for (int i = 0; i < 10; i++) {
        CHECK(std::fabs(xpp::session().histogram.hist()[1][i] - expect) < 5.0 * binsd);
        total1d += xpp::session().histogram.hist()[1][i];
    }
    CHECK(std::fabs(total1d - n) < 1.0); /* every sample is in [0,1) */

    /* spike-time autocorrelation (new_hist's which=1: a histogram of every
       pairwise difference storage[col][i]-storage[col][j]) applied to the
       "t" column itself: t is 0,1,...,n-1, so lag k occurs exactly n-|k|
       times. Bin edges land on whole lags (zlo/zhi/dz all integers) so
       new_hist's truncating (int) cast never lands two different lags in
       the same bin; restricted to |lag|<=50 to stay well under MAXSTOR so
       no pair is dropped. */
    new_hist(100, -50.0, 50.0, 0, 0, "", 1);
    CHECK(xpp::session().histogram.hist_here == 1);
    CHECK(xpp::session().histogram.hist()[1][50] == static_cast<float>(n)); /* lag 0: n self-pairs */
    CHECK(xpp::session().histogram.hist()[1][60] == static_cast<float>(n - 10)); /* lag 10 */
    CHECK(xpp::session().histogram.hist()[1][40] == static_cast<float>(n - 10)); /* lag -10 */
    double stacor_total = 0;
    for (int i = 0; i <= 100; i++) stacor_total += xpp::session().histogram.hist()[1][i];
    double expect_total = 101.0 * n;
    for (int k = 1; k <= 50; k++) expect_total -= 2.0 * k;
    CHECK(std::fabs(stacor_total - expect_total) < 1.0);

    /* 2D histogram of ran(1) (x) against normal(2,3) (y): independent, so
       the row of bins straddling the normal's mean (y=2) should hold more
       mass than the row in its tail. twod_hist() is new_2d_hist() without
       its dialog: it reads hist_inf and bins all n stored rows. */
    const int n1 = 5, n2 = 5;
    xpp::session().histogram.info.col = rcol;
    xpp::session().histogram.info.col2 = ncol;
    xpp::session().histogram.info.nbins = n1;
    xpp::session().histogram.info.nbins2 = n2;
    xpp::session().histogram.info.xlo = 0.0;
    xpp::session().histogram.info.xhi = 1.0;
    xpp::session().histogram.info.ylo = -10.0;
    xpp::session().histogram.info.yhi = 14.0;
    CHECK(twod_hist() == 1);
    CHECK(xpp::session().histogram.hist_here == 2);
    double mid_row = 0, tail_row = 0;
    for (int i = 0; i < n1; i++) {
        mid_row += xpp::session().histogram.hist()[2][i + 2 * n1];  /* y in [-1.6, 3.2): straddles mean 2 */
        tail_row += xpp::session().histogram.hist()[2][i + 0 * n1]; /* y in [-10,-5.2): far tail */
    }
    CHECK(mid_row > tail_row);
    double sum2d = 0;
    for (int k = 0; k < n1 * n2; k++) sum2d += xpp::session().histogram.hist()[2][k];
    CHECK(sum2d > 0.95 && sum2d <= 1.0001); /* nearly all mass inside the box */

    TEST_REPORT("stochast rng/histogram/stacor");
}
