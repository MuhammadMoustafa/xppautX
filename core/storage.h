#ifndef _storage_h_
#define _storage_h_

#include "xpplim.h" /* MAXODE */
#include <array>
#include <vector>
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* the integrator's state vector (the ODEs, then the Markov variables and
   the delay/Volterra extras) that the solvers step in place */
struct XPPVEC {
  int nvec,node;
  double *x;
};

/* the Session s's state vector, zeroed, for its Model's ODEs and Markov
   variables */
void init_alloc_info(Session &s);

/* the integrator's state vector (init_alloc_info), a Session's
   (session.h); each solver owns its own work memory (solver.h) */
struct SolverWork {
  XPPVEC xpv{};             /* xpv.x points at state */
  std::vector<double> state;
};

/* The data store: the rows a run stores, one float column per quantity
   (column 0 the time, then the variables and auxiliaries: NEQ+1 columns
   in use), read and written as data_store.col[column][row]; and the point
   where the last run ended. storage.cpp owns it; the data browser shows it
   (browse_data.cpp) unless another data set (a histogram, the Fourier
   modes, the adjoint) is shown in its place. The Session holds it
   (session.h). */
struct DataStore {
  float **col = nullptr; /* col[c] is column c, max_rows floats */
  int rows = 0;          /* rows stored so far */
  int max_rows = 0;      /* rows each column has room for (the model's
                            maxstor; grows when a run fills it) */
  double current[MAXODE] = {}; /* the state where the last run ended */
  double current_time = 0;     /* and its time */

  /* ncol columns of nrow rows, empty (rows = 0); nrow becomes max_rows */
  void allocate(int nrow, int ncol);
  /* the first ncol columns to nrow rows, keeping what they hold (the rows
     added are zero), or the error when there is no memory (the columns
     as they were). The caller sets max_rows. */
  Result<> grow(int ncol, int nrow);
  /* the first ncol columns keep every second row (rows 0, 2, 4, ...), so
     the rows left still span the run, with room for as many more */
  void thin(int ncol);
  /* column c (a new user column) with max_rows rows of zeros */
  void add_column(int c);
  /* Remove a derived column and shift subsequent owned columns left; the
     error when c is not one of the `count` columns (count itself at most
     the store's column table). */
  Result<> remove_column(int c, int count);
  /* A derived data set (a histogram, the Fourier modes, the adjoint's
     H function) shows the store's columns from..to (inclusive) beside its
     own computed ones: dst[from..to] point at the store's columns, which
     stay the store's (dst must not free them, and they are only valid
     until the store grows). */
  void lend_columns(float **dst, int from, int to) const;
private:
  /* the columns' memory, and the table of their addresses col points at
     (fixed, so a pointer to it stays valid) */
  std::vector<std::vector<float>> columns_;
  std::array<float *, MAXODE + 1> table_{};
};

/* A derived data set's columns (a histogram, the Fourier modes, the
   adjoint, its H function, the transposed data): n columns of its own,
   then the store's columns n..last lent after them (lend_columns), in one
   table of column pointers the data browser shows (new_browse_dat). The
   table's address never changes; its own columns are freed by release()
   or the next make(). The store's columns move when it grows (a run that
   fills it), so a data set shown again takes them afresh (view). */
class LentColumns {
public:
  /* n zeroed columns of len floats, store's n..last after them; the
     table */
  float **make(const DataStore &store, int n, int len, int last);
  /* the table to show again, with the store's columns as they are now */
  float **view(const DataStore &store);
  void release();
  float **table() noexcept { return table_.data(); }
private:
  std::vector<std::vector<float>> own_;
  std::array<float *, MAXODE + 1> table_{};
  int last_ = -1; /* the last of the store's columns lent */
};

} // namespace xpp
#endif
