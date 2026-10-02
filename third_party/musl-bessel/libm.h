#ifndef XPP_MUSL_LIBM_H
#define XPP_MUSL_LIBM_H

/* Only musl's internal facilities used by j0.c, j1.c and jn.c.
   Include system declarations before remapping the public names. */
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "bessel.h"

/* musl's double_t intermediates must evaluate in binary64 on every target. */
_Static_assert(FLT_EVAL_METHOD == 0 && sizeof(double) == sizeof(uint64_t)
               && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "musl Bessel requires IEEE binary64 evaluation");

static inline uint64_t xpp_musl_bits(double x)
{
    uint64_t bits;
    memcpy(&bits, &x, sizeof bits);
    return bits;
}

/* Integer shifts of the copied representation work on either byte order. */
#define GET_HIGH_WORD(hi, x) do { \
    (hi) = (uint32_t)(xpp_musl_bits(x) >> 32); \
} while (0)
#define EXTRACT_WORDS(hi, lo, x) do { \
    const uint64_t xpp_musl_words = xpp_musl_bits(x); \
    (hi) = (uint32_t)(xpp_musl_words >> 32); \
    (lo) = (uint32_t)xpp_musl_words; \
} while (0)

/* Reuse xpp::math's dispatch through the numerics owner's C++ bridge. */
#define sin xpp_musl_sin
#define cos xpp_musl_cos
#define log xpp_musl_log
#define sqrt xpp_musl_sqrt
#define fabs xpp_musl_fabs

/* Avoid binding musl's public Bessel symbols to the platform C library. */
#define j0 xpp_musl_j0
#define j1 xpp_musl_j1
#define jn xpp_musl_jn
#define y0 xpp_musl_y0
#define y1 xpp_musl_y1
#define yn xpp_musl_yn

#endif
