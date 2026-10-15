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
