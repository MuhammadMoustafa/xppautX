/* autox_ends: the pure parts of the two ends W604 adds to a branch and of the rule that says two special points
   are one: a cycle's size, the amplitude watch, the same-point rule. The Hopf equilibrium search and the stops
   themselves are tools/servercheck.py's. */
#include "xpptest.h"
#include "autox_ends.h"
#include "autox_global.h"

#include <cmath>
#include <vector>

namespace {

xpp::AutoxPointId point(xpp::AutoxPointKind kind, std::vector<double> parameters, std::vector<double> states,
                        std::optional<double> frequency = std::nullopt)
{
    return {kind, std::move(parameters), std::move(states), frequency};
}

} // namespace

int main()
{
    /* the trend counter the period and the amplitude watches share: one more when the series goes on the same way, else 0 */
    CHECK(xpp::autox_trend_run(2, 1.0, 2.0, true) == 3);
    CHECK(xpp::autox_trend_run(2, 2.0, 1.0, true) == 0);
    CHECK(xpp::autox_trend_run(2, 2.0, 1.0, false) == 3);
    CHECK(xpp::autox_trend_run(2, 2.0, 2.0, false) == 0); /* no change breaks the run */
    CHECK(xpp::autox_trend_run(0, NAN, 1.0, true) == 0 && xpp::autox_trend_run(0, NAN, 1.0, false) == 0); /* a first point has no trend */

    /* a cycle's size: the largest range of its variables, each against 1 + its mean */
    const std::vector<double> high = {3, 12}, low = {1, 10}, mean = {1, 0};
    CHECK(std::abs(xpp::autox_orbit_amplitude(high, low, mean) - 2.0) < 1e-15); /* the larger of 2 / (1 + 1) and 2 / (1 + 0) */
    CHECK(xpp::autox_orbit_amplitude(mean, mean, mean) == 0);

    /* the watch: the amplitude has fallen over three points in a row and is at most the limit */
    xpp::AutoxAmplitudeWatch watch;
    const double small = xpp::autox_hopf_end_amplitude / 2;
    CHECK(!xpp::autox_amplitude_watch_add(watch, 1.0, 0.0, true));
    CHECK(!xpp::autox_amplitude_watch_add(watch, 0.5, 0.0, false) && watch.shrinking == 1);
    CHECK(!xpp::autox_amplitude_watch_add(watch, 0.1, 0.0, false));
    CHECK(!xpp::autox_amplitude_watch_add(watch, 0.01, 0.0, false)); /* fallen three times, still above the limit */
    CHECK(xpp::autox_amplitude_watch_add(watch, small, 0.0, false));
    /* a branch that starts at about zero and grows, as one from a Hopf point does, is no end */
    CHECK(!xpp::autox_amplitude_watch_add(watch, small / 4, 0.0, true));
    CHECK(!xpp::autox_amplitude_watch_add(watch, small / 2, 0.0, false) && watch.shrinking == 0);
    CHECK(!xpp::autox_amplitude_watch_add(watch, small, 0.0, false));
    /* one that falls fast from the start: the first point only starts the count */
    CHECK(!xpp::autox_amplitude_watch_add(watch, small, 0.0, true));
    CHECK(!xpp::autox_amplitude_watch_add(watch, small / 2, 0.0, false));
    CHECK(!xpp::autox_amplitude_watch_add(watch, small / 4, 0.0, false));
    CHECK(xpp::autox_amplitude_watch_add(watch, small / 8, 0.0, false));


    /* the Hopf parameter: the squared amplitude is linear in the parameter, so the last two cycles' line reaches zero
       where a^2 = k (p0 - p): a = 2 at p = 8 and a = 1 at p = 11 give k = 1 and p0 = 12 */
    xpp::AutoxAmplitudeWatch line;
    CHECK(!xpp::autox_hopf_parameter(line)); /* no cycles yet */
    xpp::autox_amplitude_watch_add(line, 2.0, 8.0, true);
    CHECK(!xpp::autox_hopf_parameter(line));
    xpp::autox_amplitude_watch_add(line, 1.0, 11.0, false);
    CHECK(std::abs(*xpp::autox_hopf_parameter(line) - 12.0) < 1e-12);
    /* points below the limit are no cycles: the line stays through the last two above it */
    xpp::autox_amplitude_watch_add(line, small / 10, 12.0, false);
    CHECK(std::abs(*xpp::autox_hopf_parameter(line) - 12.0) < 1e-12);
    /* two cycles that do not shrink give no line */
    xpp::AutoxAmplitudeWatch growing;
    xpp::autox_amplitude_watch_add(growing, 1.0, 8.0, true);
    xpp::autox_amplitude_watch_add(growing, 2.0, 11.0, false);
    CHECK(!xpp::autox_hopf_parameter(growing));
    /* the same kind: equal, or an end and the point it ends at */
    CHECK(xpp::autox_same_kind(xpp::AutoxPointKind::Hopf, xpp::AutoxPointKind::Hopf) && xpp::autox_same_kind(xpp::AutoxPointKind::HopfEnd, xpp::AutoxPointKind::Hopf) && xpp::autox_same_kind(xpp::AutoxPointKind::Hopf, xpp::AutoxPointKind::HopfEnd));
    CHECK(xpp::autox_same_kind(xpp::AutoxPointKind::Snic, xpp::AutoxPointKind::Fold) && xpp::autox_same_kind(xpp::AutoxPointKind::Fold, xpp::AutoxPointKind::Snic));
    CHECK(!xpp::autox_same_kind(xpp::AutoxPointKind::Snic, xpp::AutoxPointKind::Hopf) && !xpp::autox_same_kind(xpp::AutoxPointKind::HopfEnd, xpp::AutoxPointKind::Fold) && !xpp::autox_same_kind(xpp::AutoxPointKind::Fold, xpp::AutoxPointKind::Hopf));
    CHECK(!xpp::autox_same_kind(xpp::AutoxPointKind::Homoclinic, xpp::AutoxPointKind::Fold));

    /* the rule: every parameter and state within c * epss * (1 + |a|) */
    const double epss = 1e-4, tolerance = xpp::autox_same_point_factor * epss;
    const auto hopf = point(xpp::AutoxPointKind::Hopf, {10.0}, {100.0, -2.0}, 2.0);
    CHECK(xpp::autox_point_gap(hopf, hopf, epss) == 0);
    CHECK(xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::HopfEnd, {10.0}, {100.0, -2.0}, 2.0), epss));
    CHECK(xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::Hopf, {10.0 + 0.9 * tolerance * 11}, {100.0, -2.0}, 2.0), epss)); /* a parameter just within */
    CHECK(!xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::Hopf, {10.0 + 1.1 * tolerance * 11}, {100.0, -2.0}, 2.0), epss)); /* just beyond */
    CHECK(xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::Hopf, {10.0}, {100.0 + 0.9 * tolerance * 101, -2.0}, 2.0), epss)); /* a state: relative to 1 + |a| */
    CHECK(!xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::Hopf, {10.0}, {100.0, -2.0 + 1.1 * tolerance * 3}, 2.0), epss));
    CHECK(xpp::autox_point_gap(hopf, point(xpp::AutoxPointKind::Hopf, {10.0}, {100.0, -2.0 + tolerance * 3 / 2}, 2.0), epss) > 0.49 * xpp::autox_same_point_factor);
    /* a Hopf point also matches its frequency, where both have one */
    CHECK(!xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::HopfEnd, {10.0}, {100.0, -2.0}, 2.5), epss));
    CHECK(xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::HopfEnd, {10.0}, {100.0, -2.0}), epss));
    /* another kind, or another size, is never the same */
    CHECK(!xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::Fold, {10.0}, {100.0, -2.0}, 2.0), epss));
    CHECK(!xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::Hopf, {10.0, 1.0}, {100.0, -2.0}, 2.0), epss));
    CHECK(!xpp::autox_same_point(hopf, point(xpp::AutoxPointKind::Hopf, {10.0}, {100.0}, 2.0), epss));
    CHECK(std::isinf(xpp::autox_point_gap(hopf, point(xpp::AutoxPointKind::Fold, {10.0}, {100.0, -2.0}, 2.0), epss)));
    /* the closing test's unit is at most the cycle's own size (W687): a state off by 4 units of 1 + |a| = 51, EPSS 1e-4, is the same
       point, but off by 4000 units of a cycle of range 0.05 it is not (ode_motoneuron6c's -7, -49.340 against -49.360) */
    const auto cycle = point(xpp::AutoxPointKind::Regular, {0.19427}, {-49.340, -49.392}, std::nullopt);
    const auto other = point(xpp::AutoxPointKind::Regular, {0.19427}, {-49.360, -49.372}, std::nullopt);
    CHECK(xpp::autox_same_point(cycle, other, epss));
    CHECK(xpp::autox_point_gap(cycle, other, epss, 0.052) > 1000 * xpp::autox_same_point_factor / 8);
    CHECK(xpp::autox_point_gap(cycle, other, epss, INFINITY) == xpp::autox_point_gap(cycle, other, epss));
    CHECK(xpp::autox_point_gap(cycle, cycle, epss, 0.052) == 0);

    TEST_REPORT("autox_ends");
}
