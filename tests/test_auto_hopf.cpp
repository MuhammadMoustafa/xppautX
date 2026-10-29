/* W363 (#425): the reduced Hopf equations, structured derivatives and BT extension. */
#include "load_args.h"
#include "model_text.h"
#include "xpptest.h"
#include "session.h"
#include "jacobian.h"
#include "expr_native.h"
#include "auto_c.h"
#include "auto_jacobian.h"
#include "auto_nox.h"
#include "auto_unknowns.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_math.h"
#include <array>
#include <cmath>
#include <vector>

int main() {
    xpp::TempDir temp;
    std::string model = temp.path() + "/hopf.odex";
    auto writer = xpp::Writer::binary(model);
    CHECK(writer.write(versioned("x'=y\ny'=a+b*y+x*x\npar a=-0.25,b=0\ninit x=-0.5,y=0\n")) && writer.commit());
    char name[] = "test_auto_hopf";
    char *argv[] = {name, model.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    auto &s = xpp::client_session();
    CHECK(xpp::ensure_exact_jacobian(s));
    s.numerics.jacobian = xpp::JACOBIAN_EXACT;
    auto &lib = s.auto_lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_layout.size();
    iap_type iap{};
    iap.lib = &lib;
    iap.ndm = 2;
    iap.ndim = 6;
    iap.nfpr = 2;
    iap.jac = 1;
    iap.ips = xpp::AutoProblem::SteadyState;
    iap.itpst = xpp::AutoPointKind::Hopf;
    rap_type rap{};
    std::array<integer, 2> icp{s.auto_state.parameter_layout.solver_index(0), s.auto_state.parameter_layout.solver_index(1)};
    std::vector<double> par(lib.parameter_count), dp(6 * lib.parameter_count);
    par[icp[0]] = -.25;
    std::array<double, 6> u{-.5, 0., 1., 0., 1., 0.}, old = u, f{}, minus{}, plus{};
    std::array<double, 36> jac{}, full{};
    fnhb(&iap, &rap, 6, u.data(), old.data(), icp.data(), par.data(), 2, f.data(), jac.data(), dp.data());
    // A=[[0,1],[-1,0]], v=(1,0), kappa=1 is an exact Hopf solution.
    for (double value : f) CHECK(std::abs(value) < 1e-8);
    CHECK(std::abs(jac[1] + 1) < 1e-8 && std::abs(jac[6] - 1) < 1e-8);
    CHECK(std::abs(jac[2] - 2) < 1e-5); // d(A^2 v)/dx
    CHECK(jac[2 + 4 * 6] == 1 && jac[4 + 2 * 6] == 2);
    CHECK(std::abs(jac[5 + 3 * 6] - 2) < 1e-8); // symmetric phase
    iap.jac = 0;
    s.numerics.jacobian = xpp::JACOBIAN_DIFFERENCE;
    // Independent full central Jacobian at a non-solution, including phase
    // and both parameter derivatives, so zero residuals cannot hide mistakes.
    u = {-.4, .1, .8, .6, 1.2, .15};
    fnhb(&iap, &rap, 6, u.data(), old.data(), icp.data(), par.data(), 2, f.data(), jac.data(), dp.data());
    constexpr double step = 1e-4; // larger than the inner RHS difference step
    for (int col = 0; col < 7; ++col) {
        auto state = u;
        if (col < 6) state[col] -= step;
        else par[icp[0]] -= step;
        fnhb(&iap, &rap, 6, state.data(), old.data(), icp.data(), par.data(), 0, minus.data(), full.data(), dp.data());
        state = u;
        if (col < 6) state[col] += step;
        else par[icp[0]] += 2 * step;
        fnhb(&iap, &rap, 6, state.data(), old.data(), icp.data(), par.data(), 0, plus.data(), full.data(), dp.data());
        if (col == 6) par[icp[0]] -= step;
        // Recompute after residual-only calls, which may use parameter scratch.
        fnhb(&iap, &rap, 6, u.data(), old.data(), icp.data(), par.data(), 2, f.data(), jac.data(), dp.data());
        for (int row = 0; row < 6; ++row) {
            const double expected = (plus[row] - minus[row]) / (2 * step);
            CHECK(std::abs(expected - (col < 6 ? jac[row + col * 6] : dp[row + icp[0] * 6])) < 3e-4);
        }
    }
    // The same branch crosses BT at x=0: a=-x^2, kappa=-2*x.
    par[xpp::auto_unknown::period] = 7.;
    for (double x : {-.5, 0., .5}) {
        u = {x, 0., 1., 0., -2*x, 0.};
        par[icp[0]] = -x*x;
        fnhb(&iap, &rap, 6, u.data(), old.data(), icp.data(), par.data(), 2, f.data(), jac.data(), dp.data());
        for (double value : f) CHECK(std::abs(value) < 1e-8);
        pvlsae(&iap, &rap, u.data(), par.data());
        CHECK(std::abs(par[xpp::auto_unknown::period] - pi(2.)) < 1e-10);
        CHECK(xpp::auto_hopf_curve_kind(u[4]) == (x > 0 ? xpp::auto_neutral_saddle_curve : xpp::auto_hopf_curve));
    }
    // Start at a stored one-parameter HB: construct only the reduced layout.
    lib.fp3 = xpp::files::open_stream(temp.path() + "/fort.3", "w+");
    lib.fp9 = xpp::files::open_stream(temp.path() + "/fort.9", "w+");
    CHECK(lib.fp3 && lib.fp9);
    par[icp[0]] = -.25;
    par[icp[1]] = 0.;
    par[xpp::auto_unknown::period] = pi(2.);
    xpp::print(lib.fp3, "1 1 3 1 1 1 1 3 2 0 0 {}\n0 -0.5 0\n", lib.parameter_count);
    for (double value : par) xpp::print(lib.fp3, "{} ", value);
    xpp::print(lib.fp3, "\n");
    CHECK(xpp::rewind_stream(lib.fp3, "fort.3").has_value());
    iap.irs = 1;
    iap.isw = 2;
    u.fill(0.);
    CHECK(stpnhb(&iap, &rap, par.data(), icp.data(), u.data()) == 0);
    old = u;
    fnhb(&iap, &rap, 6, u.data(), old.data(), icp.data(), par.data(), 2, f.data(), jac.data(), dp.data());
    for (double value : f) CHECK(std::abs(value) < 1e-8);
    CHECK(u[4] == 1 && u[5] == 0);
    xpp::UniqueFile restart(lib.fp3), diagnostics(lib.fp9);
    lib.fp3 = lib.fp9 = nullptr;
    // Read the current reduced neutral-saddle restart through the same
    // validator used before restoring a session's solution member.
    par[icp[0]] = -.25;
    std::string solution = xpp::format("1 1 34 1 2 2 1 7 2 0 0 {}\n0 0.5 0 1 0 -1 0\n", lib.parameter_count);
    for (double value : par) solution += xpp::number(value) + " ";
    solution += "\n";
    CHECK(xpp::check_auto_solutions(solution, "current.snapx/auto/solutions.s", 16, lib.parameter_count, false, 6));
    auto current = xpp::Writer::binary(temp.path() + "/current.s");
    CHECK(current.write(solution) && current.commit());
    auto input = xpp::open_read(temp.path() + "/current.s");
    lib.fp3 = input.get();
    u.fill(0.);
    CHECK(stpnae(&iap, &rap, par.data(), icp.data(), u.data()) == 0);
    CHECK(u == (std::array<double, 6>{.5, 0., 1., 0., -1., 0.}));
    lib.fp3 = nullptr;
    pvlsae(&iap, &rap, u.data(), par.data());
    CHECK(par[xpp::auto_unknown::period] == pi(2.));
    // Own files reject the obsolete augmented layout on its header line.
    const auto old_layout = xpp::check_auto_solutions(
        xpp::format("1 1 34 1 2 2 1 9 2 0 0 {}\n", lib.parameter_count),
        "sent.snapx/auto/solutions.s", 16, lib.parameter_count, false, 6);
    CHECK(!old_layout && old_layout.error().place.line == 1 && old_layout.error().place.file == "sent.snapx/auto/solutions.s");
    TEST_REPORT("auto_hopf");
}
