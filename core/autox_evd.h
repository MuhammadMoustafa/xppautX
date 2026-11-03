#pragma once

#include "autox_f2c.h"
#include "autox_c.h"
#include "xAutox.h"
#include "autox_codes.h"

struct AutoxState; /* autox_state.h */

namespace xpp {

/* autox_evd.cpp */

/* One failure message for a refused autoX solution, rendered by do_autox. */
[[noreturn]] void autox_nonfinite(const iap_type &iap, integer point);

/* Start checks run before storing a point or taking a continuation step. */
void autox_check_start_parameters(const iap_type &iap, const rap_type &rap, const double *par, const integer *icp);
void autox_check_start_norm(const iap_type &iap, const rap_type &rap, double norm);
void autox_check_start_equilibrium(const iap_type &iap, const rap_type &rap, double *u, const double *par, const integer *icp);

/* a point's type from autoX's signs: a negative branch is periodic, a
   negative point stable (a zero sign: a stable steady state) */
DiagramPointType get_bif_type(int ibr, int ntot);
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int labw, double *a, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> u0, std::span<const double> ubar);

/* the autoX run's own parameters (st.run) from its settings (st.bifur,
   the active view's parameters, the Mark values), for ndim variables;
   exact: func gives the model's exact Jacobian (JAC=1, W260) */
void init_autox(AutoxState &st, int ndim, bool exact);

} // namespace xpp
