#ifndef _histogram_h_
#define _histogram_h_
#ifdef __cplusplus
#include <string>
#include "xpp_error.h"
extern "C" {
#endif


/* histogram.c */
int two_d_hist(int col1, int col2, int ndat, int n1, int n2, double xlo, double xhi, double ylo, double yhi);
void four_back(void);
void hist_back(void);
void new_four(int nmodes, int col);
int new_2d_hist(void);
int twod_hist(void);
void column_mean(void);
int get_col_info(int *col, const char *prompt);
void compute_power(void);
int spectrum(float *data, int nr, int win, int w_type, float *pow);
int cross_spectrum(float *data, float *data2, int nr, int win, int w_type, float *pow, int type);
void compute_sd(void);
void compute_fourier(void);
void compute_correl(void);
void compute_stacor(void);
void mycor2(float *x, float *y, int n, int nbins, float *z, int flag);
void compute_hist(void);
void fftxcorr(float *data1, float *data2, int length, int nlag, float *cr, int flag);
void fft(float *data, float *ct, float *st, int nmodes, int length);
void post_process_stuff();
void just_fourier(int flag);
void just_sd(int flag);


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
/* a histogram or correlation of the stored data; the error of a condition
   that did not compile (it was ignored, the rest is done), for the command
   to show */
xpp::Result<> new_hist(int nbins, double zlo, double zhi, int col, int col2, const char *condition, int which);

struct HistogramState {
  HIST_INFO info{100,100,1,1,0,0,1,0,1,""};
  LentColumns hist_columns, four_columns;
  float **hist() noexcept { return hist_columns.table(); }
  float **four() noexcept { return four_columns.table(); }
  int hist_here=0, four_here=0, hist_len=0, four_len=0;
  int spec_col=1, spec_wid=512, spec_win=2, spec_col2=1;
  int post_process=0;
};
#endif
#endif

