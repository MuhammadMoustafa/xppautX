#ifndef _odesol2_h_
#define _odesol2_h_

namespace xpp {

struct Session; /* session.h */

int symplect3(Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int discrete(Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int bak_euler(Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int one_bak_step(Session &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac, int *istart);
void one_step_discrete(Session &s, double *y, double dt, double *yp, int neq, double *t);
void one_step_symp(Session &s, double *y, double h, double *f, int n, double *t);
void one_step_euler(Session &s, double *y, double dt, double *yp, int neq, double *t);
void one_step_rk4(Session &s, double *y, double dt, double *yval[3], int neq, double *tim);
void one_step_heun(Session &s, double *y, double dt, double *yval[2], int neq, double *tim);
int euler(Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int mod_euler(Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int rung_kut(Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int adams(Session &s, double *y, double *tim, double dt, int nstep, int neq, int *ist, double *work);
int rb23(Session &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
int rosen(Session &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
void get_the_jac(Session &s, double t, double *y, double *yp, double *ypnew, double *dfdy, int neq, double eps, double scal);
void get_band_jac(Session &s, double *a, double *y, double t, double *ypnew, double *ypold, int n, double eps, double scal);

} // namespace xpp
#endif
