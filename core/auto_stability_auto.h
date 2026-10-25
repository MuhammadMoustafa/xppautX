#pragma once
/* The stability owner's numerical seam. Keep AUTO's legacy scalar types
   out of auto_stability.h, which every Session (including CVODE) includes. */
#include "auto_c.h"
#include "xpp_math.h"
#include <optional>
#include "auto_stability.h"

namespace xpp {

/* Complex pair nearest the imaginary axis. Conservative spectra treat the
   AUTO-07p near-axis band as tied; equal real parts within eigensolver
   roundoff also choose the lowest frequency. Remaining
   ties prefer the negative real part and positive imaginary representative. */
double auto_hopf_axis_tolerance(Matrix<const double> jacobian, size_t dimension);
std::optional<doublecomplex> auto_hopf_pair(std::span<const doublecomplex> spectrum,
                                          double axis_tolerance);

/* No retained record for a restored equilibrium: refresh its Jacobian and
   reuse AUTO's eigenvalue calculation, without changing point parameters. */
void auto_stability_equilibrium_restored(iap_type &iap, rap_type &rap, double *par, integer *icp,
    FUNI_TYPE((*funi)), double *u, double *uold, double *f, double *dfdu, double *dfdp);

/* Original orbit pencil extracted from an augmented cycle-curve system.
   Uses the existing partial-pivot LU to choose independent equation rows. */
void auto_cycle_multipliers(const iap_type &iap, const double *p0, const double *p1, doublecomplex *ev, bool deflate);

/* The full generalized spectrum of a restored solution's circuit pencil;
   no assumption of a trivial +1 multiplier (the orbit can have collapsed). */
void auto_stability_multipliers_restored(AutoStability &st, const iap_type &iap, double *p0, double *p1);

} // namespace xpp
