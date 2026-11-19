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

## Upstream report to send (W836)

The maintainer sends it, together with W256's TinyCC report (2026-11-19: "Once we are ready we
will post it and the tinyCC one"). It goes to CORE-MATH's issue tracker
(https://gitlab.inria.fr/core-math/core-math/-/issues), titled
"binary64 sin: the fast path's absolute rounding bound sends most 2^-16 < |x| < 0.1 to the slow path".

> **Problem.** `cr_sin_moderate` tests its rounding against an absolute error bound
> (`eps = 0x1.dep-64`, src/binary64/sin/sin.c, the same at master 1fdc53360d). For
> 2^-16 < |x| < ~0.1, sin(x) is small, so that bound is a large part of an ulp of the
> result: the test fails for nearly every argument there, and the call takes
> `sin_large_accurate`, 31 ns against 8 ns on the fast path (44 ns a call on [0, 1e-3]).
>
> **Where it matters.** Coupled-oscillator and phase models (Kuramoto networks, for
> example) call sin on phase differences of 1e-4 to 1e-2. In one such model, 21% of all
> sin calls took the slow path, and they were 55% of the program's cycles. A program
> that uses CORE-MATH for identical results on every platform pays this on the whole
> class of models.
>
> **Suggested fix.** A first path for 2^-25.5 < |x| < 2^-4 whose rounding test is
> relative to the result. It evaluates x + x^3 q(x^2), q the Taylor polynomial to x^9, in
> plain doubles: the error of the correction l is at most 5.4u |l| (u = 2^-53; 10.8u in
> the directed modes). The test uses e = 2^-49 |l| = 16u |l|, so when x + (l - e) and
> x + (l + e) round to the same double, that double is the correctly rounded sin(x).
> When the test fails, the existing paths run unchanged, so every result stays the
> correctly rounded one. The derivation is in the comment:
>
> ```c
> // fast path for 0x1.7137449123ef6p-26 < |x| < 2^-4: the rounding test is
> // relative to the result. Returns 1 and the correctly rounded sin(x) in *res
> // when the test passes, else 0 (the caller then takes the existing paths).
> static inline int
> sin_small_fast (double x, double *res)
> {
>   /* sin(x) = x + x^3*q(x^2) with q(t) = -1/6 + t/120 - t^2/5040 + t^3/362880
>      - ..., the coefficients below being those of 1/3!, 1/5!, 1/7!, 1/9!
>      rounded to nearest. Let u = 2^-53 (2^-52 for the other rounding modes:
>      every error below is then at most twice as large, still within the margin
>      of the test). We compute, with one rounding each (no FMA: the build has
>      -ffp-contract=off):
>        t = x*x             relative error <= u
>        xt = x*t            relative error <= u (+ that of t)
>        p = c3 + t*(c5 + t*(c7 + t*c9))
>        l = xt*p            relative error <= u
>      p approximates q(t): c3 has relative error <= u (1/6 is not a double),
>      the final addition adds u, and the rest of the polynomial is at most
>      |t*c5/c3| = t/20 <= 2^-12.3 of c3 and carries errors of a few u of
>      that, i.e. below 0.001u; so p = q(x^2)*(1+e) with |e| <= 2.001u.
>      The terms dropped (x^11/11! on) are at most 6*x^8/11! < 2^-54.7 of
>      x^3/6 for |x| < 2^-4, which is 0.32u. Altogether
>        |l - (sin(x) - x)| <= ((1+u)^3*(1+2.001u)-1 + 0.32u)*|l| < 5.4u*|l|
>      (< 10.8u*|l| with the other rounding modes). The test below uses
>      e = 2^-49*|l| = 16u*|l|: the roundings of l-e and l+e add at most
>      2u*(1+16u)*|l|, so the computed lower bound x+(l-e) is at most the
>      exact sin(x), and the upper one at least. Both are monotone roundings
>      of numbers on either side of sin(x), so when they agree, the rounding of
>      sin(x) is that value. |l| >= 2^-80 here (x > 2^-25.5): no underflow, and
>      e is exact. Measured with mpmath (200 bits) on 3*10^6 arguments of
>      2^-26 < |x| < 2^-4: the largest error of l was 2^-51.15*|l|, within the
>      5.4u = 2^-50.6 above. The test fails for 1% of the arguments of
>      [2^-5, 2^-4), 0.06% of [2^-7, 2^-6), 0.0003% of [2^-12, 2^-11). */
>   static const double c3 = -0x1.5555555555555p-3, c5 = 0x1.1111111111111p-7,
>     c7 = -0x1.a01a01a01a01ap-13, c9 = 0x1.71de3a556c734p-19;
>   double t = x * x;
>   double p = c3 + t * (c5 + t * (c7 + t * c9));
>   double l = (x * t) * p;
>   double e = 0x1p-49 * __builtin_fabs (l);
>   double lb = x + (l - e), ub = x + (l + e);
>   *res = lb;
>   return lb == ub;
> }
> ```
>
> and in `cr_sin`, after the exponent is read:
>
> ```c
> if (e < 1019) { // |x| < 2^-4
>   double r;
>   if (__builtin_expect (sin_small_fast (x, &r), 1)) return r;
>   if (e < 1007) return sin_small_accurate (x); // |x| < 2^-16
> }
> ```
>
> **Validation.**
> - Identical to `cr_sin` at 284b3b0e, with and without FMA, on 5e8 random arguments
>   (exponent uniform over 2^-26..1, both signs) and on 456,076 ulp neighbours (3000
>   each way) of 38 points at the range ends and the known hard cases.
> - Identical to mpmath at 200 bits, rounded to nearest, on 3e6 of them.
> - A negative control (the test bound shrunk to 2^-52) gives 61 mismatches in 5e7, so
>   the harness can see an error.
> - The largest measured error of l was 2^-51.15 |l|, within the proven 5.4u = 2^-50.6.
> - The test fails for 1% of [2^-5, 2^-4), 0.06% of [2^-7, 2^-6), and less below. A call
>   takes 2.3 ns, and the model's run took 30.8 ms instead of 48.3 ms.
>
> **Also seen.** cos falls back every time for arguments within about 1e-9 of pi/2 (a
> decimal pi/2 in a model), for the same reason: an absolute bound against a tiny
> result. In our models it is 2% of cos calls, so we left it.
