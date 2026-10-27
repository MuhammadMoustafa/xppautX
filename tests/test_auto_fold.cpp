// W436 (#498): independent derivatives of a polynomial fold system.
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "auto_unknowns.h"
#include "xpp_batch.h"
#include "jacobian.h"
#include "expr_native.h"
#include <utility>
#include "xpp_files.h"
#include <array>
#include <cmath>

int main()
{
    xpp::TempDir temp;
    std::string path = temp.path() + "/fold.odex";
    auto writer = xpp::Writer::binary(path);
    CHECK(writer.write("x'=x*x*x+a*y\ny'=y*y+b*x\npar a=2,b=3\ninit x=1,y=2\n") && writer.commit());
    char name[] = "test_auto_fold";
    char *argv[] = {name, path.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    auto &s = xpp::client_session();
    auto &lib = s.auto_lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_layout.size();
    iap_type iap{};
    iap.lib = &lib; iap.ndm = 2; iap.ndim = 4; iap.nfpr = 4;
    size_run_scratch(iap);
    // Isolate the augmented blocks from roundoff in nested first differences.
    s.numerics.jacobian = xpp::JACOBIAN_EXACT;
    CHECK(xpp::ensure_exact_jacobian(s));
    iap.jac = 1;
    rap_type rap{};
    const integer a = s.auto_state.parameter_layout.solver_index(0);
    const integer b = s.auto_state.parameter_layout.solver_index(1);
    std::vector<double> par(lib.parameter_count), dp(4 * lib.parameter_count);
    par[a] = 2; par[b] = 3;
    par[xpp::auto_unknown::period] = 2;
    par[xpp::auto_unknown::fold_beta] = 0.5;
    std::array<double, 4> u{1, 2, 0.3, -0.4}, f{};
    std::array<double, 16> jac{};
    for (bool variable : {true, false}) {
        std::array<integer, 4> icp{a, b,
            variable ? xpp::auto_unknown::period : xpp::auto_unknown::fold_norm,
            xpp::auto_unknown::fold_beta};
        fnpl(&iap, &rap, 4, u.data(), u.data(), icp.data(), par.data(), 2,
             f.data(), jac.data(), dp.data());
        // T*f = (10,14); T*A*v = (0.2,-1.4).
        const std::array<double,4> expected_f{10,14,variable ? 2.7 : 0.2,variable ? 2.1 : -0.9};
        const std::array<double,16> expected_jac{
            6,6,variable ? 5.1 : 3.6,variable ? 1.5 : 0.5,
            4,8,variable ? 1. : 0.,variable ? 0.4 : -1.6,
            0,0,6,6, 0,0,4,8};
        for (std::size_t i=0;i<f.size();++i) CHECK(std::abs(f[i]-expected_f[i])<1e-5);
        for (std::size_t i=0;i<jac.size();++i) CHECK(std::abs(jac[i]-expected_jac[i])<1e-3);
        CHECK(std::abs(dp[4*a]-4)<1e-5 && std::abs(dp[4*a+2]-(variable ? 0.2 : -0.8))<1e-3);
        CHECK(std::abs(dp[4*b+1]-2)<1e-5 && std::abs(dp[4*b+3]-(variable ? 1.1 : 0.6))<1e-3);
        CHECK(std::abs(dp[4*xpp::auto_unknown::fold_beta+2]-(variable ? 5 : 0))<1e-5);
        CHECK(std::abs(dp[4*xpp::auto_unknown::fold_beta+3]-(variable ? 7 : 1))<1e-5);
        if (variable) {
            CHECK(std::abs(dp[4*xpp::auto_unknown::period]-5)<1e-5);
            CHECK(std::abs(dp[4*xpp::auto_unknown::period+2]-0.1)<1e-5);
        }
        CHECK(par[a]==2 && par[b]==3);
    }
    // W437: the phase hyperplane passes through the previous computed orbit,
    // even when its discrete u_old dot u_old' is nonzero (-1 here).
    lib.rotations.nrtn.assign(2, 0);
    const std::array<integer,4> phase_icp{a,b,xpp::auto_unknown::period,xpp::auto_unknown::fold_beta};
    const auto phase_old=u;
    const std::array<double,4> time_derivative{3,-2,0,0};
    icps(&iap,&rap,2,par.data(),phase_icp.data(),1,u.data(),phase_old.data(),
         u.data(),time_derivative.data(),f.data(),1,dp.data());
    CHECK(f[0]==0 && dp[0]==3 && dp[1]==-2);
    u[0]+=0.25;
    icps(&iap,&rap,2,par.data(),phase_icp.data(),1,u.data(),phase_old.data(),
         u.data(),time_derivative.data(),f.data(),0,nullptr);
    CHECK(f[0]==0.75);
    lib.rotations.nrtn[0]=1;
    icps(&iap,&rap,2,par.data(),phase_icp.data(),1,u.data(),phase_old.data(),
         u.data(),time_derivative.data(),f.data(),1,dp.data());
    CHECK(f[0]==0 && dp[0]==0 && dp[1]==-2);
    lib.rotations.nrtn[0]=0;
    u=phase_old;
    icpl(&iap,&rap,4,par.data(),phase_icp.data(),3,u.data(),phase_old.data(),
         u.data(),time_derivative.data(),f.data(),0,nullptr);
    CHECK(f[0]==0);
    icpd(&iap,&rap,4,par.data(),phase_icp.data(),2,u.data(),phase_old.data(),
         u.data(),time_derivative.data(),f.data(),0,nullptr);
    CHECK(f[0]==0);
    const std::array<double,6> torus{1,2,0.3,-0.4,0.5,-0.6};
    ictr(&iap,&rap,6,par.data(),phase_icp.data(),3,torus.data(),torus.data(),
         torus.data(),time_derivative.data(),f.data(),0,nullptr);
    CHECK(f[0]==0);

    // The worker's parameter-only derived values survive state differences,
    // but every parameter change (including the sign of zero) invalidates them.
    path = temp.path() + "/derived.odex";
    auto derived_writer = xpp::Writer::binary(path);
    CHECK(derived_writer.write("d=a*b\nx'=d*x\npar a=2,b=3\ninit x=1\n") && derived_writer.commit());
    argv[1] = path.data();
    CHECK(load_args(2, argv, 1).has_value());
    auto &derived = xpp::client_session();
    auto &worker = derived.auto_lib;
    worker.session = &derived;
    worker.parameter_count = derived.auto_state.parameter_layout.size();
    worker.pure_rhs = true;
    worker.rhs_constants = derived.parser.constants;
    worker.rhs_variables = derived.parser.variables;
    const std::array<integer,2> slots{derived.auto_state.parameter_layout.solver_index(0),
                                     derived.auto_state.parameter_layout.solver_index(1)};
    par.assign(worker.parameter_count, 0.);
    double state=1, value=0, unused=0;
    for (const auto &[first,second] : {std::pair{2.,3.},std::pair{-2.,3.},std::pair{2.,4.},
                                      std::pair{0.,4.},std::pair{-0.,4.},std::pair{-0.,4.},std::pair{2.,3.}}) {
        par[slots[0]]=first; par[slots[1]]=second;
        func(derived,1,&state,slots.data(),2,par.data(),0,&value,&unused,&unused,&worker);
        CHECK(value==first*second && std::signbit(value)==std::signbit(first*second));
    }
    TEST_REPORT("AUTO fold blocks");
}
