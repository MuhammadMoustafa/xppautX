#pragma once

#include <optional>
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* the numerics settings that have no key of the main table (the method's own: the Volterra points, the CVODE
   band, the delay's stability search); named as the .odex options name them (vmaxpts, autoeval, bandlo, bandup) */
struct NumericsExtras {
  std::optional<int> max_points, band_lower, band_upper, delay_grid;
  std::optional<bool> auto_evaluate, banded;
  std::optional<double> delay_real_guess, delay_imaginary_guess;
};
/* the extras set all or nothing, each checked against the method and the model it applies to; or why not */
Result<> set_numerics_extras(Session &s, const NumericsExtras &extras);

void quick_num(Session &s, int com);

/* what a new delta_t needs: the delays' and the integrals' memory again */
void dt_changed(Session &s);
void set_total(Session &s, double total);
void get_num_par(Session &s, char ch);
void chk_delay(Session &s);
void set_delay(Session &s);
void ruelle(Session &s);
/* the active plot window's rUelle shifts (how many rows each axis lags): each 0 or more; redrawn */
Result<> set_axis_shifts(Session &s, int x, int y, int z);
void compute_one_period(Session &s, double period,double *x, const char *name);
void get_pmap_pars_com(Session &s, int l);
/* the method picked from the Method menu (the one in use when none) */
int chosen_method(const Session &s);
void user_set_color_par(Session &s, int flag,const char *via,double lo,double hi);
void set_col_par_com(Session &s, int i);
/* applies the checked numerics.method: its
   settings, then a fresh solver (xpp::start_solver) */
void do_meth(Session &s);

} // namespace xpp
