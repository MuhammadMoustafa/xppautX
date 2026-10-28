/* W326 (#387): an exhausted location must not become a continuation step. */
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include <array>
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
}

int main()
{
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
    double q = -1, thu = 1;
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
                 &f, &dfdu, dfdp.data(), &q, thl.data(), &thu, std::span(&mark, 1), dstest, stepped) == 0);
    CHECK(iap.istop == 0 && iap.itp == xpp::AutoPointCode{} && iap.nit == 3 && iap.nins == 1);
    CHECK(rlcur == 1 && rlold == 0 && rldot == 1 && par[0] == 1);
    CHECK(u == 1 && uold == 0 && udot == 1);
    CHECK(rap.dsold == 1 && rap.det == 7 && q == 1);
    CHECK(aa == initial_aa && rhs == initial_rhs && du == initial_du);
    CHECK(dfdu == 1 && dfdp[0] == -1 && f == 0);
    CHECK(s.auto_state.stability.computed.re == std::vector<double>{1});
    // Two location iterations suffice only if Mueller sees the shortened
    // first step (-0.3 rather than -0.6) at its actual abscissa.
    iap.itmx = 2;
    iap.iads = 1;
    rap.dsmin = 1e-5;
    thu = 0; // arclength measures only the parameter on u=par
    q = -0.4;
    dstest = rap.dsold;
    CHECK(lcspae(&iap, &rap, par.data(), &icp, target, retry_line, &stride, aa.data(),
                 rhs.data(), &rlcur, &rlold, &rldot, &u, du.data(), &uold, &udot,
                 &f, &dfdu, dfdp.data(), &q, thl.data(), &thu, std::span(&mark, 1), dstest, stepped) == 0);
    CHECK(iap.istop == 0 && iap.itp.kind == xpp::AutoPointKind::Pending);
    CHECK(std::abs(u - 0.4) < rap.epss && std::abs(rlcur - 0.4) < rap.epss);
    CHECK(q == 0 && std::abs(rap.dsold) <= 0.4);
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
    events_model.print(R"(par p=0
init u=0
u'=p-u
)");
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
