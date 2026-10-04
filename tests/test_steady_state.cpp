#include "xpptest.h"
#include "integrate.h"
#include <array>
#include <limits>

int main()
{
    xpp::SteadyStateMonitor monitor({9,1,10});
    std::array<double,2> state{1.0000000001,2};
    monitor.begin(state,0);
    state[0]=1.0000000002;
    CHECK(!monitor.observe(state,0.5)); // Changes below the requested displayed precision.
    CHECK(monitor.observe(state,1));
    monitor.begin(state,0);
    state[1]=2.000000001;
    CHECK(!monitor.observe(state,0.5)); // Every state must match.
    CHECK(!monitor.observe(state,1)); // A changed digit resets the entire hold.
    CHECK(monitor.observe(state,1.5));
    monitor.begin(state,0);
    state[0]=1.0000000006;
    CHECK(!monitor.observe(state,1)); // Crossing the rounding boundary matters.
    CHECK(!monitor.observe(state,1)); // Time must advance.
    state[0]=std::numeric_limits<double>::infinity();
    CHECK(!monitor.observe(state,2));
    state={-0.0000000001,0};
    monitor.begin(state,0);
    state[0]=0.0000000001;
    CHECK(monitor.observe(state,1)); // Signed rounded zero is numerically equal.
    CHECK(xpp::validate_steady_state({9,1,10},0.01,0).has_value());
    CHECK(!xpp::validate_steady_state({16,1,10},0.01,0));
    CHECK(!xpp::validate_steady_state({-1,1,10},0.01,0));
    CHECK(!xpp::validate_steady_state({9,0.001,10},0.01,0));
    CHECK(!xpp::validate_steady_state({9,1,0.5},0.01,0));
    CHECK(!xpp::validate_steady_state({9,1,10},0,0));
    CHECK(!xpp::validate_steady_state({9,1,10},-0.01,0));
    CHECK(!xpp::validate_steady_state({9,1,10},1e-20,0));
    CHECK(!xpp::validate_steady_state({9,1,10},0.01,1e300));
    CHECK(!xpp::validate_steady_state({9,1,std::numeric_limits<double>::infinity()},0.01,0));
    TEST_REPORT("steady state");
}
