#include "auto_codim.h"
#include "auto_c.h"
#include "auto_unknowns.h"
#include "session.h"
#include "xpp_math.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {
// Strong resonances in increasing continued angle, in units of pi.
constexpr std::array<double,4> resonance_angles={0.,.5,2./3,1.};
constexpr std::array<xpp::AutoPointKind,4> resonance_kinds={xpp::AutoPointKind::Resonance1,
    xpp::AutoPointKind::Resonance4,xpp::AutoPointKind::Resonance3,xpp::AutoPointKind::Resonance2};

// The multiplier a fold (+1) or period-doubling (-1) curve of cycles imposes.
double imposed_multiplier(xpp::AutoPointKind start)
{
    return start==xpp::AutoPointKind::PeriodicFold ? 1. : -1.;
}

// The continued angle modulo 2*pi, folded into [0, pi] (exactly the angle
// there): exp(+-i*theta) is the same pair at theta and at 2*pi - theta.
double folded_angle(double angle)
{
    return std::abs(std::remainder(angle,pi(2.)));
}

// W419 (#481): AUTO-07p's polynomial theta*(theta-pi/2)*(theta-2pi/3)*(theta-pi)
// sees 0 to pi only (docs/auto07p-findings.md 5). It is taken of the folded
// angle, which turns back at 0 and pi, so the test meets every resonance
// modulo 2*pi; its sign flips where the fold turns at a resonance it tests
// (0 for 1:1, pi for 1:2), so the curve crossing one there still changes it.
// From 0 to pi it is AUTO-07p's value, bit for bit.
double resonance_test(const std::array<bool,xpp::auto_codim_kinds.size()> &enabled, double angle, logical *changed)
{
    int turns=0; // the nearest whole number of turns, its low bits (remquo)
    const double wrapped=std::remquo(angle,pi(2.),&turns);
    const double folded=std::abs(wrapped);
    double test=1.;
    *changed=false;
    for (std::size_t i=0;i<resonance_angles.size();++i)
        if (xpp::auto_codim_enabled(enabled,resonance_kinds[i])) {
            test*=folded-pi(resonance_angles[i]);
            *changed=true;
        }
    if (!*changed) return auto_test_not_applicable;
    // Turns at 0 passed: one fewer while the angle is below its nearest turn.
    int flips=0;
    if (xpp::auto_codim_enabled(enabled,xpp::AutoPointKind::Resonance1)) flips+=turns-(wrapped<0);
    if (xpp::auto_codim_enabled(enabled,xpp::AutoPointKind::Resonance2)) flips+=turns;
    return flips%2 ? -test : test;
}

}

namespace xpp {
void AutoCodim::prepare(std::size_t n)
{
    matrix.resize(size_product(2*n, 2*n));
    for (auto *v : {&left, &previous_left, &qr, &qi, &pr, &pi, &a, &b, &c,
                    &r, &sr, &si, &x, &plus, &minus, &work}) v->assign(n, 0.);
    rhs.resize(2*n);
    solution.resize(2*n);
}
void AutoCodim::prepare_cycle(std::size_t dimension, std::size_t original)
{
    cycle_matrix.resize(size_product(dimension,dimension));
    cycle_p0.resize(size_product(original,original));
    cycle_p1.resize(cycle_p0.size());
    cycle_rows.resize(dimension);
    cycle_pivots.resize(dimension);
}

AutoPointKind auto_map_secondary_kind(AutoPointKind start, double angle, double tolerance)
{
    const bool flip=std::abs(angle-pi(1.))<=std::sqrt(tolerance);
    const bool torus=!flip && angle>std::sqrt(tolerance);
    if (start==AutoPointKind::Regular) return flip ? AutoPointKind::PeriodDoubling : torus ? AutoPointKind::Torus : AutoPointKind::Regular;
    if (start==AutoPointKind::Fold) return flip ? AutoPointKind::FoldFlip : torus ? AutoPointKind::FoldTorus : AutoPointKind::Regular;
    if (start==AutoPointKind::PeriodDoubling) return torus ? AutoPointKind::FlipTorus : !flip ? AutoPointKind::FoldFlip : AutoPointKind::Regular;
    if (start==AutoPointKind::Torus) return flip ? AutoPointKind::FlipTorus : torus ? AutoPointKind::TorusTorus : AutoPointKind::FoldTorus;
    return AutoPointKind::Regular;
}

bool auto_torus_near_one_to_one(double angle)
{
    return auto_cycle_resonance_kind(AutoPointKind::Torus,angle)==AutoPointKind::Resonance1;
}

bool auto_torus_singular(double angle)
{
    const auto kind=auto_cycle_resonance_kind(AutoPointKind::Torus,angle);
    return kind==AutoPointKind::Resonance1 || kind==AutoPointKind::Resonance2;
}

AutoPointKind auto_cycle_resonance_kind(AutoPointKind start, double angle)
{
    if (start==AutoPointKind::Fold || start==AutoPointKind::PeriodicFold) return AutoPointKind::Resonance1;
    if (start==AutoPointKind::PeriodDoubling) return AutoPointKind::Resonance2;
    // The torus formulation continues theta unwrapped: its resonances are
    // those of theta modulo 2*pi (W419).
    const double folded=folded_angle(angle);
    std::size_t nearest=0;
    for (std::size_t i=1;i<resonance_angles.size();++i)
        if (std::abs(folded-pi(resonance_angles[i]))<std::abs(folded-pi(resonance_angles[nearest]))) nearest=i;
    return resonance_kinds[nearest];
}
}

namespace {
double dot(std::span<const double> a, std::span<const double> b)
{
    return std::inner_product(a.begin(), a.end(), b.begin(), 0.);
}

// Kuznetsov's directional derivatives, as used by AUTO-07p 0.9.4's
// CP/GH tests. Work with directions rather than building derivative tensors.
class Derivatives {
    iap_type &iap;
    rap_type &rap;
    double *par;
    integer *icp;
    const double *u;
    xpp::AutoCodim &w;
    const integer n;
    double h;

    void value(std::span<const double> direction, double distance, std::vector<double> &out)
    {
        for (integer i=0; i<n; ++i) w.x[i]=u[i]+distance*direction[i];
        double unused=0;
        funi(&iap, &rap, n, w.x.data(), w.x.data(), icp, par, 0, out.data(), &unused, &unused);
    }
public:
    Derivatives(iap_type &i, rap_type &r, double *p, integer *indices, const double *state)
        : iap(i), rap(r), par(p), icp(indices), u(state), w(i.lib->codim), n(i.ndm)
    {
        double scale=0;
        for (integer j=0; j<n; ++j) scale=std::max(scale,std::abs(u[j]));
        // Balance roundoff against the truncation error of second differences.
        h=xpp::math::pow(std::numeric_limits<double>::epsilon(),1./3)*(1+scale);
    }
    void quadratic(std::span<const double> v, std::vector<double> &out)
    {
        value(v,h,w.plus);
        value(v,-h,w.minus);
        // The corrected equilibrium still has a Newton residual. Remove it
        // before dividing by h^2: a multilinear derivative has no constant term.
        double unused=0;
        funi(&iap,&rap,n,u,u,icp,par,0,w.work.data(),&unused,&unused);
        for (integer i=0; i<n; ++i) out[i]=(w.plus[i]+w.minus[i]-2*w.work[i])/(h*h);
    }
    double mixed(std::span<const double> p, std::span<const double> q, std::span<const double> r)
    {
        // Four corners give p.B(q,r), with no Jacobian differencing.
        double result=0;
        for (double signq : {-1.,1.}) for (double signr : {-1.,1.}) {
            for (integer i=0; i<n; ++i) w.work[i]=signq*q[i]+signr*r[i];
            value(w.work,h,w.plus);
            result+=signq*signr*dot(p,w.plus);
        }
        return result/(4*h*h);
    }
    double cubic(std::span<const double> p, std::span<const double> q)
    {
        // The symmetric third difference uses epsilon^(1/4) state offsets.
        const double step=h*xpp::math::pow(std::numeric_limits<double>::epsilon(),-1./12);
        value(q,3*step,w.plus);
        double result=dot(p,w.plus);
        value(q,step,w.plus); result-=3*dot(p,w.plus);
        value(q,-step,w.plus); result+=3*dot(p,w.plus);
        value(q,-3*step,w.plus); result-=dot(p,w.plus);
        return result/(8*step*step*step);
    }
};

void left_null(iap_type &iap, const double *aa, integer stride)
{
    auto &w=iap.lib->codim;
    const integer n=iap.ndm;
    // The fold's eigenvector block is exactly the user's Jacobian.
    for (integer j=0; j<n; ++j) for (integer i=0; i<n; ++i)
        w.matrix[i+n*j]=aa[n+j+stride*(n+i)];
    xpp::nlvc(iap.lib->fp9,n,n,1,w.matrix.data(),w.left.data());
    double length=std::sqrt(dot(w.left,w.left));
    if (dot(w.left,w.previous_left)<0) length=-length;
    for (auto &v:w.left) v/=length;
    w.previous_left=w.left;
}

double lyapunov(iap_type &iap, rap_type &rap, double *par, integer *icp,
                const double *u, const double *aa, integer stride)
{
    auto &w=iap.lib->codim;
    const integer n=iap.ndm, m=2*n;
    const double omega=std::sqrt(u[2*n]);
    for (integer i=0; i<n; ++i) {
        w.qr[i]=u[n+i];
        w.qi[i]=0;
        for (integer j=0; j<n; ++j) w.qi[i]-=aa[i+stride*j]*u[n+j]/omega;
    }
    const double phase=xpp::math::atan2(-2*dot(w.qr,w.qi),dot(w.qr,w.qr)-dot(w.qi,w.qi))/2;
    const double cosine=xpp::math::cos(phase), sine=xpp::math::sin(phase);
    for (integer i=0; i<n; ++i) {
        const double real=w.qr[i];
        w.qr[i]=cosine*real-sine*w.qi[i];
        w.qi[i]=sine*real+cosine*w.qi[i];
    }
    const double length=std::sqrt(dot(w.qr,w.qr)+dot(w.qi,w.qi));
    for (integer i=0; i<n; ++i) { w.qr[i]/=length; w.qi[i]/=length; }
    std::fill(w.matrix.begin(),w.matrix.end(),0.);
    for (integer j=0; j<n; ++j) for (integer i=0; i<n; ++i) {
        w.matrix[i+m*j]=aa[j+stride*i];
        w.matrix[n+i+m*(n+j)]=aa[j+stride*i];
    }
    for (integer i=0; i<n; ++i) {
        w.matrix[i+m*(n+i)]=-omega;
        w.matrix[n+i+m*i]=omega;
    }
    xpp::nlvc(iap.lib->fp9,m,m,2,w.matrix.data(),w.solution.data());
    const double alpha=dot({w.solution.data(),static_cast<size_t>(n)},w.qr)
        +dot({w.solution.data()+n,static_cast<size_t>(n)},w.qi);
    const double beta=dot({w.solution.data()+n,static_cast<size_t>(n)},w.qr)
        -dot({w.solution.data(),static_cast<size_t>(n)},w.qi);
    const double norm=alpha*alpha+beta*beta;
    for (integer i=0; i<n; ++i) {
        w.pr[i]=(alpha*w.solution[i]+beta*w.solution[n+i])/norm;
        w.pi[i]=(alpha*w.solution[n+i]-beta*w.solution[i])/norm;
    }
    Derivatives derivatives(iap,rap,par,icp,u);
    derivatives.quadratic(w.qr,w.a);
    derivatives.quadratic(w.qi,w.b);
    for (integer i=0; i<n; ++i) w.c[i]=w.qr[i]+w.qi[i];
    derivatives.quadratic(w.c,w.c);
    for (integer i=0; i<n; ++i) w.c[i]=(w.c[i]-w.a[i]-w.b[i])/2;
    for (integer j=0; j<n; ++j) for (integer i=0; i<n; ++i) w.matrix[i+n*j]=aa[i+stride*j];
    for (integer i=0; i<n; ++i) w.rhs[i]=w.a[i]+w.b[i];
    double determinant;
    xpp::ge(iap.lib->fp9,iap.iid,n,n,w.matrix.data(),1,n,w.r.data(),n,w.rhs.data(),&determinant);
    std::fill(w.matrix.begin(),w.matrix.end(),0.);
    for (integer j=0; j<n; ++j) for (integer i=0; i<n; ++i) {
        w.matrix[i+m*j]=-aa[i+stride*j];
        w.matrix[n+i+m*(n+j)]=-aa[i+stride*j];
    }
    for (integer i=0; i<n; ++i) {
        w.matrix[i+m*(n+i)]=-2*omega;
        w.matrix[n+i+m*i]=2*omega;
        w.rhs[i]=w.a[i]-w.b[i]; w.rhs[n+i]=2*w.c[i];
    }
    xpp::ge(iap.lib->fp9,iap.iid,m,m,w.matrix.data(),1,m,w.solution.data(),m,w.rhs.data(),&determinant);
    for (integer i=0; i<n; ++i) { w.sr[i]=w.solution[i]; w.si[i]=w.solution[n+i]; }
    const double sigma=derivatives.mixed(w.pr,w.qr,w.r)+derivatives.mixed(w.pi,w.qi,w.r);
    const double delta=derivatives.mixed(w.pr,w.qr,w.sr)+derivatives.mixed(w.pr,w.qi,w.si)
        +derivatives.mixed(w.pi,w.qr,w.si)-derivatives.mixed(w.pi,w.qi,w.sr);
    double gamma=2*(derivatives.cubic(w.pr,w.qr)+derivatives.cubic(w.pi,w.qi))/3;
    for (integer i=0; i<n; ++i) { w.a[i]=w.pr[i]+w.pi[i]; w.b[i]=w.qr[i]+w.qi[i]; }
    gamma+=derivatives.cubic(w.a,w.b)/6;
    for (integer i=0; i<n; ++i) { w.a[i]=w.pr[i]-w.pi[i]; w.b[i]=w.qr[i]-w.qi[i]; }
    gamma+=derivatives.cubic(w.a,w.b)/6;
    return (gamma-2*sigma+delta)/(2*omega);
}
}

std::size_t auto_cycle_place_imposed(xpp::AutoPointKind start, std::span<doublecomplex> spectrum)
{
    // At a resonance roundoff splits the double +1/-1 into a conjugate pair:
    // that pair goes there whole (W405; AUTO-07p's PVSPBV takes reals only).
    const double imposed=imposed_multiplier(start);
    const auto distance=[imposed](const doublecomplex &m) { return xpp::math::hypot(m.r-imposed,m.i); };
    auto nearest=std::min_element(spectrum.begin()+1,spectrum.end(),
        [&](const doublecomplex &a, const doublecomplex &b) { return distance(a)<distance(b); });
    if (nearest->i<0 && nearest-1>spectrum.begin()) --nearest; // a+bi precedes a-bi
    const std::ptrdiff_t width=nearest->i!=0 && nearest+1<spectrum.end() ? 2 : 1;
    std::rotate(spectrum.begin()+1,nearest,nearest+width);
    return static_cast<std::size_t>(1+width);
}

double auto_cycle_test(const iap_type &iap, const rap_type &rap, const double *par,
                       std::span<const doublecomplex> spectrum, logical *changed)
{
    const auto start=iap.itpst;
    const double angle=par[xpp::auto_unknown::torus_angle];
    const auto kind=xpp::auto_cycle_resonance_kind(start,angle);
    const auto &enabled=iap.lib->session->auto_state.special_points;
    if (start==xpp::AutoPointKind::Torus) {
        return resonance_test(enabled,angle,changed);
    }
    *changed=xpp::auto_codim_enabled(enabled,kind);
    if (!*changed) return auto_test_not_applicable;
    if (start!=xpp::AutoPointKind::PeriodicFold && start!=xpp::AutoPointKind::PeriodDoubling) {
        *changed=false;
        return auto_test_not_applicable;
    }
    // spectrum[0] is the neutral multiplier; [1] is the curve's imposed
    // +1/-1, or one of the conjugate pair it splits into with [2] at the
    // resonance itself. Test [2], as AUTO-07p's periodic.f90 PVSPBV does.
    constexpr double imposed_multiplier_tolerance=.05; // AUTO-07p's accuracy band.
    const double imposed=imposed_multiplier(start);
    const auto &fixed=spectrum[1], &remaining=spectrum[2];
    *changed=*changed && std::abs(remaining.i)<std::sqrt(std::sqrt(rap.epss)) &&
        imposed*remaining.r>0 && xpp::math::hypot(fixed.r-imposed,fixed.i)<imposed_multiplier_tolerance;
    return *changed ? xpp::math::hypot(remaining.r,remaining.i)-1 : auto_test_not_applicable;
}

// The tests read dfdu as solvae left it, the converged point's Jacobian (W421).
FNCS_TYPE_AE(fnbtae)
{
    *chng=true;
    if (iap->itpst==xpp::AutoPointKind::Hopf) return u[2*iap->ndm];
    left_null(*iap,dfdu,iap->ndim);
    return dot({u+iap->ndm,static_cast<size_t>(iap->ndm)},iap->lib->codim.left);
}

FNCS_TYPE_AE(fncpae)
{
    *chng=true;
    left_null(*iap,dfdu,iap->ndim);
    auto &w=iap->lib->codim;
    Derivatives derivatives(*iap,*rap,par,icp,u);
    derivatives.quadratic({u+iap->ndm,static_cast<size_t>(iap->ndm)},w.a);
    return dot(w.left,w.a);
}

FNCS_TYPE_AE(fnghae)
{
    *chng=u[2*iap->ndm]>0 && iap->itp.kind!=xpp::AutoPointKind::BogdanovTakens;
    if (!*chng) return auto_test_not_applicable;
    const double coefficient=lyapunov(*iap,*rap,par,icp,u,dfdu,iap->ndim);
    if (iap->iid>=2) xpp::print(iap->lib->fp9,"GH first Lyapunov coefficient: {:.9E}\n",coefficient);
    return coefficient;
}

// Maps use the same double-eigenvalue test as equilibrium folds, with
// A-I (fold) or A+I (flip); a torus curve tests its continued angle.
FNCS_TYPE_AE(fnrnae)
{
    auto &w=iap->lib->codim;
    if (iap->itpst==xpp::AutoPointKind::Torus) {
        const double angle=u[2*iap->ndm];
        w.map_kind=xpp::auto_cycle_resonance_kind(iap->itpst,angle);
        return resonance_test(iap->lib->session->auto_state.special_points,angle,chng);
    }
    w.map_kind=xpp::auto_cycle_resonance_kind(iap->itpst,0.);
    *chng=xpp::auto_codim_enabled(iap->lib->session->auto_state.special_points,w.map_kind);
    left_null(*iap,dfdu,iap->ndim);
    return dot({u+iap->ndm,static_cast<size_t>(iap->ndm)},w.left);
}
