
#pragma once

#include "xpp_error.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct TABULAR; /* expr_tables.h */

struct SessionParts; /* model_options.h */

/* Bound both allocation and formula evaluation from hostile table counts to 8 MB per session. */
inline constexpr int table_points_limit = 1000000;

/* Tables return why they failed; the command (or the model load) shows it. */

/* the Session s's function tables again, after a parameter changed: the
   first failure, the others still done */
Result<> redo_all_fun_tables(const SessionParts &s);
/* table index's formula worked out at n points of [xlo,xhi] into y (its
   error at the table's line); formula is a
   std::string (a table's own) and not a string_view: autoX redoes the
   tables at every right-hand side (redo_all_fun_tables), and a
   string_view there cost two instructions a call (W109c) */
Result<> eval_fun_table(const SessionParts &s, int index, int n, double xlo, double xhi, const std::string &formula, double *y);
/* @ autoeval= for each table: tables, a Session's (View, Numerics' tables
   menu (i: 0 view, 1 edit) and which table it picks, -1 for none) */
void set_autox_eval_flags(std::vector<TABULAR> &tables, int f);
/* the model's table of that name, or why there is none */
Result<int> find_table(const SessionParts &s, std::string_view name);
/* what a table's edit changes, a setting left out as the table has it: a function table's points, range and
   formula, whether it is evaluated again when a parameter changes, or the file a file table is read from again */
struct TableEdit {
  std::optional<int> points;
  std::optional<double> x_min, x_max;
  std::optional<std::string> formula, file;
  std::optional<bool> auto_evaluate;
};
/* table index edited: its points, now evaluated or read, or why it could not be */
Result<int> edit_table(const SessionParts &s, int index, const TableEdit &edit);
/* table index read from the file named file (its printable characters), one of the model's own files
   (model_files.h): why it could not be, else nothing */
Result<> add_file_table(const SessionParts &s, int index, std::string_view file);
Result<> create_fun_table(const SessionParts &s, int npts, double xlo, double xhi, std::string_view formula, int index);
/* table index read from the file filename (quoted or not): one of the
   model's own files (model_file, model_files.h: a file table of the
   model) or a file the user picked (Numerics' table file) */
Result<> load_table(const SessionParts &s, std::string_view filename, int index, int model_file);

} // namespace xpp
