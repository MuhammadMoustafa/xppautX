#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>
#include <string_view>
#include "expr_program.h"

#include "adjoint_state.h"
#include "storage.h"
#include "xpp_error.h"
namespace xpp { class BlockLu; }

namespace xpp {

struct AnalysisParts; /* analysis_parts.h */
struct RunReport; /* integrate.h */


/* v scaled to unit length (unless it is 0), and its length mu */
void norm_vec(std::span<double> v, double &mu);

void adj_back(AnalysisParts &s);
void h_back(AnalysisParts &s);

/* Transpose data (File/Transpose): the stored data's `columns` columns from `first_column` in steps of
   `column_skip`, and `rows` rows from row `first_row` in steps of `row_skip` (the first is 0), shown in the browser
   as rows of the table: each column becomes a row. Every one is in the data, else the error; the options are kept
   for the next one. */
Result<> transpose_data(AnalysisParts &s, std::string_view first_column, int columns, int column_skip, int first_row, int rows, int row_skip);
void alloc_h_stuff(AnalysisParts &s);
void data_back(AnalysisParts &s);
/* the adjoint of the stored orbit; the H function for the couplings of s.adjoint->coup_string, compiled first when
   `compile` (else the formulas of the last time are used again, as a range of adjoints does) */
Result<> new_adjoint(AnalysisParts &s);
Result<> new_h_fun(AnalysisParts &s, bool compile, const std::vector<std::string> *couplings = nullptr);
Result<> make_h(AnalysisParts &s, ColumnTable orb, ColumnTable adj, ColumnTable h, int nt);
/* the adjoint, the orbit and the H function in the data table, or why there is none */
Result<> load_adjoint(AnalysisParts &s);
Result<> load_interaction(AnalysisParts &s);
/* the adjoint's iterations and tolerance, or why they are not ones it takes */
Result<> set_adjoint_parameters(AnalysisParts &s, int max_iterates, double tolerance);
/* the maximal Liapunov exponent of the stored orbit or, with `over_range`, of the run at each value of the Range
   options' sweep (kept in the data table, the exponent given the largest one): the exponent, and the runs */
struct LyapunovResult {
  double exponent;
  int runs;
};
Result<LyapunovResult> lyapunov_exponent(AnalysisParts &s, bool over_range, const RunReport &report);
void do_this_liaprun(AnalysisParts &s, double p);

/* adjoints and the maximal Liapunov exponent return why they failed; the
   command (new_adjoint, do_liapunov) shows it */
Result<> adjoint(AnalysisParts &s, ColumnTable orbit, ColumnTable adjnt, int nt, double dt, double eps, double minerr, int maxit, int node);
Result<> step_eul(double **jac, int k, int k2, double *yold, double *work, int node, double dt, BlockLu &lu, std::span<const std::vector<int>> subsystems);
Result<double> hrw_liapunov(AnalysisParts &s, double eps);

} // namespace xpp
