#include "autox_c.h"
#include "session.h"
#include "xpp_math.h"
#include <cmath>
#include <utility>

namespace {
/* This structure contains all of the input data for the condense_parameters routine
   Those values which are arrays and those
   which are input and output are markered as such*/
struct conpar_parallel_arglist {
  std::ptrdiff_t *nov, *nra, *nca; /*scalars input*/
  double *a; /*array input and output size: nca X nra X na */
  std::ptrdiff_t *ncb; /*scalar input */
  double *b; /*array input and output size: ncb X nra X na*/
  std::ptrdiff_t *nbc, *nrc; /*scalar input */
  double *c; /*array input and output size: nca X nrc X *na*/
  double *d; /*array input and output size: ncb X nrc*/
  std::ptrdiff_t *irf; /*array input size: na X nra*/
  std::ptrdiff_t *icf; /*array input: na X nca*/
  std::vector<std::vector<std::pair<std::ptrdiff_t,double>>> *dlog=nullptr;
  AutoxGlobalScratch *scratch;
  std::ptrdiff_t loop_start; /*scalar input*/
  std::ptrdiff_t loop_end; /*scalar output*/
};

/* The first column of [from, to) holding the largest |value| (strict comparison: a tie keeps
   the earlier one, a NaN never wins); `from` when every value is zero. */
std::ptrdiff_t first_maximum(const double *row, std::ptrdiff_t from, std::ptrdiff_t to)
{
  std::ptrdiff_t at = from;
  double largest = 0.;
  for (std::ptrdiff_t l = from; l < to; ++l) {
    const double value = std::fabs(row[l]);
    if (largest < value) [[unlikely]] { largest = value; at = l; }
  }
  return at;
}

/* The process function: the original autoX called it per worker, on a
   shared-memory machine or wrapped for message passing. */
void conpar_process(const conpar_parallel_arglist *arg)
{
  const std::ptrdiff_t rows = *arg->nra, cols = *arg->nca;
  const std::ptrdiff_t parameters = *arg->ncb, constraints = *arg->nrc;
  const std::ptrdiff_t left = *arg->nov, end = cols - left;
  // Per-worker scratch, reused for every interval in this worker's range.
  auto &maximum_column = arg->scratch->maximum_column;
  auto &swaps = arg->scratch->conpar_swaps;
  maximum_column.resize(rows);
  swaps.resize(end - left);
  for (std::ptrdiff_t interval = arg->loop_start; interval < arg->loop_end; ++interval) {
    xpp::Matrix<double> a({arg->a + interval * rows * cols,
                          static_cast<size_t>(rows * cols)}, cols, rows);
    xpp::Matrix<double> b({arg->b + interval * rows * parameters,
                          static_cast<size_t>(rows * parameters)}, parameters, rows);
    xpp::Matrix<double> c({arg->c + interval * constraints * cols,
                          static_cast<size_t>(constraints * cols)}, cols, constraints);
    std::span<std::ptrdiff_t> irf(arg->irf + interval * rows, rows);
    std::span<std::ptrdiff_t> icf(arg->icf + interval * cols, cols);
    // Swapping whole rows/columns also carries the already stored multipliers.
    const auto swap_rows = [&](std::ptrdiff_t first, std::ptrdiff_t second) {
      if (first == second) return;
      for (std::ptrdiff_t col = 0; col < cols; ++col) std::swap(a(col, first), a(col, second));
      for (std::ptrdiff_t col = 0; col < parameters; ++col) std::swap(b(col, first), b(col, second));
    };
    const auto swap_columns = [&](std::ptrdiff_t first, std::ptrdiff_t second) {
      if (first == second) return;
      for (std::ptrdiff_t row = 0; row < rows; ++row) std::swap(a(first, row), a(second, row));
      for (std::ptrdiff_t row = 0; row < constraints; ++row) std::swap(c(first, row), c(second, row));
    };
    // maximum_column[row]: the first largest |value| of the row's columns [col, end), kept up to date
    // below: a row whose multiplier is zero is only rescanned when a swap touched its maximum.
    for (std::ptrdiff_t row = 0; row < rows; ++row) maximum_column[row] = first_maximum(&a(0, row), left, end);
    for (std::ptrdiff_t col = left; col < end; ++col) {
      const std::ptrdiff_t pivot_row = col - left;
      std::ptrdiff_t selected_row = pivot_row, selected_column = col;
      double largest = 0.;
      // Strict comparisons preserve the original first maximum, including ties.
      for (std::ptrdiff_t row = pivot_row; row < rows; ++row) {
        const double value = std::fabs(a(maximum_column[row], row));
        if (largest < value) {
          largest = value;
          selected_row = row;
          selected_column = maximum_column[row];
        }
      }
      swaps[pivot_row] = {selected_row, selected_column};
      swap_rows(pivot_row, selected_row);
      std::swap(maximum_column[pivot_row], maximum_column[selected_row]);
      swap_columns(col, selected_column);
      std::swap(irf[pivot_row], irf[selected_row]);
      std::swap(icf[col], icf[selected_column]);
      for (std::ptrdiff_t row = pivot_row + 1; row < rows; ++row) {
        const double rm = a(col, row) / a(col, pivot_row);
        a(col, row) = rm;
        // Shape follows AUTO-07p's IMSBRA (solvebv.f90): the zero test sits outside the element loops, same bits.
        double *target = &a(0, row);
        const double *source = &a(0, pivot_row);
        if (rm != 0.) {
          xpp::subtract_scaled(target, source, rm, left);
          xpp::subtract_scaled(target + col + 1, source + col + 1, rm, cols - col - 1);
          xpp::subtract_scaled(&b(0, row), &b(0, pivot_row), rm, parameters);
          maximum_column[row] = first_maximum(target, col + 1, end);
        } else if (maximum_column[row] == col || maximum_column[row] == selected_column) {
          maximum_column[row] = first_maximum(target, col + 1, end);
        }
      }
      for (std::ptrdiff_t row = *arg->nbc; row < constraints; ++row) {
        const double rm = c(col, row) / a(col, pivot_row);
        c(col, row) = rm;
        if (rm != 0.) {
          xpp::subtract_scaled(&c(0, row), &a(0, pivot_row), rm, left);
          xpp::subtract_scaled(&c(col + 1, row), &a(col + 1, pivot_row), rm, cols - col - 1);
          for (std::ptrdiff_t l = 0; l < parameters; ++l) {
            const std::ptrdiff_t at = l + row * parameters;
            // W247 replays subtractions in serial interval/step/row/column order.
            if (arg->dlog) (*arg->dlog)[interval].emplace_back(at, rm * b(l, pivot_row));
            else arg->d[at] -= rm * b(l, pivot_row);
          }
        }
      }
    }
    // Later stages address the original storage through IRF/ICF. Undo only
    // physical moves, leaving those permutation records and every value intact.
    for (std::ptrdiff_t step = end - left; step > 0; --step) {
      const std::ptrdiff_t row = step - 1;
      swap_columns(left + row, swaps[row].second);
      swap_rows(row, swaps[row].first);
    }
  }
}

void
conpar_default_wrapper(AutoxRun &run, std::ptrdiff_t *nov, std::ptrdiff_t *na, std::ptrdiff_t *nra, std::ptrdiff_t *nca, double *a, std::ptrdiff_t *ncb, double *b, std::ptrdiff_t *nbc, std::ptrdiff_t *nrc, double *c, double *d, std::ptrdiff_t *irf, std::ptrdiff_t *icf)

{
    conpar_parallel_arglist data;
    data.scratch = &run.scratch;
    data.nov = nov;
    data.nra = nra;
    data.nca = nca;
    data.a = a;
    data.ncb = ncb;
    data.b = b;
    data.nbc = nbc;
    data.nrc = nrc;
    data.c = c;
    data.d = d;
    data.irf = irf;
    data.icf = icf;
    data.loop_start = 0;
    data.loop_end = *na;
    if (!run.collocation || run.collocation->threads()==1) {
      conpar_process(&data);
      return;
    }
    auto &dlog = run.solve_boundary_work.dlog;
    dlog.resize(*na);
    const auto terms = xpp::size_product(*nra - *nov, *nrc - *nbc, *ncb);
    for (auto &interval : dlog) {
      interval.clear();
      interval.reserve(terms);
    }
    data.dlog=&dlog;
    run.collocation->intervals(*na,[&](long begin,long end,AutoxRun &worker) {
      conpar_parallel_arglist mine=data;
      mine.scratch=&worker.scratch;
      mine.loop_start=begin;
      mine.loop_end=end;
      conpar_process(&mine);
    });
    // Replay every subtraction in interval/elimination/row/column order.
    for (const auto &interval:dlog)
      for (const auto &[at,term]:interval) d[at]-=term;
}
} // namespace

/* conpar: XPPAUT's AUTO's CONPAR (solvebv.f90:777): condensation of the system by eliminating the local variables */
int 
condense_parameters(AutoxRun &run, std::ptrdiff_t *nov, std::ptrdiff_t *na, std::ptrdiff_t *nra, std::ptrdiff_t *nca, double *a, std::ptrdiff_t *ncb, double *b, std::ptrdiff_t *nbc, std::ptrdiff_t *nrc, double *c, double *d, std::ptrdiff_t *irf, std::ptrdiff_t *icf)
{
  /* Aliases for the dimensions of the arrays */
  std::ptrdiff_t icf_dim1, irf_dim1;
  
  std::ptrdiff_t i,j;
  std::ptrdiff_t nex;

  /*<	      NEX=NCA-2*NOV >*/
  irf_dim1 = *nra;
  icf_dim1 = *nca;

  /* Function Body */
  nex = *nca - (*nov << 1);
  if (nex == 0) {
    return 0;
  }
  
  /*     Initialization */
  for (i = 0; i <*na; ++i) {
    for (j = 0; j < *nra; ++j) {
      irf[j + i * irf_dim1] = j+1;
    }
    for (j = 0; j < *nca; ++j) {
      icf[j + i * icf_dim1] = j+1;
    }
  }

  /* The same condensation loop runs serially or over disjoint intervals. */
  conpar_default_wrapper(run,nov, na, nra, nca, a,
			  ncb, b, nbc, nrc, c, d,irf, icf);
  return 0;
} 
