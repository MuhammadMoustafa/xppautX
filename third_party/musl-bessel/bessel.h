#ifndef XPP_MUSL_BESSEL_H
#define XPP_MUSL_BESSEL_H

/* C API of the vendored Bessel objects and their xpp_math bridge. */
#ifdef __cplusplus
extern "C" {
#endif
double xpp_musl_j0(double x);
double xpp_musl_j1(double x);
double xpp_musl_jn(int n, double x);
double xpp_musl_y0(double x);
double xpp_musl_y1(double x);
double xpp_musl_yn(int n, double x);
double xpp_musl_sin(double x);
double xpp_musl_cos(double x);
double xpp_musl_log(double x);
#ifdef __cplusplus
}
#endif

#endif
