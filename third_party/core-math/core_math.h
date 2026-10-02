/* The vendored CORE-MATH functions this program calls (binary64, correctly
   rounded), declared for C++. Written for xppautX; the sources beside it
   are CORE-MATH's own, unchanged (see README.md). Only core/xpp_math.h
   includes this header. */
#ifndef XPP_CORE_MATH_H
#define XPP_CORE_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

double cr_exp(double x);
double cr_log(double x);
double cr_log10(double x);
double cr_pow(double x, double y);
double cr_sin(double x);
double cr_cos(double x);
double cr_tan(double x);
double cr_asin(double x);
double cr_acos(double x);
double cr_atan(double x);
double cr_atan2(double y, double x);
double cr_sinh(double x);
double cr_cosh(double x);
double cr_tanh(double x);
double cr_hypot(double x, double y);
double cr_erf(double x);
double cr_erfc(double x);
double cr_lgamma(double x);

#ifdef __cplusplus
}
#endif

#endif
