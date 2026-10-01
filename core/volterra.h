#ifndef _volterra_h_
#define _volterra_h_

#include "xpplim.h"

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
  std::vector<int> formula,kerform; /* expr and kerexpr compiled */
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
struct VolterraState {
  std::array<KernelState,MAXKER> kernels;
  int current_point=0, kn_flag=0;
  std::array<std::vector<double>,MAXODE> memory;
};

}

#endif
