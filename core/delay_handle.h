
#ifndef _delay_handle_h_
#define _delay_handle_h_
#ifdef __cplusplus
extern "C" {
#endif

void polint(double *xa, double *ya, int n, double x, double *y, double *dy);

#ifdef __cplusplus
}

#include <array>
#include "xpp_error.h"
#include "xpplim.h"


namespace xpp {
struct Session; /* session.h */
}

double delay_stab_eval(xpp::Session &s, double delay, int var);
int alloc_delay(xpp::Session &s, double big);
void free_delay(xpp::Session &s);
void stor_delay(xpp::Session &s, double *y);
double get_delay(xpp::Session &s, int in, double tau);

/* the delay's initial data, from t0-big to t0, stored from the Delay ICs'
   formulas, or the error when one does not parse */
xpp::Result<> do_init_delay(xpp::Session &s, double big);
/* the delay equations' state (delay_handle.cpp, del_stab.cpp), a
   Session's (session.h): the model has delays (flag); the stability
   search's grid and bounds (grid, alpha_max, omega_max), whether the
   Jacobian's delayed terms are being evaluated (stab_flag) and for which
   delay (which), the delays found (ndelay of list) and the variables'
   shifts they read */
struct DelayState {
  int flag = 0, grid = 1000, stab_flag = 0, which = 0, ndelay = 0;
  double alpha_max = 2, omega_max = 2;
  std::array<std::array<double, MAXODE>, 2> variable_shift{};
  std::array<double, MAXDELAY> list{};
};
#endif
#endif
