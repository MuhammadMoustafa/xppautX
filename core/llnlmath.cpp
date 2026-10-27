/******************************************************************
 *                                                                *
 * File          : llnlmath.c                                     *
 * Programmers   : Scott D. Cohen and Alan C. Hindmarsh @ LLNL    *
 * Last Modified : 1 September 1994                               *
 *----------------------------------------------------------------*
 * This is the implementation file for a C math library.          *
 *                                                                *
 ******************************************************************/


#include <stdio.h>
#include <math.h>
#include "llnlmath.h"
#include "xpp_math.h"

namespace xpp::cvode_solver {


#define ZERO 0.0
#define ONE  1.0
#define TWO  2.0


real UnitRoundoff(void)
{
  real u;
  volatile real one_plus_u;
  
  u = ONE;
  one_plus_u = ONE + u;
  while (one_plus_u != ONE) {
    u /=  TWO;
    one_plus_u = ONE + u;
  }
  u *=  TWO;
  
  return(u);
}


real RPowerI(real base, int exponent)
{
  int i, expt;
  real prod;

  prod = ONE;
  expt = ABS(exponent);
  for(i=1; i <= expt; i++) prod *= base;
  if (exponent < 0) prod = ONE/prod;
  return(prod);
}


real RPowerR(real base, real exponent)
{
 
  if (base <= ZERO) return(ZERO);

  return(static_cast<real>(xpp::math::pow(static_cast<double>(base), static_cast<double>(exponent))));
}


real RSqrt(real x)
{
  if (x <= ZERO) return(ZERO);

  return(static_cast<real>(sqrt(static_cast<double>(x))));
}

} // namespace xpp::cvode_solver
