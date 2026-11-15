#pragma once
/* The delay equations' state a Session holds (delay_handle.cpp, del_stab.cpp): plain data, which the option table reads and writes
   (the stability search's grid and bounds) and the delay code works on (W744). */
#include <array>
#include <cstddef>
#include <vector>

namespace xpp {

/* stab_flag while no stability search is on: a delay is evaluated as the model says (the load's value) */
inline constexpr int DELAY_NORMAL = 1;
/* the delay equations' state (delay_handle.cpp, del_stab.cpp), a
   Session's (session.h): the model has delays (flag); the stability
   search's grid and bounds (grid, alpha_max, omega_max), whether the
   Jacobian's delayed terms are being evaluated (stab_flag) and for which
   delay (which), the delays found (ndelay of list) and the variables'
   shifts they read */
struct DelayState {
  int flag = 0, grid = 1000, stab_flag = 0, which = 0, ndelay = 0;
  double alpha_max = 2, omega_max = 2;
  std::array<std::vector<double>, 2> variable_shift; /* one per variable (do_delay_sing sizes them) */
  std::vector<double> list; /* the delays found: delay_stab_eval grows it */
  /* the stored history, NODE values per row, `rows` rows (a ring,
     `latest` its newest row) */
  std::vector<double> work;
  std::size_t latest = 0, rows = 0;
};

} // namespace xpp
