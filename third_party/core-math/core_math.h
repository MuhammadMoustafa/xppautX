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

/* On x86 each function is built a second time with FMA instructions
   (-mfma, renamed _fma by the Makefile) and the caller picks it on a CPU
   that has FMA: the same correctly rounded result, bit for bit, several
   times faster than the plain build, whose fused multiply-adds are calls
   into the C library. */
#if defined(__x86_64__) || defined(__i386__)
#define XPP_CORE_MATH_FMA 1
double cr_exp_fma(double x);
double cr_log_fma(double x);
double cr_log10_fma(double x);
double cr_pow_fma(double x, double y);
double cr_sin_fma(double x);
double cr_cos_fma(double x);
double cr_tan_fma(double x);
double cr_asin_fma(double x);
double cr_acos_fma(double x);
double cr_atan_fma(double x);
double cr_atan2_fma(double y, double x);
double cr_sinh_fma(double x);
double cr_cosh_fma(double x);
double cr_tanh_fma(double x);
double cr_hypot_fma(double x, double y);
double cr_erf_fma(double x);
double cr_erfc_fma(double x);
double cr_lgamma_fma(double x);
#endif

#ifdef __cplusplus
}
#endif

#endif
