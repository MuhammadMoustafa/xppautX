#pragma once

#include <stdio.h>
#include "xpp_io.h"
#include "data_formats.h"
#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace xpp {
struct Session; /* session.h */
struct Model; /* model.h */

typedef struct {
    int dataflag;
    int row0;
    int maxrow,maxcol;
    float **data;
    int istart,iend;
} BROWSER;

void waitasec(int msec);

void refresh_browser(Session &s, int length);
void reset_browser(Session &s);
void new_browse_dat(Session &s, float **new_dat, int dat_len);
/* a derived data set (a histogram, the adjoint, the H function) in the browser again with `show` when it exists, else
   `missing` as the error of `command` */
Result<> show_derived(Session &s, bool exists, const std::string &command, const std::string &missing, void (*show)(Session &));
float **get_browser_data(Session &s);

void data_get_mybrowser(Session &s, int row);

/* A file the user named, opened for a write that replaces it only at
   commit (Writer, binary: byte for byte), after asking whether to
   replace it if it exists. A command or native dialog's answer supplies
   permission; only an existing target without a decision asks in core.
   Empty when declined or when opening fails. With opened, the caller
   receives the error to show; otherwise it is shown here, once.
   independent resets the command decision for a derived destination. */
Writer open_writer_asking(std::string_view fil, bool binary = false,
                          Result<> *opened = nullptr, bool independent = false, const Place &at = {});
/* The same for a file the user already chose to replace, as the path a
   session was saved to: no question, the status checks and the temp-then-
   rename write of open_writer_asking. */
Writer open_writer_replacing(std::string_view fil, bool binary = false, Result<> *opened = nullptr);
/* One precondition, before asking for a name. Empty saves report once. */
bool save_ready(bool available);
Writer ask_output_writer(const Session &s, std::string_view title, std::string_view ext,
                         std::string_view what = {}, bool binary = false);
/* Finish a user save: report its destination and whether commit succeeded.
   The command shows the returned error once (ok_or_show for void commands). */
Result<> commit_save(Writer &writer);
/* A serializer or picture capture stopped before commit: discard the
   temporary file and report that this destination was not saved. */
void abort_save(Writer &writer);
/* The existing overwrite question, with Cancel (0). For an imported
   model, n opens the existing file instead of declining the write. */
int file_replace_choice(bool open_existing = false);

/* the stored rows First..Last as XPP's .dat (data_formats.h), only the
   columns of a batch run's "only" list (plotlist) when it has one: the
   batch run's output.dat and the files written beside it */
void write_mybrowser_data(Session &s, Writer &w);

/* what Save data writes: the "table" (the rows First..Last of every column), the model's "output" (those rows of
   its output columns: its "only" list when it has one, as --silent's output.dat) or what the plot shows
   ("plot_curves": its curves and frozen curves as one table curve,x,y[,z]); the names are the protocol's */
enum class DataTarget { table, output, plot_curves };
inline constexpr std::array<std::string_view, 3> data_target_names = {"table", "output", "plot_curves"};
/* Save data (the browser's Write, docs/protocol.md): target as above, format a data format's id (data_formats.h),
   name the file; whichever is missing is asked for (a menu, a menu of the formats, a file). The command's replace
   decision is used, or asked for independent of destination existence. The file and the rows written, 0 rows when
   the user cancelled or declined to replace the file; an error for nothing to save or a file that cannot be
   written. */
struct DataSaved {
  std::string file;
  std::size_t rows = 0;
};
Result<DataSaved> save_data_file(const Session &s, std::optional<DataTarget> target, std::string_view format,
                                 std::string_view name);
/* Save data for a key's menu: its error shown */
void data_write(const Session &s, std::optional<DataTarget> target, std::string_view format, std::string_view name);
/* What the current plot window shows written to the file `name` in data format `format` (an id of
   data_formats.h; the format of the file's extension when empty), the replace question asked as a save
   asks it: the rows written, 0 when the user declined to replace the file */
Result<std::size_t> write_plot_data(const Session &s, std::string_view format, std::string_view name);
/* the browser's Load: name (asked for when empty) read as format (by its
   extension when empty, else .dat) into s's stored columns, in order; the file and rows loaded (no file when the
   user cancelled the file's question), or why not */
struct DataLoaded {
  std::string file;
  std::size_t rows = 0;
};
Result<DataLoaded> load_data_file(Session &s, std::string_view format, std::string_view name);

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
/* the same as a value: the column of that name, -1 for none */
inline int variable_column(const Session &s, std::string_view name)
{
  int col = -1;
  find_variable(s, name, &col);
  return col;
}

/* column j's name as s's browser shows it: "T", a model variable's
   (Model::uvar_names), or (j>Model::neq) an added column's
   (BrowserState::added_columns below); "" past the last one */
std::string browse_column_name(const Session &s, int j);

/* col_index's rows 0..nrows-1: formula compiled and evaluated fresh over
   them (add_expr's constants roll back to the Model's own end right
   after, like a histogram condition -- the column is not a symbol a
   later formula can name), into s.data_store.col[col_index].
   data_add_col's own add, and a fresh run's recompute (refresh_browser)
   of every added column, both go through this; the error for
   a formula that no longer compiles */
Result<> compute_added_column(Session &s, const std::string &formula, int col_index, int nrows);
/* formula compiles as an added column's in s (Add column's check, a
   saved session's added columns' too); the parser's working symbols
   rolled back after, so s is left as it was */
bool added_column_compiles(Session &s, std::string_view formula);

/* an Add column (browse_data.cpp data_add_col): its name and formula, as
   typed, kept to recompute it after every fresh run (below) */
struct AddedColumn {
  std::string name;
  std::string formula;
};
/* the most rows and columns one block of the table holds (browser.page): a page of it, bounded so one event stays
   small; hello.limits tells the page */
inline constexpr int BROWSER_MAX_ROWS = 2000;
inline constexpr int BROWSER_MAX_COLS = 500;
/* the block of the table a client shows: its first row, rows (0: none, no block is sent), first column and
   columns after T, which is always the first value of a row (browser.page sets it, the `browser` event sends it) */
struct BrowserBlock {
  int from = 0, count = 0, col = 1, ncol = 1;
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
  BrowserBlock block;
};
/* block's first row and column brought into the data (the last row, the last column), as the data is now */
void fit_browser_block(Session &s);
/* the rows of the (fitted) block as JSON, [[T, v, ...], ...], 9 significant digits (they read back as exactly the
   stored floats), null for a number JSON cannot hold */
std::string browser_block_rows(const Session &s);

/* The browser's operations on the session s (docs/protocol.md "The data browser"): each returns its error as a
   value; the browser window's keys ask what they need (the data_* functions below) and call the same, as the
   registry's browser.* commands call them with their arguments. */
float *get_data_col(const Session &s, int c);
/* the browser shows the stored data, not a derived set (the adjoint, a histogram, ...) */
Result<> require_stored_data(const Session &s);
/* the added column `name` (a column of the stored data, computed from `formula`) deleted, or added */
Result<> delete_added_column(Session &s, std::string_view name);
Result<> add_data_column(Session &s, std::string_view name, std::string_view formula);
/* the column `column` replaced by formula: an expression of the columns, "&column" (its running integral),
   "@column" (its derivative) or a sequence "a:b" (from a to b) or "a;b" (from a, step b); undone by unreplace_column */
Result<> replace_data_column(Session &s, std::string_view column, std::string_view formula);
/* Replace's saved column dropped: nothing to Unreplace */
void wipe_rep(BrowserState &b);
Result<> unreplace_column(Session &s);
/* the row of `column`, from the selected row on, whose value is nearest `value`, made the selected one */
Result<int> find_in_column(Session &s, std::string_view column, double value);
/* the rows First..Last of `column` as a lookup table over xlo..xhi (the model's .tab format) written to
   `file`: the rows written, 0 when the user declined to replace the file */
Result<std::size_t> tabulate_column(Session &s, std::string_view column, double xlo, double xhi, std::string_view file);
/* First, Last (inclusive) and Restore: the row made the first or last one kept, and selected; the whole data
   kept again and replotted. The row, or the last row of the data */
Result<int> set_first_row(Session &s, int row);
Result<int> set_last_row(Session &s, int row);
Result<int> restore_whole_data(Session &s);
/* the browser at the start of a model: s's stored data, no rows */
void init_browser(Session &s);
void get_data_xyz(const Session &s, float *x, float *y, float *z, int i1, int i2, int i3, int off);
/* the stored row `row` made the initial conditions (and the values of the auxiliaries and the time the formulas read) */
void ic_of_row(Session &s, int row);

} // namespace xpp
