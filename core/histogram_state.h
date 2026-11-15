#pragma once
/* The histogram and the spectrum's settings and results a Session holds (histogram.cpp): plain data, which the option table reads and
   writes and the analyses work on (W744). */
#include <array>
#include <string>
#include <string_view>
#include "model_analysis.h"
#include "storage.h"

namespace xpp {

/* the histogram / spectrum settings the dialogs and the model's own
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

/* histogram.cpp's settings and results, a Session's (session.h): the
   histogram and spectrum settings, the last histogram and Fourier
   transform's columns (storage.h's LentColumns: read back by the data
   data table and the tests),
   whether one exists and its length, and the spectrum's settings (the
   model's own @ options set spec_*) and a batch run's post-processing */
struct HistogramState {
  HIST_INFO info{100,100,1,1,0,0,1,0,1,""};
  LentColumns hist_columns, four_columns;
  ColumnTable hist() noexcept { return hist_columns.table(); }
  ColumnTable four() noexcept { return four_columns.table(); }
  int hist_here=0, four_here=0, hist_len=0, four_len=0;
  int spec_col=1, spec_wid=512, spec_win=2, spec_col2=1;
  /* the spectrum computed: 0 the power spectral density, 1 the cross
     spectrum, 2 the coherence */
  int spec_type=0;
  AfterRun post_process=AfterRun::none;
};

} // namespace xpp
