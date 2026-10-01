#ifndef _gear_h_
#define _gear_h_

#include <array>
#include "xpp_error.h"
#include "xpplim.h"

namespace xpp {

struct Session; /* session.h */
class Random;   /* xpp_math.h */

void get_evec(Random &random, double *a, double *anew, double *b, double *bp, int n, int maxit, double err, int *ipivot, double eval, int *ierr);
double sqr2(double z);

void do_sing_info(Session &s, double *x, double eps, double err, double big, int maxit, int n, double *er, double *em, int *ierr);
void pr_evec(Session &s, double *x, double *ev, int n, int pr, double eval,int type);
void getjac(Session &s, double *x, double *y, double *yp, double *xp, double eps, double *dermat, int n);
void getjactrans(Session &s, double *x,double *y,double *yp,double *xp, double eps, double *d, int n);
void rooter(Session &s, double *x, double err, double eps, double big, double *work, int *ierr, int maxit, int n);
int gear(Session &s, int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);
int ggear(Session &s, int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);

/* Sing pts: the equilibrium Newton finds from x (Could not converge,
   no eigenvalues: an error), its stability (stabinfo) and, when asked,
   its invariant manifolds, integrated from it; an eigenvector or a
   manifold that failed is returned once the rest are done */
Result<> do_sing(Session &s, double *x, double eps, double err, double big, int maxit, int n, int *ierr, float *stabinfo);
/* integrates the manifolds from the saddle's stored starting points,
   returning the first that failed once all are done */
Result<> shoot_this_now(Session &s);
/* integrates the saddle's manifolds from their starting points into
   UMk.dat and SMk.dat (write_equilibrium's shoot) */
void save_batch_shoot(Session &s);
/* Sing pts' shooting (gear.cpp), a Session's (session.h): the initial
   conditions it found along the unstable and stable manifolds (count of
   ic, ic_flag once there are any), and the manifolds' colours (@ smc=,
   umc=) */
struct ManifoldShots {
  std::array<std::array<double, MAXODE>, 8> ic{};
  /* each one's kind: stable or unstable (pr_evec's type) */
  std::array<int, 8> type{};
  int ic_flag = 0, count = 0;
  int stable_color = 8, unstable_color = 5;
};

} // namespace xpp
#endif
