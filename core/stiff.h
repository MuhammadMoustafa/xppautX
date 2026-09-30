#ifndef _stiff_h_
#define _stiff_h_
#ifdef __cplusplus
extern "C" {
#endif




#ifdef __cplusplus
}

namespace xpp {
struct Session; /* session.h */
}

/* stiff.c */
void jacobn(xpp::Session &s, double x, double *y, double *dfdx, double *dermat, double eps, double *work, int n);
int adaptive(xpp::Session &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int gadaptive(xpp::Session &s, double *ystart, int nvar, double *xs, double x2, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int stiff(xpp::Session &s, double y[], double dydx[], int n, double *x, double htry, double eps, double yscal[], double *hdid, double *hnext, double *work, double epjac, int *ier);
int rkqs(xpp::Session &s, double *y, double *dydx, int n, double *x, double htry, double eps, double *yscal, double *hdid, double *hnext, double *work, int *ier);
void rkck(xpp::Session &s, double *y, double *dydx, int n, double x, double h, double *yout, double *yerr, double *work);
#endif
#endif

