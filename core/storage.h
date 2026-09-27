#ifndef _storage_h_
#define _storage_h_

#include "xpplim.h" /* MAXODE */
#ifdef __cplusplus
extern "C" {
#endif

/* the integrator's state vector (the ODEs, then the Markov variables and
   the delay/Volterra extras) that the solvers step in place */
typedef struct {
  int nvec,node;
  double *x;
} XPPVEC;

extern XPPVEC xpv;

/* the solvers' scratch space, sized by alloc_meth for the method in use */
extern double *WORK;
extern int IWORK[10000];

void init_alloc_info(void);
void alloc_meth(void);

#ifdef __cplusplus
}

#include <array>
#include <vector>

/* The data store: the rows a run stores, one float column per quantity
   (column 0 the time, then the variables and auxiliaries: NEQ+1 columns
   in use), read and written as data_store.col[column][row]; and the point
   where the last run ended. storage.cpp owns it; the data browser shows it
   (browse_data.cpp) unless another data set (a histogram, the Fourier
   modes, the adjoint) is shown in its place. */
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
     added are zero); false (and an error message) when there is no
     memory. The caller sets max_rows. */
  bool grow(int ncol, int nrow);
  /* column c (a new user column) with max_rows rows of zeros */
  void add_column(int c);
  /* A derived data set (a histogram, the Fourier modes, the adjoint's
     H function) shows the store's columns from..to (inclusive) beside its
     own computed ones: dst[from..to] point at the store's columns, which
     stay the store's (dst must not free them, and they are only valid
     until the store grows). */
  void lend_columns(float **dst, int from, int to) const;
};

extern DataStore data_store;

/* A derived data set's columns (a histogram, the Fourier modes, the
   adjoint, its H function, the transposed data): n columns of its own,
   then the store's columns n..last lent after them (lend_columns), in one
   table of column pointers the data browser shows (new_browse_dat). The
   table's address never changes; its own columns are freed by release()
   or the next make(). */
class LentColumns {
public:
  /* n zeroed columns of len floats, the store's n..last after them; the
     table */
  float **make(int n, int len, int last);
  void release();
  float **table() noexcept { return table_.data(); }
private:
  std::vector<std::vector<float>> own_;
  std::array<float *, MAXODE + 1> table_{};
};
#endif
#endif
