#pragma once
/* The stability owner's numerical seam. Keep autoX's legacy scalar types
   out of autox_stability.h, which every Session (including CVODE) includes. */
#include "autox_c.h"
#include "xpp_math.h"
#include <optional>
#include <cstddef>
#include <span>
namespace xpp { struct AutoxStability; }

namespace xpp {

/* Complex pair nearest the imaginary axis. Conservative spectra treat the
   AUTO-07p near-axis band as tied; equal real parts within eigensolver
   roundoff also choose the lowest frequency. Remaining
   ties prefer the negative real part and positive imaginary representative. */
double autox_hopf_axis_tolerance(Matrix<const double> jacobian, size_t dimension);
std::optional<xpp::ComplexPair> autox_hopf_pair(std::span<const xpp::ComplexPair> spectrum,
                                          double axis_tolerance);

/* No retained record for a restored equilibrium: refresh its Jacobian and
   reuse autoX's eigenvalue calculation, without changing point parameters. */
void autox_stability_equilibrium_restored(iap_type &iap, rap_type &rap, double *par, std::ptrdiff_t *icp,
    ProblemFunction problem_function, double *u, double *uold, double *f, double *dfdu, double *dfdp);

/* Original orbit pencil extracted from an augmented cycle-curve system.
   Uses the existing partial-pivot LU to choose independent equation rows. */
void autox_cycle_multipliers(const iap_type &iap, const double *p0, const double *p1, xpp::ComplexPair *ev, bool deflate);

/* The full generalized spectrum of a restored solution's circuit pencil;
   no assumption of a trivial +1 multiplier (the orbit can have collapsed). */
void autox_stability_multipliers_restored(AutoxStability &st, const iap_type &iap, double *p0, double *p1);

} // namespace xpp
