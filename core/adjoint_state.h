#pragma once
/* The adjoint's derived data sets a Session holds (adj2.cpp): plain data the adjoint's code works on (W744). The options the option
   table sets are model_adjoint_options.h's, its base (W757). */
#include <array>
#include <vector>
#include "expr_program.h"
#include "model_adjoint_options.h"
#include "storage.h"

namespace xpp {

/* adj2.cpp's derived data sets, a Session's (session.h): the adjoint, its
   H function and the transposed data, each its own columns with the
   stored ones lent beside them (storage.h's LentColumns), whether it
   exists and how many rows it has; shown in the browser in the stored
   data's place (adj_back, h_back, transpose_data). A load's new Session
   starts with none, so nothing of the Session before is shown again. */
struct AdjointState : AdjointSettings {
  LentColumns adjoint, h_function, transposed;
  int adjoint_rows = 0, h_rows = 0;
  bool adjoint_here = false, h_here = false, transposed_here = false;
  /* the H function has its odd and even parts too (more than two
     equations) */
  bool h_odd_even = false;
  /* each equation's coupling formula compiled (AdjointSettings::coup_string) */
  std::vector<Program> coup_fun;
  /* the maximal Liapunov exponent over a range: the parameter, the
     exponent (a point per run, while liap_flag) */
  std::array<std::vector<double>, 2> liap;
  int liap_flag = 0;
};

} // namespace xpp
