#ifndef _histogram_h_
#define _histogram_h_
#ifdef __cplusplus
#include <string>
#include "xpp_error.h"
extern "C" {
#endif


int spectrum(float *data, int nr, int win, int w_type, float *pow);
int cross_spectrum(float *data, float *data2, int nr, int win, int w_type, float *pow, int type);
void mycor2(float *x, float *y, int n, int nbins, float *z, int flag);
void fftxcorr(float *data1, float *data2, int length, int nlag, float *cr, int flag);
void fft(float *data, float *ct, float *st, int nmodes, int length);


#ifdef __cplusplus
}

#include "storage.h"

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
  int post_process=0;
};

namespace xpp {
struct Session; /* session.h */
}

void column_mean(xpp::Session &s);
void compute_correl(xpp::Session &s);
void compute_fourier(xpp::Session &s);
void compute_hist(xpp::Session &s);
void compute_power(xpp::Session &s);
void compute_sd(xpp::Session &s);
void compute_stacor(xpp::Session &s);
int new_2d_hist(xpp::Session &s);

/* a histogram or correlation of the session s's stored data; the error of
   a condition that did not compile (it was ignored, the rest is done), for
   the command to show */
xpp::Result<> new_hist(xpp::Session &s, int nbins, double zlo, double zhi, int col, int col2, const char *condition,
                       int which);
/* the other results of s's stored data: a 2D histogram, the Fourier
   modes, a spectrum; shown in the browser (four_back, hist_back) */
int two_d_hist(xpp::Session &s, int col1, int col2, int ndat, int n1, int n2, double xlo, double xhi, double ylo,
               double yhi);
int twod_hist(xpp::Session &s);
void four_back(xpp::Session &s);
void hist_back(xpp::Session &s);
void new_four(xpp::Session &s, int nmodes, int col);
void just_fourier(xpp::Session &s, int flag);
void just_sd(xpp::Session &s, int flag);
/* a batch run's post-processing (@ post_process=) of s's data */
void post_process_stuff(xpp::Session &s);
/* asks for a column of s's model (prompt); 0 when it names none */
int get_col_info(const xpp::Session &s, int *col, const char *prompt);
#endif
#endif

