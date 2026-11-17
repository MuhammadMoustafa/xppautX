#pragma once
/* The histogram and spectrum options the option table's rows and the dialogs set, and what a batch run makes of its data: plain
   data, which a Session's HistogramState (histogram_state.h, the analyses') extends with the results (W757). */
#include <array>
#include <string>
#include <string_view>
#include "model_analysis.h"

namespace xpp {

/* the histogram / spectrum options the dialogs and the model's own
   @ options edit (load_eqn.cpp's option reader; cond, the histogram's
   condition, only the Histogram dialog) */
struct HIST_INFO {
  int nbins,nbins2,col,col2,fftc;
  double xlo,xhi;
  double ylo,yhi;
  std::string cond;
};

/* what a batch run (--silent, a model's `output: { after_run }`) makes of its data once it has run: an analysis of the
   stored run, or the mean or the variance of the runs of a Compute (stochastic) sweep, which then are the data */
enum class AfterRun { none, histogram, histogram_2d, fourier_series, power_spectrum, spectral_density, cross_spectral_density, coherence, mean_of_runs, variance_of_runs };
inline constexpr std::array<std::string_view, 10> after_run_names{"none", "histogram", "histogram_2d", "fourier_series", "power_spectrum",
    spectrum_type_names[0], spectrum_type_names[1], spectrum_type_names[2], "mean_of_runs", "variance_of_runs"};
/* the spectrum type (spec_type) an after_run analysis computes */
inline constexpr int spectrum_type_of(AfterRun a) { return static_cast<int>(a) - static_cast<int>(AfterRun::spectral_density); }

/* the histogram and spectrum options (the model's own @ options set spec_*) and a batch run's post-processing, a base of
   HistogramState */
struct HistogramSettings {
  HIST_INFO info{100,100,1,1,0,0,1,0,1,""};
  int spec_col=1, spec_wid=512, spec_win=2, spec_col2=1;
  /* the spectrum computed: 0 the power spectral density, 1 the cross
     spectrum, 2 the coherence */
  int spec_type=0;
  AfterRun post_process=AfterRun::none;
};

} // namespace xpp
