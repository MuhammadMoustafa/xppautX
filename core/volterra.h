#pragma once

#include "xpplim.h"
#include "expr_program.h"

/* a kernel that is a convolution (KERNEL's flag) */
#define CONV 2

#include <array>
#include <string>
#include <vector>

/* An integral equation's kernel as the model declares it (expr_symbols.cpp
   add_kernel; volterra2.cpp alloc_v_memory compiles it while the model
   loads): K(t,t',u) with the constant mu, or a convolution kerexpr#expr
   (flag CONV). xpp::Model's kernels hold them; the running integrals are
   volterra2.cpp's. */
struct KERNEL {
  double mu=0.0;
  xpp::Program formula,kerform; /* expr and kerexpr compiled */
  xpp::Place where; /* the declaring line, including an anonymous inline kernel */
  int flag=0;
  std::string name,expr,kerexpr;
};

namespace xpp {

/* each kernel's running values (xpp::Model has its definition): the
   integral at this step and the step before (k_n, k_n1), the sum up to
   the step before, bet_nn, and at every grid point the weights alpbetjn
   (al) and the convolution's values (cnv) */
struct KernelState {
  double k_n1=0.0,k_n=0.0,sum=0.0,betnn=0.0;
  std::vector<double> al,cnv;
};

/* the integral equations' running state (volterra2.cpp), a Session's
   (session.h): the kernels', the grid point reached, whether k_n is the
   current value (kn_flag), and every variable's value at every grid
   point (allocate_volterra), one per variable, not per kernel */
/* The kernel formulas' values at the history points (init_sums, W488):
   a formula that reads only the history (the variables there and t'),
   the constants and tables is evaluated once per point and kept, as long
   as the constants are those it was computed with. Slot p is grid point
   point[p] (p its place in memory's ring, -1 empty), computed with the
   window starting at first[p] (t' is summed from there); value has a
   kernel's value per slot. */
struct HistoryValues {
  std::vector<double> value;
  std::vector<int> point, first;
  std::vector<double> constants;
};

struct VolterraState {
  std::vector<KernelState> kernels; /* one per kernel (alloc_v_memory sizes it) */
  int current_point=0, kn_flag=0;
  std::vector<std::vector<double>> memory; /* allocate_volterra's variables */
  HistoryValues history;
};

}

