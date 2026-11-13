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

## W711 Spike: the Generator emits MIR's IR directly, without c2mir (#786), Linux results

Card W711 (#786), after W706. The maintainer's decision: take from MIR only what we need; no compiler of our own,
TinyCC untouched. W706 sent the C text that `core/expr_native.cpp`'s Generator writes for TinyCC through c2mir, MIR's
C front end (334 KB of the 848 KB of MIR's text, and 30-40% of the compile time). Here the same walk of the
model's RPN also writes expression trees and statements (`core/expr_ir.h`), and `tools/mir-spike/xpp_mir_ir.cpp`
builds the module through MIR's API (`MIR_new_module`, `MIR_new_proto_arr`, `MIR_new_import`, `MIR_new_func_arr`,
`MIR_new_insn`, `MIR_new_call_insn`, `MIR_load_module`, `MIR_link`, `MIR_gen` through
`MIR_set_gen_interface`). Only `mir.c` and `mir-gen.c` (vendored, unedited) are linked into it; `make mirspike` fails
when an object of the new path names c2mir. W706's path is kept beside it for the comparison.

**What it covers.** `p_rhs` of mu26, ma23, Izhikevich's 2003 regular-spiking neuron and mu26's `p_rhs` alone: the
fixed variables and the equations, with arithmetic, the correctly rounded math functions, the XPP guarded division,
conditional expressions, integer powers, the interpreter's table/network/kernel helpers as calls, and user functions
inlined as the C does. Not lowered, and said so by `RhsIr::unsupported` (the spike skips them): a sum (a loop), a
conditional that runs statements (if/else), a call of a user function that is not inlined (PY_S1Bf_bench), a call of
the function compiled for an equation that has a sum (wcring, kuramot100), a network's operands. The other
programs (user functions, one function per equation, derived quantities, the exact Jacobian's functions) stay with
TinyCC in the core.

**The design, and what was not done.** One walk, two outputs: every `Node` of the Generator carries, beside its C
text, the index of the same value in `RhsIr::nodes`, and `statement`/`effect`/`join`/`rhs` push the statements beside
their text (`Generator::trees`; off, every `ir_` call does nothing, and the text and the md5s of all 216 examples are
unchanged). `xpp::rhs_ir(Session&)` returns the trees of `p_rhs` and compiles no C. Numbers are the literals' values
(MIR double operands, or the table `N` in the C), indices are indices, and every function the code calls is a row of
`RhsIr::callees` (name and address from our tables): the only imports. Rejected: (1) reading the emitted C text back
into MIR (a front end of our own, and it takes text); (2) one tree as the single representation with the C writer as
a second emitter of it: the better end state, but the Generator's call reuse (`calls_made`, `call_readers`) and
inlining work on the text, so it would rewrite them; card B can do it if TinyCC retires, until then the text path is
byte-identical. The price of the choice is two outputs kept in step in the Generator until TinyCC goes.

**Bit-identity.** `p_rhs` outputs (`ydot` and the fixed variables) compared with TinyCC's on the 40 states of each
model's run and four of ours (all 0, all NaN, all -0.0, all 1: every division guard and condition, NaN included; two
NaNs compare equal, as which one an operation passes on is the CPU's by operand order): 0 differences in both
paths, levels 1 and 2, on 4 of 5 fixtures (PY_S1Bf_bench has no direct path). Operations are the C's, in its order:
`(right + left)` for add and multiply as the text has them, the guarded division as a branch on `divisor != 0`
(`dbne`: NaN is not 0, as in C), a condition as the same. MIR's x86-64 and aarch64 generators have no FMA pattern
(none in `mir-gen-x86_64.c`), and the build keeps `-ffp-contract=off`; the comparison would show a fused operation as
a difference. One finding on the way: a condition written as `deq` then `bf` costs a compare, a flag and a test (62
of them in mu26, 28% more instructions than c2mir's code); `dbne` on the doubles (ucomisd, jp, jne) matched it.

**Per call** (callgrind, instructions, `p_rhs` on 40 states, per call = total / 40; the correctly rounded math, the
same for all, is the difference between "model code" and "whole call"):

| model | | TinyCC | c2mir L1 | direct L1 | c2mir L2 | direct L2 |
|---|---|---|---|---|---|---|
| mu26 | model code | 2581 | 3261 | 1995 | 1256 | 1275 |
| | whole call | 6034 | 6674 | 5240 | 4669 | 4520 |
| ma23 | model code | 795 | 958 | 606 | 401 | 394 |
| | whole call | 1640 | 1784 | 1378 | 1227 | 1166 |
| izh_2003_rs | model code | 77 | 116 | 69 | 42 | 46 |
| | whole call | 89 | 127 | 80 | 53 | 57 |

At level 2 the direct code is as good as c2mir's (0.49x TinyCC's model code on mu26 and 0.50x on ma23, 0.60x on the
tiny izh; the whole call 0.75x, 0.71x, 0.64x). At level 1, where c2mir was a loss (1.1x TinyCC's whole call), the
direct code is a gain (0.87x, 0.84x, 0.90x). The first version, with each load into a register of its own and a `deq`
for each condition, was 1.3x c2mir's at level 2; MIR folds a memory operand into the instruction only when the IR
has it there, as c2mir's does.

**Compile time** (best of 7 in one process, a loaded machine, so an indication; ms; "walk" is `rhs_ir`: the trees of
every program of the model, the text included, an upper bound of building the trees of `p_rhs`; c2mir is the whole
text, except mu26_rhs, which is `p_rhs` alone; direct is `p_rhs` alone):

| model | level | TinyCC | c2mir (front, generate) | direct (lower, generate) | walk |
|---|---|---|---|---|---|
| izh_2003_rs | 1 / 2 | 0.20 / 0.20 | 1.47 (0.62, 0.84) / 1.51 (0.65, 0.84) | 0.36 (0.06, 0.31) / 0.38 (0.06, 0.32) | 0.04 |
| ma23 | 1 / 2 | 0.36 / 0.40 | 10.09 (3.35, 6.73) / 11.61 (3.51, 7.92) | 1.83 (0.14, 1.69) / 2.53 (0.14, 2.39) | 0.23 |
| mu26 | 1 / 2 | 0.96 / 0.99 | 37.69 (12.12, 25.45) / 44.13 (12.91, 30.95) | 6.27 (0.38, 5.82) / 10.44 (0.35, 10.06) | 0.70 |
| mu26_rhs | 1 / 2 | 0.35 / 0.37 | 12.46 (2.87, 9.31) / 15.88 (2.77, 12.57) | 6.00 (0.36, 5.64) / 9.81 (0.34, 9.40) | 0.76 |

Building the IR is 0.04-0.8 ms and the lowering through the API 3% of the direct compile on mu26 (6% on ma23, 16% on
the tiny izh): MIR's own optimizer and code generator are the rest, and c2mir's front end was 30-40% of c2mir's. On the same `p_rhs` (mu26_rhs) direct is
1.6x faster than c2mir at level 2; against TinyCC it is 25x slower (9.8 ms against 0.37). The extra ~9 ms of mu26 at
level 2 is paid back after about 19 000 calls (4700 RK4 steps) at the 1500 instructions saved per call, at about 3
instructions per ns (an assumption); ma23's ~2 ms after about 13 000 calls (3300 steps); a short run on a small
model loses.

**Size** (text bytes of the objects, `-O2 -g`): the direct path 511 KB: `mir.o` 212, `mir-gen.o` 269, the adapter 14
and the lowering 16; c2mir adds 337 (`c2mir.o` 334 and its adapter 3): 849 KB with it; `libtcc.o` is 306 KB. The
lowering is 190 lines, the Generator's changes about 200 (the helper prototypes are now made from their shape,
`prototype()`, one source).

**Trust boundary.** The new path takes no C text and no file: its input is `RhsIr`, built from the validated RPN
(`expr_decode.h`) by the Generator, every name an index, every number a double, every callee a row of our table.
Imports resolve only through that table (the same `resolve` as the c2mir path; an unresolved import fails the link).
What an input can still reach: any function in our callee table (the math and helper functions a model may name
anyway); `c[k]` and `v[k]` for the indices the parser accepted (as the C path: no bound is checked in the code); a
model's size (the number of nodes, and MIR's superlinear optimization time at level 2, which the C path has too); a
tree deeper than 4096 is refused (the lowering recurses on it: a hostile deep expression cannot exhaust the host's
stack), and a copy of an inlined user function is bounded by the 512 characters of its text. MIR is not a sandbox:
its code memory is its own `mmap`, RWX only while code is written, and an internal error `longjmp`s out of MIR (the
lowering keeps its state outside that frame, so no destructor is skipped; the context leaks on that path).

**Recommendation for card B: adopt MIR this way, at level 2, for `p_rhs`, with TinyCC kept for the rest.** It gains
0.64-0.75x of TinyCC's whole `p_rhs` call (0.5x of the model code) bit-identically, for 511 KB of text (+205 KB over
TinyCC's 306 KB, the two together until TinyCC retires) and 0.4-10 ms of compile time per right-hand side against
0.2-1 ms: a loss for short runs, a gain from some thousands of steps. Costs: the second output of the Generator
kept in step until TinyCC retires (the trees now carry every construct but sums, if/else statements and
user-function calls, which card B lowers next: a loop is two labels and an int register each; a user function is a
function of its own with the arguments; the exact Jacobian's derivative functions are a plain list of statements and
the cheapest next target), the level choice (level 1 is a gain on the whole call but 6 ms of compile on mu26), an
adapter (`xpp_mir.cpp` shared state and error handling; `xpp_mir_ir.cpp`) in `core/`, the `MAP_JIT` and entitlement
question on macOS (W706's open item), and a decision on when to compile (a short run may stay interpreted or on
TinyCC first). Not adopting MIR is the alternative if most runs are short: TinyCC's 0.2-1 ms is invisible there.

## W714 Adopt MIR for all native code and retire TinyCC (#789), Linux results

Card W714 (#789), after W706 and W711. The maintainer's decision (2026-11-12): one compiler and one output of the
Generator ("Unify always"), MIR for every program, TinyCC retired. Two commits of the card: the Generator and the
adapter (this section's design and bit-identity), then the deletions and the measurements.

**What changed.** `core/expr_native.cpp`'s Generator writes only `core/expr_ir.h`'s trees and flat statements for
every program it compiled to C before: the right-hand side `p_rhs`, each equation, each user function (a function of
its own, called, or its tree copied into the caller when it is one expression of at most 64 nodes: the same rule as
the old 512 characters, which keeps bit-identity trivially, since the call and the copy do the same operations on
the same values), the bridge the interpreter calls for a user function, the derived parameters, DAE, Markov,
kernel, event and network programs, and the exact Jacobian's derivative functions with their workspace.
A sum is a loop (statements `LoopBegin` and `LoopEnd` with an integer counter, the counter written to `c[SUM_INDEX]` as
a double each time, the bounds truncated to 32-bit ints), an `if` that runs statements is `IfBegin`, `IfElse`, `IfEnd`,
and a network pair formula's operands `(double)R[k]` a node of their own. One walk, one output: `C text` is gone
(`write`, `signature`, `prototype`, `with_arguments`, the `dv` helper and the literal table `N[]`). What keyed on text
now keys on the node: an expression is one node however often it is written (the Generator makes a node once, by
its fields), so a math call made before is found by its node, and what a call may read (W559's index) is read from
the node's tree, iteratively. `core/xpp_mir.{h,cpp}` (the W711 adapter moved into `core/`, one module: the c2mir path,
`tools/mir-spike`, `tests/mir-spike` and `make mirspike` are deleted, `test_mir.cpp` takes `test_tcc.cpp`'s and
`test_expr_ir.cpp`'s place) lowers the module through MIR's API; `third_party/tinycc`, `tools/tinycc`, `xpp_tcc.*`,
the Makefile's TinyCC block and c2mir are gone.

**Design points.** (1) Imports: every external callee is loaded into MIR's environment by us (`MIR_load_external`
from the callee table, the same `xpp::math::resolve` addresses) and MIR's link gets no resolver, so a name outside the
table cannot resolve (no global of ours for it). (2) Module functions (user functions, equations the right-hand side
calls) are forward-declared by name when first called; a call of a name the module lacks is an error at the
model's place. (3) The error paths: what we find wrong while building (a call of a function the module lacks, a tree nested
too deep) sets a reason and builds nothing further, and MIR is finished as usual; an error of MIR's own (memory
exhausted, a bug of ours: its error function has no user data) records its text in one file-scope struct and
`longjmp`s back to the compile, the frames between holding only trivial locals (every scratch string and vector is a
member of the state outside). MIR's heap is kept in a list (`Arena`, through `MIR_init2`'s allocator), so such a
compile frees every block without MIR's help (MIR cannot be finished in the middle of a function: finishing it
read a freed string and crashed in the first version of this code); a failure while it links or generates finishes
it normally. Left behind, only on an error inside MIR while it builds: the code reservation's 128 MB of address
space (no memory). The unit test makes MIR refuse a repeated name (the error inside MIR), and refuses a missing
function and a tree too deep; `asancheck`'s unit-test phase runs it under LeakSanitizer.
(4) The levels are a named constant per kind of function with their measurements as the reason (below). (5) Rejected:
a second representation or a C writer kept beside the trees (the card is to have one); lazy generation of the cold
functions (`MIR_set_lazy_gen_interface`: MIR's lazy generation has no lock, and autoX's collocation workers call the
native functions from up to four threads, so a first call from two at once would race; see the open items).

**Bit-identity.** The interpreter is the reference of the whole project and TinyCC's code was proved equal to it, so
the check is end to end and by function: all 216 examples' `output.dat` md5s unchanged (`tools/examples_check.sh`, the
baseline made with TinyCC), `autoxcompare` 85 models, 31482 diagram points byte for byte (exact Jacobian
included), goldencheck, odexcheck, 3086 protocol checks, and `test_compile`'s 24081 checks, which run each
compiled program and the interpreter from the same state and compare every output bit, NaN payloads included, and
what they leave behind. The compiled counts per model ("Compiled N of M programs") are equal to TinyCC's for all 165
examples that log one (the same programs refuse for the same reasons). MIR fuses and reassociates nothing (no FMA
pattern in either back end; the build keeps `-ffp-contract=off`), the operations are written in the C's order
(`right + left` for add and multiply), the XPP division is a branch on `divisor != 0` (a NaN divisor is not 0), a
conditional a branch on `cond != 0`. One difference of definition, not of result: a sum's bounds are truncated with
`D2I` and sign-extended from 32 bits where C's `(int)` was the CPU's own conversion; they agree for every bound a
sum can run (an out-of-range bound is undefined in the interpreter's own `static_cast<int>` too).

**Levels** (`perf stat -e instructions:u`, whole process, instructions in millions; "tiny" is the
same model with `total_time` one step, so tiny minus TinyCC's tiny is the compile difference; the best level
of each kind found by the experiments below):

| model | TinyCC full | L2 full | L1 all | L0 all | L2 tiny | TinyCC tiny |
|---|---|---|---|---|---|---|
| lecar | 14.3 | 22.6 | 22.2 | 20.6 | 17.9 | 9.2 |
| mu26 | 2706 | 2455 | 2624 | 3121 | 186 | 36 |
| ma23 | 4256 | 3505 | 3896 | 4763 | 47.4 | 11.4 |
| kuramot100 | 30354 | 27576 | 28013 | 30216 | 227 | 41 |

Rhs at level 1 against 2 (the other kinds at 2): mu26 2631 against 2455, ma23 3898 against 3505; the equations at level 1
(kuramot100's sums): 28012 against 27576. So 2 for what runs every step. The exact Jacobian (mu26 with
`jacobian.kind: exact`, backward Euler: the Jacobian is evaluated every step) at levels 0, 1, 2: whole run 1406, 1309,
1535 M against TinyCC's 1082 M; its compile alone 125, 173, 443 M against TinyCC's 16 M for 1120 values; so level 1
(mu26's 1120 values compile in 17 ms of wall time at level 1, TinyCC's in 1 ms).

**Results** (this machine, Linux x86-64, gcc 15.2 `-O2`; instructions are user instructions, time the process's
task-clock mean of 7 runs; before is the TinyCC build of the branch's parent):

| | TinyCC | MIR | ratio |
|---|---|---|---|
| lecar, `--silent` (601 rows) | 14.3 M, 4.05 ms | 22.6 M, 5.52 ms | 1.58, +1.5 ms |
| mu26, 400 time units | 2706 M, 222 ms | 2455 M, 207 ms | 0.91, 0.93 |
| ma23, 1000 time units | 4256 M, 332 ms | 3505 M, 273 ms | 0.82, 0.82 |
| kuramot100 | 30354 M, 3197 ms | 27576 M, 2616 ms | 0.91, 0.82 |
| compile, lecar / ma23 / mu26 / kuramot100 (tiny minus TinyCC's tiny) | | +1.3 / +5.0 / +17.5 / +22 ms | |
| `tools/examples_check.sh`, 216 models, wall | 7.0 s | 5.7 s | 0.81 |
| stripped program | 13.02 MB | 13.25 MB | +0.23 MB |

**The finding, and what was done.** MIR's code is faster (0.82-0.91 of the instructions of a whole run of the three
models that run for thousands of steps, 0.5 of TinyCC's model code per W711), and its compile is 10 to 20 times
TinyCC's (lecar's module of 11 programs 7.4 M instructions at level 0, 10.8 M at level 2; ma23's of about 60 functions
41.5 M at level 2, most of it a fixed cost of about 0.5 M for each function, whatever its size). A run of a few hundred steps on a small
model is therefore longer by the compile difference and no more (lecar's 601 rows: 22.6 M against 14.3 M, the
difference 8.3 M is the compile's 8.7 M), and lecar's run is 36% longer in wall time (1.5 ms of 4); no level changes that
much (level 0 saves 2 M of lecar's 8.7 M but loses 10% to 36% of the long runs). Not fixed on this card:
the cure is to compile a function when it is first called, or the cold ones (the bridges, the fixed variables' and
derived parameters' own functions, which the interpreter calls on a parameter change) after the run has shown it needs
them, and MIR's lazy interface has no lock against the autoX workers; that is a design of its own (a lock around
the first call, or the cold programs on a tier that compiles on the core thread when the interpreter first reaches
them), and the card is recommended below.

**Trust boundary (what was checked).** A model file a user was sent reaches the Generator; the code MIR compiles is
a function of the trees only. (a) No text, name or path of the model enters MIR: every index comes from the validated
RPN, every number is a double; callee names are ours (`f1_N`, `f2_N`, `h_*`, `uN`, `p_eqN`). (b) A callee resolves
only through the table: an unknown name fails the compile with an error at the model's place, never a lookup in the
process. (c) A tree nested deeper than 1024 is refused by the lowering's recursion guard (the parser's own limit is 256 levels; a level costs 0.7 KB of stack, 5 KB under AddressSanitizer, against the core thread's 8 MB); the Generator's own walks
are iterative (a sum nests at most 16 deep), an inlined copy is bounded by 64 nodes, a call with the wrong number of
arguments is refused. (d) Executable memory is MIR's own mapping (`PROT_EXEC`, writable only while code is written;
`MAP_JIT` with `pthread_jit_write_protect_np` on arm64 macOS; `VirtualAlloc` on Windows); no file or library is
named by a model. (e) A failure inside MIR leaves no state (unit test, and `asancheck`'s unit tests under
LeakSanitizer). (f) What an attacker can still do: make a compile take long (a model's size is the compile time,
15 us a value of the exact Jacobian, whose cap of a million values is now some fifteen seconds of compile) and make the compiled code read `c[k]` and `v[k]`
at an index the parser let through, as TinyCC's did; MIR is not a sandbox, so a hostile *tree* (not reachable from
the RPN decoder) could call any function of the callee table, which is the interpreter's own set of functions.

**macOS and Windows.** MIR's own code allocator already does what Apple's rules want on arm64: `mmap` with `MAP_JIT`,
`pthread_jit_write_protect_np` off while writing and on afterwards with `sys_icache_invalidate`, per thread (the compile
and every write happen on the core thread; other threads only run the finished code). xppautX's macOS releases
are unsigned (release.yml: Gatekeeper asks the user to allow them), so no hardened runtime applies and no
entitlement is needed; a signed, hardened build needs `com.apple.security.cs.allow-jit` (third_party/mir/XPPAUTX.md says so).
x86-64 macOS maps `PROT_EXEC` pages and `mprotect`s them writable, which unsigned binaries may. Windows: `VirtualAlloc`
and `VirtualProtect` (W706's CI run built MIR with UCRT gcc and clang64 and ran it). Not run here, so for CI to confirm:
the macos-core and macos-ui jobs' examples md5s (the whole of `examples_check` now runs native code through MIR on
arm64), windows-core, windows-clang (clang 22: the adapter without an `extern "C"` wrapper around MIR's headers, which
guard themselves), and the sanitizer jobs (`linux-sanitizers`, `windows-clang-sanitizers`, `macos-sanitizers` run
`test_mir` and every model through MIR).

**Open items.** (1) Compile cost: a fixed 0.5 M instructions per function at level 2; recommended card: compile the cold programs (and the Jacobian, which is already on first need) when first
called, under a lock, or interpret a run's first N steps while the core compiles. (2) The exact Jacobian's cap of a
million values is some fifteen seconds of compile. (3) MIR's generated code is the same on x86-64 and arm64 only by the
md5s (checked on x86-64 here, on arm64 by CI).

## W715 MIR's compile cost: short runs and exact-Jacobian runs no slower than with TinyCC (#790), Linux results

Card W715 (#790), after W714, which found MIR's compile 10 to 20 times TinyCC's. The maintainer's order
(2026-11-13): no run may be slower than it was with TinyCC. Measured on the same machine as the W714 section
(`perf stat -e instructions:u`, whole process, millions of instructions; the TinyCC program is commit 006ac0d8,
W714's parent, built in a scratch worktree; "tiny" is the model with `total_time` one step).

**Where the compile's instructions go** (callgrind of lecar's, ma23's and mu26's one-step runs, and a scratch
harness compiling synthetic modules through `xpp::mir::Program::compile`). W714 took the cost for a fixed 0.5 M
per function; it is not: it is some 20 thousand instructions for each MIR instruction at level 2 (ma23's `p_rhs`,
833 instructions, 10.5 M cycles; mu26's, 2756, 55 M; a function of 4 instructions 0.1 M), 12.7 thousand at
level 1 (the old exact Jacobian of mu26, 9163 instructions, 116 M), plus 0.8 M for each module (`MIR_gen_init`
sorts the x86-64 pattern table each time, 0.5 M; `MIR_init` 0.2 M). No pass dominates: register allocation about
20%, SSA, GVN and dead-store elimination 15%, MIR's `simplify` making a temporary register for every memory
operand we write (`new_temp_reg`, string-named and hashed) 18%, our arena and malloc 17%, the rest spread over
thirty functions. Nothing in MIR is cheap enough for TinyCC's whole compile of lecar (about 1 M instructions for
11 programs): MIR's fixed cost per module alone is 0.8 M. So the only ways to a short run no slower than TinyCC's
are to compile less and to compile later.

**What changed.** (1) The compile is in two steps: `translate_model` at the first computation (W713's point)
runs the Generator, decides every program's purity, the subsystems and the difference groups, and leaves MIR's
input in `Model::native_pending`; `build_native` builds it when the interpreter has done as much work as the
build would cost: `ParserState::interpreted_ops` counts the interpreter's operations, and the build is due at
`INTERPRETED_OPS_PER_NODE` (500) operations for each node of the module, checked by `my_rhs` at each interpreted
right-hand side. Until then every program is interpreted, bit for bit the same (the interpreter is the project's
reference and `test_compile` proves each program equal to it). A run on threads (a sweep, autoX's collocation
workers, autoX's jobs) calls `native_ready` before it starts, which builds at once on the core thread, so no
worker ever meets a pending build or a compile (MIR is not thread-safe). A run of a model that asks for the
exact Jacobian still builds the Jacobian when the option is set (its absence is the load's error), and the
programs when due. (2) A fixed variable's own function is not compiled when `p_rhs` computes the fixed
variables, as W708 did for the equations: every evaluation of them goes through `fixed_values` (`my_rhs.cpp`),
which calls `p_rhs` (the shooting's `do_bc` now does too, through `fix_only`, instead of its own loop). mu26
compiles 157 functions instead of 236. (3) The exact Jacobian keeps each value of its derivative graph in a
register (an `IrStatement::Kind::Temporary`) instead of the workspace array it stored and loaded: `IrOp::Workspace`,
the `w` parameter and `ExactJacobian::workspace` are gone; mu26's `j_states` is 3815 MIR instructions instead of
9163 and compiles in 33 M cycles instead of 116 M at level 1. Its level stays 1: level 2's compile is 56 M for
no gain in the run (1351 M against 1248 M over 400 time units), level 0's compile saves 24 M and its code loses
them in the same run (1250 M).

**Why operations and not evaluations.** A first version built the code at the thousandth interpreted right-hand
side. The cost of an interpreted evaluation varies a thousandfold between models: lecar's 183-operation
right-hand side costs 2.8 thousand instructions interpreted (0.6 thousand native), mu26's 2756-operation one
60 thousand, kuramot100's 6.5 million (its 100 sums of 100 terms each run `eval_rpn` once a term; native 3.4
million), so a thousand interpreted evaluations of kuramot100 cost 6.5 G instructions, 24% of its run, while a
thousand of lecar cost 3 M. Counting the interpreter's operations instead makes the deferred work proportional
to the compile it defers, for every model: the build comes after 129 thousand operations for lecar (some 700
evaluations), 2 million for kuramot100 (some 15). The threshold is the ski-rental trade: a run that stops before
it has paid less than the compile would have cost, and one that goes on has paid at most half the compile again
(mu26's 400 time units: 2454 M, W714's 2455 M; at 1000 operations a node 2496 M).

**Results** (millions of instructions; W714 is the branch's parent, remeasured today where it differs from the
W714 section):

| run | TinyCC | W714 | W715 | W715 against TinyCC |
|---|---|---|---|---|
| lecar `--silent` (601 rows, nullclines) | 14.0 | 22.8 | 24.0 | 1.71 |
| lecar tiny | 9.2 | 18.4 | 19.6 | 2.13 |
| ma23 tiny | 11.2 | 48.9 | 12.3 | 1.10 |
| mu26 tiny | 35.0 | 190.3 | 45.4 | 1.30 |
| kuramot100 tiny | 40.3 | 233.5 | 57.0 | 1.41 |
| mu26 exact Jacobian, backward Euler, 400 time units | 1081 | 1309 | 1236 | 1.14 |
| mu26 exact Jacobian, tiny | 51.0 | 369.1 | 173.8 | 3.4 |
| mu26, 400 time units | 2705 | 2455 | 2454 | 0.91 |
| ma23, 1000 time units | 4256 | 3505 | 3507 | 0.82 |
| kuramot100 | 30353 | 27576 | 27617 | 0.91 |

The long runs keep W714's gain. The one-step runs of ma23, mu26 and kuramot100 cost a quarter to a fifth of
W714's, but not TinyCC's: what is left is the translation (the Generator: ma23 1.5 M, mu26 6.7 M of its 257
programs) and the interpreter, which costs more than TinyCC's compile plus TinyCC's code for mu26 (an
interpreted one-step run, `compile: false`, is 54 M against TinyCC's 35 M with its compile) and for kuramot100
(52.6 M interpreted against 40.3 M). lecar's two runs are longer than W714's: its nullclines evaluate the
right-hand side 4000 times, past the threshold, so the run pays 700 interpreted evaluations and then the
compile; the compile it pays (13 functions, 10 M) cannot reach TinyCC's 1 M in MIR. The exact-Jacobian run pays
the Jacobian's 33 M cycles and the programs' 130 M of compile against TinyCC's 16 M, and its code is only 4%
faster than TinyCC's (1005 M against 1048 M: the run is the Newton iterations' right-hand sides, not the
Jacobian), so it stays 14% over.

**Provably not reachable on this card, with the numbers.** lecar's two runs, kuramot100's one step and the
exact-Jacobian runs cannot reach TinyCC's instructions by any policy of when to compile: lecar's run needs the
code (4000 evaluations) and MIR's compile of its 13 functions costs ten times TinyCC's whole compile; a one-step
run of kuramot100 or mu26 is slower interpreted than TinyCC's compile and code together, and MIR's compile of
`p_rhs` alone (kuramot100 7 M cycles plus its 100 sum functions, mu26 55 M) is more than the gap. Closing them
needs a compile that is both cheaper than MIR's by an order of magnitude and off the run's thread: a design
decision (a background compile thread while the interpreter serves, which this card's brief put last, since
threads add risk; or a second, cheaper back end for the first build). What this card settled: the deferral and
its threshold, the fixed variables' functions, the Jacobian's registers.

**Rejected.** A build at the thousandth evaluation (above: kuramot100 24% slower). MIR's lazy generation
(`MIR_set_lazy_gen_interface`) for the cold functions: its link still simplifies every function and makes a
thunk for each, the first call generates on whatever thread makes it (a worker's would race), and the saving is
the cold functions' generation only (lecar's four bridges 1.8 M cycles). Dropping the bridges (the functions the
interpreter calls a user function through): a refused program that calls one would run slower than before.
Level 0 for every function: lecar's compile 7.4 M against 10.8 M, the long runs 10 to 36% slower (W714's
table). Loading each `c[k]` and `v[k]` once a function into a register instead of a memory operand at each use
(MIR's `simplify` and GVN do it, at 18% of the compile): a `set` or a helper writes the arrays behind the
cached register, and the interpreter reads them fresh, so bit-identity would rest on an analysis of every
call, for a fifth of the compile. Keeping one MIR context for the process to pay `MIR_gen_init`'s 0.6 M once: a
global, for 0.6 M of lecar's 24 M.

**Trust boundary.** Nothing new crosses one: the Generator's input is the validated RPN as before, the pending
module is the Generator's own trees, and the build runs on the core thread only. `interpreted_ops` is a 64-bit
count; a model cannot make it overflow.

**Checks.** `make -j4 xppautx test` (79 unit tests, `test_compile` with the deferral and `native_ready`),
`tools/examples_check.sh` (216 models, 215 md5s unchanged), `tools/autoxcompare.py`, `tools/goldencheck.py`,
`tools/odexcheck.sh`, `tools/servercheck.py`, `tools/sourcecheck.sh`, `tools/asancheck.sh --only
build,smoke,examples,unittests`; the counts are in the card's report.
