#pragma once
/* The integrator's options the option table's rows set (Range, Sing pts' Range): plain data, which a Session's IntegratorState
   (integrator_state.h, the integrators') extends with the run's state (W757). */
#include <string>

namespace xpp {

/* how many values a range varies (RangeVars::rtype): one, two together or two independently */
inline constexpr int RANGE_SINGLE = 0, RANGE_TOGETHER = 1, RANGE_INDEPENDENT = 2;
/* Initialconds/Range's options (integrate.cpp's, set by the options): the one range over item (and item2 for the double range) */
struct RangeVars {
  std::string item, item2;
  bool write_files = false; // Opt in: reset runs otherwise keep no separate files.
  int steps = 20, steps2 = 10; /* the option's default; a double range's second steps, 10 as XPPAUT's dialog fell back to */
  bool reset, oldic; /* each run starts from the initial data / from the same initial data */
  int index, index2, cycle = 0, type, type2, movie;
  double plow, phigh, plow2 = 0, phigh2 = 1;
  int rtype;
};

/* Sing pts' Range options */
struct EquilibriumRange {
  std::string item;
  int steps = 10, shoot = 0, col = -1, movie = 0, mc = 0; /* Steps starts at 10, what the dialog's empty value fell back to */
  double plow = 0, phigh = 1;
};

/* what the option table sets of the integrator, a base of IntegratorState */
struct IntegratorSettings {
  /* Initialconds/Range's options */
  RangeVars range{};
  EquilibriumRange eq_range;
};

} // namespace xpp
