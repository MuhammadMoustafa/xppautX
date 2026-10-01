#ifndef _histogram_h_
#define _histogram_h_

#include <span>
#include <string>
#include "storage.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* the spectral analysis of stored columns (two columns of the same
   length) */
int spectrum(std::span<const float> data, int win, int w_type, float *pow);
int cross_spectrum(std::span<const float> data, std::span<const float> data2, int win, int w_type, float *pow, int type);
void mycor2(std::span<const float> x, std::span<const float> y, int nbins, float *z, int flag);
void fftxcorr(std::span<const float> data1, std::span<const float> data2, int nlag, float *cr, int flag);
void fourier_modes(std::span<const float> data, float *ct, float *st, int nmodes);

/* the histogram / spectrum settings the dialogs and the model's own
   @ options edit (load_eqn.cpp's option reader; cond, the histogram's
   condition, only the Histogram dialog) */
struct HIST_INFO {
  int nbins,nbins2,col,col2,fftc;
  double xlo,xhi;
  double ylo,yhi;
  std::string cond;
};

/* histogram.cpp's settings and results, a Session's (session.h): the
   histogram and spectrum settings, the last histogram and Fourier
   transform's columns (storage.h's LentColumns: read back by the data
   browser and the tests),
   whether one exists and its length, and the spectrum's settings (the
   model's own @ options set spec_*) and a batch run's post-processing */
struct HistogramState {
  HIST_INFO info{100,100,1,1,0,0,1,0,1,""};
  LentColumns hist_columns, four_columns;
  float **hist() noexcept { return hist_columns.table(); }
  float **four() noexcept { return four_columns.table(); }
  int hist_here=0, four_here=0, hist_len=0, four_len=0;
  int spec_col=1, spec_wid=512, spec_win=2, spec_col2=1;
  /* the spectrum computed: 0 the power spectral density, 1 the cross
     spectrum, 2 the coherence */
  int spec_type=0;
  int post_process=0;
};

void column_mean(Session &s);
void compute_correl(Session &s);
void compute_fourier(Session &s);
void compute_hist(Session &s);
void compute_power(Session &s);
void compute_sd(Session &s);
void compute_stacor(Session &s);
int new_2d_hist(Session &s);

/* a histogram or correlation of the session s's stored data; the error of
   a condition that did not compile (it was ignored, the rest is done), for
   the command to show */
Result<> new_hist(Session &s, int nbins, double zlo, double zhi, int col, int col2, const char *condition,
                  int which);
/* the other results of s's stored data: a 2D histogram, the Fourier
   modes, a spectrum; shown in the browser (four_back, hist_back) */
int two_d_hist(Session &s, int col1, int col2, int ndat, int n1, int n2, double xlo, double xhi, double ylo,
               double yhi);
int twod_hist(Session &s);
void four_back(Session &s);
void hist_back(Session &s);
void new_four(Session &s, int nmodes, int col);
void just_fourier(Session &s, int flag);
void just_sd(Session &s, int flag);
/* a batch run's post-processing (@ post_process=) of s's data */
void post_process_stuff(Session &s);
/* asks for a column of s's model (prompt); 0 when it names none */
int get_col_info(const Session &s, int *col, const char *prompt);

} // namespace xpp
#endif
