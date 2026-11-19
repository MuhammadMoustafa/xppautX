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
- Our one change from that commit (W833, #914): `sin/sin.c` has a new first
  path, `sin_small_fast`, for 0x1.7137449123ef6p-26 < |x| < 2^-4. Upstream's
  fast path tests its rounding against an absolute error bound (`eps =
  0x1.dep-64`), which for 2^-16 < |x| < ~0.1 is a large part of an ulp of the
  result, so the test fails for nearly every argument there and the call takes
  the 128-bit `sin_large_accurate` (44 ns against 9 ns): kuramot100-dopri8's
  phase differences of 1e-4 to 1e-2 spent half of its cycles in it (the
  same bound is in CORE-MATH master of 2026-11-19, commit 1fdc53360d). The new
  path evaluates x + x^3*q(x^2), q the Taylor polynomial to x^9, in plain
  doubles and tests against a bound relative to the correction (2^-49|l|, the
  derivation of the 5.4u error and the margin is in the function's comment);
  when the test fails, the existing code runs as before, so the result stays
  the correctly rounded one, bit for bit, in both builds (it was compared
  with upstream's cr_sin, plain and FMA copies, on 5*10^8 random arguments of
  2^-26 to 1 and 456076 neighbours, 3000 ulps each way, both signs, of 38
  points at the ends of the ranges and CORE-MATH's hard cases, and with
  mpmath at 200 bits on 3*10^6 of them). The test fails for 1% of [2^-5,
  2^-4), 0.06% of [2^-7, 2^-6), less below; a call takes 2.3 ns (29 to 42
  before).
  The other functions are upstream's: on the arguments the examples and
  autoX's descriptions pass, exp falls back for 0.1% of its calls, pow for
  0.08%, and cos for 2%, nearly all of them near its zeros (koho's and
  wcring's cos of pi/2 rounded: a result near 1e-9, which an absolute bound
  cannot round without the slow path); none of the comparison's models does.
- To update: copy the same files from a newer CORE-MATH checkout (sin.c: apply
  sin_small_fast again, unless upstream's test has become relative), change
  the commit above, and rerun tools/examples_check.sh and goldencheck (a
  fixed bug in a function changes its results for the inputs it fixes).
