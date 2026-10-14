#ifndef _autevd_h_
#define _autevd_h_

#include "auto_f2c.h"
#include "auto_c.h"
#include "xAuto.h"
#include "auto_codes.h"

struct AutoState; /* auto_state.h */

namespace xpp {

/* autevd.cpp */

/* One failure message for a refused AUTO solution, rendered by do_auto. */
[[noreturn]] void auto_nonfinite(const iap_type &iap, integer point);

DiagramPointType get_bif_type(int ibr, int ntot, int lab);
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int labw, double *a, double *uhigh, double *ulow, double *u0, double *ubar);

/* the AUTO run's own parameters (st.run) from its settings (st.bifur,
   the active view's parameters, the Mark values), for ndim variables;
   exact: func gives the model's exact Jacobian (JAC=1, W260) */
void init_auto(AutoState &st, int ndim, bool exact);

} // namespace xpp
#endif
