// W364 (#426): independent normal forms fix the signs and zeros of the tests.
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "auto_settings.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include <array>
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
    TEST_REPORT("AUTO codim-2");
}
