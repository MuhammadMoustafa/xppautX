/* auto_global: the pure parts of how a periodic branch's global end is found
   and named (W518): the period watch, an equilibrium's eigenvalues judged,
   the end the equilibria give and the slow stretches of an orbit. The
   equilibrium search and the stops themselves are tools/servercheck.py's. */
#include "xpptest.h"
#include "auto_global.h"

#include <cmath>
#include <vector>

namespace {

xpp::AutoApproach approach(std::vector<double> re, std::vector<double> im, bool exists)
{
    xpp::AutoApproach a;
    a.exists = exists;
    a.re = std::move(re);
    a.im = std::move(im);
    xpp::auto_approach_judge(a);
    return a;
}

std::vector<std::size_t> slow(const std::vector<double> &speeds, double ratio)
{
    return xpp::auto_slow_points(speeds, ratio);
}

} // namespace

int main()
{
    /* a period that grows over five points in a row, to twice the shortest */
    xpp::AutoPeriodWatch watch;
    bool grown = false;
    for (double period : {6.0, 6.5, 7.0, 8.0, 9.0, 10.0, 11.0}) grown = xpp::auto_period_watch_add(watch, period, period == 6.0);
    CHECK(!grown && watch.growing == 6 && watch.shortest == 6.0); /* growing, but not twice 6 */
    CHECK(xpp::auto_period_watch_add(watch, 12.5, false));
    CHECK(!xpp::auto_period_watch_add(watch, 12.0, false) && watch.growing == 0); /* one step back starts the run again */
    /* a new branch forgets the last one's periods */
    CHECK(!xpp::auto_period_watch_add(watch, 3.0, true) && watch.shortest == 3.0 && watch.growing == 0);

    /* a saddle: unstable and stable eigenvalues of an equilibrium that exists */
    CHECK(approach({1, -1, -2}, {0, 0, 0}, true).kind == xpp::AutoApproachKind::saddle);
    CHECK(approach({1, -1, -2}, {0, 0, 0}, true).unstable == 1 && approach({1, -1, -2}, {0, 0, 0}, true).stable == 2);
    CHECK(approach({1, -1}, {0, 0}, false).kind == xpp::AutoApproachKind::other); /* no equilibrium: no saddle */
    CHECK(approach({-1, -2}, {0, 0}, true).kind == xpp::AutoApproachKind::other); /* a node */
    CHECK(approach({0.5, 0.5}, {2, -2}, true).kind == xpp::AutoApproachKind::other); /* an unstable focus */
    /* a zero eigenvalue is a real one within the tolerance of the spectral radius, an equilibrium or its ghost */
    CHECK(approach({-1e-5, -3}, {0, 0}, false).kind == xpp::AutoApproachKind::zero_eigenvalue);
    CHECK(approach({1e-5, -3}, {0, 0}, true).kind == xpp::AutoApproachKind::zero_eigenvalue);
    CHECK(approach({-1e-1, -3}, {0, 0}, false).kind == xpp::AutoApproachKind::other); /* a slow mode, not zero */
    CHECK(approach({0, -3}, {1e-5, 0}, false).kind == xpp::AutoApproachKind::other); /* an imaginary pair is no fold */

    /* the end the equilibria give */
    const xpp::AutoApproach saddle = approach({1, -1}, {0, 0}, true), fold = approach({0, -1}, {0, 0}, false),
                            node = approach({-1, -2}, {0, 0}, true);
    CHECK(xpp::auto_global_kind({fold}) == xpp::AutoPointKind::Snic);
    CHECK(xpp::auto_global_kind({saddle}) == xpp::AutoPointKind::Homoclinic);
    CHECK(xpp::auto_global_kind({saddle, saddle}) == xpp::AutoPointKind::Heteroclinic);
    CHECK(xpp::auto_global_kind({saddle, fold}) == xpp::AutoPointKind::End);
    CHECK(xpp::auto_global_kind({fold, fold}) == xpp::AutoPointKind::End);
    CHECK(xpp::auto_global_kind({saddle, node}) == xpp::AutoPointKind::End);
    CHECK(xpp::auto_global_kind({}) == xpp::AutoPointKind::End);

    /* the slowest point of each slow stretch of a closed orbit, slowest first */
    CHECK((slow({5, 1, 1e-7, 5, 5, 3e-7, 1e-7, 5}, 1e-6) == std::vector<std::size_t>{2, 6}));
    CHECK((slow({5, 1, 2e-7, 5, 5, 3e-7, 4e-7, 5}, 1e-6) == std::vector<std::size_t>{2, 5}));
    /* the stretch that wraps round from the last point to the first is one stretch */
    CHECK((slow({3e-7, 5, 5, 5, 5, 1e-7}, 1e-6) == std::vector<std::size_t>{5}));
    CHECK(slow({5, 5, 5}, 1e-6).empty());
    CHECK(slow({}, 1e-6).empty());

    TEST_REPORT("auto_global");
}
