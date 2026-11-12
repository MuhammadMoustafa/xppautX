# W570: a faster compiler for the generated model code (study)

Card W570 Study: a faster compiler for the generated model code (#635). Docs only: nothing in the program
changed. It follows W560 Two-parameter curves and periodic branches slower than AUTO-07p (#625), which left
the generated code as the remaining gap, and W472 Study: an optimizing compiler for the generated model code
(#535), which measured gcc, clang and MIR on 14 models (its harness and table are in the git-ignored
`paper/benchmarks/compiler-study/`; its numbers are quoted below as W472's, not remeasured).

## The question

xppautX turns every model's right-hand side into C (core/expr_native.cpp) and compiles it in process with
TinyCC (core/xpp_tcc.cpp, `TCC_OUTPUT_MEMORY`, third_party/tinycc, LGPL-2.1). TinyCC does no optimisation, no
inlining and keeps every value in memory. W560 profiled mu26 and ma23 against AUTO-07p (gfortran): ma23's
cycles run 1.07 G instructions in TinyCC's code against 0.21 G in gfortran's, and `cr_exp` 0.66 G against
libm's 0.40 G. The first is a compiler matter; the second is the price of the correctly rounded functions
(bit-identity, W159) and not this card's.

## Measured here

Machine: AMD Ryzen 9 8945HX, 32 threads, WSL2 (kernel 6.6.87), gcc 15.2, clang 21.1.8, valgrind callgrind. The
program is master at eafe6bc1, with W469's whole-right-hand-side function `p_rhs` and W560's inlined user
functions. Method as W472's (its harness, adapted): a scratch build dumps the C xppautX hands TinyCC
(`XPP_DUMP_NATIVE`, not committed); one call is `p_rhs` on 200 states spread over the model's own `--silent`
run; every external call goes through the same table of correctly rounded functions, so only the code
generation differs; gcc and clang run `-O2 -ffp-contract=off -fno-semantic-interposition -fPIC -shared`, loaded
with `dlopen`; one `tools/heavy.sh --alone` run of five models. MIR could not be rerun: it is not installed
and nothing may be downloaded, so its figures are W472's (MIR v1.0.0, i7-11700F, on the code before W469 and
W560).

Workloads: mu26 and ma23 (examples/ode, the W560 periodic-run models), motoneuron6c (ModelDB 267710, W549's
periodic run), PY_S1Bf_bench (W246's integration and periodic benchmark) and lecdiff200 (the screen's network
integration model).

| model | compiler | instr/call (all) | model-code instr/call | model code vs TinyCC | identical to TinyCC |
|---|---|---:|---:|---:|---|
| mu26 | TinyCC | 6090 | 2621 | 1 | - |
| mu26 | gcc -O2 | 4650 | 1181 | 0.45 | yes |
| mu26 | clang -O2 | 4771 | 1302 | 0.50 | yes |
| ma23 | TinyCC | 1671 | 814 | 1 | - |
| ma23 | gcc -O2 | 1217 | 360 | 0.44 | yes |
| ma23 | clang -O2 | 1257 | 400 | 0.49 | yes |
| motoneuron6c | TinyCC | 7578 | 4118 | 1 | - |
| motoneuron6c | gcc -O2 | 5203 | 1743 | 0.42 | yes |
| motoneuron6c | clang -O2 | 5179 | 1719 | 0.42 | yes |
| PY_S1Bf_bench | TinyCC | 1703 | 825 | 1 | - |
| PY_S1Bf_bench | gcc -O2 | 1288 | 410 | 0.50 | yes |
| PY_S1Bf_bench | clang -O2 | 1254 | 376 | 0.46 | yes |
| lecdiff200 | TinyCC | 131183 | 61423 | 1 | - |
| lecdiff200 | gcc -O2 | 91581 | 21821 | 0.36 | yes |
| lecdiff200 | clang -O2 | 86379 | 16619 | 0.27 | yes |

Instruction counts are exact (callgrind, collected on the call only). What a call does outside the model code
is the correctly rounded math library, the same for every compiler: 3469 of mu26's 6090 instructions. The
model code is 43-54% of a call on the five models. Geometric means over the five models: model code 0.43x
(gcc) and 0.42x (clang) of TinyCC's instructions; a whole call 0.73x and 0.72x. Every value of every one of
the 200 states (`ydot` and the stored variables) is bit-identical to TinyCC's: 10 of 10 compiler-model pairs.

Times and compile times were measured too, but the laptop was shared with other agents' builds
(`heavy.sh --alone` keeps their heavy runs out, not their other load), so they are indications, not figures:
ns/call against TinyCC was 0.59, 0.49, 0.53, 0.62, 0.56 for gcc and 0.61, 0.66, 0.55, 0.41, 0.57 for clang
(mu26, ma23, motoneuron6c, PY_S1Bf_bench, lecdiff200; one run each; TinyCC's ma23 was 382 ns here against
253 ns on W472's quiet machine), and one `cc -shared` of a model took 0.5-2.8 s. W472's quiet-machine run
(0.86-0.89x over 14 models, with optimized code slower than TinyCC's on two models, a micro-architectural
effect it did not trace) says the real gain in time is smaller than the instruction ratio and varies by model.

### W472's measurements, quoted (14 models; before W469 and W560)

| | model code vs TinyCC (geometric mean) | all of a call | compile of one model |
|---|---:|---:|---:|
| MIR O1 | 0.95 | 0.99 | 1.2-176 ms (TinyCC: 0.1-5.4 ms) |
| MIR O2 (its default) | 0.48 | 0.76 | 1.2-155 ms |
| gcc -O2 / clang -O2 | 0.42 / 0.39 | 0.73 / 0.71 | 25-1219 ms / 40-722 ms, a process each |

MIR O2 was bit-identical on all 14 models. W472's gcc and clang ratios (0.42, 0.39) match what this run found
on today's code (0.43, 0.42), so MIR's 0.48 is taken to hold on today's code too: an assumption, and the first
thing a MIR card measures.

## What it is worth on W560's workloads

Applying 0.45 (MIR O2 or gcc, from the above) to W560's profile (its commit message: instructions of the run
alone; AUTO-07p for comparison). These are projections from W560's figures, not runs:

* mu26 hb_soma: 777 M, of which the generated code is 252 M. Saves about 140 M: about 640 M, below
  AUTO-07p's 712 M.
* ma23 cycles: 4105 M, TinyCC's code 1.07 G (gfortran's 0.21 G). Saves about 0.59 G: about 3.5 G against
  AUTO-07p's 2.78 G, which is about 45% of the 1.3 G gap. The rest is `cr_exp` (0.66 G against libm's 0.40 G)
  and the generated code's extra stores and division guards, which no compiler removes (W472: about 1250
  instructions for gcc against gfortran's 820 on mu26's equations).
* lp_soma (3188 M against 3025 M): W560 gave no share for the generated code; not projected.

An integration-heavy model gains with its model-code share (47% of a call on lecdiff200, 43% on mu26), so a
whole run gains about a quarter of its instructions, and on W472's quiet machine 0-25% of its time.

## The candidates

| | model code vs TinyCC | compile at load | size and build | platforms | licence | bit-identity |
|---|---|---|---|---|---|---|
| TinyCC (today) | 1 | 0.1-5 ms (2.3 ms mu26, 4.5 ms lecdiff200 here) | libtcc.o 310 KB text, one vendored directory | Windows UCRT, Linux, macOS arm64 and x64, all built and checked today | LGPL-2.1, vendored | the baseline: no contraction, no reassociation |
| MIR O2 | 0.48 (W472) | 1-155 ms in process (W472) | about +0.6 MB (mir 248 + mir-gen 306 + c2mir 386 KB text, W472); about 32 k lines of C, vendored in third_party/mir and built by the Makefile like libtcc | upstream CI covers Linux, macOS and Windows (x86-64 through MSVC), aarch64 Linux and Apple Silicon; the MinGW UCRT and clang64 builds, and Apple's rules for executable memory, are unverified (documentation and W472, not run here) | MIT; copied verbatim with an adapter header | 14 of 14 identical (W472); MIR fuses and reassociates nothing; its output is no stable contract, so the md5 checks are the proof |
| system `cc -O2` at run time | 0.43 gcc, 0.42 clang (here) | 0.4-2.8 s per model: a process and a temp .so | nothing added, but an installed compiler is needed: none on Windows or macOS by default, several toolchains | wherever a compiler is installed | none vendored | identical on 5 of 5 models with `-ffp-contract=off`, a flag we cannot enforce on the user's compiler |
| LLVM ORC with clang's front end | the clang back end: 0.42 | clang's own, hundreds of ms | libLLVM-21 is installed in WSL (100+ MB with the clang libraries): an optional build only, not vendorable | all three, a large build | Apache-2.0 with LLVM exception | as clang with `-ffp-contract=off`; not run here (no in-process C front end tried) |
| libgccjit | as gcc: 0.43 | gcc's own, hundreds of ms | not installed here; gcc must be present at run time | Linux and some Unix | GPL-3, copyleft: cannot be vendored under our rules | as gcc; its input is gcc's IR, not C text, so the generator would be rewritten |
| QBE, Cranelift, DynASM, copy-and-patch | not measured | fast | small (QBE, DynASM) or a Rust toolchain (Cranelift) | QBE has no Windows x64; DynASM is one hand-written back end per target | MIT, Apache-2.0 | by construction if the operation order is kept |

ORC, libgccjit, QBE and the rest are assessed from documentation and from what is installed, not run: nothing
may be installed or downloaded for this card. The last row has no C front end, so each would need a second
generator written from the RPN beside the one that writes C text.

## Bit-identity, what each candidate guarantees

The md5 baselines move only if some operation is computed differently. The correctly rounded functions are
our own call targets (`xpp::math`), so no compiler touches them. What a compiler can change is the rounding of
the four operations: contraction of `a*b+c` into an FMA (gcc and clang do it by default, `-ffp-contract=off`
stops it; TinyCC and MIR never fuse), reassociation and reciprocal rewriting (only under `-ffast-math`, which no
candidate turns on by itself), x87 evaluation (not an issue on x86-64 and arm64). Every pair measured here and
by W472 was equal to TinyCC's bit for bit (10 here, 56 in W472). So bit-identity excludes none of them, but it
is a property each build must prove, not assume: a card that brings a compiler reruns the examples md5s,
goldencheck, odexcheck and the autoX diagrams, with macOS arm64 (another back end) in the proof.

## Trust boundary

Nothing here changes the program. TinyCC and MIR compile in process a text built from indices and a table of
numbers (literals are host doubles in `N[]`, no model text is pasted into the C), so a model file cannot inject
code. A system compiler would make xppautX start a process and load a library it wrote: a boundary the program
does not have, and the second reason against it. A MIR card must keep the same properties: c2mir takes the C
text with no headers, the externals through an import resolver naming only the helpers we pass.

## Recommendation

1. Not the system compiler, LLVM ORC or libgccjit as the default: the same code quality as MIR (0.42-0.45x)
   but an installed compiler or a very large library, hundreds of ms to seconds a load, and a wider trust
   boundary. A developer-only switch is possible and not worth a card.
2. MIR O2 is the candidate: in process like TinyCC, MIT, 0.48x model code, 1-155 ms to compile, about +0.6 MB,
   bit-identical in 14 of 14 (W472). It closes about 45% of ma23's periodic gap and puts mu26's Hopf
   continuation below AUTO-07p on instructions (projections above). What stays is the math (`cr_exp`, `tanh`,
   `pow`) and the generated code's own stores and guards.
3. What is not known is MIR on MinGW UCRT and clang64 and on macOS arm64's executable-memory rules, so the first
   card is a spike on exactly those.
4. Start-up: 1-155 ms against TinyCC's 0.1-5 ms. Fine for a load a user waits for; a tiered design (TinyCC at
   load, MIR when an autoX run or a long integration starts) is a question for the maintainer, not decided here.

### Proposed cards (not added to the roadmap)

* A. Spike: MIR on Windows UCRT, clang64, macOS arm64 and x64: vendor MIR verbatim under third_party/mir with an
  adapter header, a throwaway build, compile and run mu26's `p_rhs` through c2mir, bit-compare with TinyCC;
  stop and report if one platform cannot run it. Study-sized.
* B. MIR as the model-code compiler behind a named setting (`tinycc` or `mir`), one compile entry taking either;
  examples md5s, goldencheck, odexcheck, autoX diagrams and servercheck unchanged; W560's periodic runs and
  the 14 models' load times measured; ASan on the JIT memory. Needs A and the maintainer's answer on tiering.
* C. Tiering, only if B shows start-up matters: TinyCC at load, MIR when a long run starts, results identical.
* D. Not a compiler card: the correctly rounded `exp`, `tanh`, `pow` cost more than the model code on several
  models (mu26: 3469 of 6090 instructions per call); a faster, still correctly rounded implementation is worth
  as much as the compiler and is a numerics card (the md5s must not move).

## Rerun

W472's harness with two edits: the dump block (`XPP_DUMP_NATIVE`, writing the C, the constants, the `N[]`
table and the externals before `tcc::Program::compile` in `Generator::build`) applied by hand, since
`patch-dump-native.diff` no longer matches today's expr_native.cpp, and the calls changed from per-equation
`p_eq<i>` plus `p_fixed` to the single `p_rhs` of W469; MIR left out. In a WSL clone: build, dump each model with
`XPP_DUMP_NATIVE=<dir> ./xppautX <model> --silent` (motoneuron6c writes its output.dat beside the model),
link the harness against `build/obj/libxppcore.a`, run the study's `run.py` with the backends `tcc`, `gcc`,
`clang` through `tools/heavy.sh --alone`. The scratch build and harness are not kept in the repository.

## W706 Spike: MIR on the generated model code, Linux results

Card W706 Spike: MIR compiles the model code on every platform (#781), this study's card A. Vendored MIR at master
commit `dc2c763` (2026-10-23; MIT; `third_party/mir`, every file upstream's, unedited; its
[XPPAUTX.md](../../third_party/mir/XPPAUTX.md) records source and checksum), an adapter of ours with the shape of
`xpp::tcc::Program` (`tools/mir-spike/xpp_mir.{h,cpp}`) and `make mirspike` (not in `all`, `xppautx` or `test`): it
compiles the C text xppautX hands TinyCC, recorded as fixtures in `tests/mir-spike/` (mu26, mu26's `p_rhs` alone,
ma23, W246's PY_S1Bf_bench, an Izhikevich neuron; `p_rhs`, the equations and the user functions, with the literal
table `N[]` and the externals through the same table of correctly rounded functions), with both compilers, calls
`p_rhs` on 40 states of each model's own run and compares every output (`ydot` and the fixed variables) bit for bit.
W472's models were run from a scratch copy too: 10 of them have a `p_rhs` and no network or table helper (amari and
kuramot100 need a Session), 11 fixtures with mu26's `p_rhs` alone.

**Why a commit and not v1.0.0.** The release (2024-05-27) is unusable at its default level 2: on mu26 and ma23 it
aborts (`mir-gen.c`: `try_spilled_reg_mem: Assertion 'n < 2' failed`; with upstream's `-DNDEBUG` "stack smashing
detected"). The function reloads a spilled register that is an operand of an instruction into a stack array of
`MAX_INSN_RELOAD_MEM_OPS` = 2 slots; the instruction is `dmul r, r, r`, one spilled register in all three operands,
so a third write goes past the array. It needs register pressure, which the whole right-hand side function (W469) has
and W472's per-equation functions did not. Upstream fixed it on 2026-08-18 (PR #468, the array is 3 and guarded); no
release has it, so the vendored version is the master commit. Upstream issue #475, a related `op_nums` overflow, is
still open; nothing here hits it. A scratch copy of v1.0.0 with the array at 3 had already given the results below.

Linux x86-64 (gcc 15.2, `-O2`, MIR built as upstream does, `-DNDEBUG`):

* Bit-identity: every output of every state equal to TinyCC's on all 11 fixtures, at levels 1 and 2 (no failure at
  either; the Makefile runs every fixture at both).
* Instructions (callgrind, one call, 40 states, collected on the call only; the correctly rounded math, the same
  for both, is the rest): the model code of mu26's whole `p_rhs` 103 240 (TinyCC) against 50 240 (MIR level 2), 0.49x,
  2581 against 1256 per call, and a whole call 240 236 against 185 636, 0.77x (6006 against 4641 per call); ma23
  31 800 against 16 040, 0.50x (a whole call 65 229 against 48 709, 0.75x); PY_S1Bf_bench 0.51x; izh_2003_rs 0.55x.
  Level 1: the whole call is 1.11x TinyCC's, no gain. These confirm W472's 0.48 on today's code.
* Compile time in the process (a loaded machine, so an indication): TinyCC 0.2-2 ms, MIR level 1 3-71 ms (mu26's
  whole text 71 ms, `p_rhs` alone 22 ms), level 2 3-82 ms.
* Size: MIR's three objects 808 KB of text (c2mir 328, mir-gen 270, mir 211, v1.0.0's sizes) against libtcc.o's 306 KB.
* Trust boundary: c2mir runs with `no_prepro_p`, so no `#include` reaches the file system (the adapter's check
  refuses one, a syntax error and an unresolved symbol, as `test_tcc` does for TinyCC); imports resolve only through
  the symbol table the caller passes; code memory is MIR's own `mmap` (`PROT_EXEC` pages flipped to RWX and back with
  `mprotect` while code is written; on macOS arm64 `MAP_JIT` with `pthread_jit_write_protect_np`, on Windows
  `VirtualAlloc` and `VirtualProtect`), never a file or a library the model names. MIR calls its error function for
  an internal error: it must not return, so the adapter `longjmp`s back out of MIR (the context is then left, a
  leak on that path). c2mir counts an error only when it has a stream to print it to: with no `message_file` a
  syntax error is a success (found by the adapter's own check); the adapter gives it a temporary file.
* The other platforms: CI run 43137255350 on `ci/W706-mir-spike` (c5b8e7be, 2026-11-12), a `make mirspike` step in
  `windows-core` (UCRT gcc 16), `windows-clang` (clang64) and `macos-core` (Apple clang, arm64): MIR built and ran on
  all three, and every output of all 5 fixture files (mu26, mu26_rhs, ma23, PY_S1Bf_bench, izh_2003_rs) was
  bit-identical to TinyCC's at levels 1 and 2. macOS x64 (release.yml only) and a signed, hardened macOS build
  (the allow-jit entitlement) are not covered.

What this changes in the recommendation: MIR level 2 is 0.49-0.55x on the model code on every fixture and bit-identical
on all of them, mu26 and ma23 included, at the master commit; card B can start from it (the release is not usable).
