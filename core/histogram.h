#ifndef _histogram_h_
#define _histogram_h_
#ifdef __cplusplus
#include <string>
extern "C" {
#endif


/* histogram.c */
int two_d_hist(int col1, int col2, int ndat, int n1, int n2, double xlo, double xhi, double ylo, double yhi);
void four_back(void);
void hist_back(void);
void new_four(int nmodes, int col);
int new_2d_hist(void);
int twod_hist(void);
void new_hist(int nbins, double zlo, double zhi, int col, int col2, const char *condition, int which);
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

/* histogram.cpp's results, read back by the data browser and the tests:
   the last histogram and Fourier transform's columns, whether one exists
   and its length, and the spectrum's settings (the model's own
   @ options set spec_*) */
extern float **my_hist;
extern float **my_four;
extern int HIST_HERE, FOUR_HERE, hist_len, four_len;
extern int spec_col, spec_wid, spec_win, spec_col2, spec_type;
extern int post_process;

#ifdef __cplusplus
}

/* the histogram / spectrum settings the dialogs and the model's own
   @ options edit (load_eqn.cpp's option reader; cond, the histogram's
   condition, only the Histogram dialog) */
struct HIST_INFO {
  int nbins,nbins2,col,col2,fftc;
  double xlo,xhi;
  double ylo,yhi;
  std::string cond;
};

extern HIST_INFO hist_inf;
#endif
#endif

