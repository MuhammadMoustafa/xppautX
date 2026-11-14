#pragma once

#include <array>
#include "xpp_error.h"
#include "model_analysis.h"

#include <string>

namespace xpp {

struct Session; /* session.h */
struct RunReport; /* integrate.h */

void do_bc(Session &s, double *y__0, double t0, double *y__1, double t1, double *f, int n);
/* the boundary conditions compiled, or the one that does not */
Result<> compile_bvp(Session &s);
/* Range shoot over *s.shoot_range (the settings made): a solution for each parameter value, the values at its
   `side`, stored as rows; the solutions found (rows stored), or the error that stopped the sweep (shown by the
   command). ystart and yend hold the model's states, room for neq each. */
Result<int> do_sh_range(Session &s, double *ystart, double *yend, const RunReport &report);
/* the boundary-value commands (docs/protocol.md "boundary_value"), the one core each front end calls: solve from the
   initial data (show: every Newton step drawn, else only the solution) and make the solution the initial
   data; solve with the period as an unknown (the parameter `ipar`, 0-based, the variable `ivar` held at
   `sect` as the section), the period found; Range shoot over s.shoot_range. All store the solution's orbit. */
Result<> bvp_solve(Session &s, bool show, const RunReport &report);
Result<double> bvp_solve_periodic(Session &s, int ipar, int ivar, double sect, bool show, const RunReport &report);
Result<int> bvp_solve_range(Session &s, const ShootRange *given, const RunReport &report);

/* the solution the shooting found, integrated and stored (flag), or the
   error the integration failed with */
Result<> last_shot(Session &s, int flag);
/* how a shooting that found its solution ended: the residual of the boundary conditions fell below
   bvp_tolerance, or the Newton update did (the solution is as good as it will get), or it was
   stopped (Escape, `/`) */
enum class ShootEnd { residual_met, update_met, cancelled };
struct Shot {
  ShootEnd end = ShootEnd::cancelled;
  /* the error the curve drawn as it runs (ishow) failed with, which only ended the curve */
  Result<> curve;
};
/* shoots for the boundary conditions from y, the solution into y and the far end into yend; why
   there is none (the integration failed, the iterations ran out, the Jacobian is singular) is the
   error, with the iterate it happened at */
Result<Shot> bvshoot(Session &s, double *y, double *yend, double err, double eps, int maxit, int n, int ishow, int iper, int ipar, int ivar, double sect);

/* Range shoot's settings, over parameter s */

} // namespace xpp
