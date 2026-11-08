#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "storage.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* the spectral analysis of stored columns (two columns of the same
   length) */
/* Each takes columns of n values and writes into columns of the same precision, doing its arithmetic at it (a float
   column's in floats, as storing always did). */
int spectrum(Column data, int n, int win, int w_type, Column pow);
int cross_spectrum(Column data, Column data2, int n, int win, int w_type, Column pow, int type);
void fourier_modes(Column data, int n, Column ct, Column st, int nmodes);

/* the histogram / spectrum settings the dialogs and the model's own
   @ options edit (load_eqn.cpp's option reader; cond, the histogram's
   condition, only the Histogram dialog) */
struct HIST_INFO {
  int nbins,nbins2,col,col2,fftc;
  double xlo,xhi;
  double ylo,yhi;
  std::string cond;
};

/* the spectra's words, by spec_type's number: the power spectral density of one column, the cross spectrum of two, their coherence */
inline constexpr std::array<std::string_view, 3> spectrum_type_names{"spectral_density", "cross_spectral_density", "coherence"};

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
   browser and the tests),
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

/* The analyses of the stored data (histogram.cpp), each the one core function the registry's analysis.*
   commands and the Stochastic menu's items call; the settings they work from are s.histogram's (the commands
   set the ones they are given, the menu asks for them with the ask_* functions, which show their own refusals
   and say false when cancelled). Each returns what it made, or why it could not. */

/* what a histogram counted: its bins and the samples that fell in them */
struct HistogramCounts {
  int bins;
  long long samples;
};
Result<HistogramCounts> compute_hist(Session &s, const HIST_INFO *given = nullptr);
bool ask_histogram(Session &s);
/* the two-dimensional histogram of info's two columns */
Result<> new_2d_hist(Session &s, const HIST_INFO *given = nullptr);
bool ask_histogram_2d(Session &s);
/* the histogram of the differences of a column (autocorrelation): the bins computed */
Result<int> compute_stacor(Session &s, const HIST_INFO *given = nullptr);
bool ask_autocorrelation(Session &s);
/* the correlation of info's two columns at info.nbins lags, directly or by FFT (info.fftc 0 or 1): the lags computed */
Result<int> compute_correl(Session &s, const HIST_INFO *given = nullptr);
bool ask_cross_correlation(Session &s);
/* the spectral density of spec_col (and spec_col2) of spec_type, spec_wid points a window of type spec_win: the points computed */
Result<int> compute_sd(Session &s, std::optional<int> type = {}, std::optional<int> column = {}, std::optional<int> column2 = {}, std::optional<int> length = {}, std::optional<int> window = {});
bool ask_spectral_density(Session &s);
/* the Fourier modes of spec_col, and the power spectrum made from them: the harmonics or frequencies computed */
Result<int> compute_fourier(Session &s, std::optional<int> column = {});
Result<int> compute_power(Session &s, std::optional<int> column = {});
/* the mean and variance of info.col */
struct ColumnStatistics {
  double mean, variance;
};
Result<ColumnStatistics> column_mean(Session &s, std::optional<int> column = {});
/* asks for a column of s's model (prompt), col its default; false (the refusal shown) when it names none */
bool ask_column(const Session &s, int &col, const char *prompt);

/* a histogram or correlation of the session s's stored data (which: 0 a histogram of the rows where the condition
   holds, 1 of the differences, 2 and 3 the correlation of two columns, directly and by the FFT); a condition that
   is not a formula is the error, and nothing is made */
Result<> new_hist(Session &s, int nbins, double zlo, double zhi, int col, int col2, const char *condition,
                  int which);
/* the other results of s's stored data: a 2D histogram, the Fourier
   modes, a spectrum; shown in the browser (four_back, hist_back) */
int two_d_hist(Session &s, int col1, int col2, int ndat, int n1, int n2, double xlo, double xhi, double ylo,
               double yhi);
int twod_hist(Session &s);
void four_back(Session &s);
void hist_back(Session &s);
/* the last histogram shown in the browser again, or why there is none */
Result<> reload_histogram(Session &s);
void new_four(Session &s, int nmodes, int col);
Result<int> just_fourier(Session &s, int flag, std::optional<int> column = {});
Result<int> just_sd(Session &s, int flag);
/* a batch run's post-processing (@ post_process=) of s's data */
void post_process_stuff(Session &s);

} // namespace xpp
