#ifndef _volterra_h_
#define _volterra_h_

#include "xpplim.h"


#ifdef __cplusplus
#include <array>
#include <string>
#include <vector>

/* An integral equation's kernel (parserslow2.cpp add_kernel declares it,
   volterra2.cpp compiles and evaluates it): K(t,t',u) with the constant
   mu, or a convolution kerexpr#expr (flag CONV) */
struct KERNEL {
  double k_n1=0.0,k_n=0.0,sum=0.0,betnn=0.0,mu=0.0;
  std::vector<double> al,cnv;       /* alpbetjn's weights, the convolution's values */
  std::vector<int> formula,kerform; /* expr and kerexpr compiled */
  int flag=0;
  std::string name,expr,kerexpr;
};

/* the kernels (NKernel of them) and the values of each variable at every
   grid point (volterra2.cpp allocate_volterra), both parserslow2.cpp's */
extern std::array<KERNEL,MAXKER> kernel;
extern std::array<std::vector<double>,MAXODE> Memory;
#endif

#endif
