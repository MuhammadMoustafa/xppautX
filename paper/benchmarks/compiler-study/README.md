# W472: an optimizing compiler for the generated model code (study)

Card W472 Study: an optimizing compiler for the generated model code (#535). Nothing in xppautX's
compile path was changed; this folder (git-ignored, like all of `paper/`) holds the harness and the results.

## What was measured

xppautX translates each model's RPN into C (core/expr_native.cpp) and compiles it with TinyCC
(core/xpp_tcc.cpp). `patch-dump-native.diff` (a debug patch, applied only to a scratch build, not committed)
writes that exact C (`XPP_DUMP_NATIVE=<dir>`: `model0.c`, plus `model0.manifest` with the constants, the literal
table `N[]` and the externals). `harness.cpp` then loads the same C with one compiler at a time and calls the
right-hand side as `my_rhs.cpp` does (`p_fixed`, then `p_eq0..p_eq<n-1>`; W469's whole-RHS function is not on
master) on 200 states spread over the model's own `--silent` run (`output.dat`).

* Compilers: TinyCC (xppautX's own `xpp::tcc::Program`, same options); gcc 15.2 `-O2`; clang 21.1.8 `-O2` (both
  `-ffp-contract=off -fno-semantic-interposition -fPIC -shared`, loaded with dlopen; no fast-math); MIR v1.0.0
  (commit 477d820, c2mir C front end + MIR generator in-process, optimization level O1 and O2, which is MIR's
  default; O3 gave identical code to O2 on every model and is not tabulated).
* Same math for every compiler: every external call (`f1_i`, `f2_i`) goes through one wrapper per index to the
  table xppautX itself uses (`xpp::expr::fun1/fun2`, CORE-MATH's correctly rounded functions), so only the code
  generation differs. The harness links xppautX's own `libxppcore.a`.
* instr/call: callgrind, only `call_rhs` collected (one call = fixed variables + all equations), divided by 200
  calls. "model-code" is what the compiled model code itself executes (JIT or .so); the rest of the total is
  the math library, the call wrappers and the loop (the same for every compiler). TinyCC's model code on mu26
  is 3790 here, matching the ~2,800-3,800 of the earlier profile.
* ns/call: min over 5 runs of the median of 31 batches (~20 ms each), pinned to one core (`taskset -c 2`), the
  machine otherwise lightly loaded (i7-11700F, performance governor).
* compile ms: TinyCC and MIR: median of 7 in-process compiles of the model's C (c2mir + load + generate, for MIR);
  gcc and clang: median of 5 wall times of the whole `cc -shared` command (includes starting the compiler and
  linking, which a JIT-style in-process use would not pay; it is not a fair in-process figure for them).
* identical: every ydot value and every variable of the 200 states compared bit for bit with TinyCC's.
* Models: examples/ode mu26, ma23, motoneuron6c (ModelDB 267710, from .ode through the converter),
  the screen models PY_S1Bf_bench, lecdiff200, kuramot100, amari, clustor (the converted .odex: the .ode's
  `w1_[j]` syntax is not accepted by the .ode reader), episodic, and five Izhikevich models
  (2003 rs, 2003 fs, 2004 c, 2004 e, 2004 g; they generate the same code, two of them differ by a constant).
  amari and kuramot100 call Session helpers (`h_net`, `h_tab`/`h_sft`); the harness has no Session, so those
  are plain-arithmetic stubs, the same for every compiler: their totals are not xppautX's, only the
  comparison between compilers is meaningful.

## Results

| model | compiler | instr/call (total) | model-code instr/call | ns/call | compile ms | identical to TinyCC |
|---|---|---:|---:|---:|---:|---|
| mu26 | TinyCC | 9043 | 3790 | 899 | 0.9 | yes |
| mu26 | gcc -O2 | 6507 | 1254 | 664 | 410.8 | yes |
| mu26 | clang -O2 | 6613 | 1360 | 702 | 229.1 | yes |
| mu26 | MIR O1 | 8740 | 3487 | 884 | 37.1 | yes |
| mu26 | MIR O2 (default) | 6952 | 1699 | 771 | 37.9 | yes |
| ma23 | TinyCC | 2737 | 1276 | 253 | 0.4 | yes |
| ma23 | gcc -O2 | 1844 | 383 | 179 | 138.4 | yes |
| ma23 | clang -O2 | 1875 | 414 | 185 | 102.4 | yes |
| ma23 | MIR O1 | 2529 | 1068 | 231 | 11.2 | yes |
| ma23 | MIR O2 (default) | 2080 | 619 | 213 | 11.1 | yes |
| motoneuron6c | TinyCC | 10309 | 5349 | 951 | 5.4 | yes |
| motoneuron6c | gcc -O2 | 6788 | 1828 | 598 | 500.5 | yes |
| motoneuron6c | clang -O2 | 6749 | 1789 | 606 | 289.5 | yes |
| motoneuron6c | MIR O1 | 9341 | 4381 | 810 | 40.3 | yes |
| motoneuron6c | MIR O2 (default) | 7481 | 2521 | 666 | 41.2 | yes |
| PY_S1Bf_bench | TinyCC | 2762 | 1035 | 235 | 0.2 | yes |
| PY_S1Bf_bench | gcc -O2 | 2150 | 423 | 185 | 70.4 | yes |
| PY_S1Bf_bench | clang -O2 | 2105 | 378 | 187 | 66.6 | yes |
| PY_S1Bf_bench | MIR O1 | 2763 | 1036 | 243 | 6.2 | yes |
| PY_S1Bf_bench | MIR O2 (default) | 2230 | 503 | 169 | 6.0 | yes |
| lecdiff200 | TinyCC | 137205 | 63420 | 15164 | 1.1 | yes |
| lecdiff200 | gcc -O2 | 96391 | 22606 | 10472 | 495.3 | yes |
| lecdiff200 | clang -O2 | 94190 | 20405 | 12972 | 575.0 | yes |
| lecdiff200 | MIR O1 | 120604 | 46819 | 14980 | 51.2 | yes |
| lecdiff200 | MIR O2 (default) | 100792 | 27007 | 11744 | 50.7 | yes |
| kuramot100 | TinyCC | 3613953 | 733700 | 381672 | 0.7 | yes |
| kuramot100 | gcc -O2 | 3263755 | 383502 | 281126 | 420.6 | yes |
| kuramot100 | clang -O2 | 3213954 | 333701 | 282171 | 260.3 | yes |
| kuramot100 | MIR O1 | 3455453 | 575200 | 391928 | 28.4 | yes |
| kuramot100 | MIR O2 (default) | 3234353 | 354100 | 377514 | 34.6 | yes |
| amari | TinyCC | 13441 | 7608 | 1596 | 1.0 | yes |
| amari | gcc -O2 | 12038 | 6205 | 1546 | 389.6 | yes |
| amari | clang -O2 | 11037 | 5204 | 1546 | 274.0 | yes |
| amari | MIR O1 | 14240 | 8407 | 2479 | 33.4 | yes |
| amari | MIR O2 (default) | 11240 | 5407 | 1869 | 34.0 | yes |
| clustor | TinyCC | 115555 | 57348 | 9430 | 3.3 | yes |
| clustor | gcc -O2 | 83566 | 25359 | 11621 | 1219.1 | yes |
| clustor | clang -O2 | 83864 | 25657 | 14744 | 721.7 | yes |
| clustor | MIR O1 | 117752 | 59545 | 19730 | 175.9 | yes |
| clustor | MIR O2 (default) | 82001 | 23794 | 16515 | 154.6 | yes |
| episodic | TinyCC | 69767 | 29245 | 6139 | 0.8 | yes |
| episodic | gcc -O2 | 51224 | 10702 | 4332 | 589.3 | yes |
| episodic | clang -O2 | 50241 | 9719 | 4471 | 311.5 | yes |
| episodic | MIR O1 | 63118 | 22596 | 5131 | 36.8 | yes |
| episodic | MIR O2 (default) | 54315 | 13793 | 4636 | 38.9 | yes |
| izh_2003_rs | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2003_rs | gcc -O2 | 124 | 43 | 11 | 25.3 | yes |
| izh_2003_rs | clang -O2 | 118 | 37 | 11 | 41.1 | yes |
| izh_2003_rs | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2003_rs | MIR O2 (default) | 128 | 47 | 13 | 1.2 | yes |
| izh_2003_fs | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2003_fs | gcc -O2 | 124 | 43 | 11 | 25.2 | yes |
| izh_2003_fs | clang -O2 | 118 | 37 | 11 | 40.1 | yes |
| izh_2003_fs | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2003_fs | MIR O2 (default) | 128 | 47 | 13 | 1.2 | yes |
| izh_2004c_tonic_bursting | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2004c_tonic_bursting | gcc -O2 | 124 | 43 | 11 | 25.0 | yes |
| izh_2004c_tonic_bursting | clang -O2 | 118 | 37 | 11 | 40.2 | yes |
| izh_2004c_tonic_bursting | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2004c_tonic_bursting | MIR O2 (default) | 128 | 47 | 13 | 1.2 | yes |
| izh_2004e_mixed_mode | TinyCC | 178 | 97 | 11 | 0.1 | yes |
| izh_2004e_mixed_mode | gcc -O2 | 124 | 43 | 11 | 25.2 | yes |
| izh_2004e_mixed_mode | clang -O2 | 118 | 37 | 11 | 40.3 | yes |
| izh_2004e_mixed_mode | MIR O1 | 186 | 105 | 14 | 1.2 | yes |
| izh_2004e_mixed_mode | MIR O2 (default) | 128 | 47 | 13 | 1.3 | yes |
| izh_2004g_class1_excitable | TinyCC | 184 | 103 | 11 | 0.1 | yes |
| izh_2004g_class1_excitable | gcc -O2 | 128 | 47 | 11 | 25.7 | yes |
| izh_2004g_class1_excitable | clang -O2 | 121 | 40 | 11 | 40.4 | yes |
| izh_2004g_class1_excitable | MIR O1 | 198 | 117 | 14 | 1.2 | yes |
| izh_2004g_class1_excitable | MIR O2 (default) | 132 | 51 | 13 | 1.2 | yes |

Geometric-mean ratios against TinyCC over the models (below 1 = fewer / faster):
| compiler | model-code instr | total instr | ns/call | compile ms (x) |
|---|---:|---:|---:|---:|
| gcc -O2 | 0.42 | 0.73 | 0.86 | 285 |
| clang -O2 | 0.39 | 0.71 | 0.89 | 266 |
| MIR O1 | 0.95 | 0.99 | 1.15 | 20 |
| MIR O2 (default) | 0.48 | 0.76 | 1.00 | 20 |

Reading the table:

* Bit identity: every value, every state, every model and every compiler (TinyCC, gcc, clang, MIR O1, MIR O2)
  is identical to TinyCC's, 56 of 56 non-trivial model-compiler pairs. With `-ffp-contract=off` and IEEE doubles this is
  expected (no reassociation without fast-math), and MIR does not fuse multiply-adds either.
* Model-code instructions fall to 0.42x (gcc), 0.39x (clang), 0.48x (MIR O2) of TinyCC's (geometric mean);
  MIR O1 barely optimizes (0.95x). On mu26: 3790 -> 1254 (gcc), 1360 (clang), 1699 (MIR O2); AUTO-07p's
  gfortran -O is ~820 for the equations. The C xppautX generates carries more than AUTO's equations do (it stores
  every fixed variable into v[], guards divisions with `dv`), so ~1250 is about what an optimizer can reach on
  this C; getting to 820 needs a leaner C (W469 and W471 work on that).
* Total instructions per call fall much less (0.73x gcc, 0.71x clang, 0.76x MIR O2): the correctly rounded
  `tanh`/`pow`/`exp` of CORE-MATH cost more than the model code on several models (mu26: 5253 of 9043
  instructions are not model code, 2416 of them in `tanh`, 1644 in `pow`). That floor is not a compiler matter:
  even a perfect compiler leaves mu26 at ~5300-5500 per call against AUTO-07p's ~820 (libm there).
* Time: gcc/clang about 0.86-0.89x of TinyCC's ns/call (geometric mean), MIR O2 about 1.00x. Instructions are the
  stable measure. ns/call is not, on clustor and amari: optimized code there is *slower* than TinyCC's
  (clustor: TinyCC 9.4 us, gcc 11.6, clang 14.7, MIR O2 16.5) although it executes 28-30% fewer instructions in
  the model code. The same gcc -O2 C, with one unrelated flag changed, ranges from 9.5 to 17.5 us on clustor
  (`-falign-functions=64` 9.5, `-O1` 8.0-8.4, `-fno-schedule-insns2` 17.5), and a shift of ydot against v
  (`YPAD`) changes nothing, so it is a micro-architectural effect of the generated code (clustor makes ~730 calls
  of `exp` per right-hand side), not noise and not the arrays' alignment; not traced further. Do not read the
  ns column of those two models as a compiler ranking. Where code is a straight chain of arithmetic
  (mu26, ma23, motoneuron6c, lecdiff200, episodic) gcc/clang gain 15-35% in ns/call, MIR O2 14-30%.
* Compile time: TinyCC 0.1-5.4 ms; MIR O2 1.2-155 ms (8-50x TinyCC); gcc 25-1219 ms and clang 40-722 ms as a
  separate process (90-720x TinyCC). A model such as mu26 takes 0.9 ms with TinyCC, 38 ms with MIR, 411 ms with gcc.

## Recommendation

**If the maintainer wants the gain, MIR O2 is the compiler to try; gcc/clang are not shippable as the
compiler, and TinyCC stays the fast-start fallback.**

* gcc/clang give the best code (model code ~0.4x, ns 0.86-0.89x) but cannot ship: a user would need an installed
  C compiler (none on Windows/macOS by default; MinGW/UCRT, clang64 and Xcode toolchains differ), a process spawn
  and a temp .so per model, 0.1-1.2 s of start-up per model, and a security surface on a trust boundary (a model
  file makes xppautX run a compiler and load a library; today the C is trusted, built from indices, and
  compiled in-process). Fine for a developer option, not for the default.
* MIR is an in-process library like TinyCC: MIT licence (TinyCC is LGPL-2.1, already vendored), C sources
  (~32k lines: mir.c, mir-gen.c, mir-x86_64.c/-aarch64.c, c2mir/), upstream CI on Linux, macOS and Windows
  (x86_64 via CMake/MSVC; aarch64 Linux and Apple Silicon too). Object text size at -O2: mir 248 KB +
  mir-gen 306 KB + c2mir 386 KB = ~0.94 MB (x86_64), against 310 KB for TinyCC (libtcc.o), so about +0.6 MB
  of binary. It would be vendored in third_party/mir and built by the Makefile like libtcc; Linux and macOS
  should be straightforward; the MinGW UCRT and clang64 builds are NOT verified here (upstream tests Windows
  only through MSVC; the `_WIN32` paths exist in mir-x86_64.c and c2mir) and would be the first thing a
  follow-up card must try. c2mir takes the same C text (no includes needed), the same `extern` helpers by
  address (an import resolver instead of `tcc_add_symbol`), and `N[]` is found as the bss item.
* Start-up: +1 to +155 ms per model against TinyCC's ~0.1-5 ms: below human notice for mu26-size models (38 ms),
  noticeable for clustor/lecdiff-size ones (50-155 ms), and paid once per load (a model recompiles on a
  parameter-table change only if that already happens today). A tiered use would keep TinyCC for the first
  run and switch only when a long run (AUTO, a long integration) starts; that is a design question for a card.
* Expected gain against AUTO-07p: MIR O2 (or gcc) brings the model code on mu26 from 3790 to ~1700 (~1250),
  so per call 9043 -> 6950 (6500), -23% (-28%) instructions and about -14% (-26%) time on mu26. AUTO-07p's
  ~820 is not reachable by the compiler alone: the correctly rounded math functions (5.3k instr/call on mu26)
  and the generated C's extra stores and guards remain. A cheaper, bit-identical `tanh`/`pow`, and W469/W471's
  leaner C, are worth as much as the compiler change on math-heavy models. On models whose time is the math
  (kuramot100: 80% outside model code) a compiler changes little (0.9x of total).
* So the honest summary: an optimizing compiler removes 50-60% of the model code's instructions on every
  model, which is 20-30% of the total per call and 0-25% of the time per call, bit-identical; it costs
  +0.6 MB, 8-50x the compile time of TinyCC, and an unverified MinGW port. Whether that is worth a card is
  the maintainer's call; if yes, MIR O2 first, behind a switch, measured again on the real RHS (W469).

## Rerun

All commands from the checkout root unless noted; the Python goes through uv.

1. A scratch build of xppautX with the dump patch (not committed): in a worktree,
   `git apply paper/benchmarks/compiler-study/patch-dump-native.diff && make -j4 xppautx`, then
   `git checkout core/expr_native.cpp` (the built `xppautX` keeps the dump).
2. `X=<path to that xppautX> paper/benchmarks/compiler-study/collect.sh` (models/ holds the sources; writes data/).
   motoneuron6c's `OUTPUT=output.dat` is written next to the model: move `models/output.dat` to
   `data/motoneuron6c/`.
3. MIR: `git clone --depth 1 --branch v1.0.0 https://github.com/vnmakarov/mir paper/benchmarks/compiler-study/mir-v1.0.0`
   and `make -j4 libmir.a c2m` inside it (nothing installed).
4. `W=<xppautX tree> paper/benchmarks/compiler-study/build_harness.sh`.
5. `uv run --locked python paper/benchmarks/compiler-study/run.py [MODEL...]` (callgrind runs under valgrind: about
   an hour for all models, MIR under valgrind is slow on the big ones); `RETIME=1` repeats only the times;
   `uv run --locked python paper/benchmarks/compiler-study/report.py > table.md`.

Files: `harness.cpp`, `build_harness.sh`, `collect.sh`, `run.py`, `report.py`, `patch-dump-native.diff`,
`results.json` (all numbers), `table.md`, `models/` (the sources used), `run.log`, `retime.log`.
