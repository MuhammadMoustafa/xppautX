#pragma once

namespace xpp {

struct RunParts; /* run_parts.h */

double alpha1n(double mu, double dt, double t, double t0);
double alpbetjn(double mu, double dt, int l);
double betnn(double mu, double dt, double t0, double t);

double ker_val(RunParts &s, int in);
void alloc_v_memory(RunParts &s);
void allocate_volterra(RunParts &s, int npts, int flag);
void re_evaluate_kernels(RunParts &s);
void alloc_kernels(RunParts &s, int flag);
void init_sums(RunParts &s, double t0, int n, double dt, int i0, int iend, int ishift);
void get_kn(RunParts &s, double *y, double t);
int volterra(RunParts &s, double *y, double *t, double dt, int nt, int neq, int *istart, double *work);
int volt_step(RunParts &s, double *y, double t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac);

} // namespace xpp
