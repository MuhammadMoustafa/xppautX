#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>
#include <string_view>
#include "expr_program.h"

#include "storage.h"
#include "xpp_error.h"
namespace xpp { class BlockLu; }

namespace xpp {

struct Session; /* session.h */

/* adj2.cpp's derived data sets, a Session's (session.h): the adjoint, its
   H function and the transposed data, each its own columns with the
   stored ones lent beside them (storage.h's LentColumns), whether it
   exists and how many rows it has; shown in the browser in the stored
   data's place (adj_back, h_back, transpose_data). A load's new Session
   starts with none, so nothing of the Session before is shown again. */
struct AdjointState {
  LentColumns adjoint, h_function, transposed;
  int adjoint_rows = 0, h_rows = 0;
  bool adjoint_here = false, h_here = false, transposed_here = false;
  /* the H function has its odd and even parts too (more than two
     equations) */
  bool h_odd_even = false;
  /* the adjoint's Newton tolerance and iterations */
  double err = 1.e-3;
  int maxit = 20;
  /* each equation's coupling for the H function: its formula and the
     formula compiled */
  std::vector<Program> coup_fun;
  std::vector<std::string> coup_string;
  /* Transpose's settings: the first column and its name, how many and
     the skip; the first row, how many and the skip */
  struct {
    int col0 = 2, ncol = 2, colskip = 1;
    int row0 = 1, nrow = 1, rowskip = 1;
    std::string firstcol;
  } transpose;
  /* the maximal Liapunov exponent over a range: the parameter, the
     exponent (a point per run, while liap_flag) */
  std::array<std::vector<double>, 2> liap;
  int liap_flag = 0;
};

/* v scaled to unit length (unless it is 0), and its length mu */
void norm_vec(std::span<double> v, double &mu);

void adj_back(Session &s);
void h_back(Session &s);

/* Transpose data (File/Transpose): the stored data's `columns` columns from `first_column` in steps of
   `column_skip`, and `rows` rows from row `first_row` in steps of `row_skip` (the first is 0), shown in the browser
   as rows of the table: each column becomes a row. Every one is in the data, else the error; the settings are kept
   for the next one. */
Result<> transpose_data(Session &s, std::string_view first_column, int columns, int column_skip, int first_row, int rows, int row_skip);
void alloc_h_stuff(Session &s);
void data_back(Session &s);
/* the adjoint of the stored orbit; the H function for the couplings of s.adjoint->coup_string, compiled first when
   `compile` (else the formulas of the last time are used again, as a range of adjoints does) */
Result<> new_adjoint(Session &s);
Result<> new_h_fun(Session &s, bool compile, const std::vector<std::string> *couplings = nullptr);
Result<> make_h(Session &s, ColumnTable orb, ColumnTable adj, ColumnTable h, int nt);
/* the adjoint, the orbit and the H function in the data table, or why there is none */
Result<> load_adjoint(Session &s);
Result<> load_interaction(Session &s);
/* the adjoint's iterations and tolerance, or why they are not ones it takes */
Result<> set_adjoint_parameters(Session &s, int max_iterates, double tolerance);
/* the maximal Liapunov exponent of the stored orbit or, with `over_range`, of the run at each value of the Range
   settings' sweep (kept in the data table, the exponent given the largest one): the exponent, and the runs */
struct LyapunovResult {
  double exponent;
  int runs;
};
Result<LyapunovResult> lyapunov_exponent(Session &s, bool over_range);
void do_this_liaprun(Session &s, double p);

/* adjoints and the maximal Liapunov exponent return why they failed; the
   command (new_adjoint, do_liapunov) shows it */
Result<> adjoint(Session &s, ColumnTable orbit, ColumnTable adjnt, int nt, double dt, double eps, double minerr, int maxit, int node);
Result<> step_eul(double **jac, int k, int k2, double *yold, double *work, int node, double dt, BlockLu &lu, std::span<const std::vector<int>> subsystems);
Result<double> hrw_liapunov(Session &s, double eps);

} // namespace xpp
