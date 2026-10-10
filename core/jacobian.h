#ifndef _jacobian_h_
#define _jacobian_h_

namespace xpp {

struct Session; /* session.h */

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

/* The model's Jacobian at (t, x), by forward differences: the one owner
   of the step rule (W255). `f0` is f(t, x) (for a map, already f(x)-x),
   `dfdt`, if given, receives df/dt. This is where an exact Jacobian
   plugs in (W260). */
void jacobian(Session &s, double t, const double *x, const double *f0, int n, double eps, const JacobianForm &form, double *out, double *dfdt = nullptr);

} // namespace xpp
#endif
