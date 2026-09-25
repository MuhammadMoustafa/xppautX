#ifndef AUTO_STABILITY_H
#define AUTO_STABILITY_H
#ifdef __cplusplus
extern "C" {
#endif

/* The one source of truth for the stability values of AUTO's points
   (issue #27, W15): the eigenvalues of a steady state, the Floquet
   multipliers of a periodic orbit, which the diagram stores per point,
   saves in .auto files and the stability circle shows (auto_data.h).

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

   "Not computed" is all zeros, the form .auto files keep (XPPAUT reads
   them); the page says so (web2 autoInfo.ts). XPPAUT itself stores the
   previous point's values there.

   auto_stability.cpp; C++ with a C API, nothing escapes it. */

/* what a point or a run continues */
typedef enum {
    AUTO_STABILITY_NONE = 0, /* no label: a run from initial data */
    AUTO_STABILITY_STEADY,   /* a steady state (or a map's fixed point): eigenvalues */
    AUTO_STABILITY_PERIODIC, /* a periodic orbit: Floquet multipliers */
    AUTO_STABILITY_OTHER     /* a two-parameter curve, a boundary value problem, ... */
} AutoStabilityKind;

/* AUTO computed the stability of point pt of branch br (either sign):
   n values, values[2k] + i values[2k+1] (AUTO's doublecomplex array).
   kind AUTO_STABILITY_STEADY: eigenvalues lambda, kept as e^lambda (inside
   the unit circle is stable, as for multipliers); AUTO_STABILITY_PERIODIC:
   Floquet multipliers, kept as they are. */
void auto_stability_computed(int br, int pt, int n, const double *values, int kind);

/* A run starts; nothing computed before it belongs to its points. run: the
   kind it continues; isw: AUTO's ISW (-1 switches branches); label: the
   kind of the stored label it restarts from (AUTO_STABILITY_NONE: none),
   label_itp its AUTO type, evr/evi its n stored values. The run's first
   point takes them if the run continues the label's own solution: same
   kind, steady or periodic, and not a period doubling's switch (the
   doubled orbit's multipliers are not the label's). Returns 1 if it does. */
int auto_stability_run_start(int run, int isw, int label, int label_itp, int n, const double *evr,
                             const double *evi);

/* the n values of point pt of branch br (either sign) into evr/evi:
   1 if they are that point's (computed for it, or the first point of a
   same-kind restart), else 0 and zeros: not computed */
int auto_stability_for(int br, int pt, int n, double *evr, double *evi);

#ifdef __cplusplus
}
#endif
#endif
