# W544 Speed checked on the final tip before the comparison

Card [#609](https://github.com/MuhammadMoustafa/xppautX/issues/609), 2026-10-29.

## Method and scope

Compare W451's tip `958f18f76386fb70582aec34527c9c454a2b080e` with
`4071055d49c0e90edc4d11f48943b87ef10bddd5`, the starting HEAD of this card
(the table's tip column is that commit plus this card's two fixes).
This is the final tip **available to W544**, before concurrent W541/W543 work;
any later code merged before W354 needs its own check. No XPPAUT or AUTO-07p
source, benchmark file, or benchmark driver is edited.

Both builds use the comparison setup's flags:

```sh
make -j6 xppautx WINDOW=0 'OPT=-g -O2' \
  XPPAUTX_VERSION=paper-comparison BUILDDIR=SCRATCH/build-REV
```

The compiler is Ubuntu g++ 15.2.0-16ubuntu1, with the Makefile's C++23,
`-Werror` and `-ffp-contract=off`; Valgrind is 3.26.0, x86-64 Linux.
The base uses a detached scratch worktree; builds have separate object folders
and binary copies. Each process runs under
`valgrind --tool=callgrind --callgrind-out-file=PROFILE BINARY MODEL --silent`,
with `LC_ALL=C` and an isolated configuration directory. Server/AUTO runs use
a wrapper with the same Valgrind invocation as `tools/auto-profile/callgrind_wrap.sh`.
Only one build, test suite, or Valgrind process runs at a time in this card.
No wall-clock performance measurement is used. Counts are whole-process `Ir`
(Callgrind's `summary:`), including loading, model compilation, computation,
output, protocol and shutdown. These are instruction ratios, not elapsed-time
speedups or a replacement for the final comparison.

All inputs are legacy `.ode` models, identical between binaries. Source copies
are staged in scratch; only their `@ total` values change as listed below.
No versioned `.odex`/session file is fed across the format-version boundary.
The comparison's integration driver stages its screen models from
`paper/benchmarks/five-model-screen/models`; the scaling files are from
`paper/benchmarks/size-scaling/models-ubuntu-20261020`, paired with
`cases-ubuntu-20261020.json`. Both locations are read in the main checkout.

`.ode` models are read by both programs (each converts the text to `.odex` in
memory on every load). The integration cases are `lecar.ode` as is, Terman
(the screen's `episodic.ode`, `@ total=20`), kuramoto50 and lecdiff200 from the
size-scaling study (`@ total=5`; `long-` rows `@ total=40`, so the per-step cost
shows over the start-up). The AUTO workload is `tests/auto/ode_lecar.json` driven
by `tools/autocompare.py`'s `compute` through `--server` (79 points; the diagrams
hold the same points, though the base names the point types as numbers and the tip
as words); the server workload is 100 pairs of (set a parameter, integrate) through
`tools/xppclient.py`, written once per protocol (`set` + key `i`,`g` on the base;
`value.set_parameters` + `run.initial` on the tip). Every output file has the same
md5 on both binaries.

## Result (Ir, whole process; ratio = tip / base)

| Workload | Base | Tip | Ratio |
|---|---:|---:|---:|
| start-up, lecar (1 step) | 21,610,819 | 19,382,842 | 0.897 |
| start-up, amari | 113,192,006 | 108,054,064 | 0.955 |
| start-up, lecdiff200 | 185,020,510 | 167,789,215 | 0.907 |
| integration, lecar | 32,603,717 | 25,234,339 | 0.774 |
| integration, Terman | 185,395,199 | 148,446,784 | 0.801 |
| integration, kuramoto50, T=5 | 482,696,865 | 480,442,594 | 0.995 |
| integration, lecdiff200, T=5 | 253,931,980 | 231,201,816 | 0.911 |
| integration, kuramoto50, T=40 | 3,244,027,546 | 3,235,254,530 | 0.997 |
| integration, lecdiff200, T=40 | 697,733,127 | 639,175,947 | 0.916 |
| AUTO, lecar steady state and cycles, via `--server` | 159,220,166 | 161,609,799 | 1.015 |
| `--server`, 200 commands | 473,102,179 | 453,733,721 | 0.959 |

No workload is more than 2% slower; the integrations and the start-ups got
faster (W466-W471's native code, W451's and W453's start-up work).

## Findings

Before this card's two fixes the tip was slower on two workloads; both are
code added after W451, fixed in this card with the numbers unchanged
(`tools/examples_check.sh` 215/216, `tools/autocompare.py` 71 models, 0 failures,
servercheck 2849 checks, unit tests all pass):

- **start-up, amari: 113.2M to 128.4M (+13%)**, then 107.2M. Two causes.
  `ParserState::arg_count` (W497, c0b6f5a6) counted the symbol table's
  argument symbols on every compiled expression, a scan of the whole table each
  time (4.6M for amari's 400-symbol table); it is a member, kept by `arg_symbol`.
  `Generator::may_read` (W468, b5af9279) formatted `"v[k]"` once per call
  made, for each fixed variable (20,100 `xpp::format` calls, 15.7M); the text
  is now made once per variable.
- **`--server` start-up: `hello`'s keymap** (W517/W529): `keymap::effective_json`
  called `command::find` (a scan of every row) for each of the registry's rows;
  `effective_keys` now takes the row. That was 2.5M of the 8.5M by which `hello` cost
  more than on the base.

What stays above the base in the AUTO row (+1.5%) is `hello` itself,
about 6M instructions once per `--server` session: the registry's
`append_commands` (6.2M inclusive) and keymap listing. AUTO's own work is
2.4% cheaper (autobv/cnrlbv 95.4M to 93.2M). It is a fixed cost, small
against any run; a follow-up could build the command list of `hello` once,
but nothing needs it before the comparison.

## Leads: the 10 costliest functions on the tip (self Ir)

Integration, kuramoto50 T=40 (3.24G): `cr_sin_fma` 63.6%, the model's
native code (two unnamed JIT ranges) 9.4% and 8.0%, `expr::do_shift` 8.8%,
`xpp::math::sin` 3.8% (the wrapper), `sin_large_accurate` 1.2%, `do_shift`'s
vector access 1.0%, `mhUU` 0.8%, JIT 0.8%, `expr_native.cpp` 0.5%.
The correctly rounded `sin` is two thirds of it; `do_shift` (the delayed
`shift` call) is 8.8% on its own.

Integration, lecdiff200 T=40 (0.64G): `cr_tanh_fma` 20.5%, `cr_cosh_fma` 12.3%,
then eight unnamed JIT ranges (the generated right-hand side) of 2.8% to 5.7% each.

AUTO, lecar (0.16G, including `hello` and the diagram's compression):
`tdefl_compress` 8.8% (the session file's zip), `cr_tanh_fma` 7.7%,
`conpar_process` 5.1%, `setubv` (2 rows) 4.3% and 2.0%, `cr_cosh_fma` 3.5%,
`xpp_math.h` inlines 3.5%, unnamed JIT 3.4%, `icps` 3.2%, `memset` 1.9%.

No follow-up card is needed from this study; the leads are for later work.
