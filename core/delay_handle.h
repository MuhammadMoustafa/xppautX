
#pragma once

#include <array>
#include <span>
#include <vector>
#include <cstddef>
#include "xpp_error.h"
#include "delay_state.h"

namespace xpp {

struct RunParts; /* run_parts.h */

/* a delay's value is interpolated by the cubic through four stored points */
inline constexpr std::size_t polint_points = 4;
/* Neville's interpolation through the points (xa[i], ya[i]) at x: the
   value y and its error estimate dy */
void polint(std::span<const double, polint_points> xa, std::span<const double, polint_points> ya, double x, double &y, double &dy);

double delay_stab_eval(RunParts &s, double delay, int var);
Result<> alloc_delay(RunParts &s, double big);
void free_delay(RunParts &s);
void stor_delay(RunParts &s, double *y);
double get_delay(RunParts &s, int in, double tau);
/* what a program's delay(tau, x) and delay(tau, x, shift) give (ExprHost::delay and delay_shift): variable x as the
   program holds it (its symbol index, as a double) delayed by tau, or the variable itself while the stability
   search evaluates the Jacobian's delayed terms */
double do_delay(RunParts &s, double delay, double i);
double do_delay_shift(RunParts &s, double delay, double shift, double variable);

/* the delay's initial data, from t0-big to t0, stored from the Delay ICs'
   formulas, or the error when one does not parse */
Result<> do_init_delay(RunParts &s, double big);

} // namespace xpp
