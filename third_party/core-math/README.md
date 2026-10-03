# CORE-MATH (vendored)

Correctly rounded binary64 elementary functions, from the CORE-MATH project
(https://core-math.gitlabpages.inria.fr/, source
https://gitlab.inria.fr/core-math/core-math), MIT licence (`LICENSE`).

- Taken from commit `284b3b0e198042c38f5c30316f696786b10816b0`
  (2026-10-01, branch master), `src/binary64/<function>/`, each function's
  `.c` and the headers it includes, unchanged: exp, log, log10, pow, sin,
  cos, tan, asin, acos, atan, atan2, sinh, cosh, tanh, hypot, erf, erfc,
  lgamma. (Not taken: the project's tests, its MPFR checkers, the Sollya
  and Sage scripts, and the functions xppautX does not call.)
- `core_math.h` is ours: the C++ declarations of the `cr_*` functions;
  `roundeven.c` is ours too, built on Windows only: C23's `roundeven`, which MinGW's
  C library lacks and the plain x86 copies call;
  `lgamma_sign.h` is ours too, included before `lgamma.c` (the Makefile's
  `--include`): that file stores gamma's sign in the C library's global
  `signgam`, which MinGW's math.h lacks and which xppautX never reads, so the
  stores go to a temporary.
  `wasm_fenv.h` is ours too, included before each function in the
  WebAssembly build (`make wasm`): Emscripten's `<fenv.h>` has no
  `FE_UNDERFLOW` and the other exception flags (WebAssembly has none), so
  they are defined as 0.
  Only `core/xpp_math.h` includes it; the rest of the core calls
  `xpp::math::exp` and the others (CLAUDE.md "Single source").
- Why: the C library's exp, log, pow, sin, cos, ... are CPU-dispatched
  (glibc picks FMA or plain SSE2 variants at run time, and UCRT and macOS
  have their own algorithms), and none is correctly rounded: the same call
  gives different last bits on different CPUs and systems, which an
  adaptive step size or a chaotic model amplifies into different outputs
  (issue #211). A correctly rounded function returns the exact value
  rounded to nearest, so every platform returns the same bits.
- Built by the Makefile as one object per function (`core_math_<name>.o`,
  C, `-ffp-contract=off`, no warnings of ours, no LTO), like third_party/miniz.
  Where the compiler targets x86 each function is built a second time with
  `-mfma` and renamed `cr_<name>_fma` (`core_math_<name>_fma.o`), and
  `xpp::math` calls that copy on a CPU that has FMA (`__builtin_cpu_supports`):
  a plain build's fused multiply-adds are calls into the C library, and its
  `cos` and `pow` take 100-150 ns against 20-50 with FMA instructions (glibc's
  own: 20 and 25). Correct rounding makes the two copies one function, bit for
  bit: tests/test_math.cpp compares them at 20000 inputs each, on a CPU with
  FMA, and the plain copy is what runs on one without.
  CORE_MATH_SUPPORT_ERRNO is not defined: no errno, as the core never reads it.
- To update: copy the same files from a newer CORE-MATH checkout, change
  the commit above, and rerun tools/examples_check.sh and goldencheck (a
  fixed bug in a function changes its results for the inputs it fixes).
