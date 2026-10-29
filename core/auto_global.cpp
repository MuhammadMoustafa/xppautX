#include "auto_unknowns.h"
/* The global end of a periodic branch (auto_global.h, W518). */
#include <algorithm>
#include <cmath>
#include <vector>

#include "auto_c.h"
#include "auto_global.h"
#include "equilibria.h"
#include "model.h"
#include "session.h"
#include "xpp_io.h"
#include "xpp_math.h"

namespace xpp {

bool auto_period_watch_add(AutoPeriodWatch &watch, double period, bool first_point)
{
    if (first_point) watch = AutoPeriodWatch{};
    watch.growing = period > watch.last ? watch.growing + 1 : 0;
    watch.last = period;
    watch.shortest = std::min(watch.shortest, period);
    return watch.growing >= auto_global_growing_points && period >= auto_global_growth_factor * watch.shortest;
}

void auto_approach_judge(AutoApproach &approach)
{
    approach.spectral_radius = 0;
    for (std::size_t i = 0; i < approach.re.size(); ++i)
        approach.spectral_radius = std::max(approach.spectral_radius, xpp::math::hypot(approach.re[i], approach.im[i]));
    const double zero = auto_global_zero_eigenvalue * approach.spectral_radius;
    approach.unstable = approach.stable = 0;
    bool zero_real = false;
    for (std::size_t i = 0; i < approach.re.size(); ++i) {
        if (approach.re[i] > zero) ++approach.unstable;
        else if (approach.re[i] < -zero) ++approach.stable;
        else if (approach.im[i] == 0) zero_real = true;
    }
    if (zero_real) approach.kind = AutoApproachKind::zero_eigenvalue;
    else if (approach.exists && approach.unstable > 0 && approach.stable > 0) approach.kind = AutoApproachKind::saddle;
    else approach.kind = AutoApproachKind::other;
}

AutoPointKind auto_global_kind(const std::vector<AutoApproach> &approaches)
{
    std::size_t saddles = 0, folds = 0;
    for (const AutoApproach &a : approaches) {
        if (a.kind == AutoApproachKind::saddle) ++saddles;
        else if (a.kind == AutoApproachKind::zero_eigenvalue) ++folds;
        else return AutoPointKind::End;
    }
    if (folds == 1 && saddles == 0) return AutoPointKind::Snic;
    if (saddles == 1 && folds == 0) return AutoPointKind::Homoclinic;
    if (saddles >= 2 && folds == 0) return AutoPointKind::Heteroclinic;
    return AutoPointKind::End;
}

std::vector<std::size_t> auto_slow_points(std::span<const double> speeds, double ratio)
{
    std::vector<std::size_t> slowest;
    const std::size_t n = speeds.size();
    if (n == 0) return slowest;
    const double limit = ratio * *std::max_element(speeds.begin(), speeds.end());
    const auto slow = [&](std::size_t i) { return speeds[i % n] <= limit; };
    for (std::size_t first = 0; first < n; ++first) {
        if (!slow(first) || slow(first + n - 1)) continue; /* the start of a stretch */
        std::size_t best = first;
        for (std::size_t i = first; slow(i) && i < first + n; ++i)
            if (speeds[i % n] < speeds[best]) best = i % n;
        slowest.push_back(best);
    }
    /* an orbit that is slow all round has no start: its slowest point */
    if (slowest.empty() && slow(0)) slowest.push_back(static_cast<std::size_t>(std::min_element(speeds.begin(), speeds.end()) - speeds.begin()));
    std::stable_sort(slowest.begin(), slowest.end(), [&](std::size_t a, std::size_t b) { return speeds[a] < speeds[b]; });
    return slowest;
}

namespace {

double distance(const std::vector<double> &a, const std::vector<double> &b)
{
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) sum += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt(sum);
}

} // namespace

std::optional<AutoGlobalEnd> auto_global_end(const iap_type &iap, const double *par, const std::ptrdiff_t *icp,
                                             const double *ups, std::ptrdiff_t ndxloc, double period, double shortest)
{
    Session &s = *iap.lib->session;
    const std::size_t n = static_cast<std::size_t>(iap.ndm), ntst = static_cast<std::size_t>(iap.ntst),
                      ncol = static_cast<std::size_t>(iap.ncol), count = ntst * ncol;
    /* the orbit's mesh points (the last, AUTO's ups(ntst), is the first again), and the speed |f| at each */
    std::vector<double> points(count * n), speeds(count), f(n);
    std::vector<double> low(n, INFINITY), high(n, -INFINITY);
    for (std::size_t m = 0; m < count; ++m) {
        for (std::size_t k = 0; k < n; ++k) {
            const double v = ups[m / ncol + (m % ncol * static_cast<std::size_t>(iap.ndim) + k) * static_cast<std::size_t>(ndxloc)];
            points[m * n + k] = v;
            low[k] = std::min(low[k], v);
            high[k] = std::max(high[k], v);
        }
        func(s, iap.ndm, &points[m * n], icp, iap.nfpr, par, 0, f.data(), nullptr, nullptr);
        double sum = 0;
        for (const double v : f) sum += v * v;
        speeds[m] = std::sqrt(sum);
    }
    AutoGlobalEnd end;
    end.period = period;
    end.shortest = shortest;
    const double slowest = *std::min_element(speeds.begin(), speeds.end());
    if (!(slowest <= auto_global_slow_ratio * *std::max_element(speeds.begin(), speeds.end()))) return std::nullopt;

    double extent = 0;
    for (std::size_t k = 0; k < n; ++k) extent += (high[k] - low[k]) * (high[k] - low[k]);
    extent = std::sqrt(extent);
    for (const std::size_t m : auto_slow_points(speeds, auto_global_slow_ratio)) {
        const std::span<const double> start(&points[m * n], n);
        auto found = equilibrium_near(s, start);
        if (!found) { /* the model failed to evaluate: the branch still ends, unnamed, with the reason */
            end.failed = found.error().text();
            end.approaches.clear();
            return end;
        }
        AutoApproach approach;
        approach.x = std::move(found->x);
        approach.exists = found->found;
        approach.re = std::move(found->re);
        approach.im = std::move(found->im);
        auto_approach_judge(approach);
        const bool known = std::any_of(end.approaches.begin(), end.approaches.end(),
            [&](const AutoApproach &a) { return distance(a.x, approach.x) <= auto_global_same_equilibrium * extent; });
        if (!known) end.approaches.push_back(std::move(approach));
    }
    end.kind = auto_global_kind(end.approaches);
    return end;
}

namespace {

std::string g(double v) { return xpp::format("{:.6g}", v); }

/* an approached equilibrium in words: its kind, its eigenvalues' counts and where it is */
std::string describe_approach(const Model &m, const AutoApproach &a)
{
    std::string where;
    for (std::size_t i = 0; i < std::min(a.x.size(), auto_global_shown_variables); ++i)
        where += xpp::format("{}\"{}\" = {}", i ? ", " : "", m.uvar_names[i], g(a.x[i]));
    if (a.x.size() > auto_global_shown_variables) where += xpp::format(", ... ({} variables)", a.x.size());
    switch (a.kind) {
    case AutoApproachKind::saddle:
        return xpp::format("a saddle ({} unstable and {} stable eigenvalues) at {}", a.unstable, a.stable, where);
    case AutoApproachKind::zero_eigenvalue: {
        double nearest = INFINITY; /* the eigenvalue at zero */
        for (std::size_t i = 0; i < a.re.size(); ++i)
            if (a.im[i] == 0 && std::abs(a.re[i]) < std::abs(nearest)) nearest = a.re[i];
        return xpp::format("{} with an eigenvalue {} of spectral radius {} (a zero eigenvalue, on a fold of the equilibria) at {}",
                           a.exists ? "an equilibrium" : "the ghost of an equilibrium (Newton's method finds none)", g(nearest),
                           g(a.spectral_radius), where);
    }
    case AutoApproachKind::other:
        break;
    }
    return xpp::format("{} ({} unstable and {} stable eigenvalues, spectral radius {}) at {}",
                       a.exists ? "an equilibrium that is no saddle" : "no equilibrium (Newton's method finds none, and no eigenvalue is zero)",
                       a.unstable, a.stable, g(a.spectral_radius), where);
}

} // namespace

std::string auto_global_text(const Session &s, const AutoGlobalEnd &end)
{
    std::string text = xpp::format("period growing without bound: T = {}, {} times the shortest period of the branch ({}), has grown over the last {} points",
        g(end.period), g(end.period / end.shortest), g(end.shortest), auto_global_growing_points);
    if (!end.failed.empty()) return text + xpp::format("; the equilibria the orbit lingers near could not be searched: {}", end.failed);
    text += "; the orbit lingers near ";
    for (std::size_t i = 0; i < end.approaches.size(); ++i)
        text += (i ? "; then " : "") + describe_approach(s.model(), end.approaches[i]);
    if (end.kind != AutoPointKind::End)
        text += xpp::format(": point type {} ({})", auto_point_kind_name(end.kind), auto_point_kind_mark(end.kind));
    else text += ": no point type fits (a snic needs one equilibrium with a zero eigenvalue, a homoclinic one saddle, a heteroclinic two or more)";
    return text;
}

} // namespace xpp
