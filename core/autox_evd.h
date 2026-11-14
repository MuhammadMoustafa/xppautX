#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include "autox_c.h"
#include "autox_codes.h"
#include "autox_nox.h"
#include "xAutox.h"

struct AutoxState; /* autox_state.h */
namespace xpp { struct Session; /* session.h */ }

namespace xpp {

/* autox_evd.cpp */

/* One failure message for a refused autoX solution, rendered by do_autox. */
[[noreturn]] void autox_nonfinite(const iap_type &iap, std::ptrdiff_t point);

/* why a diagram of that many points (a Session's, with the points of the jobs not joined yet) takes no more, or nothing */
std::optional<std::string> autox_diagram_full(std::size_t points);
/* what it says (the same whichever run reaches the limit: a run's own buffer or the join of jobs) */
std::string autox_diagram_limit_message();
/* the most points a session's diagram holds (XPP_DIAGRAM_POINTS_LIMIT lowers it for servercheck) */
std::size_t autox_diagram_points_limit();
/* the most jobs a run of branch jobs makes: what a recorded run's jobs list may hold (autox_evd.cpp says why) */
std::size_t autox_recorded_jobs_limit();

/* Start checks run before storing a point or taking a continuation step. */
void autox_check_start_parameters(const iap_type &iap, const rap_type &rap, const double *par, const std::ptrdiff_t *icp);
void autox_check_start_norm(const iap_type &iap, const rap_type &rap, double norm);
void autox_check_start_equilibrium(const iap_type &iap, const rap_type &rap, double *u, const double *par, const std::ptrdiff_t *icp);

/* a point's type from autoX's signs: a negative branch is periodic, a
   negative point stable (a zero sign: a stable steady state) */
DiagramPointType get_bif_type(int ibr, int ntot);
/* the point p of a run into the Session's diagram, drawn: a run's addbif, and the join of jobs (autox_jobs.cpp), do it;
   new_segment: the point starts a branch (the drawing's new segment). p.from is the label its run started from, or 0 */
void autox_store_point(Session &s, const DIAGRAM &p, bool new_segment);
/* the point p of a running job drawn as it comes, as job `live`'s (XppDiagPoint.live): not in the diagram, which the join of jobs
   fills; the numbers it carries are the job's own */
void autox_show_live_point(Session &s, const DIAGRAM &p, int live, bool new_segment);
/* a start at or outside the parameter window's edge with the step leading out of it, which no run can make */
bool autox_start_points_away(double value, double rl0, double rl1, double ds);
void addbif(iap_type *iap, rap_type *rap, std::ptrdiff_t ntots, std::ptrdiff_t ibrs, double *par,std::ptrdiff_t *icp,int labw, double *a, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> u0, std::span<const double> ubar);

/* the autoX run's own parameters (run) from its settings (st.bifur,
   the active view's parameters, the Mark values), for ndim variables;
   exact: user_right_hand_side gives the model's exact Jacobian (JAC=1, W260) */
void init_autox(const AutoxState &st, XAUTOX &run, int ndim, bool exact);

} // namespace xpp
