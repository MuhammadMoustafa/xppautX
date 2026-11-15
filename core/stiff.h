#pragma once

namespace xpp {

struct RunParts; /* run_parts.h */

/* QualRK's and Stiff's limits: the steps between two output times (a run
   that needs more has a tolerance too tight or a solution too stiff for
   the method: it stops rather than spin), and the tries of one stiff step,
   each with a smaller step (Numerical Recipes' stiff, MAXTRY) */
inline constexpr int ADAPTIVE_MAX_STEPS = 100000;
inline constexpr int ADAPTIVE_MAX_TRIES = 40;
/* why an adaptive advance stopped (its *ier): the step no longer changes t,
   it fell below min_step, more than ADAPTIVE_MAX_STEPS steps, more than
   ADAPTIVE_MAX_TRIES tries, or the stiff step's singular matrix
   (SolverWork::Stop says where) */
inline constexpr int ADAPTIVE_STEP_UNDERFLOW = 1, ADAPTIVE_BELOW_MIN_STEP = 2, ADAPTIVE_TOO_MANY_STEPS = 3,
                     ADAPTIVE_TOO_MANY_TRIES = 4, ADAPTIVE_SINGULAR = -1;

void stiff_numerical_jacobian(RunParts &s, double x, double *y, double *dfdx, double *dermat, double eps, double *work, int n);
int adaptive(RunParts &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int adaptive_integration_without_flags(RunParts &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int stiff(RunParts &s, double y[], double dydx[], int n, double *x, double htry, double eps, double yscal[], double *hdid, double *hnext, double *work, double epjac, int *ier);
int cash_karp_adaptive_step(RunParts &s, double *y, double *dydx, int n, double *x, double htry, double eps, double *yscal, double *hdid, double *hnext, double *work, int *ier);
void cash_karp_step(RunParts &s, double *y, double *dydx, int n, double x, double h, double *yout, double *yerr, double *work);

} // namespace xpp
