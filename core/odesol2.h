#pragma once

namespace xpp {

struct RunParts; /* run_parts.h */

int symplectic_third_order(RunParts &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int discrete(RunParts &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int bak_euler(RunParts &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int one_bak_step(RunParts &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *errvec, double *jac, int *istart);
void one_step_discrete(RunParts &s, double *y, double dt, double *yp, int neq, double *t);
void one_step_symp(RunParts &s, double *y, double h, double *f, int n, double *t);
void one_step_euler(RunParts &s, double *y, double dt, double *yp, int neq, double *t);
void one_step_rk4(RunParts &s, double *y, double dt, double *yval[3], int neq, double *tim);
void one_step_heun(RunParts &s, double *y, double dt, double *yval[2], int neq, double *tim);
int euler(RunParts &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int modified_euler_steps(RunParts &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int runge_kutta_fixed(RunParts &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
int adams(RunParts &s, double *y, double *tim, double dt, int nstep, int neq, int *ist, double *work);
int rosenbrock_driver(RunParts &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
int rosenbrock_steps(RunParts &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
void get_the_jac(RunParts &s, double t, double *y, double *yp, double *dfdy, int neq, double eps, double scal);

} // namespace xpp
