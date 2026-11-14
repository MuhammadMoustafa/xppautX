#pragma once

#include <cstddef>
#include <limits>
#include <span>
#include <vector>
#include "numerics_settings.h"
#include "xpp_error.h"
#include "xpp_math.h"
#include "xpp_mem.h"

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
  std::vector<double> flag_yold; /* flags.cpp's flag_loop: the state before a step */
  std::vector<int> pivot;        /* the implicit steps' LU pivots (volterra2.cpp, do_fit.cpp), sized once by size_pivots */
  xpp::BlockLu lu;               /* the implicit steps' LU of a model's Jacobian, by independent subsystem (odesol2.cpp, stiff.cpp) */
  /* where a method's routine (odesol2.cpp, stiff.cpp, volterra2.cpp) last
     gave up, for its Solver's error (solver.cpp): the time it reached, the
     step size it last tried, the variable it failed in (from 0, -1 when it
     cannot tell) and the iterations it made */
  struct Stop {
    double t = 0, h = 0;
    int variable = -1, iterations = 0;
  } stop;
  /* the pivots for an n x n factorization: its address, valid until the
     next call with a different n */
  int *size_pivots(int n)
  {
    pivot.resize(static_cast<std::size_t>(n));
    return pivot.data();
  }
};

/* One stored column: a view of float or double values (which one is the store's precision, StoragePrecision), read as
   col[row], a double, and written as col[row] = value, rounded to a float under single, the way storing always did.
   A view owns nothing; it is a copy of a pointer and a flag. */
class Column {
public:
  /* col[row]: reads and writes the value (a pointer to it, whichever of the two the column holds) */
  class Ref {
  public:
    Ref(float *f, double *d) noexcept : f_(f), d_(d) {}
    operator double() const noexcept { return d_ ? *d_ : static_cast<double>(*f_); }
    Ref &operator=(double v) noexcept
    {
      if (d_) *d_ = v;
      else *f_ = static_cast<float>(v);
      return *this;
    }
    Ref &operator=(const Ref &other) noexcept { return *this = static_cast<double>(other); }
    Ref &operator+=(double v) noexcept { return *this = static_cast<double>(*this) + v; }
  private:
    float *f_;
    double *d_;
  };

  Column() noexcept = default;
  explicit Column(float *p) noexcept : f_(p) {}
  explicit Column(double *p) noexcept : d_(p) {}
  /* no values behind it (a column nothing owns) */
  bool empty() const noexcept { return !f_ && !d_; }
  bool is_double() const noexcept { return d_ != nullptr; }
  Ref operator[](std::ptrdiff_t row) const noexcept { return d_ ? Ref(nullptr, d_ + row) : Ref(f_ + row, nullptr); }
  double get(std::ptrdiff_t row) const noexcept { return d_ ? d_[row] : static_cast<double>(f_[row]); }
  void put(std::ptrdiff_t row, double v) const noexcept
  {
    if (d_) d_[row] = v;
    else f_[row] = static_cast<float>(v);
  }
  /* the n values from `from`, as doubles (exactly: a float is a double) */
  std::vector<double> values(std::ptrdiff_t from, std::ptrdiff_t n) const;
  /* the n values src holds, stored from row `from` on */
  void store(std::ptrdiff_t from, std::span<const double> src) const noexcept;
  /* the n values from `from`, each rounded to a float (what a drawing and a float32 series take) */
  std::vector<float> as_floats(std::ptrdiff_t from, std::ptrdiff_t n) const;
  /* the values as they are held when they are floats (the bytes a float32 series sends): nullptr for a double column */
  const float *float_data() const noexcept { return f_; }
private:
  float *f_ = nullptr;
  double *d_ = nullptr;
};

/* f called with a float{} or a double{}, the type c holds: the one place that picks an analysis's arithmetic by the
   store's precision (a single store's in floats, as it always was) */
template<class F> decltype(auto) with_value_type(const Column &c, F &&f)
{
  if (c.is_double()) return f(double{});
  return f(float{});
}

/* A table of columns: the store's, or a derived data set's (LentColumns), read as table[column][row]. A table is a
   pointer, so a view of the table made once stays valid while the columns change precision or move. */
class ColumnTable {
public:
  ColumnTable() noexcept = default;
  explicit ColumnTable(Column *columns) noexcept : p_(columns) {}
  Column &operator[](std::ptrdiff_t c) const noexcept { return p_[c]; }
  bool operator==(const ColumnTable &) const noexcept = default;
  explicit operator bool() const noexcept { return p_ != nullptr; }
private:
  Column *p_ = nullptr;
};

/* the memory of one column of n values that a derived data set or a data-table column owns, zeroed, float or double */
class OwnedColumn {
public:
  /* n zeros of the precision (std::bad_alloc when there is no memory) */
  void assign(std::size_t n, StoragePrecision precision);
  /* n values, the new ones zero, the old kept */
  void resize(std::size_t n);
  /* src's rows [from, to), held at src's precision (a float column of a single store, else doubles) */
  void assign_copy(const Column &src, std::size_t from, std::size_t to);
  /* src's rows [from, to) added after the values held, each at this column's precision */
  void append(const Column &src, std::size_t from, std::size_t to);
  /* the same values at the precision (a float held exactly, a double rounded) */
  void convert(StoragePrecision precision);
  void clear() noexcept;
  bool empty() const noexcept { return f_.empty() && d_.empty(); }
  std::size_t size() const noexcept { return wide_ ? d_.size() : f_.size(); }
  /* a view of the values (a view is shallow: it writes through a const column too) */
  Column view() const noexcept;
private:
  std::vector<float> f_;
  std::vector<double> d_;
  bool wide_ = false;
};

/* The data store: the rows a run stores, one column per quantity (column 0
   the time, then the variables and auxiliaries: NEQ+1 columns in use), each
   float or double by the `storage_precision` setting, read and written as
   data_store.col[column][row] (Column); and the point where the last run
   ended. storage.cpp owns it; the data table shows it (browse_data.cpp)
   unless another data set (a histogram, the Fourier modes, the adjoint) is
   shown in its place. The Session holds it (session.h). */
struct DataStore {
  ColumnTable col;       /* col[c] is column c, max_rows values */
  int rows = 0;          /* rows stored so far */
  int max_rows = 0;      /* rows each column has room for (start_rows at a load; grows when a run fills it or data
                            that is longer is loaded) */
  std::vector<double> current; /* the state where the last run ended (the model's columns, set by allocate) */
  double current_time = 0;     /* and its time */

  /* Add column's room: the columns past the model's own that the data
     data table may add. The table of columns has a fixed address (the
     data table keeps it as col), so it is made once, with this room. */
  static constexpr int ADDED_COLUMNS = 1000;

  /* The most rows the store starts with at a load (fewer when one run stores fewer: a large model pays a page in every
     column for rows it never writes), the length of a run of 5000 steps, and
     the store grows from there when a run or the data loaded is longer. A steady run (run.steady) keeps at most
     this many, thinning when it reaches them, so its memory does not grow with its length. */
  static constexpr int start_rows = 5000;
  /* The most rows a run keeps: the store doubles its rows when it fills (stor_full), and twice this is still an int
     (W678: a run that stores more stops with an error that names the store, its rows kept). */
  static constexpr int max_rows_kept = std::numeric_limits<int>::max() / 2;

  /* ncol columns of nrow rows at the precision, empty (rows = 0); nrow becomes max_rows; the table has room for
     ADDED_COLUMNS more */
  void allocate(int nrow, int ncol, StoragePrecision precision);
  /* the precision the columns are held at */
  StoragePrecision precision() const noexcept { return precision_; }
  /* v as storing it keeps it: rounded to a float under single, v itself under double (an intermediate of a computation
     that its result's float arithmetic would have rounded, so a single store's results stay what they were) */
  double round(double v) const noexcept { return precision_ == StoragePrecision::double_ ? v : static_cast<double>(static_cast<float>(v)); }
  /* the bytes of one stored value */
  std::size_t value_bytes() const noexcept { return precision_ == StoragePrecision::double_ ? sizeof(double) : sizeof(float); }
  /* Every column, the rows stored so far, to the precision, in place and with no question: a float becomes the double
     it is, a double the float storing it under single would have kept. The columns' addresses (the table's) stay;
     the error when there is no memory (the store as it was). */
  Result<> set_precision(StoragePrecision precision);
  /* the first ncol columns to nrow rows, keeping what they hold (the rows
     added are zero), or the error when there is no memory (the columns
     as they were). The caller sets max_rows. */
  Result<> grow(int ncol, int nrow);
  /* room for nrow rows in every column, the user's added ones too (the first ncol keep what they hold, the rows added
     are zero): nothing when there is room already, else grown and max_rows set; the error when there is no memory
     (the columns as they were) */
  Result<> fit(int ncol, int nrow);
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
     own computed ones: dst[from..to] are the store's columns, which
     stay the store's (they are only valid until the store grows or changes precision). */
  void lend_columns(Column *dst, int from, int to) const;
private:
  /* The model's columns are one block of pool_cols_ x pool_rows_ zeroed
     values (untouched until written: a short run of a large model does
     not pay for its whole storage); a column the data table adds is an
     OwnedColumn of its own, added_[c]. table_ holds their views, which col
     points at (fixed, so a pointer to it stays valid). */
  StoragePrecision precision_ = StoragePrecision::single;
  ZeroedBytes pool_;
  int pool_cols_ = 0;
  int pool_rows_ = 0;
  std::vector<OwnedColumn> added_;
  std::vector<Column> table_;
  /* the view of the pool's column c */
  Column pool_column(int c) const noexcept;
};

/* The Session's stored data converted to numerics.storage_precision (the setting's `applied`): the data store's columns,
   and every derived data set made from it (a histogram, the Fourier modes, the adjoint, the transposed data, the
   stochastic statistics), in place and with no question, so what the data table shows stays what it was. A failure for
   want of memory is shown and the setting goes back to what the store holds. */
void storage_precision_changed(Session &s);

/* A derived data set's columns (a histogram, the Fourier modes, the
   adjoint, its H function, the transposed data): n columns of its own,
   at the store's precision, then the store's columns n..last lent after
   them (lend_columns), in one table of columns the data table shows
   (new_browse_dat). The table's address never changes; its own columns are
   freed by release() or the next make(). The store's columns move when it
   grows (a run that fills it), so a data set shown again takes them afresh
   (view). */
class LentColumns {
public:
  /* n zeroed columns of len values, store's n..last after them; the
     table */
  ColumnTable make(const DataStore &store, int n, int len, int last);
  /* the table to show again, with the store's columns as they are now */
  ColumnTable view(const DataStore &store);
  /* its own columns converted to the store's precision, and the store's lent again (the store has changed precision) */
  void retype(const DataStore &store);
  void release();
  ColumnTable table() noexcept { return ColumnTable(table_.data()); }
private:
  std::vector<OwnedColumn> own_;
  std::vector<Column> table_;
  int last_ = -1; /* the last of the store's columns lent */
};

} // namespace xpp
