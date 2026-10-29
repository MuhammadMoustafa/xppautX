#pragma once
/* the phase space's Torus (torus.cpp), C++ only */
#include "xpp_error.h"
#include <optional>
#include <vector>
namespace xpp {
struct Session; /* session.h */

/* which variables the phase space folds into a torus of one period: all, none, or a chosen few */
enum class TorusMode { all, none, choose };
/* the torus: with `all`, or `choose` and the variables (their indices; those it had when none are given), folded at
   the period (the one it had when none is given), which must be above 0; `none` unfolds them all */
Result<> set_torus(Session &s, TorusMode mode, std::optional<double> period, std::optional<std::vector<int>> chosen);
/* the Phase space menu's items (an imported button sequence's), asking for what analysis.phase_space_torus takes */
void do_torus_com(Session &s, int c);
}
