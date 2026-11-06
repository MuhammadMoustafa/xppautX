#pragma once

/* The global end of a periodic branch (W518, #582): a cycle branch whose
   period grows without bound, autoX's MX or step limit with no word on why,
   ends on a saddle-node on an invariant circle (SNIC), a homoclinic orbit to
   a saddle or a heteroclinic cycle between saddles. Nothing here changes
   what autoX computes: stplbv hands each periodic point's period to the
   watch below, and when the period has grown for a run of points while the
   orbit lingers where the vector field all but vanishes, the equilibria the
   orbit lingers near are searched (equilibria.h's equilibrium_near), their
   eigenvalues judged, and the end named. The branch then ends there with
   the new point type and says why (autox_stop.h).

   autox_global.cpp. */

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "autox_codes.h"

struct iap_type; /* autox_c.h */

namespace xpp {

struct Session; /* session.h */
struct Model;   /* model.h */
class EquilibriumModel; /* equilibria.h */

/* The criterion, each number with its reason. A period that has grown over
   this many points in a row is no wobble of the continuation's step ... */
inline constexpr int autox_global_growing_points = 5;
/* ... and is this multiple of the branch's shortest period: the lingering
   below, not this factor, says it is a global bifurcation (a homoclinic
   period grows with the logarithm of the distance to the saddle, a SNIC's
   with its inverse square root: no one multiple fits both) */
inline constexpr double autox_global_growth_factor = 2;
/* The orbit lingers where some mesh point's speed |f(x)| is at most this
   fraction of the orbit's largest. A cycle of a model with slow variables
   (mu26's calcium and gating, 28 states) passes 1e-3 to 1e-5 of its fastest
   speed for most of its period with an equilibrium nowhere near; a SNIC's
   bottleneck passes a millionth of it when the period is some thousand
   times the shortest, a homoclinic orbit when its period is a few times
   (autoX follows it to periods of 1e5 with the parameter at its limit). Also
   what makes a stretch of the orbit slow, to be searched for an equilibrium */
inline constexpr double autox_global_slow_ratio = 1e-6;
/* An eigenvalue is zero, and the equilibrium on a fold, when it is this
   fraction of the spectral radius from zero: the ghost of a fold has an
   eigenvalue of the order of the square root of its distance from the fold
   (1e-4 of the radius in mu26 at its SNIC), a slow mode of the model is
   several times more (3e-3 there) */
inline constexpr double autox_global_zero_eigenvalue = 1e-3;
/* The equilibrium the orbit lingers near is one of a saddle-node pair, the node or the saddle whichever the search finds first,
   when exactly one of its eigenvalues is this fraction of the spectral radius from zero: the pair's two equilibria have
   eigenvalues of opposite sign and nearly equal size, of the order of the square root of their distance from the fold, so
   the search can land on either (the class-I Morris-Lecar cycle of tests/autox/ode_lecar_orbit_snic stops at 8e-3, with
   the pair's eigenvalues 2e-2 of the radius 1.7e-5 beyond the fold). Wider than the zero-eigenvalue tolerance above, which
   only decides an eigenvalue's sign */
inline constexpr double autox_global_pair_eigenvalue = 2e-2;
/* Two equilibria (or slow stretches with none) are one when closer than this
   fraction of the orbit's extent: Newton's tolerance is far below, and the
   saddles of a heteroclinic cycle are an orbit's length apart */
inline constexpr double autox_global_same_equilibrium = 1e-2;
/* A saddle the orbit passes near at a local minimum of its speed that is no slow point counts when Newton's method from that
   minimum lands this fraction of the orbit's extent from it (a coarse mesh samples a saddle's neighbourhood only at its
   edges: pp2's NTST 15 lands 5e-4 from the saddles it passes, mu26's SNIC orbit lands 0.28 and more from the equilibria it
   jumps to, which are no approach at all) */
inline constexpr double autox_global_near_orbit = 1e-2;
/* The variables a message names for an equilibrium; the rest is "..." */
inline constexpr std::size_t autox_global_shown_variables = 6;

/* a number as the stop texts spell it: six significant digits */
std::string autox_number_text(double v);

/* an equilibrium's variables in words ("V" = 21.9, "n" = 0.3, ...: the first autox_global_shown_variables) */
std::string autox_equilibrium_text(const Model &m, std::span<const double> x);

/* the model at x with autoX's parameters par: its right-hand side into f, and its constants left at those
   parameters, which Newton's method and the Jacobian (equilibrium_near) read */
void autox_evaluate_at(const iap_type &iap, const double *par, const std::ptrdiff_t *icp, std::span<const double> x,
                       std::span<double> f);

/* The evaluation the equilibrium search of a run's ends (a Hopf end, a global end) uses at autoX's parameters par (copied):
   the run's own when it evaluates on its own constants (a job run at once or in turn with isolation), else the Session's,
   its constants left at par. Never the Session's parser from a job, and the job's cancel is its Escape. */
std::unique_ptr<EquilibriumModel> autox_equilibrium_model(const iap_type &iap, const double *par, const std::ptrdiff_t *icp,
                                                           std::span<const double> state);

/* How many points in a row a series has gone one way: run, one more when value goes on from last (above it when
   rising, below it otherwise), else 0 (a first point, last NaN, starts again). The period's growth and (W604) a
   cycle's amplitude falling are counted by it */
inline int autox_trend_run(int run, double last, double value, bool rising)
{
    return (rising ? value > last : value < last) ? run + 1 : 0;
}

/* A branch's periods so far (AutoxStop::period_watch): the shortest, the last
   and how many points in a row the period has grown */
struct AutoxPeriodWatch {
    double shortest = INFINITY, last = NAN;
    int growing = 0;
};

/* The period of a branch's next point; first_point starts the branch's watch.
   True when the period has grown over autox_global_growing_points points in a
   row and is autox_global_growth_factor times the shortest */
bool autox_period_watch_add(AutoxPeriodWatch &watch, double period, bool first_point);

/* what an equilibrium (or a slow stretch's ghost of one) the orbit lingers
   near is, by its eigenvalues */
enum class AutoxApproachKind {
    saddle,          /* hyperbolic, with unstable and stable eigenvalues */
    zero_eigenvalue, /* an eigenvalue at or near zero: on or near a fold of the equilibria (a saddle-node pair) */
    other            /* a node or focus, or no real equilibrium and no zero eigenvalue */
};

struct AutoxApproach {
    /* the equilibrium, or the orbit's slowest point of the stretch when Newton's method found none */
    std::vector<double> x;
    bool exists = false; /* Newton's method found the equilibrium */
    std::vector<double> re, im; /* the Jacobian's eigenvalues there */
    AutoxApproachKind kind = AutoxApproachKind::other;
    std::size_t unstable = 0, stable = 0; /* eigenvalues beyond the zero tolerance, by sign of the real part */
    double spectral_radius = 0;
};

/* the kind of eigenvalues re + i im, and their counts; pure */
void autox_approach_judge(AutoxApproach &approach);

/* what the end of the branch is, from the equilibria its orbit lingers near:
   a SNIC for one with a zero eigenvalue, a homoclinic orbit for one saddle, a
   heteroclinic cycle for two or more distinct ones; End (not named) for
   anything else; pure */
AutoxPointKind autox_global_kind(const std::vector<AutoxApproach> &approaches);

/* the slowest mesh point of each stretch of consecutive points (the orbit is
   a closed curve: the last continues into the first) whose speed is at most
   ratio of the largest, slowest first; pure */
std::vector<std::size_t> autox_slow_points(std::span<const double> speeds, double ratio);

/* the end found: its type (End when not named), the periods and where the
   orbit lingers */
struct AutoxGlobalEnd {
    AutoxPointKind kind = AutoxPointKind::End;
    double period = NAN, shortest = NAN;
    std::vector<AutoxApproach> approaches;
    std::string failed; /* what stopped the search for an equilibrium (an error's text), else empty */
};

/* The end of the branch at the point just computed, when its orbit lingers
   (the slowest mesh point's speed at most autox_global_slow_ratio of the
   fastest's): none when it does not, and the period grows for another
   reason, the branch going on. ups is autoX's solution, ndxloc rows; period
   and shortest are the watch's */
std::optional<AutoxGlobalEnd> autox_global_end(const iap_type &iap, const double *par, const std::ptrdiff_t *icp,
                                             const double *ups, std::ptrdiff_t ndxloc, double period, double shortest);

/* the stop's words: "period growing without bound (...): the period ... lingers near ..." */
std::string autox_global_text(const Session &s, const AutoxGlobalEnd &end);

} // namespace xpp
