#pragma once

#include "auto_f2c.h"
#include "auto_c.h"
#include "xAuto.h"
#include "auto_codes.h"

struct AutoState; /* auto_state.h */

namespace xpp {

/* autevd.cpp */

/* One failure message for a refused AUTO solution, rendered by do_auto. */
[[noreturn]] void auto_nonfinite(const iap_type &iap, integer point);

/* Start checks run before storing a point or taking a continuation step. */
void auto_check_start_parameters(const iap_type &iap, const rap_type &rap, const double *par, const integer *icp);
void auto_check_start_norm(const iap_type &iap, const rap_type &rap, double norm);
void auto_check_start_equilibrium(const iap_type &iap, const rap_type &rap, double *u, const double *par, const integer *icp);

/* a point's type from AUTO's signs: a negative branch is periodic, a
   negative point stable (a zero sign: a stable steady state) */
DiagramPointType get_bif_type(int ibr, int ntot);
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int labw, double *a, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> u0, std::span<const double> ubar);

/* the AUTO run's own parameters (st.run) from its settings (st.bifur,
   the active view's parameters, the Mark values), for ndim variables;
   exact: func gives the model's exact Jacobian (JAC=1, W260) */
void init_auto(AutoState &st, int ndim, bool exact);

} // namespace xpp
