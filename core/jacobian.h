#ifndef _jacobian_h_
#define _jacobian_h_

#include <array>
#include <vector>
#include <span>
#include <functional>
#include <string_view>
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

// Compiled derivative evaluation storage; AUTO workers each own one.
struct JacobianScratch {
  std::vector<double> point, workspace, values;
};

/* How the model's Jacobian is formed, the numerics' `jacobian` (W260):
   by forward differences, XPPAUT's (the default: a model gives XPPAUT's
   numbers), or exactly, from the derivatives compile_model made of the
   model's right-hand sides (Model::jacobian). Their names, by value, are
   what the `jacobian` option and `set num` take. */
constexpr int JACOBIAN_DIFFERENCE = 0, JACOBIAN_EXACT = 1;
inline constexpr std::array<std::string_view, 2> jacobian_names{"difference", "exact"};
inline constexpr std::string_view jacobian_name(int jacobian) { return jacobian_names[static_cast<std::size_t>(jacobian)]; }
/* its key in the numerics (`set num`, the numerics event) */
inline constexpr std::string_view JACOBIAN_KEY = "jacobian";

/* How the Jacobian is stored: d f_row / d x_col is out[row*n+col]
   (RowMajor) or out[row+n*col] (ColumnMajor); Banded, with `lower` and
   `upper` diagonals and mt = lower+upper+1, is out[row*mt+(col-row)+lower]
   (the layout bandfac wants), the rest of the band zero. */
enum class JacobianLayout { RowMajor, ColumnMajor, Banded };

struct JacobianForm {
  JacobianLayout layout = JacobianLayout::RowMajor;
  int lower = 0, upper = 0;      /* Banded only */
  bool discrete_map = false;     /* the Jacobian of f(x)-x, for a map's fixed point */
};

/* The form the implicit solvers (Backward Euler, Rosenbrock) store the
   model's Jacobian in: banded when the numerics ask for a band, with the
   widths clamped to n-1 (a band at least as wide as the system is the
   full matrix), so differencing, factorisation and the work size agree
   (W261). */
JacobianForm model_jacobian_form(const Session &s, int n);

/* How many doubles a Jacobian of n equations in `form` holds. */
int jacobian_entries(int n, const JacobianForm &form);

/* The one step rule of every forward difference of ours (W263):
   eps * max(eps, |x|), so a variable near zero still gets a step. */
double difference_step(double eps, double x);

/* What a Jacobian differences: evaluate(i, xi, f) puts into f the function
   with component i of the point replaced by xi (the rest as at x). The
   default is the model's rhs at (t, x); del_stab's delay Jacobians perturb
   the delay's copy of x instead. */
using JacobianEvaluate = std::function<void(int i, double xi, double *f)>;

/* The Jacobian of `evaluate` at x by forward differences, f0 = f(x). */
void jacobian(const JacobianEvaluate &evaluate, const double *x, const double *f0, int n, double eps, const JacobianForm &form, double *out);

/* The model's Jacobian at (t, x): by forward differences (the one owner
   of the step rule, W255), or the exact one when the numerics say so
   (W260; then eps and f0 are not used). `f0` is f(t, x) (for a map,
   already f(x)-x), `dfdt`, if given, receives df/dt. */
void jacobian(Session &s, double t, const double *x, const double *f0, int n, double eps, const JacobianForm &form, double *out, double *dfdt = nullptr);

/* An exact derivative that is not finite where the right-hand side is
   (W260: sqrt or log at 0, 1/x at 0, a power of a base at most 0): the
   run stops there. The exact Jacobian's functions below throw it, with
   the equation's line, the variable, the time and the state; the
   computation that asked catches it where it ends and returns its error
   (Solver::advance, the equilibria's rooter, the adjoint, AUTO's func as
   an AutoFailed, which a collocation worker's caller rethrows). No
   difference is taken instead. */
struct JacobianFailed {
  Error error;
};
/* its error's `where`, by which a run knows it (integrate.cpp offers to
   go on with differences) */
inline constexpr std::string_view JACOBIAN_FAILED = "exact jacobian";
/* what a run the failure stopped offers (W260, W275): the command that
   sets the Jacobian to difference, then `resume`, the protocol command
   that goes on from where the run stopped, both as JSON objects a
   recording replays */
ErrorAction switch_to_differences(std::string resume);

/* The model's exact Jacobian at (t, x) with the constants c (W260), its
   n = Model::node equations' d f/d x in `form`, from the compiled
   derivatives alone: nothing of the Session is written, so an AUTO
   collocation worker gives its own constants and gets the same bits as
   the caller's thread. f is the right-hand side there (for a map's form,
   f(x)-x): an entry not finite where f is throws JacobianFailed. The
   model must have one (Model::jacobian). */
void exact_jacobian(const Session &s, JacobianScratch &scratch, const double *c, double t, const double *x, const double *f, int n, const JacobianForm &form, double *out);
/* d f/d c[k] at (t, x) with the constants c into out[0..n), 0 where no
   equation depends on constant k; d f/dt for the time; checked as above */
void exact_constant_derivative(const Session &s, JacobianScratch &scratch, const double *c, int k, double t, const double *x, const double *f, int n, double *out);
void exact_time_derivative(const Session &s, JacobianScratch &scratch, const double *c, double t, const double *x, const double *f, int n, double *out);

} // namespace xpp
#endif
