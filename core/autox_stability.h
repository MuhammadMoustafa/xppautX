#pragma once

/* The one source of truth for the stability values of autoX's points
   (issue #27, W15): the eigenvalues of a steady state, the Floquet
   multipliers of a periodic orbit, which the diagram stores per point,
   saves in sessions (xpp_session_autox.h) and the stability circle shows (autox_data.h).

   autoX computes them in its stability check (autox_lib1.cpp: store_branching_data for a
   steady state, secondary_bifurcation_test_boundary's floquet_multipliers for an orbit) for the point it is about to
   store, and hands them here with autox_stability_computed(). When the point
   is stored (autox_evd.cpp addbif) autox_stability_for() gives its values only
   if they are that point's; otherwise the point was "not computed". autoX
   computes a run's first point when it stores it, a steady state's (a run's or a
   branch's, store_first_point_algebraic) and a periodic orbit's (control_boundary's
   restart, W858, #939): the same orbit's values where a stored point gives them (a
   run restarting from a stored label of the same kind: steady to steady, periodic
   to periodic, a two-parameter curve of orbits from its PD, TR or NS point), the
   Floquet multipliers of a Hopf start's zero-amplitude orbit, exp(lambda T) of the
   Hopf point's eigenvalues (autox_stability_run_start_zero_amplitude), and otherwise
   the pencil's (its own).

   A failed step restores its predictor's old solution, and the MX endpoint
   takes that solution's retained stability (W341, #402).

   "Not computed" is all zeros, the form sessions keep (as XPPAUT's
   .auto files did); the page says so (web2 autoxInfo.ts). XPPAUT itself stores the
   previous point's values there.

   autox_stability.cpp; nothing escapes it. */

#include <span>
#include <vector>
namespace xpp { struct AutoxPointCode; }


namespace xpp {

/* what a point or a run continues */
enum AutoxStabilityKind {
    AUTOX_STABILITY_NONE = 0, /* no label: a run from initial data */
    AUTOX_STABILITY_STEADY,   /* a steady state (or a map's fixed point): eigenvalues */
    AUTOX_STABILITY_PERIODIC, /* a periodic orbit: Floquet multipliers */
    AUTOX_STABILITY_OTHER     /* a two-parameter curve, a boundary value problem, ... */
};

/* autoX computed the stability of point pt of branch br (either sign):
   values.size()/2 values, values[2k] + i values[2k+1] (autoX's ComplexPair array).
   kind AUTOX_STABILITY_STEADY: eigenvalues lambda, kept as e^lambda (inside
   the unit circle is stable, as for multipliers); AUTOX_STABILITY_PERIODIC:
   Floquet multipliers, kept as they are. */
/* what a Session's autoX run computed and takes (AutoxState::stability) */
struct AutoxStability {
    /* one point's values, re and im */
    struct Values {
        int br = 0, pt = 0; /* the point (abs), 0: none */
        std::vector<double> re, im;
    };
    Values computed; /* the last point autoX checked */
    Values previous; /* the solution saved by the current predictor, before any trials */
    Values first;    /* the running restart's first point (pt 1), when it has one */
    bool has_first = false;
};

/* Before a predictor copies solution pt into uold/uoldps, retain its spectrum.
   Trial solutions must compute their own stability. A failed solve restores
   that old solution and gives its retained spectrum to the endpoint. */
void autox_stability_step_start(AutoxStability &st, int br, int pt);
/* Returns whether the restored solution had a retained spectrum. */
bool autox_stability_step_failed(AutoxStability &st, int br, int pt);

void autox_stability_computed(AutoxStability &st, int br, int pt, std::span<const double> values, AutoxStabilityKind kind);

/* A run starts; nothing computed before it belongs to its points. run: the
   kind it continues; isw: autoX's ISW (-1 switches branches); label: the
   kind of the stored label it restarts from (AUTOX_STABILITY_NONE: none),
   label_itp its autoX point type, evr/evi its stored values (equally many; none: no first point). The run's first
   point takes them if the run continues the label's own solution: same
   kind, steady or periodic, and not a period doubling's switch (the
   doubled orbit's multipliers are not the label's). Returns 1 if it does. */
int autox_stability_run_start(AutoxStability &st, AutoxStabilityKind run, int isw, AutoxStabilityKind label, AutoxPointCode label_itp,
                             std::span<const double> evr, std::span<const double> evi);

/* A run's first point that restarts from a stored label takes the label's values (autox_stability_run_start), not the
   ones just computed for it (store_first_point_algebraic): drops what was computed for point pt of branch br when
   the first point takes the label's values. */
void autox_stability_first_takes_label(AutoxStability &st, int br, int pt);
/* whether point pt of branch br takes the first point's record (autox_stability_run_start) */
bool autox_stability_first_applies(const AutoxStability &st, int br, int pt);
/* A periodic run from a Hopf point starts from a zero-amplitude orbit, the equilibrium itself over its period T: its
   first point's Floquet multipliers are exp(lambda_i T) of the equilibrium's eigenvalues, which the Hopf point stores as
   e^lambda (AUTOX_STABILITY_STEADY). Recorded as the run's first point (after autox_stability_run_start). */
void autox_stability_run_start_zero_amplitude(AutoxStability &st, double period, std::span<const double> evr, std::span<const double> evi);

/* the values of point pt of branch br (either sign) into evr/evi (equally long, filling n = their size):
   1 if they are that point's (computed for it, or the first point of a
   same-kind restart), else 0 and zeros: not computed */
int autox_stability_for(const AutoxStability &st, int br, int pt, std::span<double> evr, std::span<double> evi);

} // namespace xpp
