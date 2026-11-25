/* The queue of found points (autox_queue.h, W605). */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <utility>
#include <vector>

#include "autox_global.h"
#include "autox_queue.h"
#include "autox_state.h"
#include "autox_stop.h"
#include "autox_nox.h"

namespace xpp {

bool autox_job_before(const AutoxJobKey &a, const AutoxJobKey &b)
{
    if (a.path.size() != b.path.size()) return a.path.size() < b.path.size();
    return a.path < b.path;
}

AutoxJobKey autox_child_key(const AutoxJobKey &parent, long point, bool negative)
{
    AutoxJobKey key = parent;
    key.path.push_back(2 * point + (negative ? 1 : 0));
    return key;
}

namespace {

/* the start a point gives, for the problem and branch switch ips and isw, in the direction sign */
AutoxJobStart start_at(const DIAGRAM &p, AutoxProblem ips, int isw, std::optional<int> sign)
{
    AutoxJobStart start;
    start.ips = ips;
    start.isw = isw;
    start.sign = sign;
    start.id = autox_point_id(p);
    start.label = p.lab;
    start.point = std::abs(p.ntot);
    return start;
}

/* the equilibria through a cycle branch's end, both directions, from the equilibrium x at the end's parameter */
void equilibria_through(std::vector<AutoxJobStart> &found, const DIAGRAM &p, std::span<const double> x)
{
    for (const int sign : {1, -1}) {
        AutoxJobStart start = start_at(p, AutoxProblem::SteadyState, 1, sign);
        start.id->states.assign(x.begin(), x.end());
        start.label = 0;
        start.equilibrium.assign(x.begin(), x.end());
        start.parameter = p.par.empty() ? NAN : p.par[0];
        found.push_back(std::move(start));
    }
}

/* the same problem, branch switch and point, whatever the direction */
bool same_start(const AutoxJobStart &a, const AutoxJobStart &b, double epss)
{
    return a.id && b.id && a.ips == b.ips && a.isw == b.isw && autox_same_point(*a.id, *b.id, epss);
}

/* the largest ratio of |start - on the segment| to epss * (1 + |start|) over the parameters then the states, at the point of the
   segment a to b nearest to the start in the (1 + |start|)-scaled coordinates */
double segment_gap(const AutoxPointId &start, const DIAGRAM &a, const DIAGRAM &b, double epss)
{
    const std::size_t n = start.parameters.size(), count = n + start.states.size();
    const auto coordinate = [&](std::size_t i) { /* the start's, a's and b's value of coordinate i: the parameters, then the states */
        return i < n ? std::array<double, 3>{start.parameters[i], a.par[i], b.par[i]}
                     : std::array<double, 3>{start.states[i - n], a.u0[i - n], b.u0[i - n]};
    };
    double along = 0, length = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto [at, from, to] = coordinate(i);
        const double unit = 1 + std::abs(at);
        along += (at - from) * (to - from) / (unit * unit);
        length += (to - from) * (to - from) / (unit * unit);
    }
    const double t = length > 0 ? std::clamp(along / length, 0.0, 1.0) : 0;
    double gap = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto [at, from, to] = coordinate(i);
        gap = std::max(gap, std::abs(at - (from + t * (to - from))) / (epss * (1 + std::abs(at))));
    }
    return gap;
}

/* the walk of autox_on_branch over `count` points, the i-th being at(i) */
template <typename At>
bool on_branch(std::size_t count, At at, const AutoxJobStart &start, double epss)
{
    if (!start.id || start.ips != AutoxProblem::SteadyState || start.equilibrium.empty()) return false;
    const AutoxPointId &id = *start.id;
    const auto fits = [&](const DIAGRAM &p) { return p.ibr > 0 && p.par.size() == id.parameters.size() && p.u0.size() == id.states.size(); };
    for (std::size_t i = 0; i < count; ++i) {
        const DIAGRAM &a = at(i), &b = at(std::min(i + 1, count - 1)); /* the last point is a segment of its own */
        if (!fits(a) || !fits(b) || a.ibr != b.ibr) continue;
        if (segment_gap(id, a, b, epss) <= autox_same_point_factor) return true;
    }
    return false;
}

} // namespace

bool autox_on_branch(std::span<const AutoxBufferedPoint> points, const AutoxJobStart &start, double epss)
{
    return on_branch(points.size(), [&](std::size_t i) -> const DIAGRAM & { return points[i].point; }, start, epss);
}

bool autox_on_branch(const std::deque<DIAGRAM> &points, const AutoxJobStart &start, double epss)
{
    return on_branch(points.size(), [&](std::size_t i) -> const DIAGRAM & { return points[i]; }, start, epss);
}

std::vector<AutoxJobStart> autox_found_starts(AutoxProblem ips, std::span<const AutoxBufferedPoint> points, const AutoxStop &stop)
{
    std::vector<AutoxJobStart> found;
    for (const AutoxBufferedPoint &held : points) {
        const DIAGRAM &p = held.point;
        if (p.itp.start != AutoxPointKind::Regular || p.par.empty()) continue; /* a point of a two-parameter curve, or with no model parameter continued */
        const AutoxPointKind kind = p.itp.kind;
        if ((problem_is_steady_or_map(ips) && kind == AutoxPointKind::BranchPoint) ||
            (ips == AutoxProblem::Periodic && kind == AutoxPointKind::CycleBranch)) {
            for (const int sign : {1, -1}) found.push_back(start_at(p, ips, -1, sign));
        } else if (ips == AutoxProblem::SteadyState && kind == AutoxPointKind::Hopf) {
            found.push_back(start_at(p, AutoxProblem::Periodic, 1, std::nullopt));
        } else if (ips == AutoxProblem::Periodic && kind == AutoxPointKind::HopfEnd) {
            equilibria_through(found, p, p.u0);
        } else if (ips == AutoxProblem::Periodic && kind == AutoxPointKind::Snic) {
            const auto &approaches = stop.global.approaches;
            const auto at = std::find_if(approaches.begin(), approaches.end(), [](const AutoxApproach &a) {
                return a.exists && a.kind == AutoxApproachKind::zero_eigenvalue;
            });
            if (at != approaches.end()) equilibria_through(found, p, at->x);
        }
    }
    return found;
}

std::vector<AutoxJobStart> autox_end_claims(AutoxProblem ips, std::span<const AutoxBufferedPoint> points)
{
    std::vector<AutoxJobStart> made;
    for (const AutoxBufferedPoint &held : points) {
        const DIAGRAM &p = held.point;
        if (p.itp.start != AutoxPointKind::Regular) continue;
        const AutoxPointKind kind = p.itp.kind;
        const bool equilibrium_point = ips == AutoxProblem::SteadyState && (kind == AutoxPointKind::Hopf || kind == AutoxPointKind::Fold);
        const bool cycles_end = ips == AutoxProblem::Periodic && kind == AutoxPointKind::HopfEnd;
        if (!equilibrium_point && !cycles_end) continue;
        AutoxJobStart claim;
        claim.ips = equilibrium_point ? AutoxProblem::SteadyState : AutoxProblem::Periodic;
        claim.id = autox_point_id(p);
        made.push_back(std::move(claim));
    }
    return made;
}

bool autox_start_made(const AutoxJobStart &made, const AutoxJobStart &wanted, double epss)
{
    return same_start(made, wanted, epss) && (!made.sign || !wanted.sign || *made.sign == *wanted.sign);
}

bool autox_start_reversed(const AutoxJobStart &made, const AutoxJobStart &wanted, double epss)
{
    return made.sign && wanted.sign && *made.sign != *wanted.sign && same_start(made, wanted, epss);
}

} // namespace xpp
