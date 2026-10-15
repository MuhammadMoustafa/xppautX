#include "auto_unknowns.h"
/* The stability values of AUTO's points: what AUTO computed, for which
   point, and what a run's first point takes (auto_stability.h). */
#include <cmath>
#include <cstdlib>
#include <span>
#include <vector>
#include <tuple>
#include <limits>

#include "auto_stability.h"
#include "auto_stability_auto.h"
#include "xpp_math.h"
#include "auto_nox.h"
#include "auto_state.h"

namespace xpp {

namespace {



/* the values v into evr/evi (evi as long as evr), zeros past what v holds */
void copy_out(const AutoStability::Values &v, std::span<double> evr, std::span<double> evi)
{
    for (size_t i = 0; i < evr.size(); i++) {
        bool in = i < v.re.size();
        evr[i] = in ? v.re[i] : 0.0;
        evi[i] = in ? v.im[i] : 0.0;
    }
}

void forget(AutoStability::Values &v)
{
    v.br = v.pt = 0;
    v.re.clear();
    v.im.clear();
}

/* FUNI request for state and parameter Jacobians. */
constexpr integer BOTH_JACOBIANS = 2;

} // namespace

double auto_hopf_axis_tolerance(Matrix<const double> jacobian, size_t n)
{
    double trace = 0, scale = 0;
    for (size_t i = 0; i < n; ++i) {
        trace += jacobian(i, i);
        for (size_t j = 0; j < n; ++j)
            scale = std::max(scale, std::abs(jacobian(i, j)));
    }
    // AUTO-07p equilibrium.f90:437-453: even dimension, negligible trace.
    constexpr double conservative_axis_tolerance = 1e-5;
    return n % 2 == 0 && std::abs(trace) < auto_hmach * scale
        ? conservative_axis_tolerance : 0;
}

std::optional<doublecomplex> auto_hopf_pair(std::span<const doublecomplex> spectrum,
                                          double tolerance)
{
    const auto distance = [tolerance](const doublecomplex &value) {
        return std::abs(value.r) < tolerance ? 0 : std::abs(value.r);
    };
    double nearest = std::numeric_limits<double>::infinity(), scale = 0;
    for (const auto &value : spectrum) {
        scale = std::max(scale, std::abs(value.r) + std::abs(value.i));
        if (value.i != 0) nearest = std::min(nearest, distance(value));
    }
    // Equal real parts (e.g. equally damped modes) differ by roundoff between
    // eigensolvers. Use n machine epsilons at the spectrum's scale as a tie,
    // in addition to AUTO-07p's conservative near-axis band.
    const double real_part_roundoff = spectrum.size() * std::numeric_limits<double>::epsilon() * scale;
    const auto rank = [](const doublecomplex &value) {
        return std::tuple{std::abs(value.i), value.r, -value.i};
    };
    std::optional<doublecomplex> selected;
    for (const auto &value : spectrum) {
        if (value.i != 0 && distance(value) <= nearest + real_part_roundoff &&
            (!selected || rank(value) < rank(*selected)))
            selected = value;
    }
    return selected;
}

void auto_stability_step_start(AutoStability &st, int br, int pt)
{
    const bool label_solution = st.has_first && st.first.br == 0;
    if (st.computed.pt != 0 && st.computed.br == std::abs(br) && st.computed.pt == std::abs(pt))
        st.previous = st.computed;
    else if (label_solution && std::abs(pt) == st.first.pt) {
        st.previous = st.first;
    } else
        forget(st.previous);
    // Consume label eligibility even when computed values won: startup's
    // unstored seed and an internal switch may reuse the first point's ID.
    if (label_solution) st.first.br = std::abs(br);
    // The predictor changes the solution, even within a special-point search.
    forget(st.computed);
}

bool auto_stability_step_failed(AutoStability &st, int br, int pt)
{
    st.computed = st.previous;
    if (st.computed.pt != 0) {
        st.computed.br = std::abs(br);
        st.computed.pt = std::abs(pt);
    }
    return st.computed.pt != 0;
}

void auto_stability_equilibrium_restored(iap_type &iap, rap_type &rap, double *par, integer *icp,
    FUNI_TYPE((*funi)), double *u, double *uold, double *f, double *dfdu, double *dfdp)
{
    (*funi)(&iap, &rap, iap.ndim, u, uold, icp, par, BOTH_JACOBIANS, f, dfdu, dfdp);
    logical chng;
    const auto saved_nins = iap.nins;
    const auto saved_period = par[xpp::auto_unknown::period];
    // fnhbae's uniform FNCS interface has unused arguments: the eigenvalue
    // calculation itself consumes only the context, parameters and Jacobian.
    fnhbae(&iap, &rap, par, nullptr, &chng, nullptr, nullptr, nullptr,
           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
           dfdu, nullptr, {});
    iap.nins = saved_nins;
    par[xpp::auto_unknown::period] = saved_period;
}

void auto_stability_multipliers_restored(AutoStability &st, const iap_type &iap, double *p0, double *p1)
{
    integer ndim = iap.ndim, iid = iap.iid;
    std::vector<doublecomplex> ev(ndim);
    flowkm(iap.lib->fp9, &ndim, p0, p1, &iid, nullptr, ev.data(), false);
    auto_stability_computed(st, iap.ibr, iap.ntot + 1, {&ev[0].r, 2 * ev.size()}, AUTO_STABILITY_PERIODIC);
}

void auto_stability_computed(AutoStability &st, int br, int pt, std::span<const double> values, AutoStabilityKind kind)
{
    const size_t n = values.size() / 2;
    AutoStability::Values &computed = st.computed;
    try {
        computed.br = std::abs(br);
        computed.pt = std::abs(pt);
        computed.re.assign(n, 0.0);
        computed.im.assign(computed.re.size(), 0.0);
        for (size_t i = 0; i < n; i++) {
            double r = values[2 * i], im = values[2 * i + 1];
            if (kind == AUTO_STABILITY_STEADY) { /* e^lambda */
                double er = xpp::math::exp(r);
                computed.re[i] = er * xpp::math::cos(im);
                computed.im[i] = er * xpp::math::sin(im);
            } else {
                computed.re[i] = r;
                computed.im[i] = im;
            }
        }
    } catch (...) {
        forget(computed);
    }
}

int auto_stability_run_start(AutoStability &st, AutoStabilityKind run, int isw, AutoStabilityKind label, AutoPointCode label_itp,
                             std::span<const double> evr, std::span<const double> evi)
{
    AutoStability::Values &first = st.first;
    bool &has_first = st.has_first;
    forget(st.computed);
    forget(st.previous);
    forget(first);
    has_first = false;
    bool same = label != AUTO_STABILITY_NONE && label == run &&
                (run == AUTO_STABILITY_STEADY || run == AUTO_STABILITY_PERIODIC) &&
                !(run == AUTO_STABILITY_PERIODIC && isw == -1 && label_itp.kind == AutoPointKind::PeriodDoubling);
    if (!same || evr.empty() || evr.size() != evi.size()) return 0;
    try {
        first.pt = 1;
        first.re.assign(evr.begin(), evr.end());
        first.im.assign(evi.begin(), evi.end());
        has_first = true;
    } catch (...) {
        forget(first);
    }
    return has_first ? 1 : 0;
}

int auto_stability_for(const AutoStability &st, int br, int pt, std::span<double> evr, std::span<double> evi)
{
    const AutoStability::Values &computed = st.computed, &first = st.first;
    const bool has_first = st.has_first;
    if (computed.pt != 0 && computed.br == std::abs(br) && computed.pt == std::abs(pt)) {
        copy_out(computed, evr, evi);
        return 1;
    }
    if (has_first && std::abs(pt) == first.pt && (first.br == 0 || first.br == std::abs(br))) {
        copy_out(first, evr, evi);
        return 1;
    }
    copy_out(AutoStability::Values{}, evr, evi);
    return 0;
}

} // namespace xpp
