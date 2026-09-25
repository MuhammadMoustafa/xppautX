#ifndef _browse_h_
#define _browse_h_

#define BMAXCOL 20
#include "xpp_types.h"

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
		XppWinId base,upper;
		XppWinId find,up,down,pgup,pgdn,home,end,left,right;
		XppWinId first,last,restore,write,get,close;
		XppWinId load,repl,unrepl,table,addcol,delcol;
                XppWinId main;
                XppWinId label[BMAXCOL];
                XppWinId time;
                XppWinId hint;
		char hinttxt[256];
		int dataflag,xflag;
		int col0,row0,ncol,nrow;
		int maxrow,maxcol;
                float **data;
		int istart,iend;
                } BROWSER;

/*extern BROWSER my_browser;
*/

float **get_browser_data(void);
void set_browser_data(float **data, int col0);
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
void redraw_browser(BROWSER b);
void new_browse_dat(float **new_dat, int dat_len);
void refresh_browser(int length);
void reset_browser(void);
void draw_data(BROWSER b);
void init_browser(void);
void kill_browser(BROWSER *b);
void make_new_browser(void);
void make_browser(BROWSER *b, const char *wname, const char *iname, int row, int col);
void data_up(BROWSER *b);
void data_down(BROWSER *b);
void data_pgup(BROWSER *b);
void data_pgdn(BROWSER *b);
void data_home(BROWSER *b);
void data_end(BROWSER *b);
void get_data_xyz(float *x, float *y, float *z, int i1, int i2, int i3, int off);
void data_get(BROWSER *b);
void data_replace(BROWSER *b);
void data_unreplace(BROWSER *b);
void data_table(BROWSER *b);
void data_find(BROWSER *b);
void open_write_file(FILE **fp, const char *fil, int *ok);
/* open_write_file's question alone, for a writer (xpp_io.h) that opens
   the file itself: 1 when fil does not exist yet or may be overwritten */
int may_write_file(const char *fil);
void data_read(BROWSER *b);
void data_write(BROWSER *b);
void data_left(BROWSER *b);
void data_right(BROWSER *b);
void data_first(BROWSER *b);
void data_last(BROWSER *b);
void data_restore(BROWSER *b);

#ifdef __cplusplus
}
#endif
#endif











