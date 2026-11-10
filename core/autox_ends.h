#pragma once

/* Two more ends of a branch, and the rule that says two special points are one (W604, #671).

   Reaches a Hopf: a branch of cycles whose amplitude falls back to about zero at an equilibrium with a purely
   imaginary pair of eigenvalues ends there (point type hopf_end) and says why, where autoX goes on past the Hopf
   point, along the same family back, to the step limit. Like W518's global ends nothing here changes what autoX
   computes: stplbv hands each cycle's extremes to the watch below.

   Branch closed: a branch that comes back to its own start (an isola of equilibria, a closed family of cycles)
   ends there (point type branch_closed). The start is kept (AutoxRun::branch_start); a hidden user zero at its
   parameter value, a test like a user's own that is never stored, labelled or written, makes autoX locate every
   crossing of that value, and autox_lib1.cpp keeps a crossing only when the states there are the start's by the
   rule below; any other crossing leaves nothing, not even a step.

   The same-point rule (maintainer, 2026-11-04, #662 corner case 5) is the one place that says whether two special
   points are one: the same kind, the same parameter values and every state within a relative tolerance. W605's
   queue of found points calls it too.

   autox_ends.cpp. */

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "autox_codes.h"
#include "autox_nox.h"

struct iap_type; /* autox_c.h */

namespace xpp {

struct Session; /* session.h */

/* An orbit is no longer a cycle when every variable's range is at most this fraction of (1 + |its mean|): a Hopf
   point's cycles lie about the square of their relative amplitude from it in the parameter (1e-3: 1e-6, below the
   default special_point_tolerance of 1e-4 the same-point rule measures in), and a continuation that passes the
   Hopf point lands on its trivial solutions, constant orbits, whose range is autoX's own state_tolerance and
   less (hodgkin_huxley: a relative range of 2e-6 at state_tolerance 1e-4) */
inline constexpr double autox_hopf_end_amplitude = 1e-3;
/* The amplitude has fallen over this many points in a row: a branch that starts at a Hopf point starts at about
   zero and grows, and the cycles of a Hopf point that is passed first shrink towards it over many points */
inline constexpr int autox_hopf_end_shrinking_points = 3;
/* The eigenvalue pair is purely imaginary when its real part is at most this fraction of its imaginary part:
   at the amplitude above the real part is the parameter's distance from the Hopf point times a rate of the
   model's, a small fraction of the frequency; a pair that is far from the axis is no Hopf point. 1e-2 is a
   round bound with room (not measured to a finer value: hodgkin_huxley is the one model that reaches it) */
inline constexpr double autox_hopf_end_imaginary = 1e-2;
/* c of the same-point rule: two special points are one when every parameter and state differs by at most
   c * EPSS * (1 + |a|) (docs/auto-references.md W604 gives the measurements it is chosen from) */
inline constexpr double autox_same_point_factor = 8;

/* A cycle's size: the largest of its variables' ranges, each relative to (1 + |its mean|); pure */
double autox_orbit_amplitude(std::span<const double> high, std::span<const double> low, std::span<const double> mean);

/* A branch's amplitudes so far (AutoxStop::amplitude_watch): the last, how many points in a row it has fallen, and
   the last two points still above autox_hopf_end_amplitude (the cycles proper) with their parameters, which say
   where the Hopf point is */
struct AutoxAmplitudeWatch {
    struct Cycle {
        double parameter = NAN, amplitude = NAN;
    };
    double last = NAN;
    int shrinking = 0;
    std::optional<double> passed; /* the Hopf parameter when the last step turned round at it: the line of the cycles before it gives it */
    std::array<Cycle, 2> cycles; /* the older first */
};

/* The amplitude and parameter of a branch's next point; first_point starts the branch's watch. True when the
   amplitude has fallen over autox_hopf_end_shrinking_points points in a row and is at most autox_hopf_end_amplitude,
   or when the parameter turns round at this point within one step of the Hopf point the last two shrinking cycles' line
   gives (the watch's `passed`: the step went over it, whatever the amplitude); the equilibrium's eigenvalues there decide */
bool autox_amplitude_watch_add(AutoxAmplitudeWatch &watch, double amplitude, double parameter, bool first_point);

/* the parameter at which the cycles' amplitude reaches zero: their squared amplitude is linear in the parameter near
   a Hopf point, so the line through the watch's last two cycles; none when it has not two that shrink */
std::optional<double> autox_hopf_parameter(const AutoxAmplitudeWatch &watch);

/* the Hopf point a branch of cycles reaches, as one coherent point: its parameter (the line above), the equilibrium
   there (Newton's method), the eigenvalue pair's real part and frequency, and the period 2 pi over the frequency */
struct AutoxHopfEnd {
    double parameter = NAN, real = NAN, frequency = NAN, period = NAN, amplitude = NAN; /* amplitude: the last cycle's */
    bool passed = false; /* found by a step that turned at the Hopf point, not by an amplitude below autox_hopf_end_amplitude */
    std::vector<double> equilibrium;
};

/* The end of the branch at the cycle just computed, whose amplitude the watch said fell to about zero: none when
   there is no equilibrium at the Hopf parameter near its mean or no purely imaginary pair there (the branch goes on).
   mean is the orbit's variables' means (iap.ndm of them); par and icp are autoX's. A model that fails to evaluate is
   a note of the run's, once, and none */
std::optional<AutoxHopfEnd> autox_hopf_end(const iap_type &iap, const double *par, const std::ptrdiff_t *icp,
                                           std::span<const double> mean, const AutoxAmplitudeWatch &watch);

/* The cycle just computed becomes the Hopf point end: par at icp[0] and the period, ups (autoX's solution, ndxloc
   rows) the equilibrium at every mesh point */
void autox_hopf_end_apply(const iap_type &iap, const AutoxHopfEnd &end, double *par, const std::ptrdiff_t *icp,
                          std::ptrdiff_t ndxloc, double *ups);

/* the stop's words: "the cycles' amplitude fell to ... at an equilibrium with the imaginary pair ..." */
std::string autox_hopf_end_text(const Session &s, const AutoxHopfEnd &end);

/* A special point as the same-point rule sees it: its kind, its parameters (the continuation's), its states
   and, for a Hopf point, its frequency */
struct AutoxPointId {
    AutoxPointKind kind = AutoxPointKind::Regular;
    std::vector<double> parameters, states;
    std::optional<double> frequency;
};

/* A stored point (a diagram point) as the rule sees it: its kind, its continued parameters, its states and, for a Hopf point or
   the end at one, its frequency (2 pi over the period). The states of a cycle are each variable's maximum then minimum over
   the orbit (phase-free, as the closing test's); those of a steady state, and of the end at a Hopf point, its variables */
AutoxPointId autox_point_id(const DIAGRAM &point);

/* whether a point of kind a is one of kind b: the same kind, or an end and the point it ends at: a Hopf end and a
   Hopf point (HB), a SNIC end and a fold (LP) */
bool autox_same_kind(AutoxPointKind a, AutoxPointKind b);

/* How far apart two points are in units of the rule's tolerance EPSS * (1 + |a|): the largest ratio over their
   parameters, states and (where both have one) frequencies; infinite when they differ in kind or in size. A state's unit
   is at most state_cap (the closing test's: a cycle's own size, autox_branch_stored) */
double autox_point_gap(const AutoxPointId &a, const AutoxPointId &b, double epss, double state_cap = INFINITY);

/* the rule: the same kind and a gap of at most autox_same_point_factor */
bool autox_same_point(const AutoxPointId &a, const AutoxPointId &b, double epss);

/* A branch's first point, and whether the branch has been away from it (AutoxRun::branch_start): a cycle branch
   from a Hopf point starts at a cycle of no size, and the first cycles beyond it are still within the rule's
   tolerance of it, and cross its parameter value if the parameter first turns the wrong way (ode_motoneuron6c's):
   a crossing is the branch coming back only after a point of the branch was further from the start than that.
   The closing test measures a cycle's states in units of at most its own size, orbit_size (W687, #762): the largest
   range of a state over the start's orbit. A cycle near a Hopf point is smaller than the unit EPSS * (1 + |state|)
   (ode_motoneuron6c's at -49: 0.052 of a unit of 0.005 times the rule's 8), so two different cycles of it are
   within the rule, and a located point of the branch that was no return counted as one; the cycles it is told
   apart by are the same cycle only when they agree to a small fraction of the cycle's own size. A steady state, and a cycle of no size
   (a Hopf point's), keep the unit as before (infinite) */
struct AutoxBranchStart {
    AutoxPointId point;
    bool left = false;
    double orbit_size = INFINITY;
};

/* The branch's point just stored (ntot its number on the branch): its continuation parameters (par at icp, iap.nfpr
   of them) and its states (a cycle's: orbit, each variable's maximum then minimum). The first is the start; each later
   one says whether the branch has left it, by the closing test's gap */
void autox_branch_stored(iap_type &iap, std::ptrdiff_t ntot, double epss, const double *par, const std::ptrdiff_t *icp,
                         std::span<const double> states, bool orbit);

/* Whether the point just computed is the branch's start by the rule (the hidden test's accept): the branch has left
   the start and is back within the rule's factor of the start's closing units of it (AutoxBranchStart). When it is, the gap is kept in the run's stop
   (AutoxStop::closure_gap) and stplae/stplbv end the branch */
bool autox_branch_closes(iap_type &iap, double epss, const double *par, const std::ptrdiff_t *icp, std::span<const double> states);

/* the stop's words for a branch that closed on itself: gap is autox_point_gap between the crossing and the start */
std::string autox_closure_text(double gap, double epss);

} // namespace xpp
