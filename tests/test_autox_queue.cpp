/* autox_queue: the pure parts of the queue of found points (W605): the join order, what a job's points and end start,
   what a finished job has made, and when a start is already made. The jobs themselves, and the diagram they join into,
   are tools/autoxcompare.py's and tools/servercheck.py's. */
#include "xpptest.h"
#include "autox_queue.h"
#include "autox_state.h"

#include <vector>

namespace {

using xpp::AutoxPointKind;
using xpp::AutoxProblem;

/* a stored point of kind `kind` on a branch of the given sign, at parameter p, with its numbers */
AutoxBufferedPoint stored(AutoxPointKind kind, int branch, int number, int label, double p, std::vector<double> state)
{
    AutoxBufferedPoint held;
    held.point.itp = xpp::AutoxPointCode{kind};
    held.point.ibr = branch;
    held.point.ntot = number;
    held.point.lab = label;
    held.point.par = {p};
    held.point.u0 = state;
    held.point.uhi = state;
    held.point.ulo = state;
    held.point.per = 2.0;
    return held;
}

} // namespace

TEST_CASE("test_autox_queue")
{
    /* the join order: the generation first, then the path; a root's is its index */
    const xpp::AutoxJobKey plus{{0}}, minus{{1}};
    const xpp::AutoxJobKey first_child = xpp::autox_child_key(plus, 5, false), second_child = xpp::autox_child_key(plus, 5, true);
    const xpp::AutoxJobKey grandchild = xpp::autox_child_key(first_child, 1, false), minus_child = xpp::autox_child_key(minus, 1, false);
    CHECK(xpp::autox_job_before(plus, minus) && !xpp::autox_job_before(minus, plus));
    CHECK(xpp::autox_job_before(minus, first_child)); /* every root before every child: the generation first */
    CHECK(xpp::autox_job_before(first_child, second_child)); /* a point's +DS before its -DS */
    CHECK(xpp::autox_job_before(second_child, minus_child)); /* the jobs of one parent before the next parent's */
    CHECK(xpp::autox_job_before(minus_child, grandchild)); /* a grandchild after every child, whatever its parent */
    CHECK(!xpp::autox_job_before(plus, plus));

    /* what an equilibrium job's points start: a branch point's two runs, a Hopf point's one run of cycles, nothing from a fold */
    const xpp::AutoxStop nothing;
    std::vector<AutoxBufferedPoint> branch = {stored(AutoxPointKind::End, 1, 1, 1, 0.0, {0.0, 0.0}),
                                              stored(AutoxPointKind::BranchPoint, 1, 4, 2, 1.0, {0.0, 0.0}),
                                              stored(AutoxPointKind::Fold, 1, 6, 3, 2.0, {1.0, 1.0}),
                                              stored(AutoxPointKind::Hopf, 1, 9, 4, 3.0, {2.0, 2.0}),
                                              stored(AutoxPointKind::End, 1, 12, 5, 5.0, {3.0, 3.0})};
    auto starts = xpp::autox_found_starts(AutoxProblem::SteadyState, branch, nothing);
    CHECK(starts.size() == 3);
    CHECK(starts[0].ips == AutoxProblem::SteadyState && starts[0].isw == -1 && starts[0].sign == 1 && starts[0].label == 2 && starts[0].point == 4);
    CHECK(starts[1].isw == -1 && starts[1].sign == -1 && starts[1].label == 2);
    CHECK(starts[2].ips == AutoxProblem::Periodic && starts[2].isw == 1 && !starts[2].sign && starts[2].label == 4 && starts[2].point == 9);
    CHECK(starts[2].id && starts[2].id->kind == AutoxPointKind::Hopf && starts[2].id->parameters == std::vector<double>{3.0} && starts[2].id->frequency);
    /* a map's branch points are followed, its other points are not (a Hopf point of a map is a Neimark-Sacker one) */
    starts = xpp::autox_found_starts(AutoxProblem::DiscreteMap, branch, nothing);
    CHECK(starts.size() == 2 && starts[0].ips == AutoxProblem::DiscreteMap && starts[0].isw == -1);
    /* a cycle's branch points start the cycles' branch, a Hopf point of an equilibrium problem is no start of a cycle job */
    std::vector<AutoxBufferedPoint> cycles = {stored(AutoxPointKind::CycleBranch, -2, 7, 2, 4.0, {1.0}),
                                              stored(AutoxPointKind::PeriodDoubling, -2, 9, 3, 4.5, {1.0}),
                                              stored(AutoxPointKind::CycleFold, -2, 11, 4, 5.0, {1.0}),
                                              stored(AutoxPointKind::Hopf, -2, 12, 5, 5.0, {1.0})};
    starts = xpp::autox_found_starts(AutoxProblem::Periodic, cycles, nothing);
    CHECK(starts.size() == 2 && starts[0].ips == AutoxProblem::Periodic && starts[0].isw == -1 && starts[0].sign == 1 && starts[1].sign == -1);
    /* the points of a two-parameter curve start nothing */
    branch[1].point.itp = xpp::AutoxPointCode{AutoxPointKind::BranchPoint, AutoxPointKind::Fold};
    CHECK(xpp::autox_found_starts(AutoxProblem::SteadyState, branch, nothing).size() == 1);

    /* a branch of cycles that ends at a Hopf point starts the equilibria through it, both directions, from its equilibrium */
    cycles.push_back(stored(AutoxPointKind::HopfEnd, -2, 20, 6, 9.5, {4.0, 5.0}));
    starts = xpp::autox_found_starts(AutoxProblem::Periodic, cycles, nothing);
    CHECK(starts.size() == 4 && starts[2].ips == AutoxProblem::SteadyState && starts[2].isw == 1 && starts[2].sign == 1 && starts[3].sign == -1);
    CHECK(starts[2].label == 0 && starts[2].equilibrium == std::vector<double>({4.0, 5.0}) && starts[2].parameter == 9.5);
    CHECK(starts[2].id->kind == AutoxPointKind::HopfEnd && starts[2].id->states == std::vector<double>({4.0, 5.0}));
    /* ... and one that ends at a SNIC the equilibria through the equilibrium its cycles lingered at; none when they found none */
    std::vector<AutoxBufferedPoint> snic = {stored(AutoxPointKind::Snic, -2, 30, 2, 0.5, {0.1, 0.2})};
    CHECK(xpp::autox_found_starts(AutoxProblem::Periodic, snic, nothing).empty());
    xpp::AutoxStop lingered;
    xpp::AutoxApproach saddle, ghost;
    saddle.exists = true, saddle.kind = xpp::AutoxApproachKind::saddle, saddle.x = {9.0, 9.0};
    ghost.exists = true, ghost.kind = xpp::AutoxApproachKind::zero_eigenvalue, ghost.x = {0.3, 0.4};
    lingered.global.approaches = {saddle, ghost};
    starts = xpp::autox_found_starts(AutoxProblem::Periodic, snic, lingered);
    CHECK(starts.size() == 2 && starts[0].equilibrium == std::vector<double>({0.3, 0.4}) && starts[0].parameter == 0.5);
    CHECK(starts[0].id->kind == AutoxPointKind::Snic && starts[0].id->states == std::vector<double>({0.3, 0.4}));
    /* a homoclinic end starts nothing */
    snic[0].point.itp = xpp::AutoxPointCode{AutoxPointKind::Homoclinic};
    CHECK(xpp::autox_found_starts(AutoxProblem::Periodic, snic, lingered).empty());

    /* what a finished job has made besides its start: the equilibria through the Hopf points and folds it found, the cycles
       of the Hopf point its cycles ended at */
    branch[1].point.itp = xpp::AutoxPointCode{AutoxPointKind::BranchPoint};
    auto claims = xpp::autox_end_claims(AutoxProblem::SteadyState, branch);
    CHECK(claims.size() == 2 && claims[0].ips == AutoxProblem::SteadyState && claims[0].id->kind == AutoxPointKind::Fold && claims[1].id->kind == AutoxPointKind::Hopf);
    CHECK(!claims[0].sign && claims[0].isw == 1);
    claims = xpp::autox_end_claims(AutoxProblem::Periodic, cycles);
    CHECK(claims.size() == 1 && claims[0].ips == AutoxProblem::Periodic && claims[0].id->kind == AutoxPointKind::HopfEnd);

    /* a start is made by one with the same problem, branch switch and point, in the same direction or either's none */
    const double epss = 1e-4;
    const auto from_branch = xpp::autox_found_starts(AutoxProblem::SteadyState, branch, nothing);
    const xpp::AutoxJobStart &bp_plus = from_branch[0], &bp_minus = from_branch[1], &hopf_cycles = from_branch[2];
    CHECK(xpp::autox_start_made(bp_plus, bp_plus, epss) && !xpp::autox_start_made(bp_plus, bp_minus, epss));
    /* the other way round: the same point in the opposite direction, both directions given */
    CHECK(xpp::autox_start_reversed(bp_plus, bp_minus, epss) && xpp::autox_start_reversed(bp_minus, bp_plus, epss));
    CHECK(!xpp::autox_start_reversed(bp_plus, bp_plus, epss));
    xpp::AutoxJobStart either = bp_plus;
    either.sign.reset();
    CHECK(xpp::autox_start_made(either, bp_minus, epss) && xpp::autox_start_made(bp_minus, either, epss));
    CHECK(!xpp::autox_start_reversed(either, bp_minus, epss) && !xpp::autox_start_reversed(bp_minus, either, epss)); /* no direction, no reverse */
    xpp::AutoxJobStart other_switch = bp_plus;
    other_switch.isw = 1;
    CHECK(!xpp::autox_start_made(other_switch, bp_plus, epss)); /* the run through a point is not the branch switched at it */
    xpp::AutoxJobStart elsewhere = bp_plus;
    elsewhere.id->parameters = {1.0 + 1e-2};
    CHECK(!xpp::autox_start_made(elsewhere, bp_plus, epss) && !xpp::autox_start_reversed(elsewhere, bp_minus, epss));
    xpp::AutoxJobStart root; /* a root has no point: nothing makes it, it makes nothing */
    CHECK(!xpp::autox_start_made(root, bp_plus, epss) && !xpp::autox_start_made(bp_plus, root, epss));
    /* the Hopf point found on an equilibrium branch has made the equilibria through its end at it, which a cycle branch that
       ended there asks for; the cycles that ended there have made the Hopf point's cycles */
    xpp::AutoxJobStart through;
    through.ips = AutoxProblem::SteadyState;
    through.sign = 1;
    through.id = *hopf_cycles.id;
    through.id->kind = AutoxPointKind::HopfEnd;
    const auto on_branch = xpp::autox_end_claims(AutoxProblem::SteadyState, branch);
    CHECK(xpp::autox_start_made(on_branch[1], through, epss));
    through.id->parameters = {3.0 + 1e-2};
    CHECK(!xpp::autox_start_made(on_branch[1], through, epss));
    xpp::AutoxJobStart ended_there;
    ended_there.ips = AutoxProblem::Periodic;
    ended_there.id = *hopf_cycles.id;
    ended_there.id->kind = AutoxPointKind::HopfEnd;
    CHECK(xpp::autox_start_made(ended_there, hopf_cycles, epss));
    ended_there.id->frequency = *ended_there.id->frequency * 1.5; /* a Hopf point also by its frequency */
    CHECK(!xpp::autox_start_made(ended_there, hopf_cycles, epss));

    TEST_REPORT("autox_queue");
}
