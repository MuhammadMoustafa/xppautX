#ifndef _odesol2_h_
#define _odesol2_h_
#ifdef __cplusplus
extern "C" {
#endif

/* the right-hand side the integrators call (my_rhs) */



#ifdef __cplusplus
}

namespace xpp {
struct Session; /* session.h */
}

int symplect3(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int discrete(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int bak_euler(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int one_bak_step(xpp::Session &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac, int *istart);
void one_step_discrete(xpp::Session &s, double *y, double dt, double *yp, int neq, double *t);
void one_step_symp(xpp::Session &s, double *y, double h, double *f, int n, double *t);
void one_step_euler(xpp::Session &s, double *y, double dt, double *yp, int neq, double *t);
void one_step_rk4(xpp::Session &s, double *y, double dt, double *yval[3], int neq, double *tim);
void one_step_heun(xpp::Session &s, double *y, double dt, double *yval[2], int neq, double *tim);
int euler(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int mod_euler(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int rung_kut(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int adams(xpp::Session &s, double *y, double *tim, double dt, int nstep, int neq, int *ist, double *work);
int abmpc(xpp::Session &s, double *y, double *t, double dt, int neq);
int rb23(xpp::Session &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
int rosen(xpp::Session &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
void get_the_jac(xpp::Session &s, double t, double *y, double *yp, double *ypnew, double *dfdy, int neq, double eps, double scal);
void get_band_jac(xpp::Session &s, double *a, double *y, double t, double *ypnew, double *ypold, int n, double eps, double scal);
#endif
#endif
