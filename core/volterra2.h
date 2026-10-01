#ifndef _volterra2_h_
#define _volterra2_h_

namespace xpp {

struct Session; /* session.h */

double alpha1n(double mu, double dt, double t, double t0);
double alpbetjn(double mu, double dt, int l);
double betnn(double mu, double dt, double t0, double t);

double ker_val(Session &s, int in);
void alloc_v_memory(Session &s);
void allocate_volterra(Session &s, int npts, int flag);
void re_evaluate_kernels(Session &s);
void alloc_kernels(Session &s, int flag);
void init_sums(Session &s, double t0, int n, double dt, int i0, int iend, int ishift);
void get_kn(Session &s, double *y, double t);
int volterra(Session &s, double *y, double *t, double dt, int nt, int neq, int *istart, double *work);
int volt_step(Session &s, double *y, double t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac);

} // namespace xpp
#endif
