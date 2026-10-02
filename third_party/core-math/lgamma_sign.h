/* Included before CORE-MATH's lgamma.c (the Makefile's -include): that file
   stores the sign of gamma(x) in the C library's global `signgam`, which
   some systems (MinGW's math.h) do not have and which is shared state
   besides. xppautX never reads it, so each store goes to a temporary
   instead: no global, the same on every system. */
#include <math.h>
#undef signgam
#define signgam (((int[1]){0})[0])
