#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_error.h"
#include "colormap.h"
#include "jacobian.h"
#include <memory>

namespace xpp {

struct Session; /* session.h */

/* What Newton's method asks of the model, so that the one search serves two evaluations: the Session's own (Sing pts, the
   delay equations' stability: session_equilibrium_model) and a job's of autoX (autox_equilibrium_model), which evaluates
   on the job's own constants, never the Session's parser, and answers Escape through the job. */
class EquilibriumModel {
public:
  virtual ~EquilibriumModel() = default;
  /* the model's right-hand side f at x */
  virtual void rhs(const double *x, double *f) = 0;
  /* its Jacobian at x, where f0 is the right-hand side (a map's: f(x)-x), in form */
  virtual void jacobian(const double *x, const double *f0, double eps, const JacobianForm &form, double *out) = 0;
  /* the solver is a map: its equilibria are the fixed points */
  virtual bool discrete() const = 0;
  /* asked once per iteration: the search is stopped (Escape; "/" ends it too, and the model notes that) */
  virtual bool stopped() = 0;
  /* an evaluation failed (a delay, an algebraic solve: the Session's): take_failure() says why and forgets it */
  virtual bool failed() const { return false; }
  virtual Error take_failure() { return Error{}; }
};
/* the Session's own evaluation (it holds the computation Escape stops while it lives) */
std::unique_ptr<EquilibriumModel> session_equilibrium_model(Session &s, int n);

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

/* The equilibrium near a start (W518: where a long orbit lingers): Newton's method from it with the
   session's equilibria settings, and the eigenvalues (re, im: one per variable) of the Jacobian at the
   root; when Newton finds no root, found is false and they are the Jacobian's at the start itself
   (an equilibrium that has just vanished, a fold's ghost, still has its near-zero eigenvalue). It marks
   nothing on the graph and integrates no manifold; an error is the model's own (a failed evaluation). The model is
   evaluated by `model`; the settings are s's. */
struct EquilibriumNear {
  bool found = false;
  std::vector<double> x, re, im;
};
Result<EquilibriumNear> equilibrium_near(const Session &s, EquilibriumModel &model, std::span<const double> start);

Result<> do_sing_info(Session &s, double *x, double eps, double err, double big, int maxit, int n, double *er, double *em, int *ierr);
void pr_evec(Session &s, double *x, double *ev, int n, int pr, double eval,int type);
void getjac(EquilibriumModel &model, double *x, double *y, double eps, double *dermat, int n);
/* the variable whose equation f (n values) is furthest from zero: the equation an error about it names */
int worst_residual(const double *f, int n);
/* (a failed exact Jacobian is its error, the point back where it was) */
Result<> rooter(EquilibriumModel &model, double *x, double err, double eps, double big, double *work, NewtonRun &run, int maxit, int n);
/* the same on the Session's own evaluation */
Result<> rooter(Session &s, double *x, double err, double eps, double big, double *work, NewtonRun &run, int maxit, int n);

/* Sing pts: the equilibrium Newton finds from x (Could not converge,
   no eigenvalues: an error), its stability (stabinfo) and, when asked,
   its invariant manifolds, integrated from it; an eigenvector or a
   manifold that failed is returned once the rest are done */
Result<> do_sing(Session &s, double *x, double eps, double err, double big, int maxit, int n, int *ierr, float *stabinfo);
/* integrates the manifolds from the saddle's stored starting points,
   returning the first that failed once all are done */
Result<> shoot_this_now(Session &s);
/* Integrate the stored manifolds into file-stem-eigenN-stable/unstable.dat,
   using the export command's overwrite decision and returning its first error. */
Result<> save_batch_shoot(Session &s, std::string_view file, bool confirm);
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
