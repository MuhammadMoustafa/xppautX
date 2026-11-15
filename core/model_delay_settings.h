#pragma once
/* The delay equations' settings the option table's rows set (the stability search's grid and bounds): plain data, which a Session's
   DelayState (delay_state.h, the integrators') extends with the delays' state (W757). */

namespace xpp {

/* the stability search's grid and bounds, a base of DelayState */
struct DelaySettings {
  int grid = 1000;
  double alpha_max = 2, omega_max = 2;
};

} // namespace xpp
