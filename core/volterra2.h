#ifndef _volterra2_h_
#define _volterra2_h_
#ifdef __cplusplus
extern "C" {
#endif


double alpha1n(double mu, double dt, double t, double t0);
double alpbetjn(double mu, double dt, int l);
double betnn(double mu, double dt, double t0, double t);


#ifdef __cplusplus
}

namespace xpp {
struct Session; /* session.h */
}

/* volterra2.c */
double ker_val(xpp::Session &s, int in);
void alloc_v_memory(xpp::Session &s);
void allocate_volterra(xpp::Session &s, int npts, int flag);
void re_evaluate_kernels(xpp::Session &s);
void alloc_kernels(xpp::Session &s, int flag);
void init_sums(xpp::Session &s, double t0, int n, double dt, int i0, int iend, int ishift);
void get_kn(xpp::Session &s, double *y, double t);
int volterra(xpp::Session &s, double *y, double *t, double dt, int nt, int neq, int *istart, double *work);
int volt_step(xpp::Session &s, double *y, double t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac);
#endif
#endif

