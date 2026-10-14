/* auto_stability: a point's stability values are its own (W15). What AUTO
   computed belongs to the point it computed it for; a run's first point is
   not computed unless the run restarts from a label of the same kind.
   tools/autocheck.py's stability section runs the same cases on lecar. */
#include "xpptest.h"
#include "auto_stability.h"
#include "auto_stability_auto.h"

#include <cmath>
#include <array>
#include "session.h"
#include "auto_c.h"
#include "autevd.h"
#include "xpp_batch.h"
#include "xpp_files.h"

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

// Finite, nonsingular derivatives, but a residual that Newton cannot reduce.
int no_convergence(const iap_type *, const rap_type *, integer, const double *u,
                   const double *, const integer *icp, double *, integer,
                   double *f, double *dfdu, double *dfdp)
{
    f[0] = 1;
    dfdu[0] = u[0] + 1;
    dfdp[icp[0]] = -1;
    return 0;
}
int boundary(const iap_type *, const rap_type *, integer, double *,
             const integer *, integer, const double *u0, const double *,
             double *f, integer, double *dbc)
{
    f[0] = u0[0];
    dbc[0] = 1;
    return 0;
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-12; }

} // namespace

int main()
{
    xpp::AutoStability st;
    double r[N], i[N];

    /* a run from initial data: its first point is not computed */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_STEADY, 1, xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr) == 0);
    r[0] = i[0] = 7;
    CHECK(xpp::auto_stability_for(st, 1, 1, N, r, i) == 0);
    CHECK(all_zero(r, i));

    /* a steady state's eigenvalues are kept as e^lambda, for their point only */
    xpp::auto_stability_computed(st, 1, 2, N, EIG, xpp::AUTO_STABILITY_STEADY);
    CHECK(xpp::auto_stability_for(st, 1, 2, N, r, i) == 1);
    CHECK(near(r[0], std::exp(-0.5) * std::cos(0.25)) && near(i[0], std::exp(-0.5) * std::sin(0.25)));
    CHECK(near(r[1], std::exp(-2.0)) && i[1] == 0);
    CHECK(xpp::auto_stability_for(st, -1, -2, N, r, i) == 1); /* either sign: AUTO's stability flags */
    CHECK(xpp::auto_stability_for(st, 1, 3, N, r, i) == 0);   /* another point */
    CHECK(all_zero(r, i));
    CHECK(xpp::auto_stability_for(st, 2, 2, N, r, i) == 0);   /* another branch */
    CHECK(all_zero(r, i));

    /* multipliers are kept as they are; more asked than computed: zeros */
    xpp::auto_stability_computed(st, -2, 5, N, MULT, xpp::AUTO_STABILITY_PERIODIC);
    double r3[N + 1], i3[N + 1];
    CHECK(xpp::auto_stability_for(st, -2, 5, N + 1, r3, i3) == 1);
    CHECK(r3[0] == 1.0 && i3[0] == 0 && r3[1] == 0.3 && i3[1] == -0.4 && r3[2] == 0 && i3[2] == 0);

    /* a new run forgets what the last one computed */
    xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, 1, xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
    CHECK(xpp::auto_stability_for(st, -2, 5, N, r, i) == 0);

    /* a same-kind restart: the first point is the label's solution */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_STEADY, 1, xpp::AUTO_STABILITY_STEADY, 3, N, LAB_R, LAB_I) == 1);
    CHECK(xpp::auto_stability_for(st, 1, 1, N, r, i) == 1);
    CHECK(r[0] == 0.9 && i[0] == 0.4 && r[1] == 0.9 && i[1] == -0.4);
    CHECK(xpp::auto_stability_for(st, 1, 2, N, r, i) == 0); /* only the first point */
    xpp::auto_stability_computed(st, 1, 2, N, EIG, xpp::AUTO_STABILITY_STEADY);
    CHECK(xpp::auto_stability_for(st, 1, 2, N, r, i) == 1 && near(r[1], std::exp(-2.0)));
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, 1, xpp::AUTO_STABILITY_PERIODIC, 9, N, LAB_R, LAB_I) == 1);
    CHECK(xpp::auto_stability_for(st, -3, 1, N, r, i) == 1 && r[0] == 0.9);
    /* a branch switch at a periodic branch point continues the same orbit */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, -1, xpp::AUTO_STABILITY_PERIODIC, 6, N, LAB_R, LAB_I) == 1);
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_STEADY, -1, xpp::AUTO_STABILITY_STEADY, 1, N, LAB_R, LAB_I) == 1);

    /* a change of kind: not computed */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, 1, xpp::AUTO_STABILITY_STEADY, 3, N, LAB_R, LAB_I) == 0);
    CHECK(xpp::auto_stability_for(st, -2, 1, N, r, i) == 0 && all_zero(r, i)); /* periodic from a Hopf point */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_OTHER, 2, xpp::AUTO_STABILITY_STEADY, 2, N, LAB_R, LAB_I) == 0);
    CHECK(xpp::auto_stability_for(st, 2, 1, N, r, i) == 0 && all_zero(r, i)); /* two parameters from a limit point */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_STEADY, 1, xpp::AUTO_STABILITY_PERIODIC, 9, N, LAB_R, LAB_I) == 0);
    /* two-parameter to two-parameter, and boundary value problems, are not steady or periodic */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_OTHER, 2, xpp::AUTO_STABILITY_OTHER, 9, N, LAB_R, LAB_I) == 0);
    /* a period doubling's switch: the doubled orbit's multipliers are not the label's */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, -1, xpp::AUTO_STABILITY_PERIODIC, 7, N, LAB_R, LAB_I) == 0);
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, -1, xpp::AUTO_STABILITY_PERIODIC, -27, N, LAB_R, LAB_I) == 0);
    CHECK(xpp::auto_stability_for(st, -4, 1, N, r, i) == 0);
    /* but extending the orbit at a period doubling is the orbit itself */
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, 1, xpp::AUTO_STABILITY_PERIODIC, 7, N, LAB_R, LAB_I) == 1);

    /* what AUTO computes for the first point itself wins over the label's */
    xpp::auto_stability_computed(st, 1, 1, N, MULT, xpp::AUTO_STABILITY_PERIODIC);
    CHECK(xpp::auto_stability_for(st, 1, 1, N, r, i) == 1 && r[1] == 0.3);

    // W341 (#402): the endpoint repeats the predictor's old solution, not
    // a failed trial. Keep its exact spectrum even if a trial overwrites it.
    for (int kind : {xpp::AUTO_STABILITY_STEADY, xpp::AUTO_STABILITY_PERIODIC}) {
        xpp::auto_stability_run_start(st, kind, 1, xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
        xpp::auto_stability_computed(st, -2, 5, N, MULT, kind);
        double old_r[N], old_i[N];
        CHECK(xpp::auto_stability_for(st, 2, 5, N, old_r, old_i) == 1);
        xpp::auto_stability_step_start(st, 2, 5);
        CHECK(xpp::auto_stability_for(st, 2, 6, N, r, i) == 0 && all_zero(r, i));
        xpp::auto_stability_computed(st, 2, 6, N, EIG, kind);
        xpp::auto_stability_step_failed(st, -2, -6);
        CHECK(xpp::auto_stability_for(st, 2, 6, N, r, i) == 1);
        for (int k = 0; k < N; ++k) CHECK(r[k] == old_r[k] && i[k] == old_i[k]);
        // A location's predictor starts from an unstored solution: retain
        // that solution's spectrum, not the previous diagram row's.
        xpp::auto_stability_step_start(st, 2, 6);
        xpp::auto_stability_computed(st, 2, 6, N, EIG, kind);
        xpp::auto_stability_step_failed(st, 2, 6);
        CHECK(xpp::auto_stability_for(st, 2, 6, N, r, i) == 1);
        for (int k = 0; k < N; ++k) CHECK(r[k] == old_r[k] && i[k] == old_i[k]);
        // A different branch's solution must never inherit these values.
        xpp::auto_stability_step_start(st, 3, 6);
        xpp::auto_stability_step_failed(st, 3, 7);
        CHECK(xpp::auto_stability_for(st, 3, 7, N, r, i) == 0 && all_zero(r, i));
    }
    CHECK(xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, 1,
          xpp::AUTO_STABILITY_PERIODIC, 9, N, LAB_R, LAB_I) == 1);
    xpp::auto_stability_step_start(st, 4, 1);
    xpp::auto_stability_step_failed(st, 4, 2);
    CHECK(xpp::auto_stability_for(st, 4, 2, N, r, i) == 1);
    for (int k = 0; k < N; ++k) CHECK(r[k] == LAB_R[k] && i[k] == LAB_I[k]);
    CHECK(xpp::auto_stability_for(st, 5, 1, N, r, i) == 0 && all_zero(r, i));
    xpp::auto_stability_run_start(st, xpp::AUTO_STABILITY_PERIODIC, 1,
          xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
    xpp::auto_stability_step_failed(st, 4, 2);
    CHECK(xpp::auto_stability_for(st, 4, 2, N, r, i) == 0 && all_zero(r, i));

    // Exercise the real restoration seams, including DSMIN retries. The
    // spectrum is computed for the exact solution each predictor saves.
    char name[] = "test_auto_stability", file[] = "examples/ode/lecar.odex";
    char *argv[] = {name, file, nullptr};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    auto &session = xpp::client_session();
    xpp::TempDir temp;
    xpp::UniqueFile diagnostics(xpp::files::open_stream(temp.path() + "/stability.log", "w"));
    auto &lib = session.auto_lib;
    lib.session = &session;
    lib.parameter_count = session.auto_state.parameter_layout.size();
    lib.fp9 = diagnostics.get();
    iap_type iap{};
    iap.lib = &lib;
    iap.ibr = 1;
    iap.ntot = 5;
    iap.ndim = iap.ndm = iap.nfpr = 1;
    iap.itnw = 4; // adptds reduces failed steps only when ITNW is at least 3.
    iap.iads = 1;
    rap_type rap{};
    rap.ds = rap.dsold = rap.dsmax = 1;
    rap.dsmin = 0.2;
    rap.epsl = rap.epsu = 1e-8;
    integer icp = 0, stride = 2;
    double rlcur = 2, rlold = 1, rldot = 1;
    double u = 2, uold = 1, udot = 1, f = 0, dfdu = 1, thu = 1, ds = 1;
    std::array<double, 4> aa{};
    std::array<double, 2> rhs{}, du{};
    std::vector<double> par(lib.parameter_count), dfdp(lib.parameter_count), thl(lib.parameter_count, 1);
    auto &actual = session.auto_state.stability;
    for (bool switching : {false, true}) {
        iap.istop = 0;
        iap.ips = switching ? 1 : 0;
        ds = rap.dsold = 1;
        u = rlcur = par[0] = 2;
        uold = rlold = 1;
        udot = rldot = 1;
        xpp::auto_stability_run_start(actual, xpp::AUTO_STABILITY_STEADY, 1,
            xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
        const double switch_eigenvalue[] = {3, 0};
        xpp::auto_stability_computed(actual, 1, 5, switching ? 1 : N,
            switching ? switch_eigenvalue : EIG, xpp::AUTO_STABILITY_STEADY);
        double expected_r[N], expected_i[N];
        CHECK(xpp::auto_stability_for(actual, 1, 5, N, expected_r, expected_i) == 1);
        if (switching) {
            // swpnt supplies the branch point as both u and uold.
            uold = rlold = 2;
            swprc(&iap, &rap, par.data(), &icp, no_convergence, &stride,
                aa.data(), rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(),
                &uold, &udot, &f, &dfdu, dfdp.data(), &ds, thl.data(), &thu);
        } else {
            contae(&iap, &rap, &ds, &rlcur, &rlold, &rldot, &u, &uold, &udot);
            solvae(&iap, &rap, par.data(), &icp, no_convergence, &ds, &stride,
                aa.data(), rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(),
                &uold, &udot, &f, &dfdu, dfdp.data(), thl.data(), &thu);
        }
        CHECK(iap.istop == 1 && ds < rap.dsmin && u == 2 && uold == 2 && par[0] == 2);
        CHECK(xpp::auto_stability_for(actual, 1, 6, N, r, i) == 1);
        for (int k = 0; k < N; ++k) CHECK(r[k] == expected_r[k] && i[k] == expected_i[k]);
    }
    // A failed equilibrium location also returns to an unstored point.
    xpp::auto_stability_run_start(actual, xpp::AUTO_STABILITY_STEADY, 1,
        xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
    lib.locating = 1;
    iap.ips = 1;
    iap.istop = 0;
    ds = rap.dsold = 1;
    u = rlcur = par[0] = 2;
    uold = rlold = 1;
    udot = rldot = 1;
    contae(&iap, &rap, &ds, &rlcur, &rlold, &rldot, &u, &uold, &udot);
    solvae(&iap, &rap, par.data(), &icp, no_convergence, &ds, &stride,
        aa.data(), rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(),
        &uold, &udot, &f, &dfdu, dfdp.data(), thl.data(), &thu);
    CHECK(iap.istop == 1 && ds < rap.dsmin && u == 2 && par[0] == 2);
    CHECK(xpp::auto_stability_for(actual, 1, 6, 1, r, i) == 1 && near(r[0], std::exp(3.0)) && i[0] == 0);
    lib.locating = 0;

    // stprae's successful seed is unstored: the following contae sees
    // NTOT=1 again, but its u is no longer the restart label's solution.
    xpp::auto_stability_run_start(actual, xpp::AUTO_STABILITY_STEADY, 1,
        xpp::AUTO_STABILITY_STEADY, 3, N, LAB_R, LAB_I);
    xpp::auto_stability_step_start(actual, 1, 1); // stprae consumed the label.
    iap.ntot = 1;
    iap.istop = 0;
    ds = rap.dsold = 1;
    u = rlcur = par[0] = 2; // the converged seed, with a different Jacobian.
    uold = rlold = 1;
    udot = rldot = 1;
    contae(&iap, &rap, &ds, &rlcur, &rlold, &rldot, &u, &uold, &udot);
    solvae(&iap, &rap, par.data(), &icp, no_convergence, &ds, &stride,
        aa.data(), rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(),
        &uold, &udot, &f, &dfdu, dfdp.data(), thl.data(), &thu);
    CHECK(iap.istop == 1 && ds < rap.dsmin && u == 2 && par[0] == 2);
    CHECK(xpp::auto_stability_for(actual, 1, 2, 1, r, i) == 1 && near(r[0], std::exp(3.0)) && i[0] == 0);

    // A switched branch may return to an earlier BP, absent from the last
    // record. Its restored Jacobian, not a failed trial's, supplies stability.
    xpp::auto_stability_run_start(actual, xpp::AUTO_STABILITY_STEADY, 1,
        xpp::AUTO_STABILITY_STEADY, 3, N, LAB_R, LAB_I);
    iap.ntot = 1; // BR/PT matches the old restart label, but the solution differs.
    iap.ips = 1;
    iap.istop = 0;
    ds = 1;
    u = rlcur = par[0] = 2;
    udot = rldot = 1;
    const double saved_period = par[10];
    swprc(&iap, &rap, par.data(), &icp, no_convergence, &stride,
        aa.data(), rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(),
        &uold, &udot, &f, &dfdu, dfdp.data(), &ds, thl.data(), &thu);
    CHECK(iap.istop == 1 && ds < rap.dsmin && u == 2 && par[0] == 2 && par[10] == saved_period);
    CHECK(xpp::auto_stability_for(actual, 1, 2, 1, r, i) == 1 && near(r[0], std::exp(3.0)) && i[0] == 0);

    iap.istop = 0;
    iap.ntot = 5;
    iap.ips = 4;
    iap.ntst = iap.nbc = iap.nwtn = 1;
    iap.ncol = 2;
    std::array<double, 4> mesh{2, 2, 2, 0}, old{}, tangent{1, 1, 1, 0};
    const auto solution = mesh;
    std::array<double, 4> dups{}, upold{}, fa{};
    std::array<double, 2> fc{}, tm{0, 1}, dtm{1, 0};
    double p0 = 0, p1 = 0;
    ds = 1;
    rlcur = par[0] = 2;
    xpp::auto_stability_run_start(actual, xpp::AUTO_STABILITY_PERIODIC, 1,
        xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
    xpp::auto_stability_computed(actual, 1, 5, N, MULT, xpp::AUTO_STABILITY_PERIODIC);
    extrbv(&iap, &rap, no_convergence, &ds, &rlcur, &rlold, &rldot,
           &stride, mesh.data(), old.data(), tangent.data());
    stepbv(&iap, &rap, par.data(), &icp, no_convergence, boundary, nullptr, nullptr,
           &ds, &rlcur, &rlold, &rldot, &stride, mesh.data(), dups.data(), old.data(),
           tangent.data(), upold.data(), fa.data(), fc.data(), tm.data(), dtm.data(),
           &p0, &p1, thl.data(), &thu);
    CHECK(iap.istop == 1 && ds < rap.dsmin && mesh == solution && old == solution && par[0] == 2);
    CHECK(xpp::auto_stability_for(actual, 1, 6, N, r, i) == 1);
    for (int k = 0; k < N; ++k) CHECK(r[k] == MULT[2*k] && i[k] == MULT[2*k+1]);
    // The restored solution in a location can be unstored and have no
    // spectrum yet. Compute a reference pencil at the old solution; the
    // state-dependent Jacobian makes a failed trial's pencil different.
    xpp::auto_stability_run_start(actual, xpp::AUTO_STABILITY_PERIODIC, 1,
        xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
    iap.ips = iap.isp = 2;
    iap.istop = 0;
    ds = 1;
    rlcur = par[0] = 2;
    mesh = solution;
    old = solution;
    rlold = rlcur;
    integer fresh = 1, nullity = 0;
    double zero_step = 0;
    solvbv(&fresh, &iap, &rap, par.data(), &icp, no_convergence, boundary,
           nullptr, &zero_step, &nullity, &rlcur, &rlold, &rldot, &stride,
           mesh.data(), dups.data(), old.data(), tangent.data(), upold.data(),
           dtm.data(), fa.data(), fc.data(), &p0, &p1, thl.data(), &thu);
    xpp::auto_stability_multipliers_restored(actual, iap, &p0, &p1);
    CHECK(xpp::auto_stability_for(actual, 1, 6, 1, r, i) == 1);
    const double expected = r[0];
    CHECK(std::isfinite(expected) && i[0] == 0);
    // This reference is not retained for the attempted endpoint.
    xpp::auto_stability_run_start(actual, xpp::AUTO_STABILITY_PERIODIC, 1,
        xpp::AUTO_STABILITY_NONE, 0, 0, nullptr, nullptr);
    extrbv(&iap, &rap, no_convergence, &ds, &rlcur, &rlold, &rldot,
           &stride, mesh.data(), old.data(), tangent.data());
    stepbv(&iap, &rap, par.data(), &icp, no_convergence, boundary, nullptr, nullptr,
           &ds, &rlcur, &rlold, &rldot, &stride, mesh.data(), dups.data(), old.data(),
           tangent.data(), upold.data(), fa.data(), fc.data(), tm.data(), dtm.data(),
           &p0, &p1, thl.data(), &thu);
    CHECK(iap.istop == 1 && ds < rap.dsmin && mesh == solution && par[0] == 2);
    CHECK(xpp::auto_stability_for(actual, 1, 6, 1, r, i) == 1 && r[0] == expected && i[0] == 0);
    // Full pencil spectrum: a collapsed orbit need not have multiplier +1.
    // This exact rotation pencil has multipliers +0.75i and -0.75i.
    iap.ndim = 2;
    std::array<double, 4> rotation{0, 0.75, -0.75, 0}, minus_identity{-1, 0, 0, -1};
    xpp::auto_stability_multipliers_restored(actual, iap, rotation.data(), minus_identity.data());
    CHECK(xpp::auto_stability_for(actual, 1, 6, N, r, i) == 1);
    CHECK(near(r[0], 0) && near(r[1], 0) && near(std::fabs(i[0]), 0.75) && near(i[0], -i[1]));
    lib.fp9 = nullptr;

    TEST_REPORT("auto_stability");
}
