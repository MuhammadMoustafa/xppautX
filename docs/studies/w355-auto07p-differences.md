# W355: what AUTO-07p does differently from our AUTO

Card W355 What AUTO-07p does differently from our AUTO: define W302's next stages
([#417](https://github.com/MuhammadMoustafa/xppautX/issues/417)), 2026-10-14. Read-only study: nothing was
built, run, profiled or timed. Every number about speed is either quoted from an existing measurement
(paper/BENCHMARK_HANDOFF.md 7c-7f, paper/benchmarks/rhs-ceiling/README.md, auto-parallel/README.md) or
marked *inference*. Line numbers are of the current tree (master `9adbf978`).

Sources: ours `core/autlib1..5.cpp`, `setubv2.cpp`, `conpar2.cpp`, `auto_*.{cpp,h}`; AUTO-07p
`reference/auto-07p/src/*.f90` (the local tree is **0.9.4**, git `44cc1c4`, 2025-06-26; W300's references
were made with **0.9.3**, docs/auto-references.md: CHANGELOG 0.9.4 lists one numerical change, "mesh: use
closest point for linear time interpolation", CHANGELOG:11-12, so the 46 compared points should not
move, unverified); AUTO2000 as XPPAUT 8.0 carries it, `reference/xppaut-8.0/autlib*.c`, the ancestor of
ours. Our code is AUTO2000's C with xppautX's glue; where ours and XPPAUT 8.0 agree the line in 8.0 is given
once only if it matters.

## 1. What the two codes share (so it is not a difference)

Pseudo-arclength predictor/corrector with the secant tangent (ours `contae` autlib1.cpp:1275, `contbv`
:4905; 07p `SOLVAE` ae.f90:435, `CONTBV` bvp.f90:365); the same step control (`adptds` autlib1.cpp:3370,
`ADPTDS` mesh.f90:224: x2 / x1.5 / x1.1 / x0.5 by Newton count, clamped to DSMAX); the same Newton
convergence and divergence tests (`delref = 20 * first` and `NWTN` chord iterations: ours stepbv
:5098-5250, 07p `NEWTONBV` bvp.f90:558); the same finite-difference Jacobian of the user function
(central in the states with step `HMACH*(1+max|u|)`, forward in the parameters: ours `funi`
autlib3.cpp:3231 + auto_jacobian.h, 07p `FUNI` interfaces.f90:60-138); the same equidistribution function
(`eqdf` :3696, `EQDF` mesh.f90:480) and complete pivoting in the condensation of parameters and the
reduction; the same AE branch switching (`swprc` :2174, `STEPAE(...,SW)`); the same bisection-like
location by secant then Mueller with `EPSS` (the structure of `lcspae` :1563, `LCSPAE` ae.f90:624).
That is why W300 finds every untraced point in agreement.

## 2. Differences, one by one

Effect columns: **N** = effect on our numbers (models of tests/auto/ and quantities, against W300's tolerance
formula, docs/auto-references.md "The tolerance"); **T** = effect on time per point; **C** = cost to adopt.
"07p never worse" means the change must not raise any entry of W300's table.

### D1. Mueller's method (special-point location)

- Ours `mueller` autlib1.cpp:1682-1718: `r = sqrt(c*c - q/b)` (:1701) and `rds = -c ± r`; a negative
  discriminant gives NaN, and `-c+r` loses digits when `c*c >> q/b`. 07p `MUELLER` support.f90:31-61:
  `R = SQRT(A**2-4*B*Q)` only if positive (else 0, the vertex), `RDS = -2*Q/(A+R)`, no cancellation
  (CHANGELOG:326-327 "avoid loss of significance").
- 07p also adds the retry-corrected abscissa: `DSTEST = S1 + RDS` *after* `STEPAE`, which may have halved
  `RDS` (bvp.f90:1405, ae.f90:743); ours computes `s = s1 + rds` *before* the solve (autlib1.cpp:1630,
  :5915) so a retried step feeds Mueller a wrong abscissa (CHANGELOG:324-326).
- **N:** every located special point (HB, LP, BP, PD, TR, UZ) moves by up to the `EPSS*(1+DSMAX)` position
  error W300 already allows; the models with located points: ode_lecar, tstauto_lecar, ode_lamom..., lamom2,
  lo, torx, pp2, frcp, ab, pf, san, simplefold, brper. Agreement with 07p can only improve (same formula)
  but the largest entries (lecar HB 5.5e-5 in the parameter, 1.7e-3 in the period) are location error, so
  they should shrink; *inference*. **T:** none per point; saves rare failed locations. **C:** small (two
  routines, both lcsp* callers); risk: baseline rewrite for ~14 descriptions.
- Also an XPPAUT finding (NaN from a negative discriminant, [autlib1.c:1714](../../reference/xppaut-8.0/autlib1.c#L1714));
  the evidence needs a case that reaches it, not yet found.

### D2. Special-point test bookkeeping

- 07p `CNRLAE`/`CNRLBV` (ae.f90:187-219, bvp.f90:279-307) run all tests through one loop and, when a locator
  moved the solution (`STEPPED`, `ISTEPPED`), **recompute the test functions of the earlier tests at the new
  state** before the next step. Ours (cnrlae autlib1.cpp:1020-1085, cnrlbv :4804-4885) calls each `lcsp*`
  in turn and keeps each test's `q` from wherever its own location stopped, so after a UZ or fold location
  the next tests compare their old value with a value at the displaced point. CHANGELOG:317-323 describes it.
- 07p skips the location when the sign change is already inside `EPSS`:
  `ABS((1+HMACH)*Q1*DSTEST) < EPSS*(1+SQRT(ABS(DS*DSMAX)))*ABS(Q0-Q1)` (ae.f90:679, bvp.f90:1309): one
  corrector solve fewer per special point; ours always starts the secant (autlib1.cpp:1607-1633, :5889-5915).
- 07p's "first come, first served" rules between coincident tests (ae.f90:683-692), ITPST-dependent
  stop counts (support.f90:587-640).
- **N:** only runs where two events fall in one step (lecar, pp2 cycles: W326's traced entries are
  exactly this region); a label can appear or disappear, hence the baseline rewrite *names models*; agreement
  with 07p improves where it is a mismatch today (`pp2` 4 traced points). **T:** none. **C:** medium
  (control flow in two long routines; collides with W329, §5).

### D3. Stability-based suppression and the multiplier count

- 07p labels PD/TR/HB/BP only when the stability count changes ("`HB0`", CHANGELOG:292-295; equilibrium.f90:
  613-632, periodic.f90:2112-2118), counts multipliers inside the circle with `|mu| <= 1 + 1e-5`
  (periodic.f90:2023 `tol`), keeps conjugate pairs together in the sort (periodic.f90:1960-1983), and
  switches PD/TR/Rn detection off for 2-dimensional systems (CHANGELOG:239-241). Ours `fnspbv` autlib1.cpp:
  6100-6275 counts with `z_abs(ev) <= 1.` (:6234) and sorts by selection; the Hopf test `fnhbae` :1822 asks
  for a change of the eigenvalue count (`nins1 != nins`, with HMACH tolerance only for `isw == 2`).
- **N:** a neutral count at a multiplier on the circle (lecar, lamom, lo cycles) can change a PD/TR label
  or its stability flag; most points unchanged. **T:** none. **C:** small.

### D4. The fold test function for cycles (and where the direction vector is solved)

- 07p computes the fold direction **once, in `NEWTONBV`**, from the factorization of the converged Newton
  step with `NLLV = -1` and `IFST = 0`, and `SOLVBV` then skips `SETUBV` altogether for it
  (`IF(NLLV>=0.OR.IFST==1) CALL SETUBV`, solvebv.f90:158). Ours `fnlpbv` autlib1.cpp:5974-6040 calls
  `solvbv(ifst 0, nllv -1)` at every test evaluation, and `solvbv` with `ifst == 0` always runs `setrhs`
  (autlib2.cpp:125-135), a full residual pass (one RHS call per collocation point, boundary and integral
  conditions) that `nllv == -1` throws away (the right-hand side of that solve is zero except its last
  entry).
- **N:** none (same linear solve; bit-identical expected, *inference*). **T:** one RHS pass of the
  `ntst*ncol` points per step and per location iteration, about 1/(2n+nfpr+2) of a Jacobian build, ~3-5%
  of a step's RHS calls (*inference*, n=8). **C:** small, but inside solvbv, which W334 changes (§5).

### D5. One pass for Jacobian and residual; work arrays

- 07p `SBVFUN` (solvebv.f90:426-481) calls `FUNI` once per collocation point with `IJAC = 2*IFST` and
  fills Jacobian and residual from it. Ours does two passes on a Jacobian iteration: `setubv_make_aa_bb`
  (setubv2.cpp:59-150, `funi(...,2,...)`, whose `f` is dropped) then `setubv_make_fa` (the residual,
  `funi(...,0,...)`, W253/finding #39): one extra RHS call per point = 1 of `2n+nfpr+2`.
- 07p keeps A, B, C, D, A1... as `SAVE`d arrays reallocated only when the shape changes
  (solvebv.f90:112-142). Ours `solvbv` autlib2.cpp:71-110 `assign()`s ~15 vectors (zero-filled) on every
  Jacobian iteration, `setrhs`/`setubv_make_*`/`funi` allocate 20+ `std::vector`s per call (autlib2.cpp:201-230,
  setubv2.cpp:67-70, autlib3.cpp:3245-3248). The rhs-ceiling callgrind puts "memset and funi's per-call
  vectors" at 5% of instructions on the gcc-RHS build (rhs-ceiling/README.md:55-70).
- **N:** none (same values, same order: bit-identical). **T:** ~5% (RHS count) + up to ~5% (allocation),
  *inference*. **C:** small to medium; the same functions as D4 and W334.

### D6. The condensation of parameters (`conpar`) and the layout of the blocks

- Ours `conpar_process` conpar2.cpp:~75-165 searches the full pivot square of the remaining submatrix at
  every elimination step through two index arrays (`irf`, `icf`: `a[icf[k2]-1 + a_dim1*(irf[k1]-1 + ...)]`,
  :90-105): O((nra-irp)(nca-ic)) indirect loads per step, and the elimination loops index through `icf`.
  07p `CONPAR` solvebv.f90:777-878 caches the column of each row's maximum (`IAMAX`, kept up to date inside
  the elimination by `IMSBRA`, :880), so a pivot search is O(nra), swaps rows and columns **physically**
  (contiguous, stride-1 inner loops that vectorize) and keeps `ICF/IRF` only as records. The pivot rule
  (largest |a| by rows in order, first maximum) is the same, so the pivots should be too; *inference* until
  tested.
- rhs-ceiling/README.md:55-70 (callgrind, hand-written RHS, instructions): **conpar 29%**, brbd 35% in all;
  with a compiled RHS `brbd` is the second share after the RHS. 
- **N:** none if pivots and operation order per element are kept (bit-identical; the pivot ties are the risk).
  **T:** the pivot search is roughly a third of conpar's work for n=8, ncol=4 (*inference*: ~240 indirect
  loads per step against ~440 multiply-adds): conpar 1.3-1.6x, ~8-12% of a step; more for larger models.
  **C:** medium; the arglist and interval threading of W247 (conpar2.cpp:1-60, 168-206) must stay.
- Not adopted: 07p's nested-dissection `REDUCE`/`REDUCER` (solvebv.f90:1031-1155, "results independent of the
  number of processors", CHANGELOG:488-490): the serial flop count is the same, the order differs (every
  periodic diagram would change in the last bits), and our threads already cover setubv/conpar.

### D7. The cost of a Newton iteration seen from outside the numerics

- Our extra layers per accepted point: `pvli` (multipliers via `flowkm` on every converged iterate,
  including each location iterate), `auto_stability_*`, `xpp::byeauto_` at every Newton iteration
  (autlib1.cpp:5178), `all_finite` scans, diagram/data events, fort.7/8/9 text through `xpp::print`
  (autlib1.cpp:4040-4110 even prints every pivot of `ge`, **unconditionally**: `Pivots in GE` :4057, as XPPAUT
  8.0 does, [autlib1.c:4295](../../reference/xppaut-8.0/autlib1.c#L4295); 07p prints nothing unless a pivot is tiny,
  support.f90:382-390). `ge` is the Newton solve of the equilibrium branch (solvae :1437) and of every test
  function of AE runs.
- **N:** none. **T:** for equilibrium runs of small systems the pivot formatting is a visible share of a step
  (*inference*; for the periodic bench `ge` only solves the reduced system and `fnbpbv`'s P1 determinant:
  negligible). **C:** small (gate the print by `iid`, as the other diagnostics are).

### D8. Floquet multipliers: SVD and QZ

- 07p `FLOWKM` floquet.f90:64-250: LAPACK `DGESVJ` (Jacobi SVD) for the null vector, `DGGEV` (balancing
  + QZ) for the deflated pencil, infinite multipliers marked `HUGE`. Ours `flowkm` autlib4.cpp:67-420:
  Demmel-Kahan `ezsvd` (tolerance 1e-16, :218) and EISPACK `qzhes/qzit/qzval` without balancing (:334-352);
  an added `deflate` switch (W341) for collapsed orbits.
- Equilibrium eigenvalues: 07p `RG` is a wrapper around LAPACK `DGEEV` (support.f90:1064); ours EISPACK
  `rg` (`eig` autlib1.cpp:3796). Both balance; accuracy equal to ~1e-14 relative.
- **N:** only multipliers far from 1 on badly scaled pencils (near-homoclinic cycles: pp2 past period 60,
  lecar to 1e6); W300 compares spectra within the finite-difference tolerance and finds no
  disagreement outside the traced regimes, so no measurable gain is expected today. **T:** negligible.
  **C:** large (vendor and port LAPACK pieces; W337 owns the numerical API).

### D9. Mesh adaptation

- `eqdf`/`EQDF` compute the same function. Differences are roundoff-level or marginal: 07p's `NEWMSH`
  (mesh.f90:395-448) interpolates from the closer end with `DEQF` (CHANGELOG:236-244, "avoids an addition
  followed by a subtraction"); ours `newmsh` autlib1.cpp:3576 divides by `eqf[j]-eqf[j-1]`; 07p treats
  `IPS == 12` as periodic too (bvp.f90:301 vs ours autlib1.cpp:3465, `ips == 2`); 07p 0.9.4 interpolates
  the time-like variable from the closest point (CHANGELOG:11-12).
- **N:** the mesh differs in the last bits, so every periodic diagram of the baseline changes in low bits
  (the `.diagram.csv` is byte-exact), the agreement with 07p does not change measurably. **T:** none.
  **C:** small; only worth doing in a stage that rewrites the periodic baseline anyway.

### D10. Two-parameter systems (LP, HB, PD, TR curves)

- **Hopf curve system size.** Ours `ffhb` autlib3.cpp:692-735: `ndim = 3*ndm+2` (u, two complex-eigenvector
  parts, `omega`, p2). 07p `FFHB` equilibrium.f90:190-222 (Kuznetsov (10.80)): `2*ndm+2` (u, v, `kappa`, p2),
  a symmetric phase condition, and BT detectable on the curve (CHANGELOG:365-371).
- **Structured Jacobian.** 07p `FNLPF` (toolboxae.f90:171-265) fills the blocks that are exactly the
  user's `DFDU` (copy, `2*U(NDM+1:)` row) and differences only the second-derivative columns (NDM+1
  columns, central). Ours `auto_jacobian::problem` (autlib3.cpp:25-35, :684) differences the whole
  augmented function in all `2*ndm+1` (or `3*ndm+2`) variables, each evaluation calling `funi(ijac 1)` which
  is itself a finite difference: a difference of differences everywhere. CHANGELOG:372-376.
- **N:** the curves' points move by the Newton tolerance and the step sequence changes; **no 07p reference
  exists for two-parameter curves** in tests/auto/ (all 37 descriptions' kinds are `equilibrium`, `periodic`,
  `bvp`, `heteroclinic`; W300 table), so agreement cannot be claimed or checked until one is added.
  **T:** Newton Jacobian of a Hopf curve ~1.5-2x fewer RHS calls (*inference*: columns 2n+2 vs 3n+2, and
  structure). **C:** large (new unknown layout for Hopf start/restart files, `stpnhb`, the AUTO forms).

### D11. Codimension-2 and secondary-bifurcation detection (new labels)

- 07p detects BT, ZH, GH (Bautin, with the first Lyapunov coefficient), CP on HB/LP curves and R1-R4, LPD,
  LTR, PTR, TTR on cycle and map curves (support.f90:23-26 `ATYPES`, :538 `CHECKSP`, equilibrium.f90:
  593-826, toolboxae.f90:807-855, periodic.f90:2091-2210), maps with PD/TR (maps.f90), and stop counts
  (`STOP`, support.f90:587). Ours has HB, BP, LP, PD, TR, UZ, MX, EP only (no such code in autlib1/3/5;
  diagram.h:33 enumerates four point kinds).
- **N:** no existing entry changes; it adds labels. **T:** one extra test per step on 2-parameter
  curves. **C:** large, and a protocol/UI change (new `ty` codes, W329/W330 types first).

### D12. HomCont

- Stable/unstable projection: 07p `PRJCTN` homcont.f90:2151-2290 orders the real Schur form with LAPACK
  `DGEES`/`DTRSEN` and takes exactly `NSTAB`/`NUNSTAB` vectors even when the eigenvalues are not all of
  the expected sign, and reverses time for `PAR(11) < 0`; ours `prjctn` autlib5.cpp:1377-1545 uses
  `orthes/ortran/hqr3lc` with a `compzero` threshold (:1467). Eigen-decomposition: 07p `EIGHO` `DGEEV`
  (homcont.f90:2005), ours `eighi/eigho` EISPACK (autlib5.cpp:1188-1364). Analytic derivatives where the
  user gives them (CHANGELOG:372-376), `NSTAB`/`NUNSTAB` found automatically (CHANGELOG:860-870).
- **N:** none measurable now: W343/W345's only HomCont fixtures have no 07p reference (tstheti's constraints
  are deprecated zero functions; the planar companion was not run in 07p, docs/auto-references.md). Benefit
  is robustness near neutral saddles. **T:** small. **C:** large, and needs a valid 07p homoclinic reference first.

### D13. Linear algebra routines

- `ge` autlib1.cpp:3991 (complete pivoting via `ir/ic` indices, determinant of the permuted product, prints
  pivots) vs `GELI` support.f90:317-470 (physical swaps, `SCALE` option, guards a pivot below
  `||A||max/HUGE` by replacing it, so a singular system does not converge instead of producing NaN,
  CHANGELOG:432-435); same for `nlvc` :3846 vs `NULLVC` :120-228. The pivot guard is exactly what finding
  #50 (NaN accepted as converged) lacks upstream; ours since W343 stops on non-finite values itself.
- **N:** none expected. **T:** `ge` printing (D7) only. **C:** small; belongs to W337's owner move.

### D14. Branch point continuation, other problem types

07p's BPCONT (explicit derivatives for ISW=2 branch-point curves, CHANGELOG:96-120), IPS 11/12/14/15/16/17
and the Python/GUI layers are out of scope for the comparison W300 can make; listed so it is not forgotten.

## 3. Licence

- AUTO-07p's terms are in its manual, **doc/auto.tex:132-166** (the local reference copy has no LICENSE or
  COPYING file, and the GitHub front page names none): the **3-clause BSD licence**, "Copyright 1979-2007,
  E. J. Doedel, California Institute of Technology, and Concordia University": keep the copyright notice and
  disclaimer in source and in documentation of binaries, no use of the holders' names to endorse. PLAUT04's
  optional Coin/Qt dependency is GPL, not relevant here. The bundled LAPACK/BLAS (src/lapack.f, blas.f) carry
  their own (BSD-style) terms and EISPACK is public (netlib); both are mentioned only if D8/D12 are done.
- xppautX is **GPL v2** (LICENSE, README.md "License"). BSD-3 code can be combined into a GPL v2
  program; the BSD notice must stay with what comes from AUTO-07p.
- So code **may** be taken, not only the method; still, AUTO-07p is Fortran 90: nothing can be vendored
  verbatim (memory "vendor outside code verbatim" needs a build that compiles it; we ship C++ only) and
  AGENTS.md says AUTO's translated Fortran is ours. A stage therefore ports a routine, with a header in the
  file that names AUTO-07p's routine and carries its copyright and the BSD text once (a
  `third_party/auto07p/LICENSE` notice file, not a vendored tree), and docs/xppautx-vs-xppaut.md records
  what was taken. W326 reimplemented the principle without copying; that remains allowed and is cheaper
  to review for small routines (D1, D2); for numerical kernels (D6, D10) a faithful port is safer.

## 4. Where the 2x per point may come from (hypotheses, nothing run)

Known: the periodic benchmark (PY_S1Bf, 8 states, NTST 100, 336 points) is 4.90 s on xppautX compiled (FD,
1 thread) against 2.44 s on AUTO-07p (JAC 0); ModelDB 267710: 38.2 s / 145 points against 49.0 s / 355
points (0.263 vs 0.138 s per point) (BENCHMARK_HANDOFF.md:327-350); with the exact Jacobian ours equals
07p's FD per point; AUTO-07p's own C equation file is compiled gcc -O2 (benchmarks/auto/bench.py:42). The
gcc-hand-RHS callgrind (instructions): RHS 47% (its sub-line "of which CORE-MATH exp/pow" reads 33% in the README table), brbd 35% (conpar 29%), memset/vectors 5%, rest 13%.

Ranked, with the measurement that would test each:

1. **H1 The right-hand side itself (most likely the largest part).** The shipped build compiles the model
   with TinyCC (about 2/3 of gcc's gain, W250: AUTO 2.43x vs 3.42x), calls it through `func` with its
   parameter copy and Session indirection (autpp.cpp), and uses correctly rounded `exp/pow` (W159) where 07p
   calls glibc (CORE-MATH: 33% of instructions in that table but "saves only 13% of the time" with libm, rhs-ceiling README:55-70). Test: the same
   AUTO run with (a) the RHS counted in both programs (a counter in 07p's C `func`, `perf`/callgrind
   inclusive `my_rhs` calls and self-time in ours) -> calls per point and ns per call; (b) ours with the W249
   gcc build, (c) 07p with the equation file compiled by TinyCC.
2. **H2 conpar** (29% of instructions in the gcc build): D6. Test: callgrind (or `perf`) inclusive time of
   `conpar_process` per Jacobian build in both programs; a microbenchmark of one 32x40 interval.
3. **H3 Extra RHS passes:** D4 and D5, ~2 more RHS passes per step beyond 07p's count. Test: RHS calls per
   point, ours against 07p (counter, as H1a); any ratio above 1.05 is this or a Newton/step difference.
4. **H4 Allocation and zeroing churn** (D5; plus `all_finite`, per-call `std::vector`s). Test: malloc
   count per point (`ltrace -c -e malloc`/heaptrack, valgrind `--tool=callgrind` malloc rows) and
   `memset` self-time.
5. **H5 Different Newton/step trajectories** (more Newton iterations, Jacobian builds or retried steps per
   point although the point count matches). Test: per point, count `solvbv(ifst 1)`, `ifst 0`, location
   iterations and step retries from fort.9 (IID 2 in both) - cheap and decisive for "same algorithm?".
6. **H6 Per-point layers outside the numerics** (stability, events, fort.* text, D7). Test: ours with IID 0,
   no diagram subscription and `jobs` checkpoints off (callgrind `do_auto` exclusive minus RHS minus linear
   algebra) against 07p's `fort.8` writing.
7. **H7 Compiler/flags:** `-ffp-contract=off` forbids FMA; gfortran at -O2/-O3 may vectorize contiguous
   loops our indexed ones block (D6). Test: read both builds' flags; compile 07p with `-ffp-contract=off`
   and see whether its time moves.
8. **H8 Serial remainder** (reduction, `dimrge`, back-substitution; Amdahl's cap in auto-parallel/README.md
   :60-70) matters for threads, not for 1-thread per point; 07p's nested dissection is not the reason at one
   thread.

H1's counters (a) and H5 should be taken first: if calls per point are equal and a per-call cost explains the
gap, W302's stages 3 and 4 are bounded by the remainder. This belongs to W354's stage 2 (`--alone`, external
profilers only: the program carries no measuring code, AGENTS.md).

## 5. Proposed stages (cards, in order)

W326 (the restore) is stage 1. Each card: baseline `tools/autocompare.py --update --only MODELS` once,
naming the models; W300's AUTO-07p comparison never worse (`autocompare` without update shows 0 new
differences); the difference from XPPAUT 8.0 recorded in docs/xppautx-vs-xppaut.md and CHANGELOG.md;
findings in docs/xppaut-findings.md where XPPAUT is wrong; both thread counts equal.

**Stage 2. Count and profile the per-point cost** (read-only for the program; after W354 stage 1's plan).
Does: sections 4's H5, H1a, H3, H4 counts in both programs on PY_S1Bf and ModelDB 267710. Done: a table of
RHS calls, Newton iterations, Jacobian builds, mallocs and self-times per point, ours vs 07p; the stage-3
list reduced to what the numbers support. Risk: none (heavy, `--alone`). Collides with nothing.

**Stage 3. Cost without a number change (bit-identical).** D4 (no residual pass for the fold direction),
D5 (one pass for Jacobian and residual, persistent zero-free work arrays, no per-call vectors in
`funi`/`setrhs`/`setubv_make_*`), D7 (pivot print gated), D6 (conpar's physical swaps and cached row
maxima). Done: `autocompare` and `examples_check` baselines **unchanged** (not rewritten), both thread
counts identical, unit tests, speed per point measured by W354's harness against stage 2's table. Risk: low;
the pivot tie order in D6 is the one place bits can move - then stop and report. Split into two cards if
review prefers (3a: D4, D5, D7 in solvbv/setubv2/funi; 3b: D6 in conpar2).

**Stage 4. Special-point location and test bookkeeping as 07p** (D1, D2, D3). Done: baseline rewritten
for ode_lecar, tstauto_lecar, lamom, lamom2, lo, torx, pp2, frcp, brper, ab, pf, san, simplefold (the
models with located points), each changed point explained; agreement with 07p equal or better in every
entry of the table (the large entries - lecar's HB 5.5e-5 / period 1.7e-3 / spectrum 2.9e-3 - expected to
drop); `test_auto_location` extended for a retried step and for two events in one step. Risk: medium, many
diagrams change; the labels (not only the positions) of pp2's cycles may change, which W326 already left
mesh-dependent.

**Stage 5. Mesh and Floquet bit-level matches** (D9, and D8 only if stage 2/4 show a spectrum gap). Done:
periodic baseline rewritten once for all nine periodic descriptions; spectra agreement with 07p equal or
better. Risk: medium-low; D8's LAPACK port is large and is recommended only on evidence (open question 6).

**Stage 6. Two-parameter curves: references first, then the 07p system** (D10). First half: extend
`tools/auto_reference.py` and tests/auto with fold/Hopf/PD curve descriptions and 07p references (the gap
named above); second half: reduced Hopf system and structured Jacobians. Done: new baselines for the curve
descriptions only (no existing diagram changes), agreement with 07p stated by W300's rules. Risk: high
(restart files, `stpnhb`, AUTO forms).

**Stage 7. Codim-2 labels** (D11), only if the maintainer wants the labels (open question 4). **Stage 8.
HomCont projection** (D12) after a valid 07p homoclinic reference exists.

### Order against the W329-W337 chain (docs/studies/w301-auto-audit.md; roadmap.md:974-982)

Done: W332 (MPI plumbing out of autlib2/reduce), W333 (wrapper/restart merge), W335, W336 (EISPACK
reentrant). Open: W329 (typed codes: autlib1 cnrlae/cnrlbv, gogoauto, auto_nox) in progress; W330 (axes)
and W331 (marks) blocked on it; W334 (owned views: setubv2, conpar2, autlib2 solvbv/brbd signatures,
`ARRAY2D/3D`) blocked; W337 (`ge`, `nlvc`, `eig`, `flowkm` into `xpp_math`) last.

1. **Stage 2 now**: no files.
2. **Stage 4 after W329 merges** (both rewrite the cnrl*/lcsp* region of autlib1.cpp; rebasing a
   fresh, smaller stage on the typed-code API avoids rework). It needs none of W330-W337.
3. **Stage 3a/3b after W334**: it changes the same signatures and arrays (`solvbv`, `setrhs`, `conpar`,
   work-array ownership); doing it first would be rewritten by W334's views, doing it later in W334's shape
   is cheaper. If the speed gap matters sooner, 3a's `funi`/`setrhs` allocations (outside W334's list) can
   go first alone.
4. **Stage 5 after W337** (`ge`/`flowkm`/`eig` move to `xpp_math`; porting D8 before would be moved twice).
5. **Stage 6** is independent of W330-W337 except the typed codes (after W329) and the parameter axes of the
   curves (after W330, whose tagged axes the two-parameter references need); do the reference half
   (tools only) whenever.
6. Stage 7 after W329/W330 (new point kinds belong to the typed codes).

## 6. Open questions for the maintainer

1. **Take code or reimplement?** BSD-3 allows taking; the sources are Fortran. Recommendation: port with
   the AUTO-07p header and a `third_party/auto07p/LICENSE` notice for numerical kernels (D6, D10, D12),
   reimplement from the principle for small routines (D1-D3), and say so in each card.
2. **Is speed an acceptance goal of W302 or only agreement?** Recommendation: both, separately: stage 3 is
   bit-identical and judged by time; stages 4+ are judged by agreement; no stage is held for the other.
3. **Pin which AUTO-07p?** The tree is 0.9.4, W300's files 0.9.3. Recommendation: pin 0.9.4 for every
   new reference; rerun W300's 26 once to confirm no point moves (needs 07p built; heavy).
4. **Do we want BT/CP/ZH/GH/Rn labels (D11)?** They are new product surface (labels, ty codes, UI).
   Recommendation: defer to after stage 6; opt-in through the stop/SP settings.
5. **Hopf curves 3n+2 -> 2n+2 (D10):** changes the unknowns and files. Recommendation: adopt in stage 6
   after the references exist; no compatibility with old restart data (AGENTS.md: no fallbacks).
6. **LAPACK (D8, D12):** vendor pieces of LAPACK 3.2.2 (BSD) or keep EISPACK? Recommendation: keep EISPACK
   until a W300-style comparison shows a disagreement; no evidence today.
7. **Correctly rounded `exp/pow` vs glibc speed (H1):** keep W159. Recommendation: keep, and exclude that
   share from W302's goal; report it in the paper as the price of bit-identical numbers on every platform.
8. **Mesh bit-level matches (D9) alone?** Recommendation: not alone; fold into stage 5's single periodic
   baseline rewrite.
9. **Who runs stage 2?** It needs `--alone` heavy runs and the W354 harness. Recommendation: add it as a
   section of W354's stage 2 instead of a new card, so the matched timing and the counters share runs.
10. **Record Mueller's NaN (D1) as an XPPAUT finding now?** Recommendation: only with a reproducing case;
    until then it stays in this study.
