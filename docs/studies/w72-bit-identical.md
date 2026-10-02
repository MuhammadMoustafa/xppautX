# W72 study: bit-identical results on every platform

**Decision (maintainer, 2026-09-28):** this report is merged and the change
is not adopted for now. Adopting it means building and testing a build with
FMA and one without on every platform, which is not a priority. The
implementation (vendored CORE-MATH, `-ffp-contract=off`, the call sites) is
kept on the local branch `task/W72-bitident-study`, commit `48c6279`.

Card W72 (#120, maintainer 2026-09-27): a seed (W71) should reproduce a
run on any machine. The random stream already is the same everywhere; the
C library's `exp`, `log`, `pow`, `sin` ... are not, and a compiler may fuse
`a*b+c` into one FMA. This branch (`task/W72-bitident-study`) tries the
fix and measures it. Measured 2026-09-28 on one Windows 11 machine (WSL
Ubuntu for Linux), branch point `bfad36e`.

## What the branch does

1. **`-ffp-contract=off`** for every C and C++ object (Makefile
   `FPFLAGS`). On x86-64 this changes nothing (the base instruction set
   has no FMA, so gcc and clang never fused anything); on arm64 (Apple
   Silicon) clang fuses by default, which is part of why macOS differed.
2. **CORE-MATH** (MIT, https://gitlab.inria.fr/core-math/core-math,
   commit `6b84457`) under `third_party/core-math/`: the 18 double
   functions the core calls -- sin cos tan asin acos atan atan2 sinh cosh
   tanh exp log log10 pow hypot erf erfc lgamma -- each correctly rounded,
   so each has exactly one right answer on every platform. Built as
   `cm_<f>.o` with `-w`, no LTO, like miniz.
3. **Single source**: `core/xpp_math.h` gains `xpp::math::sin` ... (inline
   forwarders to `cr_*`). All 107 libm calls in 25 core files, the parser's
   built-in table (`expr_functions.cpp`: the `^` operator is `pow`) and
   pocketfft's twiddle factors (a two-macro hook, `POCKETFFT_COS/SIN`,
   noted in its README) now go through it. `nm -u xppautX` lists none of
   the 18 from libm any more. `sqrt`, `fabs`, `floor`, `ceil`, `fmod` stay
   the C library's: IEEE 754 makes them exact everywhere.

## Examples matching Linux (tools/examples_check.sh, 184 models)

| build | before | after |
|---|---|---|
| Linux, gcc 15.2 / glibc 2.43 | 184 (baseline) | 184 (baseline changed for 12) |
| Windows MinGW, Strawberry gcc 13.2 (mingw-w64 11, msvcrt) | 171 (13 differ) | **184** |
| Windows MSYS2 clang64 22.1.7 (mingw-w64 15, UCRT) | 166 (18 differ) | **184** |
| either Windows build with `CORE_MATH_FLAGS="-mfma -msse4.1"` | -- | **184** |
| Linux with `CORE_MATH_FLAGS="-mfma -msse4.1"` | -- | **184** |
| macOS (committed CI baseline) | 155 (29 differ) | not measurable here |

"before" for Windows is against the Linux baseline of the time, "after"
against the new Linux md5s. Linux's own md5s change for 12 models,
because CORE-MATH rounds some results differently from glibc (glibc is
not correctly rounded: see the table below): canonical/sine-circle,
ode/cuplamdif, doubpend, geisel, hhred, idoubpend, itoy, nf3, r3b, toy_ok,
waterwheel, wcring (all use sin/cos/exp/tanh/`^`). lecar.ode's smoke md5
does not change. The branch commits the new tests/examples.md5.

**macOS** cannot be measured here. A CI run on the branch would: build
with Apple clang on macos-latest (arm64: FMA inline, so CORE-MATH is fast
there) and run `examples_check.sh --platform macos`. For that run to
report against Linux instead of failing against the stale
tests/examples.macos.md5, the three platform files
(tests/examples.{windows,windows-clang,macos}.md5, now out of date:
they record the old numerics) must be removed on the branch -- first-run
mode then reports the differing models, uploads them as
`examples-md5-macos`, and fails only on a crash. This branch leaves the
files in place (removing them is the reviewer's decision), so windows-core,
windows-clang and macos-core would fail on it as it stands. The
macos-x64 release runner (macos-15-intel) also needs checking: whether
Apple's libm has `roundeven` (the Makefile builds our own,
`xpp_roundeven.c`, everywhere but Linux, which should cover it).

## The functions themselves (docs/studies/w72-libmcmp.c)

1 000 000 inputs per function (splitmix64, the same on every platform),
the system function against CORE-MATH, bit for bit. CORE-MATH's hashes are
identical on all three toolchains for all 18 functions, except erfc on
Strawberry MinGW: 1 input in 1M comes out one ulp off, because
mingw-w64 11's `fma()` is not exact (its own fma hash differs from
glibc's and UCRT's, which agree). With `-mfma` (the hardware instruction)
it matches.

System results that differ from the correctly rounded one, per 1M:

| function | glibc 2.43 | MinGW msvcrt | UCRT |
|---|---|---|---|
| sin / cos / tan | 1441 / 1278 / 2516 | 488 / 439 / 43097 | 30448 / 30990 / 43097 |
| asin / acos / atan / atan2 | 1559 / 719 / 518 / 813 | 64603 / 77293 / 42977 / 247 | 64603 / 77293 / 42977 / 1038 |
| sinh / cosh / tanh | 256446 / 234676 / 110078 | 135987 / 166071 / 32633 | same as MinGW |
| exp / log / log10 | 755 / 140 / 70998 | 317 / 236 / 218 | 5010 / 170 / 218 |
| pow / hypot | 855 / 5899 | 760 / 162995 | 509 / 162995 |
| erf / erfc / lgamma | 0 / 0 / 0 | 24195 / 254622 / 493650 | 196349 / 431140 / 493657 |

## Speed

Per call (ns, same program; "cr" is CORE-MATH as built by default, "fma"
with `-mfma -msse4.1`):

| | glibc | cr | fma | MinGW msvcrt | cr | UCRT (clang64) | cr |
|---|---|---|---|---|---|---|---|
| exp | 9 | 15 | 8 | 79 | 86 | 7 | 13 |
| log | 7 | 37 | 11 | 64 | 616 | 7 | 28 |
| pow | 18 | 141 | 36 | 220 | 2171 | 23 | 96 |
| sin / cos | 42 / 31 | 35 / 121 | 22 / 46 | 84 / 76 | 252 / 1694 | 27 / 22 | 27 / 90 |
| tanh | 26 | 38 | 22 | 27 | 326 | 27 | 31 |

The cost is almost all `__builtin_fma`: without `-mfma` it is a call to
the C library's `fma` (glibc: an ifunc to the instruction plus the call,
UCRT ~30 ns, mingw-w64 11 ~160 ns and inexact). With the instruction,
CORE-MATH is as fast as glibc or faster for most functions (pow 2x, cos
1.5x slower). The results are the same bits either way: fma is exact.

`xppautX --silent` wall time, median (ms; Linux 7 runs, Windows 5, the
builds run in turn so drift hits them alike; other agents shared the
machine, so +-5% is noise):

| model | Linux before | after | after+fma | MinGW before | after | after+fma | clang64 before | after |
|---|---|---|---|---|---|---|---|---|
| tools/models/heavy.ode (stiff, no libm) | 423 | 426 | 425 | 565 | 576 | 556 | 542 | 544 |
| tools/models/million.ode (10^6 Euler steps, `^`) | 1105 | 1201 | 1120 | 1120 | 3258 | 1095 | 1052 | 1071 |
| docs/studies/w72-stochastic.ode (4*10^6 steps, 2 Wiener, sin/tanh/exp/log/`^`) | 1657 | 3642 | 1927 | 3829 | 35566 | 2159 | 2026 | 2912 |
| examples/ode/lecar.ode | 43 | 44 | 43 | 128 | 130 | 133 | 131 | 130 |
| examples/canonical/cgl.ode | 342 | 299 | 295 | 366 | 380 | 384 | 408 | 412 |

So: a model whose right-hand side is mostly `^` and transcendental calls
pays 2.2x (Linux), 1.4x (UCRT) and 9x (Strawberry MinGW) as built by
default, and 1.16x (Linux) or 0.56x (MinGW: faster than its own libm)
with the FMA instruction. Stiff, linear-algebra-bound or I/O-bound runs
do not notice.

## Size

18 functions, 26 files, 1.2 MB of source (pow.c alone 292 KB, mostly
tables); +416 KB of code in xppautX on Linux (text 3.58 MB to 4.00 MB,
+11.6%).

## What CORE-MATH lacks

- **jn, yn** (the parser's `besselj`, `bessely`): CORE-MATH has no double
  Bessel functions. Left on the C library (xpp_math.h says so): those two
  alone may still differ by platform. No example uses them. A follow-up
  could port fdlibm's e_jn.c (what glibc uses) on top of `xpp::math`'s
  sin/cos/log/sqrt: portable, glibc-accurate, not correctly rounded but
  the same bits everywhere.
- `roundeven` (C23), which CORE-MATH calls and Windows' C libraries lack:
  `third_party/core-math/xpp_roundeven.c` (ours, exact for every double).
- Nothing else: `sqrt`/`fabs`/`floor`/`ceil`/`fmod` are exact by IEEE 754,
  and the core uses no `long double`, `std::complex` math or float
  functions.

## One baseline instead of four?

Locally, yes for Windows: both Windows toolchains now match Linux on all
184 models, so tests/examples.windows.md5 and windows-clang.md5 are
redundant (examples_check.sh's own rule: "a platform that matches Linux
exactly needs none"). macOS is likely (arm64 has no contraction now and no
Apple libm call is left) but only a CI run can say. What can still split
them: jn/yn, a toolchain whose `fma()` is inexact (mingw-w64 11, found
here), and anything a future change adds outside xpp::math (a check could
fail the build when the linked binary imports libm's exp/log/pow/sin...:
`nm -u` on Linux).

## Recommendation

Adopt it, but not as built by default on x86-64: take the FMA cost away
first. The results do not depend on how fma is computed (it is exact), so
build the 18 objects twice -- plain and with `-mfma` (renamed, e.g.
`-Dcr_exp=cr_exp_fma`) -- and pick once at start-up with
`__builtin_cpu_supports("fma")` (every x86-64 CPU since 2013/2015; arm64
always). That keeps the old speed (Linux +0-16% on the worst model, faster
than before on MinGW) and makes the Windows build independent of the
toolchain's `fma()` (Strawberry's is wrong and slow; the release's MSYS2
UCRT64 is right). Without dispatch, adopting it costs up to 2.2x on
Linux and 9x with the local MinGW on libm-heavy models. Then: run CI on a
branch with the three platform baselines removed, and if macOS matches,
keep one tests/examples.md5; port jn/yn if a user needs Bessel functions
reproducible. The 12 Linux md5 changes are the price of the switch, once.
