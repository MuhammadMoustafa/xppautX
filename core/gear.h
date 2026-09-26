#ifndef _gear_h_
#define _gear_h_
#ifdef __cplusplus
extern "C" {
#endif

void do_sing(double *x, double eps, double err, double big, int maxit, int n, int *ierr, float *stabinfo);
void do_sing_info(double *x, double eps, double err, double big, int maxit, int n, double *er, double *em, int *ierr);


void shoot_this_now();
void pr_evec(double *x, double *ev, int n, int pr, double eval,int type);
void get_evec(double *a, double *anew, double *b, double *bp, int n, int maxit, double err, int *ipivot, double eval, int *ierr);
void getjactrans(double *x,double *y,double *yp,double *xp, double eps, double *d, int n);
void getjac(double *x, double *y, double *yp, double *xp, double eps, double *dermat, int n);
void rooter(double *x, double err, double eps, double big, double *work, int *ierr, int maxit, int n);
double sqr2(double z);
int gear(int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);
int ggear(int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);


#ifdef __cplusplus
}
#endif
#endif
