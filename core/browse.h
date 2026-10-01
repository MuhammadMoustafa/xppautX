#ifndef _browse_h_
#define _browse_h_

#include <stdio.h>
#include "xpp_io.h"
#include "data_formats.h"
#include <vector>

namespace xpp {
struct Session; /* session.h */

typedef struct {
    int dataflag;
    int row0;
    int maxrow,maxcol;
    float **data;
    int istart,iend;
} BROWSER;

void waitasec(int msec);
void chk_seq(std::string_view f, int *seq, double *a1, double *a2);
void make_d_table(double xlo, double xhi, int col, std::string_view filename, BROWSER b);
void find_value(int col, double val, int *row, BROWSER b);
void data_first(BROWSER *b);
void data_last(BROWSER *b);

void refresh_browser(Session &s, int length);
void reset_browser(Session &s);
void new_browse_dat(Session &s, float **new_dat, int dat_len);
float **get_browser_data(Session &s);

void data_get_mybrowser(Session &s, int row);

void data_restore(Session &s, BROWSER *b);

/* A file the user named, opened for a write that replaces it only at
   commit (Writer, binary: byte for byte), after asking whether to
   overwrite it when it exists: an empty Writer when the answer is no, or
   when it cannot be written (err_msg says so) */
Writer open_writer_asking(std::string_view fil, bool binary = false);

/* the stored rows First..Last as XPP's .dat (data_formats.h), only the
   columns of a batch run's "only" list (plotlist) when it has one: the
   batch run's output.dat and the files written beside it */
void write_mybrowser_data(Session &s, Writer &w);

/* Save data (the browser's Write, docs/protocol.md): what is "table" (the
   rows First..Last of every column), "output" (those rows of the model's
   output columns: its "only" list when it has one, as -silent's
   output.dat) or "plot" (what the current plot window shows: its curves
   and frozen curves as one table curve,x,y[,z]), format a data format's
   id (data_formats.h), name the file; whichever is empty is asked for (a
   menu, a menu of the formats, a file). An existing file is replaced
   without asking when replace is set, else after asking. */
void data_write(const Session &s, BROWSER *b, std::string_view what, std::string_view format,
                std::string_view name, bool replace = false);
/* the browser's Load: name (asked for when empty) read as format (by its
   extension when empty, else .dat) into s's stored columns, in order */
void data_read(Session &s, BROWSER *b, std::string_view format, std::string_view name);

/* the data table as a session file saves it (xpp_session.cpp, W57):
   every stored row of T and the model's variables and auxiliaries, and
   the seed of the run that made them */
DataTable stored_data_table(const Session &s);
/* table's columns into the stored ones of the same names (find_variable
   below; a name the model does not have is left out), the store grown to
   hold every row: its rows, the new data set; 0 when there was no memory
   for them */
int put_stored_data(Session &s, const DataTable &table);

/* the data column of variable s: 0 for T, i+1 for variable i, a browser
   column added by data_add_col (by name, case ignored, below), -1 for
   none */
void find_variable(const Session &s, std::string_view name, int *col);

/* column j's name as s's browser shows it: "T", a model variable's
   (Model::uvar_names), or (j>Model::neq) an added column's
   (BrowserState::added_columns below); "" past the last one */
std::string browse_column_name(const Session &s, int j);

/* col_index's rows 0..nrows-1: formula compiled and evaluated fresh over
   them (add_expr's constants roll back to the Model's own end right
   after, like a histogram condition -- the column is not a symbol a
   later formula can name), into s.data_store.col[col_index].
   data_add_col's own add, and a fresh run's recompute (refresh_browser)
   of every added column, both go through this; false (and an error) on
   a formula that no longer compiles */
bool compute_added_column(Session &s, const std::string &formula, int col_index, int nrows);

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
     Model::neq+1, +2, ...: the Session's own data, not the
     Model's (docs/roadmap.md W77 -- the Model stays as the load left
     it). A fresh load clears this with the rest of the Session; a fresh
     run recomputes every one of them over its own rows
     (recompute_added_columns, refresh_browser) instead of dropping
     them, since docs/manual/07-data-browser.md promises an added
     column stays computed "as though ... another auxiliary variable". */
  std::vector<AddedColumn> added_columns;
};

/* The browser's commands and what they reach, on the session s whose
   browser view b is (docs/protocol.md "The data browser") */
float *get_data_col(const Session &s, int c);
int check_for_stor(const Session &s, float **data);
void data_del_col(const Session &s, BROWSER *b);
void data_add_col(Session &s, BROWSER *b);
int add_stor_col(Session &s, std::string_view name, const std::string &formula, BROWSER *b);
void replace_column(Session &s, const char *var, char *form, float **dat, int n);
/* Replace's saved column dropped: nothing to Unreplace */
void wipe_rep(BrowserState &b);
void unreplace_column(Session &s);
/* the browser at the start of a model: s's stored data, no rows */
void init_browser(Session &s);
void get_data_xyz(const Session &s, float *x, float *y, float *z, int i1, int i2, int i3, int off);
void data_get(Session &s, BROWSER *b);
void data_replace(Session &s, BROWSER *b);
void data_unreplace(Session &s);
void data_table(const Session &s, BROWSER *b);
void data_find(const Session &s, BROWSER *b);

} // namespace xpp
#endif
