#ifndef _browse_h_
#define _browse_h_


#include <stdio.h>
#ifdef __cplusplus
#include "xpp_io.h"
extern "C" {
#endif

typedef struct {
    int dataflag;
    int row0;
    int maxrow,maxcol;
    float **data;
    int istart,iend;
} BROWSER;

/* browse_data.cpp: the data browser's view of the stored data */
extern BROWSER my_browser;

float **get_browser_data(void);
float *get_data_col(int c);
void waitasec(int msec);
int get_maxrow_browser(void);
void write_mybrowser_data(FILE *fp);
void data_get_mybrowser(int row);
void write_browser_data(FILE *fp, BROWSER *b);
int check_for_stor(float **data);
void data_del_col(BROWSER *b);
void data_add_col(BROWSER *b);
int add_stor_col(const char *name, const char *formula, BROWSER *b);
void chk_seq(const char *f, int *seq, double *a1, double *a2);
void replace_column(const char *var, char *form, float **dat, int n);
void wipe_rep(void);
void unreplace_column(void);
void make_d_table(double xlo, double xhi, int col, const char *filename, BROWSER b);
void find_value(int col, double val, int *row, BROWSER b);
void find_variable(const char *s, int *col);
void new_browse_dat(float **new_dat, int dat_len);
void refresh_browser(int length);
void reset_browser(void);
void init_browser(void);
void get_data_xyz(float *x, float *y, float *z, int i1, int i2, int i3, int off);
void data_get(BROWSER *b);
void data_replace(BROWSER *b);
void data_unreplace(BROWSER *b);
void data_table(BROWSER *b);
void data_find(BROWSER *b);
void data_read(BROWSER *b);
void data_write(BROWSER *b);
void data_first(BROWSER *b);
void data_last(BROWSER *b);
void data_restore(BROWSER *b);

#ifdef __cplusplus
}

/* A file the user named, opened for a write that replaces it only at
   commit (xpp::Writer), after asking whether to overwrite it when it
   exists: an empty Writer when the answer is no, or when it cannot be
   written (err_msg says so) */
xpp::Writer open_writer_asking(const char *fil);
#endif
#endif

