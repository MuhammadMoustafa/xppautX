#ifndef AUTO_STABILITY_AUTO_H
#define AUTO_STABILITY_AUTO_H
/* The stability owner's numerical seam. Keep AUTO's legacy scalar types
   out of auto_stability.h, which every Session (including CVODE) includes. */
#include "auto_c.h"
#include "auto_stability.h"

namespace xpp {

/* No retained record for a restored equilibrium: refresh its Jacobian and
   reuse AUTO's eigenvalue calculation, without changing point parameters. */
void auto_stability_equilibrium_restored(iap_type &iap, rap_type &rap, double *par, integer *icp,
    FUNI_TYPE((*funi)), double *u, double *uold, double *f, double *dfdu, double *dfdp);

/* The full generalized spectrum of a restored solution's circuit pencil;
   no assumption of a trivial +1 multiplier (the orbit can have collapsed). */
void auto_stability_multipliers_restored(AutoStability &st, const iap_type &iap, double *p0, double *p1);

} // namespace xpp
#endif
