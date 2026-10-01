#ifndef _pp_shoot_h_
#define _pp_shoot_h_

#include <cstdio>
#include <string_view>
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

void dump_shoot_range(FILE *fp, int f);
void bad_shoot(int iret);

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
/* shoots for the boundary conditions from y (iret: how it ended, for
   bad_shoot); the error the curve drawn as it runs (ishow) failed
   with, once the shooting has ended */
Result<> bvshoot(Session &s, double *y, double *yend, double err, double eps, int maxit, int *iret, int n, int ishow, int iper, int ipar, int ivar, double sect);

/* Range shoot's settings, over parameter s */
void init_shoot_range(std::string_view s);

} // namespace xpp
#endif
