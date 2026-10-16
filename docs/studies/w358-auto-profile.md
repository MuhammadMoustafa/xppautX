# W358: where our AUTO's instructions go, and what two other libraries would change

Card W358 Profile our AUTO per point, and comparison builds for its choices
([#420](https://github.com/MuhammadMoustafa/xppautX/issues/420)), W302 stage 2, 2026-10-14. Answers
questions 6, 7 and 9 of [w355-auto07p-differences.md](w355-auto07p-differences.md) and sets the targets of
stage 3 (W359 the Newton step, W360 conpar). Everything below is **instructions counted by callgrind**
(independent of the machine's load) on one thread, at the commit of this card; **time stays W354's**: an
instruction count is not a run time (CORE-MATH runs at a high instructions per cycle:
paper/benchmarks/rhs-ceiling/README.md), and where this study says "price" for correct rounding it says
instructions.

## What was measured

- **Build:** the default one, `-g -O2 -ffp-contract=off`, WSL gcc 15, in its own `BUILDDIR`; the model
  right-hand side is the shipped TinyCC-compiled code (`pure_rhs`, and the unnamed generated functions).
  The program carries no measuring code: `callgrind_control -d/-z` cuts the phases of one valgrind process
  from outside, and call counts are callgrind's.
- **Workloads** (`tools/auto-profile/run_all.sh`; the models of paper/benchmarks, which is git-ignored, so
  only a checkout that has it can rerun the first two):
  - `py_s1bf`: W246/W269's PY_S1Bf (8 states, NTST 100, NCOL 4): the steady-state run to its Hopf point (403
    points), then the periodic branch from it, **cut at 100 points**.
  - `modeldb`: W269's ModelDB 267710 motoneuron (25 states, NTST 100, NCOL 4): the steady-state run (1081
    points), then the periodic branch from the rightmost Hopf point, cut at 20 points (8 for the build
    comparisons: each point costs 4.7 to 9.4 G instructions). Its `opt.inc` `NMAX=10000000` is edited to
    100000 in the temporary copy, because the program now refuses 1e7 as past its allocation maximum; the
    `.odex` files of paper/benchmarks are older than the format and are converted again from the `.ode`.
  - nine `tests/auto` descriptions that continue periodic orbits or compute multipliers (`ode_lecar`,
    `tstauto_brper`, `frcp`, `lamom`, `lamom2`, `lecar`, `lo`, `pp2`, `torx`), run whole.
- "Per point" is per stored diagram point of the phase. Shares are of self cost, so each instruction is
  counted once; "kind" is assigned by routine name (`tools/auto-profile/tables.py`).

## 1. Per point

| workload, phase | points | M instr per point | math (xpp::math) | model RHS | conpar | assembly | block solve | eigen | alloc | Newton iterations (solvbv) | Jacobian builds (setubv) | residual only (setrhs) | funi calls | func calls |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| py_s1bf, periodic | 100 | 126.4 | 17% | 22% | **38%** | 8% | 7% | 0% | 5% | 2.7 | 1.7 | 1.0 | 2149 | 14317 |
| py_s1bf, steady | 403 | 0.43 | 13% | 18% | 0% | 2% | 4% | 4% | 5% | (solvae 1.0) | | | 2.0 | 36 |
| modeldb, periodic | 20 | 4653 | 14% | 22% | **53%** | 2% | 7% | 0% | 0% | 5.0 | 3.1 | 1.9 | 4323 | 68805 |
| modeldb, steady | 1081 | 3.97 | 26% | 41% | 0% | 1% | 8% | 10% | 1% | (solvae) | | | 2.0 | 105 |
| tstauto_lecar | 83 | 2.41 | 10% | 13% | 8% | 17% | 4% | 0% | 13% | 1.7 | 1.3 | 0.4 | 213 | 682 |
| tstauto_brper | 49 | 2.39 | 14% | 9% | 7% | 16% | 4% | 0% | 13% | 1.8 | 1.2 | 0.6 | 213 | 644 |
| tstauto_lamom2 | 60 | 2.34 | 23% | 7% | 7% | 14% | 3% | 0% | 11% | 1.5 | 1.0 | 0.5 | 185 | 556 |
| tstauto_frcp | 103 | 1.79 | 0% | 10% | 19% | 15% | 6% | 0% | 13% | 0.8 | 0.6 | 0.3 | 103 | 453 |
| tstauto_pp2 | 130 | 1.34 | 2% | 9% | 9% | 20% | 5% | 0% | 14% | 1.3 | 0.8 | 0.5 | 148 | 455 |

(`ode_lecar`, `lamom`, `lo`, `torx` give the same picture as their neighbours: `tools/auto-profile/tables.py
--summary`.) The two large problems are the ones whose cost is the periodic run, and there:

- **conpar, the condensation of parameters, is the largest single routine**: 37.8% of the instructions of
  PY_S1Bf's periodic run (47.8 M per point) and 53.0% of ModelDB's (2461 M per point); W355's callgrind of the
  hand-written RHS build had it at 29%, and the compiled RHS and the faster math have made it more of the
  whole. `conpar_process` is one function; its self cost is its whole cost.
- **The right-hand side and the math functions it calls together are 39% (PY_S1Bf) and 36% (ModelDB)**
  (`pure_rhs`, the generated code, `cr_exp_fma` and `cr_pow_fma` at 8.4% each in PY_S1Bf). Of the
  14317 `func` calls per point, 12920 (90%) belong to the 680 derivative `funi` calls of the Jacobian builds:
  each is **2n + 1 + nfpr = 19** evaluations (central differences in the states, forward in the two
  parameters, `core/auto_jacobian.h`); the 1469 plain `funi` calls cost one evaluation each.
- **Newton iterations, Jacobian builds, residual passes:** 2.7 `solvbv` calls per point (1.7 builds a new
  Jacobian, 1.0 only the residual: NWTN 3 chord iterations). A build calls `funi` 800 times (400
  collocation points twice: `setubv_make_aa_bb` with derivatives, `setubv_make_fa` for the residual, W355
  D5); a residual-only iteration 400 times. That is 1760 of the 2149 `funi` calls per point; the remaining
  **about 390 are one more pass over the 400 collocation points** that nothing in the iteration count
  explains, consistent with W355 D4 (a residual pass for the fold direction), though the call counts alone
  do not name its caller.
- **Allocation is small in instructions and large in calls:** 9796 `malloc`s per point (the per-call
  `std::vector`s of `funi`, `setrhs`, `setubv_make_*`, W355 D5) and 20553 `memset`s; 4.6% of the instructions
  in PY_S1Bf, 0.5% in ModelDB, 13% in the small `tests/auto` models (where the Newton solve is short).
- **Eigenvalue routines:** nothing in a periodic run (`flowkm` 0.05 M per point, 0.0%, once per point); in
  the steady-state runs 4% (PY_S1Bf, 8x8) and 10% (ModelDB, 25x25) at every point.
- **Output, files, session** (formatting, compression) is 0.2% of a periodic run: not a target.
- The steady-state runs are 300 to 1000 times cheaper per point than the periodic ones, so they do not
  decide anything.

### What this sets for stage 3

| target | what the numbers support | ceiling in instructions |
|---|---|---|
| **W360 conpar** (W355 D6) | the largest routine of both big problems | up to 38% (PY_S1Bf) and 53% (ModelDB) of the run is spent in it; a faster pivot search and contiguous swaps can remove part of it, not all (the elimination arithmetic stays). It is stage 3's main card |
| **W359 the Newton step** (W355 D4, D5, D7) | the extra residual pass is ~390 cheap plain `funi` calls (one `func` each against 19 for a derivative `funi`); `setubv_make_fa` is 3.1% inclusive; allocation 4.6%; `setrhs` 2.4% | about 8 to 10% of PY_S1Bf's instructions at most, from the removed pass, the second residual pass and the allocation together; ModelDB gains less (allocation 0.5%) |
| *not in stage 3's list* | the Jacobian's 19 evaluations per collocation point are 90% of the RHS calls, and the RHS with the math it calls is 36 to 39% | a structural change (fewer evaluations, a sparse or analytic Jacobian: W252 measured 1.48x on AUTO periodic) is a numbers change, so outside the bit-identical stage |

Reading it together with W355: the 2x of AUTO-07p per point (W355 section 4) is **not** the Newton step's
bookkeeping alone; H5 (different trajectories) is closed here for our side (calls per point above),
H1/H2/H3/H4 are measured: conpar and the RHS are the two big shares, the extra passes and allocation the
small ones. The same counts for AUTO-07p are still to take (W355 H1a, H5 in both programs: W354).

## 2. LIBM=system: the price of correctly rounded math

`make LIBM=system BUILDDIR=build/libm xppautx` builds build/libm/xppautX (never ./xppautX) with `tools/libm-system/libm_system.cpp` in
place of the CORE-MATH objects: it defines the `cr_*` symbols (`core/xpp_math.h` calls them) by forwarding
to glibc's `exp`, `pow`, `sin`, ... . Only under the switch, only with a `BUILDDIR` of its own (the Makefile
refuses `build/obj`), never released; the default build is byte-identical (gates below).

Instructions per stored point, same workloads and points, default against LIBM=system:

| workload, phase | M instr per point, default | LIBM=system | system / default | math functions M per point: default | system |
|---|---:|---:|---:|---:|---:|
| py_s1bf, periodic (100 points) | 126.44 | 120.82 | 0.956 | 21.38 | 15.41 |
| py_s1bf, steady | 0.432 | 0.419 | 0.971 | 0.055 | 0.040 |
| modeldb, periodic (8 points) | 9369.9 | 8865.2 | 0.946 | 1354.2 | 814.5 |
| modeldb, steady | 3.974 | 3.589 | 0.903 | 1.031 | 0.622 |
| tstauto_lamom2 | 2.336 | 2.096 | 0.898 | 0.547 | 0.300 |
| tstauto_lo | 2.267 | 2.131 | 0.940 | 0.322 | 0.181 |
| tstauto_brper | 2.388 | 2.247 | 0.941 | 0.333 | 0.190 |
| tstauto_lecar | 2.411 | 2.377 | 0.986 | 0.252 | 0.216 |
| tstauto_frcp, pp2, torx (hardly any math) | | | 1.00 | | |

**The price of correct rounding is 3 to 10% of the instructions** (4.4% in PY_S1Bf's periodic run, 5.4% in
ModelDB's), the math functions themselves costing 1.4 to 1.8 times glibc's instructions. In time it is
smaller, as W249 found (W249: 13% over libm on AUTO's periodic run with the interpreter gone): the CORE-MATH
FMA copies retire many instructions per cycle. What it buys is the same bits on every system (W159).

**What the numbers do when the math changes** (`tools/auto-profile/diagram_diff.py`, the 29 runnable `tests/auto`
descriptions at 1 thread, against the default build's baselines and AUTO-07p's special points with
`autocompare`'s tolerances):

- AUTO-07p: **0 differences** in both the default and the LIBM=system build, and the largest difference
  as a fraction of its tolerance is the same, 0.298 (`ode_lecar`, `tstauto_lecar`).
- The default build's baselines: every description keeps its number of points; the stored values move by
  up to 1e-16 (most), `ode_lecar` 3.8e-11 in the parameter and 9.5e-7 in the eigenvalues, `tstauto_lecar`
  7.8e-11 and 5.2e-6, `tstauto_pp2` 3.6e-10 in the parameter and 4.1e-4 in the eigenvalues (relative to
  1+|value|): a tiny change in a Newton iterate moves a nearly-defective or fast-varying eigenvalue most.
- The big problems (100 and 8 points): PY_S1Bf's periodic branch moves by at most 3.7e-6 (norm) and the
  parameter not at all; ModelDB's by 1.9e-7 (parameter) and 9.4e-5 (norm), over eight periodic points
  whose Newton paths differ.

So the C library's math gives an answer a continuation user could not tell from ours within AUTO's own
tolerances (EPS 1e-4), is 3 to 10% fewer instructions, and gives different last bits on every system
(issue #211); the default stays correctly rounded.

## 3. LINALG=lapack: LAPACK against EISPACK

`make LINALG=lapack BUILDDIR=build/lapack xppautx` (needs gfortran; it links build/lapack/xppautX): Reference-LAPACK 3.12.1 vendored
verbatim in `third_party/lapack` (80 routines: `DGEEV`, `DGGEV` and what they call, reference BLAS,
`dlamch`; `tools/lapack/extract.py` makes the folder from the tarball and checks its SHA-256). Its adapter
`tools/lapack-adapter/lapack_adapter.cpp` gives `rg`, `qzhes`, `qzit`, `qzval` their old signatures:
`rg` calls `DGEEV`, and `qzval` `DGGEV` (`qzhes`, `qzit` do nothing, `DGGEV` reduces the pencil itself).
`core/eispack.cpp` is compiled with those four names renamed, so everything else in it (the SVD and BLAS
copies) is untouched; the default build is byte-identical and never links LAPACK.

**Accuracy**, against a reference computed outside the program: `tools/auto-profile/dump_matrices.py` (a gdb
script, no code in the program) writes the matrices EISPACK is given at every call (`rg`: the equilibrium
Jacobians and HomCont's; `qz`: flowkm's deflated Floquet pencil, 2728 matrices over 14 workloads),
`build/lapack/eigcompare` (`tools/lapack-adapter/eigcompare.cpp`) runs both libraries on them, and
`tools/auto-profile/eig_accuracy.py` computes the eigenvalues of the same exact doubles with mpmath at 50
digits (B^-1 A for a pencil) and matches each with its nearest. Twelve matrices per workload and kind,
evenly spaced; errors are of the worst eigenvalue, absolute over the Frobenius norm of the matrix (the
backward-stable scale) and relative:

| workload, kind (n) | EISPACK abs/norm, max | rel, max | LAPACK abs/norm, max | rel, max |
|---|---:|---:|---:|---:|
| modeldb, rg (25) | 4.9e-18 | 2.5e-14 | 2.9e-18 | 1.1e-14 |
| **modeldb, qz (24)** | 9.9e-17 | 1.2e-11 | **1.6e-17** | **1.1e-12** |
| py_s1bf, rg (8) | 4.8e-18 | 2.5e-15 | 7.7e-18 | 4.7e-14 |
| py_s1bf, qz (7) | 1.9e-17 | 3.2e-14 | 8.0e-18 | 3.9e-14 |
| tstauto_frcp, rg (4) / qz (3) | 1.7e-17 / 7.9e-17 | 2.6e-16 / 4.1e-16 | 5.8e-17 / 6.8e-17 | 4.4e-16 / 4.7e-16 |
| tstauto_torx, rg (3) / qz (2) | 3.4e-16 / 4.0e-15 | 7.9e-16 / 1.7e-15 | 4.1e-16 / 1.9e-15 | 1.7e-15 / 9.6e-16 |
| the 2x2 and 1x1 models (lecar, lamom, lo, brper, pp2, ...) | at most 6.3e-13 (a 1x1 pencil, both) | | the same | |

The other rows of `tools/auto-profile/eig_accuracy.py`'s output are at 1e-16 to 1e-15 for both. Neither
library failed on any matrix. **The one clear difference is ModelDB's 24x24 Floquet pencil, where LAPACK is
6 times closer (normwise) and 11 times closer (relative)**; everywhere else they are equal to rounding,
EISPACK exact to the reference's 1e-51 on `tstauto_brper`'s and `lamom2`'s 2x2 Jacobians where LAPACK
is at 1.6e-16 (equal on `lamom`, `lo`). Both are 1e-11 or better: seven orders below AUTO's
EPS.

**Diagrams** (the same 29 descriptions, 1 thread): AUTO-07p **0 differences**, the same largest fraction of
tolerance (0.298); against the default baselines the points and parameters agree to 1e-16 in all but
`ode_lecar` (1.5e-11 in the parameter, 1.1e-6 in the eigenvalues), the eigenvalues as sets to 1e-14 or
better except there. One output **depends on the order a library returns the eigenvalues in**: `tstauto_pen`'s
stored `period` column (up to 3.06 relative difference between the builds, parameter and norm identical):
the program picks a complex pair by position, so EISPACK and LAPACK, which order a spectrum differently, pick
different pairs. That is a finding about our AUTO, not about LAPACK (the stored period of a point should
not depend on an eigenvalue ordering); it is not fixed here. PY_S1Bf's and ModelDB's steady branches agree
to 1e-7 in the parameter (their eigenvalue computations differ at the Hopf point).

**Instructions** per stored point, EISPACK against LAPACK (reference BLAS): the periodic runs are
unchanged (eigenvalues are 0.0% of them: 125.79 against 126.44 M per point in PY_S1Bf, 1.000 in every
`tests/auto` model); the steady-state runs, where `rg` runs at every point, are **6% (PY_S1Bf, 0.432 to
0.457 M per point) and 10% (ModelDB, 3.974 to 4.365) more**: `DGEEV` on 25x25 takes about twice EISPACK's
instructions. LAPACK is not faster for AUTO; its speed matters only for a much larger matrix or a
vendor BLAS, which this comparison does not use.

**Decision for the maintainer** (the card says accuracy decides a split and the faster default wins):
there is no speed to gain from LAPACK and the accuracy gain is confined to the multipliers of a large
stiff problem and sits far below AUTO's tolerances; EISPACK stays the default, the LAPACK build stays as a
documented benchmark build (the files are small). If a later problem
shows a Floquet pencil where EISPACK's QZ is not good enough, `DGGEV` is one adapter away.

## Gates

The default build and numbers are unchanged: `make WERROR=1 xppautx test`, `tools/autocompare.py` (all
descriptions at both thread counts) and `tools/examples_check.sh` pass byte for byte with the
switches' code present; each switch builds with `WERROR=1` in its own folder.

## Rerunning it

```
tools/wslrun.sh sh -c 'make -j4 BUILDDIR=build/prof build/prof/xppautX'
tools/wslrun.sh tools/auto-profile/run_all.sh build/prof/xppautX build/pr      # ~10 min + ModelDB ~16
python3 tools/auto-profile/tables.py build/pr --summary                         # or no flag: the full tables
make LIBM=system BUILDDIR=build/libm xppautx      # links build/libm/xppautX; then run_all.sh on it, tables.py --compare
python3 tools/auto-profile/diagram_diff.py $PWD/build/libm/xppautX              # diagrams against the baselines and AUTO-07p
make LINALG=lapack BUILDDIR=build/lapack xppautx build/lapack/eigcompare
python3 tools/auto-profile/profile.py build/prof/xppautX build/mat tstauto_lecar --tool gdb   # the matrices
python tools/auto-profile/eig_accuracy.py DIR                                    # with the Windows Python (mpmath)
```

`tools/auto-profile/` also holds the wrappers (`callgrind_wrap.sh`, `gdb_wrap.sh`) and the gdb script; a
run beyond ten minutes (ModelDB's) is moved to the background by the tool, not by these scripts.

## W370: the rerun after the AUTO cards (2026-10-16)

[W370, #432](https://github.com/MuhammadMoustafa/xppautX/issues/432), core at `6f8609f9`;
all W358 tables above remain the record at W358. The same one-thread, compiled-RHS workflows
were rerun with `WERROR=1`, `-g -O2 -ffp-contract=off`, in WSL's own clone of this worktree.
Toolchain: GCC/GFortran 15.2.0 (Ubuntu 15.2.0-16ubuntu1), Valgrind 3.26.0, gdb 17.1,
WSL2 kernel 6.6.87.2 on x86_64.
Each build has its own folder: `build/prof`, `build/libm`, `build/lapack`. Counts are instructions,
not timings. Each callgrind workload ran once per build, sequentially through `tools/wslrun.sh`.
PY_S1Bf uses 100 periodic points and ModelDB uses run_all.sh's default cut of **20 in every build**.
W358's main profile also used 20, but its library comparisons used eight: those old 8-point costs
are not a matched comparison with this rerun's 20-point costs.

### Instructions per stored point: W358 against W370

W358 did not tabulate an old total for three of the nine descriptions; a dash means unpublished,
not zero. The description totals include their steady and periodic phases, as in W358.

| workload, phase | W370 points | W358 M/point | W370 default M/point |
|---|---:|---:|---:|
| py_s1bf, periodic | 100 | 126.44 | 107.314 |
| py_s1bf, steady | 403 | 0.432 | 0.378 |
| modeldb, periodic | 20 | 4653 | 8945.477 |
| modeldb, steady | 1081 | 3.974 | 3.844 |
| ode_lecar | 79 | - | 1.996 |
| tstauto_brper | 49 | 2.388 | 1.883 |
| tstauto_frcp | 103 | 1.79 | 1.459 |
| tstauto_lamom | 70 | - | 1.322 |
| tstauto_lamom2 | 60 | 2.336 | 1.858 |
| tstauto_lecar | 83 | 2.411 | 1.908 |
| tstauto_lo | 50 | 2.267 | 1.782 |
| tstauto_pp2 | 130 | 1.34 | 1.037 |
| tstauto_torx | 81 | - | 1.429 |

PY_S1Bf costs **15.1% fewer instructions** per periodic point. Its conpar cost falls from 47.8 to
40.465 M/point, allocation/zeroing from 4.6% to 0.4%, and counted `funi` calls from about 2149 to
1077/point, while Newton iterations stay about 2.7. These changes fit
[W360 `556979ef`](https://github.com/MuhammadMoustafa/xppautX/commit/556979efafc6ce27da5918750f1a38782f8dfd6d)'s
physical swaps/cached row maxima and
[W359 `2a146b61`](https://github.com/MuhammadMoustafa/xppautX/commit/2a146b61472bab210fd1f28fca4fd2ab6f1a78f4)'s
removed residual visits, combined Jacobian/residual evaluation and reused work arrays, followed by
[W375 `e3ac7b75`](https://github.com/MuhammadMoustafa/xppautX/commit/e3ac7b759275bb541de2d6d7e6f9f92da4c3e8e3)'s
additional scratch reuse. These are measurements of the combined revision, not an isolated percentage
credited to each card. A zero counted `setrhs` call is not evidence of zero residual work: current
assembly helpers share that work, and optimized call counts need not preserve old wrapper calls.

**ModelDB goes the other way:** its 20-point periodic cost rises **92.3%**, with Newton calls/point
5.0 -> 11.55, Jacobian builds 3.1 -> 7.05 and model evaluations 68805 -> 151748. Conpar's share falls
53.0% -> 46.3%, but its absolute work rises 2461 -> 4142.885 M/point because there are more solves.
[W361 `9c7e8c29`](https://github.com/MuhammadMoustafa/xppautX/commit/9c7e8c29cd844a45811bacb355acc30093da4aab)
changed the mesh and the periodic numerical paths on purpose; this rerun establishes the extra work,
but does not isolate the card responsible. It would be wrong to describe the entire AUTO wave as a
speedup. Proposed follow-up: trace ModelDB's extra Newton/Jacobian work on the same 20-point cut,
across the post-W358 revisions, including W361; distinguish mesh accuracy and branch progress from work per
point before changing an algorithm. No numerical change is made here.

### Current comparison builds

The next two tables come from `tables.py --compare`; default is "this", the named comparison build
is "other". All cuts and stored-point counts match, but a fixed count does not force the same
parameter locations or Newton history. Ratios are work per stored point, not isolated library costs.

**LIBM=system**

| workload, phase | points this / other | M instr per point, this | other | other / this | math functions, M per point: this | other |
|---|---|---:|---:|---:|---:|---:|
| modeldb, periodic | 20 / 20 | 8945.477 | 3812.934 | 0.426 | 1484.555 | 405.773 |
| modeldb, steady | 1081 / 1081 | 3.844 | 3.461 | 0.900 | 1.034 | 0.623 |
| ode_lecar, all | 79 / 79 | 1.996 | 1.963 | 0.984 | 0.230 | 0.198 |
| py_s1bf, periodic | 100 / 100 | 107.314 | 102.481 | 0.955 | 19.774 | 14.260 |
| py_s1bf, steady | 403 / 403 | 0.378 | 0.365 | 0.967 | 0.055 | 0.040 |
| tstauto_brper, all | 49 / 49 | 1.883 | 1.766 | 0.938 | 0.279 | 0.159 |
| tstauto_frcp, all | 103 / 103 | 1.459 | 1.459 | 1.000 | 0.002 | 0.001 |
| tstauto_lamom, all | 70 / 70 | 1.322 | 1.242 | 0.940 | 0.192 | 0.109 |
| tstauto_lamom2, all | 60 / 60 | 1.858 | 1.660 | 0.893 | 0.456 | 0.250 |
| tstauto_lecar, all | 83 / 83 | 1.908 | 1.879 | 0.985 | 0.219 | 0.188 |
| tstauto_lo, all | 50 / 50 | 1.782 | 1.666 | 0.935 | 0.268 | 0.151 |
| tstauto_pp2, all | 130 / 130 | 1.037 | 1.039 | 1.001 | 0.025 | 0.023 |
| tstauto_torx, all | 81 / 81 | 1.429 | 1.431 | 1.002 | 0.002 | 0.001 |

PY_S1Bf's periodic system/default ratio is **0.955**, almost W358's 0.956; its math functions cost
19.774/14.260 M per point. ModelDB's steady ratio is 0.900 (W358: 0.903).

**ModelDB's periodic ratio, 0.426, includes changed continuation work and cannot isolate the cost of
correct rounding.** The system build makes about 5.2 Newton calls and 3.2 Jacobian builds per point, against
11.55 and 7.05 in the default, and evaluates the model about 68945 rather than 151748 times.
Over 20 index-paired points, the continued parameter differs by up to 3.89e-4 and the plotted
ordinate by 1.91e-2, using `|a-b|/(1+|default|)`. These are different continuation locations;
this does not measure an error at a matched solution point. W358's eight-point library comparison
(0.946) neither used this cut nor this current trajectory. The follow-up must locate the first
Newton/step-history divergence and compare a common parameter interval and mesh accuracy before
claiming a library speedup. This rerun cannot assign the new behaviour to one intervening card.
PY_S1Bf keeps its parameter values exactly, with a largest ordinate change of 7.70e-7.

**LINALG=lapack**

| workload, phase | points this / other | M instr per point, this | other | other / this | math functions, M per point: this | other |
|---|---|---:|---:|---:|---:|---:|
| modeldb, periodic | 20 / 20 | 8945.477 | 5584.183 | 0.624 | 1484.555 | 926.588 |
| modeldb, steady | 1081 / 1081 | 3.844 | 4.236 | 1.102 | 1.034 | 1.034 |
| ode_lecar, all | 79 / 79 | 1.996 | 1.997 | 1.001 | 0.230 | 0.230 |
| py_s1bf, periodic | 100 / 100 | 107.314 | 107.324 | 1.000 | 19.774 | 19.777 |
| py_s1bf, steady | 403 / 403 | 0.378 | 0.404 | 1.068 | 0.055 | 0.055 |
| tstauto_brper, all | 49 / 49 | 1.883 | 1.887 | 1.002 | 0.279 | 0.279 |
| tstauto_frcp, all | 103 / 103 | 1.459 | 1.465 | 1.004 | 0.002 | 0.002 |
| tstauto_lamom, all | 70 / 70 | 1.322 | 1.326 | 1.003 | 0.192 | 0.192 |
| tstauto_lamom2, all | 60 / 60 | 1.858 | 1.862 | 1.002 | 0.456 | 0.456 |
| tstauto_lecar, all | 83 / 83 | 1.908 | 1.911 | 1.001 | 0.219 | 0.219 |
| tstauto_lo, all | 50 / 50 | 1.782 | 1.786 | 1.002 | 0.268 | 0.268 |
| tstauto_pp2, all | 130 / 130 | 1.037 | 1.044 | 1.006 | 0.025 | 0.025 |
| tstauto_torx, all | 81 / 81 | 1.429 | 1.440 | 1.008 | 0.002 | 0.002 |

LAPACK now costs 6.8% more instructions in PY_S1Bf's steady phase and 10.2% more in ModelDB's
(W358: about 6% and 10%); PY_S1Bf's periodic cost is unchanged to the displayed ratio, and the nine
small descriptions cost 0.1-0.8% more. **ModelDB's periodic run is the exception: 5584.183 M/point,
37.6% less than EISPACK**, with Newton calls 7.10 rather than 11.55, Jacobian builds 4.40 rather than
7.05 and model evaluations about 94706 rather than 151748. Its math-function cost also falls in
proportion to those model calls (926.588/1484.555 M/point). This is a change in continuation work,
not evidence that the small Floquet eigenvalue calculation accounts for a third of the run.

The large-model comparison uses the existing profile JSON's plotted coordinates, not a saved
full-precision session diagram. PY_S1Bf's periodic parameter values remain identical across builds;
LAPACK's largest ordinate difference is 1.00e-5. On the steady branches the largest parameter drifts
are 0/7.05e-8 for system libm (PY_S1Bf/ModelDB) and 1.04e-7/5.24e-8 for LAPACK, with identical plotted
ordinates. ModelDB's LAPACK periodic parameter/ordinate maxima are 2.16e-4/1.28e-2, again at different
continuation locations. Their twenty-point parameter spans
make the changed step histories concrete (endpoints as printed by the diagram protocol):

| ModelDB periodic build | first parameter | last parameter | stored points | Newton/point | Jacobian/point |
|---|---:|---:|---:|---:|---:|
| default EISPACK | 0.5727871 | 0.5728517 | 20 | 11.55 | 7.05 |
| LIBM=system | 0.5727871 | 0.5734632 | 20 | about 5.2 | about 3.2 |
| LINALG=lapack | 0.5727871 | 0.5731918 | 20 | 7.10 | 4.40 |

LAPACK and system libm both advance farther for fewer instructions in this particular run. W358's
blanket statement that LAPACK brings no instruction gain to AUTO cannot be carried forward without
this exception. A common parameter interval, mesh convergence and the first differing Newton/step
history are needed to decide why, and to turn this into a fair timing comparison.

### Eigenvalue accuracy on the current matrices

The existing gdb dumper and `build/lapack/eigcompare` were used on PY_S1Bf, ModelDB and all 43 runnable
AUTO descriptions, a superset of W358's 14 workloads. They captured **6679 matrices**. The default gdb and callgrind diagram coordinates match exactly in
both phases of both large models (403/100 and 1081/20 points). The four BVP
workflows `ode_tstheti`, `tstauto_dumbvp`, `tstauto_exp` and `tstauto_tsthom2` call neither sampled
entry point and have empty dumps. Windows Python with mpmath 1.4.1, at 50 digits, ran
`eig_accuracy.py DIR --per-file 12`: **684 samples, 57 workload/kind rows, twelve in every row**;
zero library failures and zero infinite pencils in the samples.
As in W358, eigcompare requests eigenvalues only (`matz=0`), even for a matrix captured at a
HomCont eigenvector call; this calculation does not assess eigenvectors or projection bases.

The following errors are maxima of the worst matched eigenvalue in each sampled matrix. The existing
script's scale for a pencil is `sqrt(||A||_F^2 + ||B||_F^2)` (the combined Frobenius norm), not the sum
of norms described in its older docstring; the implementation and W358's calculation use this same
scale. Relative errors retain the script's cutoff `|lambda| > 1e-6 * scale`.

| workload, kind (n) | EISPACK abs/norm max | rel max | LAPACK abs/norm max | rel max |
|---|---:|---:|---:|---:|
| explicit_homoclinic, rg (3) | 6.19e-17 | 1.42e-16 | 7.78e-17 | 1.78e-16 |
| forced_pp_ns, rg (4) | 2.11e-17 | 6.49e-17 | 8.01e-17 | 1.72e-16 |
| forced_pp_ns, qz (3) | 5.25e-16 | 7.77e-16 | 3.84e-16 | 7.65e-16 |
| forced_pp_pd, rg (4) | 2.01e-17 | 6.48e-17 | 8.02e-17 | 1.41e-16 |
| forced_pp_pd, qz (3) | 6.94e-15 | 2.70e-15 | 2.03e-15 | 1.40e-15 |
| henon_rot_lp, rg (4) | 8.34e-17 | 3.70e-15 | 5.72e-16 | 3.87e-14 |
| henon_rot_pd, rg (4) | 4.06e-16 | 8.86e-16 | 1.03e-16 | 6.77e-16 |
| henon_rot_tr, rg (4) | 6.09e-17 | 7.64e-17 | 1.23e-16 | 2.40e-16 |
| hill_cycles_pd, rg (4) | 2.11e-17 | 7.56e-17 | 4.37e-17 | 1.66e-16 |
| hill_cycles_pd, qz (3) | 1.10e-15 | 1.42e-15 | 5.90e-16 | 5.30e-16 |
| hill_cycles_torus, rg (4) | 3.07e-17 | 8.69e-17 | 5.23e-17 | 1.35e-16 |
| hill_cycles_torus, qz (3) | 4.52e-16 | 3.85e-16 | 2.76e-16 | 2.85e-16 |
| lorenz_homoclinic, rg (3) | 1.35e-16 | 2.81e-16 | 1.07e-16 | 2.17e-16 |
| modeldb, rg (25) | 4.90e-18 | 8.43e-14 | 2.93e-18 | 1.12e-14 |
| modeldb, qz (24) | 4.05e-17 | 8.50e-12 | 1.25e-17 | 1.78e-12 |
| ode_del_log, rg (2) | 7.85e-17 | 1.35e-16 | 1.44e-16 | 2.46e-16 |
| ode_lecar, rg (2) | 1.69e-16 | 8.30e-16 | 1.04e-16 | 7.08e-16 |
| ode_lecar, qz (1) | 1.00e-12 | 9.65e-17 | 1.00e-12 | 9.65e-17 |
| ode_osc, rg (2) | 1.19e-16 | 3.35e-16 | 1.25e-16 | 3.50e-16 |
| py_s1bf, rg (8) | 7.45e-18 | 2.27e-15 | 6.29e-18 | 4.03e-14 |
| py_s1bf, qz (7) | 1.96e-17 | 2.17e-14 | 2.44e-17 | 2.32e-14 |
| res_cycles, rg (4) | 2.33e-17 | 5.65e-17 | 9.51e-17 | 2.43e-16 |
| res_cycles, qz (3) | 9.44e-16 | 6.84e-16 | 1.39e-15 | 7.31e-16 |
| tstauto_ab, rg (2) | 1.25e-16 | 2.65e-15 | 2.73e-16 | 1.37e-14 |
| tstauto_ab_fold, rg (2) | 3.80e-16 | 4.00e-11 | 1.23e-16 | 5.54e-11 |
| tstauto_ab_hopf, rg (2) | 7.10e-17 | 2.43e-15 | 7.10e-17 | 2.43e-15 |
| tstauto_bigaut, rg (350) | 0.00e+00 | 0.00e+00 | 0.00e+00 | 0.00e+00 |
| tstauto_bistable, rg (2) | 9.01e-17 | 2.10e-16 | 9.01e-17 | 2.10e-16 |
| tstauto_bob, rg (2) | 4.79e-17 | 1.09e-16 | 6.48e-17 | 1.70e-16 |
| tstauto_brper, rg (2) | 1.91e-51 | 2.70e-51 | 1.57e-16 | 2.22e-16 |
| tstauto_brper, qz (1) | 3.33e-17 | 8.07e-17 | 3.33e-17 | 8.07e-17 |
| tstauto_dumb, rg (195) | 0.00e+00 | 0.00e+00 | 0.00e+00 | 0.00e+00 |
| tstauto_eiz-hom, rg (5) | 8.81e-16 | 1.33e-15 | 8.75e-16 | 5.53e-15 |
| tstauto_frcp, rg (4) | 1.68e-17 | 2.61e-16 | 5.79e-17 | 4.38e-16 |
| tstauto_frcp, qz (3) | 1.08e-16 | 3.73e-16 | 1.29e-16 | 4.59e-16 |
| tstauto_lamom, rg (2) | 1.93e-51 | 2.73e-51 | 1.93e-51 | 2.73e-51 |
| tstauto_lamom, qz (1) | 3.86e-17 | 6.36e-17 | 3.86e-17 | 6.36e-17 |
| tstauto_lamom2, rg (2) | 2.05e-51 | 2.90e-51 | 1.57e-16 | 2.22e-16 |
| tstauto_lamom2, qz (1) | 1.21e-16 | 9.98e-17 | 1.21e-16 | 9.98e-17 |
| tstauto_lecar, rg (2) | 2.08e-16 | 1.25e-15 | 2.64e-16 | 1.01e-15 |
| tstauto_lecar, qz (1) | 9.36e-13 | 5.37e-17 | 9.36e-13 | 5.37e-17 |
| tstauto_lo, rg (2) | 1.90e-51 | 2.68e-51 | 1.90e-51 | 2.68e-51 |
| tstauto_lo, qz (1) | 1.58e-17 | 8.90e-17 | 1.58e-17 | 8.90e-17 |
| tstauto_pen, rg (4) | 4.02e-16 | 7.55e-16 | 5.40e-16 | 1.01e-15 |
| tstauto_pf, rg (1) | 0.00e+00 | 0.00e+00 | 0.00e+00 | 0.00e+00 |
| tstauto_pp2, rg (2) | 7.47e-17 | 1.92e-16 | 5.71e-17 | 1.73e-16 |
| tstauto_pp2, qz (1) | 2.96e-17 | 7.24e-17 | 2.96e-17 | 7.24e-17 |
| tstauto_san, rg (3) | 4.15e-17 | 8.67e-17 | 4.15e-17 | 8.67e-17 |
| tstauto_she, rg (5) | 1.25e-51 | 1.34e-50 | 1.25e-51 | 1.34e-50 |
| tstauto_simplefold, rg (1) | 0.00e+00 | 0.00e+00 | 0.00e+00 | 0.00e+00 |
| tstauto_torx, rg (3) | 3.37e-16 | 7.94e-16 | 4.06e-16 | 1.69e-15 |
| tstauto_torx, qz (2) | 4.25e-15 | 1.67e-15 | 2.03e-15 | 1.07e-15 |
| tstauto_torx_homcont, rg (3) | 5.74e-16 | 2.02e-15 | 4.17e-16 | 1.47e-15 |
| tstauto_tstheti_homcont, rg (2) | 7.37e-17 | 1.93e-16 | 8.13e-17 | 2.48e-16 |
| tstauto_tsthom, rg (2) | 1.42e-16 | 2.68e-16 | 1.42e-16 | 2.68e-16 |
| tstauto_wcbiwave, rg (3) | 1.15e-15 | 2.30e-15 | 6.28e-16 | 1.15e-15 |
| zero_hopf, rg (3) | 1.89e-51 | 2.74e-51 | 1.89e-51 | 2.74e-51 |

ModelDB's 24x24 pencil remains the case where LAPACK helps: W358 EISPACK/LAPACK abs/norm
9.9e-17/1.6e-17, relative 1.2e-11/1.1e-12; now **4.05e-17/1.25e-17** and
**8.50e-12/1.78e-12**, about 3.2x and 4.8x closer. The matrices and samples changed with the
numerical paths, so this is not an accuracy regression test on identical old inputs. PY_S1Bf's pencil
remains at rounding scale (EISPACK/LAPACK abs/norm 1.96e-17/2.44e-17).

Across the expanded sample the largest relative error is **5.54e-11** (`tstauto_ab_fold`, LAPACK),
still over six orders below 1e-4; the largest abs/norm is **1.00e-12**, the same in both libraries on
`ode_lecar`'s 1x1 pencil (W358's small-model maximum was 6.3e-13). Neither supports changing libraries.
The diagonal 350- and 195-variable fixtures are exact in both libraries; those are not evidence
about large dense matrices. These are sampled input-matrix errors, not a proof about all AUTO runs.

### Diagrams and the coordinator's wave checks

`diagram_diff.py` ran every current runnable description, **43 per build**, at one thread, using the
committed EISPACK baseline and the existing AUTO-07p references/tolerances. Value differences are
`|a-b|/(1+|baseline|)`, with spectra compared as sets; descriptions and runs with
explicit reference skips retain them. Both comparison builds report **zero AUTO-07p differences**.
W358 checked 29 descriptions with a largest tolerance fraction of 0.298; now both builds' maximum is
**0.788** (`tstauto_torx_homcont`), inside the unchanged tolerance.

| checkpoint | coordinator after wave 1, `5337db7c` | W370 LAPACK | W370 LIBM=system |
|---|---:|---:|---:|
| forced_pp_ns, points this/default | 264/263 | 264/263 | 266/263 |
| forced_pp_ns, largest fraction of tolerance | 0.554 | 0.554 | 0.400 |
| forced_pp_pd, points | 463 | 463 | 463 |
| forced_pp_pd, largest fraction of tolerance | 0.618 | 0.618 | 0.049 |
| tstauto_torx_homcont, points | 88 | 88 | 88 |
| tstauto_torx_homcont, largest fraction of tolerance | 0.788 | 0.788 | 0.788 |

These reproduce the LAPACK checkpoints in #432's saved coordinator comments. Before that wave,
forced_pp_ns stored 267/268 points; the wave containing W406's step reduction changed its termination
counts. It still ends near the double -1 resonance, where last-bit changes can change a step.
With different point counts, diagram_diff.py's index-paired parameter/spectrum maxima after the first
divergence compare different steps, not the same solution point. The independent special-point/curve
comparison still passes. No tolerance or baseline was changed to obtain these results.

For LAPACK, the familiar spectrum differences remain: ode_lecar 3.11e-6, ab_hopf 1.16e-6,
forced_pp_pd 6.68e-6 (relative to 1+|value|). LIBM=system gives ode_lecar 3.89e-6,
tstauto_lecar 1.65e-6, forced_pp_pd 4.22e-5 and ab_fold 6.75e-5. The 0.788 HomCont fraction also
belongs to the default, following
[W365 `0557526e`](https://github.com/MuhammadMoustafa/xppautX/commit/0557526e8c13c78b6567ac9f2aad1c2221df0db0)'s
projection change; it is not a loss caused by LAPACK.

**The period-order finding no longer holds.**
[W369 `29a87725`](https://github.com/MuhammadMoustafa/xppautX/commit/29a87725a8b9e1c5d659e075a9c4a839be15de68)
already selected the pair by an order-free rule, with lowest frequency resolving ties. On tstauto_pen,
LAPACK and EISPACK now differ by **9.74e-16** in stored period (W358: 3.06), with identical parameter,
norm and state values; LIBM=system's period is identical. No further period-order card is proposed.

### Tool maintenance and gates

Searched the existing `tools/auto-profile/` classifier/call-prefix functions and the numerical owners
`core/xpp_math_auto.cpp`, `core/xpp_math_eispack.cpp`, conpar and autlib before changing the tool.
[W337 `efe265e8`](https://github.com/MuhammadMoustafa/xppautX/commit/efe265e8ec574c6ea9b772b554b540e874da7e28)
moved numerical functions into `xpp::`; the old matchers lost their eigenvalue/linear-solve categories
and counts. W370's
[b03bcfbf](https://github.com/MuhammadMoustafa/xppautX/commit/b03bcfbf0394df7504f8540ede600c3c5490b7f8)
makes five existing expressions ignore that owner prefix. It reuses the
classifier and counters, changes no instruction total, reader, file/process operation or core source,
and adds no helper. The simpler total-only report was rejected because it would lose the attribution
this card requests. This is the only measuring-tool change.

- Three WSL benchmark builds succeed with `-j4 WERROR=1`; build logs contain no compiler warnings.
- Default `make -j4 WERROR=1 xppautx test`: **57 unit programs pass**, with 69517 printed checks.
- Examples: **185 models, 184/184 expected outputs** match the unchanged md5 baseline; ml-range has no
  output by design.
- AUTO: **52 descriptions, 9 documented model skips, 9972 points, 126 checks, 0 failures**, both thread
  counts, against unchanged diagram baselines and references. Explicit run/reference skips remain.
- `tools/sourcecheck.sh`: **pass**, including LTO, dead-code, global, duplication, names and schema
  checks; `OMP_NUM_THREADS=4 OMP_THREAD_LIMIT=4` makes its `nproc` four, keeping its builds at four jobs.
- web2 typecheck: **pass** on this worktree's src/test, using TypeScript 7.0.2 and existing dependencies
  in the main checkout. Both package-lock hashes are identical; installed preact 10.29.8, uplot 1.6.32,
  marked 18.0.14 and @types/node 22.20.4 match. A temporary config only redirects dependency resolution;
  nothing is installed and no filesystem link is created.
- Comparison diagrams: **43 descriptions per build, zero AUTO-07p differences**; matrix accuracy:
  **684 samples, zero failures**. verify.sh, the full web2check and asancheck were not run, as instructed.

### Decision and the maintainer's next choice

**Keep EISPACK as the default and LAPACK as the documented benchmark build, with a qualification to
the old performance rationale.** The accuracy results do not require a split; all current reference
comparisons pass, and LAPACK still costs more in the steady and small-model runs. The new ModelDB
periodic exception is real in instructions and branch progress, so "LAPACK is never faster" is not
supported by this rerun. These are instruction counts on different adaptive trajectories, not
matched-interval wall-time measurements or a reason to choose a default for favourable last bits.
Correctly rounded math remains the project default; the system-libm outlier likewise is not an
isolated price for correct rounding.

The maintainer should decide on a focused follow-up card before using the ModelDB result in a speed
claim: **trace ModelDB's Newton/step-history sensitivity after the AUTO cards**, reproduce the first
divergence among the three builds, compare the same parameter interval and independently check mesh
accuracy. It must account for the default's 92.3% increase since W358 and the comparison builds'
fewer iterations and farther progress. This card measures and reports that finding; it makes no
core, tolerance or baseline change. The 0.788 HomCont reference fraction remains a checkpoint for
later waves, and W369's period-order issue needs no new card.

Raw callgrind profiles, phase JSONs, matrix dumps/results and temporary run reports were removed
after recording these tables: **none left**. Ordinary compiler build caches stay in the WSL clone,
as tools/wslrun.sh normally keeps them. No background run was started by this card.
