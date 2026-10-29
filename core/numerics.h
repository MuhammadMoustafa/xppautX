#pragma once

#include <optional>
#include <utility>
#include "numerics_settings.h"
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
/* the Poincare map: its kind, and for a kind other than none the variable (a column), the section, the crossing
   direction and whether the run stops on it, a setting left out as it is; or why the settings are not ones it takes */
Result<> set_poincare_map(Session &s, PoincareMap map, std::optional<int> variable, std::optional<double> section,
                          std::optional<SectionSign> direction, std::optional<bool> stop_on_section);
/* the Poincare map items of an imported button sequence's, asking for what analysis.poincare_map takes */
void get_pmap_pars_com(Session &s, int l);
/* the method picked from the Method menu (the one in use when none) */
int chosen_method(const Session &s);
void user_set_color_par(Session &s, int flag,const char *via,double lo,double hi);
/* what the active graph's orbit is colored by: nothing, its velocity or a column; the numbers are the graph's ColorFlag */
enum class ColorBy { none, velocity, variable };
/* the active graph colored by `by` (`variable`, a column, for ColorBy::variable) between the given limits, or between the
   least and the greatest the stored data reach; or why it cannot be */
Result<> set_color_coding(Session &s, ColorBy by, int variable, std::optional<std::pair<double, double>> limits);
/* the Color code items of an imported button sequence's, asking for what analysis.color_coding takes */
void set_col_par_com(Session &s, int i);
/* applies the checked numerics.method: its
   settings, then a fresh solver (xpp::start_solver) */
void do_meth(Session &s);

} // namespace xpp
