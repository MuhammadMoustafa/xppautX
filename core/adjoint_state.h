#pragma once
/* The adjoint's derived data sets and settings a Session holds (adj2.cpp): plain data, which the option table reads and writes and the
   adjoint's code works on (W744). */
#include <array>
#include <string>
#include <vector>
#include "expr_program.h"
#include "storage.h"

namespace xpp {

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

} // namespace xpp
