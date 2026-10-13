/* W326 (#387): an exhausted location must not become a continuation step. */
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include <array>

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
// A noisy test claims a sign change but has no zero on this straight branch.
double noisy(iap_type *iap, rap_type *, double *, integer *, logical *chng,
             FUNI_TYPE((*)), integer *, double *, double *, double *, double *,
             double *u, double *, double *, double *, double *, double *, integer *, double *)
{
    *chng = TRUE_;
    iap->lib->session->auto_state.stability.computed.re = {u[0]};
    return 1;
}
}

int main()
{
    char name[] = "test_auto_location";
    char file[] = "examples/ode/lecar.odex";
    char *argv[] = {name, file, nullptr};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    auto &s = xpp::client_session();
    xpp::TempDir temp;
    xpp::UniqueFile diagnostics(xpp::files::open_stream(temp.path() + "/location.log", "w"));
    auto &lib = s.auto_lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_count();
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
    integer icp = 0, stride = 2, iuz = 0;
    double rlcur = 1, rlold = 0, rldot = 1;
    double u = 1, uold = 0, udot = 1, f = 0, dfdu = 1;
    double q = -1, vuz = 0, thu = 1;
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
                 &f, &dfdu, dfdp.data(), &q, thl.data(), &thu, &iuz, &vuz) == 0);
    CHECK(iap.istop == 0 && iap.itp == 0 && iap.nit == 3 && iap.nins == 1);
    CHECK(rlcur == 1 && rlold == 0 && rldot == 1 && par[0] == 1);
    CHECK(u == 1 && uold == 0 && udot == 1);
    CHECK(rap.dsold == 1 && rap.det == 7 && q == 1);
    CHECK(aa == initial_aa && rhs == initial_rhs && du == initial_du);
    CHECK(dfdu == 1 && dfdp[0] == -1 && f == 0);
    CHECK(s.auto_state.stability.computed.re == std::vector<double>{1});
    lib.fp9 = nullptr;
    TEST_REPORT("AUTO location");
}
