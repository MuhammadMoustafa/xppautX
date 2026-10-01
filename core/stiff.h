#ifndef _stiff_h_
#define _stiff_h_

namespace xpp {

struct Session; /* session.h */

void jacobn(Session &s, double x, double *y, double *dfdx, double *dermat, double eps, double *work, int n);
int adaptive(Session &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int gadaptive(Session &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int stiff(Session &s, double y[], double dydx[], int n, double *x, double htry, double eps, double yscal[], double *hdid, double *hnext, double *work, double epjac, int *ier);
int rkqs(Session &s, double *y, double *dydx, int n, double *x, double htry, double eps, double *yscal, double *hdid, double *hnext, double *work, int *ier);
void rkck(Session &s, double *y, double *dydx, int n, double x, double h, double *yout, double *yerr, double *work);

} // namespace xpp
#endif
