/* W326 (#387): an exhausted location must not become a continuation step. */
#include "load_args.h"
#include "model_text.h"
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include <array>
#include <optional>
#include <tuple>
#include <limits>
#include <cmath>
#include "autevd.h"
#include "auto_settings.h"
#include "auto_nox.h"
#include "auto_run.h"
#include "diagram.h"

namespace {
int line(const iap_type *, const rap_type *, integer, const double *u,
         const double *, const integer *icp, double *par, integer,
         double *f, double *dfdu, double *dfdp)
{
    f[0] = u[0] - par[icp[0]];
    dfdu[0] = 1;
    dfdp[icp[0]] = -1;
    return 0;
}
int nonfinite_line(const iap_type *, const rap_type *, integer, const double *,
         const double *, const integer *icp, double *, integer,
         double *f, double *dfdu, double *dfdp)
{
    // Finite, nonsingular Jacobian with a nonfinite Newton right-hand side.
    f[0] = std::numeric_limits<double>::quiet_NaN();
    dfdu[0] = 1;
    dfdp[icp[0]] = -1;
    return 0;
}
// Reject a long prediction so SOLVAE must halve the location step.
int retry_line(const iap_type *iap, const rap_type *rap, integer ndim, const double *u,
         const double *uold, const integer *icp, double *par, integer ijac,
         double *f, double *dfdu, double *dfdp)
{
    line(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp);
    constexpr double longest_prediction = 0.4; // force one retry of the -0.6 trial
    if (std::abs(u[0] - uold[0]) > longest_prediction)
        f[0] = std::numeric_limits<double>::quiet_NaN();
    return 0;
}
double target(iap_type *, rap_type *, double *, integer *, bool *chng,
             FUNI_TYPE((*)), integer *, double *, double *, double *, double *,
             double *u, double *, double *, double *, double *, double *, std::span<const AutoMark>)
{
    *chng = true;
    return u[0] - 0.4; // special point on the straight branch
}
int initial_boundary(const iap_type *, const rap_type *, integer, double *,
                     const integer *, integer, const double *u0, const double *,
                     double *f, integer, double *dbc)
{
    f[0] = u0[0];
    dbc[0] = 1;
    return 0;
}
// A noisy test claims a sign change but has no zero on this straight branch.
double noisy(iap_type *iap, rap_type *, double *, integer *, bool *chng,
             FUNI_TYPE((*)), integer *, double *, double *, double *, double *,
             double *u, double *, double *, double *, double *, double *, std::span<const AutoMark>)
{
    *chng = true;
    iap->lib->session->auto_state.stability.computed.re = {u[0]};
    return 1;
}
// W545 (#610): tests on the straight branch u = par (the test is a function of u alone).
double two_zeros(iap_type *, rap_type *, double *, integer *, bool *chng,
                 FUNI_TYPE((*)), integer *, double *, double *, double *, double *,
                 double *u, double *, double *, double *, double *, double *, std::span<const AutoMark>)
{
    *chng = true;
    return (u[0] - 0.4) * (u[0] - 0.7);
}
double jump(iap_type *, rap_type *, double *, integer *, bool *chng,
            FUNI_TYPE((*)), integer *, double *, double *, double *, double *,
            double *u, double *, double *, double *, double *, double *, std::span<const AutoMark>)
{
    *chng = true;
    return u[0] < 0.4 ? -1 : 1;
}
// cells of width 0.02 alternate in sign: every sample's sign is that of its cell, none is a crossing
double noise(iap_type *, rap_type *, double *, integer *, bool *chng,
             FUNI_TYPE((*)), integer *, double *, double *, double *, double *,
             double *u, double *, double *, double *, double *, double *, std::span<const AutoMark>)
{
    *chng = true;
    return static_cast<long>(std::floor(u[0] / 0.02)) % 2 == 0 ? 1e-3 : -1e-3;
}
double touching(iap_type *, rap_type *, double *, integer *, bool *chng,
                FUNI_TYPE((*)), integer *, double *, double *, double *, double *,
                double *u, double *, double *, double *, double *, double *, std::span<const AutoMark>)
{
    *chng = true;
    return std::pow(u[0] - 0.4, 3);
}
}

int main()
{
    // W545: the crossing rule. The same slope across the step and at the end of the search is a crossing;
    // a slope that grew (a jump, noise) or shrank (a touching) by more than the factor, either way, is not.
    CHECK(auto_crossing_smooth(1, 1, 1e-6, 1e-6));
    CHECK(auto_crossing_smooth(1, 1, 2e-6, 1e-6) && auto_crossing_smooth(1, 1, 5e-7, 1e-6));
    CHECK(!auto_crossing_smooth(1, 1, 1, 1e-4) && !auto_crossing_smooth(1, 1, 1e-12, 1e-4));
    CHECK(!auto_crossing_smooth(1, 1, 0, 1e-4) && !auto_crossing_smooth(1, 0, 1, 1) && !auto_crossing_smooth(1, 1, 1, 0));
    CHECK(auto_crossing_smooth(-1, 1, 3e-6, 1e-6) && !auto_crossing_smooth(-1, 1, 3.1e-6, 1e-6));
    // a cycle's branch-point ratio is in scale while it is below what the step holds tolerances
    CHECK(auto_ratio_in_scale(-1.4, 0.25, 0.05, 1e-4));
    CHECK(!auto_ratio_in_scale(2.4e13, -5.9e14, 0.05, 1e-4) && !auto_ratio_in_scale(-2.7e7, 1.0e8, 0.01, 1e-4));
    CHECK(auto_ratio_in_scale(1e-7, -3e-8, 0.002, 1e-4));

    // MUELLER's quadratic through (-1,2), (0,1), (1,2) has no
    // real zero. AUTO-07p sets the radical to zero instead of sqrt(-1).
    double q0 = 2, q1 = 1, value = 2, s0 = -1, s1 = 0, abscissa = 1, step = 0;
    CHECK(mueller(&q0, &q1, &value, &s0, &s1, &abscissa, &step) == 0);
    CHECK(std::isfinite(step) && step == -2);
    CHECK(q0 == 2 && q1 == 2 && s0 == -1 && s1 == 1);
    // The same formula must also handle an exactly linear interpolant.
    q0 = -1; q1 = 0; value = 1; s0 = -1; s1 = 0; abscissa = 1;
    CHECK(mueller(&q0, &q1, &value, &s0, &s1, &abscissa, &step) == 0);
    CHECK(step == -1);

    char name[] = "test_auto_location";
    char file[] = "examples/ode/lecar.odex";
    char *argv[] = {name, file, nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    auto &s = xpp::client_session();
    xpp::TempDir temp;
    xpp::UniqueFile diagnostics(xpp::files::open_stream(temp.path() + "/location.log", "w"));
    auto &lib = s.auto_lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_layout.size();
    lib.fp9 = diagnostics.get();
    iap_type iap{};
    iap.lib = &lib;
    iap.ndim = iap.ndm = iap.nfpr = 1;
    iap.itnw = 10;
    iap.nit = 3;
    iap.nins = 1;
    // No location iterations may be retained; the first trial exhausts ITMX.
    iap.itmx = 0;
    rap_type rap{};
    rap.ds = rap.dsold = rap.dsmax = 1;
    rap.epsl = rap.epsu = 1e-8;
    rap.epss = 1e-6;
    rap.det = 7;
    integer icp = 0, stride = 2;
    const AutoMark mark{0, 0};
    double rlcur = 1, rlold = 0, rldot = 1;
    double u = 1, uold = 0, udot = 1, f = 0, dfdu = 1;
    AutoTestSample q{-1};
    double thu = 1;
    double dstest = rap.dsold;
    bool stepped = false;
    std::array<double, 4> aa{1, 2, 3, 4};
    std::array<double, 2> rhs{5, 6}, du{7, 8};
    std::vector<double> par(lib.parameter_count), dfdp(lib.parameter_count), thl(lib.parameter_count, 1);
    par[0] = 1;
    dfdp[0] = -1;
    const auto initial_aa = aa;
    const auto initial_rhs = rhs;
    const auto initial_du = du;
    CHECK(lcspae(&iap, &rap, par.data(), &icp, noisy, line, &stride, aa.data(),
                 rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(), &uold, &udot,
                 &f, &dfdu, dfdp.data(), q, thl.data(), &thu, std::span(&mark, 1), dstest, stepped) == 0);
    CHECK(iap.istop == 0 && iap.itp == xpp::AutoPointCode{} && iap.nit == 3 && iap.nins == 1);
    CHECK(rlcur == 1 && rlold == 0 && rldot == 1 && par[0] == 1);
    CHECK(u == 1 && uold == 0 && udot == 1);
    CHECK(rap.dsold == 1 && rap.det == 7 && q.value == 1 && !q.offset);
    CHECK(aa == initial_aa && rhs == initial_rhs && du == initial_du);
    CHECK(dfdu == 1 && dfdp[0] == -1 && f == 0);
    CHECK(s.auto_state.stability.computed.re == std::vector<double>{1});
    // Two location iterations suffice only if Mueller sees the shortened
    // first step (-0.3 rather than -0.6) at its actual abscissa.
    iap.itmx = 2;
    iap.iads = 1;
    rap.dsmin = 1e-5;
    thu = 0; // arclength measures only the parameter on u=par
    q = AutoTestSample{-0.4};
    dstest = rap.dsold;
    CHECK(lcspae(&iap, &rap, par.data(), &icp, target, retry_line, &stride, aa.data(),
                 rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(), &uold, &udot,
                 &f, &dfdu, dfdp.data(), q, thl.data(), &thu, std::span(&mark, 1), dstest, stepped) == 0);
    CHECK(iap.istop == 0 && iap.itp.kind == xpp::AutoPointKind::Pending);
    CHECK(std::abs(u - 0.4) < rap.epss && std::abs(rlcur - 0.4) < rap.epss);
    // W545: the test keeps its value at the discarded end of the step (u = 1: 0.6), 0.6 ahead of the zero
    CHECK(std::abs(q.value - 0.6) < 1e-12 && q.offset && std::abs(*q.offset - 0.6) < 10 * rap.epss && std::abs(rap.dsold) <= 0.4);
    thu = 1;

    // W545 (#610): the straight branch from u = 0, a step of `end` taken, the test's sample `from`.
    thu = 0;
    iap.itmx = 8;
    const auto reset = [&](double end) {
        par[0] = rlcur = u = end;
        rlold = uold = 0;
        rldot = udot = 1;
        f = 0;
        dfdu = 1;
        dfdp[0] = -1;
        rap.dsold = dstest = end;
        iap.itp = xpp::AutoPointCode{};
        iap.istop = 0;
        stepped = false;
    };
    const auto locate = [&](auto &test, AutoTestSample &sample) {
        return lcspae(&iap, &rap, par.data(), &icp, test, line, &stride, aa.data(),
                      rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(), &uold, &udot,
                      &f, &dfdu, dfdp.data(), sample, thl.data(), &thu, std::span(&mark, 1), dstest, stepped);
    };
    const auto notes = [&] { return s.auto_state.stop.notes.size(); };
    const auto frozen = [&] { return std::array<double, 6>{u, uold, rlcur, rlold, rap.dsold, par[0]}; };
    // A: a second zero in the step after a located one. AUTO-07p's stored 0 hides it.
    {
        const auto before = notes();
        reset(0.55);
        AutoTestSample sample{0.28};
        CHECK(locate(two_zeros, sample) == 0);
        CHECK(iap.itp.kind == xpp::AutoPointKind::Pending && std::abs(u - 0.4) < 1e-5);
        CHECK(std::abs(sample.value + 0.0225) < 1e-9 && sample.offset && std::abs(*sample.offset - 0.15) < 1e-4);
        iap.itp = xpp::AutoPointCode{};
        double rds = 0.4;
        contae(&iap, &rap, &rds, &rlcur, &rlold, &rldot, &u, &uold, &udot);
        solvae(&iap, &rap, par.data(), &icp, line, &rds, &stride, aa.data(), rhs.data(), &rlcur, &rlold,
               &rldot, &u, du.data(), &uold, &udot, &f, &dfdu, dfdp.data(), thl.data(), &thu);
        CHECK(std::abs(u - 0.8) < 1e-5);
        dstest = rap.dsold;
        AutoTestSample stored_zero{0};
        const auto state = frozen();
        CHECK(locate(two_zeros, stored_zero) == 0);
        CHECK(iap.itp.kind != xpp::AutoPointKind::Pending && state == frozen());
        CHECK(locate(two_zeros, sample) == 0);
        CHECK(iap.itp.kind == xpp::AutoPointKind::Pending && std::abs(u - 0.7) < 1e-5 && notes() == before);
    }
    // B: a search that uses up ITMX (0 here) bisects its bracket and the zero of a smooth test is located
    {
        iap.itmx = 0;
        reset(1);
        const auto before = notes();
        AutoTestSample sample{-0.4};
        CHECK(locate(target, sample) == 0);
        CHECK(iap.itp.kind == xpp::AutoPointKind::Pending && stepped && std::abs(u - 0.4) < 1e-5 && notes() == before);
    }
    // a jump, noise and a touching are crossings of nothing smooth: refused, said once, the state the step end's
    // (the first starts from the test's discarded end, the others from ITMX 0's bisection)
    for (const auto &[test, first, itmx] : {
             std::tuple{jump, -1.0, 8}, std::tuple{jump, -1.0, 0},
             std::tuple{noise, -1e-3, 0}, std::tuple{touching, -0.064, 0}}) {
        iap.itmx = itmx;
        reset(1);
        const auto state = frozen();
        const auto before = notes();
        AutoTestSample sample{first, itmx == 8 ? std::optional<double>{0.} : std::nullopt};
        CHECK(locate(test, sample) == 0);
        const auto refused = notes() == before + 1 ? s.auto_state.stop.notes.back() : std::string();
        CHECK(iap.itp.kind != xpp::AutoPointKind::Pending && !stepped && !sample.offset);
        CHECK(state == frozen());
        CHECK(refused.find("was not accepted at parameter") != std::string::npos);
        CHECK(refused.find("not a smooth crossing") != std::string::npos);
    }
    iap.itmx = 2;
    thu = 1;

    // W343 (#404): reject a nonfinite step, retry down to Dsmin, restore
    // the last solution and report its attempted point; nothing is stored.
    iap.ibr = 1;
    iap.ntot = 1;
    iap.iads = 1;
    rap.dsmin = 0.2;
    double ds = 1;
    const auto count = s.diagram.points.size();
    std::string error;
    try {
        solvae(&iap, &rap, par.data(), &icp, nonfinite_line, &ds, &stride,
               aa.data(), rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(),
               &uold, &udot, &f, &dfdu, dfdp.data(), thl.data(), &thu);
    } catch (const xpp::AutoFailed &e) { error = e.error.what; }
    CHECK(error == "Branch 1 point 2: nonfinite AUTO solution");
    CHECK(ds > 0 && ds < rap.dsmin);
    CHECK(iap.istop == 1 && u == uold && rlcur == rlold && par[0] == rlold);
    CHECK(s.diagram.points.size() == count);

    // The same failure in the collocation solver must not pass its
    // maximum-increment test (NaN comparisons formerly left done=true).
    iap.istop = 0;
    iap.ips = xpp::AutoProblem::BoundaryValue;
    iap.ntst = iap.nbc = iap.nwtn = 1;
    iap.ncol = 2;
    integer mesh_stride = 2;
    std::array<double, 4> mesh{1, 1, 1, 0}, old{}, tangent{1, 1, 1, 0};
    std::array<double, 4> dups{}, upold{}, fa{};
    std::array<double, 2> fc{}, tm{0, 1}, dtm{1, 0};
    double p0 = 0, p1 = 0;
    ds = rlcur = 1;
    rlold = par[0] = 0;
    error.clear();
    try {
        stepbv(&iap, &rap, par.data(), &icp, nonfinite_line, initial_boundary,
               nullptr, nullptr, &ds, &rlcur, &rlold, &rldot, &mesh_stride,
               mesh.data(), dups.data(), old.data(), tangent.data(), upold.data(),
               fa.data(), fc.data(), tm.data(), dtm.data(), &p0, &p1,
               thl.data(), &thu);
    } catch (const xpp::AutoFailed &e) { error = e.error.what; }
    CHECK(error == "Branch 1 point 2: nonfinite AUTO solution");
    CHECK(ds > 0 && ds < rap.dsmin);
    CHECK(iap.istop == 1 && mesh == old && rlcur == rlold && par[0] == rlold);
    CHECK(s.diagram.points.size() == count);

    // The final storage seam refuses nonfinite state, parameter and norm
    // independently, before filling even the initial diagram placeholder.
    std::array<integer, 4> slots{};
    const double infinity = std::numeric_limits<double>::infinity();
    for (int field = 0; field < 3; ++field) {
        double norm = field == 2 ? infinity : 1;
        double state = field == 0 ? infinity : 1;
        par[0] = field == 1 ? infinity : 1;
        error.clear();
        try {
            xpp::addbif(&iap, &rap, 2, 1, par.data(), slots.data(), 0,
                        &norm, std::span(&state, 1), std::span(&state, 1), std::span(&state, 1), std::span(&state, 1));
        } catch (const xpp::AutoFailed &e) { error = e.error.what; }
        CHECK(error == "Branch 1 point 2: nonfinite AUTO solution");
        CHECK(s.diagram.points.size() == count && !s.auto_state.diag_flag);
    }
    lib.fp9 = nullptr;
    // Two UZ tests cross in the same step on u=p. The farther mark is
    // tested first, then the second locator moves back past it. Its sample
    // must be recomputed, so the next continuation step finds it again.
    const std::string events_file = temp.path() + "/two_events.odex";
    xpp::Writer events_model(events_file);
    events_model.print("{}", versioned("par p=0\ninit u=0\nu'=p-u\n"));
    CHECK(events_model.commit().has_value());
    std::string event_path = events_file;
    std::array<char *, 3> event_argv{name, event_path.data(), nullptr};
    CHECK(load_args(2, event_argv.data(), 1).has_value());
    auto &events_session = xpp::client_session();
    events_session.auto_state.dir = temp.path();
    xpp::init_auto_win(events_session);
    xpp::AutoSettingsSet settings;
    settings.par1 = "p";
    for (const auto &[field, number] : {
        std::pair{xpp::AUTO_NUM_MAX_STEPS, 8.0}, std::pair{xpp::AUTO_NUM_STEP_SIZE, 0.2},
        std::pair{xpp::AUTO_NUM_MAX_STEP_SIZE, 0.2}, std::pair{xpp::AUTO_NUM_MAX_BRANCH_SWITCHES, 0.0},
        std::pair{xpp::AUTO_NUM_SPECIAL_POINT_TOLERANCE, 1e-7}}) {
        settings.has_num[field] = 1;
        settings.num[field] = number;
    }
    settings.nmarks = 2;
    settings.mark_name = {"p", "p"};
    settings.mark_value = {0.35, 0.3};
    std::string why;
    CHECK(xpp::auto_settings_apply(events_session, settings, why) == 0);
    CHECK(xpp::auto_run_start(events_session,xpp::AutoRunRequest{}));
    std::vector<double> marks;
    for (const auto &point : events_session.diagram.points) {
        if (point.itp.kind == xpp::AutoPointKind::UserZero) {
            marks.push_back(diagram_parameter(point, 0));
            CHECK(std::abs(point.u0[0] - marks.back()) < 1e-7);
        }
    }
    CHECK(marks.size() == 2);
    if (marks.size() == 2) {
        CHECK(std::abs(marks[0] - 0.3) < 1e-7);
        CHECK(std::abs(marks[1] - 0.35) < 1e-7);
    }
    TEST_REPORT("AUTO location");
}
