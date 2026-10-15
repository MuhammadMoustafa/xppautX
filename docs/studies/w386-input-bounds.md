# W386: input-sized arrays

[Card and issue #448](https://github.com/MuhammadMoustafa/xppautX/issues/448).

The four patterns in W385 were checked against their current code before
editing. W385's proposed arbitrary memory caps were not adopted.

## Owners searched and reused

- `storage.cpp`, `integrate.cpp`: `DataStore::grow`, `stor_full`, and
  `XPP_STORAGE_FAIL_GROW`. Both sweeps call `stor_full` before a full-store
  write; no second allocation path. The error stops the sweep with its
  previously stored rows intact. The neighboring Monte Carlo equilibrium
  browser copy uses the same guard too.
- `expr_native*`, `expr_compile*`, then `expr_decode*`: the native
  compiler's stack validation is the shared decoder. It now reports its
  maximum depth instead of refusing at 200. `compile_model` uses this even
  with native compilation disabled. `add_expr` reuses it for runtime
  expressions too; an expression compiled before its function metadata is
  installed uses its instruction count as a conservative bound. Both add the lengths of the model's
  user-function programs (or their argument counts when larger) to cover
  simultaneously active function frames. The stacks keep their high-water allocation; no
  evaluator push allocates or checks capacity. This conservative sizing is
  simpler than an interprocedural depth analysis and lifts the old limit
  rather than refusing deeper valid formulas. Recursive user functions
  remain outside this card's scope, as in W385's separate recursion finding.
- `xpp_error.h`, `model_files.h`, and callers of `alloc_delay`: delay
  allocation returns `Result<>`; load failures use `model_failed`, command
  failures use `show_error`. `model_place` can locate a model option too.
  History rows, cursor and offsets use size_t; initialization no longer
  converts the ratio to int. An unrepresentable or unallocatable history
  names both delay and dt.
- `xpp_util*`, `xpp_mem*`, then all of core: no checked size-product helper
  was present. `xpp_mem.h` now owns `size_product` and `size_sum`; all work
  allocation arithmetic uses them, and matrix views reuse them. The
  existing AUTO exception boundary turns length/allocation failures into
  one placed error after run cleanup. `TokenReader` shares its integer
  scanner between long and long long to read pointer-width AUTO integers
  on Windows without duplicating the file grammar.

## AUTO audit

Grepped `autlib*`, `auto_*`, `setubv*`, `autevd`, `autpp`, `gogoauto`, and
`diagram` for vector constructors, assign/resize, dimension products and
linear indices. Changes cover:

- `autlib1.cpp`: state-copy weights; equilibrium derivative and bordered
  matrices; remeshing and interpolation history; eigenvector matrices;
  all six boundary-value history arrays, endpoint matrices, replacement
  meshes and periodic scratch; shared Jacobian scratch.
- `autlib2.cpp`: every `SolvbvWork` allocation, collocation `ft`, elimination
  `e`/`fcc`, three solution buffers, and border solve scratch.
- `autlib3.cpp`: equilibrium/Hopf/fold matrices, structured derivative
  blocks and parameter matrices, integral derivatives, restart history,
  and boundary derivatives.
- `autlib4.cpp`: the SVD matrix and extra singular-value slot.
- `autlib5.cpp`: boundary derivatives, equilibrium/eigenvector matrices,
  homoclinic persistent matrices and all eigenvalue/Newton scratch.
- `auto_jacobian.h`: structured base/parameter/numerical matrices and
  integral derivatives; `setubv2.cpp`: collocation derivatives, weights,
  integrals and boundary matrices; `autpp.cpp`: exact-Jacobian buffers;
  `gogoauto.cpp`: doubled parameter arrays.
- `auto_f2c.h`, `auto_state.h`, `auto_parallel.*`, `setubv2.cpp`, and
  `f2c_helpers.cpp`: AUTO's integer/index storage and worker signatures
  use pointer width, avoiding Windows long truncation. Its existing
  floating-point operations and numerical loop order stay unchanged.
- `auto_nox.*`: restart-row offsets and callback coordinates have pointer
  width too; header counts retain their existing file-validation bounds.
- `auto_*` files not listed above, `autevd` and `diagram` had no additional
  allocation product to change. Shared matrix views also check overflow.

The regression uses arithmetic above 2^31 and a provably unrepresentable
workspace, avoiding an actual multi-gigabyte allocation. Hostile dimension
products and delay ratios are checked before allocation. Very large but
representable requests can still exhaust available memory; no arbitrary
memory budget was introduced. See finding 59 for XPPAUT source evidence.

## Validation

All gates ran sequentially from Git Bash through `tools/heavy.sh`, against
the final UCRT executable; builds used `-j4 WERROR=1`. Python gates used
the installed `python` executable (`python3` is absent from Git Bash's
PATH). The example runner used four workers (`JOBS=4`).

| Gate | Result |
|---|---|
| UCRT build and unit tests | 0 compiler warnings; 53 executables, 43,177 checks, 0 failures; bounds regression: 37 checks |
| examples_check | 185 models; 184 output checksums match; ml-range intentionally has no output |
| servercheck | 1,169 checks, all passed |
| autocompare | 40 descriptions, 9 documented skips, 5,936 points, 90 checks, 0 failures |
| autocheck | 248 passed, 3 failed; all three failures follow the recording snapshot rejection below |

The full autocheck and a focused `recording` rerun both reject
`examples/recordings/lecar_auto.recx` because its embedded `model.setx`
has no `rangefiles` in the complete `range` group (lines 93-104).
Consequently the playback, idle-count and saved-diagram checks fail.
The recording and `core/model_options.cpp`, `core/setx_file.cpp`,
`core/setx.cpp`, and `tools/autocheck.py` match HEAD after CRLF
normalization. The existing saved `RANGEFILES` row is at
[model_options.cpp:671](../../core/model_options.cpp#L671); the complete
reader refuses a missing key at [setx.cpp:38](../../core/setx.cpp#L38).
This is source-confirmed fixture/schema incompatibility independent of
W386; a separate HEAD executable was not built. The fixture was left
unchanged for the W349/#410 follow-up, with no compatibility fallback.

Reproduce the three failures with:

```sh
tools/heavy.sh python tools/autocheck.py --server ./xppautX.exe recording
```

Logs are retained under `build/w386-*.log`. WSL gates and sanitizers are
the coordinator's review tier, as the brief requires. All foreground
runs have ended; this task started no background runs and made no commit.
