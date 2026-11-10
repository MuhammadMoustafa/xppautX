#include "autox_unknowns.h"
/* The queue of found points (autox_queue.h, W605). */
#include <algorithm>
#include <cstdlib>
#include <utility>
#include <vector>

#include "autox_global.h"
#include "autox_queue.h"
#include "autox_state.h"
#include "autox_stop.h"

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

} // namespace

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
