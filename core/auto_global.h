#pragma once

/* The global end of a periodic branch (W518, #582): a cycle branch whose
   period grows without bound, AUTO's MX or step limit with no word on why,
   ends on a saddle-node on an invariant circle (SNIC), a homoclinic orbit to
   a saddle or a heteroclinic cycle between saddles. Nothing here changes
   what AUTO computes: stplbv hands each periodic point's period to the
   watch below, and when the period has grown for a run of points while the
   orbit lingers where the vector field all but vanishes, the equilibria the
   orbit lingers near are searched (equilibria.h's equilibrium_near), their
   eigenvalues judged, and the end named. The branch then ends there with
   the new point type and says why (auto_stop.h).

   auto_global.cpp. */

#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "auto_codes.h"

struct iap_type; /* auto_c.h */

namespace xpp {

struct Session; /* session.h */

/* The criterion, each number with its reason. A period that has grown over
   this many points in a row is no wobble of the continuation's step ... */
inline constexpr int auto_global_growing_points = 5;
/* ... and is this multiple of the branch's shortest period: the lingering
   below, not this factor, says it is a global bifurcation (a homoclinic
   period grows with the logarithm of the distance to the saddle, a SNIC's
   with its inverse square root: no one multiple fits both) */
inline constexpr double auto_global_growth_factor = 2;
/* The orbit lingers where some mesh point's speed |f(x)| is at most this
   fraction of the orbit's largest. A cycle of a model with slow variables
   (mu26's calcium and gating, 28 states) passes 1e-3 to 1e-5 of its fastest
   speed for most of its period with an equilibrium nowhere near; a SNIC's
   bottleneck passes a millionth of it when the period is some thousand
   times the shortest, a homoclinic orbit when its period is a few times
   (AUTO follows it to periods of 1e5 with the parameter at its limit). Also
   what makes a stretch of the orbit slow, to be searched for an equilibrium */
inline constexpr double auto_global_slow_ratio = 1e-6;
/* An eigenvalue is zero, and the equilibrium on a fold, when it is this
   fraction of the spectral radius from zero: the ghost of a fold has an
   eigenvalue of the order of the square root of its distance from the fold
   (1e-4 of the radius in mu26 at its SNIC), a slow mode of the model is
   several times more (3e-3 there) */
inline constexpr double auto_global_zero_eigenvalue = 1e-3;
/* Two equilibria (or slow stretches with none) are one when closer than this
   fraction of the orbit's extent: Newton's tolerance is far below, and the
   saddles of a heteroclinic cycle are an orbit's length apart */
inline constexpr double auto_global_same_equilibrium = 1e-2;
/* A saddle the orbit passes near at a local minimum of its speed that is no slow point counts when Newton's method from that
   minimum lands this fraction of the orbit's extent from it (a coarse mesh samples a saddle's neighbourhood only at its
   edges: pp2's NTST 15 lands 5e-4 from the saddles it passes, mu26's SNIC orbit lands 0.28 and more from the equilibria it
   jumps to, which are no approach at all) */
inline constexpr double auto_global_near_orbit = 1e-2;
/* The variables a message names for an equilibrium; the rest is "..." */
inline constexpr std::size_t auto_global_shown_variables = 6;

/* A branch's periods so far (AutoStop::period_watch): the shortest, the last
   and how many points in a row the period has grown */
struct AutoPeriodWatch {
    double shortest = INFINITY, last = NAN;
    int growing = 0;
};

/* The period of a branch's next point; first_point starts the branch's watch.
   True when the period has grown over auto_global_growing_points points in a
   row and is auto_global_growth_factor times the shortest */
bool auto_period_watch_add(AutoPeriodWatch &watch, double period, bool first_point);

/* what an equilibrium (or a slow stretch's ghost of one) the orbit lingers
   near is, by its eigenvalues */
enum class AutoApproachKind {
    saddle,          /* hyperbolic, with unstable and stable eigenvalues */
    zero_eigenvalue, /* an eigenvalue at zero: on a fold of the equilibria */
    other            /* a node or focus, or no real equilibrium and no zero eigenvalue */
};

struct AutoApproach {
    /* the equilibrium, or the orbit's slowest point of the stretch when Newton's method found none */
    std::vector<double> x;
    bool exists = false; /* Newton's method found the equilibrium */
    std::vector<double> re, im; /* the Jacobian's eigenvalues there */
    AutoApproachKind kind = AutoApproachKind::other;
    std::size_t unstable = 0, stable = 0; /* eigenvalues beyond the zero tolerance, by sign of the real part */
    double spectral_radius = 0;
};

/* the kind of eigenvalues re + i im, and their counts; pure */
void auto_approach_judge(AutoApproach &approach);

/* what the end of the branch is, from the equilibria its orbit lingers near:
   a SNIC for one with a zero eigenvalue, a homoclinic orbit for one saddle, a
   heteroclinic cycle for two or more distinct ones; End (not named) for
   anything else; pure */
AutoPointKind auto_global_kind(const std::vector<AutoApproach> &approaches);

/* the slowest mesh point of each stretch of consecutive points (the orbit is
   a closed curve: the last continues into the first) whose speed is at most
   ratio of the largest, slowest first; pure */
std::vector<std::size_t> auto_slow_points(std::span<const double> speeds, double ratio);

/* the end found: its type (End when not named), the periods and where the
   orbit lingers */
struct AutoGlobalEnd {
    AutoPointKind kind = AutoPointKind::End;
    double period = NAN, shortest = NAN;
    std::vector<AutoApproach> approaches;
    std::string failed; /* what stopped the search for an equilibrium (an error's text), else empty */
};

/* The end of the branch at the point just computed, when its orbit lingers
   (the slowest mesh point's speed at most auto_global_slow_ratio of the
   fastest's): none when it does not, and the period grows for another
   reason, the branch going on. ups is AUTO's solution, ndxloc rows; period
   and shortest are the watch's */
std::optional<AutoGlobalEnd> auto_global_end(const iap_type &iap, const double *par, const std::ptrdiff_t *icp,
                                             const double *ups, std::ptrdiff_t ndxloc, double period, double shortest);

/* the stop's words: "period growing without bound (...): the period ... lingers near ..." */
std::string auto_global_text(const Session &s, const AutoGlobalEnd &end);

} // namespace xpp
