# AUTO checked against AUTO-07p and XPPAUT 8.0

W299 ([#357](https://github.com/MuhammadMoustafa/xppautX/issues/357)) keeps our own AUTO diagrams byte for
byte, which guards a change but not the numbers. W300
([#358](https://github.com/MuhammadMoustafa/xppautX/issues/358)) checks them against two programs that
were not written by us: **AUTO-07p 0.9.4** (the maintained AUTO, a newer line than XPPAUT's; the one reference since W357 ([#419](https://github.com/MuhammadMoustafa/xppautX/issues/419)), 0.9.3 having been used before) and **XPPAUT
8.0** itself (reference/xppaut-8.0; its AUTO is AUTO2000's C translation, [autlib5.c:2008](../reference/xppaut-8.0/autlib5.c#L2008)
names the translator, with XPPAUT's own glue in `auto_nox.c`, `autevd.c` and `autpp.c`). The comparison
is part of `tools/autocompare.py` and runs on every push from committed files: neither program is
installed in CI, nor vendored.

## Mesh adaptation and measured Floquet candidates (W361, [#423](https://github.com/MuhammadMoustafa/xppautX/issues/423))

Mesh redistribution now retains the EQDF monitor density and interpolates
from the nearer time endpoint, preserving both endpoints exactly; IPS 12
uses periodic derivative extension like IPS 2. This reimplements AUTO-07p
0.9.4's method. The [W361 study](studies/w361-auto-mesh-floquet.md) gives
before/after reference measures and attributes all 13 changed diagrams
(nine periodic and four BVP/HomCont) to mesh, including the numerical deltas.
All steady-state-only diagrams are unchanged.

Three independent Floquet experiments measured permutation-only pencil
balancing, the null-vector tolerance and the infinite-multiplier marker.
None improved any reported reference measure; all were omitted. EISPACK
and the existing SVD-deflated circuit-pencil path remain. All 151
AUTO-07p quantities pass before and after, and the worst tolerance fraction
is unchanged at 0.654 across all descriptions (0.296 over the nine periodic
descriptions). Raw last-digit errors need not decrease monotonically; the
study records the small increases too. W369 remains separate.

## Reduced equilibrium Hopf and structured fold curves (W363, [#425](https://github.com/MuhammadMoustafa/xppautX/issues/425))

The ODE Hopf curve now uses Kuznetsov (10.80), reimplemented in C++ from
AUTO-07p 0.9.4's [equilibrium.f90:84-258](../reference/auto-07p/src/equilibrium.f90#L84):
`F(u)=0`, `(A*A+kappa*I)v=0`, `dot(v,v)=1`, and
`dot((transpose(A)-A)*v_old,v)=0`, with `A=dF/du` and `kappa=omega^2`.
The unknowns are `[u(n), v(n), kappa, p2]`, `2n+2` in all. The start takes
a unit null vector of `A*A+kappa*I`, following
[equilibrium.f90:278-323](../reference/auto-07p/src/equilibrium.f90#L278).
Map and wave curves retain their separate eigenvector formulations; they are outside this ODE card.

The Hopf and algebraic fold Jacobians copy the model Jacobian and parameter
blocks, fill the vector and normalization derivatives directly, and centrally
difference only the state/parameter columns of the variational equations and
Hopf phase condition. This follows
[equilibrium.f90:84-187](../reference/auto-07p/src/equilibrium.f90#L84) and
[toolboxae.f90:171-265](../reference/auto-07p/src/toolboxae.f90#L171).
The principal parameter uses forward differences, as in AUTO-07p.

The reduced equations continue through `kappa=0` (Bogdanov-Takens). Negative
kappa describes a neutral saddle: the diagram and readout use curve code 13,
"neutral saddle", rather than code 3, "Hopf". W364 labels BT at the zero of kappa. AUTO-07p updates `PAR(11)=2*pi/sqrt(kappa)` only for positive
kappa ([equilibrium.f90:365-367](../reference/auto-07p/src/equilibrium.f90#L365));
for nonpositive kappa it retains the last period. We do the same, never taking
a square root of negative kappa or deriving a period from it. Starting a
periodic orbit from a grabbed neutral saddle is not offered; Run extends its curve.

Own session solutions use the new layout and reject the old 3n+2 Hopf
layout on the solution header line before restoring any state. No old-layout
restart reader is provided. `test_auto_hopf` checks closed-form residuals,
the structured Jacobian against full finite differences, the start and period
handling; `autocheck hopf_reduced` continues the BT normal form
`x'=y, y'=a+b*y+x*x` along `a=-x*x, b=0` across `x=0`.

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
  periodic run from the first HB of the named run, or a two-parameter curve from an LP or HB (below); XPPAUT 8.0 built from reference/xppaut-8.0 with its
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
  (parameter, norm) path length (both parameters on a cycle curve), measured from our own stored points on both sides of the special point,
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

## Two-parameter curves (W362, [#424](https://github.com/MuhammadMoustafa/xppautX/issues/424))

`tstauto_ab_fold` and `tstauto_ab_hopf` continue the exothermic reaction's fold (the LP of the feed `p0`
at the model's `p1` = 14) and Hopf point (the same `p1`, `p0` = 0.1309) in `p0` and `p1`, as a user does:
Grab the point, two-parameter. The Hopf description runs both directions of the curve (`hopf_up`, `hopf_down`).
A model has more than one description now (`autocompare.py` still requires every model to have one).

- **AUTO-07p's run** (`tools/auto_reference.py`): the equilibrium problem restarted from the label of the earlier
  run's LP or HB, `ISW=2`, `ICP=[p0, p1]`, the description's constants, `ILP=0` as ours. `ISP=1`, which ours
  does not have (`ISP=0`, core/auto_nox.cpp `auto_2p_limit`): it makes AUTO-07p label BT, CP, ZH and GH
  (`CHECKSP`, support.f90), and those labels are what W363 and W364 are judged against. Detection moves
  the steps around a label a little and nothing else, so the curves stay comparable. A curve of cycles (a PD)
  is not driven: no model of `tests/auto` has a PD point (AUTO-07p, 300 steps of the cycles of torx, frcp, pp2,
  ode/lecar, lo, lamom and lamom2: folds, branch points, none a PD), so it would be code nothing exercises.
- **What the file holds** (`tstauto_ab_fold.auto07p`): per run the special points as before, now with
  `par2` and the codim-2 types (BT, CP, ZH, GH, R1-R4, LPD, LTR, PTR, TTR) for a two-parameter run, and its
  `curve`: every step AUTO-07p took, `[parameter, second parameter, norm]` from its `b` file (NPR only decides
  which are labelled).
- **The comparison** (`compare_curve`): the programs' steps differ, so points are matched by where they lie. Every
  sample of the reference is put on our curve as a polyline in all three coordinates (with the parameters
  alone, the two arcs of the cusp lie together and are mixed up); then the other way round, so a curve that
  stops short or runs on is a difference. W400 matches against each segment's Newton and chord-error
  bounds below, choosing the least fraction of those bounds consumed. It intersects the allowed parameter
  intervals of all coordinates before accepting a match: roundoff in constant coordinates cannot set the
  direction, and a starting vertex on a closed curve cannot hide a valid coarser chord on a later lap.
  At that place the parameter, the second parameter and the norm must agree within
  `2 * EPS * (1 + |q|)` plus the polyline's chord error, a quarter of the largest second difference at the
  segment's ends. `EPS` is Newton's tolerance on every continued point (`EPSL`, `EPSU`), twice for two programs; no
  location term, the samples being converged points, not located ones. The chord of a smooth curve through points a
  step apart misses it by h^2|q''|/8 and the second difference estimates h^2|q''|; the steps vary by up to a factor
  two, hence a quarter. Only samples inside the run's bounds on the first parameter are compared:
  a run ends at the first point beyond a bound, which depends on the last step, and two programs' step sequences
  differ (ours reached p1 16.67 where AUTO-07p, 150 steps in, reached 16.56). A run that stops at its bound
  has the other curve around every sample inside it; one that stops short does not, and is a difference.
- **Special points**: a codim-2 point AUTO-07p labels and ours does not is a difference like any other, held in
  the description's `differences` until checked against independent derivatives (W364).

Result: both curves agree with AUTO-07p's. The fold (218 points of ours, 221 of the reference's) to 3.9e-5 in
`p0`, 1.8e-3 in `p1` and 6.3e-4 in the norm, the largest at the cusp (the norm at 0.65 of its tolerance, `p1` at
0.31). After W363 the Hopf curve in both directions (501 points of ours, 501 of the reference's) agrees to
2.05e-6 in `p0`, 6.40e-5 in `p1` and 5.22e-5 in the norm (previously 8.7e-6, 2.7e-4 and 9e-5). Nothing was widened. W364's updated comparisons and traced residual-contaminated labels are below.

### W363 baseline changes ([#425](https://github.com/MuhammadMoustafa/xppautX/issues/425))

Only `tstauto_ab_hopf` and `tstauto_ab_fold` were updated. At one and automatic
threads the resulting diagrams are byte-identical. The full 40-description
comparison passes, including the other 29 runnable baselines (nine descriptions
remain explicitly skipped). Tolerances and AUTO-07p reference files are unchanged.

- `tstauto_ab_hopf`: 350 to 623 total points (122 steady, 501 on the two Hopf
  runs). Replacing the reciprocal-frequency/complex-vector arclength coordinates
  by a unit real vector and squared frequency changes the sampling and step count.
  Among matched branch/step numbers the largest p1 change is 10.53224, norm
  3.64543, period 0.962963; these are different positions on the same curve,
  not same-solution errors. Comparison by geometric projection improves all three
  curve quantities at the reference's resolution, as above. Period is computed
  from the new positive kappa, and the exact normal-form tests check that formula.
- `tstauto_ab_fold`: 318 points before and after. The largest matched-step
  changes are p0 9.58e-7, p1 4.04e-5, norm 2.76e-6 and the auxiliary period
  0.59353 (570.31126 to 570.90479). A fold has no oscillation period: this
  field comes from intermediate complex eigenvalues near the double-zero point,
  and is sensitive to the Newton residual. The physical curve is unchanged at
  the reference's resolution: maximum errors remain p0 3.90e-5, p1 1.80e-3,
  norm 6.27e-4. The structured Jacobian now uses the model's Jacobian directly
  instead of differentiating that first derivative again in every column.

### W364: equilibrium codimension-two points ([#426](https://github.com/MuhammadMoustafa/xppautX/issues/426))

BT uses the left/right null-vector product on folds and kappa on Hopf curves;
CP uses the fold quadratic coefficient; ZH uses the next unconstrained
Jacobian eigenvalue and stability count; GH uses the first Lyapunov
coefficient from directional second/third derivatives. These are our own
implementations of the AUTO-07p 0.9.4 methods, located through W356's pipeline.
The diagonal second differences subtract the remaining Newton residual;
this deliberate numerical correction is explained in AUTO-07p finding 2.

Only three diagrams were written by the single `--update`:

- `tstauto_ab_fold`: 318 to 320 points; BT and CP location adds steps, and
  PAR(11) stays at the starting LP's 54.271663657 instead of being replaced
  by a near-axis pair's auxiliary period (formerly 570.90479 near BT).
- `tstauto_ab_hopf`: 623 points before and after; refreshing the current
  augmented Jacobian for test functions also synchronises its secondary
  parameter and spectrum with the corrected state. The extra GH labels
  of the reference are residual artifacts, traced below.
- `zero_hopf` (`tools/models/zero_hopf.odex`, outside the examples MD5 set): new, 71 points, including ZH at b=-5.1e-9 on the fold a=0.
  AUTO-07p 0.9.4 labels the same ZH (its reference generated at W364's
  review); the two agree to 2.1e-09 in the parameter, with no traced difference.

The older `tools/models/lecar_diagram.csv` fixture also contains a
2,000-point fold continuation: that part is refreshed for the same current
Jacobian/secondary-parameter/spectrum correction. Its 45-line header and
one-parameter prefix are kept literally unchanged. The checked-in
`lecar_auto.recx` starting snapshot is rewritten with the four required
boolean settings and a recomputed fingerprint; its recorded steps stay the same.

Both thread settings produce identical diagrams. All other diagrams,
including every one-parameter diagram, are unchanged. The full comparison
is 41 descriptions, nine skipped, 6,082 points, 93 checks, zero failures.
The table below uses the post-change `--table` output; the seven traced
reference labels are retained without widening any tolerance. Before W364,
the two reaction descriptions had six and three traced labels; afterwards
four and three, with two additional real special points now compared.

## Result

35 descriptions have an AUTO-07p reference (80 special points compared) and 24 an XPPAUT 8.0
reference (44 points). All 38 runnable descriptions agree in the W400 run: 47 descriptions,
9 skipped, 7186 stored points across one and automatic threads, 111 checks, zero failures. AUTO-07p has
25 traced points and XPPAUT 3; pending curve skips remain explicit. No tolerance was widened.
Before W326 there were 44 compared points and six traced points against AUTO-07p, and 44 compared
points with none traced against XPPAUT.

| Description | Reference | Compared points | Largest absolute differences | Result |
|---|---|---|---|---|
| henon_rot_lp | AUTO-07p | 2 | par 4.6e-12, norm 9.9e-11, spectrum 3.3e-06 | agrees, 3 traced |
| henon_rot_pd | AUTO-07p | 1 | par 0, norm 0 | agrees, 5 traced |
| henon_rot_tr | AUTO-07p | 1 | par 0, norm 4.2e-13 | agrees, 5 traced |
| hill_cycles_pd | AUTO-07p | 10 | par 9.3e-10, par2 4.6e-12, norm 6e-12, period 3.6e-10, spectrum 9.4e-07, curve par 2.7e-09, curve par2 1e-09, curve norm 8.4e-12 | agrees |
| hill_cycles_torus | AUTO-07p | 7 | par 2.1e-11, par2 3.9e-12, norm 7.7e-12, period 2e-11, spectrum 2.6e-06, curve par 2.1e-11, curve par2 4.4e-06, curve norm 1.5e-11 | agrees |
| ode_del_log | AUTO-07p | 1 | par 0, norm 4.9e-12 | agrees |
| ode_del_log | XPPAUT 8.0 | 1 | par 0, norm 2.2e-08 | agrees |
| ode_lecar | AUTO-07p | 4 | par 1.2e-05, norm 1.5e-06, period 0.0017, spectrum 0.0028 | agrees |
| ode_lecar | XPPAUT 8.0 | 3 | par 5.5e-05, norm 4.4e-08, period 5.7e-09, spectrum 4.1e-08 | agrees, 1 traced |
| ode_osc | AUTO-07p | 1 | par 0, norm 2.7e-11 | agrees |
| ode_osc | XPPAUT 8.0 | 1 | par 0, norm 4.7e-07 | agrees |
| tstauto_ab | AUTO-07p | 2 | par 3.1e-13, norm 1.1e-09, spectrum 4.2e-08 | agrees |
| tstauto_ab | XPPAUT 8.0 | 2 | par 2.2e-08, norm 4.6e-07, spectrum 2.9e-09 | agrees |
| tstauto_ab_fold | AUTO-07p | 4 | par 1.3e-06, par2 5.4e-05, norm 8.1e-05, spectrum 0.0006, curve par 3e-05, curve par2 0.00018, curve norm 2.5e-05 | agrees, 4 traced |
| tstauto_ab_hopf | AUTO-07p | 4 | par 1.6e-07, norm 9.7e-07, period 3e-05, spectrum 0.00012, curve par 5.2e-06, curve par2 1.9e-05, curve norm 6.6e-06 | agrees, 3 traced |
| tstauto_bigaut | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_bigaut | XPPAUT 8.0 | 1 | par 0, norm 0 | agrees |
| tstauto_bistable | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_bistable | XPPAUT 8.0 | 1 | par 0, norm 0 | agrees |
| tstauto_bob | AUTO-07p | 1 | par 0, norm 1.8e-12 | agrees |
| tstauto_bob | XPPAUT 8.0 | 1 | par 0, norm 1.4e-08 | agrees |
| tstauto_brper | AUTO-07p | 1 | par 0, norm 0 | agrees, 1 traced |
| tstauto_brper | XPPAUT 8.0 | 2 | par 2.5e-16, norm 0, period 2e-11, spectrum 1.1e-15 | agrees |
| tstauto_dumb | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_dumb | XPPAUT 8.0 | 1 | par 0, norm 0 | agrees |
| tstauto_dumbvp | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_eiz-hom | AUTO-07p | 1 | par 0, norm 1.2e-13 | agrees |
| tstauto_eiz-hom | XPPAUT 8.0 | 1 | par 0, norm 2e-09 | agrees |
| tstauto_exp | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_frcp | AUTO-07p | 2 | par 3.4e-12, norm 1e-13, period 2.1e-06, spectrum 0.00093 | agrees |
| tstauto_frcp | XPPAUT 8.0 | 2 | par 5.2e-06, norm 4.6e-08, period 2.5e-12, spectrum 4.1e-06 | agrees |
| tstauto_lamom | AUTO-07p | 2 | par 3.4e-18, norm 0, period 2e-11, spectrum 1e-14 | agrees |
| tstauto_lamom | XPPAUT 8.0 | 2 | par 2.7e-16, norm 0, period 2e-11, spectrum 1e-14 | agrees |
| tstauto_lamom2 | AUTO-07p | 3 | par 9.2e-13, norm 5.9e-12, period 2.6e-11, spectrum 3.8e-06 | agrees |
| tstauto_lamom2 | XPPAUT 8.0 | 3 | par 6.8e-09, norm 4e-08, period 2.6e-11, spectrum 2.4e-08 | agrees |
| tstauto_lecar | AUTO-07p | 4 | par 1.2e-05, norm 1.6e-06, period 0.0017, spectrum 0.0029 | agrees |
| tstauto_lecar | XPPAUT 8.0 | 3 | par 5.5e-05, norm 4e-08, period 3.5e-09, spectrum 4.7e-08 | agrees, 1 traced |
| tstauto_lo | AUTO-07p | 2 | par 2.7e-18, norm 0, period 2e-11, spectrum 9.8e-15 | agrees |
| tstauto_lo | XPPAUT 8.0 | 2 | par 2.5e-16, norm 0, period 2e-11, spectrum 2.5e-16 | agrees |
| tstauto_pen | AUTO-07p | 1 | par 0, norm 3.6e-12 | agrees |
| tstauto_pen | XPPAUT 8.0 | 1 | par 0, norm 4.9e-09 | agrees |
| tstauto_pf | AUTO-07p | 2 | par 2.7e-18, norm 0, spectrum 9.8e-15 | agrees |
| tstauto_pf | XPPAUT 8.0 | 2 | par 2.5e-16, norm 0, spectrum 2.2e-16 | agrees |
| tstauto_pp2 | AUTO-07p | 3 | par 1.8e-12, norm 3.3e-12, period 6.3e-11, spectrum 5e-07 | agrees, 4 traced |
| tstauto_pp2 | XPPAUT 8.0 | 4 | par 4.8e-08, norm 4.9e-07, period 2.3e-10, spectrum 2.9e-07 | agrees, 1 traced |
| tstauto_san | AUTO-07p | 2 | par 2.4e-12, norm 2.8e-15, spectrum 1.3e-07 | agrees |
| tstauto_san | XPPAUT 8.0 | 2 | par 1.7e-08, norm 4.1e-11, spectrum 1.3e-07 | agrees |
| tstauto_she | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_she | XPPAUT 8.0 | 1 | par 0, norm 0 | agrees |
| tstauto_simplefold | AUTO-07p | 3 | par 4.8e-12, norm 4.5e-11, spectrum 8.6e-11 | agrees |
| tstauto_simplefold | XPPAUT 8.0 | 3 | par 2.1e-08, norm 4.3e-08, spectrum 2.7e-13 | agrees |
| tstauto_torx | AUTO-07p | 3 | par 4.7e-12, norm 0, period 3.3e-12, spectrum 3.9e-07 | agrees |
| tstauto_torx | XPPAUT 8.0 | 3 | par 2.2e-08, norm 0, period 3.3e-12, spectrum 3.7e-08 | agrees |
| tstauto_torx_homcont | AUTO-07p | 2 | par 4.1e-12, norm 2.5e-15, spectrum 3.2e-07 | agrees |
| tstauto_tsthom | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_tsthom | XPPAUT 8.0 | 1 | par 0, norm 0 | agrees |
| tstauto_wcbiwave | AUTO-07p | 1 | par 0, norm 0 | agrees |
| tstauto_wcbiwave | XPPAUT 8.0 | 1 | par 0, norm 0 | agrees |
| zero_hopf | AUTO-07p | 3 | par 2.1e-09, par2 1.3e-40, norm 5.8e-17, spectrum 2.1e-09, curve par 7.6e-17, curve par2 6e-18, curve norm 3.6e-13 | agrees |

Rows above are emitted by `autocompare.py --table` (W400): `agrees` means every compared quantity is
inside its tolerance; `traced` counts the explicitly recorded differences. The differences include both
parameters on curves, norm, period and spectrum (the eigenvalue or multiplier pair furthest apart). Where the code path is shared the
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

Five causes in six descriptions; W326 fixes the missing restore and records the resulting differences with XPPAUT.

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

4. **`tstauto_ab_fold`: the four extra CP labels of AUTO-07p are not cusps.**
   W364 locates BT and the analytic cusp, agreeing with the reference's BT and third CP
   within the unchanged tolerances. The other four CP remain traced: their diagonal
   second-difference stencil retains `2*F(u)/h^2` from the finite Newton residual.
   xppautX subtracts that constant term. See [AUTO-07p finding 2](auto07p-findings.md#2-a-newton-residual-contaminates-cp-and-gh-test-functions).
5. **`tstauto_ab_hopf`, `hopf_up`: the three GH labels of AUTO-07p are not generalised Hopf points.**
   The same residual term contaminates its diagonal second derivatives. An independent
   calculation with exact analytic tensors gives a negative first Lyapunov coefficient
   throughout their neighbourhood near `t=p0*exp(u2)=5.5`; the planar formula has sign
   `24-10*t-8*t^2-t^3`. xppautX labels none there; its GH detection is checked on the
   Bautin normal form `l1=2*b`, including continuation through `b=0`.
   See [AUTO-07p finding 2](auto07p-findings.md#2-a-newton-residual-contaminates-cp-and-gh-test-functions).

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
- **Two-parameter curves, XPPAUT 8.0 and the cycles' curves** (W362): `tstauto_ab_fold` and `tstauto_ab_hopf` carry an
  `xppaut_skip`: the keys of `tools/auto_reference.py` do not continue a curve in XPPAUT's window. A period-doubling
  curve has no description: no model of `tests/auto` has a PD point (section "Two-parameter curves"). ZH and the
  cycles' R1-R4, LPD, LTR, PTR, TTR have no reference either: no model here reaches them.
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

## W402: references for cycle curves, maps and HomCont ([#464](https://github.com/MuhammadMoustafa/xppautX/issues/464))

`tools/auto_reference.py` now writes AUTO-07p 0.9.4's references for three kinds of run that W400 (codimension-two
points on cycle curves, [#462](https://github.com/MuhammadMoustafa/xppautX/issues/462)) and W365 (HomCont's
projection, [#427](https://github.com/MuhammadMoustafa/xppautX/issues/427)) need, and fixes nothing in xppautX.

- **A two-parameter curve of cycles** (an LP, PD or TR of a periodic run): IPS=2, ISW=2, ISP=2, ICP the two
  parameters and the period. From a PD, TR or LP AUTO-07p first writes the curve's starting data (an EP) and the
  curve is the run restarted from that; xppautX does both in one run. The direction the restart takes is not fixed by
  the sign of DS, and the two programs' DS differ: the references follow AUTO-07p's.
- **A two-parameter curve of a map's fixed points** (an LP, PD or TR): IPS=-1, ISP=1. xppautX labels a map's torus
  bifurcation HB and finds no flip bifurcation of a map; the description's `HB` start is AUTO-07p's `TR`.
  AUTO-07p's printed eigenvalues of a map's fixed point are its multipliers.
- **A HomCont run** (`homoclinic`, IEQUIB=1, `heteroclinic`, -2): one run from the orbit the description integrates
  (`settle`); STPNT integrates the same orbit with RK4 over `T*PAR(11)`.

AUTO-07p labels the 1:1, 1:2, 1:3 and 1:4 resonances (R1-R4) on torus, flip and fold curves of cycles and maps, but the
fold-flip, fold-torus, flip-torus and torus-torus points (LPD, LTR, PTR, TTR) **only for maps**
(`src/support.f90` CHECKSP, `src/maps.f90`; the table of its manual says "maps" for each). A flow's cycle curve never
carries them, so W400's cycle-curve work covers R1-R4 and the map work the rest. The demos' flows do not reach
the labels robustly: `tor`'s fold curve of cycles reaches R1 near rn = -0.5973 with AUTO-07p's own constants (analytic
Jacobian, the demo's step) but not with the constants xppautX fixes (ITNW=7, numerical Jacobian, NMX, DSMAX)
-- the curve passes within 7e-4 of the Hopf point, where either of two curves is followed by a small change of step.
It is not a reference. The demo's torus branch (R1 at the same place, label -85) starts from a torus bifurcation on
the branch from the first branch point, which xppautX's switched branch does not have.

| Description | Model | Reaches in AUTO-07p | xppautX today |
|---|---|---|---|
| `hill_cycles_torus` | `tools/models/hill_cycles.odex` | torus curve of cycles: R4 (d = 0.5637; 9/16 = 0.5625 for q = 0), R3 (d = 0.4464; 4/9 = 0.4444), a BP of the curve near d = 0.277 | R3, R4 and BP agree; the straight curve has max abs(c + mu) < 7.9e-11; the uncoupled R3/R4 places agree within 1.1e-7 (W400) |
| `hill_cycles_pd` | the same | period-doubling curve of cycles: a closed lens, R2 at (c, d) = (-0.17495, 0.3107) and (-0.17494, 0.1854), where c + mu = 0 exactly (mu = 0.1749 at PD1) | all five R2 encounters agree, on the same closed lens; max abs(c + mu) at a labelled end is 1.8e-5, within location tolerance (W400) |
| `henon_rot_tr` (curve skipped) | `tools/models/henon_rot.odex` (the hen demo's Henon map beside a rotation) | torus curve of the map: R2 (alpha = 3), R3 (1.25), TTR (1.1), R4 (0), R1 (-1), the exact places of x = -cos(theta) | runs, labels none of them |
| `henon_rot_pd` (curve skipped) | the same | flip curve: PTR (0.5139, 3.1198), R2 (3), LPD (0, -1), PTR (0.0195, 1.4136) | finds no flip point to start from |
| `henon_rot_lp` (curve skipped) | the same | fold curve: LTR (-0.0127, -0.7873), LPD (0, -1), R1 (-1, 1) | stops at the second point (nonfinite) |
| `tstauto_torx_homcont` (curve skipped) | `examples/tstauto/torx.odex` at the cir demo's parameters | HomCont, three dimensions (one real unstable eigenvalue, a stable pair): the curve in (rn, be) from (-0.721309, 0.6), an LP at (-0.71809, 0.54238) | runs; the same LP, but the orbit's norm differs at the first point (0.214167 against 0.214053), W365 |

`hill_cycles` has closed forms: its torus bifurcation is at mu = -c = 0.3 (AUTO-07p 0.3), the resonances of the torus curve
at sqrt(d) = 1/4, 1/3, 1/2, 2/3, 3/4 for q = 0 (the places above move by about 1e-3 for q = 0.1, the Hill equation's
coupling), the lens of the period-doubling curve ends where both multipliers are -1, c = -mu. `henon_rot` has them too:
the torus curve is beta = 1 with cos(theta) = -x, x = -1 + sqrt(1 + alpha); the fold curve alpha = -(1+beta)^2/4, the flip
curve alpha = 3(1+beta)^2/4, and the rotation's radius is 1 on beta = 2 alpha - 1.2, which crosses them at alpha = 1.1,
(-0.0127, -0.7873) and (0.0196, 0.5137): AUTO-07p's TTR, LTR and PTR are exactly there (to 1e-4).

How a reference reaches the repository when xppautX cannot run it yet: `NAME.auto07p` holds every run, and a run
skipped in the description (its reason names W400, W404 or W365) is read but not compared; the runs xppautX does run are compared
(the base runs, whose points all agree, and a map's, with the five traced name differences HB/TR and the undetected PD
that the `differences` list records). The card that makes xppautX run the curve drops the skip.
The `tools/autocompare.py` changes that came with it: a curve of cycles' first rows (the starting data, one continued parameter)
are not on the curve; a spectrum's order of a conjugate pair no longer depends on the last bit of its real parts; a
HomCont run's reference is read like a two-parameter run's.

## W403: references from published models and models of our own ([#465](https://github.com/MuhammadMoustafa/xppautX/issues/465))

Five more descriptions, each with AUTO-07p 0.9.4's reference made by `tools/auto_reference.py` and a baseline of xppautX's own
diagram. Fixes nothing in xppautX: a curve it runs but labels differently, or a HomCont run it cannot follow, is skipped in the
description with a reason naming W400 ([#462](https://github.com/MuhammadMoustafa/xppautX/issues/462)) or W365
([#427](https://github.com/MuhammadMoustafa/xppautX/issues/427)), as W402's are. Each run's skip says what AUTO-07p labels there.

| Description | Model | Source | AUTO-07p reaches | xppautX today |
|---|---|---|---|---|
| `res_cycles` | `tools/models/res_cycles.odex`: a Hopf cycle beside a Bogdanov-Takens system (w,v) | closed form | torus curve of cycles `c = -mu - sqrt(b)`: R1, R4, R3, R2 at the exact places below | runs both directions of the curve, labels none (PENDING W400) |
| `forced_pp_pd` | `tools/models/forced_predator_prey.odex` | Kuznetsov, Muratori, Rinaldi 1992; MatCont Tutorial V (Kuznetsov, Govaerts, Della Rossa, De Witte) | PD of the period-1 cycle; its closed PD curve with R2 at both ends | the PD is found, the curve runs, no R2 (PENDING W400) |
| `forced_pp_ns` | the same, at b0 = 0.338 | the same | TR of the period-1 cycle; its NS curve to the R2 end, labelled BP; the curve ends at the unforced Hopf point | runs, no BP (PENDING W400) |
| `lorenz_homoclinic` | `tools/models/lorenz_homoclinic.odex` | Sparrow 1982, Kaplan and Yorke 1979 | HomCont in three dimensions, r = 13.92655741 to 15.5988 in (r, b) | stops at its first step: singular Newton system (PENDING W365) |
| `explicit_homoclinic` | `tools/models/explicit_homoclinic.odex` | closed form | HomCont in three dimensions, q = 1 to 3.0145 on k = 0 | stops at its second point: no convergence at DSMIN (PENDING W365) |

**R1 on a cycle curve (`res_cycles`).** The cycle `(x,y) = sqrt(mu)(cos t, sin t)`, `(w,v) = (sqrt(b), 0)` has period 2 pi and, besides
the trivial multiplier, `exp(2 pi lambda)` for the eigenvalues of `lambda^2 + (c+mu+w) lambda + 2w = 0`: the torus curve is
`c = -mu - w` and the pair's angle `theta = 2 pi sqrt(2w)`, so the resonances are at `w = 1/2` (R1, theta = 2 pi), `9/32` (R4),
`2/9` (R3), `1/8` (R2) and `0` (R1 again, where `b = w^2` has its fold). The start is the torus point at mu = 0.5, c = -0.8, b = 0.09.
AUTO-07p labels, with mu = 0.5 (c = -0.5 - w, b = w^2):

| label | AUTO-07p (c, b) | closed form (c, b) |
|---|---|---|
| R1 | (-0.99999999, 0.24999998) | (-1, 1/4) |
| R4 | (-0.78125002, 0.0791016) | (-0.78125, 0.0791016) |
| R3 | (-0.72222216, 0.0493827) | (-0.722222, 0.0493827) |
| R2 | (-0.62500002, 0.0156250) | (-0.625, 0.015625) |
| BP | (-0.50000004, 1e-15) | R1 at (-0.5, 0) |

All agree to the tolerances (EPSL 1e-7: 2e-8 here). Two things AUTO-07p does not do, both from its source and neither a defect:
the test function of a torus curve is `theta (theta - pi/2)(theta - 2 pi/3)(theta - pi)` on the continued angle
([periodic.f90:2044](../reference/auto-07p/src/periodic.f90#L2044)), so it labels R1-R4 only while the angle runs through (0, pi]:
past R1 the angle is negative, and the R4 at `w = 25/32`, R2 at `9/8`, ... of the closed form are not labelled. At `w = 0`, where
the cycle is a fold in b, the label is a BP (the torus system is singular there), not R1. xppautX's samples of the curve agree with
AUTO-07p's only to the curve tolerance after the 99th step of the down direction.

**The forced predator-prey model.** `x' = r x (1 - x/K) - a x y / (b0 (1 + eps u) + x)`, `y' = e a x y / (b0 (1 + eps u) + x) - d y`
(K = 1, r = d = 6.28, a = 12.56, e = 1, the tutorial's rounded values), the periodic forcing `eps cos(2 pi t)` supplied by
`u' = m u - 2 pi v - (u^2+v^2) u`, `v' = 2 pi u + m v - (u^2+v^2) v`. The tutorial has m = 1 and finds its cycle by integration;
here the circle's radius `sqrt(m)` is born at a Hopf point (m = 0) and the (x,y) system feels `eps sqrt(m) cos(2 pi t)`, so the
tutorial's eps is `eps sqrt(m)` (m is the value of the run's point; the two-parameter curve runs in eps at that m). The
tutorial's printed pairs are (b0, eps): its Figure 1 and the zoom show eps along the abscissa. `x = b0, y = b0 (1 - b0)` is the
(x,y) equilibrium (a e = 2 d).

| point | published (MatCont 7.3) | AUTO-07p 0.9.4 |
|---|---|---|
| PD of the period-1 cycle, b0 = 0.4 | eps = 0.3050914 | 0.3050919 (m = 0.3723242, eps = 0.5) |
| R2 of the PD curve | (b0, eps) = (0.3401303, 0.3362213) | (0.3401302, 0.3362215) on the PD curve; (0.3401301, 0.3362216) as the BP at the end of the NS curve |
| the other R2 of the closed PD curve | "far away", not given | (b0, eps) = (2.5702457, 1.18921) |

AUTO-07p's numbers differ from the paper's by 5e-7 and 2e-7: the tutorial's values are MatCont's, found with its own tolerances,
and ours with `NTST = 40`, `NCOL = 4`. The curve step matters for the label: with `DSMAX = 0.05` the PD curve's R2 is
(0.3401071, 0.3362670), 2e-5 and 5e-5 off, so the reference uses 0.005 (the NS curve's BP is the same to 1e-7 at 0.05). The R1, R3,
R4 and R2 the tutorial lists on the Neimark-Sacker curve of the **period-2** cycle (b0 = 0.1771951, 0.1772744, 0.1798043, 0.1838264;
eps = 0.6029715, 0.6740362, 0.7084323, 0.7431615) are not reproduced: a period-2 cycle there needs a start the descriptions cannot give
(a run starts only at a Hopf point or a labelled point of an earlier run, and the period-1 period doubling is at b0 >= 0.318; the
tutorial reaches it by integration), so a card that adds such a start (a stored orbit as a run's start) can add those four. The model's
header comment lists their values.

**Lorenz.** `x' = s (y - x)`, `y' = r x - y - x z`, `z' = x y - b z` at s = 10, b = 8/3. The origin has the eigenvalues 7.130, -2.667,
-18.130 at r = 13.9266: one unstable and two stable, where HomCont's projection matters. The published homoclinic r is 13.926; the orbit
from the unstable manifold (x = 1e-4) returns to the origin along the z axis, and bisecting on its side finds r = 13.92655741
(the description's start, the model's value). AUTO-07p's Newton solution of the boundary value problem leaves r at 13.92655741 and
continues the curve in (r, b) to (15.5988, 3.0903). It does so only with the shorter truncated interval 4.6 and 200 mesh intervals: from
5.8 and 35 intervals, with the orbit's far end at 6e-4 from the origin, the first Newton step diverges (singular pivot). From r = 13.9265
or 13.926 the first point does not converge either.

**An explicit homoclinic orbit.** `x' = y`, `y' = q (x - x^2) - k y`, `z' = -2 (z - x^2) + 2 x y`: at k = 0 the orbit
`x = 3/2 sech^2(sqrt(q) t/2)`, `y = x'`, `z = x^2` is homoclinic to the origin (eigenvalues sqrt(q), -sqrt(q), -2) for every q. The loop's
L2 norm over the interval 22 is `sqrt((int x^2 + y^2 + z^2 dt)/22)`, and AUTO-07p's matches it:

| q | AUTO-07p norm | closed form | k |
|---|---|---|---|
| 1 | 0.86489996318 | 0.86489996419 | 0 |
| 3.0145499523 | 0.70294597955 | 0.70294597391 | -1.5e-10 |

Over the 45 points of the curve k stays within 2.3e-10 of 0 and the norm within 1.7e-7 of the closed form (the difference is the
window: the orbit starts at x = 1e-4 and not at the end of the integral). W365's projection can be judged against this exact orbit.
xppautX's HomCont run stops at the second point for this model and at the first for Lorenz's.

## W400: R1-R4 on curves of cycles ([#462](https://github.com/MuhammadMoustafa/xppautX/issues/462))

W400 drops the two Hill descriptions' curve
skips. Before this card the base TR was at mu = 0.299999999753 and the first PD at mu = 0.174946997174;
those one-parameter rows remain byte-for-byte equivalent as parsed diagram records. The new curve runs
locate R4 at d = 0.563694064300, R3 at d = 0.446356111806, and the five R2 encounters at alternating
ends d = 0.310709537918 and 0.185352874006, agreeing with AUTO-07p. At q = 0 the independently known
R4 (9/16) and R3 (4/9) differ by 5.75e-8 and 1.03e-7. R1 has no flow reference here yet (W403); its
fold/torus tests have synthetic-spectrum coverage, not a claimed flow-reference comparison.

Two causes hid agreement before W400: torus startup oriented the tangent by roundoff in the constant
first parameter (AUTO-07p's STDRBV uses the first significant component), and the comparator amplified
constant-coordinate roundoff and chose a starting vertex over a valid coarse chord on a later lap of
the PD lens. Matching against the existing Newton/chord bounds fixes both straight and closed curves;
no acceptance bound was increased. The original orbit's Floquet pencil is extracted from the augmented
cycle system using the numerical owner's partial-pivot LU and existing FLOWKM; it supplies both the
resonance test and stored stability. The new detector uses W356's location and W364's controls/storage.

The shipped lecar_auto.recx snapshot now includes the four resonance booleans and its missing W376
main-window open flag; its recording commands and all other snapshot data are unchanged.
