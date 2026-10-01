
#ifndef _delay_handle_h_
#define _delay_handle_h_

#include <array>
#include <span>
#include <vector>
#include "xpp_error.h"
#include "xpplim.h"

namespace xpp {

struct Session; /* session.h */

/* Neville's interpolation through the points (xa[i], ya[i]) (at most 10,
   ya as long as xa) at x: the value y and its error estimate dy */
void polint(std::span<const double> xa, std::span<const double> ya, double x, double &y, double &dy);

double delay_stab_eval(Session &s, double delay, int var);
int alloc_delay(Session &s, double big);
void free_delay(Session &s);
void stor_delay(Session &s, double *y);
double get_delay(Session &s, int in, double tau);

/* the delay's initial data, from t0-big to t0, stored from the Delay ICs'
   formulas, or the error when one does not parse */
Result<> do_init_delay(Session &s, double big);
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
  /* the stored history, NODE values per row, `rows` rows (a ring,
     `latest` its newest row) */
  std::vector<double> work;
  int latest = 0, rows = 0;
};

} // namespace xpp
#endif
