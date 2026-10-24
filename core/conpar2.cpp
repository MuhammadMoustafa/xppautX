#include "auto_c.h"
#include "session.h"
#include "xpp_math.h"
#include <utility>

namespace {
/* This structure contains all of the input data for the conpar routine
   Those values which are arrays and those
   which are input and output are markered as such*/
struct conpar_parallel_arglist {
  integer *nov, *nra, *nca; /*scalars input*/
  doublereal *a; /*array input and output size: nca X nra X na */
  integer *ncb; /*scalar input */
  doublereal *b; /*array input and output size: ncb X nra X na*/
  integer *nbc, *nrc; /*scalar input */
  doublereal *c; /*array input and output size: nca X nrc X *na*/
  doublereal *d; /*array input and output size: ncb X nrc*/
  integer *irf; /*array input size: na X nra*/
  integer *icf; /*array input: na X nca*/
  std::vector<std::vector<std::pair<integer,doublereal>>> *dlog=nullptr;
  AutoGlobalScratch *scratch;
  integer loop_start; /*scalar input*/
  integer loop_end; /*scalar output*/
};

/* The process function: the original AUTO called it per worker, on a
   shared-memory machine or wrapped for message passing. */
void conpar_process(const conpar_parallel_arglist *arg)
{
  const integer rows = *arg->nra, cols = *arg->nca;
  const integer parameters = *arg->ncb, constraints = *arg->nrc;
  const integer left = *arg->nov, end = cols - left;
  // Per-worker scratch, reused for every interval in this worker's range.
  auto &maximum_column = arg->scratch->maximum_column;
  auto &swaps = arg->scratch->conpar_swaps;
  maximum_column.resize(rows);
  swaps.resize(end - left);
  for (integer interval = arg->loop_start; interval < arg->loop_end; ++interval) {
    xpp::Matrix<double> a({arg->a + interval * rows * cols,
                          static_cast<size_t>(rows * cols)}, cols, rows);
    xpp::Matrix<double> b({arg->b + interval * rows * parameters,
                          static_cast<size_t>(rows * parameters)}, parameters, rows);
    xpp::Matrix<double> c({arg->c + interval * constraints * cols,
                          static_cast<size_t>(constraints * cols)}, cols, constraints);
    std::span<integer> irf(arg->irf + interval * rows, rows);
    std::span<integer> icf(arg->icf + interval * cols, cols);
    // Swapping whole rows/columns also carries the already stored multipliers.
    const auto swap_rows = [&](integer first, integer second) {
      if (first == second) return;
      for (integer col = 0; col < cols; ++col) std::swap(a(col, first), a(col, second));
      for (integer col = 0; col < parameters; ++col) std::swap(b(col, first), b(col, second));
    };
    const auto swap_columns = [&](integer first, integer second) {
      if (first == second) return;
      for (integer row = 0; row < rows; ++row) std::swap(a(first, row), a(second, row));
      for (integer row = 0; row < constraints; ++row) std::swap(c(first, row), c(second, row));
    };
    for (integer row = 0; row < rows; ++row) {
      double largest = 0.;
      maximum_column[row] = left;
      for (integer col = left; col < end; ++col) {
        double value = a(col, row);
        if (value < 0.) value = -value;
        if (largest < value) {
          largest = value;
          maximum_column[row] = col;
        }
      }
    }
    for (integer col = left; col < end; ++col) {
      const integer pivot_row = col - left;
      integer selected_row = pivot_row, selected_column = col;
      double largest = 0.;
      // Strict comparisons preserve the original first maximum, including ties.
      for (integer row = pivot_row; row < rows; ++row) {
        double value = a(maximum_column[row], row);
        if (value < 0.) value = -value;
        if (largest < value) {
          largest = value;
          selected_row = row;
          selected_column = maximum_column[row];
        }
      }
      swaps[pivot_row] = {selected_row, selected_column};
      swap_rows(pivot_row, selected_row);
      swap_columns(col, selected_column);
      std::swap(irf[pivot_row], irf[selected_row]);
      std::swap(icf[col], icf[selected_column]);
      for (integer row = pivot_row + 1; row < rows; ++row) {
        const double rm = a(col, row) / a(col, pivot_row);
        a(col, row) = rm;
        // Shape follows AUTO-07p's IMSBRA (solvebv.f90): the zero test sits outside the element loops, same bits.
        double *__restrict target = &a(0, row);
        const double *__restrict source = &a(0, pivot_row);
        largest = 0.;
        integer at = col + 1;
        if (rm != 0.) {
          for (integer l = 0; l < left; ++l) target[l] -= rm * source[l];
          for (integer l = col + 1; l < end; ++l) {
            const double v = target[l] - rm * source[l];
            target[l] = v;
            const double value = v < 0. ? -v : v;
            if (largest < value) [[unlikely]] { largest = value; at = l; }
          }
          for (integer l = end; l < cols; ++l) target[l] -= rm * source[l];
          for (integer l = 0; l < parameters; ++l) b(l, row) -= rm * b(l, pivot_row);
        } else {
          // A zero multiplier still needs a scan: the column swap can change tie order.
          for (integer l = col + 1; l < end; ++l) {
            const double v = target[l];
            const double value = v < 0. ? -v : v;
            if (largest < value) [[unlikely]] { largest = value; at = l; }
          }
        }
        maximum_column[row] = at;
      }
      for (integer row = *arg->nbc; row < constraints; ++row) {
        const double rm = c(col, row) / a(col, pivot_row);
        c(col, row) = rm;
        if (rm != 0.) {
          for (integer l = 0; l < left; ++l) c(l, row) -= rm * a(l, pivot_row);
          for (integer l = col + 1; l < cols; ++l) c(l, row) -= rm * a(l, pivot_row);
          for (integer l = 0; l < parameters; ++l) {
            const integer at = l + row * parameters;
            // W247 replays subtractions in serial interval/step/row/column order.
            if (arg->dlog) (*arg->dlog)[interval].emplace_back(at, rm * b(l, pivot_row));
            else arg->d[at] -= rm * b(l, pivot_row);
          }
        }
      }
    }
    // Later stages address the original storage through IRF/ICF. Undo only
    // physical moves, leaving those permutation records and every value intact.
    for (integer step = end - left; step > 0; --step) {
      const integer row = step - 1;
      swap_columns(left + row, swaps[row].second);
      swap_rows(row, swaps[row].first);
    }
  }
}

void
conpar_default_wrapper(AutoLib &lib, integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nbc, integer *nrc, doublereal *c, doublereal *d, integer *irf, integer *icf)

{
    conpar_parallel_arglist data;
    data.scratch = &lib.scratch;
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
    if (!lib.collocation || lib.collocation->threads()==1) {
      conpar_process(&data);
      return;
    }
    auto &dlog = lib.solvbv.dlog;
    dlog.resize(*na);
    const auto terms = xpp::size_product(*nra - *nov, *nrc - *nbc, *ncb);
    for (auto &interval : dlog) {
      interval.clear();
      interval.reserve(terms);
    }
    data.dlog=&dlog;
    lib.collocation->intervals(*na,[&](long begin,long end,AutoLib &worker) {
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


int 
conpar(AutoLib &lib, integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nbc, integer *nrc, doublereal *c, doublereal *d, integer *irf, integer *icf)
{
  /* Aliases for the dimensions of the arrays */
  integer icf_dim1, irf_dim1;
  
  /* Local variables */
  integer i,j;
  integer nex;

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
  conpar_default_wrapper(lib,nov, na, nra, nca, a,
			  ncb, b, nbc, nrc, c, d,irf, icf);
  return 0;
} 








