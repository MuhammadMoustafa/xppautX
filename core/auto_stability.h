#pragma once

/* The one source of truth for the stability values of AUTO's points
   (issue #27, W15): the eigenvalues of a steady state, the Floquet
   multipliers of a periodic orbit, which the diagram stores per point,
   saves in sessions (xpp_session_auto.h) and the stability circle shows (auto_data.h).

   AUTO computes them in its stability check (autlib1.cpp: stbif for a
   steady state, fnspbv's flowkm for an orbit) for the point it is about to
   store, and hands them here with auto_stability_computed(). When the point
   is stored (autevd.cpp addbif) auto_stability_for() gives its values only
   if they are that point's; otherwise the point was "not computed". AUTO
   does not check stability at a run's first point, so that one is "not
   computed" unless the run restarts from a stored label of the same kind
   (steady state to steady state, periodic orbit to periodic orbit, one
   parameter both): its first point is exactly the label's solution, and
   takes the label's values (auto_stability_run_start()).

   A failed step restores its predictor's old solution, and the MX endpoint
   takes that solution's retained stability (W341, #402).

   "Not computed" is all zeros, the form sessions keep (as XPPAUT's
   .auto files did); the page says so (web2 autoInfo.ts). XPPAUT itself stores the
   previous point's values there.

   auto_stability.cpp; nothing escapes it. */

#include <span>
#include <vector>

#include "auto_codes.h"

namespace xpp {

/* what a point or a run continues */
enum AutoStabilityKind {
    AUTO_STABILITY_NONE = 0, /* no label: a run from initial data */
    AUTO_STABILITY_STEADY,   /* a steady state (or a map's fixed point): eigenvalues */
    AUTO_STABILITY_PERIODIC, /* a periodic orbit: Floquet multipliers */
    AUTO_STABILITY_OTHER     /* a two-parameter curve, a boundary value problem, ... */
};

/* AUTO computed the stability of point pt of branch br (either sign):
   values.size()/2 values, values[2k] + i values[2k+1] (AUTO's doublecomplex array).
   kind AUTO_STABILITY_STEADY: eigenvalues lambda, kept as e^lambda (inside
   the unit circle is stable, as for multipliers); AUTO_STABILITY_PERIODIC:
   Floquet multipliers, kept as they are. */
/* what a Session's AUTO run computed and takes (AutoState::stability) */
struct AutoStability {
    /* one point's values, re and im */
    struct Values {
        int br = 0, pt = 0; /* the point (abs), 0: none */
        std::vector<double> re, im;
    };
    Values computed; /* the last point AUTO checked */
    Values previous; /* the solution saved by the current predictor, before any trials */
    Values first;    /* the running restart's first point (pt 1), when it has one */
    bool has_first = false;
};

/* Before a predictor copies solution pt into uold/uoldps, retain its spectrum.
   Trial solutions must compute their own stability. A failed solve restores
   that old solution and gives its retained spectrum to the endpoint. */
void auto_stability_step_start(AutoStability &st, int br, int pt);
/* Returns whether the restored solution had a retained spectrum. */
bool auto_stability_step_failed(AutoStability &st, int br, int pt);

void auto_stability_computed(AutoStability &st, int br, int pt, std::span<const double> values, AutoStabilityKind kind);

/* A run starts; nothing computed before it belongs to its points. run: the
   kind it continues; isw: AUTO's ISW (-1 switches branches); label: the
   kind of the stored label it restarts from (AUTO_STABILITY_NONE: none),
   label_itp its AUTO point type, evr/evi its stored values (equally many; none: no first point). The run's first
   point takes them if the run continues the label's own solution: same
   kind, steady or periodic, and not a period doubling's switch (the
   doubled orbit's multipliers are not the label's). Returns 1 if it does. */
int auto_stability_run_start(AutoStability &st, AutoStabilityKind run, int isw, AutoStabilityKind label, AutoPointCode label_itp,
                             std::span<const double> evr, std::span<const double> evi);

/* the values of point pt of branch br (either sign) into evr/evi (equally long, filling n = their size):
   1 if they are that point's (computed for it, or the first point of a
   same-kind restart), else 0 and zeros: not computed */
int auto_stability_for(const AutoStability &st, int br, int pt, std::span<double> evr, std::span<double> evi);

} // namespace xpp
