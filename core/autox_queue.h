#pragma once

/* The queue of found points (W605, #672): with follow_all_branches on, what a job finds starts new jobs
   (autox_jobs.cpp runs them; this is the rule that says which, in which order, and when a start is already taken).

   What a job's points and end start (#659 question 1 and 5, #662 corner cases 3, 4, 6, 7):
     - a branch point (BP) on equilibria: the new branch, as two runs, +DS and -DS;
     - a branch point of cycles (BPC): the same on cycles;
     - a Hopf point (HB): its cycles, one run (the family has one direction to leave it by);
     - a branch of cycles that ends at a Hopf point or a SNIC: the equilibria through that point, both directions.
   A fold (LP), period doubling, torus, a global end, a codimension-two point start nothing.

   The join order is a job's key: its generation, then the path of the points it was found at, so the jobs are in the
   order a queue served one after another would run them (the roots, their children in the order the parents found
   them, their children's children, ...), whatever order they finished in.

   A start is taken when a job earlier in that order already made it: its own start (the same problem, direction
   and point by the same-point rule of autox_ends.h) or what it ended with (a cycle branch that ended at a Hopf point
   has made that Hopf point's cycles, an equilibrium branch through a Hopf point or a fold has made the equilibria
   through it, and a loop that closed at the point, in the other direction, has made it too). Only the earlier jobs the join
   keeps count, and one counts only once every job before it has ended and made its jobs (autox_jobs.cpp's repeats), so the
   jobs the join keeps never depend on which ran first, nor on how many threads ran them; what runs in vain is dropped at
   the join by the same rule.

   autox_queue.cpp. */

#include <optional>
#include <span>
#include <vector>
#include <math.h>

#include "autox_ends.h"
#include "autox_codes.h"

struct AutoxBufferedPoint; /* autox_state.h */

namespace xpp {

struct AutoxStop; /* autox_stop.h */

/* A job's place in the join: the generation (the path's length), then the path, each element the point it was found at
   (that point's number on the finding job's branch) and its direction; a root's is its index */
struct AutoxJobKey {
    std::vector<long> path;
};
/* whether a comes before b: an earlier generation, or the same generation and the lesser path */
bool autox_job_before(const AutoxJobKey &a, const AutoxJobKey &b);
/* the key of the job started at the finding job's point number `point`; negative: its -DS run (a Hopf's one run is positive) */
AutoxJobKey autox_child_key(const AutoxJobKey &parent, long point, bool negative);

/* What a job continues, and the point it starts at. Also what a finished job has made (autox_end_claims): the same
   start, which taking it again would repeat */
struct AutoxJobStart {
    AutoxProblem ips = AutoxProblem::SteadyState;
    int isw = 1; /* -1: a branch switch */
    std::optional<int> sign; /* the sign of step_size; none: the one run of a point (a Hopf's cycles) */
    std::optional<AutoxPointId> id; /* the point; none: a root, the start of the run itself */
    /* where it goes on from: the finding job's label whose solution it restarts from, or (label 0) the equilibrium at the
       parameter value of the point */
    int label = 0;
    std::vector<double> equilibrium;
    double parameter = NAN;
    long point = 0; /* the number of the point that was found on the finding job's branch, which the key is made of */
};

/* the starts the points and the end of a finished job give, in the order they were found; ips is the job's problem and
   stop its stop (a SNIC end says there which equilibrium the cycles lingered at) */
std::vector<AutoxJobStart> autox_found_starts(AutoxProblem ips, std::span<const AutoxBufferedPoint> points, const AutoxStop &stop);

/* what a finished job has made besides its own start: the Hopf points and folds it found on equilibria (the equilibria
   through them are traced), and the Hopf point its cycles ended at (the cycles of that Hopf point are) */
std::vector<AutoxJobStart> autox_end_claims(AutoxProblem ips, std::span<const AutoxBufferedPoint> points);

/* whether `made` is the start `wanted` is: the same problem and branch switch, a direction that is the same or either's
   none, and the same point by the rule of autox_ends.h at special_point_tolerance epss */
bool autox_start_made(const AutoxJobStart &made, const AutoxJobStart &wanted, double epss);

/* whether `wanted` runs the branch of `made` the other way round: the same problem, branch switch and point (as
   autox_start_made) in the opposite direction, both directions given. When made's branch closed on itself, wanted's
   is the same loop, so it is made too (a point that two branches found, each from its own, is one point) */
bool autox_start_reversed(const AutoxJobStart &made, const AutoxJobStart &wanted, double epss);

} // namespace xpp
