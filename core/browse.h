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

float **get_browser_data(void);
float *get_data_col(int c);
void waitasec(int msec);
void data_get_mybrowser(int row);
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
void data_first(BROWSER *b);
void data_last(BROWSER *b);
void data_restore(BROWSER *b);

#ifdef __cplusplus
}

/* A file the user named, opened for a write that replaces it only at
   commit (xpp::Writer, binary: byte for byte), after asking whether to
   overwrite it when it exists: an empty Writer when the answer is no, or
   when it cannot be written (err_msg says so) */
xpp::Writer open_writer_asking(const char *fil, bool binary = false);

/* the stored rows First..Last as XPP's .dat (data_formats.h), only the
   columns of a batch run's "only" list (plotlist) when it has one: the
   batch run's output.dat and the files written beside it */
void write_mybrowser_data(xpp::Writer &w);

/* Save data (the browser's Write, docs/protocol.md): what is "table" (the
   rows First..Last of every column) or "plot" (what the current plot
   window shows: its curves and frozen curves as one table curve,x,y[,z]),
   format a data format's id (data_formats.h), name the file; whichever
   is empty is asked for (a menu, a menu of the formats, a file). */
void data_write(BROWSER *b, std::string_view what, std::string_view format, std::string_view name);
/* the browser's Load: name (asked for when empty) read as format (by its
   extension when empty, else .dat) into the stored columns, in order */
void data_read(BROWSER *b, std::string_view format, std::string_view name);

/* the data column of variable s: 0 for T, i+1 for variable i, a browser
   column added by data_add_col (by name, case ignored, below), -1 for
   none */
void find_variable(std::string_view s, int *col);

/* column j's name as the browser shows it: "T", a model variable's
   (xpp::model().uvar_names), or (j>xpp::model().neq) an added column's
   (BrowserState::added_columns below); "" past the last one */
std::string browse_column_name(int j);

/* col_index's rows 0..nrows-1: formula compiled and evaluated fresh over
   them (add_expr's constants roll back to xpp::model()'s own end right
   after, like a histogram condition -- the column is not a symbol a
   later formula can name), into xpp::session().data_store.col[col_index].
   data_add_col's own add, and a fresh run's recompute (refresh_browser)
   of every added column, both go through this; false (and an error) on
   a formula that no longer compiles */
bool compute_added_column(const std::string &formula, int col_index, int nrows);

#include <vector>
/* an Add column (browse_data.cpp data_add_col): its name and formula, as
   typed, kept to recompute it after every fresh run (below) */
struct AddedColumn {
  std::string name;
  std::string formula;
};
/* the data browser (browse_data.cpp), a Session's (session.h) */
struct BrowserState {
  /* the data set it shows: the stored data, or a derived set in its place */
  BROWSER view{};
  /* Replace's column as it was, for Unreplace: whether there is one,
     its column and its values */
  int replaced=0,replaced_col=0;
  std::vector<float> old_column;
  /* data_add_col's columns, in the order added, at data_store columns
     xpp::model().neq+1, +2, ...: the Session's own data, not the
     Model's (docs/roadmap.md W77 -- the Model stays as the load left
     it). A fresh load clears this with the rest of the Session; a fresh
     run recomputes every one of them over its own rows
     (recompute_added_columns, refresh_browser) instead of dropping
     them, since docs/manual/07-data-browser.md promises an added
     column stays computed "as though ... another auxiliary variable". */
  std::vector<AddedColumn> added_columns;
};
#endif
#endif

