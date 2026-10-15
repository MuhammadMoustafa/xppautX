# W361: AUTO mesh adaptation and Floquet candidates

W361 ([#423](https://github.com/MuhammadMoustafa/xppautX/issues/423)) implements
D9 of [W355](w355-auto07p-differences.md), and measures D8 separately. The
reference is AUTO-07p 0.9.4; EISPACK remains the eigenvalue library (W358).
The before build is checkout `53ac294d8b5a32b1a8b13a07f22a7083af217207`,
UCRT GCC 16, `WERROR=1`, `-j4`, 2026-10-15, in the W361 worktree.
All builds and runs used `tools/heavy.sh` sequentially.

## Implementation and search

Searched `xpp_math.h`, `xpp_math_auto.h`, `xpp_math_auto.cpp`,
`xpp_math_eispack.cpp`, all of `core/` for pencil/balance/mesh operations,
and autlib1's `eqdf`, `newmsh`, `interp`, `intwts`, `ordr`. Reused EQDF's
existing derivative/monitor computation and ORDR's interval lookup, and
extended the existing EQDF output with its unaccumulated monitor density.
No second mesh or eigensolver implementation is retained.

NEWMSH now divides distance in monitor space by the density directly,
then adds/subtracts that distance from the nearer old time endpoint.
Both mesh endpoints are copied exactly. In the small-derivative case the
density is `1/dt`, consistent with the existing interval-index monitor.
IPS 12 (wave trains) now shares IPS 2's periodic derivative extension.
These are our implementations of the method in AUTO-07p's
[mesh.f90:395-448](../../reference/auto-07p/src/mesh.f90#L395),
[EQDF](../../reference/auto-07p/src/mesh.f90#L480),
[bvp.f90:333-334](../../reference/auto-07p/src/bvp.f90#L333), and the
[0.9.4 closest-time interpolation note](../../reference/auto-07p/CHANGELOG#L17).
The density avoids the addition/subtraction loss described in
[CHANGELOG:176-178](../../reference/auto-07p/CHANGELOG#L176).
No source was copied or translated from AUTO-07p, and no LAPACK was linked.

The simpler change of interpolating from the nearer endpoint while still
recovering density from `EQF[j]-EQF[j-1]` was rejected: it retains the
cancellation the method removes. The new unit test demonstrates that
loss with a large early monitor followed by small positive densities;
it also checks exact endpoints, increasing times, the constant-solution
case on a nonuniform mesh, and equality of IPS 2/12 adaptation.

## Floquet candidates, measured independently

AUTO-07p's [FLOWKM](../../reference/auto-07p/src/floquet.f90#L64) uses
DGESVJ and DGGEV. DGGEV calls `DGGBAL('P')`: **permutation-only** isolation
of structural singleton rows/columns, not general diagonal scaling.
The local read-only library source used to verify that distinction is
[DGGEV:393-400](../../third_party/lapack/dggev.f#L393).
`balanc` balances one ordinary matrix; calling it separately on the two
matrices would not preserve the pencil's generalized eigenvalues.

Three separate experiments were built over the mesh-only implementation.
Each ran all nine periodic descriptions at 1 and automatic threads
(1,410 stored points, 27 checks: nine reference checks passed; the 18
byte-baseline checks intentionally failed before the one final update).
All 152 printed reference measures, including 76 against AUTO-07p, were
identical to mesh-only in every experiment:

| Candidate | Difference from mesh-only over all stored points | Decision |
|---|---|---|
| Independently implemented permutation isolation before EISPACK QZ, extracting the isolated roots and solving the active subpencil | Eight diagrams byte-identical; FRCP spectrum changes by at most 4.97e-16 as a set, parameter/norm/period unchanged | Omit: no reference gain |
| Null-vector SVD tolerance `sqrt(ndim)*epsilon`, compared with existing `1e-16` | Eight diagrams byte-identical; FRCP spectrum changes by at most 3.45e-14 as a set, parameter/norm/period unchanged | Omit: no reference gain; Jacobi and bidiagonal SVD tolerances are different contracts |
| AUTO-07p's largest finite double for each component of an infinite multiplier, instead of `1e30` | All nine diagrams byte-identical; no reference measure changes | Omit: no measured benefit in these descriptions |

FRCP's changed multipliers are ordinary cycle points, without special-point
reference spectra at those locations; these references cannot rank those
last-bit changes. No improvement is inferred from a changed bit pattern.

The final Floquet code is unchanged. Its shared SVD-deflated circuit-pencil
method already agrees at the resolution of these references. This does not
establish accuracy for arbitrary badly scaled pencils or a new infinite
multiplier case. W369's eigenvalue-order-dependent stored period is untouched.
The experiment scripts, per-candidate builds, diagrams and raw measures
are in `build/w361/` for coordinator review (local, ignored).

## Attribution of the intentional diagram changes

Every change below is caused by **mesh**, with the original Floquet code.
Maxima are absolute differences at corresponding stored points, across
all runs, in continued parameter, norm, period and spectrum. Spectra are
matched as sets (minimum maximum pairing distance), so a changed conjugate
ordering is not counted as a large numerical error. Stored point counts,
calculation types, branch/point numbers, labels and bifurcation flags all
remain identical. One- and automatic-thread diagrams agree byte for byte.

| Description | Parameter | Norm | Period | Spectrum | Cause |
|---|---:|---:|---:|---:|---|
| `ode_lecar` | 2.22e-12 | 9.53e-10 | 3.5e-07 | 28.9 | mesh |
| `tstauto_brper` | 4.46e-12 | 6.11e-12 | 6.34e-12 | 3.62e-10 | mesh |
| `tstauto_dumbvp` | 5.55e-16 | 5.55e-16 | 0 | 0 | mesh |
| `tstauto_exp` | 2.22e-15 | 2e-15 | 0 | 0 | mesh |
| `tstauto_frcp` | 6.66e-16 | 6.66e-16 | 5.77e-15 | 7.5e-08 | mesh |
| `tstauto_lamom` | 5.17e-16 | 2.22e-16 | 2.66e-15 | 4.37e-10 | mesh |
| `tstauto_lamom2` | 8.22e-12 | 8.81e-12 | 3.48e-11 | 1.34e-09 | mesh |
| `tstauto_lecar` | 1.26e-12 | 5.42e-10 | 1.97e-07 | 23.5 | mesh |
| `tstauto_lo` | 2.64e-15 | 8.33e-16 | 6.22e-15 | 2.72e-10 | mesh |
| `tstauto_pp2` | 4.59e-10 | 3.09e-07 | 0.00365 | 19.8 | mesh |
| `tstauto_torx` | 9.99e-16 | 4.16e-17 | 3.55e-15 | 6.16e-10 | mesh |
| `tstauto_tstheti_homcont` | 1.55e-15 | 7.77e-16 | 0 | 0 | mesh |
| `tstauto_tsthom2` | 9.63e-08 | 5.95e-07 | 0 | 0 | mesh |

All nine periodic diagrams change, as do four descriptions sharing the
same BVP/HomCont mesh routine. No steady-state-only diagram changes.
The larger absolute spectral changes are on large, sensitive multipliers:
`ode_lecar` point 70 changes -13359622.0291 to -13359593.1519 (2.16e-6
relative with denominator `1+abs(old)`), `tstauto_lecar` point 74 changes
-13333233.6283 to -13333210.1409 (1.76e-6), and `tstauto_pp2` point 130
changes 64673.0058 to 64653.1687 (3.07e-4). PP2's period there changes
145.1598007095 to 145.1561549426. These are mesh effects on the existing
underresolved/sensitive regimes; no Floquet change is being credited for them.

## Agreement with AUTO-07p before and after

The unchanged comparison rules and references are autocompare's own.
Before: 40 descriptions, 9 explicit skips, 5,936 points, 90 checks, zero
failures. Mesh-only: the same counts and all reference checks pass; the
26 intentional byte-baseline mismatches are the 13 changed diagrams at
two thread settings. All 151 AUTO-07p quantities remain inside their
existing tolerances. The largest difference/tolerance over all descriptions
is unchanged, 0.654 (`tstauto_ab_fold`, curve norm). Over the nine periodic
descriptions it is unchanged, 0.296 (FRCP's steady Hopf spectrum).
No AUTO-07p spectrum measure changes at the three-digit precision of
`--measures`; all nine spectrum agreement maxima in `--table` are unchanged.

Raw errors are not monotonically smaller: `ode_lecar`'s periodic LP period
difference is 4.17e-4 before and 4.18e-4 after (tolerance 0.0969), while
lamom2's LP parameter/norm/period errors change from
9.14e-13/2.91e-12/8.52e-12 to 9.22e-13/5.90e-12/2.63e-11.
These changes are below the reference's numerical resolution, not evidence
of an accuracy gain. The lamom2 spectrum error remains 3.8e-6; its table
row and the XPPAUT PP2 period row are refreshed from `--table`.
Thus the agreement is equal at the reference tolerances/resolution for
every description; this card claims no measurable Floquet improvement.

## Validation

All runs were sequential through `tools/heavy.sh`, in Git Bash with `python`:

- UCRT GCC 16, `mingw32-make -j4 xppautx test BUILDDIR=build/ucrt WERROR=1`:
  55 unit programs, 43,372 checks, zero failures and zero compiler warnings;
  the new mesh test contributes 31 checks (`final-unit.log`).
- `python tools/autocompare.py --bin ./xppautX.exe --update`: run exactly once;
  13 diagrams updated, 31 checks passed, one/automatic threads byte-identical.
- Full `--measures --table` before and after: 90 checks passed each, 40
  descriptions, nine explicit skips, 5,936 points. Final measures and table
  are identical to mesh-only (`before.log`, `mesh.log`, `after.log`).
- `python tools/autocheck.py --server ./xppautX.exe`: 250 passed, one failed
  on its existing 3-second stdin-EOF assertion (null device exited in 3.6 s).
  Unchanged `input` section rerun: both checks passed. This is recorded as
  **one flaky check**, not a clean first full pass. No timeouts or tests
  were relaxed (`autocheck.log`, `autocheck-input-rerun.log`).
- `tools/examples_check.sh --bin xppautX.exe`: 185 models, all 184 expected
  outputs match the unchanged checksum file; ml-range has no output by design.
- `python tools/findingscheck.py`: both findings files pass numbering checks.
- `git diff --check`: clean. No background runs were started.

Local raw evidence and experiment scripts are in `build/w361/`. The unit
snapshot briefly caught a stale experimental object after source restoration
preserved its old timestamp; rebuilding that object restored the original
Floquet snapshot, without changing any expected digest.
The coordinator runs the WSL source checks, WSL autocompare and ASan gates.
No trust-boundary reader or input limit
was changed. XPPAUT's IPS 12 endpoint-monitor omission is recorded as
[finding 63](../xppaut-findings.md#63-wave-trains-use-nonperiodic-mesh-extrapolation);
no AUTO-07p bug was found.
