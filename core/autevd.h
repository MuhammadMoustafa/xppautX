#ifndef _autevd_h_
#define _autevd_h_

#include "autlim.h"
#include "auto_f2c.h"
#include "auto_c.h"
#include "xAuto.h"

struct AutoState; /* auto_state.h */

namespace xpp {

/* autevd.cpp */

int get_bif_type(int ibr, int ntot, int lab);
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int labw, double *a, double *uhigh, double *ulow, double *u0, double *ubar);

/* the AUTO run's own parameters (st.run) from its settings (st.bifur,
   the active view's parameters, the Mark values), for ndim variables */
void init_auto(AutoState &st, int ndim);

} // namespace xpp
#endif
