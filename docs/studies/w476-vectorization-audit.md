# W476: hot loops for vectorization and reduction, across the core

Card W476 Audit: hot loops for vectorization and reduction across the whole core
([#539](https://github.com/MuhammadMoustafa/xppautX/issues/539)), 2026-10-26. A study: nothing in `core/` changed.
Everything below is **instructions counted by callgrind** on one thread (independent of the machine's load) unless it says
"time"; time numbers are native wall-clock, best of three, on an otherwise idle machine, and are only given where an
experiment was run. The tools are `tools/vectorization-audit/` (README there); earlier studies were read first and
not repeated: [w358-auto-profile.md](w358-auto-profile.md) (where AUTO's instructions go), W473/W475 (conpar), W470
(product_accumulate), W488 (the solvers' pre-release batch: its comments cite "W476 findings 4 and 6" and it already acted
on them, see F1).

## Summary

- **Almost nothing in the core is a vectorization target.** In the 40 workloads the instructions go to the model
  right-hand side (TinyCC code, 17-25%), CORE-MATH's correctly rounded functions (`tanh`/`cosh` 30% of lecar, `exp` 11% of
  motoneuron, `sin` 58% of kuramot100), formatting and compression, and a long tail of short scalar loops (2 to 28
  elements) with branches. A vectorizer helps long, branch-free, elementwise loops, and the core has two kinds of
  them: dense LU on large systems (F1) and AUTO's conpar row updates (already done at W475).
- **One loop family is worth a card now (F1, same bits, measured):** the dense LU kernels (`sgefa`'s row update,
  CVODE's `gefa`/`gesl`) are left scalar by gcc -O2, `sgefa`'s despite W488's comment saying it vectorizes. Written through
  `xpp::subtract_scaled` they gave, on a 400-state model, **CVODE 0.158 to 0.133 s (-16%), Stiff 6.55 to 5.90 s (-10%),
  Gear 1.66 to 1.60 s (-3.5%)**, all bit-identical (examples 215/216, autocompare 61 models 0 failures), and +0.6% instructions on
  2-state models. Nothing for models under about 50 states.
- **The largest share seen is not vectorization but algorithm (F2):** on that 400-state tridiagonal-coupled model `sgefa`
  is 84% (Stiff) and 52% + `sgesl` 37% (Gear) of all instructions, a dense LU of a sparse matrix.
- **Four ideas were measured and are not worth it:** conpar's `first_maximum` as two passes (**+1.5% time**, worse), an AVX2
  build (`-march=x86-64-v3`: AUTO -1.7%, LU -10%, so no function multiversioning), `-fvect-cost-model=dynamic`
  everywhere (bit-identical but +2 to +7% instructions on small systems, -13 to -33% on large ones), and any reduction
  reordering (class b): **no reduction is above 1% of any workload except the Volterra convolution sum, which is not
  vectorizable as written**; no numerics change is proposed.
- Per-step overhead on cheap right-hand sides (`integrate`'s five loops over the states, `set`, `extra`, the job polling)
  is 10-25% of a lecar run and about 1% of a realistic model: a cleanup, not a vectorization (F8), as is the DDE's
  `polint` (F9, 27% of the DDE run).

## Method

- **Build.** The normal one, `make -j6 xppautx BUILDDIR=build/prof` (`-g -O2 -ffp-contract=off`, g++ 15.2.0, x86-64 baseline:
  SSE2, 2 doubles per vector, no `-march`), 0 warnings. Experiments used their own build folders, each checked with
  `tools/examples_check.sh --bin` (215/216, ml-range by design) and `tools/autocompare.py --bin` (61 models, 0
  failures): `-O2 -fvect-cost-model=dynamic` (build/vcm), `-O2 -march=x86-64-v3` (build/v3), and a prototype of F1 plus the
  first_maximum variant (build/proto, reverted: `git diff` of core/ is empty).
- **Workloads** (`tools/vectorization-audit/profile.py`, one valgrind process each, seconds of native time):
  integration runs are `examples/ode` models with their `integration:` block replaced, driven by `xppautX MODEL --silent
  --noout` (no output.dat: writing rows was 40-80% of the first profiles and is not the workload); AUTO runs are
  `tests/auto` descriptions through tools/auto-profile/profile.py (the 1-thread run of tools/autocompare.py's own
  driver); the page's data work is one `--server` run with every data event on. Models: lecar (2 states) under every
  integrator; kuramot100 (100 states, rk4); lecdiff200 (400 states: CVODE, Stiff, Gear); dde.odex; kohonen.odex (map);
  tstvol2 (Volterra); dae_ex1; a harmonic oscillator for Fourier and histogram (a right-hand side that costs
  nothing); lecar with a 400-point nullcline mesh and a 100-point direction field; model loading of kuramot100,
  lecdiff200 and tools/models/longnames.ode.
- **Shares** are of the whole process's instructions, self cost, with `callgrind_annotate`'s inlined-source attribution
  for per-line numbers (`--lines`). `???:0x...` entries are TinyCC's generated model code.
- **Why a loop is scalar:** `-fopt-info-vec-all` per file (tools/vectorization-audit/vecinfo.sh). gcc 15's -O2 uses the
  *very-cheap* vector cost model, which vectorizes only a loop whose trip count needs no scalar epilogue; every loop
  of unknown length is scalar, whatever its shape (W475 found this in conpar; objdump of `xpp::sgefa` and `gefa` shows no
  `mulpd`). Writing the loop in groups of four, as `xpp::subtract_scaled` does, is what makes -O2 vectorize it. gcc
  never vectorizes a floating-point sum without `-ffast-math`, so no flag could have changed a reduction's bits.

### The workloads and where their instructions go (M = million instructions)

| workload | M | largest self shares |
|---|---:|---|
| int_rk4 / modeuler / euler (lecar) | 148 / 234 / 338 | tanh 22% (16-19), cosh 10%, compiled RHS 9%, `integrate` 6% (10, 14) |
| int_qualrk / dopri5 / dopri8 | 966 / 563 / 595 | tanh 24 / 13 / 14%; `integrate` 5 / 19 / 18%; dormpri `step_by` 13 / 12% |
| int_cvode | 456 | `integrate` 22%, tanh 8%, `extra` 6%, `CVodeDky` 6% |
| int_adams / stiff / gear / rosenbrock / backeuler | 103 / 1338 / 666 / 1189 / 191 | tanh 18-20%; `adams` 11%; `ggear` 27%; `stiff` 6%; `rosen` 7% |
| int_kuramoto100_rk4 (100 states) | 521 | `cr_sin` 58%, compiled RHS 16%, `do_shift` 8% |
| int_lecdiff200_cvode / stiff / gear (400 states) | 1573 / 3052 / 6032 | `gefa` 71% / `sgefa` 84% / `sgefa` 52% + `sgesl` 37% |
| int_dde / int_map / int_volterra / int_dae | 114 / 201 / 5388 / 2514 | `polint` 27% / `cr_cos` 19% / `init_sums` 87% / `solve_dae` 17% |
| nullclines_dfield / fourier / histogram | 323 / 343 / 175 | tanh 22%; pocketfft about half (vendored); `integrate` 18% (`new_hist` 3%) |
| plot_data (every data event, 20000 rows) | 63 | libstdc++ formatting 15%, `to_chars` 8%, tanh 11% |
| load_kuramoto100 / lecdiff200 / longnames | 37 / 76 / 20 | TinyCC's lexer and code generator, `memcmp` 4-9% |
| tstauto_ab, ab_hopf, ab_fold (equilibria, Hopf and fold curves) | 34 / 222 / 101 | miniz compress 19-20% (the session save), no loop of ours above 3% |
| tstauto_pp2, brper (periodic) | 115 / 67 | miniz 12-14%, conpar 6-7%, subtract_scaled 4-5%, setubv 5-6% |
| tstauto_dumbvp, exp, ode_tstheti (BVP) | 66 / 97 / 52 | conpar 7-16%, subtract_scaled 4-11%, setubv 6-9% |
| ode_motoneuron6c (25 states, periodic, two-parameter) | 37578 | compiled RHS 25%, `cr_exp` 11%, conpar 16% (`first_maximum` 6.5, `subtract_scaled` 7.2), `sgefa` 5.5%, `cr_pow` 5.5%, reduce 4%, `product_accumulate` 3.3% |
| forced_pp_ns (two-parameter of cycles) | 8415 | conpar 29% (`subtract_scaled` 11.6, `first_maximum` 9.3, swaps 6), compiled RHS 16%, reduce 6%, `setubv_make_fa_aa_bb` 4% |
| lorenz_homoclinic | 3628 | conpar 26% (`subtract_scaled` 12: its scalar tail 5.6, body 3.3), `setubv_make_fa_aa_bb` 8%, reduce 5% |

## Findings

Class: (a) same bits (elementwise, layout, a hoisted test); (b) reduction, a deliberate numerics change; (c) not worth
it. "Gain" is measured where the text says so, else an estimate from the share (the share is the ceiling).

| id | where | share (workload) | class | gain | proposed card |
|---|---|---|---|---|---|
| F1 | xpp_math.cpp:314 (sgefa dense row update), dense.cpp:146, 177, 187 (CVODE gefa, gesl) | 71% (lecdiff200 CVODE), 25% of Stiff's 84%, 13% of Gear's 52% | (a) | measured: time -16% CVODE, -10% Stiff, -3.5% Gear at 400 states; +0.6% instructions at 2 states | **A**: the dense LU kernels through `xpp::subtract_scaled` |
| F2 | xpp_math.cpp:317, 256 (sgefa's sparse-row path, strided `saxpy`) | 65% of lecdiff200 Stiff, 72% of Gear | algorithm, (a) with a caveat | several-fold on large sparse models (estimate, not measured) | **B**: a banded or row-skipping LU for Stiff, Gear, backward Euler, Rosenbrock, DAE |
| F3 | conpar2.cpp:29-36 (`first_maximum`) | 6.5% motoneuron, 9.3% forced_pp_ns, 2% lorenz | (c) | measured: two passes cost +1.5% time | none |
| F4 | xpp_math.h:273 (`subtract_scaled`) in conpar2.cpp:101-114 | 7.2%, 11.6%, 12% | (c) | measured: AVX2 build -1.7% on AUTO; the scalar tail is 1/5 to 2/3 of it on short rows | none |
| F5 | conpar2.cpp:64, 69, 72 and the undo pass (physical row and column swaps, std::swap) | 1.4% motoneuron, 6.2% forced_pp_ns, 3% lorenz | (a) layout | at most 3% of a periodic run (the undo is half), not measured | **D** (optional) |
| F6 | autlib2.cpp:487-545 (reduce_merge's strided row updates, column-major blocks) | 4.0%, 6.1%, 5% | (a) with a masked update | at most 2% of a periodic run | **D** (optional) |
| F7 | setubv2.cpp:116-141 (fa and aa assembly) | 4% (forced_pp_ns), 8% (lorenz) | (c) | dfdu is strided by ndim; a transpose copy costs what it saves | none |
| F8 | integrate.cpp:1780-1800, 1949; my_rhs.cpp:17, 72; getvar.h:9; xpp_job.cpp:215, 230 | `integrate` self 6-22%, `set` + `extra` 3-5%, job polling 3-7% on cheap right-hand sides | (a), a cleanup | 5-8% of a cheap-RHS run, about 1% of a real one | **C** |
| F9 | delay_handle.cpp:95 (`polint`, 4 points), `get_delay` | 27% + 9% (DDE) | (a) scalar | 15-20% of a DDE run (estimate) | **C** |
| F10 | volterra2.cpp:208-261 (`init_sums`: the cache predicate per term) | 87% (Volterra) | (c) here; the sum is (b) | algorithmic: not vectorizable as written | none (a kernel-history layout is W488's follow-up) |
| F11 | odesol2.cpp:369, 372, 381 (Adams predictor/corrector over k), gear.cpp:243, dormpri.cpp:870-872 (dense output) | `adams` 11%, `dopri8` 4%, `ggear` 27% on 2 states | (a) at large n, (c) at small | only for large systems; Adams and Gear are rare | none |
| F12 | vector.cpp (CVODE N_V*), cvode.cpp, stiff.cpp `rkck` | below 1% in every workload, 400 states included | (c) | none | none |
| F13 | nullcline.cpp `do_cline`, `fnull`, histogram.cpp, pocketfft, miniz | 6% / 3% / 49% / 20% | (c): scalar grid calls, vendored | none | none |

### F1: the dense LU kernels are scalar, and W488's sgefa comment is wrong

`xpp::sgefa` (xpp_math.cpp:288-325) was rewritten at W488 with a row-oriented update "which vectorizes". It does not: objdump
of `xpp::sgefa` has 3 `mulsd` and no `mulpd`, and `-fopt-info-vec-all` says "couldn't vectorize loop" at line 314. CVODE's `gefa`
(dense.cpp:81-152, the column update at 146) and `gesl` (177, 187) are the same shape. XPPAUT 8.0's `sgefa` is the strided LINPACK
`saxpy` form ([gear.c:1645](../reference/xppaut-8.0/gear.c#L1645), [gear.c:1746](../reference/xppaut-8.0/gear.c#L1746)). The fix that
was measured: the three loops call `xpp::subtract_scaled(target, source, -factor, n)`. It is the same bits (`a*(-b)` is exactly
`-(a*b)`, and `x - (-p)` is exactly `x + p`), no new helper, no shared kernel written twice. Results: examples 215/216 and
autocompare 61 models, 0 failures, unchanged. Time (400-state `lecdiff200`, 20x the profiled length): CVODE 0.158, 0.160, 0.163 s before and
0.132, 0.133, 0.136 after; Stiff 6.54-6.58 before, 5.90-5.91 after; Gear 1.66 before, 1.60 after. Instructions: -34%, -11%, -6%,
and +0.6% on the 2-state lecar Stiff run (the tails). Not a gain for small systems, which is most of examples/ode; it matters for the
models that were slow (lecdiff200, a 200-cell cable).

### F2: dense LU of a sparse matrix

With F1 in place the remaining 65% of lecdiff200 Stiff is `sgefa`'s sparse branch: a pivot row with zeros keeps LINPACK's
column sweep, and each column sweeps all `n-k-1` rows through `saxpy` at stride `lda`, though the multipliers below the pivot are
almost all zero. The model's Jacobian is banded; the factorization costs O(n^3) where O(n b^2) would do. This is not vectorization
(W488 made the Jacobian evaluation sparse-aware; the factorization is the other half). Skipping rows whose multiplier is zero
changes bits only where `x + 0*y` differs from `x`: `x = -0` becomes `+0`, and an infinite or NaN `y`; the first needs a
decision (the output of such a run prints "0" either way), the second is already an error. Proposed as its own study first.

### F3, F4: conpar is done, measured

`first_maximum` (conpar2.cpp:29) is 6.5% of motoneuron: 3 instructions per element for a branch that is rarely taken
(W475's `[[unlikely]]`). A two-pass version (four independent maxima, then a search for the first equal element) was built and
measured: instructions +1 to 2%, time 3.56 to 3.61 s on motoneuron + forced_pp_ns (three alternating runs each), i.e. 1.5%
slower. `subtract_scaled` is vectorized (xpp_math.h:276, "loop vectorized using 16 byte vectors"); a `-march=x86-64-v3` build
of the same sources, bit-identical, runs AUTO 3.55 s against 3.61 s (-1.7%): the rows are 5 to 100 elements and the loop is latency
bound, so no function multiversioning (`target_clones`) is proposed. On a 3-state problem (`lorenz_homoclinic`) the scalar tail
is 5.6% against 3.3% for the body: the rows are shorter than the group of four.

### F5, F6: AUTO's remaining data movement

conpar physically swaps rows and columns of the condensed blocks (conpar2.cpp:62-70) and undoes every swap at the end (lines
126-129): up to 6.2% of forced_pp_ns (the swap loops 2.8%, `std::swap`'s own instructions 3.4%) and 1.4% of motoneuron. The permutation is already
recorded in `irf`/`icf`; reading through it instead of moving the data would drop both passes. `reduce_merge`'s row updates
(autlib2.cpp:487-545) walk column-major blocks along a row (stride `nov`); updating a column at a time (compute every multiplier first,
which is safe since the pivot column is excluded) is contiguous and elementwise, the same bits, but needs the zero-multiplier skip
as a mask (`rm != 0 ? rm*y : 0.0`: `x - (+0.0)` is `x` for every `x`, NaN and -0 included). Both are small; they are the only
loops left in AUTO's periodic path that a change of layout would speed up, and together at most 5% of forced_pp_ns. Optional.

### F8, F9: cheap-right-hand-side overhead

For lecar, `integrate` (integrate.cpp:1780, 1787, 1794: float conversion, NaN test and bound test per state; 1949: the store
into the column-major data table, one array per variable) costs about 320 instructions per step in itself (rk4: 9.6 M over 30000 steps): 6 to 22% of
the workload, plus `set(i+1, y[i])` (my_rhs.cpp:17 with getvar.h:9's bound check per element, 1-2%), `extra`'s loop (my_rhs.cpp:72,
1.5-2%) and the job polling (`xpp::every` calls `steady_clock::now`, `job::cancelled`; 3-7%). These loops are short and
branchy, so a vectorizer does not help; one pass per stored point (convert, test, store) and a memcpy for `set` are the cleanup.
`polint` (delay_handle.cpp:95) interpolates 4 points with data-dependent indices; with the count fixed it can be straight-line
code, the same operations in the same order. Both are for models whose right-hand side is cheap; on a realistic model the
right-hand side is 40-60% and these are about 1%.

### F10 to F13: checked, not worth it

Volterra's `init_sums` is 87% of the Volterra run, but its time is the per-term cache predicate (volterra2.cpp:208, 27%) and
formula lookups, not arithmetic; its sum `sum[ker] += alpbet*formula*cnv` is a reduction whose order we keep. Adams, Gear
and Dormand-Prince combine a few stage vectors over the states (n = 2 in lecar, 100 in kuramot100, where the right-hand side
is 80% anyway): loops of 2 to 100 elements, below 1% of the large-system workloads. CVODE's vector kernels (vector.cpp) are
vectorized where the length allows and are under 1% even at 400 states. The nullcline grid (6%), the histogram (3%) are scalar
calls of the right-hand side; Fourier's time is pocketfft (vendored, its Bluestein path for a length with a large prime factor);
miniz's 12-20% in the AUTO descriptions is the session save at the end of each description, once, not per point.

## Reductions (class b)

Every sum, dot product and norm was looked for in the profiles: AUTO's `bcksub_merge` (autlib2.cpp:587, `sm +=` over a block),
`rinpr`, `ge`; CVODE's `N_VDotProd`/`N_VWrmsNorm`; the Runge-Kutta error norms; Fourier; the histogram; Volterra's
convolution. None is above 1% of any workload except Volterra's, and that one is not a loop gcc or a vector unit can use as it
stands (F10). **No reduction card is proposed, and no baseline would change.** Were one ever needed, the rule is: a fixed
shape that does not depend on the machine, for example four partial sums over elements `i mod 4` combined as `((s0+s1)+(s2+s3))`
written in the source (not left to a vectorizer, which does not reorder without `-ffast-math`), identical at every width, thread
count and compiler; it would change the baselines of exactly the models that reach that routine (examples md5s, the diagram
baselines for AUTO's) and be a deliberate, recorded numerics change like W474's tree.

## The duplicated kernels a shared helper would serve

`xpp::subtract_scaled(target, source, factor, n)` (xpp_math.h:273) is the shared `y[i] -= a*x[i]` kernel, and `y[i] += a*x[i]` is
the same call with `-a` (same bits). Lines of the form `y[i] +-= c*x[i]` in core/ (a grep count, so an upper bound on loops):
dormpri.cpp 24, odesol2.cpp 16, stiff.cpp 10, xpp_math.h 6 (its own), volterra2.cpp 3, dense.cpp 3 (F1), equilibria.cpp 2,
do_fit.cpp 2, band.cpp 2, autlib3.cpp 2, and one each in xpp_math.cpp (`saxpy`, strided), markov, integrate, flags, conpar2,
auto_codim, autlib1, autlib2, adj2. Only dense.cpp and `sgefa` (F1) are loops where a vectorized kernel is measurable; the rest
are short stage combinations. The strided `saxpy` (xpp_math.cpp:249) and `sscal` are the sparse path's (F2) and, in a different
layout, reduce_merge's (F6). `product_accumulate` (xpp_math.cpp:742) is already vectorized (W470). `largest_magnitude` (the pivot
search, called per column by `sgefa`; the same loop is written again in CVODE's `gefa` and, as a row maximum, in conpar's
`first_maximum`) is three copies of one scan, none of them worth changing for speed (F3).

## Proposed cards, ranked by measured share times achievable gain

1. **A. The dense LU kernels through `xpp::subtract_scaled`** (F1: `sgefa`, CVODE `gefa`/`gesl`; `sgesl`'s and `band.cpp`'s
   updates if they show): small, same bits, measured -3.5 to -16% on a 400-state model; fix W488's comment.
2. **B. A study of sparse-aware LU for the stiff solvers** (F2): the largest share in any workload (84%), algorithmic, with the
   signed-zero decision above; measure on lecdiff200 and the models of W378 (large stiff).
3. **C. Per-step overhead on cheap right-hand sides** (F8, F9): one pass per stored point, `set` as a copy, `polint` straight-line,
   the job poll less often than every step; 5-8% of a cheap-RHS run; same bits.
4. **D. (optional) AUTO's data movement** (F5, F6): the swap and undo of conpar through the recorded permutation, `reduce_merge`'s
   column updates; at most 5% of a periodic run, to be measured per step before it is kept.

Not proposed: a two-pass `first_maximum`, AVX2 multiversioning, `-fvect-cost-model=dynamic` or `-O3` for the build, reduction
reordering. The model right-hand side and CORE-MATH (together 40-60% of most workloads) are W472's and the correct-rounding
rule's, not a vectorization matter.

## What was not measured

Instruction counts are not time (CORE-MATH runs at a high instructions-per-cycle, W358); the times above are given only for
the three experiments that ran natively, on a laptop-class CPU with AVX2 and FMA, where the baseline build uses SSE2 only. F5, F6, F8
and F9's gains are estimates from the share. The 25-state ModelDB run was cut by the test description (not W358's 20-point
cut), and models with many states and a dense Jacobian (a network of 100 coupled cells) were not in the set beyond
kuramot100 and lecdiff200. A Windows or macOS compiler may vectorize differently (clang's -O2 vectorizes loops of unknown
length), so F1's gain is measured with gcc only; the other platforms can only gain.

## Questions for the maintainer

- The scripts are in `tools/vectorization-audit/`, not `paper/benchmarks/vectorization-audit/`: `/paper/` is git-ignored (AGENTS.md
  says the same of paper/benchmarks), so a committed script could not live there. Move it if you prefer a forced add.
- F2 (skipping zero multipliers in the LU) changes the sign of a zero in the weird case above; accept it, or keep
  LINPACK's exact test and skip only what is provably zero.
