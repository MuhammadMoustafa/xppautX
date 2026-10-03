/* Included before every CORE-MATH function in the WebAssembly build (the
   Makefile's `make wasm`, --include): WebAssembly has no floating-point
   exception flags, and Emscripten's <fenv.h> defines no FE_UNDERFLOW and the
   like, which a few functions pass to feraiseexcept to raise the exception
   the C standard asks of an underflow. The values and the rounding are
   unaffected, and xppautX never reads the flags; with each exception 0,
   feraiseexcept and fetestexcept do nothing, as the platform's own do. */
#include <fenv.h>
#ifndef FE_INVALID
#define FE_INVALID 0
#endif
#ifndef FE_DIVBYZERO
#define FE_DIVBYZERO 0
#endif
#ifndef FE_OVERFLOW
#define FE_OVERFLOW 0
#endif
#ifndef FE_UNDERFLOW
#define FE_UNDERFLOW 0
#endif
#ifndef FE_INEXACT
#define FE_INEXACT 0
#endif
