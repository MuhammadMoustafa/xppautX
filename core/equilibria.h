#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_error.h"
#include "xpplim.h"
#include "colormap.h"

namespace xpp {

struct Session; /* session.h */

/* how Newton's method for an equilibrium ended: the way it converged, or what stopped it (a failed
   evaluation of the right-hand side is rooter's error, not an end) */
enum class NewtonEnd { converged, singular_jacobian, left_box, max_iterates, cancelled, step_error };
struct NewtonRun {
  NewtonEnd end = NewtonEnd::converged;
  int iterations = 0;   /* the steps taken */
  double change = 0;    /* the size of the last step (the sum of its parts) */
  int worst = 0;        /* the variable whose equation was furthest from zero at the last iterate */
};
/* what stopped Newton's method, as words ("exceeded max_iterations = 100 ...") and the key of the
   setting that did ("" when none): for n variables */
struct NewtonCause {
  std::string why;
  std::string_view setting;
};
NewtonCause newton_cause(const Session &s, const NewtonRun &run, std::size_t variables);
/* why Newton's method from start (the variables' values) found no equilibrium, with the setting that
   stopped it and the model line (the setting's, else the worst equation's) */
Error newton_error(const Session &s, const NewtonRun &run, std::span<const double> start);

/* the eigenvector of the Jacobian a (a copy, anew, is factored) for eval, into b and bp (inverse
   iteration); the error says why there is none */
Result<> get_evec(Session &s, double *a, double *anew, double *b, double *bp, int n, int maxit, double err, int *ipivot, double eval);

Result<> do_sing_info(Session &s, double *x, double eps, double err, double big, int maxit, int n, double *er, double *em, int *ierr);
void pr_evec(Session &s, double *x, double *ev, int n, int pr, double eval,int type);
void getjac(Session &s, double *x, double *y, double eps, double *dermat, int n);
/* (a failed exact Jacobian is its error, the point back where it was) */
Result<> rooter(Session &s, double *x, double err, double eps, double big, double *work, NewtonRun &run, int maxit, int n);

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
/* Sing pts' shooting (equilibria.cpp), a Session's (session.h): the initial
   conditions it found along the unstable and stable manifolds (count of
   ic, ic_flag once there are any), and the manifolds' colours (@ smc=,
   umc=) */
struct ManifoldShots {
  std::array<std::vector<double>, 8> ic; /* each node values (pr_evec sizes it) */
  /* each one's kind: stable or unstable (pr_evec's type) */
  std::array<int, 8> type{};
  int ic_flag = 0, count = 0;
  Color stable_color = xppaut_color(8), unstable_color = xppaut_color(5);
};

} // namespace xpp
