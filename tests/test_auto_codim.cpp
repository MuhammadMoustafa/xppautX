// W364 (#426): independent normal forms fix the signs and zeros of the tests.
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "auto_settings.h"
#include "auto_stability_auto.h"
#include "auto_unknowns.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include <array>
#include <algorithm>
#include <cmath>

int main()
{
    xpp::TempDir temp;
    std::string path=temp.path()+"/bautin.odex";
    auto writer=xpp::Writer::binary(path);
    CHECK(writer.write("x'=-y+a*x+b*x*(x*x+y*y)\ny'=x+a*y+b*y*(x*x+y*y)\npar a=0,b=1\ninit x=0,y=0\n") && writer.commit());
    char name[]="test_auto_codim";
    char *argv[]={name,path.data(),nullptr};
    CHECK(xpp::load_model(2,argv,1).has_value());
    auto &s=xpp::client_session();
    auto &lib=s.auto_lib;
    lib.session=&s;
    lib.parameter_count=s.auto_state.parameter_layout.size();
    lib.codim.prepare(2);
    iap_type iap{};
    iap.lib=&lib; iap.ndm=2; iap.ndim=6; iap.nfpr=2; iap.isw=2; iap.ips=xpp::AutoProblem::SteadyState;
    iap.itpst=xpp::AutoPointKind::Hopf;
    rap_type rap{};
    std::array<integer,2> icp{s.auto_state.parameter_layout.solver_index(0),s.auto_state.parameter_layout.solver_index(1)};
    std::vector<double> par(lib.parameter_count),dp(6*lib.parameter_count);
    std::array<double,6> u{0,0,1,0,1,1},rhs{},old=u;
    std::array<double,36> jac{};
    integer stride=7;
    logical enabled;
    const auto coefficient=[&](double b) {
        u[5]=b; par[icp[1]]=b;
        return fnghae(&iap,&rap,par.data(),icp.data(),&enabled,fnhb,&stride,nullptr,
            nullptr,nullptr,nullptr,u.data(),old.data(),nullptr,rhs.data(),jac.data(),dp.data(),{});
    };
    // Unit complex eigenvector gives l1=2*b at omega=1 for this normal form.
    CHECK(std::abs(coefficient(1)-2)<1e-5 && enabled);
    CHECK(std::abs(coefficient(-1)+2)<1e-5 && enabled);
    CHECK(std::abs(coefficient(0))<1e-8 && enabled);
    u[4]=-1;
    CHECK(coefficient(1)==0 && !enabled);
    xpp::AutoSettingsSet set;
    set.special_points.fill(false);
    std::string why;
    CHECK(xpp::auto_settings_apply(s,set,why)==0);
    for (bool on:s.auto_state.special_points) CHECK(!on);
    set.special_points[0]=true;
    set.has_num[xpp::AUTO_NUM_NTST]=1; set.num[xpp::AUTO_NUM_NTST]=-1;
    CHECK(xpp::auto_settings_apply(s,set,why)!=0 && !s.auto_state.special_points[0]);
    // The reaction's exact multilinear derivatives give these coefficients;
    // independent contraction: tests/auto/codim_coefficients.py.
    std::string reaction="examples/tstauto/ab.odex";
    char *reaction_args[]={name,reaction.data(),nullptr};
    CHECK(xpp::load_model(2,reaction_args,1).has_value());
    auto &reaction_session=xpp::client_session();
    auto &reaction_lib=reaction_session.auto_lib;
    reaction_lib.session=&reaction_session;
    reaction_lib.parameter_count=reaction_session.auto_state.parameter_layout.size();
    reaction_lib.codim.prepare(2);
    iap.lib=&reaction_lib;
    icp={reaction_session.auto_state.parameter_layout.solver_index(0),reaction_session.auto_state.parameter_layout.solver_index(1)};
    par.resize(reaction_lib.parameter_count); dp.resize(6*reaction_lib.parameter_count);
    const std::array<std::array<double,2>,3> cases={{{1,0.23809523809523817},{2,-0.2783653083592841},{5.5,-0.360695777622527}}};
    for (const auto &item:cases) {
        const double t=item[0], y=(4+t)/3, p1=(4+t)*(1+t)/t;
        u={t/(1+t),y,1,0,2*t-1,p1}; old=u;
        par[icp[0]]=t*xpp::math::exp(-y); par[icp[1]]=p1;
        const double got=fnghae(&iap,&rap,par.data(),icp.data(),&enabled,fnhb,&stride,nullptr,
            nullptr,nullptr,nullptr,u.data(),old.data(),nullptr,rhs.data(),jac.data(),dp.data(),{});
        CHECK(std::abs(got-item[1])<1e-3 && enabled);
    }
    // Independent spectra: fixed neutral/imposed multipliers and a real
    // remaining multiplier crossing +1 (fold) or -1 (flip).
    iap.ndm=3; iap.ndim=6; rap.epss=1e-5;
    reaction_session.auto_state.special_points.fill(true);
    std::array<doublecomplex,3> multipliers={{{1,0},{1,0},{.9,0}}};
    iap.itpst=xpp::AutoPointKind::PeriodicFold;
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)<0 && enabled);
    multipliers[2].r=1.1;
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)>0 && enabled);
    iap.itpst=xpp::AutoPointKind::PeriodDoubling;
    multipliers[1].r=-1; multipliers[2].r=-1.1;
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)>0 && enabled);
    multipliers[2].r=-.9;
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)<0 && enabled);
    multipliers[2].i=.5;
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)==0 && !enabled);
    iap.itpst=xpp::AutoPointKind::Torus;
    for (double root:{0.,.5,2./3,1.}) {
        par[xpp::auto_unknown::torus_angle]=pi(root)-1e-4;
        const double before=auto_cycle_test(iap,rap,par.data(),multipliers,&enabled);
        par[xpp::auto_unknown::torus_angle]=pi(root)+1e-4;
        CHECK(before*auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)<0 && enabled);
    }
    // A disabled root is removed, so it cannot hide a later enabled root.
    const auto r4=std::find(xpp::auto_codim_kinds.begin(),xpp::auto_codim_kinds.end(),xpp::AutoPointKind::Resonance4);
    reaction_session.auto_state.special_points[static_cast<std::size_t>(r4-xpp::auto_codim_kinds.begin())]=false;
    par[xpp::auto_unknown::torus_angle]=pi(.5);
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)!=0 && enabled);
    reaction_session.auto_state.special_points.fill(false);
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)==0 && !enabled);
    // A mixed augmented pencil with exact original multipliers {1,-1,-2}.
    // P1's last columns have full rank only after selecting different rows.
    reaction_lib.codim.prepare_cycle(6,3);
    std::array<double,36> p0{},p1{};
    const std::array<double,3> exact={1.,-1.,-2.};
    for (int row=0;row<6;++row) for (int col=0;col<3;++col) {
        const double value=(row%3==col ? 1. : 0.)+(row==5 ? .2 : 0.);
        p1[row+6*(3+col)]=value;
        p0[row+6*(3+col)]=-value*exact[col];
    }
    xpp::auto_cycle_multipliers(iap,p0.data(),p1.data(),multipliers.data(),true);
    std::array<double,3> real={multipliers[0].r,multipliers[1].r,multipliers[2].r};
    std::sort(real.begin(),real.end());
    CHECK(std::abs(real[0]+2)<1e-10 && std::abs(real[1]+1)<1e-10 && std::abs(real[2]-1)<1e-10);
    TEST_REPORT("AUTO codim-2");
}
