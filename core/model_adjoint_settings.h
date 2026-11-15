#pragma once
/* The adjoint's settings the option table's rows set (the H function's couplings, Transpose): plain data, which a Session's
   AdjointState (adjoint_state.h, the analyses') extends with the derived data sets (W757). */
#include <string>
#include <vector>

namespace xpp {

/* the adjoint's Newton tolerance and iterations, each equation's coupling for the H function as typed and Transpose's settings, a
   base of AdjointState */
struct AdjointSettings {
  /* the adjoint's Newton tolerance and iterations */
  double err = 1.e-3;
  int maxit = 20;
  /* each equation's coupling for the H function as typed (AdjointState::coup_fun holds each compiled) */
  std::vector<std::string> coup_string;
  /* Transpose's settings: the first column and its name, how many and
     the skip; the first row, how many and the skip */
  struct {
    int col0 = 2, ncol = 2, colskip = 1;
    int row0 = 1, nrow = 1, rowskip = 1;
    std::string firstcol;
  } transpose;
};

} // namespace xpp
