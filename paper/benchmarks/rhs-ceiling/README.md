# The ceiling of a compiled right-hand side (W249, #305)

Question (maintainer, 2026-10-09): W246 found 84% of xppautX's AUTO time is
the model's right-hand side through the expression interpreter. Before
choosing how to make it fast (a faster built-in evaluator, an embedded
compiler, generated C, an analytic Jacobian), measure what a compiled
right-hand side would give at most.

## Method

- A test build, never merged: the pinned xppautX (88e09741, the paper's
  WSL build, same make command) with `core/my_rhs.cpp` patched by
  `apply_patch.py` to call `hand_rhs.inc`, PY_S1Bf's right-hand side
  written by hand in C++ (what a compiler of the model would emit for
  this model: the same operations in the interpreter's order,
  `ode_divisor` as the .odex writes it, the 21 fixed quantities stored as
  variables so aux output is unchanged). `XPP_HAND_RHS` picks the variant
  at run time:
  - `0`: the interpreter (the stock binary is used for this variant);
  - `1`: hand-written, `xpp::math`'s correctly rounded exp/pow, no FMA:
    the results must be bit-identical to the interpreter's;
  - `2`: hand-written, the C library's exp/pow.
- `ceiling.py` (WSL): one excluded warm-up and five measured runs per
  variant, order rotating, pinned to one CPU:
  - integrate: `--silent` on the bench model at Iapp = 2000 pA, where the
    cell spikes (the model's own settings: stiff, tol 1e-8, 2000 ms,
    200001 rows), CPU time and the output's md5;
  - AUTO: W246's periodic run (`../auto/hopf_xppautx.py`), CPU time and
    the labelled points.
- Results: `results/wsl/` (`runs.json`, `summary.json`, `run.log`).

## Results (2026-10-09)

| Variant | Integration, CPU s | Speed-up | AUTO periodic run, CPU s | Speed-up |
|---|---:|---:|---:|---:|
| interpreter (stock) | 24.38 +/- 0.15 | 1 | 24.03 +/- 0.30 | 1 |
| hand-written, xpp::math | 6.17 +/- 0.10 | **3.95x** | 6.89 +/- 0.07 | **3.49x** |
| hand-written, libm | 5.32 +/- 0.05 | 4.58x | 6.02 +/- 0.05 | 3.99x |

AUTO-07p on the same run (W246): 2.63 s on one thread.

- Compiling the model with the correctly rounded math keeps every result
  bit for bit: the same output md5 (`5d2fcce09937...`) as the interpreter
  in every run, the same 336-point AUTO branch. Reproducibility costs
  nothing to keep.
- Correctly rounded exp/pow cost 14% (integration) and 13% (AUTO) over
  libm once the interpreter is gone; libm changes the output.
- The integration time still includes writing 200001 rows; the right-hand
  side's own gain is larger than 3.95x.
- With a compiled right-hand side xppautX's AUTO is still 2.6x slower than
  AUTO-07p's on one thread: the rest is AUTO2000's own code (see below).

## What is left once the model is compiled

callgrind of the hand-written (xpp::math) build's whole AUTO run, steady
state and periodic (`results/wsl/callgrind-hand-auto.out`); shares of
AUTO's instructions (`do_auto`, 51.9e9):

| Part | Share |
|---|---:|
| the right-hand side (`my_rhs`), called by the finite-difference Jacobians (`funi`: ndim+1 calls, plus the parameter's) | 47% |
| of which correctly rounded `exp` and `pow` (CORE-MATH; three `pow` per call for `m^3`, `n^4`, `aA^3`) | 33% |
| block linear algebra, `brbd` | 35% |
| of which `conpar` (condensation of parameters) | 29% |
| `memset` (AUTO2000 zeroing its work arrays), `funi`'s per-call vectors | 5% |
| the rest (`setubv`'s own loops, step control, output) | ~13% |

(Instruction counts, not time: CORE-MATH runs at a high instructions per
cycle, which is why libm saves only 13% of the time.)

## TinyCC as the compiler (W250, #306; 2026-10-10)

Question (maintainer, 2026-10-10): how much of the ceiling does an embedded
compiler keep, on Linux and Windows? TinyCC (`mob` 43c7708b, 2026-10-03;
LGPL 2.1; built from source in a temp folder, nothing installed:
`./configure && make`, seconds) compiles `tcc_rhs.inc`'s C, the same
operations as `hand_rhs.inc`, in-process (`libtcc`, `TCC_OUTPUT_MEMORY`,
`-nostdlib`), calling xpp::math's exp/pow through `tcc_add_symbol`. The test
build is patched with `apply_patch.py TREE --tcc` and linked with `-ltcc`;
`XPP_HAND_RHS=3` picks it. `ceiling.py --variants interpreter,hand-xppmath,tcc`.

- Compile time of the model: 1.2 ms. libtcc.a 640 KB, the `tcc` program 381 KB.

| WSL (gcc 15), CPU s | Integration | Speed-up | AUTO periodic | Speed-up |
|---|---:|---:|---:|---:|
| interpreter | 24.25 +/- 0.14 | 1 | 24.07 +/- 0.22 | 1 |
| gcc -O2 (hand_rhs.inc) | 6.32 +/- 0.03 | 3.84x | 7.04 +/- 0.07 | 3.42x |
| TinyCC | 9.26 +/- 0.07 | **2.62x** | 9.90 +/- 0.14 | **2.43x** |

| Windows (MSYS2 UCRT gcc 16, libtcc built by it), CPU s | Integration | Speed-up | AUTO periodic | Speed-up |
|---|---:|---:|---:|---:|
| interpreter | 22.81 +/- 0.03 | 1 | 22.52 +/- 0.04 | 1 |
| gcc -O2 (hand_rhs.inc) | 5.66 +/- 0.01 | 4.03x | 6.31 +/- 0.03 | 3.57x |
| TinyCC | 8.88 +/- 0.02 | **2.57x** | 9.43 +/- 0.01 | **2.39x** |

Windows: the same pinned source built natively (`mingw32-make xppautx
BUILDDIR=build/ucrt OPT="-g -O2"`), TinyCC configured with `--cc=gcc` and
`make libtcc.a` (445 KB); `ceiling.py` pins itself to CPU 2 (its programs
inherit the affinity) and reads CPU time with GetProcessTimes. Compile time
0.9 ms.

Every run on both systems has the interpreter's output md5 (`5d2fcce09937`,
the same on Linux and Windows), and the AUTO branch is the same: TinyCC keeps
the results bit for bit (SSE2 doubles, no contraction, no reassociation).
TinyCC keeps about two thirds of gcc's gain on both systems. Results:
`results/wsl-tcc/`, `results/windows-tcc/`.

## What this says about the approach

1. Compiling the model is the largest single gain: 3.5-4x, on integration
   and AUTO alike, and bit-identical results come free with it (the same
   operations, xpp::math, no FMA). Whatever produces the code (generated C
   compiled by gcc/clang, an embedded compiler, a JIT), this is the
   ceiling for this model at gcc -O2; a slower code generator gets a
   fraction of it.
2. After that, AUTO's collocation is a third of the time: W247 (parallel
   `setubv`/`conpar`, what AUTO-07p's OpenMP does) becomes worth it, and a
   compiled right-hand side that writes no Session state is what makes it
   thread-safe. With ~80% of AUTO in those two, 4 threads could give
   ~2.5x on top.
3. An analytic Jacobian (the model differentiated symbolically) would
   replace `funi`'s ndim+2 right-hand-side calls per point; it changes
   AUTO's iterates (no longer bit-identical to XPPAUT's finite
   differences), so it would be an option, not the default.
4. Integer powers (`m^3`) go through the general correctly rounded `pow`;
   a correctly rounded fast path for small integer exponents (exact
   products in double-double, then one rounding) would keep the bits and
   cut most of that cost; it needs a proof or an exhaustive check against
   CORE-MATH's pow.
