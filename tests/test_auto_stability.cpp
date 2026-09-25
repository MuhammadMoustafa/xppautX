/* auto_stability: a point's stability values are its own (W15). What AUTO
   computed belongs to the point it computed it for; a run's first point is
   not computed unless the run restarts from a label of the same kind.
   tools/autocheck.py's stability section runs the same cases on lecar. */
#include "xpptest.h"
#include "auto_stability.h"

#include <cmath>

namespace {

constexpr int N = 2;

/* re/im pairs, as AUTO's doublecomplex array */
const double EIG[2 * N] = {-0.5, 0.25, -2.0, 0.0};  /* lambda */
const double MULT[2 * N] = {1.0, 0.0, 0.3, -0.4};   /* Floquet multipliers */
const double LAB_R[N] = {0.9, 0.9}, LAB_I[N] = {0.4, -0.4}; /* a label's stored values */

bool all_zero(const double *r, const double *i)
{
    for (int k = 0; k < N; k++)
        if (r[k] != 0 || i[k] != 0) return false;
    return true;
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-12; }

} // namespace

int main()
{
    double r[N], i[N];

    /* a run from initial data: its first point is not computed */
    CHECK(auto_stability_run_start(AUTO_STABILITY_STEADY, 1, AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr) == 0);
    r[0] = i[0] = 7;
    CHECK(auto_stability_for(1, 1, N, r, i) == 0);
    CHECK(all_zero(r, i));

    /* a steady state's eigenvalues are kept as e^lambda, for their point only */
    auto_stability_computed(1, 2, N, EIG, AUTO_STABILITY_STEADY);
    CHECK(auto_stability_for(1, 2, N, r, i) == 1);
    CHECK(near(r[0], std::exp(-0.5) * std::cos(0.25)) && near(i[0], std::exp(-0.5) * std::sin(0.25)));
    CHECK(near(r[1], std::exp(-2.0)) && i[1] == 0);
    CHECK(auto_stability_for(-1, -2, N, r, i) == 1); /* either sign: AUTO's stability flags */
    CHECK(auto_stability_for(1, 3, N, r, i) == 0);   /* another point */
    CHECK(all_zero(r, i));
    CHECK(auto_stability_for(2, 2, N, r, i) == 0);   /* another branch */
    CHECK(all_zero(r, i));

    /* multipliers are kept as they are; more asked than computed: zeros */
    auto_stability_computed(-2, 5, N, MULT, AUTO_STABILITY_PERIODIC);
    double r3[N + 1], i3[N + 1];
    CHECK(auto_stability_for(-2, 5, N + 1, r3, i3) == 1);
    CHECK(r3[0] == 1.0 && i3[0] == 0 && r3[1] == 0.3 && i3[1] == -0.4 && r3[2] == 0 && i3[2] == 0);

    /* a new run forgets what the last one computed */
    auto_stability_run_start(AUTO_STABILITY_PERIODIC, 1, AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
    CHECK(auto_stability_for(-2, 5, N, r, i) == 0);

    /* a same-kind restart: the first point is the label's solution */
    CHECK(auto_stability_run_start(AUTO_STABILITY_STEADY, 1, AUTO_STABILITY_STEADY, 3, N, LAB_R, LAB_I) == 1);
    CHECK(auto_stability_for(1, 1, N, r, i) == 1);
    CHECK(r[0] == 0.9 && i[0] == 0.4 && r[1] == 0.9 && i[1] == -0.4);
    CHECK(auto_stability_for(1, 2, N, r, i) == 0); /* only the first point */
    auto_stability_computed(1, 2, N, EIG, AUTO_STABILITY_STEADY);
    CHECK(auto_stability_for(1, 2, N, r, i) == 1 && near(r[1], std::exp(-2.0)));
    CHECK(auto_stability_run_start(AUTO_STABILITY_PERIODIC, 1, AUTO_STABILITY_PERIODIC, 9, N, LAB_R, LAB_I) == 1);
    CHECK(auto_stability_for(-3, 1, N, r, i) == 1 && r[0] == 0.9);
    /* a branch switch at a periodic branch point continues the same orbit */
    CHECK(auto_stability_run_start(AUTO_STABILITY_PERIODIC, -1, AUTO_STABILITY_PERIODIC, 6, N, LAB_R, LAB_I) == 1);
    CHECK(auto_stability_run_start(AUTO_STABILITY_STEADY, -1, AUTO_STABILITY_STEADY, 1, N, LAB_R, LAB_I) == 1);

    /* a change of kind: not computed */
    CHECK(auto_stability_run_start(AUTO_STABILITY_PERIODIC, 1, AUTO_STABILITY_STEADY, 3, N, LAB_R, LAB_I) == 0);
    CHECK(auto_stability_for(-2, 1, N, r, i) == 0 && all_zero(r, i)); /* periodic from a Hopf point */
    CHECK(auto_stability_run_start(AUTO_STABILITY_OTHER, 2, AUTO_STABILITY_STEADY, 2, N, LAB_R, LAB_I) == 0);
    CHECK(auto_stability_for(2, 1, N, r, i) == 0 && all_zero(r, i)); /* two parameters from a limit point */
    CHECK(auto_stability_run_start(AUTO_STABILITY_STEADY, 1, AUTO_STABILITY_PERIODIC, 9, N, LAB_R, LAB_I) == 0);
    /* two-parameter to two-parameter, and boundary value problems, are not steady or periodic */
    CHECK(auto_stability_run_start(AUTO_STABILITY_OTHER, 2, AUTO_STABILITY_OTHER, 9, N, LAB_R, LAB_I) == 0);
    /* a period doubling's switch: the doubled orbit's multipliers are not the label's */
    CHECK(auto_stability_run_start(AUTO_STABILITY_PERIODIC, -1, AUTO_STABILITY_PERIODIC, 7, N, LAB_R, LAB_I) == 0);
    CHECK(auto_stability_run_start(AUTO_STABILITY_PERIODIC, -1, AUTO_STABILITY_PERIODIC, -27, N, LAB_R, LAB_I) == 0);
    CHECK(auto_stability_for(-4, 1, N, r, i) == 0);
    /* but extending the orbit at a period doubling is the orbit itself */
    CHECK(auto_stability_run_start(AUTO_STABILITY_PERIODIC, 1, AUTO_STABILITY_PERIODIC, 7, N, LAB_R, LAB_I) == 1);

    /* what AUTO computes for the first point itself wins over the label's */
    auto_stability_computed(1, 1, N, MULT, AUTO_STABILITY_PERIODIC);
    CHECK(auto_stability_for(1, 1, N, r, i) == 1 && r[1] == 0.3);

    TEST_REPORT("auto_stability");
}
