#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include "storage.h"
#include "xpp_error.h"
#include "histogram_state.h"

namespace xpp {

struct AnalysisParts; /* analysis_parts.h */

/* the spectral analysis of stored columns (two columns of the same
   length) */
/* Each takes columns of n values and writes into columns of the same precision, doing its arithmetic at it (a float
   column's in floats, as storing always did). */
int spectrum(Column data, int n, int win, int w_type, Column pow);
int cross_spectrum(Column data, Column data2, int n, int win, int w_type, Column pow, int type);
void fourier_modes(Column data, int n, Column ct, Column st, int nmodes);



/* The analyses of the stored data (histogram.cpp), each the one core function the registry's analysis commands
   commands and the Stochastic menu's items call; the settings they work from are *s.histogram's (the commands
   set the ones they are given, the menu asks for them with the ask_* functions, which show their own refusals
   and say false when cancelled). Each returns what it made, or why it could not. */

/* what a histogram counted: its bins and the samples that fell in them */
struct HistogramCounts {
  int bins;
  long long samples;
};
Result<HistogramCounts> compute_hist(AnalysisParts &s, const HIST_INFO *given = nullptr);
/* the two-dimensional histogram of info's two columns */
Result<> new_2d_hist(AnalysisParts &s, const HIST_INFO *given = nullptr);
/* the histogram of the differences of a column (autocorrelation): the bins computed */
Result<int> compute_stacor(AnalysisParts &s, const HIST_INFO *given = nullptr);
/* the correlation of info's two columns at info.nbins lags, directly or by FFT (info.fftc 0 or 1): the lags computed */
Result<int> compute_correl(AnalysisParts &s, const HIST_INFO *given = nullptr);
/* the spectral density of spec_col (and spec_col2) of spec_type, spec_wid points a window of type spec_win: the points computed */
Result<int> compute_sd(AnalysisParts &s, std::optional<int> type = {}, std::optional<int> column = {}, std::optional<int> column2 = {}, std::optional<int> length = {}, std::optional<int> window = {});
/* the Fourier modes of spec_col, and the power spectrum made from them: the harmonics or frequencies computed */
Result<int> compute_fourier(AnalysisParts &s, std::optional<int> column = {});
Result<int> compute_power(AnalysisParts &s, std::optional<int> column = {});
/* the mean and variance of info.col */
struct ColumnStatistics {
  double mean, variance;
};
Result<ColumnStatistics> column_mean(AnalysisParts &s, std::optional<int> column = {});

/* a histogram or correlation of the session s's stored data (which: 0 a histogram of the rows where the condition
   holds, 1 of the differences, 2 and 3 the correlation of two columns, directly and by the FFT); a condition that
   is not a formula is the error, and nothing is made */
Result<> new_hist(AnalysisParts &s, int nbins, double zlo, double zhi, int col, int col2, const char *condition,
                  int which);
/* the other results of s's stored data: a 2D histogram, the Fourier
   modes, a spectrum; shown in the browser (four_back, hist_back) */
int two_d_hist(AnalysisParts &s, int col1, int col2, int ndat, int n1, int n2, double xlo, double xhi, double ylo,
               double yhi);
int twod_hist(AnalysisParts &s);
void four_back(AnalysisParts &s);
void hist_back(AnalysisParts &s);
/* the last histogram shown in the browser again, or why there is none */
Result<> reload_histogram(AnalysisParts &s);
void new_four(AnalysisParts &s, int nmodes, int col);
Result<int> just_fourier(AnalysisParts &s, int flag, std::optional<int> column = {});
Result<int> just_sd(AnalysisParts &s, int flag);
/* a batch run's post-processing (@ post_process=) of s's data */
void post_process_stuff(AnalysisParts &s);

} // namespace xpp
