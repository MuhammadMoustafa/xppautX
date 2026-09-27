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

#include <array>
#include "xpplim.h"
/* Sing pts' shooting (gear.cpp), a Session's (session.h): the initial
   conditions it found along the unstable and stable manifolds (count of
   ic, ic_flag once there are any), and the manifolds' colours (@ smc=,
   umc=) */
struct ManifoldShots {
  std::array<std::array<double, MAXODE>, 8> ic{};
  int ic_flag = 0, count = 0;
  int stable_color = 8, unstable_color = 5;
};
#endif
#endif
