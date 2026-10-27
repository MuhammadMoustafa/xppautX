// W364 (#426): independent normal forms fix the signs and zeros of the tests.
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "auto_settings.h"
#include "jacobian.h"
#include "expr_native.h"
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
    CHECK(load_args(2,argv,1).has_value());
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
    // The test reads the Jacobian solvae computed at the converged point (W421).
    const auto lyapunov=[&]() {
        fnhb(&iap,&rap,iap.ndim,u.data(),old.data(),icp.data(),par.data(),2,rhs.data(),jac.data(),dp.data());
        return fnghae(&iap,&rap,par.data(),icp.data(),&enabled,fnhb,&stride,nullptr,
            nullptr,nullptr,nullptr,u.data(),old.data(),nullptr,rhs.data(),jac.data(),dp.data(),{});
    };
    const auto coefficient=[&](double b) {
        u[5]=b; par[icp[1]]=b;
        return lyapunov();
    };
    // Unit complex eigenvector gives l1=2*b at omega=1 for this normal form.
    CHECK(std::abs(coefficient(1)-2)<1e-5 && enabled);
    CHECK(std::abs(coefficient(-1)+2)<1e-5 && enabled);
    CHECK(std::abs(coefficient(0))<1e-8 && enabled);
    u[4]=-1;
    CHECK(!auto_test_applies(coefficient(1)) && !enabled);
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
    CHECK(load_args(2,reaction_args,1).has_value());
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
        const double got=lyapunov();
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
    CHECK(!auto_test_applies(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)) && !enabled);
    // W405: a real imposed multiplier goes next to the neutral one alone; at the
    // 1:2 resonance roundoff splits the double -1 into a conjugate pair, which
    // goes there whole and is still tested (AUTO-07p's test gives 0 there, a
    // zero its location accepts).
    std::array<doublecomplex,4> placed={{{1,0},{.47,0},{-.9,0},{-1,0}}};
    CHECK(auto_cycle_place_imposed(iap.itpst,placed)==2);
    CHECK(placed[1].r==-1 && placed[2].r==.47 && placed[3].r==-.9);
    placed={{{1,0},{.47,0},{-1.0000359,2.2e-4},{-1.0000359,-2.2e-4}}};
    CHECK(auto_cycle_place_imposed(iap.itpst,placed)==3);
    CHECK(placed[1].i>0 && placed[2].i<0 && placed[3].r==.47);
    const double split=auto_cycle_test(iap,rap,par.data(),placed,&enabled);
    CHECK(std::abs(split-(xpp::math::hypot(placed[2].r,placed[2].i)-1))<1e-15 && split>0 && enabled);
    // A torus curve's 1:1 resonance is its angle 0 modulo 2pi, from either side.
    for (double angle:{-1e-3,1e-3,pi(2.)-1e-3,pi(2.)+1e-3}) CHECK(xpp::auto_torus_near_one_to_one(angle));
    for (double angle:{pi(.5),pi(2./3),pi(1.),pi(4./3),pi(1.5)}) CHECK(!xpp::auto_torus_near_one_to_one(angle));
    // W419: its equations are singular at 1:1 and 1:2, where sin(theta) = 0.
    for (double angle:{-1e-3,pi(1.)-1e-3,pi(1.)+1e-3,pi(3.)}) CHECK(xpp::auto_torus_singular(angle));
    for (double angle:{pi(.5),pi(2./3),pi(4./3),pi(1.5)}) CHECK(!xpp::auto_torus_singular(angle));
    iap.itpst=xpp::AutoPointKind::Torus;
    const auto torus_test=[&](double angle) {
        par[xpp::auto_unknown::torus_angle]=angle;
        return auto_cycle_test(iap,rap,par.data(),multipliers,&enabled);
    };
    // W419: every resonance modulo 2pi, past pi and below 0 too, and the kind it labels.
    const std::array<std::pair<double,xpp::AutoPointKind>,9> roots={{
        {-.5,xpp::AutoPointKind::Resonance4},{0.,xpp::AutoPointKind::Resonance1},
        {.5,xpp::AutoPointKind::Resonance4},{2./3,xpp::AutoPointKind::Resonance3},
        {1.,xpp::AutoPointKind::Resonance2},{4./3,xpp::AutoPointKind::Resonance3},
        {1.5,xpp::AutoPointKind::Resonance4},{2.,xpp::AutoPointKind::Resonance1},
        {3.,xpp::AutoPointKind::Resonance2}}};
    for (const auto &[root,kind]:roots) {
        const double before=torus_test(pi(root)-1e-4);
        CHECK(before*torus_test(pi(root)+1e-4)<0 && enabled);
        CHECK(xpp::auto_cycle_resonance_kind(iap.itpst,pi(root)+1e-4)==kind);
    }
    // From 0 to pi the test is AUTO-07p's polynomial, bit for bit.
    for (double angle:{1e-3,.7,pi(.5)+.1,2.5,pi(1.)})
        CHECK(torus_test(angle)==angle*(angle-pi(.5))*(angle-pi(2./3))*(angle-pi(1.)));
    // A disabled root is removed, so it cannot hide a later enabled root.
    const auto r4=std::find(xpp::auto_codim_kinds.begin(),xpp::auto_codim_kinds.end(),xpp::AutoPointKind::Resonance4);
    reaction_session.auto_state.special_points[static_cast<std::size_t>(r4-xpp::auto_codim_kinds.begin())]=false;
    par[xpp::auto_unknown::torus_angle]=pi(.5);
    CHECK(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)!=0 && enabled);
    // Without 1:1 and 1:2 the folded angle's turns at 0 and pi change no sign.
    for (auto kind:{xpp::AutoPointKind::Resonance1,xpp::AutoPointKind::Resonance2})
        reaction_session.auto_state.special_points[static_cast<std::size_t>(
            std::find(xpp::auto_codim_kinds.begin(),xpp::auto_codim_kinds.end(),kind)-xpp::auto_codim_kinds.begin())]=false;
    for (double turn:{0.,1.,2.,3.})
        CHECK(torus_test(pi(turn)-1e-4)*torus_test(pi(turn)+1e-4)>0 && enabled);
    reaction_session.auto_state.special_points.fill(false);
    CHECK(!auto_test_applies(auto_cycle_test(iap,rap,par.data(),multipliers,&enabled)) && !enabled);
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
    // Map secondary labels distinguish real +1/-1 crossings from pairs.
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::Regular,pi(1.),1e-4)==xpp::AutoPointKind::PeriodDoubling);
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::Regular,pi(.5),1e-4)==xpp::AutoPointKind::Torus);
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::Fold,pi(1.),1e-4)==xpp::AutoPointKind::FoldFlip);
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::Fold,pi(.5),1e-4)==xpp::AutoPointKind::FoldTorus);
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::PeriodDoubling,0.,1e-4)==xpp::AutoPointKind::FoldFlip);
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::PeriodDoubling,pi(.5),1e-4)==xpp::AutoPointKind::FlipTorus);
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::Torus,pi(.5),1e-4)==xpp::AutoPointKind::TorusTorus);
    CHECK(xpp::auto_map_secondary_kind(xpp::AutoPointKind::Torus,0.,1e-4)==xpp::AutoPointKind::FoldTorus);
    // Henon's exact unit-circle pair satisfies A^2-2*cos(theta)*A+I=0.
    std::string map_path="tools/models/henon_rot.odex";
    char *map_args[]={name,map_path.data(),nullptr};
    CHECK(load_args(2,map_args,1).has_value());
    auto &map_session=xpp::client_session();
    CHECK(xpp::ensure_exact_jacobian(map_session));
    map_session.numerics.jacobian=xpp::JACOBIAN_EXACT;
    iap.jac=1;
    auto &map_lib=map_session.auto_lib;
    map_lib.session=&map_session;
    map_lib.parameter_count=map_session.auto_state.parameter_layout.size();
    iap.lib=&map_lib; iap.ndm=4; iap.ndim=10;
    iap.ips=xpp::AutoProblem::DiscreteMap; iap.itpst=xpp::AutoPointKind::Torus;
    icp={map_session.auto_state.parameter_layout.solver_index(0),map_session.auto_state.parameter_layout.solver_index(1)};
    std::vector<double> mp(map_lib.parameter_count),mj(100),mdp(10*map_lib.parameter_count),mr(10);
    const double fixed=std::sqrt(2.5)-1.;
    mp[icp[0]]=1.5;
    std::array<double,10> state={fixed,fixed,0,0,1,0,0,0,xpp::math::acos(-fixed),1}, previous=state;
    fnhb(&iap,&rap,10,state.data(),previous.data(),icp.data(),mp.data(),2,mr.data(),mj.data(),mdp.data());
    for (double value:mr) CHECK(std::abs(value)<1e-6);
    // An independent central difference checks every augmented Jacobian column.
    constexpr double perturbation=1e-4; // larger than the model's inner difference step
    for (int column=0;column<10;++column) {
        auto low=state,high=state;
        low[column]-=perturbation; high[column]+=perturbation;
        std::array<double,10> fm{},fp{};
        fnhb(&iap,&rap,10,low.data(),previous.data(),icp.data(),mp.data(),0,fm.data(),nullptr,nullptr);
        fnhb(&iap,&rap,10,high.data(),previous.data(),icp.data(),mp.data(),0,fp.data(),nullptr,nullptr);
        for (int row=0;row<10;++row) {
            const double numeric=(fp[row]-fm[row])/(2*perturbation);
            CHECK(std::abs(mj[row+10*column]-numeric)<2e-5);
        }
    }
    iap.itpst=xpp::AutoPointKind::PeriodDoubling; iap.ndim=9;
    mp[icp[0]]=0.;
    std::array<double,9> flip={0,0,0,0,1/std::sqrt(2.),-1/std::sqrt(2.),0,0,-1};
    fnlp(&iap,&rap,9,flip.data(),flip.data(),icp.data(),mp.data(),2,mr.data(),mj.data(),mdp.data());
    for (int row=0;row<9;++row) CHECK(std::abs(mr[row])<1e-6);
    CHECK(std::abs(mj[4+9*4]-1.)<1e-6 && std::abs(mj[0]+1.)<1e-6);
    // W407 (#469): the torus phase condition compares the eigenfunction with the previous point's.
    // XPPAUT's u.v - v.u was identically zero, so a rotated eigenfunction satisfied it.
    iap_type ti{};
    ti.lib=&map_lib; ti.ndm=2; ti.ndim=6;
    map_lib.rotations.nrtn.assign(2,0);
    CHECK(map_lib.parameter_count>xpp::auto_unknown::torus_norm);
    std::vector<double> tpar(map_lib.parameter_count);
    const std::array<double,6> tuold{0,0,1,0.5,0,1}, tupold{};
    const std::array<double,6> tu{0,0,0.5,0.25,0.75,-0.5};
    const auto condition=[&](const std::array<double,6> &at,std::array<double,3> &fv,std::vector<double> *dint) {
        return ictr(&ti,&rap,6,tpar.data(),nullptr,3,at.data(),tuold.data(),nullptr,tupold.data(),fv.data(),
            dint?1:0,dint?dint->data():nullptr);
    };
    std::array<double,3> tf{};
    CHECK(condition(tu,tf,nullptr)==0);
    const double expected=tu[2]*tuold[4]-tu[4]*tuold[2]+tu[3]*tuold[5]-tu[5]*tuold[3];
    CHECK(std::abs(tf[1]-expected)<1e-14 && std::abs(expected)>0.1);
    const std::size_t columns=6+map_lib.parameter_count;
    std::vector<double> tdint(3*columns);
    CHECK(condition(tu,tf,&tdint)==0);
    for (std::size_t column=2;column<6;++column) {
        auto low=tu,high=tu;
        low[column]-=1e-3; high[column]+=1e-3;
        std::array<double,3> fl{},fh{};
        condition(low,fl,nullptr); condition(high,fh,nullptr);
        CHECK(std::abs(tdint[1+3*column]-(fh[1]-fl[1])/2e-3)<1e-9);
    }
    TEST_REPORT("AUTO codim-2");
}
