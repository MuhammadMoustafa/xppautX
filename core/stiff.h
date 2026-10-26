#pragma once

namespace xpp {

struct Session; /* session.h */

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

void jacobn(Session &s, double x, double *y, double *dfdx, double *dermat, double eps, double *work, int n);
int adaptive(Session &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int gadaptive(Session &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int stiff(Session &s, double y[], double dydx[], int n, double *x, double htry, double eps, double yscal[], double *hdid, double *hnext, double *work, double epjac, int *ier);
int rkqs(Session &s, double *y, double *dydx, int n, double *x, double htry, double eps, double *yscal, double *hdid, double *hnext, double *work, int *ier);
void rkck(Session &s, double *y, double *dydx, int n, double x, double h, double *yout, double *yerr, double *work);

} // namespace xpp
