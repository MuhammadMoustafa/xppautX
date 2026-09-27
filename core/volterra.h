#ifndef _volterra_h_
#define _volterra_h_

#include "xpplim.h"


#ifdef __cplusplus
#include <array>
#include <string>
#include <vector>

/* An integral equation's kernel as the model declares it (parserslow2.cpp
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
#endif

#endif
