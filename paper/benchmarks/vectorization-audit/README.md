# W476: audit of hot loops for vectorization and reduction (study)

Card W476 Audit: hot loops for vectorization and reduction across the core (#539). Nothing in core/ or tests/ was
changed (`git status` of the worktree is clean); this folder (git-ignored, like all of `paper/`) holds the harness,
the profile summaries, the experiment patches and this report. Constraints kept by every proposal: `-O2
-ffp-contract=off`, baseline x86-64 (no -march, no FMA, no fast-math), bit-identical results on every OS, CPU,
compiler and thread count; a numerics change only as a deliberate, platform-independent one (class b).

## How it was run

Build: master at e5a66597 (W475 included), `make xppautx` (-g -O2, the project's flags), Ubuntu, gcc 15.2. Instruction
counts are callgrind's (`valgrind --tool=callgrind`; perf is blocked here); callgrind finds the hot lines, but
instruction counts are not time (see the first_maximum experiment below, where fewer instructions were slower), so
each proposal was also timed natively (min of 3, `taskset -c 4`) where an experiment was cheap.

* `run_cg.sh NAME MODEL`: `./xppautX MODEL --silent` under callgrind; `out/NAME.top` has the top functions
  (`out/*.cg` were deleted after summarizing: large). `summarize.py N out/*.top` makes the tables below (TinyCC's
  generated model code is one entry; libstdc++'s `to_chars`/`format` internals and string appends are one entry,
  "text output").
* `autocg.py DESCRIPTION [--cg]`: one tests/auto description through tools/autocompare.py's `compute` (the AUTO
  baseline's own session), threads=1; `--cg` runs the server under callgrind; `NMAX=15` shortens the mu26 `cycles` run
  as W475 did; `BIN=...` points at an experiment build, `CGOPT=--toggle-collect=*fn*` collects one function.
* `server_cg.py MODEL --cg`: one run through `--server` with the page's `series`/`plots` subscription.
* `exp.sh NAME "FLAGS" core/x.cpp...`: scratch build of the named sources (working-tree text, extra flags) linked over
  build/obj into `scratch/exp-NAME/xppautX`; nothing under build/ or core/ is touched. `patches/*.patch` are the three
  experiments that were measured (applied to a scratch tree, built, reverted; never committed).
* `scratch/fmt.cpp`, `scratch/lu.cpp`, `scratch/ge.cpp`: stand-alone kernels (number formatting, sgefa, AUTO's ge) that
  compare the old and proposed code for bits and time.

### Workloads (instructions in G = 10^9, callgrind, self cost; "JIT" = TinyCC-compiled model code, W472's subject)

models/ holds the model copies (examples/ode with the integrator and total time edited, the W354 screen models, the
analysis/plot variants of lecar). Integrators: mu26 (28 states) with rk4, adams, euler, qualrk, cvode, dopri5, dopri8,
stiff, rosenbrock, gear, backward Euler; motoneuron6c (backward Euler), PY_S1Bf_bench (stiff), episodic (qualrk, 120
states), lecdiff200 (rk4, 400 states), kuramot100 (rk4, 100 states, `shift`/`sum`), amari and clustor (networks: `conv`),
koho3 (discrete maps with flags), fr (delay), hhred (gear), mlvolt and cuplamdif (Volterra). Non-solver: lecar with
nullclines (mesh 400) and direction field (grid 100), a 60000-row run with the histogram and with the Fourier transform
(`@ postprocess`), model load and parse (motoneuron6c and episodic with total 0.1), and the server's data events.
AUTO: lecar (equilibrium and cycles, NMAX 15), ma23, motoneuron6c, explicit_homoclinic and lorenz_homoclinic
(BVP/homoclinic), hill_cycles_pd (periodic and period doubling, two-parameter), forced_pp_pd (map, two-parameter),
tstauto_bigaut (large equilibrium system), mu26 control (NMAX=15). Skipped: interactive nullclines in the page (the
`--ncdraw 2` write path computes the same data), a multi-thread profile (callgrind serialises threads; every
description ran with threads=1).

## Top functions per workload (self %, the first six; totals in instructions)

### Integration and output (silent run)

| workload | total Ir | top functions |
|---|---:|---|
| PY_S1Bf_bench | 11,768,661,732 | std::format/to_chars text output 24.1%; model code (TinyCC JIT) 22.8%; core-math/exp/exp.c:cr_exp_fma 17.4%; xpp_math.cpp:xpp::sgesl 6.2%; xpp_math.cpp:xpp::sgefa 3.1%; stiff.cpp:xpp::stiff 2.8% |
| amari | 1,782,540,874 | simplenet.cpp:xpp::evaluate_network 50.2%; std::format/to_chars text output 34.4%; libc mem* 3.6%; /usr/include/c++/15/bits/unicode.h:std::__unicod 0.6%; integrate.cpp:xpp::integrate 0.5% |
| clustor | 1,495,323,467 | simplenet.cpp:xpp::evaluate_network 37.7%; std::format/to_chars text output 17.6%; core-math/exp/exp.c:cr_exp_fma 9.0%; model code (TinyCC JIT) 3.2%; libc mem* 2.9%; stiff.cpp:xpp::rkck 1.3% |
| cuplamdif | 2,643,104,137 | volterra2.cpp:xpp::init_sums 50.3%; model code (TinyCC JIT) 17.3%; expr_eval.cpp:xpp::evaluate 15.2%; getvar.h:xpp::init_sums 7.8%; /usr/include/c++/15/bits/stl_vector.h:xpp::init_ 2.4%; /usr/include/c++/15/bits/stl_vector.h:xpp::evalu 1.4% |
| dde | 13,499,673 | std::format/to_chars text output 17.8%; libc mem* 8.6%; delay_handle.cpp:xpp::polint 7.5%; ./elf/./elf/dl-lookup.c:do_lookup_x 4.4%; ./elf/../sysdeps/generic/dl-new-hash.h:_dl_looku 4.0%; delay_handle.cpp:xpp::get_delay 2.7% |
| episodic | 3,087,800,578 | std::format/to_chars text output 26.8%; model code (TinyCC JIT) 20.4%; core-math/exp/exp.c:cr_exp_fma 18.7%; libc mem* 2.5%; stiff.cpp:xpp::rkck 2.4%; xpp_math.h:xpp::math::exp 1.5% |
| fr | 855,623,596 | delay_handle.cpp:xpp::polint 38.7%; delay_handle.cpp:xpp::get_delay 13.8%; core-math/cos/cos.c:cr_cos_fma 10.7%; model code (TinyCC JIT) 4.5%; std::format/to_chars text output 4.4%; core-math/sin/sin.c:cr_sin_fma 4.4% |
| hhred | 14,750,055 | libc mem* 9.8%; model code (TinyCC JIT) 6.0%; core-math/exp/exp.c:cr_exp_fma 5.6%; gear.cpp:xpp::ggear 4.5%; ./elf/./elf/dl-lookup.c:do_lookup_x 4.0%; ./elf/../sysdeps/generic/dl-new-hash.h:_dl_looku 3.6% |
| koho3 | 8,559,126,605 | flags.cpp:one_flag_step 29.0%; getvar.h:one_flag_step 28.3%; core-math/cos/cos.c:cr_cos_fma 8.9%; std::format/to_chars text output 7.5%; core-math/exp/exp.c:cr_exp_fma 4.2%; model code (TinyCC JIT) 3.6% |
| kuramot100 | 6,506,350,082 | core-math/sin/sin.c:cr_sin_fma 62.5%; model code (TinyCC JIT) 18.0%; expr_functions.cpp:xpp::expr::do_shift 8.6%; xpp_math.h:xpp::math::sin 3.7%; std::format/to_chars text output 1.7%; core-math/sin/sin.c:sin_large_accurate 1.4% |
| lecar_fft | 474,635,700 | std::format/to_chars text output 13.5%; model code (TinyCC JIT) 13.5%; core-math/tanh/tanh.c:cr_tanh_fma 13.4%; pocketfft/pocketfft_hdronly.h:void pocketfft::de 12.8%; core-math/cosh/cosh.c:cr_cosh_fma 6.1%; integrate.cpp:xpp::integrate 3.7% |
| lecar_hist | 289,611,254 | model code (TinyCC JIT) 22.1%; core-math/tanh/tanh.c:cr_tanh_fma 22.0%; core-math/cosh/cosh.c:cr_cosh_fma 9.9%; integrate.cpp:xpp::integrate 6.1%; my_rhs.cpp:my_rhs 3.8%; odesol2.cpp:xpp::one_step_rk4 3.4% |
| lecar_nullc_dfield | 409,257,892 | model code (TinyCC JIT) 22.5%; core-math/tanh/tanh.c:cr_tanh_fma 19.1%; core-math/cosh/cosh.c:cr_cosh_fma 10.4%; std::format/to_chars text output 9.4%; my_rhs.cpp:my_rhs 4.1%; nullcline.cpp:xpp::anon::do_cline 3.9% |
| lecdiff200 | 637,657,640 | std::format/to_chars text output 35.6%; model code (TinyCC JIT) 15.3%; core-math/tanh/tanh.c:cr_tanh_fma 10.1%; core-math/cosh/cosh.c:cr_cosh_fma 6.0%; libc mem* 3.7% |
| load_episodic | 55,250,478 | libc mem* 11.8%; tinycc/tccpp.c:next_nomacro 3.3%; tinycc/tccpp.c:next 2.4%; tinycc/x86_64-gen.c:o 2.2%; ./malloc/./malloc/malloc.c:_int_malloc 1.9%; tinycc/tccgen.c:unary'2 1.3% |
| load_motoneuron6c | 46,912,697 | libc mem* 11.8%; model code (TinyCC JIT) 2.8%; tinycc/tccpp.c:next_nomacro 2.6%; ./malloc/./malloc/malloc.c:_int_malloc 1.8%; tinycc/tccpp.c:next 1.7%; tinycc/x86_64-gen.c:o 1.4% |
| mlvolt | 688,253,709 | volterra2.cpp:xpp::init_sums 49.7%; model code (TinyCC JIT) 14.8%; expr_eval.cpp:xpp::evaluate 13.2%; getvar.h:xpp::init_sums 8.7%; /usr/include/c++/15/bits/stl_vector.h:xpp::init_ 2.9%; std::format/to_chars text output 1.6% |
| motoneuron6c | 11,925,647,185 | model code (TinyCC JIT) 40.1%; core-math/exp/exp.c:cr_exp_fma 15.6%; xpp_math.cpp:xpp::sgefa 10.1%; core-math/pow/pow.c:cr_pow_fma 6.8%; std::format/to_chars text output 6.0%; xpp_math.cpp:xpp::sgesl 1.9% |
| mu26_adams | 1,005,741,399 | model code (TinyCC JIT) 29.1%; core-math/tanh/tanh.c:cr_tanh_fma 19.2%; std::format/to_chars text output 16.1%; odesol2.cpp:xpp::adams 13.4%; core-math/pow/pow.c:cr_pow_fma 3.3%; core-math/exp/exp.c:cr_exp_fma 1.9% |
| mu26_backward_euler | 4,792,723,926 | model code (TinyCC JIT) 41.1%; core-math/tanh/tanh.c:cr_tanh_fma 26.9%; xpp_math.cpp:xpp::sgefa 6.8%; core-math/pow/pow.c:cr_pow_fma 4.6%; xpp_math.cpp:xpp::sgesl 2.9%; jacobian.cpp:xpp::jacobian 2.7% |
| mu26_cvode | 2,465,806,859 | std::format/to_chars text output 73.6%; libc mem* 7.1%; /usr/include/c++/15/bits/unicode.h:std::__unicod 1.3%; integrate.cpp:xpp::integrate 1.3%; vector.cpp:N_VLinearSum 1.2% |
| mu26_dopri5 | 1,705,287,389 | std::format/to_chars text output 50.9%; model code (TinyCC JIT) 7.2%; dormpri.cpp:xpp::anon::DormandPrince::sample 6.3%; core-math/tanh/tanh.c:cr_tanh_fma 5.7%; libc mem* 4.5%; integrate.cpp:xpp::dense_event 3.6% |
| mu26_dopri8 | 1,710,061,969 | std::format/to_chars text output 49.4%; model code (TinyCC JIT) 8.6%; core-math/tanh/tanh.c:cr_tanh_fma 6.6%; dormpri.cpp:xpp::anon::DormandPrince::sample 5.1%; libc mem* 4.5%; dormpri.cpp:xpp::anon::DormandPrince::step_by 2.0% |
| mu26_euler | 582,940,296 | std::format/to_chars text output 29.2%; model code (TinyCC JIT) 25.1%; core-math/tanh/tanh.c:cr_tanh_fma 16.5%; core-math/pow/pow.c:cr_pow_fma 2.8%; libc mem* 2.7%; core-math/exp/exp.c:cr_exp_fma 1.7% |
| mu26_gear | 2,467,629,315 | std::format/to_chars text output 35.3%; xpp_math.cpp:xpp::sgesl 17.0%; model code (TinyCC JIT) 11.9%; gear.cpp:xpp::ggear 8.5%; core-math/tanh/tanh.c:cr_tanh_fma 7.8%; libc mem* 3.1% |
| mu26_qualrk | 4,333,054,865 | std::format/to_chars text output 40.2%; model code (TinyCC JIT) 20.3%; core-math/tanh/tanh.c:cr_tanh_fma 13.3%; libc mem* 3.5%; stiff.cpp:xpp::rkck 2.8%; core-math/pow/pow.c:cr_pow_fma 2.3% |
| mu26_rosenbrock | 5,000,353,390 | model code (TinyCC JIT) 35.1%; core-math/tanh/tanh.c:cr_tanh_fma 23.2%; xpp_math.cpp:xpp::sgesl 7.0%; std::format/to_chars text output 5.7%; xpp_math.cpp:xpp::sgefa 4.9%; core-math/pow/pow.c:cr_pow_fma 4.0% |
| mu26_runge_kutta | 1,527,813,282 | model code (TinyCC JIT) 38.3%; core-math/tanh/tanh.c:cr_tanh_fma 25.2%; std::format/to_chars text output 10.3%; core-math/pow/pow.c:cr_pow_fma 4.3%; odesol2.cpp:xpp::one_step_rk4 3.5%; core-math/exp/exp.c:cr_exp_fma 2.6% |
| mu26_stiff | 7,115,098,186 | model code (TinyCC JIT) 36.1%; core-math/tanh/tanh.c:cr_tanh_fma 23.8%; xpp_math.cpp:xpp::sgesl 9.1%; xpp_math.cpp:xpp::sgefa 4.9%; core-math/pow/pow.c:cr_pow_fma 4.1%; std::format/to_chars text output 4.0% |
| server_srv_mu26 | 1,307,735,744 | model code (TinyCC JIT) 45.0%; core-math/tanh/tanh.c:cr_tanh_fma 29.5%; core-math/pow/pow.c:cr_pow_fma 5.1%; odesol2.cpp:xpp::one_step_rk4 4.1%; core-math/exp/exp.c:cr_exp_fma 3.0%; my_rhs.cpp:my_rhs 1.8% |

Finite-difference Jacobian (inclusive, `jacobian.cpp` with its model-RHS calls, which show above as JIT code and
math): mu26 stiff 66.8%, rosenbrock 65.7%, backward Euler 81.8%, gear about 8%; motoneuron6c backward Euler 70.5%;
PY_S1Bf_bench 33.6%; in AUTO runs of ODE models: motoneuron6c 29.7%, ma23 46.7%.

### AUTO (threads=1)

| workload | total Ir | top functions (anon:conpar_process = conpar2.cpp, "xpp_math.h:conpar_process" = the inlined `subtract_scaled`) |
|---|---:|---|
| auto_explicit_homoclinic | 481,354,524 | conpar2.cpp:anon::conpar_process 9.2%; setubv2.cpp:anon::setubv_make_fa_aa_bb 7.0%; xpp_math.h:anon::conpar_process 6.1%; model code (TinyCC JIT) 5.1%; autlib2.cpp:std::_Function_handler<void  4.5%; miniz/miniz.c:tdefl_compress 3.5%; autpp.cpp:func 2.6% |
| auto_forced_pp_pd | 29,541,728,731 | model code (TinyCC JIT) 21.5%; conpar2.cpp:anon::conpar_process 13.1%; xpp_math.h:anon::conpar_process 8.9%; my_rhs.cpp:pure_rhs 7.2%; autpp.cpp:func 6.3%; autlib2.cpp:std::_Function_handler<void  5.5%; autpp.cpp:anon::continued_constants 5.0% |
| auto_hill_cycles_pd | 2,332,238,734 | model code (TinyCC JIT) 13.6%; conpar2.cpp:anon::conpar_process 12.8%; my_rhs.cpp:pure_rhs 7.6%; autpp.cpp:func 6.6%; xpp_math.h:anon::conpar_process 6.1%; autlib2.cpp:std::_Function_handler<void  5.5%; autpp.cpp:anon::continued_constants 5.1% |
| auto_lorenz_homoclinic | 3,620,655,447 | conpar2.cpp:anon::conpar_process 14.4%; xpp_math.h:anon::conpar_process 11.6%; autlib2.cpp:std::_Function_handler<void  7.7%; setubv2.cpp:anon::setubv_make_fa_aa_bb 7.7%; model code (TinyCC JIT) 3.9%; setubv2.cpp:setubv 3.6%; my_rhs.cpp:pure_rhs 3.5% |
| auto_ode_lecar | 78,617,946 | miniz/miniz.c:tdefl_compress 12.7%; libc mem* 6.9%; core-math/tanh/tanh.c:cr_tanh_fma 6.4%; std::format/to_chars text output 4.8%; model code (TinyCC JIT) 4.1%; conpar2.cpp:anon::conpar_process 3.1%; core-math/cosh/cosh.c:cr_cosh_fma 3.1% |
| auto_ode_ma23 | 1,417,965,226 | model code (TinyCC JIT) 38.5%; core-math/exp/exp.c:cr_exp_fma 15.1%; core-math/pow/pow.c:cr_pow_fma 6.4%; miniz/miniz.c:tdefl_compress 3.2%; xpp_math.cpp:xpp::sgefa 2.7%; conpar2.cpp:anon::conpar_process 2.0%; my_rhs.cpp:my_rhs 1.5% |
| auto_ode_motoneuron6c | 46,640,080,991 | model code (TinyCC JIT) 32.8%; core-math/exp/exp.c:cr_exp_fma 12.8%; conpar2.cpp:anon::conpar_process 7.2%; xpp_math.h:anon::conpar_process 6.4%; core-math/pow/pow.c:cr_pow_fma 6.0%; xpp_math.cpp:xpp::sgefa 4.3%; autlib2.cpp:std::_Function_handler<void  3.5% |
| auto_ode_mu26_control | 200,405,626,864 | conpar2.cpp:anon::conpar_process 28.1%; xpp_math.h:anon::conpar_process 25.2%; autlib2.cpp:std::_Function_handler<void  14.2%; model code (TinyCC JIT) 10.9%; core-math/tanh/tanh.c:cr_tanh_fma 7.2%; /usr/include/c++/15/bits/move.h:anon::conpar_pro 2.3%; setubv2.cpp:anon::setubv_make_fa_aa_bb 2.1% |
| auto_tstauto_bigaut | 95,696,499,348 | xpp_math_auto.cpp:xpp::ge 36.9%; xpp_math_auto.cpp:xpp::anon::complete_eliminate_ 32.2%; auto_f2c.h:xpp::ge 27.1%; model code (TinyCC JIT) 0.6%; auto_jacobian.h:funi 0.4%; my_rhs.cpp:pure_rhs 0.3%; libc mem* 0.2% |

mu26 control, line level (200.4 G): conpar_process 55.6% inclusive: `subtract_scaled` (already vectorized by W475)
about 25% (42.5 G on its group-of-four line, 5 G on the scalar tail), `first_maximum` 23.5% (fabs, compare and a
rarely taken branch per element), the swaps 2.3%; brbd's condensation (autlib2.cpp 552-585, strided `-= rm *` loops
with `perm[]` indirection) about 7%; the model's `tanh` 7.2% (131 M calls, 114 instructions each).

## Findings

Class: (a) same bits; (b) reduction, a deliberate numerics change; (c) not worth it. "Measured" = a patch in
patches/ or a stand-alone kernel, bits compared; times are native, min of 3.

| # | where | share | what it does now | simplest change | class | gain |
|---|---|---|---|---|---|---|
| 1 | data_formats.cpp:48 `write_dat` (`format_append(text, "{:.8g} ", v)` per number); the other `{:.Ng}` and CSV `{}` writers | text output: mu26 cvode 91%, episodic 27%, PY_S1Bf 24%, amari 34%, clustor 18%, lecdiff200 about 25% | `std::vformat_to` scans the format string for every number, then `to_chars` | `std::to_chars(buf, v, general, 8)` straight into a stack buffer: the same call json_number.cpp:22 makes, so one helper in xpp_io (`xpp::append_general`) that json_append_number also uses; the text is printf's `%.8g` character for character (output.dat md5 identical on mu26 cvode; 100k random values identical) | a | **measured** `scratch/fmt.cpp`: 2087 -> 1123 instr/number; mu26 cvode 2.47 G -> 1.06 G, 0.171 s -> 0.086 s (-50%); amari 0.107 -> 0.079 s |
| 2 | flags.cpp:340-360 `one_flag_step` | koho3 57% (flags.cpp 29% + setvar 28%) | for each flag, re-stores every state (`setvar(j+1, ynew[j])`, `setvar(0, tnew)`) before evaluating its condition: neq x flags stores per step, the values are the same each time | store the state once before the loop over flags (a hoisted invariant; evaluating a condition does not write the state) | a | **measured** koho3 8.56 G -> 3.51 G (-59%), 0.579 -> 0.375 s, output.dat md5 identical |
| 3 | simplenet.cpp:`evaluate_network` CONVE (also CONV0, CONVP: same shape) | amari 50%, clustor 38% | per unit: `abs`, two range tests and a scalar add per tap, taps added in j order | interior units (all `i+j` inside [0,n)) four at a time with four scalar accumulators over j, boundary units as now: each unit's sum is still added in j order, so the bits are the same, and gcc's SLP turns the four into packed mulpd/addpd | a | **measured** instr amari -23% total (-46% of the function), clustor -28%; native `--noout`: amari 0.97 -> 0.58 s (-40%), clustor 0.61 -> 0.43 s (-30%); output md5 identical. Not tried: MMULT/DEL_MUL/DEL_SPAR are dots over j per unit: only (b) or (c) |
| 4 | xpp_math.cpp:262-338 `sgefa`/`sgesl`/`saxpy` (dense LU of the stiff, gear, Rosenbrock, backward-Euler integrators and AUTO equilibria) | sgesl+sgefa: gear 17% (saxpy 14.7%), stiff 14%, rosenbrock 12%, motoneuron6c 10%, backward Euler 10%, PY_S1Bf 9% | LINPACK's column algorithm on the row-major block: every `saxpy` has stride `lda` on both vectors, so nothing vectorizes and every access strides | sgefa: eliminate row by row (rows below k, columns k+1..n-1 contiguous: `row[j] += m * pivot_row[j]`, the same product per element, the `sa == 0` skip kept as a per-element select so zeros and infinities behave as now); sgesl's strided column reads stay (its updates are ordered across k) | a | **measured** `scratch/lu.cpp`: bits and pivots identical, sgefa 1.6x (n=28), 1.5x (100), 2.7x (400), 2.8x (1000). Expected 2-6% of a stiff workload at n=28, more for large n (the cubic part) |
| 5 | xpp_math_auto.cpp:14-45, 198-300 `ge` (+ `complete_pivot`, `complete_eliminate_row`) | bigaut 96% (ge 37% + eliminate 32% + the inlined indexing 27%); absent from the small-system runs | complete pivoting through `ir[]`/`ic[]`: the search reads `a[ir[i] + ic[j]*lead]` in row-then-column order (stride `lead` inner loop), the update gathers through `ic[]` | swap rows and columns physically (the multipliers are not stored in ge, so only the trailing block moves), update the trailing block down the contiguous columns with the next pivot's per-column maximum taken in the same pass, then the global maximum, ties to the smallest (row, column) exactly as `p > piv` chooses; `ic` kept for the solution's scatter | a | **measured** `scratch/ge.cpp`: determinant and solution bit-identical on dense, 75%-zero and integer (tie-heavy) matrices, n=2..400; 1.6x (n=50), 1.8x (150), 2.1x (300), 2.6x (600). The fp9 pivot print and the NOTE lines keep their order (the pivot value and index are the same) |
| 6 | jacobian.cpp:`jacobian` (forward differences) | 66-82% of every stiff or implicit workload above; 30-47% of AUTO equilibrium runs of ODE models | n full right-hand-side calls, one per perturbed variable, although most equations do not depend on it | evaluate only the equations that depend on the perturbed variable (a dependency list per equation from the compiled formulas, or from the exact-Jacobian code's structure); the others contribute `(f0-f0)/r`, written as that, so the bits (the sign of a zero included) are the same | a | not measured: the share is the ceiling, the gain is (1 - density); needs the dependency graph (touches the W469 whole-RHS function) |
| 7 | volterra2.cpp:154 `init_sums` | mlvolt and cuplamdif 50-60% (init_sums + evaluate + setvar) | every step re-sets all variables and re-evaluates every kernel formula at every history point (O(n^2) evaluations over a run) | cache `f(u_i)` per history point when the kernel formula does not read `t` or a changed parameter (checked when the model loads); the weighted sums stay as now | a | not measured; up to about -40% of the Volterra workloads (the evaluate and setvar share); examples using it: about 10, all small |
| 8 | autpp.cpp `continued_constants` (per `func` call) | hill_cycles_pd 7.7%, forced_pp_pd 5.0% | copies the continued parameters into the constants and compares bits on every right-hand-side call, though the parameters are the same for a whole residual evaluation | do it once per `par` change in the caller that sets the parameters (W465 touches the same function) | a | up to 5-8% of small-ODE AUTO runs |
| 9 | autlib2.cpp:552-585 (brbd condensation: `at(a2l, ir, perm[l]) -= rm * ...`, `at(s1l, ir, l) -= ...`) | mu26 control about 7%, lorenz about 3% | row operations across columns of column-major blocks: stride `nov`, `perm[]` gathers | a transposed (row-contiguous) layout of the condensation blocks would let `subtract_scaled` serve them | a | at most 4-5% of mu26 control, and a layout change through setubv2/brbd/conpar glue: not worth a card of its own |
| 10 | conpar2.cpp:`first_maximum`, `subtract_scaled` | mu26 control 23.5% and 25% | scalar max scan with a rarely taken branch; vectorized axpy | **tried and rejected**: (i) lane-wise max with lane indices: hill +30% instr, mu26 native 13.4 -> 14.3 s; (ii) lane-wise max, then a scalar scan for the first equal element: mu26 14.4 s; (iii) `-mavx2` for conpar2.cpp (no FMA, same bits): hill's conpar_process -10.6% instr | c | the scalar scan is branch-predicted and cheap; AVX2 needs runtime dispatch for 10% of one kernel |
| 11 | delay_handle.cpp `polint`/`get_delay` | fr 52%, dde small | 4-point Neville with zero-initialised arrays, a division per node, per delay per call | a closed form for the four equally spaced nodes would change the rounding (class b, deliberate); keeping the arithmetic leaves only the array zero-fill | c | at most 10% of delay models |
| 12 | odesol2.cpp adams (history shift copies `y_p[k][i] = y_p[k-1][i]`), rk4 stage loops, dormpri, gear's `ggear`, `do_shift` | adams 13% (2.3% the shifts), rk4 3.5%, `shift()` 8.6% of kuramot100 | small loops over `neq` (28-400) | a ring of history rows instead of copies would save about 2% of adams | c | below 3% everywhere |
| 13 | JIT model code (TinyCC) | 20-40% of every ODE workload (mu26 38%, motoneuron6c 40%, episodic 20%) | W472's subject: gcc -O2 compiled code is about 3x fewer instructions | see W472 | | the largest remaining share, outside this audit |
| 14 | `cr_tanh/cr_exp/cr_sin/cr_pow` (CORE-MATH, W159) | tanh 25% of mu26 (rk4), sin 62% of kuramot100, exp 17-19% of episodic/PY_S1Bf | scalar correctly rounded functions, 100-250 instructions per call | none that keeps the bits: a vector version would need the same correctly rounded results lane by lane | c | |
| 15 | json_number.cpp:30 `json_append_number_shortest` | not in any profile (state events) | `snprintf("%.15g")` + `strtod` + a second `snprintf("%.17g")` per value | `std::to_chars` shortest gives the same round-trip text except possibly in format (17 vs 15 digits); a text change, so a deliberate protocol change | c | |
| 16 | model load and parse, nullclines/direction field, histogram, FFT (vendored pocketfft), the server's `series`/`plots` events | load 47-55 M instr, flat (TinyCC compile, memcmp, malloc); nullclines: model evaluation (tanh/cosh) dominates, `do_cline` 3.9%; FFT: pocketfft 13% of a 475 M run; the data events cost 2% of a run | no loop worth a change | | c | |
| 17 | miniz `tdefl_compress` in the AUTO session save | 3.5% of explicit_homoclinic, 12.7% of lecar's AUTO run (short runs) | default level deflate of the session zip | vendored; the level is a format choice, not a loop | c | |

Reductions (class b): no sum, dot or norm is above 1% in any workload. The reductions that exist (CONVE's taps, MMULT,
ge's back substitution, `ddot` in AUTO's Householder code) are either small or are avoided by vectorizing across
independent outputs with the per-output order kept (finding 3), so no numerics change is proposed. Where a fixed-order
tree reduction would be needed it was not worth it.

Duplicated kernels a shared helper would serve (searched `-= rm *`, `-= factor *`, dot and argmax loops in core/,
AUTO's files included): the elimination update appears in conpar2.cpp (now `xpp::subtract_scaled`), autlib2.cpp 552-574
and 580-585 (strided, indirect), xpp_math_auto.cpp:43 and :282 (`ge`, gathered through `ic`), xpp_math.cpp `saxpy` (strided)
and the network convolutions; the "largest |x| with the first index" loop appears in `first_maximum`, `complete_pivot`,
`isamax` and brbd's pivot search (autlib2.cpp 510-524): four copies of one operation, which finding 10 shows is not worth
vectorizing but could still be one function (dupcheck does not see them: different shapes). `product_accumulate`
(autlib3.cpp:557) is the only user of its helper. Hot-path inefficiencies not in the table: none found beyond findings
2 (recomputed invariant), 8 (recomputed invariant) and 7 (recomputed values).

## Proposed cards, ranked by measured share x achievable gain

1. **Number text through `to_chars`, one helper** (finding 1; class a): `xpp::append_general(out, v, precision)` in
   xpp_io (the call json_number.cpp already makes), used by `write_dat`, the CSV writer (`{}` shortest: `to_chars`
   without precision), and the other `{:.Ng}` uses in output paths; byte-identical output (examples md5s). Measured -50%
   time on a 40k-row x 29-column run, -27% on amari; affects every `--silent` run that writes output.dat, i.e. the
   speed comparison against XPPAUT.
2. **Flag conditions: state stored once per step** (finding 2; a): a 4-line move, koho3 -59% instructions, -35% time.
3. **Convolution networks: interior units blocked** (finding 3; a): CONVE, CONV0 and CONVP; -30..-40% time on amari and
   clustor.
4. **Sparse finite-difference Jacobian** (finding 6; a): the largest ceiling (67-82% of implicit workloads), needs the
   equation dependency lists; measure the density of mu26, motoneuron6c, PY_S1Bf first; coordinate with W469.
5. **Dense LU row-oriented, `sgefa` and AUTO's `ge`** (findings 4 and 5; a): one card for both elimination kernels, one
   shared helper beside `subtract_scaled`; bits proven by stand-alone comparison (done here) and the examples/autocompare
   baselines; sgefa 1.5-2.8x, ge 1.6-2.6x, large systems gain most.
6. **AUTO's `continued_constants` once per parameter change** (finding 8; a): 5-8% of small-ODE AUTO runs; with W465.
7. **Volterra: kernel formula values per history point** (finding 7; a): up to -40% of the Volterra models, rare.

Not proposed: vectorizing `first_maximum` or AVX2 dispatch for conpar (finding 10: measured slower or 10% of one
kernel), the brbd layout (9), delay interpolation (11), adams copies (12), the correctly rounded libm (14).

## What could not be profiled

perf is blocked (callgrind only), so cache and branch effects were measured by native timing of experiments only; the
multi-thread AUTO profile (callgrind serialises threads); the page's interactive nullclines and Fourier/histogram
windows (their data paths were driven through `--ncdraw`, `--dfdraw` and `@ postprocess`, not through the page); the
Windows and macOS builds (every finding is source-level and compiler-independent, but gains are measured with gcc 15 on
x86-64 only).
