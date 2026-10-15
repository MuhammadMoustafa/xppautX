// W358 (#420): the comparison build LIBM=system. Defines the symbols
// third_party/core-math/core_math.h declares (cr_exp, cr_pow, ..., and on x86
// their _fma twins) by forwarding to the C library's functions, so xpp::math
// (core/xpp_math.h) runs on glibc's instead of CORE-MATH's correctly rounded
// ones. Compiled only by `make LIBM=system BUILDDIR=<its own>` (Makefile);
// the default build never contains it, and no release does. Its numbers are
// not the baselines': glibc rounds some calls differently (W159, issue #211),
// and the point of the build is to measure what correct rounding costs and
// changes. tools/mathcheck.sh checks core/ only, which is why this file is
// not there.
#include "core_math.h"

#include <cmath>

// one list of the functions, by arity; glibc has one implementation of each
// (its own ifunc picks the variant), so a twin is the same call
#define XPP_UNARY(F) F(exp) F(log) F(log10) F(sin) F(cos) F(tan) F(asin) F(acos) F(atan) \
    F(sinh) F(cosh) F(tanh) F(erf) F(erfc) F(lgamma)
#define XPP_BINARY(F) F(pow) F(atan2) F(hypot)

#define XPP_DEFINE1(name) double cr_##name(double x) { return std::name(x); }
#define XPP_DEFINE2(name) double cr_##name(double x, double y) { return std::name(x, y); }
XPP_UNARY(XPP_DEFINE1)
XPP_BINARY(XPP_DEFINE2)

#ifdef XPP_CORE_MATH_FMA
#define XPP_TWIN1(name) double cr_##name##_fma(double x) { return cr_##name(x); }
#define XPP_TWIN2(name) double cr_##name##_fma(double x, double y) { return cr_##name(x, y); }
XPP_UNARY(XPP_TWIN1)
XPP_BINARY(XPP_TWIN2)
#endif
