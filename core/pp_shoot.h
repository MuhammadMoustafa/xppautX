#pragma once

#include <cstdio>
#include <string_view>
#include "xpp_error.h"

#include <string>

namespace xpp {

/* BVP's Range settings (pp_shoot.cpp) */
struct ShootRange {
  std::string item;
  int steps = 0, side = 0, cycle = 0, movie = 0;
  double plow = 0, phigh = 0;
};

struct Session; /* session.h */

/* Range shoot's settings asked for: 0 when cancelled */
int set_up_sh_range(Session &s);

void do_bc(Session &s, double *y__0, double t0, double *y__1, double t1, double *f, int n);
void compile_bvp(Session &s);
void reset_bvp(Session &s);
void do_sh_range(Session &s, double *ystart, double *yend);
int set_up_periodic(Session &s, int *ipar, int *ivar, double *sect, int *ishow);
void find_bvp_com(Session &s, int com);

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
void init_shoot_range(Session &s, std::string_view item);

} // namespace xpp
