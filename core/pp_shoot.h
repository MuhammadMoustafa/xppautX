#ifndef _pp_shoot_h_
#define _pp_shoot_h_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

void dump_shoot_range(FILE *fp, int f);
void bad_shoot(int iret);

#ifdef __cplusplus
}

#include <string_view>

#include "xpp_error.h"


namespace xpp {
struct Session; /* session.h */
}

/* Range shoot's settings asked for: 0 when cancelled */
int set_up_sh_range(xpp::Session &s);

void do_bc(xpp::Session &s, double *y__0, double t0, double *y__1, double t1, double *f, int n);
void compile_bvp(xpp::Session &s);
void reset_bvp(xpp::Session &s);
void do_sh_range(xpp::Session &s, double *ystart, double *yend);
int set_up_periodic(xpp::Session &s, int *ipar, int *ivar, double *sect, int *ishow);
void find_bvp_com(xpp::Session &s, int com);

/* the solution the shooting found, integrated and stored (flag), or the
   error the integration failed with */
xpp::Result<> last_shot(xpp::Session &s, int flag);
/* shoots for the boundary conditions from y (iret: how it ended, for
   bad_shoot); the error the curve drawn as it runs (ishow) failed
   with, once the shooting has ended */
xpp::Result<> bvshoot(xpp::Session &s, double *y, double *yend, double err, double eps, int maxit, int *iret, int n, int ishow, int iper, int ipar, int ivar, double sect);

/* Range shoot's settings, over parameter s */
void init_shoot_range(std::string_view s);
#endif
#endif
