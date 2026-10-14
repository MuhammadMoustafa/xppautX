# AUTO checked against AUTO-07p and XPPAUT 8.0

W299 ([#357](https://github.com/MuhammadMoustafa/xppautX/issues/357)) keeps our own AUTO diagrams byte for
byte, which guards a change but not the numbers. W300
([#358](https://github.com/MuhammadMoustafa/xppautX/issues/358)) checks them against two programs that
were not written by us: **AUTO-07p 0.9.4** (the maintained AUTO, a newer line than XPPAUT's; the one reference since W357 ([#419](https://github.com/MuhammadMoustafa/xppautX/issues/419)), 0.9.3 having been used before) and **XPPAUT
8.0** itself (reference/xppaut-8.0; its AUTO is AUTO2000's C translation, [autlib5.c:2008](../reference/xppaut-8.0/autlib5.c#L2008)
names the translator, with XPPAUT's own glue in `auto_nox.c`, `autevd.c` and `autpp.c`). The comparison
is part of `tools/autocompare.py` and runs on every push from committed files: neither program is
installed in CI, nor vendored.

## What is compared, and how

- `tests/auto/<description>.auto07p` and `.xppaut` hold the **special points** each program finds
  for the problem a description runs: each bifurcation's type (BP, LP, HB, PD, TR), its parameter
  value, the state norm, the period (the Hopf point's, and every point of a periodic branch), and the
  eigenvalues (an equilibrium) or Floquet multipliers (a cycle) there; and the run's start. Not the step
  sequence (the programs' differ), and not a run's last point (it depends on the step count).
- `tools/auto_reference.py` writes them, by hand, once (`--program auto07p` or `xppaut`): AUTO-07p from a
  Fortran translation of the `.odex` (`par`, `init`, formulas, `fun`, `boundary`) with the description's
  constants (NTST, NCOL, DS, DSMIN, DSMAX, EPSL, EPSU, EPSS, NMX, NPR, MXBF, and the ITMX, ITNW, NWTN,
  IAD, IID, IADS xppautX fixes), the Newton-corrected first point of our diagram as the start, and a
  periodic run from the first HB of the named run; XPPAUT 8.0 built from reference/xppaut-8.0 with its
  AUTO window opened on a private Xvfb display and driven by the keys a user presses (Parameters and
  Numerics dialogs, Run, Grab, Run), the constants it used read back from the `.b` header and required to
  be the run's. Its point table is XPPAUT's own stdout, the spectra its diagnostics file, the period its
  solution file's PAR(11), named `auto_unknown::period` in our solver. W340 ([#401](https://github.com/MuhammadMoustafa/xppautX/issues/401)) keeps AUTO's block at `max(36, 12 + 4*node)` and stores model parameter `k` at `block + k`; foreign XPPAUT PAR numbering is mapped only at import.
- `tools/autocompare.py` runs our description as before (the byte-for-byte check unchanged), then compares
  our special points with each file, matched by type and order. **A missing or extra special point is a
  difference.** A description with no file for a program says why (`auto07p_skip`, `xppaut_skip`), never
  silence; a difference found, traced and kept is a `differences` entry with its cause, which the check
  holds exact (it fails if the difference goes away, too). `--measures` prints every quantity compared,
  `--table` this page's table lines.

## The tolerance, per quantity, and why

For a quantity *q* at a special point the allowed difference is

    2 * EPSS*(1+DSMAX) * g  +  2 * EPS * (1+|q|)   [+ 2 * (FD + PRINTED) * (1+L) * |mu| for a spectrum]

- `EPSS*(1+DSMAX)` is the **position error along the branch**: both programs locate a special point by the
  secant and Mueller method until the arclength step is below `EPSS*(1+sqrt|DS*DSMAX|)`
  ([autlib1.c lcspae](../reference/xppaut-8.0/autlib1.c#L1579), AUTO-07p `ae.f90` `LCSPAE`), at most
  `EPSS*(1+DSMAX)`; two programs differ by twice that. *g* is how fast *q* changes per unit of
  (parameter, norm) path length, measured from our own stored points on both sides of the special point,
  so the formula never allows more than the branch itself moves *q* over the error.
- `EPS` is Newton's convergence tolerance on each point (EPSL for the parameter, EPSU for the state and
  everything computed from it), twice.
- A spectrum adds the Jacobian's finite-difference accuracy (`FD` = 1e-7, HMACH) and the six digits
  AUTO-07p prints (`PRINTED` = 5e-6) relative to the size *L* of the eigenvalues, carried through
  e^lambda.
- **Eigenvalues are compared as XPPAUT stores them: e^lambda** for an equilibrium
  ([autevd.c send_eigen](../reference/xppaut-8.0/autevd.c#L123): the stability circle is the unit circle),
  the Floquet multipliers as they are; a run's first point holds no spectrum (zeros, not computed).
- The tolerance is **never widened to make a point pass**: a difference beyond it is traced to a cause
  (below) and recorded, or it is a bug.

## Result

26 models have an AUTO-07p reference (46 special points compared) and 24 an XPPAUT 8.0 reference (44 points); 24 of the 28 runnable descriptions hold both. Every untraced comparison agrees. AUTO-07p has five traced points in two descriptions; XPPAUT 8.0 has three in three descriptions. No tolerance was widened. Before W326 there were 44 compared points and six traced points against AUTO-07p, and 44 compared points with none traced against XPPAUT.

| Model | Special points in xppautX | AUTO-07p | XPPAUT 8.0 |
|---|---|---|---|
| ode/del_log | steady: EP | agrees (1 points; par 0, norm 4.9e-12) | agrees (1 points; par 0, norm 2.2e-08) |
| ode/lecar | steady: EP HB HB; cycles: LP | agrees (4 points; par 1.2e-05, norm 1.5e-06, period 0.0017, spectrum 0.0028) | agrees, 1 traced (3 points; par 5.5e-05, norm 4.4e-08, period 5.7e-09, spectrum 4.1e-08) |
| ode/osc | steady: EP | agrees (1 points; par 0, norm 2.7e-11) | agrees (1 points; par 0, norm 4.7e-07) |
| ab | steady: EP LP | agrees (2 points; par 3.1e-13, norm 1.1e-09, spectrum 4.2e-08) | agrees (2 points; par 2.2e-08, norm 4.6e-07, spectrum 2.9e-09) |
| bigaut | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| bistable | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| bob | steady: EP | agrees (1 points; par 0, norm 1.8e-12) | agrees (1 points; par 0, norm 1.4e-08) |
| brper | steady: EP HB; cycles: - | agrees, 1 traced (1 points; par 0, norm 0) | agrees (2 points; par 2.5e-16, norm 0, period 2e-11, spectrum 1.1e-15) |
| dumb | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| dumbvp | boundary: EP | agrees (1 points; par 0, norm 0) | not run |
| eiz-hom | steady: EP | agrees (1 points; par 0, norm 1.2e-13) | agrees (1 points; par 0, norm 2e-09) |
| exp | boundary: EP | agrees (1 points; par 0, norm 0) | not run |
| frcp | steady: EP HB; cycles: - | agrees (2 points; par 3.4e-12, norm 1e-13, period 2.1e-06, spectrum 0.00093) | agrees (2 points; par 5.2e-06, norm 4.6e-08, period 2.5e-12, spectrum 4.1e-06) |
| lamom | steady: EP HB; cycles: - | agrees (2 points; par 3.4e-18, norm 0, period 2e-11, spectrum 1e-14) | agrees (2 points; par 2.7e-16, norm 0, period 2e-11, spectrum 1e-14) |
| lamom2 | steady: EP HB; cycles: LP | agrees (3 points; par 9.1e-13, norm 2.9e-12, period 2e-11, spectrum 3.8e-06) | agrees (3 points; par 6.8e-09, norm 4e-08, period 2e-11, spectrum 2.3e-08) |
| lecar | steady: EP HB HB; cycles: LP | agrees (4 points; par 1.2e-05, norm 1.6e-06, period 0.0017, spectrum 0.0029) | agrees, 1 traced (3 points; par 5.5e-05, norm 4e-08, period 3.5e-09, spectrum 4.7e-08) |
| lo | steady: EP HB; cycles: - | agrees (2 points; par 2.7e-18, norm 0, period 2e-11, spectrum 9.8e-15) | agrees (2 points; par 2.5e-16, norm 0, period 2e-11, spectrum 2.5e-16) |
| pen | steady: EP | agrees (1 points; par 0, norm 3.6e-12) | agrees (1 points; par 0, norm 4.9e-09) |
| pf | steady: EP BP | agrees (2 points; par 2.7e-18, norm 0, spectrum 9.8e-15) | agrees (2 points; par 2.5e-16, norm 0, spectrum 2.2e-16) |
| pp2 | steady: EP HB BP; cycles: LP LP | agrees, 4 traced (3 points; par 1.8e-12, norm 3.3e-12, period 6.3e-11, spectrum 5e-07) | agrees, 1 traced (4 points; par 4.8e-08, norm 4.9e-07, period 2.2e-10, spectrum 2.9e-07) |
| san | steady: EP BP | agrees (2 points; par 2.4e-12, norm 2.8e-15, spectrum 1.3e-07) | agrees (2 points; par 1.7e-08, norm 4.1e-11, spectrum 1.3e-07) |
| she | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| simplefold | steady: EP LP LP | agrees (3 points; par 4.8e-12, norm 4.5e-11, spectrum 8.6e-11) | agrees (3 points; par 2.1e-08, norm 4.3e-08, spectrum 2.7e-13) |
| torx | steady: EP HB BP; cycles: - | agrees (3 points; par 4.7e-12, norm 0, period 3.3e-12, spectrum 3.9e-07) | agrees (3 points; par 2.2e-08, norm 0, period 3.3e-12, spectrum 3.7e-08) |
| tsthom | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| wcbiwave | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |

Cells: `agrees` (every compared quantity inside its tolerance), the number of points compared and the
largest absolute difference per quantity (`par`, `norm`, `period`, `spectrum`: the eigenvalue or multiplier
pair furthest apart); `traced` counts the points of that description recorded below. The second column lists
the special points xppautX finds, run by run (EP: the run's start). Where the code path is shared the
differences remain within the per-quantity tolerances. W356's refreshed test histories
move the displayed Morris-Lecar HB parameters away from XPPAUT by up to 5.5e-5 and frcp's by
5.2e-6; these are traced below, with no tolerance widened. Against AUTO-07p 0.9.4 the largest
parameter difference falls from 5.45114e-5 to 1.23904e-5. The largest norm difference remains
1.57141e-6; the period remains 1.66839e-3 and the spectrum 2.90189e-3 (Morris-Lecar's cycle LP).
The anticipated large drop in period and spectrum did not occur; it is not claimed.
A point is only as accurate as its location and finite-difference tolerances.

### W356: location and test history

W356 ([#418](https://github.com/MuhammadMoustafa/xppautX/issues/418)) reimplements AUTO-07p's
Mueller root formula and retry-corrected abscissae (D1), ordered test bookkeeping and coincidence
rules (D2), and multiplier/stability-based detection criteria (D3). Both equilibrium and BVP
controllers use one ordering helper. After a later locator moves the solution, earlier test
functions are evaluated there before continuation resumes. Coincident tests keep the first label;
a bifurcation can override a user zero. A correction already inside EPSS needs no new solve.

The maintainer's 2026-10-14 resolution rule for #418 judges an increase at the reference's last
printed digit, or EPSS for a located point. D1's increases of roughly 1e-10 to 3e-9 are numerical
noise by that rule. Against the refreshed 0.9.4 references, no table entry grows beyond EPSS,
at D1, D2 or D3, and every recorded-reference check passes its original scientific tolerance.
This acceptance rule does not change the comparison tolerances in `autocompare.py`.

Exactly seven baseline diagrams change: `ode_lecar`, `tstauto_lecar`, `tstauto_brper`,
`tstauto_lamom2`, `tstauto_frcp`, `tstauto_pp2`, `tstauto_simplefold`; both thread counts agree.
D1 changes the root arithmetic and its subsequent continuation path. D2 additionally refreshes
`fnlpae` at ode/lecar HB19/HB46, tstauto/lecar HB23/HB50, frcp HB38 and pp2 BP44. That evaluation
sets `par[icp[0]] = rlcur[0]` after the corrector's final update, so the stored parameter belongs
to the accepted state. This explains the larger *parameter-only* differences from XPPAUT at
Lecar and frcp; their states, periods and spectra do not change in D2. The pp2 BP parameter-only
change remains inside printed resolution. No labels or point counts change; the previously traced
underresolved pp2 cycle folds remain traced. D3 changes no diagram bytes in this set of fixtures.
Every changed stored point, per part, and full-precision maxima are in the
[W356 study](studies/w356-auto-location-07p.md). No negative-discriminant finding is claimed for
XPPAUT: the unit test exercises ours, without an executed XPPAUT reproduction.

## Disagreements, traced

Three causes in four descriptions; W326 fixes the missing restore and records the resulting differences with XPPAUT.

1. **Morris-Lecar cycles, `ode_lecar` and `tstauto_lecar`: xppautX and AUTO-07p find the LP; XPPAUT does not.**
   At NTST 15 the BP test changes sign by noise at point 21 and its location exhausts ITMX.
   AUTO2000 leaves the displaced iterate ([autlib1.c:6498](../reference/xppaut-8.0/autlib1.c#L6498)),
   which reverses the path. W326 ([#387](https://github.com/MuhammadMoustafa/xppautX/issues/387)),
   the first W302 stage, restores the converged solution, tangent, old point, time derivative,
   parameters, step history, test value and monodromy/eigen data, including the factored system
   reused by a fold test. This is our own implementation with local owned copies, following
   AUTO-07p's restoration principle ([finding #47](xppaut-findings.md#47-auto-a-special-point-whose-location-does-not-converge-leaves-the-iterate-behind)).
   The fold is now labelled at iapp 0.248413877494 (`ode_lecar`) and 0.248413877545 (`tstauto_lecar`),
   versus AUTO-07p's 0.248413876; periods 25.757802 and 25.757787 versus 25.7582.
   All quantities pass the existing tolerance. The missing-LP entries against AUTO-07p are removed;
   each extra LP against XPPAUT is traced to this restore.
2. **Predator-prey `tstauto_pp2` cycles: xppautX now labels two LPs, as AUTO-07p does; XPPAUT labels one.**
   The original mesh-dependent LP at point 24 remains. A failed fold location at point 27 previously
   displaced the path; restoring it now reaches a second LP at point 29 (par 0.708072787613,
   period 106.551753). At NTST 15 the near-homoclinic cycles beyond period 60 are underresolved:
   AUTO-07p's LPs at points 27 and 29 have periods 79.893485 and 84.541967, so these labels are
   still mesh-dependent disagreements, not newly verified physical folds. At NTST 60 and EPS 1e-9
   neither program finds any LP up to period 84.4 (par 0.7080728, norm 0.426543 in both).
   Both of ours and both of AUTO-07p's LPs are therefore traced; the new extra point against
   XPPAUT is traced specifically to W326's restore. The untraced points still agree.
3. **`tstauto_brper`, the radial oscillator: AUTO-07p labels no HB, ours and XPPAUT 8.0 do.** The
   Hopf point is at a = 0, which the 11th step (DS 0.1 from -1) lands on to roundoff: AUTO-07p's Hopf function
   there is 2.5e-16 and it labels nothing, so its cycles run has no HB to start from (`unavailable`). The
   HB is real (the model's Jacobian at the origin has eigenvalues a +- i), and AUTO-07p labels it from a
   start at a = -1.05 (at a = 9.5e-8), with cycles ending at a = 1.00003, norm 1.0000298, period 5.71196, as
   ours (1.00002979, 5.711955). Ours and XPPAUT 8.0 agree to 2.5e-16 on the HB and its cycles.

Past the first fold of cycles whose multipliers reach 1e6 none of the three programs' numbers are the model's
(the near-homoclinic orbits of the cases above), so a description's short run is only as good as its mesh
there; the two recorded cases are the ones that reach it.

## Not compared, and why

- **Skipped by W299 (nine models, no AUTO run at all):** three Volterra integral models, two random
  delay-network models, three rotating-phase models whose supplied start fails, and the unfinished `tsthom3`
  draft; each description gives its reason.
- **AUTO-07p not run** (`auto07p_skip`): `ode/tstheti` and `tstauto_tsthom2`. Their starts use
  integrated storage. W343 established that `tstheti`'s `hom_bcs` conditions are deprecated zero
  functions even in XPPAUT 8.0; it is now an expected-error regression, not a validated connection.
  AUTO-07p's homoclinic continuation needs a separately specified valid problem.
- **XPPAUT 8.0 not run** (`xppaut_skip`): the four boundary-value fixtures (`exp`, `dumbvp`, `tstheti`,
  `tsthom2`). XPPAUT's boundary-value run starts from the solution in its storage, which an integration in its
  main window fills; the keys of `tools/auto_reference.py` do not drive that. The other 24 models were driven.
  XPPAUT's AUTO has no batch mode, so any run needs its window and an X server.
- **Compared at the start only:** `bigaut` (350 variables) and `dumb` (195) are linear dimension fixtures
  with no special point, and the models whose run meets none before it ends on the step count or a bound
  compare the start alone. A run's last point is not compared.
- **Constants not equal in the two programs:** XPPAUT 8.0 has no way to set a run's THL, ISP or ILP
  from its window (it runs with the ones in `core/auto_nox.cpp`, which our descriptions share), so those are
  XPPAUT's as they are; the constants that can be set are read back and checked.

## W302's first stage: W326

W326 restores the converged state after ITMX is exhausted, without changing successful location
iterations or integration. The baseline was intentionally updated at both one and automatic threads;
all 28 runnable descriptions remain identical between thread counts. Exactly four diagrams changed:

| Description | First changed stored point | Failed location responsible |
|---|---|---|
| `ode_lecar` | 70 (cycles point 21) | noisy BP; the restored path now reaches the real LP |
| `tstauto_lecar` | 74 (cycles point 21) | the same Morris-Lecar BP failure |
| `tstauto_pp2` | 127 (cycles point 27) | fold; the restored path reaches a second underresolved LP |
| `ode_tstheti` | 2 (connection point 2) | fold and BP tests return NaN and exhaust ITMX; W326 restores the iterate, but W343 establishes that the iterate itself was already nonfinite |

The diagnostics contain `NOTE:Possible special point` at each first changed continuation point.
`ode_tstheti` had no independent reference, and W326's changed NaN baseline did not validate a
connection. W343 traced the missing constraints to the deprecated zero-valued `hom_bcs` in XPPAUT
8.0 and corrected the acceptance test (below). None of the other 24 runnable diagrams changed.
The deterministic `test_auto_location` also forces a false equilibrium sign change
and checks the restored solution, history, tangent, step size, test value, derivative data and spectrum.

### W343: the original tstheti is an expected failure

The supplied `hom_bcs(0)` and `hom_bcs(1)` are identically zero in both codes,
not projection conditions. A seventh boundary is missing and filled with zero,
so three boundary rows vanish. This makes the ordinary BVP singular; HomCont's
left/right equilibria, stable/unstable dimensions and IEQUIB are used in IPS=9,
not this IPS=4 run. The 1.01 integration provides a finite orbit but cannot add
missing boundary constraints. See [finding 50](xppaut-findings.md#50-auto-accepts-nan-as-a-converged-bvp-solution)
for the upstream source lines and the NaN convergence-test defect.

The baseline was rewritten once with `autocompare.py --update --only ode_tstheti`:
point 1 is unchanged; former NaN points 2-120 are removed. Both thread counts
must report `AUTO stopped: Branch 1 point 2: nonfinite AUTO solution`, with a
command location, after the failed-step retries. The remaining row is the
supplied start, not a certified solution. Every saved diagram is checked for
finite values; no connection reference or successful continuation is claimed.

### W345: tstheti's planar HomCont companion

W343's description and one-point baseline are retained unchanged as
`ode_tstheti`. `tstauto_tstheti_homcont` now runs
`examples/tstauto/tstheti_homcont.odex` in both directions through IPS=9,
IEQUIB=-2. Its descriptions use two `parameters` and a `connection` object
(`left`, `right`, `nunstab`, `nstab`); `execute_run` answers the existing AUTO
Homoclinic info form through `Server.answer_asks`. No core numerics changed.
Only `autocompare.py --update --only tstauto_tstheti_homcont` wrote a baseline, a new
one of 142 planar connection points
(71 per direction). All other models keep their baseline bytes.

`autocheck.py nagumo` reuses `read_description`, `compute` and `diagram_rows`
to compare every full-precision speed against `(1-2*a)/sqrt(2)`, rather than
comparing rounded plot coordinates. It also doubles NTST and lengthens the
interval. [Finding 50](xppaut-findings.md#50-auto-accepts-nan-as-a-converged-bvp-solution)
documents the derivation and truncation bounds. Neither independent program
was run for the companion; the description records that explicitly.

No previously compared special point becomes a disagreement with AUTO-07p. Predator-prey's extra
label remains within the previously traced underresolved regime; it does not establish quantitative
agreement for either LP. The stability restart check uses a ten-point periodic run (DS and DSMAX 0.01)
to keep the fixture short. W341 ([#402](https://github.com/MuhammadMoustafa/xppautX/issues/402))
preserves the restored solution's spectrum at a minimum-step MX endpoint; that endpoint no longer
means stability is unavailable. Its equality and nonzero-spectrum assertions are unchanged.


### W341: failed-step endpoint stability and lecar's repeated traversal

The 37 committed descriptions (nine skipped) have no changed baseline rows:
all 82 checks and 3,770 points across one/automatic threads agree exactly.
No baseline update is needed. The short `ode_lecar` and `tstauto_lecar` cycles
runs stop at NMX 30, before the repeated traversal is visible.

For the full fixture, reproduce `servercheck.py`'s `lecar_to_auto` (Hopf set,
Find Equilibrium, steady continuation), grab the first HB, and run periodic
without the W338 NMX 20 override. The model defaults are DS=DSMAX=0.02,
DSMIN=1e-5, NTST=15, NCOL=4 and NMX=2000. It stops at MX point 1459.
There are 13 low-period minima near the second HB, separated by about
108-115 rows: points 57, 165, 280, 391, 503, 617, 730, 843, 958, 1071,
1182, 1292 and 1402. Representative rows:

| periodic point | applied current | period |
|---|---|---|
| 57 | 0.45685135431228674 | 9.37798976551534 |
| 165 | 0.4568503941603806 | 9.377767071540763 |
| 1402 | 0.4568537604185555 | 9.378448462798955 |

Thus it retraces the Hopf-to-Hopf family about 13 times; these are repeated
visits, not 13 distinct physical orbit families. This observation does not
certify the coarse collocation solutions independently.

The original fixture's MX is an important distinction from a normal failed
continuation step: its location search restores an **unstored** solution,
not diagram point 1458. Row 1458 has current 0.26179561637933446 and period
14.608617105487582; MX 1459 has current 0.25946721925288646 and period
14.532605665920137. Its voltage amplitude is only about 7.72e-7, so it must
not be presented as an independently validated nontrivial periodic orbit.
W341 recomputes its stability from that restored solution's discrete Jacobian,
rather than copying row 1458's spectrum. For an uncached endpoint it uses the
full generalized eigenvalues of the rebuilt circuit pencil: the normal
`flowkm` calculation deflates an assumed multiplier near +1, which need not
exist when the orbit collapses. Regular continuation retains its original
deflated calculation, and cached endpoint spectra are copied exactly.
The MX spectrum is 0.7933499560998446 +/- 0.0585598149830441i. An independent
analytical Jacobian of the model at the nearly constant endpoint gives
`exp(J*T)` = 0.7933736684554719 +/- 0.0585564677729071i (difference about
2.4e-5); this near-constant comparison is not an independent orbit validation.
The saved-diagram comparison against
the pre-fix fixture changes only `evr1`, `evi1`, `evr2`, `evi2` of MX 1459: every other column
of every row is identical. Regular failed steps with a retained spectrum
reuse it exactly. [W341 / #402](https://github.com/MuhammadMoustafa/xppautX/issues/402).
