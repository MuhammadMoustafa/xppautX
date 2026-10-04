/* W184 diagnostic only, never part of the application Makefile.
   Relaxed SIMD permits either fused or unfused rounding depending on the
   runtime. This replacement measures the hardware-assisted ceiling on the
   current runtime; passing samples does not prove portable correctness.
   Do not adopt it as the portable math implementation. */
#include <wasm_simd128.h>

// The vendored C math objects call this C-library symbol.
extern "C" double fma(double a, double b, double c)
{
    return wasm_f64x2_extract_lane(wasm_f64x2_relaxed_madd(
        wasm_f64x2_splat(a), wasm_f64x2_splat(b), wasm_f64x2_splat(c)), 0);
}
