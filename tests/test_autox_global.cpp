/* autox_global: the pure parts of how a periodic branch's global end is found
   and named (W518): the period watch, an equilibrium's eigenvalues judged,
   the end the equilibria give and the slow stretches of an orbit. The
   equilibrium search and the stops themselves are tools/servercheck.py's. */
#include "xpptest.h"
#include "autox_global.h"
#include "autox_codes.h"

#include <vector>
#include <cstddef>
#include <span>
#include <utility>

namespace {

xpp::AutoxApproach approach(std::vector<double> re, std::vector<double> im, bool exists)
{
    xpp::AutoxApproach a;
    a.exists = exists;
    a.re = std::move(re);
    a.im = std::move(im);
    xpp::autox_approach_judge(a);
    return a;
}

std::vector<std::size_t> slow(const std::vector<double> &speeds, double ratio)
{
    return xpp::autox_slow_points(speeds, ratio);
}

} // namespace

TEST_CASE("test_autox_global")
{
    /* a period that grows over five points in a row, to twice the shortest */
    xpp::AutoxPeriodWatch watch;
    bool grown = false;
    for (double period : {6.0, 6.5, 7.0, 8.0, 9.0, 10.0, 11.0}) grown = xpp::autox_period_watch_add(watch, period, period == 6.0);
    CHECK(!grown && watch.growing == 6 && watch.shortest == 6.0); /* growing, but not twice 6 */
    CHECK(xpp::autox_period_watch_add(watch, 12.5, false));
    CHECK(!xpp::autox_period_watch_add(watch, 12.0, false) && watch.growing == 0); /* one step back starts the run again */
    /* a new branch forgets the last one's periods */
    CHECK(!xpp::autox_period_watch_add(watch, 3.0, true) && watch.shortest == 3.0 && watch.growing == 0);

    /* a saddle: unstable and stable eigenvalues of an equilibrium that exists */
    CHECK(approach({1, -1, -2}, {0, 0, 0}, true).kind == xpp::AutoxApproachKind::saddle);
    CHECK(approach({1, -1, -2}, {0, 0, 0}, true).unstable == 1 && approach({1, -1, -2}, {0, 0, 0}, true).stable == 2);
    CHECK(approach({1, -1}, {0, 0}, false).kind == xpp::AutoxApproachKind::other); /* no equilibrium: no saddle */
    CHECK(approach({-1, -2}, {0, 0}, true).kind == xpp::AutoxApproachKind::other); /* a node */
    CHECK(approach({0.5, 0.5}, {2, -2}, true).kind == xpp::AutoxApproachKind::other); /* an unstable focus */
    /* a zero eigenvalue is a real one within the tolerance of the spectral radius, an equilibrium or its ghost */
    CHECK(approach({-1e-5, -3}, {0, 0}, false).kind == xpp::AutoxApproachKind::zero_eigenvalue);
    CHECK(approach({1e-5, -3}, {0, 0}, true).kind == xpp::AutoxApproachKind::zero_eigenvalue);
    CHECK(approach({-1e-1, -3}, {0, 0}, false).kind == xpp::AutoxApproachKind::other); /* a slow mode, not zero */
    CHECK(approach({0, -3}, {1e-5, 0}, false).kind == xpp::AutoxApproachKind::other); /* an imaginary pair is no fold */

    /* the end the equilibria give */
    const xpp::AutoxApproach saddle = approach({1, -1}, {0, 0}, true), fold = approach({0, -1}, {0, 0}, false),
                            node = approach({-1, -2}, {0, 0}, true);
    CHECK(xpp::autox_global_kind({fold}) == xpp::AutoxPointKind::Snic);
    CHECK(xpp::autox_global_kind({saddle}) == xpp::AutoxPointKind::Homoclinic);
    CHECK(xpp::autox_global_kind({saddle, saddle}) == xpp::AutoxPointKind::Heteroclinic);
    CHECK(xpp::autox_global_kind({saddle, fold}) == xpp::AutoxPointKind::End);
    CHECK(xpp::autox_global_kind({fold, fold}) == xpp::AutoxPointKind::End);
    CHECK(xpp::autox_global_kind({saddle, node}) == xpp::AutoxPointKind::End);
    CHECK(xpp::autox_global_kind({}) == xpp::AutoxPointKind::End);

    /* the slowest point of each slow stretch of a closed orbit, slowest first */
    CHECK((slow({5, 1, 1e-7, 5, 5, 3e-7, 1e-7, 5}, 1e-6) == std::vector<std::size_t>{2, 6}));
    CHECK((slow({5, 1, 2e-7, 5, 5, 3e-7, 4e-7, 5}, 1e-6) == std::vector<std::size_t>{2, 5}));
    /* the stretch that wraps round from the last point to the first is one stretch */
    CHECK((slow({3e-7, 5, 5, 5, 5, 1e-7}, 1e-6) == std::vector<std::size_t>{5}));
    CHECK(slow({5, 5, 5}, 1e-6).empty());
    CHECK(slow({}, 1e-6).empty());

    TEST_REPORT("autox_global");
}
