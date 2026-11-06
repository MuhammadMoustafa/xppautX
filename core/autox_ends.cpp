#include "autox_unknowns.h"
/* The end at a Hopf point, the branch closed on itself and the same-point rule (autox_ends.h, W604). */
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "autox_c.h"
#include "autox_ends.h"
#include "autox_global.h"
#include "autox_state.h"
#include "equilibria.h"
#include "model.h"
#include "session.h"
#include "xpp_io.h"

namespace xpp {

double autox_orbit_amplitude(std::span<const double> high, std::span<const double> low, std::span<const double> mean)
{
    double amplitude = 0;
    for (std::size_t k = 0; k < mean.size(); ++k) amplitude = std::max(amplitude, (high[k] - low[k]) / (1 + std::abs(mean[k])));
    return amplitude;
}

bool autox_amplitude_watch_add(AutoxAmplitudeWatch &watch, double amplitude, double parameter, bool first_point)
{
    if (first_point) watch = AutoxAmplitudeWatch{};
    /* the cycles of a Hopf point are one family on both sides of it, so a continuation that steps over the point
       turns round in the parameter there (a cycle and its phase shifts: W611), where the line of the last two
       shrinking cycles' squared amplitude puts the point no further than one step beyond the last cycle: the
       step turned at the Hopf point, whatever the amplitude (none is waited for) */
    watch.passed.reset();
    if (const auto hopf = autox_hopf_parameter(watch)) {
        const double step = watch.cycles[1].parameter - watch.cycles[0].parameter, back = parameter - watch.cycles[1].parameter;
        const double reach = (*hopf - watch.cycles[1].parameter) / step;
        if (step * back < 0 && reach >= 0 && reach <= 1) watch.passed = hopf;
    }
    watch.shrinking = autox_trend_run(watch.shrinking, watch.last, amplitude, false);
    watch.last = amplitude;
    if (amplitude > autox_hopf_end_amplitude) watch.cycles = {watch.cycles[1], {parameter, amplitude}};
    return watch.passed.has_value() || (watch.shrinking >= autox_hopf_end_shrinking_points && amplitude <= autox_hopf_end_amplitude);
}

std::optional<double> autox_hopf_parameter(const AutoxAmplitudeWatch &watch)
{
    const auto &[older, newer] = watch.cycles;
    const double squares = older.amplitude * older.amplitude - newer.amplitude * newer.amplitude;
    if (!(squares > 0)) return std::nullopt; /* two cycles that shrink (NaN: fewer than two) */
    return newer.parameter - newer.amplitude * newer.amplitude * (newer.parameter - older.parameter) / -squares;
}

std::optional<AutoxHopfEnd> autox_hopf_end(const iap_type &iap, const double *par, const std::ptrdiff_t *icp,
                                           std::span<const double> mean, const AutoxAmplitudeWatch &watch)
{
    Session &s = *iap.run->session;
    /* autoX's parameters with the continued one at the Hopf point's, which the model is evaluated at */
    std::vector<double> at(par, par + 2 * iap.run->parameter_count);
    const double here = par[icp[0]];
    at[icp[0]] = watch.passed.value_or(autox_hopf_parameter(watch).value_or(here)); /* the line before this point joined the cycles */
    const auto model = autox_equilibrium_model(iap, at.data(), icp, mean);
    auto found = equilibrium_near(s, *model, mean);
    if (!found) {
        AutoxStop &stop = iap.run->stop;
        if (!std::exchange(stop.hopf_failure_noted, true))
            autox_stop_note(*iap.run, xpp::format("Br {} Pt {}: the cycles' amplitude fell to {}, but whether it is a Hopf point could not be checked: {}",
                                                  f2c::abs(iap.ibr), iap.ntot, watch.last, found.error().text()));
        return std::nullopt;
    }
    if (!found->found) return std::nullopt;
    /* the complex pair nearest the imaginary axis */
    std::optional<std::size_t> pair;
    for (std::size_t i = 0; i < found->re.size(); ++i)
        if (found->im[i] != 0 && (!pair || std::abs(found->re[i]) < std::abs(found->re[*pair]))) pair = i;
    if (!pair || !(std::abs(found->re[*pair]) <= autox_hopf_end_imaginary * std::abs(found->im[*pair]))) return std::nullopt;
    AutoxHopfEnd end;
    end.parameter = at[icp[0]];
    end.equilibrium = std::move(found->x);
    end.real = found->re[*pair];
    end.frequency = std::abs(found->im[*pair]);
    end.period = 2 * std::numbers::pi / end.frequency;
    end.amplitude = watch.last;
    end.passed = watch.passed.has_value();
    return end;
}

void autox_hopf_end_apply(const iap_type &iap, const AutoxHopfEnd &end, double *par, const std::ptrdiff_t *icp,
                          std::ptrdiff_t ndxloc, double *ups)
{
    par[icp[0]] = end.parameter;
    par[autox_unknown::period] = end.period;
    const std::ptrdiff_t ndim = iap.ndim;
    for (std::ptrdiff_t i = 0; i < ndim * iap.ncol; ++i)
        for (std::ptrdiff_t j = 0; j < ndxloc; ++j) ups[j + i * ndxloc] = end.equilibrium[static_cast<std::size_t>(i % ndim)];
}

std::string autox_hopf_end_text(const Session &s, const AutoxHopfEnd &end)
{
    const auto g = autox_number_text;
    if (end.passed)
        return xpp::format("the cycles' amplitude shrank over the steps and the step to amplitude {} (of their variables' size) turned round in the parameter at the Hopf parameter {} (the squared amplitude's line to zero); there the equilibrium has the imaginary pair {} +- {}i (real part at most {} of the imaginary), period {}, at {}: a Hopf point, point type {} ({})",
                           g(end.amplitude), g(end.parameter), g(end.real), g(end.frequency), g(autox_hopf_end_imaginary), g(end.period),
                           autox_equilibrium_text(s.model(), end.equilibrium), autox_point_kind_name(AutoxPointKind::HopfEnd),
                           autox_point_kind_mark(AutoxPointKind::HopfEnd));
    return xpp::format("the cycles' amplitude fell to {} of their variables' size (at most {}) over the last {} points; at the Hopf parameter {} (the squared amplitude's line to zero) the equilibrium has the imaginary pair {} +- {}i (real part at most {} of the imaginary), period {}, at {}: a Hopf point, point type {} ({})",
                       g(end.amplitude), g(autox_hopf_end_amplitude), autox_hopf_end_shrinking_points, g(end.parameter),
                       g(end.real), g(end.frequency), g(autox_hopf_end_imaginary), g(end.period),
                       autox_equilibrium_text(s.model(), end.equilibrium), autox_point_kind_name(AutoxPointKind::HopfEnd),
                       autox_point_kind_mark(AutoxPointKind::HopfEnd));
}

bool autox_same_kind(AutoxPointKind a, AutoxPointKind b)
{
    const auto ends_at = [](AutoxPointKind end, AutoxPointKind point) {
        return (end == AutoxPointKind::HopfEnd && point == AutoxPointKind::Hopf) || (end == AutoxPointKind::Snic && point == AutoxPointKind::Fold);
    };
    return a == b || ends_at(a, b) || ends_at(b, a);
}

namespace {

/* the largest ratio of |a - b| to epss * (1 + |a|) over two lists; infinite when they differ in size */
double list_gap(std::span<const double> a, std::span<const double> b, double epss)
{
    if (a.size() != b.size()) return INFINITY;
    double gap = 0;
    for (std::size_t i = 0; i < a.size(); ++i) gap = std::max(gap, std::abs(a[i] - b[i]) / (epss * (1 + std::abs(a[i]))));
    return gap;
}

AutoxPointId branch_point_id(const iap_type &iap, const double *par, const std::ptrdiff_t *icp, std::span<const double> states)
{
    AutoxPointId id;
    for (std::ptrdiff_t i = 0; i < iap.nfpr; ++i) id.parameters.push_back(par[icp[i]]);
    id.states.assign(states.begin(), states.end());
    return id;
}

} // namespace

double autox_point_gap(const AutoxPointId &a, const AutoxPointId &b, double epss)
{
    if (!autox_same_kind(a.kind, b.kind)) return INFINITY;
    double gap = std::max(list_gap(a.parameters, b.parameters, epss), list_gap(a.states, b.states, epss));
    if (a.frequency && b.frequency) {
        const double f[] = {*a.frequency}, h[] = {*b.frequency};
        gap = std::max(gap, list_gap(f, h, epss));
    }
    return gap;
}

bool autox_same_point(const AutoxPointId &a, const AutoxPointId &b, double epss)
{
    return autox_point_gap(a, b, epss) <= autox_same_point_factor;
}

void autox_branch_stored(iap_type &iap, std::ptrdiff_t ntot, double epss, const double *par, const std::ptrdiff_t *icp,
                         std::span<const double> states)
{
    AutoxPointId id = branch_point_id(iap, par, icp, states);
    if (ntot == 1) iap.run->branch_start = AutoxBranchStart{std::move(id), false};
    else if (iap.run->branch_start && !autox_same_point(iap.run->branch_start->point, id, epss))
        iap.run->branch_start->left = true;
}

bool autox_branch_closes(iap_type &iap, double epss, const double *par, const std::ptrdiff_t *icp, std::span<const double> states)
{
    if (!iap.run->branch_start || !iap.run->branch_start->left) return false;
    const AutoxPointId here = branch_point_id(iap, par, icp, states);
    if (!autox_same_point(iap.run->branch_start->point, here, epss)) return false;
    iap.run->stop.closure_gap = autox_point_gap(iap.run->branch_start->point, here, epss);
    iap.run->stop.closure_epss = epss;
    return true;
}

std::string autox_closure_text(double gap, double epss)
{
    return xpp::format("the branch closed on itself: back at its start, every parameter and state within {} times special_point_tolerance = {} times (1 + the start's value) of the start's, the largest difference {} of that: point type {} ({})",
                       autox_number_text(autox_same_point_factor), autox_number_text(epss), autox_number_text(gap),
                       autox_point_kind_name(AutoxPointKind::BranchClosed), autox_point_kind_mark(AutoxPointKind::BranchClosed));
}

} // namespace xpp
