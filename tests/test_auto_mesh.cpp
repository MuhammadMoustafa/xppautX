/* W361 (#423): mesh endpoints and the unaccumulated monitor density. */
#include "xpptest.h"
#include "auto_c.h"
#include <array>
#include <cmath>

int main()
{
    iap_type iap{};
    rap_type rap{};
    integer ndim = 1, intervals = 4, columns = 1, leading = 5, periodic = 0;
    iap.ndim = ndim;
    std::array<double, 5> times{0., .25, .5, .75, 1.};
    std::array<double, 5> widths{.25, .25, .25, .25, 0.};
    std::array<double, 5> values{0., 1e30, 0., 1., 2.1};
    std::array<double, 5> monitor{};
    std::array<double, 4> density{};
    eqdf(&iap, &rap, &intervals, &ndim, &columns, widths.data(), &leading,
         values.data(), monitor.data(), density, &periodic);
    // The final two slopes are 4 and 4.4: u''=1.6, density=sqrt(1.6).
    CHECK(std::abs(density[2] - std::sqrt(1.6)) < 1e-14);
    CHECK(density[2] == density[3]);
    CHECK((monitor[3] - monitor[2]) / widths[2] != density[2]);
    integer new_intervals = 7;
    std::array<double, 8> mesh{};
    newmsh(&iap, &rap, &leading, values.data(), &intervals, &columns,
           times.data(), widths.data(), &new_intervals, mesh.data(), &periodic);
    CHECK(mesh.front() == times.front());
    CHECK(mesh.back() == times.back());
    for (integer j = 1; j <= new_intervals; ++j) CHECK(mesh[j] > mesh[j - 1]);
    // A constant solution has no derivatives. Its monitor is the interval
    // index, so the density must account for nonuniform interval widths.
    values.fill(0.);
    times = {0., .125, .375, .5, 1.};
    widths = {.125, .25, .125, .5, 0.};
    eqdf(&iap, &rap, &intervals, &ndim, &columns, widths.data(), &leading,
         values.data(), monitor.data(), density, &periodic);
    for (integer j = 0; j < intervals; ++j) {
        CHECK(monitor[j] == j);
        CHECK(density[j] == 1. / widths[j]);
    }
    new_intervals = intervals;
    newmsh(&iap, &rap, &leading, values.data(), &intervals, &columns,
           times.data(), widths.data(), &new_intervals, mesh.data(), &periodic);
    for (integer j = 0; j <= intervals; ++j) CHECK(mesh[j] == times[j]);
    // Wave trains (IPS 12) use the same periodic derivative extension as IPS 2.
    // Two collocation columns: endpoint values are zero; midpoint values
    // give exact second derivatives [4,8,-4,-8] on the four intervals.
    columns = 2;
    iap.ncol = columns;
    iap.isw = 1;
    times = {0., .25, .5, .75, 1.};
    widths = {.25, .25, .25, .25, 0.};
    std::array<double, 10> orbit{0.,0.,0.,0.,0., -.03125,-.0625,.03125,.0625,0.};
    periodic = 1;
    eqdf(&iap, &rap, &intervals, &ndim, &columns, widths.data(), &leading,
         orbit.data(), monitor.data(), density, &periodic);
    CHECK(std::abs(density.back() - std::cbrt(48.)) < 1e-14);
    periodic = 0;
    eqdf(&iap, &rap, &intervals, &ndim, &columns, widths.data(), &leading,
         orbit.data(), monitor.data(), density, &periodic);
    CHECK(std::abs(density.back() - std::cbrt(16.)) < 1e-14);
    auto wave_times = times, wave_widths = widths;
    auto wave_orbit = orbit, tangent = orbit, wave_tangent = orbit;
    iap.ips = xpp::AutoProblem::Periodic;
    adapt(&iap, &rap, &intervals, &columns, &intervals, &columns,
          times.data(), widths.data(), &leading, orbit.data(), tangent.data());
    iap.ips = xpp::AutoProblem::WavePeriodic;
    adapt(&iap, &rap, &intervals, &columns, &intervals, &columns,
          wave_times.data(), wave_widths.data(), &leading, wave_orbit.data(), wave_tangent.data());
    CHECK(times == wave_times);
    CHECK(widths == wave_widths);
    CHECK(orbit == wave_orbit);
    CHECK(tangent == wave_tangent);
    TEST_REPORT("auto_mesh");
}
