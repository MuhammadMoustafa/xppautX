/* W496: regressions for XPPAUT findings 79, 85 and 87. */
#include "xpptest.h"
#include "session.h"
#include "xpp_batch.h"
#include "flags.h"
#include "histogram.h"
#include "solver.h"
#include <array>
#include <cmath>

int main()
{
    char *argv[] = {const_cast<char *>("test_numerical_fixes"),
                   const_cast<char *>("tools/models/stops/stiff_singular.odex"), nullptr};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    auto &s = xpp::client_session();
    /* A singular first step must leave the state at its start and report
       that matrix, rather than use an unset hnext or pretend to succeed. */
    const auto &info = xpp::solver_info(xpp::method::STIFF);
    auto solver = info.start(info, s, 1);
    double y = 1, t = 0, h = 1;
    int start = 1;
    auto result = solver->advance({&y, &t, 1, &start, 1, 1, 1, &h});
    CHECK(!result.has_value());
    if (!result) {
        CHECK(result.error().what.find("singular") != std::string::npos);
        CHECK(result.error().what.find("t = 0") != std::string::npos);
    }
    CHECK(y == 1 && t == 0);

    /* A constant pair has Fourier DC coefficients 2 and 3. With an
       eight-sample rectangular window Pxy = 6/8, PSDxy = log((6/8)^2),
       independently of how many identical windows are averaged. */
    std::array<float, 32> a, b;
    a.fill(2); b.fill(3);
    std::array<float, 4> power{};
    CHECK(xpp::cross_spectrum(a, b, 8, 0, power.data(), 1) == 1);
    CHECK(std::fabs(power[0] - std::log(0.75 * 0.75)) < 1e-6);
    CHECK(xpp::cross_spectrum(a, b, 8, 0, power.data(), 2) == 1);
    CHECK(std::fabs(power[0] - 1) < 1e-6);

    /* Establish a previous sample, then land exactly on a root at t=1.
       A second step away from that root must not apply x=x+1 again. */
    for (int sign : {-1, 0, 1}) {
        s.model().flags.clear();
        CHECK(add_global(s, sign == -1 ? "1-t" : "t-1", sign, {{"x", "x+1"}}) == 0);
        CHECK(compile_flags(s) == 0);
        start = 1;
        double old = 0, next = 0, time = 0.5, fraction = 0;
        CHECK(one_flag_step(s, &old, &next, &start, 0, &time, 1, &fraction) == 0);
        time = 1;
        CHECK(one_flag_step(s, &old, &next, &start, 0.5, &time, 1, &fraction) == 1);
        CHECK(time == 1 && fraction == 1 && next == 1);
        if (sign == 0) {
            // Initial sign-zero events (used to initialize random states)
            // still fire, but a second sample at that root does not.
            start = 1;
            old = next = 0;
            time = 1;
            CHECK(one_flag_step(s, &old, &next, &start, 1, &time, 1, &fraction) == 1);
            CHECK(next == 1);
            old = next;
            CHECK(one_flag_step(s, &old, &next, &start, 1, &time, 1, &fraction) == 0);
            CHECK(next == 1);
        }
        old = next;
        time = 1.5;
        CHECK(one_flag_step(s, &old, &next, &start, 1, &time, 1, &fraction) == 0);
        CHECK(next == 1);
        // The fixed-step driver must finish an endpoint hit without
        // asking the solver to take a zero-length remainder.
        time = 0.5;
        next = 0;
        s.integrator.flags[0].f1 = sign == -1 ? 0.5 : -0.5;
        int steps = 0;
        one_flag_steps(s, &next, 1, &time, &start, 0.5, [&](double dt) {
            CHECK(dt > 0);
            time += dt;
            ++steps;
        });
        CHECK(steps == 1 && time == 1 && next == 1);
        // The first step of a fresh run needs the condition at its start;
        // it cannot use the zero or value retained from a preceding run.
        start = 1;
        old = 0;
        next = 0;
        time = 1;
        CHECK(one_flag_step(s, &old, &next, &start, 0, &time, 1, &fraction) == 1);
        CHECK(time == 1 && fraction == 1 && next == 1);
    }
    s.model().flags.clear();
    CHECK(add_global(s, "t", 0, {{"x", "x+1"}}) == 0);
    CHECK(compile_flags(s) == 0);
    start = 1;
    y = t = 0;
    int initial_steps = 0;
    one_flag_steps(s, &y, 1, &t, &start, 0, [&](double dt) {
        CHECK(dt == 0);
        ++initial_steps;
    });
    CHECK(initial_steps == 1 && y == 1 && t == 0);
    TEST_REPORT("numerical fixes");
}
