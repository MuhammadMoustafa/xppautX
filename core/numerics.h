#pragma once

#include <optional>
#include <utility>
#include "xpp_error.h"
namespace xpp { enum class PoincareMap; enum class SectionSign; }

namespace xpp {

struct RunParts; /* run_parts.h */
struct RunReport; /* integrate.h */

/* the numerics options that have no key of the main table (the method's own: the Volterra points, the CVODE
   band, the delay's stability search); named as the .odex options name them (vmaxpts, bandlo, bandup) */
struct NumericsExtras {
  std::optional<int> max_points, band_lower, band_upper, delay_grid;
  std::optional<bool> banded;
  std::optional<double> delay_real_guess, delay_imaginary_guess;
};
/* the extras set all or nothing, each checked against the method and the model it applies to; or why not */
/* Check extras against the method the same command proposes, without applying it. */
Result<> check_numerics_extras(const RunParts &s, const NumericsExtras &extras, int method);
Result<> set_numerics_extras(RunParts &s, const NumericsExtras &extras);


/* what a new delta_t needs: the delays' and the integrals' memory again */
void dt_changed(RunParts &s);
void set_total(RunParts &s, double total);
void chk_delay(RunParts &s);
void set_delay(RunParts &s);
/* the active plot window's rUelle shifts (how many rows each axis lags): each 0 or more; redrawn */
Result<> set_axis_shifts(RunParts &s, int x, int y, int z);
/* the Poincare map: its kind, and for a kind other than none the variable (a column), the section, the crossing
   direction and whether the run stops on it, an option left out as it is; or why the options are not ones it takes */
Result<> set_poincare_map(RunParts &s, PoincareMap map, std::optional<int> variable, std::optional<double> section,
                          std::optional<SectionSign> direction, std::optional<bool> stop_on_section);
/* applies the checked numerics.method: its
   options, then a fresh solver (xpp::start_solver) */
void do_meth(RunParts &s);

} // namespace xpp
