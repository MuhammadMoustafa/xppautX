# AUTO checked against AUTO-07p and XPPAUT 8.0

W299 ([#357](https://github.com/MuhammadMoustafa/xppautX/issues/357)) keeps our own AUTO diagrams byte for
byte, which guards a change but not the numbers. W300
([#358](https://github.com/MuhammadMoustafa/xppautX/issues/358)) checks them against two programs that
were not written by us: **AUTO-07p 0.9.4** (the maintained AUTO, a newer line than XPPAUT's; the one reference since W357 ([#419](https://github.com/MuhammadMoustafa/xppautX/issues/419)), 0.9.3 having been used before) and **XPPAUT
8.0** itself (reference/xppaut-8.0; its AUTO is AUTO2000's C translation, [autlib5.c:2008](../reference/xppaut-8.0/autlib5.c#L2008)
names the translator, with XPPAUT's own glue in `auto_nox.c`, `autevd.c` and `autpp.c`). The comparison
is part of `tools/autocompare.py` and runs on every push from committed files: neither program is
installed in CI, nor vendored.

## Steady-state starts refined by Newton (W495, [#559](https://github.com/MuhammadMoustafa/xppautX/issues/559))

A steady-state start from initial conditions whose largest residual is above
EPSU is refined by Newton's method, with the Sing pts settings, before the
run (xppautX refines an inexact steady-state start; AUTO-07p starts from it
as given). The run reports it: `autoinfo`'s `start_refined` (docs/protocol.md).
Principal-parameter bounds only: the second parameter of a two-parameter run
is not bounded, as in AUTO-07p (`ae.f90` tests U(NDIM+1) only).

Diagrams of the descriptions whose start was refined changed on purpose; their
first point is the refined equilibrium. Against AUTO-07p and XPPAUT 8.0,
autocompare leaves a refined run's first point out of the special-point and
quantity comparison, because both programs start from the inexact point, and
keeps every later point in it. Descriptions this applies to and that pass:
`explicit_homoclinic`, `lorenz_homoclinic`, `tstauto_eiz-hom`,
`tstauto_torx_homcont` and `tstauto_wcbiwave`. Not yet passing for other
reasons: `tstauto_san` (a later special point, the BP, which we do not find)
and the two `ode_mu26` descriptions (their baselines, see the W495 study).

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

W428 keeps XPPAUT's parameter slots stable across the named runs. A curve's two parameters
are selected in Axes; the displayed ranges do not add a stopping bound on the second
parameter. The Numerics RL0/RL1 still bound the first parameter, exactly as the description
specifies. The original one-parameter references are kept byte for byte.

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

For XPPAUT curve samples, W428 also accounts for the `.b` file's `%14.5E`
format ([autlib1.c:3035-3041](../reference/xppaut-8.0/autlib1.c#L3035)):
six significant digits. The curve comparison adds half a last printed unit,
`0.5 * 10**(floor(log10(M)) - 5)`, where `M` is the absolute reference sample or the largest absolute coordinate
of the two reference segment endpoints (zero contributes zero). This bounds
the reference's rounding in either projection direction. AUTO-07p's comparison
and all Newton/location tolerances are unchanged. A constant-norm regression
accepts the rounded export but rejects an offset larger than the printed unit.

## Two-parameter curves (W362, [#424](https://github.com/MuhammadMoustafa/xppautX/issues/424))

`tstauto_ab_fold` and `tstauto_ab_hopf` continue the exothermic reaction's fold (the LP of the feed `p0`
at the model's `p1` = 14) and Hopf point (the same `p1`, `p0` = 0.1309) in `p0` and `p1`, as a user does:
Grab the point, two-parameter. The Hopf description runs both directions of the curve (`hopf_up`, `hopf_down`).
A model has more than one description now (`autocompare.py` still requires every model to have one).

- **AUTO-07p's run** (`tools/auto_reference.py`): the equilibrium problem restarted from the label of the earlier
  run's LP or HB, `ISW=2`, `ICP=[p0, p1]`, the description's constants, `ILP=0` as ours. `ISP=1`
  makes AUTO-07p label BT, CP, ZH and GH (`CHECKSP`, support.f90); xppautX detects those after W364.
  Detection moves steps around a label a little, so curves are compared geometrically. W402 also drives
  LP/PD/TR curves of cycles and maps; W428 drives XPPAUT's available menu paths, which keep ISP 0.
- **What the file holds** (`tstauto_ab_fold.auto07p`): per run the special points as before, now with
  `par2` and the codim-2 types (BT, CP, ZH, GH, R1-R4, LPD, LTR, PTR, TTR) for a two-parameter run, and its
  `curve`: every step AUTO-07p took, `[parameter, second parameter, norm, period]` from its `b` file (the PERIOD column, 0 for an
  equilibrium; an `.xppaut` curve sample keeps `[parameter, second parameter, norm]`: one format per program, W539) (NPR only decides
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

49 descriptions have an AUTO-07p reference (176 special points compared before W458, and the two-parameter curves) and 43 an
XPPAUT 8.0 reference (120 points before W458); 43 hold both. There are 36 traced difference entries for AUTO-07p
and 58 for XPPAUT, including ten and five quantity-specific discrepancies respectively (four of AUTO-07p's
and one of XPPAUT's since [W474](#w474-auto-07ps-nested-dissection-for-the-collocation-blocks-537), one and two
since [W458](#w458-the-first-steps-of-cycles-from-a-hopf-point-521), whose `bautin_near_hopf` adds a description with both). Those fifteen
quantities remain outside their unchanged bounds; the other quantities and labels stay checked. The full
W437 comparison passes: 60 descriptions, 9 skipped, 18674 computed points across one and automatic threads,
150 checks, zero untraced failures. Both Mu26 and the sixteen approved periodic baselines were updated;
each is named with its change size [below](#w437-user-points-on-mu26s-approach-to-the-snic-499).
The [before-update impact report](studies/w437-phase-impact.md) preserves the initial 36 failures.
Newton and location tolerances are unchanged; XPPAUT curve exports include their half-unit printing uncertainty
(above). W428 ([#490](https://github.com/MuhammadMoustafa/xppautX/issues/490)) adds eleven XPPAUT references; only the map fold
remains refused among the twelve two-parameter descriptions. Three one-parameter references (`tstauto_ab`, `tstauto_dumb`,
`ode_del_log`) were replayed byte for byte, and no existing reference file changed. Before W326 there were 44 compared points
and six traced points against AUTO-07p, and 44 compared points with none traced against XPPAUT.

| Model | Special points in xppautX | AUTO-07p | XPPAUT 8.0 |
|---|---|---|---|
| bautin_near_hopf | steady: EP HB; cycles_up: LP; cycles_down: LP | agrees, 4 traced (3 points; par 1.5e-09, norm 0.00048, period 2e-11, spectrum 2e-06; curve par 5.6e-05, par2 0, norm 3.5e-05) | agrees, 2 traced (4 points; par 1.5e-09, norm 0.00048, period 2e-11, spectrum 1.6e-06; curve par 5.6e-05, par2 0, norm 1.1e-05) |
| explicit_homoclinic | steady: EP; homoclinic: EP | agrees (2 points; par 0, norm 1.8e-08; curve par 5.2e-08, par2 2.7e-15, norm 1.8e-08) | not run |
| forced_pp_ns | steady: EP HB; cycles: TR PD; ns_up: BP; ns_down: - | agrees (5 points; par 7.9e-10, par2 9.1e-11, norm 4.3e-10, period 2e-14, spectrum 1.9e-05; curve par 0.00053, par2 9.8e-06, norm 4.9e-06) | agrees, 1 traced (4 points; par 3.1e-08, norm 2.6e-08, period 2e-14, spectrum 3.8e-07; curve par 0.00062, par2 7.9e-06, norm 4.2e-06) |
| forced_pp_pd | steady: EP HB; cycles: PD; pd_up: R2; pd_down: R2 | agrees, 4 traced (5 points; par 5e-06, par2 5.2e-05, norm 3.5e-06, period 2.1e-14, spectrum 0.00032; curve par 9.5e-07, par2 7.7e-05, norm 5.8e-06) | agrees, 2 traced (3 points; par 1.6e-08, norm 4.8e-08, period 2.1e-14, spectrum 3.1e-07; curve par 6.5e-06, par2 0.0029, norm 0.00019) |
| henon_rot_lp | steady: EP LP TR PD; up: LTR LPD R1; down: LTR | agrees (8 points; par 1.4e-09, par2 1.4e-09, norm 1.2e-09, spectrum 1.7e-05; curve par 9.1e-06, par2 2.8e-06, norm 1.9e-06) | not run |
| henon_rot_pd | steady: EP PD TR TR; up: PTR R2 PTR; down: LPD PTR PTR | agrees (10 points; par 9.8e-09, par2 3.7e-09, norm 2.2e-09, spectrum 1.3e-05; curve par 1.4e-09, par2 6.8e-09, norm 2.9e-09) | agrees, 6 traced (4 points; par 2.6e-07, norm 3.5e-07, spectrum 7e-07; curve par 0.0003, par2 0.00033, norm 0.0076) |
| henon_rot_th | steady: EP PD TR TR; turn: R4 R3 R2 BP R3 R4 | agrees, 2 traced (8 points; par 3.8e-05, par2 2.8e-11, norm 7.7e-11, spectrum 3.5e-05; curve par 5e-11, par2 2.8e-11, norm 5.7e-12) | agrees, 6 traced (4 points; par 2e-07, norm 3e-07, spectrum 8.4e-07; curve par 5.2e-07, par2 3.9e-08, norm 3e-07) |
| henon_rot_tr | steady: EP PD TR TR; up: R2 | agrees (5 points; par 1.3e-09, par2 1.4e-11, norm 4.5e-10, spectrum 3.8e-05; curve par 3.2e-08, par2 4.5e-09, norm 2.5e-08) | agrees, 1 traced (4 points; par 2e-07, norm 3e-07, spectrum 8.4e-07; curve par 0.00062, par2 1.8e-07, norm 2.8e-05) |
| hill_cycles_pd | steady: EP HB HB; cycles: PD PD; doubling: R2 R2 R2 R2 R2 | agrees (10 points; par 2.4e-10, par2 4.6e-12, norm 6e-12, period 3.6e-10, spectrum 9.4e-07; curve par 2.6e-09, par2 1e-09, norm 8.4e-12) | agrees, 5 traced (5 points; par 4e-08, norm 4.2e-08, period 3.6e-10, spectrum 2.8e-07; curve par 0.0071, par2 0.0013, norm 3.4e-07) |
| hill_cycles_torus | steady: EP HB HB; cycles: TR; torus_up: R4; torus_down: R3 BP R3 | agrees, 1 traced (7 points; par 5.8e-11, par2 1e-11, norm 7.7e-12, period 2e-11, spectrum 4.1e-06; curve par 5.8e-11, par2 1.9e-08, norm 1.5e-11) | agrees, 4 traced (4 points; par 4e-08, norm 4.3e-08, period 2e-11, spectrum 5.7e-08; curve par 2.5e-10, par2 0.0002, norm 4.4e-07) |
| izh_class1_integrator | steady: EP LP | agrees (2 points; par 7.5e-15, norm 4.1e-10, spectrum 2e-08) | agrees (2 points; par 7.5e-15, norm 3.7e-06, spectrum 2e-08) |
| izh_class2 | steady: EP HB LP | agrees (3 points; par 4.4e-13, norm 3.7e-10, period 6.6e-11, spectrum 4.6e-07) | agrees (3 points; par 7.1e-06, norm 4.7e-06, period 1.5e-05, spectrum 4.1e-08) |
| izh_resonator | steady: EP HB LP | agrees (3 points; par 2.5e-12, norm 4.5e-10, period 3.1e-10, spectrum 2e-07) | agrees (3 points; par 2.1e-05, norm 3.2e-06, period 3.9e-05, spectrum 1.6e-07) |
| lorenz_homoclinic | steady: EP; homoclinic: EP | agrees (2 points; par 0, norm 1.8e-08; curve par 5.8e-08, par2 1.4e-08, norm 6.3e-08) | not run |
| ode_del_log | steady: EP | agrees (1 points; par 0, norm 4.9e-12) | agrees (1 points; par 0, norm 2.2e-08) |
| ode_lecar | steady: EP HB HB; cycles: LP | agrees (4 points; par 2.7e-11, norm 2.5e-11, period 9.6e-09, spectrum 4.2e-07) | agrees, 1 traced (3 points; par 5.4e-05, norm 1.5e-06, period 0.0017, spectrum 0.00041) |
| ode_ma23 | steady: EP LP LP HB; cycles: - | agrees (4 points; par 4.6e-07, norm 5e-09, period 5.7e-09, spectrum 1.2e-06) | agrees (4 points; par 9.3e-06, norm 4.7e-06, period 2.1e-08, spectrum 8.5e-08) |
| ode_motoneuron6c | steady: EP LP LP HB HB LP HB LP HB HB; periodic: -; hb_up: -; hb_down: - | agrees, 6 traced (10 points; par 1.4e-09, norm 1.6e-08, period 3.7e-07, spectrum 4.6e-06; curve par 5.8e-06, par2 7.4e-08, norm 0.00015) | agrees, 1 traced (10 points; par 5.4e-06, norm 0.00019, period 0.1, spectrum 1e-05; curve par 2.2e-05, par2 5.1e-07, norm 0.00067) |
| ode_mu26_blocked | steady: EP LP LP HB; cycles: LP; to_snic: UZ UZ UZ UZ; hb_soma: -; hb_dend: -; lp_soma: CP; lpc_soma: -; lpc_dend: - | agrees, 4 traced (8 points; par 1.7e-07, norm 7.3e-05, period 0.0034, spectrum 3e-05; curve par 0.00038, par2 0.001, norm 0.0012) | agrees, 4 traced (6 points; par 1.2e-06, norm 4.1e-05, period 0.00011, spectrum 1.4e-05; curve par 0.034, par2 0.00087, norm 2.1) |
| ode_mu26_control | steady: EP LP LP HB; cycles: LP; to_snic: UZ UZ UZ UZ | agrees, 3 traced (8 points; par 3.2e-08, norm 3.1e-05, period 0.00041, spectrum 0.00053; curve par 0.0009, par2 0, norm 8.6e-05) | agrees, 4 traced (6 points; par 3.5e-06, norm 4.4e-05, period 0.00018, spectrum 0.00019; curve par 4.1, par2 0, norm 1.9) |
| ode_osc | steady: EP | agrees (1 points; par 0, norm 2.7e-11) | agrees (1 points; par 0, norm 4.7e-07) |
| res_cycles | steady: EP HB HB; cycles: TR; torus_down: R1 R4 R3 R2 R3; torus_up: R4 R3 R2 R3 R4 BP | agrees, 6 traced (9 points; par 4.3e-07, par2 3.3e-07, norm 1.9e-07, period 2.2e-11, spectrum 0.0013; curve par 2.3e-06, par2 2.5e-05, norm 1.5e-06) | agrees, 11 traced (4 points; par 4e-08, norm 2.2e-08, period 2.2e-11, spectrum 2.4e-07; curve par 2.8e-06, par2 2.4e-05, norm 7.2e-06) |
| tstauto_ab | steady: EP LP | agrees (2 points; par 3.1e-13, norm 1.1e-09, spectrum 4.2e-08) | agrees (2 points; par 2.2e-08, norm 4.6e-07, spectrum 2.9e-09) |
| tstauto_ab_fold | steady: EP LP; fold: BT CP | agrees, 4 traced (4 points; par 5.7e-08, par2 4.6e-06, norm 8.3e-05, spectrum 0.00044; curve par 4.5e-06, par2 0.00018, norm 2e-05) | agrees, 2 traced (2 points; par 2.2e-08, norm 4.6e-07, spectrum 2.9e-09; curve par 4.3e-05, par2 0.0021, norm 0.00018) |
| tstauto_ab_hopf | steady: EP LP LP HB; hopf_up: -; hopf_down: - | agrees, 3 traced (4 points; par 2.2e-12, norm 3.4e-11, period 3.7e-11, spectrum 2.7e-05; curve par 7.6e-09, par2 4.7e-08, norm 2.3e-08) | agrees (4 points; par 6.3e-06, norm 1.3e-06, period 3e-05, spectrum 0.00012; curve par 1.2e-05, par2 9.1e-05, norm 2.3e-05) |
| tstauto_bigaut | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| tstauto_bistable | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| tstauto_bob | steady: EP | agrees (1 points; par 0, norm 1.8e-12) | agrees (1 points; par 0, norm 1.4e-08) |
| tstauto_brper | steady: EP HB; cycles: - | agrees, 1 traced (1 points; par 0, norm 0) | agrees (2 points; par 2.5e-16, norm 0, period 2e-11, spectrum 1.1e-15) |
| tstauto_dumb | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| tstauto_dumbvp | boundary: EP | agrees (1 points; par 0, norm 0) | not run |
| tstauto_eiz-hom | steady: EP | agrees (1 points; par 0, norm 1.2e-13) | agrees (1 points; par 0, norm 2e-09) |
| tstauto_exp | boundary: EP | agrees (1 points; par 0, norm 0) | not run |
| tstauto_frcp | steady: EP HB; cycles: - | agrees (2 points; par 3.4e-12, norm 1e-13, period 2.1e-12, spectrum 4e-05) | agrees (2 points; par 5.2e-06, norm 4.6e-08, period 2.1e-06, spectrum 0.00093) |
| tstauto_lamom | steady: EP HB; cycles: - | agrees (2 points; par 3.4e-18, norm 0, period 2e-11, spectrum 1e-14) | agrees (2 points; par 2.7e-16, norm 0, period 2e-11, spectrum 1e-14) |
| tstauto_lamom2 | steady: EP HB; cycles: LP | agrees (3 points; par 9.2e-13, norm 6.4e-12, period 2.8e-11, spectrum 1.4e-07) | agrees (3 points; par 6.8e-09, norm 4e-08, period 2.8e-11, spectrum 3.9e-06) |
| tstauto_lecar | steady: EP HB HB; cycles: LP | agrees (4 points; par 4.1e-11, norm 2.2e-10, period 9.3e-08, spectrum 2.2e-06) | agrees, 1 traced (3 points; par 5.4e-05, norm 1.6e-06, period 0.0017, spectrum 0.00041) |
| tstauto_lo | steady: EP HB; cycles: - | agrees (2 points; par 2.7e-18, norm 0, period 2e-11, spectrum 9.8e-15) | agrees (2 points; par 2.5e-16, norm 0, period 2e-11, spectrum 2.5e-16) |
| tstauto_pen | steady: EP | agrees (1 points; par 0, norm 3.6e-12) | agrees (1 points; par 0, norm 4.9e-09) |
| tstauto_pf | steady: EP BP | agrees (2 points; par 2.7e-18, norm 0, spectrum 9.8e-15) | agrees (2 points; par 2.5e-16, norm 0, spectrum 2.2e-16) |
| tstauto_pp2 | steady: EP HB BP; cycles: LP LP | agrees (5 points; par 3.4e-12, norm 7.7e-08, period 0.00011, spectrum 2e-06) | agrees, 3 traced (4 points; par 1.3e-06, norm 0.01, period 15, spectrum 0.0025) |
| tstauto_san | steady: EP BP | agrees (2 points; par 2.4e-12, norm 2.8e-15, spectrum 1.3e-07) | agrees (2 points; par 1.7e-08, norm 4.1e-11, spectrum 1.3e-07) |
| tstauto_she | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| tstauto_simplefold | steady: EP LP LP | agrees (3 points; par 4.8e-12, norm 4.5e-11, spectrum 2.7e-12) | agrees (3 points; par 2.1e-08, norm 4.3e-08, spectrum 8.8e-11) |
| tstauto_torx | steady: EP HB BP; cycles: - | agrees (3 points; par 4.7e-12, norm 0, period 3.3e-12, spectrum 3.9e-07) | agrees (3 points; par 2.2e-08, norm 0, period 3.3e-12, spectrum 3.7e-08) |
| tstauto_torx_homcont | steady: EP BP; homoclinic: EP LP | agrees (4 points; par 6.9e-12, norm 3.6e-08, spectrum 3.2e-07; curve par 6.5e-06, par2 9.8e-06, norm 3.4e-07) | not run |
| tstauto_tsthom | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| tstauto_wcbiwave | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| zero_hopf | steady: EP LP; fold: ZH | agrees (3 points; par 1.7e-18, par2 1.3e-40, norm 5.8e-17, spectrum 7.4e-12; curve par 3.5e-18, par2 2.4e-21, norm 1e-16) | agrees, 1 traced (2 points; par 3.1e-18, norm 3.2e-13, spectrum 2.6e-12; curve par 9.2e-16, par2 1.3e-10, norm 3.7e-12) |

Rows are the output of `autocompare.py --table`. Cells: `agrees` (every untraced comparison inside its tolerance), the
number of points compared and the largest absolute difference per quantity (`par`, `par2` on curves, `norm`,
`period`, `spectrum`: the eigenvalue or multiplier pair furthest apart); `traced` counts explicit difference
entries, either unmatched labels or out-of-bound quantities. It does not assert numerical agreement for those
quantities. Their causes are recorded in each description and below. The second column lists the special points xppautX stores, run by run (EP: the run's
start). Where the code path is shared the
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
  main window fills; the keys of `tools/auto_reference.py` do not drive that. HomCont runs likewise need an integrated
  starting orbit that this driver does not create; their descriptions retain explicit skips.
  XPPAUT's AUTO has no batch mode, so any run needs its window and an X server.
- **Two-parameter curves in XPPAUT 8.0** (W428, [#490](https://github.com/MuhammadMoustafa/xppautX/issues/490)):
  the driver now uses Parameters/Axes, Grab and Run (Two Param for HB/PD/TR, immediate continuation for LP).
  Parameter slots stay fixed across restarts; Grab starts in the named source run. The `.b` header verifies
  the numerical constants, restart label, problem, detection switches and active parameter slots. Cycles
  automatically generate five starting-data steps and restart (`do_auto`,
  [auto_nox.c:653-659](../reference/xppaut-8.0/auto_nox.c#L653)); the driver waits for the final continuation
  and records its curve alone, reusing AUTO-07p's b-file reader with XPPAUT's zero-based column indices.
- **Extend from a point of cycles** (W438, [#500](https://github.com/MuhammadMoustafa/xppautX/issues/500)): a periodic
  run whose start is not an HB grabs it by Tab traversal, as a two-parameter run does, and presses Run then `e`.
  Run on a periodic point opens `periodic_choice` (Extend `e`, Fixed Period `f`;
  [auto_nox.c:1883-1900](../reference/xppaut-8.0/auto_nox.c#L1883), reached from [auto_nox.c:2003](../reference/xppaut-8.0/auto_nox.c#L2003)).
  Every run's labels now feed the Tab order, not only curves'.
  Paired cycle runs reverse from the same generated EP: Grab that EP, set the requested Numerics, Run/Two Param.
  Regenerating starting data for each sign can change its tangent's orientation as well (Hill's two signs
  initially both increased the second parameter); reusing the EP makes the reversal explicit, with each
  final run's DS and IRS checked in its header.
  Missing labels are traced, not silently omitted: XPPAUT sets ISP/ILP to zero on these curves.
  The map flip is continued through its native period-two HB path, which retains IPS -1; the ordinary
  PD menu is not used. Only `henon_rot_lp` remains refused because its Run routine forces IPS 1
  ([finding 66](xppaut-findings.md#66-auto-map-fold-continuation-loses-the-map-problem)).
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
- **A two-parameter curve of a map's fixed points** (an LP, PD or TR): IPS=-1, ISP=1. W404 labels a map's torus
  bifurcation TR and its flip PD, matching AUTO-07p (before W404 these were HB and undetected, respectively).
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
| `henon_rot_tr` (down skipped) | `tools/models/henon_rot.odex` (the hen demo's Henon map beside a rotation) | torus curve of the map: R2 (alpha = 3), R3 (1.25), TTR (1.1), R4 (0), R1 (-1), the exact places of x = -cos(theta) | R2 up agrees; R3, TTR, R4 are detected down; R1 is 3.7e-7 from (-1, 1), where AUTO-07p's R1 and BP lie 1.9e-5 along the fold branch (W404, W405 below) |
| `henon_rot_pd` | the same | flip curve: PTR (0.5139, 3.1198), R2 (3), LPD (0, -1), PTR (0.0195, 1.4136) | both directions agree, including PTR, R2 and LPD (W404) |
| `henon_rot_lp` | the same | fold curve: LTR (-0.0127, -0.7873), LPD (0, -1), R1 (-1, 1) | both directions agree, including LTR, LPD and R1 (W404) |
| `tstauto_torx_homcont` | `examples/tstauto/torx.odex` at the cir demo's parameters | HomCont, three dimensions (one real unstable eigenvalue, a stable pair): the curve in (rn, be) from (-0.721309, 0.6), an LP at (-0.71809, 0.54238) | agrees, the first point and the LP to 2e-9 and 4e-8 (W365 below) |

`hill_cycles` has closed forms: its torus bifurcation is at mu = -c = 0.3 (AUTO-07p 0.3), the resonances of the torus curve
at sqrt(d) = 1/4, 1/3, 1/2, 2/3, 3/4 for q = 0 (the places above move by about 1e-3 for q = 0.1, the Hill equation's
coupling), the lens of the period-doubling curve ends where both multipliers are -1, c = -mu. `henon_rot` has them too:
the torus curve is beta = 1 with cos(theta) = -x, x = -1 + sqrt(1 + alpha); the fold curve alpha = -(1+beta)^2/4, the flip
curve alpha = 3(1+beta)^2/4, and the rotation's signed radius rho is +1 on beta = 2 alpha - 1.2: its crossings are at alpha = 1.1 (TTR),
alpha = -0.0127017 and -0.7872983 (the two LTR), and alpha = 0.0194600 and 0.5138734 (PTR).
The second unit-circle crossing, rho = -1 on beta = 2 alpha - 5.2, gives PTR at alpha = 1.4135678 and 3.1197655.
These lists are alpha values, not (alpha, beta) pairs; AUTO-07p and W404 agree within the stated location tolerances.

How a reference reaches the repository when xppautX cannot run it yet: `NAME.auto07p` holds every run, and a run
skipped in the description (its reason names the card that takes it: W405 or W365) is read but not compared; the runs xppautX does run are compared
(the base runs and, since W404, all map runs except the torus down direction, whose skip W405 traced). The card that makes xppautX run the curve drops the skip.
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
| `res_cycles` | `tools/models/res_cycles.odex`: a Hopf cycle beside a Bogdanov-Takens system (w,v) | closed form | torus curve of cycles `c = -mu - sqrt(b)`: R1, R4, R3, R2 at the exact places below | R1, R4, R3, R2 and the BP agree since W405 ([#467](https://github.com/MuhammadMoustafa/xppautX/issues/467), below) |
| `forced_pp_pd` | `tools/models/forced_predator_prey.odex` | Kuznetsov, Muratori, Rinaldi 1992; MatCont Tutorial V (Kuznetsov, Govaerts, Della Rossa, De Witte) | PD of the period-1 cycle; its closed PD curve with R2 at both ends | the R2 at the published end agrees to 6e-7, the far one to 1.9e-5 in b0 since W405 ([#467](https://github.com/MuhammadMoustafa/xppautX/issues/467), below) |
| `forced_pp_ns` | the same, at b0 = 0.338 | the same | TR of the period-1 cycle; its NS curve to the R2 end, labelled BP; the curve ends at the unforced Hopf point | the curve and its BP agree |
| `lorenz_homoclinic` | `tools/models/lorenz_homoclinic.odex` | Sparrow 1982, Kaplan and Yorke 1979 | HomCont in three dimensions, r = 13.92655741 to 15.6085 in (r, b) | agrees, the whole curve to 6e-8 (W365 below) |
| `explicit_homoclinic` | `tools/models/explicit_homoclinic.odex` | closed form | HomCont in three dimensions, q = 1 to 3.0145 on k = 0 | agrees, and with the closed form to 2e-8 (W365 below) |

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
continues the curve in (r, b) to (15.6085, 3.0927). It does so only with the shorter truncated interval 4.6 and about 200 mesh
intervals: from 5.8 and 35 intervals, with the orbit's far end at 6e-4 from the origin, the first Newton step diverges (singular
pivot). From r = 13.9265 or 13.926 the first point does not converge either. W365 takes 230 intervals (from 200, which reached
(15.5988, 3.0903)): a mesh point every 0.005 time units is a row of the settle's integration with step 0.001, so the two programs'
starting orbits are the same RK4 values (W365 below).

**An explicit homoclinic orbit.** `x' = y`, `y' = q (x - x^2) - k y`, `z' = -2 (z - x^2) + 2 x y`: at k = 0 the orbit
`x = 3/2 sech^2(sqrt(q) t/2)`, `y = x'`, `z = x^2` is homoclinic to the origin (eigenvalues sqrt(q), -sqrt(q), -2) for every q. The loop's
L2 norm over the interval 22 is `sqrt((int x^2 + y^2 + z^2 dt)/22)`, and AUTO-07p's matches it:

| q | AUTO-07p norm | closed form | k |
|---|---|---|---|
| 1 | 0.86489996318 | 0.86489996419 | 0 |
| 3.0145499523 | 0.70294597955 | 0.70294597391 | -1.5e-10 |

Over the 45 points of the curve k stays within 2.3e-10 of 0 and the norm within 1.7e-7 of the closed form (the difference is the
window: the orbit starts at x = 1e-4 and not at the end of the integral). Since W365 xppautX's run agrees: 0.86489994504 at q = 1
and 0.70294598013 at its last point, q = 3.0145499362.

## W400: R1-R4 on curves of cycles ([#462](https://github.com/MuhammadMoustafa/xppautX/issues/462))

xppautX now labels R1, R2, R3 and R4 on the fold, period-doubling and torus curves of cycles, as AUTO-07p does
(periodic.f90 TPSPBV, PVSPBV): the test is the angle of the cycle's multipliers (1:1, 1:2, 1:3, 1:4), found by
the location W356 reimplemented and stored as a typed point. `special_points` switches each kind off.

What agrees with AUTO-07p, within the tolerances of this page: hill_cycles_torus (R4 and R3 to 1.4e-9 in the
second parameter, the BP to 1e-12), hill_cycles_pd (all five R2), forced_pp_ns (the BP to 1e-8) and forced_pp_pd
going down (the published R2, to 6.4e-7). At q = 0 the independently known R4 (9/16) and R3 (4/9) differ by 5.75e-8
and 1.03e-7.

Two comparison problems hid agreement, and both were fixed without widening a bound: a torus curve started with
its tangent oriented by roundoff in the constant first parameter (AUTO-07p uses the first significant component),
and the curve comparison amplified the roundoff of a constant coordinate and could pair a sample with a vertex
on the wrong lap of a closed curve (the lens of hill_cycles_pd). It now matches each sample against each segment's
Newton and chord-error bounds, which serves straight and closed curves alike.

A location that cannot converge no longer ends the run. AUTO-07p gives the point up with a note ("Possible special
point") and goes on; xppautX stopped the whole branch with "no convergence". Only a cancel or a failed exact
Jacobian still stops it (W406 extends this to equilibria, lcspae).

Three runs stay skipped, each with its traced reason in the description:

- res_cycles, both curves: R1 (c = -1) and R2 (c = -0.625) are exactly singular for this model, whose (w,v) block is
  the identity there. Newton reaches EPSU = 1e-7 only if the torus eigenfunction is small. AUTO-07p's start run
  lost its first eight steps to non-convergence and left the eigenfunction's squared norm at 7.6e-7; xppautX's
  converges at once and leaves 2.1e-2 (where AUTO-07p's own start also ends for hill_cycles and forced_pp: 2.06e-2
  and about 1e-3), and its difference-quotient Jacobian then keeps the update near 1e-5. Scaling the
  eigenfunction down to 7e-7, or using the exact Jacobian, finds R1 at AUTO-07p's place to every digit and R2,
  but the scale is not copied: it makes the Hill curves' step sequences differ from AUTO-07p's and loses one of
  hill_cycles_pd's five R2.
- forced_pp_pd, going up: the far R2 is at b0 = 2.57016 against AUTO-07p's 2.57025 (tolerance 2.1e-5). A double
  multiplier -1 makes the test |mu| - 1 as inaccurate as the square root of roundoff; neither program is nearer than
  that, and the curve is steep there.

R1 now has a flow reference (res_cycles) but it is the skipped one (W405 takes the skipped runs); the fold and torus tests otherwise have
synthetic-spectrum coverage (tests/test_auto_codim.cpp).

The shipped lecar_auto.recx snapshot now includes the four resonance booleans and its missing W376 main-window
open flag; its recording commands and all other snapshot data are unchanged.

## W404: AUTO on maps ([#466](https://github.com/MuhammadMoustafa/xppautX/issues/466))

Map fixed points now label flips **PD** and Neimark-Sacker bifurcations **TR**.
Fold, flip and torus curves keep the map equation and detect R1-R4, LPD, LTR,
PTR and TTR as typed labels, stored in sessions. Existing named switches enable
them by default in every front end; their common core reuses W400's resonance
tests, W356's locator and the equilibrium curve/Jacobian owner.

The three descriptions compare **23 points and 92 quantities**, all agreeing
(previously four compared points and thirteen traced HB/TR/undetected-PD differences).
Both fold/flip directions and torus up agree. Updated once: `henon_rot_lp` (150
points: map fold equation and LTR/LPD/R1), `henon_rot_pd` (185: PD start and
PTR/R2/LPD), `henon_rot_tr` (78: PD/TR and R2 up). One and automatic threads
match; every flow diagram and `tests/examples.md5` is unchanged. The generated
table above includes all results, with no reference or tolerance changed.
The recording's snapshot settings and fingerprint were refreshed; its steps are unchanged.

Closed-form curve residuals are at most 3.11e-6 for
`alpha+(1+beta)^2/4`, 3.15e-5 for `alpha-3*(1+beta)^2/4`, and 5.81e-7 for
`beta-1`. Torus R2 is at alpha=2.99999999996; fold R1 differs from (-1,1)
by 1.63e-5 and flip R2 from (3,1) by 9.21e-5. LPD has |alpha|<4.5e-13
and |beta+1|<1.33e-6. These satisfy location accuracy.

**One traced skip: `henon_rot_tr/down`.** R3, TTR and R4 are detected at
alpha=1.24999950, 1.09999929 and 5.31e-7. At the singular double-+1 endpoint,
the zero-angle torus equations also admit the fold branch. Numerical model
Jacobian differences let Newton follow that branch briefly, displacing R1 to
(-1.000268303,1.000268285): 2.68e-4 off (-1,1), failing the existing
8.74e-6 curve bound; AUTO-07p's following BP is absent. Exact model derivatives
reduce R1's displacement to 1.89e-5 and restore curve agreement, but still miss
that BP. This is a real singular-endpoint difference. Its measured skip remains;
independent checks exercise the down labels and switches before that endpoint.

## W407: the torus phase condition ([#469](https://github.com/MuhammadMoustafa/xppautX/issues/469))

The second integral condition of the torus-bifurcation continuation compared the eigenfunction with itself, so it
was zero for every input (XPPAUT's own bug, [finding 67](xppaut-findings.md)). It now compares with the previous
point's eigenfunction, with its Jacobian row, as AUTO-07p's ICTR does. On the references it changed nothing a
comparison can see: the tangent of the continuation already holds the phase between steps, so only the last
digits of two diagrams moved (forced_pp_ns and hill_cycles_torus, rewritten once; both still agree with AUTO-07p,
24 and 32 quantities). tests/test_auto_codim.cpp is the check that it matters: a rotated eigenfunction gives a
nonzero value and its Jacobian row matches a central difference, neither of which the old condition could.

## W405: the skipped resonance runs ([#467](https://github.com/MuhammadMoustafa/xppautX/issues/467))

All four runs stay skipped, each with its reason in the description. What W405 found:

- AUTO-07p's R1, R2 and BP on res_cycles come from its start run, not from a sounder location. Changing only
  the start amplitude of the torus eigenfunction in AUTO-07p's STPNTR (1e-4, periodic.f90:1772) to 3e-4, 1e-3,
  1e-2 or 0.1 makes both torus runs end at their second point; at 1e-4 they find R1, R2 and the BP
  ([AUTO-07p finding 4](auto07p-findings.md)). The points are real (closed form), so the skips remain xppautX's
  own limit.
- A second location try, with the eigenfunction rescaled to a squared norm of 1e-6 when the first fails, did
  not locate them: Newton stops converging about 8e-6 from the point, as before. It was not kept (no
  code is left for it). Rescaling at the restart, tried in W400, finds them but changes the agreeing runs.
- forced_pp_pd's far R2 and henon_rot_tr's R1 are unchanged by W407's phase condition: the first is the
  square-root-of-roundoff location of a double multiplier -1, the second the zero-angle torus equations
  admitting the fold branch at a singular endpoint.

The comparison needed no change: the tolerances are as before.

## W365: HomCont projection ([#427](https://github.com/MuhammadMoustafa/xppautX/issues/427))

HomCont's end conditions are AUTO-07p 0.9.4's (homcont.f90 PRJCTN, EIGHO, BCHO, ICHO), written on our own
numerical helpers (orthes, ortran, hqr3lc, ge, rg): each projection takes exactly NSTAB stable or NUNSTAB unstable
ordered Schur vectors at machine precision (XPPAUT's AUTO split at `compzero`), reversed for a negative interval;
the first call's basis is kept as it is and Beyn's normalisation makes the later ones continuous with it; the boundary
Jacobian's difference quotients use the saved basis without replacing it (xppaut-findings.md, finding 68); the
equilibria's eigenvectors are DGEEV's (unit length, a complex one turned so its largest component is real); the
phase conditions' derivatives are exact. Homoclinic info refuses dimensions that do not add up to the model's.

The three runs W402 and W403 skipped were not held back by the projection: the first pass of that code alone
changed none of their numbers, and with the old projection and the fixes below all three agree too (the projection
moves their diagrams by at most 5e-11, 4.5e-8 and 2e-10 relative). What differed:

- `tools/autocompare.py` started a run from the current state with the parameters the run before it had left: the
  torx HomCont run started at rn = 1.6e-7, the end of its steady run, where AUTO-07p's STPNT starts every run at
  the description's values. A later run from the current state now sets them again.
- A homoclinic start sampled the integrated orbit with an interpolation weight not divided by the step
  (finding 69): the explicit orbit's first L2 norm was 0.8659934 at q = 1, against AUTO-07p's 0.8648999632.
- The first point of a HomCont run is its starting data, which neither program corrects (AUTO-07p CNRLBV), and the
  phase condition carries it along the curve, so both programs must start from the same integration. xppautX's
  interval was its last stored time (36.1 for torx's settle of 36.13 at the model's step 0.05), and Lorenz's orbit
  magnifies a change of integrator (RK4 at 1e-3 and at 1e-4 differ by 7e-3). The three settles now integrate with
  step 0.001, which `tools/auto_reference.py` takes from the description for AUTO-07p's orbit, and Lorenz's mesh has
  230 intervals, a mesh point on every fifth row; Lorenz's reference was regenerated for that mesh (explicit's and
  torx's came out byte for byte the same).
- `autocompare` did not count a HomCont run's first point as its start (EP).

Before: torx's first norm 0.214167 against AUTO-07p's 0.214053 (in the description's order, from rn = 1.6e-7), Lorenz
stopped at its first step, the explicit orbit at its second point. After, against AUTO-07p: torx's first norm to
2.2e-9 (0.21405289126) and its LP to 4e-8, the curve within its tolerances; Lorenz's whole curve to 6.6e-8, ending at
(15.6085, 3.0927); the explicit orbit's to 5.2e-8, and against the closed form 0.86489994504 at q = 1 (0.86489996419)
and 0.70294598013 at q = 3.0145499362 (0.70294597391). The three skips are dropped; tstauto_tstheti_homcont's
diagram moves by 5e-11 relative (the exact phase derivatives and the machine-precision Schur vectors), every other
diagram and the example checksums are unchanged. The comparison: 52 descriptions, 9 skipped, 9982 stored points,
126 checks, zero failures; 118 special points against AUTO-07p (114 before).

## W405 (second attempt): where the torus equations are singular ([#467](https://github.com/MuhammadMoustafa/xppautX/issues/467))

Three of the four skipped runs now run and agree with AUTO-07p; the fourth keeps a traced skip.

- **res_cycles torus_down, R1.** At a torus curve's 1:1 resonance (its angle theta = 0 modulo 2 pi) the pair
  `exp(+-i theta)` meets a cycle's trivial multiplier 1, whose eigenfunction can then be added to both halves of the
  torus eigenfunction: the solution there is not isolated, whatever the model. The location aimed at the zero of the
  angle test, where Newton's correction of the eigenfunction halves each iteration and then hovers at 1e-5 (a hundred
  times EPSU); its retries halved the step down to DSMIN and gave the point up. AUTO-07p reaches it only because its
  start run leaves the eigenfunction 160 times smaller (AUTO-07p finding 4). Now a location whose zero is at the 1:1
  resonance aims half its tolerance short of it and ends on a converged point within that tolerance (lcspbv, lcspae).
  R1 is at c = -0.99999919, b = 0.24999919, 7.9e-7 from AUTO-07p's and 8.1e-7 from the closed form (-1, 1/4).
- **res_cycles torus_up, the BP at b = 0.** The curve reaches the 1:1 resonance again (theta = 2 pi) and turns back;
  AUTO-07p labels it BP (its angle test does not look past pi: finding 5). The branch-point test vanishes there like
  (theta - 2 pi)^3, since the trivial multiplier's block of the torus equations (a rotation by theta minus the identity)
  contributes 4 sin^2(theta/2), and Mueller's method creeps to a triple zero: the location ran out of iterations. Near the
  1:1 resonance that factor is now divided out of the test (fnbpbv), which leaves a simple zero (measured: the divided
  test is -51.8 (theta - 2 pi) at three points 0.05 apart; in torus_down it vanishes like theta^4, with no sign change,
  and neither program labels a BP there). The BP is at c = -0.500000006, 3.7e-8 from AUTO-07p's, b = 0 to 2e-23.
  R2 (theta = pi) needs neither: Newton converges there, and torus_up's R4, R3 and R2 agree.
- **forced_pp_pd pd_up, the far R2.** At a period-doubling curve's R2 the imposed -1 and the next multiplier form a
  Jordan block, which roundoff splits into a conjugate pair. AUTO-07p's test then finds no imposed multiplier and
  returns 0, which its location accepts as the zero (finding 6); xppautX did the same at another location step
  (b0 = 2.5701605 against AUTO-07p's 2.5702457). Now the split pair stands for the imposed multiplier and its modulus
  is tested, and the location goes on to |mu| = 1: b0 = 2.5702266, 1.9e-5 from AUTO-07p's (tolerance 2.1e-5). Real
  multipliers are placed as before, so hill_cycles_pd's five R2 and pd_down's do not move.
- **henon_rot_tr down** stays skipped, its reason rewritten: R1 is now 3.7e-7 from (-1, 1) (2.68e-4 before), but
  AUTO-07p's R1 and the BP it labels next lie 1.9e-5 along the fold branch, which its step entered there; along the
  torus branch the branch-point test keeps its sign, so xppautX labels no BP.
- **The comparison.** torus_down's curve agrees, but AUTO-07p's ends 0.002 further in b: both runs take NMX = 130
  steps, and xppautX's are 0.1% shorter, its eigenfunction (squared norm 2.1e-2 against 7.6e-7) counting in the
  arclength. As for a parameter bound, when both runs end at NMX the samples beyond the other's last point are no
  longer compared (tools/autocompare.py, tested in tools/test_autocompare.py with a run that stops before NMX and a curve
  that leaves the other, which still differ). In four other descriptions' rows of the table a trailing sample no
  longer compared lowers the largest curve difference (hill_cycles_torus par2 from 2.7e-6 to 1.9e-8); none changed agreement.

Tried and not kept: retrying a failed location step below DSMIN also finds torus_down's R1, but it acts only after
Newton fails, and on the map Newton does not fail, it converges onto the fold branch. More Newton iterations do not
help: at the singular point the corrections stay at the roundoff level. Testing the pair's sum (accurate at a Jordan
block) instead of |mu| - 1 locates R2 better still, but at the true R2 the computed pair is split by 5e-4 and
hill_cycles_pd's R2 spectra would no longer agree. Dividing the trivial factor out of the branch-point test at every
angle, not only near the 1:1 resonance, moved forced_pp_ns's BP and its later steps.

Diagrams rewritten: res_cycles (its two torus runs added) and forced_pp_pd (pd_up added; pd_down's numbers are byte for
byte the same, its branch and labels renumbered). Every other diagram and tests/examples.md5 are unchanged. The
comparison: 52 descriptions, 9 skipped, 10998 stored points, 126 checks, zero failures; 124 special points against
AUTO-07p (118 before).

## W415: two research neuron models ([#477](https://github.com/MuhammadMoustafa/xppautX/issues/477))

Two of the maintainer's published models join `examples/ode` (each as the `.ode` with a header naming its source and its
converted `.odex`, which `tools/odexcheck.sh` runs beside it) and three descriptions of `tests/auto`, each with a
baseline and AUTO-07p's reference made by `tools/auto_reference.py`:

| Description | Model | Runs | AUTO-07p | Points, quantities |
|---|---|---|---|---|
| ode_mu26_control | steady: EP LP LP HB; cycles: LP; to_snic: - | agrees (5 points; par 1.2e-07, norm 3.9e-09, period 1.4e-08, spectrum 1.1e-06) | agrees (5 points; par 3.5e-06, norm 4.3e-06, period 8e-08, spectrum 1.3e-07) |
| ode_mu26_blocked | steady: EP LP LP HB; cycles: LP; to_snic: -; hb_soma: -; hb_dend: -; lp_soma: CP; lpc_soma: -; lpc_dend: - | agrees (6 points; par 8.2e-08, par2 2.4e-08, norm 5.9e-07, period 1.3e-08, spectrum 1.1e-06; curve par 0.00017, par2 1.1e-07, norm 5.7e-09) | agrees, 1 traced (5 points; par 1.2e-06, norm 1e-05, period 7e-08, spectrum 1e-07; curve par 0.024, par2 0.00087, norm 0.00021) |
| ode_ma23 | `ma23`: the fast-spiking PV interneuron of Ma et al. 2023, 5 states | steady (LP 35.18, LP 34.21, HB 539.22 pA), cycles from the HB (stable cycles down to period 124 ms) | agrees | 4 points, 12 quantities |

The largest differences from AUTO-07p are: Mu26 control, Hopf point 5.1e-8 pA, folds 5e-10 pA, cycles fold 2.3e-11 pA
and period 1.8e-9; Mu26 blocked, Hopf 1.8e-8 pA, the Hopf loci within 1.9e-4 in g_h (tolerances 0.01 and 0.04), 1.6e-7 in
current and 6e-9 in norm of the other program's polyline, the fold locus within 1e-6, 2.9e-6 and 4.4e-6 in its three
coordinates, the cusp 5.2e-9 in current and 8.6e-9 in g_h; Ma23, Hopf 2.8e-6 pA (tolerance 1.7e-4). No tolerance was widened.

**Integration.** `mu26.ode` (backward Euler, DT 0.05, 400 ms, every 10th step stored, 5 pA) and `ma23.ode` (backward
Euler, DT 0.01, 1000 ms, every 20th step, 200 pA) give XPPAUT 8.0's `-silent` output digit for digit, 801 rows of 29
columns and 5001 rows of 6 (XPPAUT 8.0 built from reference/xppaut-8.0 as the five-model screen does: GCC 15,
`-ffp-contract=off`); `tests/examples.md5` holds both.

**One model for two conditions.** Control and blocked differ in two conductances only, so `mu26` has them as parameters
2 and 3 (the order of the source's AUTO-07p files, where `run_2d_bifurcation.auto` continues them) and the two
descriptions set them. Two models would copy 300 lines. The `.ode` runs at the control values; the description sets
`SmIapp` 0 and, for control, the converged control rest from the source's `stpnt` (the `.ode`'s own initial state is the
blocked rest, which is the blocked description's start). The source's `NMAX=10000000` is 1000 in the example (W432 lifted xppautX's
refusal of an NMAX above 1000000; a description sets its own).

**Source parameters against the public version.** `parameters.json` of
[pvn-hcn-marmoset-model](https://github.com/MuhammadMoustafa/pvn-hcn-marmoset-model) maps to 119 parameters of the
`.ode` (soma and dendrite). 64 are identical to every digit (the h conductances `0.05196355494318059` and
`0.6435532020105316` among them); 55 differ by the export's rounding to four to six significant digits, relative
3.6e-3 at most (g_m dendrite 0.0039 against 0.003885907, g_m soma 0.0143 against 0.01433809, g_SK soma 0.0232 against
0.02324651, g_Ka dendrite 0.0459 against 0.04594782) and below 8e-4 for the other 51. The reversal potentials E_Ca 130,
E_K -90 and E_Na 50 are identical; E_h -30.001, E_L -74.9596 (soma) and -74.9825 (dendrite) are rounded the same way.
The model was not changed to hide this. Only parameters were compared, not the Python equations.

**Ma23 against its paper and its AUTO-07p export.** `models/main.ode` of the maintainer's parvocell_model agrees with
Table 1 and the equations of Ma et al. 2023 (docs/ma2023.md there) except E_Ca: the table lists 68 mV, the export
130 mV. The example used the export's 130 until W424 ([#486](https://github.com/MuhammadMoustafa/xppautX/issues/486)),
and Table 1's 68 since. `models/Ma2_test_auto07p_export/ma2_test.f90` is not the same
model: C_m 12 (30), E_leak -58 (-50), E_K -90 (-80), E_Ca 120 (68 in the paper, 130 in the export), E_Na 50 (58), a
different start state and a floor on the calcium of the SK term. The reference is therefore made from the example's
own equations (`tools/auto_reference.py` translates the `.odex`), not from that file.

**Disagreements, traced.**

- *Hopf point of Mu26 control.* With DSMAX 0.5 xppautX locates it at 13.2656035 pA, AUTO-07p at 13.2654734, a
  difference of 1.3e-4 against a tolerance of 3.3e-5. At DSMAX 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 and 1.0 xppautX gives
  13.265539, 13.265527, 13.265604, 13.2654734, 13.265533, 13.26547355 and 13.26547351; AUTO-07p gives 13.265473491,
  13.265473468, 13.265473448, 13.265473564, 13.265473568, 13.2654736 and 13.265473353. The real part of the Hopf pair
  moves 6e-4 per pA, and xppautX's located point had a Hopf function of -7.9e-8; its location iterations
  (diagnostics file, IID 2) alternate in sign at steps of 1e-4 to 1e-5 while the function at one and the same
  parameter value takes values from +5e-8 to -8e-8. Cause: the test functions are evaluated at the Jacobian of the last
  Newton iterate, as XPPAUT's AUTO2000 does; AUTO-07p's SOLVAE recomputes it at the converged point first
  ([xppaut-findings.md 70](xppaut-findings.md#70-auto-evaluates-the-test-functions-at-the-last-newton-iterates-jacobian)).
  The description uses DSMAX 0.8, where xppautX agrees (5e-8 pA); changing the call changes every AUTO baseline's last
  digits, so it is a card of its own (proposed in W415's report). W421 made it: within 9.8e-8 pA at every DSMAX
  ("W421" below).
- *The second fold of Mu26 at DSMAX 1.* xppautX labels a BP at -78.744 pA and the fold at -79.0566 pA is not labelled;
  AUTO-07p labels neither. Both label the fold from DSMAX 0.8 down. Traced in W423: the BP was the fold test's
  determinant read as the branch point test's, and xppautX labels neither since.
- *The cusp of Mu26's rheobase-fold locus.* xppautX labels CP at 1.08110512 pA at DSMAX 1, 0.5, 0.25 and 0.1;
  AUTO-07p at 0.5 and 0.25 (1.08110512), not at 1 and 0.1. The description uses 0.5. Traced in W423: at 1
  AUTO-07p's location gives up, at 0.1 neither program reaches the cusp within the 45 points.
- *The folds of cycles in g_h* (`lpc_soma`, `lpc_dend` of `run_2d_bifurcation.auto`) are not in the descriptions.
  With the script's RL0=0 AUTO-07p's own run stops at its first step, which leaves g_h negative (one sample of the
  curve; the source's saved `b.lpc_soma` has four points ending at g_h -4e-4), and past it xppautX did not finish
  12 points of the curve within the driver's 120 s safety limit. W423 adds both, skipped with the traced cost.
- *XPPAUT 8.0.* Ma23: with the export's E_Ca 130 it labelled no Hopf point, so its cycles run could not start from
  one (finding 70); with Table 1's 68 (W424) it labels the HB at 539.2246 pA, and its reference agrees. Mu26:
  the XPPAUT 8.0 reference for the control description is made (W422, [#484](https://github.com/MuhammadMoustafa/xppautX/issues/484): the
  driver now waits for the run's own end, a later EP or MX row, not the start's EP row): LP 0.0058127 at point 9, LP -79.05658 at
  127, HB 13.26547 at 244, and for the cycles LP 99.99655 at 118, the points of ours and AUTO-07p's. The blocked
  description now has its XPPAUT reference too (W428, [#490](https://github.com/MuhammadMoustafa/xppautX/issues/490)),
  including both HB loci and the rheobase-LP locus through the cusp. XPPAUT sets ISP/ILP 0 on these curves:
  its missing CP label is traced explicitly, while the three curves are compared.

**Not run.** The periodic branches of Mu26 to the rheobase fold (`run_bifurcation_*_extend.auto`: 16000 + 30000 steps
at 50 and 150 mesh intervals, the period diverging to 25 s), and the extent of the cycles beyond their fold. W423
adds them (`to_snic`).

The follow-ups are cards: the test functions at the converged Jacobian (finding 70) are W421
([#483](https://github.com/MuhammadMoustafa/xppautX/issues/483)); the untraced DSMAX-1 BP and CP, the LPC loci and the
branches to the SNIC are W423 ([#485](https://github.com/MuhammadMoustafa/xppautX/issues/485)).

## W421: the test functions at the converged point's Jacobian ([#483](https://github.com/MuhammadMoustafa/xppautX/issues/483))

`solvae` now recomputes the Jacobian at the converged point, as AUTO-07p's SOLVAE does ("Recompute Jacobian for test
functions", ae.f90), and every equilibrium and map test reads that one: the fold test (it computed its own, at the
same point, so a run with fold detection costs no more), the Hopf test and the eigenvalues stored with the point (they
read the last Newton iterate's, [finding 70](xppaut-findings.md#70-auto-evaluates-the-test-functions-at-the-last-newton-iterates-jacobian)),
the codimension-two tests of fold, Hopf and map curves (BT, CP, GH, RN computed their own) and `stbif`'s branch
direction. The Hopf test takes its eigenvalues from a copy, as AUTO-07p's PVLSAE does: EISPACK overwrote the matrix
in place, which `stbif` read after a branch point's location (it runs after every test, W356) and GH after the HB
test. Only automatic branch switching (MXBF other than 0) uses `stbif`'s direction; no description sets it.
Curves of cycles and boundary-value problems already matched: AUTO-07p's NEWTONBV also takes the fold function from
a solve with the last iterate's factorization and the multipliers from that iterate's P0 and P1, as `stepbv` does.

**The Hopf point against DSMAX** (the steady runs of `ode_mu26_control` and `ode_ma23`, NMAX raised where the run
did not reach the point):

| DSMAX | 0.3 | 0.4 | 0.5 | 0.6 | 0.7 | 0.8 | 1.0 |
|---|---|---|---|---|---|---|---|
| Mu26, before | 13.265539107 | 13.265526825 | 13.265603538 | 13.265473414 | 13.265532821 | 13.265473550 | 13.265473513 |
| Mu26, W421 | 13.265473526 | 13.265473439 | 13.265473469 | 13.265473487 | 13.265473516 | 13.265473484 | 13.265473537 |

| DSMAX | 0.5 | 1.0 | 1.5 | 2.0 | 3.0 |
|---|---|---|---|---|---|
| Ma23 (E_Ca 130), before | 553.876745993 | 553.876753998 | 553.876751804 | 553.876748995 | 553.876729314 |
| Ma23 (E_Ca 130), W421 | 553.876729713 | 553.876728607 | 553.876726552 | 553.876730607 | 553.876727617 |

Mu26's spread falls from 1.3e-4 to 9.8e-8 pA (AUTO-07p's 2.2e-7, W415), Ma23's from 2.5e-5 to 4.1e-6 pA (AUTO-07p
553.87672736 at DSMAX 2), within the location's own tolerance (EPSS relative to the arclength step).

**Baselines.** 16 descriptions change, each at the points the change reaches: the located HB of `izh_class2` and
`izh_resonator` (its period, spectrum and stability sign; the parameter is the same), of `tstauto_frcp` and of
`ode_lecar`, `tstauto_lecar`, `ode_ma23`, `ode_mu26_control` and `ode_mu26_blocked` (the location moves, by up to
1.2e-5 in lecar's current, and the cycles started there follow); the BP point's spectrum of `tstauto_pp2` and
`tstauto_san` (its last digits); the two-parameter curves of `tstauto_ab_fold` (the fold curve's zero eigenvalue at its
second point was -2.9e-4, the last iterate's, and is -1.4e-7), `tstauto_ab_hopf`, `zero_hopf` and the Mu26 loci; and
the map curves of `henon_rot_lp`, `_pd` and `_tr`. The comparison's totals do not change (15188 points, 144 checks).

**Agreement with AUTO-07p**, the largest difference over tolerance in each changed description, before and after:
henon_rot_lp 0.097, 0.049; henon_rot_pd 0.137, 0.065; henon_rot_tr 0.144, 0.013; izh_class2 0.00114, 0.00108;
izh_resonator 0.0030, 0.00048; ode_lecar 0.296, 0.00082; ode_ma23 0.282, 0.046 (at the export's E_Ca 130; W424's E_Ca 68 came first at the merge, and its baseline, rewritten
then, is at 0.044); ode_mu26_blocked 0.104, 0.038;
ode_mu26_control 0.0229, 0.0229; tstauto_ab_fold 0.406, 0.343; tstauto_ab_hopf 0.043, 0.0026; tstauto_frcp 0.024,
0.0010; tstauto_lecar 0.296, 0.00082; tstauto_pp2 3.45e-4, 3.45e-4; tstauto_san 3.7e-5, 3.7e-5; zero_hopf 5.1e-6,
8.6e-9. Of the 534 quantities compared, 33 move further, the most `tstauto_ab_fold`'s CP spectrum (0.113 to 0.224 of
its tolerance) while that point's parameter comes 400 times closer (1.3e-6 to 3.2e-9). Against XPPAUT 8.0, which
evaluates at the last iterate, Morris-Lecar's HB spectrum moves from 0.12 to 0.29 of its tolerance and frcp's from
0.011 to 0.024; both still agree. No tolerance was widened.

**Cost**, `tools/auto-profile/profile.py` (callgrind) and `tables.py`, the steady phases: ModelDB 2.03 model Jacobians
per stored point before and after (then `solvae` 1.0 and the fold test 1.0, now `solvae` 2.0), 3.844 and 3.846 M
instructions per point; PY_S1Bf 2.01 and 2.01, 0.378 and 0.378 M. Their periodic phases do not change. A run without
fold detection (ILP 0) builds one Jacobian more per point; a two-parameter curve with BT, CP or GH detection builds
fewer.

## W431: ModelDB 267710, the six-compartment motoneuron ([#493](https://github.com/MuhammadMoustafa/xppautX/issues/493))

`examples/ode/motoneuron6c.ode` (and its `.odex`) is ModelDB 267710 (Moustafa et al. 2023, archive commit
2dd9ba299e56c3f2962f883afaada5a899c4e3f6, `6cModel Xppaut/rest.ode` with its 19 include files inlined in the source's order):
25 states, 254 fixed quantities. Staging edits: `@ OUTPUT=output.dat`, `@ total=600`, and `CaDen_Max` (glob.inc, 0.000168, the
dendritic L-type Ca / PIC density) a `par`. Its `output.dat` is the original's, digit for digit (md5 0a075c6a7d80...).
`tests/auto/ode_motoneuron6c.json` runs, with opt.inc's settings (NTST 100, NPR 100000, NMAX 10000000, DSMAX 0.1, ParMin -10, ParMax 1):

| Run | What | xppautX and AUTO-07p |
|---|---|---|
| steady | ampPar from the model integrated 1200 ms (the README's Go and Last) | 1081 points: LP 0.1743, 0.1735, 0.2435, -0.00378; HB 0.17376, 0.23105, 0.19427, 0.04117, 0.57279; EP 1.0074; the benchmark's (paper/benchmarks/modeldb-267710) points in all three programs |
| periodic | from the HB at 0.57279, Ds -0.02, 20 points | the fold of cycles at 0.57278 |
| hb_up, hb_down | the HB locus in (ampPar, CaDen_Max) from the same HB, CaDen_Max from 2x nominal (ampPar 0.44) down to 0 (ampPar 0.82) | 138 points together, no codimension-two point; the curves agree (par 5.8e-6, par2 7.4e-8, norm 1.5e-4) |

The first parameter of the locus is ampPar, so that both programs orient the curve by it: AUTO-07p takes the first significant
component of its tangent, and CaDen_Max (1e-4) is not significant next to ampPar (0.57), so with CaDen_Max first the same
sign of Ds ran the two programs in opposite directions. Both directions end at ParMin/ParMax of ampPar, past which the
step overshoots (CaDen_Max -1.4e-5 at the end of `hb_up`).

**Traced: five GH on the locus, AUTO-07p only.** AUTO-07p labels five GH between ampPar 0.739 and 0.795; xppautX labels none.
The cause is `tstauto_ab_hopf`'s ([finding 2](auto07p-findings.md): DERIV2a omits the subtraction of 2*F(u), so the Newton
residual enters magnified by h^-2). With EPSL = EPSU = EPSS 1e-7 or 1e-9 AUTO-07p labels none on the same curve. Checked against an independent first
Lyapunov coefficient at W456 (below): positive along the whole curve. Five entries in the description's `differences`.

**XPPAUT 8.0 agrees** on all 11 special points and both loci, through W428's two-parameter driver
([#490](https://github.com/MuhammadMoustafa/xppautX/issues/490)), made at review once W428 had merged; like xppautX it labels no GH
on the locus (its two-parameter detection is off, docs/xppaut-findings.md finding 71).

**What `tools/auto_reference.py` learned for this model.** `if C then A else B` in a user function (ModelDB's vtrap and expmod) is
written as Fortran's `MERGE(A, B, C)` and as XPPAUT's `if(C)then(A)else(B)`; a quantity read before its definition (conn.inc's five
conductances, which XPPAUT reads ahead as constants) is emitted after the quantities it reads, where the Fortran local was garbage.

**W432** (#494): the model's `@ NMAX=10000000` loads. NMAX bounds one branch's loop, not an allocation; the bound is the diagram's
stored points (1,000,000, what a saved session keeps), a shown error that keeps the points.

## W419: the resonance test modulo 2 pi, and tests that do not apply ([#481](https://github.com/MuhammadMoustafa/xppautX/issues/481))

**A deliberate difference from AUTO-07p.** A torus curve continues its angle theta unwrapped, and AUTO-07p's
resonance test `theta (theta - pi/2) (theta - 2pi/3) (theta - pi)` sees 0 to pi only
([AUTO-07p finding 5](auto07p-findings.md#5-a-torus-curves-resonance-test-is-blind-once-its-angle-leaves-0-to-pi)).
xppautX's (`resonance_test`, core/auto_codim.cpp, for cycles and maps) is that polynomial of theta modulo 2 pi folded
into 0 to pi, its sign flipped each time the fold turns at a resonance it tests (0 for 1:1, pi for 1:2), so it changes
sign at every resonance of the circle; from 0 to pi it is AUTO-07p's value bit for bit, and the kind it labels is the
folded angle's nearest resonance. A curve that stays in 0 to pi is unchanged. New labels, each at its closed form's
place (`autocheck codim`, "torus past pi") and each a traced difference against AUTO-07p:

- `res_cycles` torus_up: R3 at c = -0.555556 (theta = 4 pi/3, w = 1/18) and R4 at c = -0.531250 (3 pi/2, w = 1/32);
  at b = 0 (theta = 2 pi) the BP keeps the label of the coincident R1. torus_down, past theta = 0: R4, R3, R2, R3 at
  c = -1.28125, -1.38889, -1.62500, -1.88889 (w = 25/32, 8/9, 9/8, 25/18).
- `hill_cycles_torus` torus_down: after the BP near the 1:2 endpoint the curve comes back with theta past pi and
  meets the 1:3 point again, R3 at d = 0.4463560 (the first at 0.4463561).
- `henon_rot_th` (new): the rotation's torus curve of the Henon-with-rotation map, continued in th and beta from its
  torus point at beta = 1.8, where the continued angle is th itself: R4, R3, R2 agree with AUTO-07p (pi/2, 2 pi/3, pi,
  and the BP AUTO-07p also labels next to R2), and R3 at 4 pi/3 and R4 at 3 pi/2 are the traced differences. XPPAUT
  8.0 follows the same curve (`.xppaut`, W428's driver) and labels none of the six: its map torus curve runs with ISP=0
  and ILP=0 ([finding 71](xppaut-findings.md#71-auto-two-parameter-map-problems-and-disabled-detection)), traced as for henon_rot_tr. res_cycles' and hill_cycles_torus' new
  labels are traced against XPPAUT 8.0 the same way.

**The 1:2 resonance is singular too.** At theta = pi, as at 0, sin(theta) = 0 and the equations of the pair's real and
imaginary parts decouple, so the torus solution is not isolated. W405 found Newton converging at R2 and left it; that
was the iterates' luck. With the new test (its values past pi are the reflected polynomial's), res_cycles torus_up's
R2 location reached theta = 3.1415820, its next solve failed and the point was given up; the LAPACK build already
lost it so before this card (W426). A location whose zero is nearest a 1:1 or 1:2 resonance now aims half its tolerance
short of it (`auto_torus_singular`, lcspae and lcspbv), as W405's did at 1:1 alone.

**A test that does not apply** returned 0, which a location takes as the zero it seeks if an iterate falls where the
test does not apply (W405 fixed one case, finding 6). Every such 0 is now `auto_test_not_applicable` (core/auto_c.h,
a NaN, never a number), and a location neither starts from it (`location_sign_change`) nor stops on it: an iterate
that meets it gives the point up, as a solve that does not converge does. The tests that return it: `fnhbae` (no
complex pair; a map with no eigenvalue past the imposed ones; a curve of equilibria that is not a codimension-two
curve, or with ZH detection off or at a BT), `fnghae` (no pair, or at a BT), `fnrnae` and `fnspbv`'s torus test
(every resonance kind off), `fnbpbv` (a singular P1), `fnspbv` (a planar orbit, an inaccurate trivial multiplier, the
second multiplier real and positive, ISP other than 2) and `auto_cycle_test` (a kind off, a curve that is not a fold
or flip curve, no multiplier near the imposed one). The fold, branch-point (equilibria), BT, CP and user-zero tests
always apply. HomCont's test functions (`psiho`) are parameters of the continued system, located as user zeros, and
are not test functions of this kind. No diagram changed through this part.

**Baselines rewritten**, each by a change above: `res_cycles` (the new labels; R1 in torus_down at c = -0.99999966,
from -0.99999919, the closed form -1, its location's iterates now on the reflected test below 0; R2 in torus_up at
-0.62500045, from -0.62500002, the singular step); `hill_cycles_torus` (the new R3; the BP at the 1:2 endpoint at
d = 0.27700597075, the singular step); `henon_rot_tr` (R2 at alpha = 2.99999999806, from 2.99999999880, the singular
step and the reflected test past pi); `forced_pp_ns` (the BP where the torus curve meets the period-doubling curve's
R2, eps = 0.59665693572 from 0.59665694778, 4.1e-10 from AUTO-07p's where it was 1.2e-8; the run ends one point
later). Every other diagram is unchanged, and tests/examples.md5 too (no integrator code changed).

Tried and not kept: a product of trigonometric factors (`sin(theta/2) cos(theta/2) cos(theta) (cos(theta) + 1/2)`)
sees the circle as well but changes every torus location's last bits from 0 to pi; returning `std::optional` from the
twelve test functions says "does not apply" in the type, but changes their shared signature, every stored test value
and Mueller's arguments, where one named value checked in two places does the same.

## W426: the LAPACK build and res_cycles' 1:2 resonance ([#488](https://github.com/MuhammadMoustafa/xppautX/issues/488))

With `LINALG=lapack`, res_cycles torus_up's point 39 (285 of the whole diagram) was a regular point at c = -0.6217
where the default build and AUTO-07p locate R2 at -0.625. Traced on master `bde36b9e` in both builds:

- **Not the test.** The resonance test at points 37 and 38 is -0.0396748370 and 0.2339037361 in both builds (they
  differ from the 12th digit on): it changes sign in both, and both start the location with the same secant step,
  whose iterate has the test -0.0030763843 at theta = 3.1409967.
- **The location's solve.** Mueller's next step aims 6.0e-4 further. EISPACK's solve there converges, at theta =
  3.14159243 (2.3e-7 from pi), and the location ends on R2. LAPACK's does not: `stepbv` halves the step to 3.0e-4,
  and every later iterate's again, until theta = 3.1415740 (1.9e-5 from pi), where no step converges and the location
  gives the point up (NOTE: Possible special point). The torus equations are singular at the 1:2 resonance (W419); which
  step still converges that close to it is decided by the last bits every earlier point carries, and those differ
  between the two libraries from the first located point on, through the multipliers its location's test reads.
  AUTO-07p's location is the same Mueller iteration with no handling of the singular point: its R2 converges, as
  EISPACK's did, by the same luck.

The location, not the test, decided it, and W419 makes it independent of the library here: a location whose zero is
nearest a 1:2 resonance aims half its tolerance short of it, as one at 1:1 did since W405. After W419,
`tools/auto-profile/diagram_diff.py build/lapack/xppautX` gives **0 AUTO-07p differences** over the 51 runnable
descriptions (master's LAPACK build: 1, this one); res_cycles' largest difference is 0.48 of its tolerance, and
forced_pp_ns's LAPACK diagram now has the default build's 264 points (it differed by 0.34 in the parameter after its
BP at the 1:2 endpoint, where the same singular solve decided a step). The largest fraction, 0.908, is forced_pp_pd's
far R2 (W405: 1.9e-5 against a tolerance of 2.1e-5), the same in master's LAPACK build. EISPACK stays the default.

## W423: Mu26's remaining runs ([#485](https://github.com/MuhammadMoustafa/xppautX/issues/485))

**The branches to the SNIC.** Both Mu26 descriptions gain `to_snic`: the cycles extended (AUTO's Extend, which the
driver now presses for a periodic run from a stored point that is not an HB) from the cycles run's end, past their fold,
down to the rheobase fold, 170 points at NTST 50 and DSMAX 1. The source's `run_bifurcation_*_extend.auto` needed
16000 + 30000 steps at DSMAX 0.1 and NTST 50, then 150; at DSMAX 1 both programs get there with 50 mesh intervals.
The run ends, in xppautX and AUTO-07p alike, within 2e-5 pA of the fold of equilibria with the period in the tens of
seconds:

| Description | Rheobase fold (LP) | xppautX's end | AUTO-07p's end | T * sqrt(I - I_LP), ours / AUTO-07p |
|---|---|---|---|---|
| ode_mu26_control | steady: EP LP LP HB; cycles: LP; to_snic: - | agrees (5 points; par 1.2e-07, norm 3.9e-09, period 1.4e-08, spectrum 1.1e-06) | agrees (5 points; par 3.5e-06, norm 4.3e-06, period 8e-08, spectrum 1.3e-07) |
| ode_mu26_blocked | steady: EP LP LP HB; cycles: LP; to_snic: -; hb_soma: -; hb_dend: -; lp_soma: CP; lpc_soma: -; lpc_dend: - | agrees (6 points; par 8.2e-08, par2 2.4e-08, norm 5.9e-07, period 1.3e-08, spectrum 1.1e-06; curve par 0.00017, par2 1.1e-07, norm 5.7e-09) | agrees, 1 traced (5 points; par 1.2e-06, norm 1e-05, period 7e-08, spectrum 1e-07; curve par 0.024, par2 0.00087, norm 0.00021) |

The last column is the SNIC's law, period proportional to (I - I_LP)^(-1/2): the two programs' ends, at different
steps, lie on one curve (the control's 3% is the fold's last digit). Run on, both stop with MX near their 310th point
(periods of 1e5 to 1e6 ms). The branch has no special point between the cycles' fold and the SNIC, so the
reference comparison adds no quantity; the baseline holds every point. XPPAUT 8.0's control reference reaches the same end (W438, [#500](https://github.com/MuhammadMoustafa/xppautX/issues/500)):
the cycles' EP grabbed by Tab, then Run and `e` (Extend), 170 points, ending at 0.0058180 pA with period 36225 ms.
The blocked one ends at 0.0658090 pA with period 45125 ms (T * sqrt(I - I_LP) 142.9, against 143.1 and 143.4 above); the
driver leaves a run the description skips (`lpc_soma`, `lpc_dend`) unavailable with the skip's reason.
The run costs about 70 s at one thread per description (the whole comparison went from 144 to 291 s at one thread,
65 to 112 s at automatic threads).

**The DSMAX-1 BP on the steady branch**
([xppaut-findings.md 72](xppaut-findings.md#72-autos-branch-point-test-reads-the-fold-tests-determinant)). The step
past the second fold converges on the branch beyond it; the fold function keeps its sign, and the branch point test
read the determinant the fold test had just computed with the tangent row, which changes sign there. AUTO-07p's
branch point test reads the Newton matrix's determinant, which does not. `fnlpae` no longer stores its determinant:
xppautX now labels neither point at DSMAX 1, as AUTO-07p. The fold is missed by both, a step size matter (both label
it from DSMAX 0.8 down). Two baselines change: `tstauto_pp2` (its BP's parameter by 2.8e-13, the points after it by
up to 5.2e-10) and `tstauto_torx_homcont` (its BP's states, about 2.8e-56, by 7.6e-60); both agree with every
reference as before, and no other diagram changes.

**The cusp at DSMAX 1 and 0.1** ([auto07p-findings.md 2](auto07p-findings.md#2-a-newton-residual-contaminates-cp-and-gh-test-functions)).
At DSMAX 1 AUTO-07p's cusp function changes sign between points 28 and 29, but next to its root it jumps by about
3e-10 from one location iterate to the next, more than it changes along the curve: Mueller's method uses up ITMX and
the point is not labelled. xppautX's cusp function (without the Newton residual's term) is located at 1.0811051 pA.
At DSMAX 0.1 neither program reaches the cusp within the 45 points: both end at 0.41587 pA, g_h -3.798.

**The folds of cycles in g_h** (`lpc_soma`, `lpc_dend`, from the cycles' LP, DSMAX 0.5, 40 points to g_h 16, Par Min
-0.001 so that the start, at g_h -2e-7, is inside it). They are in the blocked description, skipped, with AUTO-07p's
reference (both curves to g_h 15.94, no R1). Measured on `lpc_soma` at NTST 50 and one thread: AUTO-07p takes 0.9 s a
point (20 points in 17.9 s, after 6.6 s for its starting data), xppautX about 14 s (8 points, 5 of them its starting
data, in 115 s), with the same 1 or 2 Newton steps a point. The cost is in the Newton step, and the backtraces of a
callgrind run are all the model's evaluation (`evaluate_pure`) under `fnpl` -> `ffpl` -> `funi`:
- `fnpl` differences all 2 x 28 columns of the fold system and its 4 parameters (`auto_jacobian::problem`), 117
  evaluations of `ffpl`, each of which differences the model's Jacobian (61 evaluations): about 7100 model
  evaluations per collocation point. AUTO-07p's FNPLF differences only the 28 state columns and takes the eigenvector
  block from the Jacobian it has ([periodic.f90:427-507](../reference/auto-07p/src/periodic.f90#L427)): about 3500.
- A model evaluation costs more: the cycles run, the same evaluations in both, takes 48 s against AUTO-07p's 19 s.
- Those two give about 5.5 of the 15; the rest is not traced (xppautX's starting data are five continuation steps in
  the eigenvector's norm, AUTO2000's start, against AUTO-07p's one solve).
The fix (FNPLF's structure, which changes every fold-of-cycles curve's last digits) is W436 ([#498](https://github.com/MuhammadMoustafa/xppautX/issues/498)).

**Found, not fixed: a user point (UZ) on these cycles.** To compare the period at given currents, Mark values
(`marks`, AUTO's UZR) at 5, 1, 0.1 and 0.07 pA were tried on the branch to the SNIC (not committed). AUTO-07p labels
all four. xppautX labels 5, misses 1 (its location uses up ITMX, `NOTE:Possible special point`), labels 0.1 at
0.10018 pA, and then continues the branch backwards: it comes back up through 1 and 5 pA, and in the control
condition past the cycles' fold again. Without marks the same run goes on to the SNIC. W437 ([#499](https://github.com/MuhammadMoustafa/xppautX/issues/499)).

## W436: structured folds of cycles ([#498](https://github.com/MuhammadMoustafa/xppautX/issues/498))

For a variable period the equations are `F = T f(u)` and `G = T J(u) v + beta f(u)`.
`fnpl` now takes both diagonal state blocks from `T J`, the upper parameter block from the model's derivatives,
and the period, beta and norm columns analytically. Only `G` is differenced in the physical states and model
parameters, following [FNPLF, periodic.f90:427-507](../reference/auto-07p/src/periodic.f90#L427).
For fixed period the beta term is `beta df/dp2`; the same structure applies. The independent polynomial test
uses the exact model Jacobian to isolate these blocks from nested-difference roundoff, and includes nonzero beta.

The search covered `auto_jacobian`, `funi`, the other augmented systems in `autlib3`/`autlib5`, `jacobian` and
`auto_codim`'s directional derivatives. The implementation reuses `auto_jacobian::central` and
`forward_parameters`, `funi`, and the run's preallocated scratch. Keeping the generic whole-system difference
would be fewer lines, but would retain twice the model evaluations and numerical approximations to known blocks.

There was another avoidable cost in `autpp`'s pure worker: every physical-state difference reevaluated all
parameter-only derived expressions. `continued_constants` now reports a bit change in a model parameter, so the
worker retains the derived values copied by `AutoParallel::prepare` until a parameter changes. The existing
constants array is its source of truth; no cache state is added. The worker unit case checks parameter changes,
restoration and signed zero. The potentially impure serial evaluator retains its evaluation order.

**Capped instruction measurement.** The original binary was built from `375b00f7`; the changed one includes the
structured Jacobian and the derived-value reuse. Both restart the very same saved `lpc_soma` solution, after
its five starting-data points, at NTST 50, NCOL 4, 28 physical states, NMAX 3: three stored points, two new points,
one Newton solve/Jacobian per new point. Callgrind counts all threads (the default automatic four).
The whole-process totals include loading/compilation and the final session export; the inclusive `fnpl` cost
isolates the changed evaluator. No full branch was run under callgrind.

| Quantity per new point | Before | After |
|---|---:|---:|
| Whole-process instructions | 43,148,633,352 | 19,511,922,527 |
| Inclusive `fnpl` instructions | 38,715,652,729 | 15,078,811,168 |
| Model evaluations | 1,439,661 | 684,857 |
| Worker derived-expression evaluations | 1,427,400 | 1,200 |

The whole-process count falls 2.21 times and `fnpl` 2.57 times. At each collocation Jacobian the model evaluations
fall from `(1 + 2*56 + 4)*61 = 7137` to `61 + (2*28 + 2)*57 = 3367`; the remaining 402 residual evaluations
in the capped run fall from 61 model evaluations each to 57. The Newton count stays two in total.

**The remaining factor.** Starting data are outside this continuation capture, so they cannot explain its
remaining cost. In the changed capture, correctly rounded `pow` and `tanh` alone take 45.2% of all instructions;
condensation takes 14.9%. The original capture spends 16.6% reevaluating derived expressions; the changed one
spends 0.031%. Thus evaluation and the augmented linear solve, rather than extra Newton iterations, remain.
The 48 s / 19 s ordinary-cycle comparison in W423 was a whole-run ratio, not a matched RHS microbenchmark.
AUTO-07p's translated Fortran uses the system math functions, with integer powers emitted as Fortran integer
powers (`auto_reference.py`); xppautX retains its correctly rounded CORE-MATH calls and compiled array evaluator.
Replacing those by the C library or repeated multiplication for higher powers would change the canonical
numerics and is outside this card. AUTO-07p also sets NMX=5 for ISW=-2 in
[bvp.f90:112-115](../reference/auto-07p/src/bvp.f90#L112); W423's startup shorthand should not be read as NMX=1.

The description search included every two-parameter run starting from a periodic LP, as well as the LPC/fold
word search. Only `ode_mu26_blocked/lpc_soma` and `lpc_dend` use this formulation in the current inventory;
there are no other existing fold-of-cycles baselines to update.

The full 40-point curves still need minutes at one thread. The comparison driver's hang guard is now 600 s,
matching the reference driver; `XPP_CHECK_SLOW` can scale this safety ceiling. No solver or comparison tolerance
changes, and elapsed time is never a numerical acceptance criterion.

**Native timings.** Five paired runs at one thread, under `tools/heavy.sh --alone`, restarting the same saved
solution and dividing by its two new points: before **9.0478 +/- 0.0761 s/point**, after **3.1972 +/- 0.0381**
(mean and sample SD, `ddof=1`), **2.83 times faster**. Before: 8.98249, 8.97987, 9.02080, 9.10861, 9.14716;
after: 3.16457, 3.14799, 3.22086, 3.23202, 3.22054 s/point. Exact timings, binary and snapshot hashes,
conditions and instruction counts are in [w436-lpc-cost.json](studies/w436-lpc-cost.json).
The original native preparation took 105.5655 s for the five startup rows and two curve rows; that mixed cost
is not a steady continuation-point cost. The 3.20 s result still exceeds W423's approximately 0.9 s AUTO-07p
measurement. Its remaining cost is present after initialization and is dominated by evaluation and condensation,
as the instruction breakdown above shows.

**XPPAUT 8.0 attempt.** The two LPC runs are generated through `auto_reference` using the requested
`/home/muham/w424-xpp8/xppaut`, with only the required steady/cycles starts in the scratch description (the
checkout's driver does not support Extend, W438). `lpc_soma` hit the driver's 600 s guard after completing its
five starting-data points and restarting the curve. The saved partial diagnostics have point 4 complete at
g_h 0.0349988, current 99.99831, and Newton at point 5 (g_h 0.06499794), with no convergence failure.
Its reference remains explicitly unavailable for this runtime bound; this is not evidence of a numerical defect.

The separate `lpc_dend` attempt also hit 600 s after successful startup: point 7 completed at g_h 0.1999988,
current 99.99807, and point 8 was in Newton at g_h 0.3349983. Neither partial trace reports a convergence
failure. Both `.xppaut` entries retain these explicit runtime reasons; AUTO-07p's full 40-point curves remain
the references for the two newly enabled runs. XPPAUT's original full augmented differences and interpreted
model evaluator are retained in that reference binary; no altered XPPAUT implementation is used as a reference.

**Baseline and gates.** Only `ode_mu26_blocked.diagram.csv` changes: 90 rows and four branch headers are
appended, every previous row preserved byte for byte. Each LPC run adds five starting-data rows and 40 curve
rows. Their ends are (g_h, current) = (15.9420397502, 99.8773794776) for `lpc_soma` and
(15.9424458079, 99.9584150441) for `lpc_dend`, with no R1, agreeing with both full AUTO-07p curves.
The build uses `-Werror` with zero warnings; all 58 unit binaries pass (64 checks in the fold/derived unit).
The selected control/blocked comparison stores 2522 points across one and automatic threads, both thread
comparisons pass, and all 92 reference quantities agree. The control baseline does not change. Full packed-card
AUTO, autocheck and example gates are recorded with W437 below.

## W437: user points on Mu26's approach to the SNIC ([#499](https://github.com/MuhammadMoustafa/xppautX/issues/499))

The special-point test is parameter minus target in both XPPAUT 8.0's FNUZBV
(`autlib1.c:6872`) and AUTO-07p's FNUZ (`support.f90:746`). LCSPBV brackets and
corrects points on the BVP branch. AUTO-07p restores the converged orbit,
tangent, old point, step history and stability after a failed location
(`bvp.f90:1436-1459`); W326 already supplies that restoration in xppautX.
XPPAUT 8.0 does not restore (`autlib1.c:6498-6501`, finding 47).

A separate inherited error remains inside the correction: ICPS imposes
`integral(u * uold') = 0`. The identity `integral(uold * uold') = 0` holds
for an exact periodic orbit, but need not hold for quadrature of a computed
collocation orbit and its model derivative. Therefore a correction with a
vanishing arclength step can move the previous orbit. AUTO-07p instead uses
`integral((u-uold) * uold') = 0`, which contains the previous computed orbit
exactly. W437 adopts that condition, including every augmented periodic
system, by reusing ICPS. The Jacobian of this condition is unchanged. This
is a deliberate difference from XPPAUT's discretized equations (finding 73),
not a larger location tolerance or an extra iteration allowance. Failed
location rollback is retained. Increasing ITMX or adding another rollback
would leave the phase bias itself intact.

Search/reuse: ICPS/ICPL/ICPD/ICTR/FIPO and the other `fn*` systems in all
`autlib*`, `auto_jacobian`, FUNI, LCSPBV/CONTBV/STEPBV, `LocationState`, the
shared `user_zero`, and the existing comparison/reference readers. ICPS owns
the physical phase condition; the four augmented systems call it. The run
description's optional `marks` is passed to the existing core setting, to
AUTO-07p's UZR, and to XPPAUT's AutoPer dialog. The shared reference vocabulary
now includes UZ (`itp=-4`), so their parameters, norms, periods and spectra are
actually compared. The reference driver reuses `read_description` for shape
and finite-value validation; xdotool receives separate literal arguments,
never a shell command built from a model or parameter name. The foreign
nine-field AutoPer bound is checked before the dialog is opened, and NUZR is
read back from XPPAUT's own constants header. No core trust boundary changes.

Both Mu26 `to_snic` runs now carry the four marks 5, 1, 0.1 and 0.07 pA.
Control labels them at 4.9999999704, 0.9999999355, 0.0999996688 and
0.0699999969 pA. Blocked labels them at 4.9999999923, 0.9999997724,
0.0999999821 and 0.06999999994 pA. Each 170-point continuation has **zero
upward current steps**: control ends at 0.00581952509 pA (period 31305.9150),
blocked at 0.06581564808 pA (period 35096.6263). Thus neither reverses after
the marks. Endpoints depend on the number and sequence of steps and are not
reference special points. The marked XPPAUT 8.0 references now use W438's Extend
driver. Native control labels 5 pA, fails to locate 1 pA, labels an LP at
0.9990257 pA, reverses, and labels 5 pA again. Native blocked labels 5 and
0.9999987 pA, then LPs at 0.1009347 and 0.07001995 pA and ends back at
0.08047857 pA. Neither reaches all four marks. These actual labels are retained
and explained in the descriptions (findings 47 and 73), not replaced by the
modern results. The shared native UZ points remain numerically compared.

The intended Mu26 baseline changes are `ode_mu26_control` (294 of 540 stored
rows change) and `ode_mu26_blocked` (388 of 721). Their steady rows remain
unchanged; their periodic corrections use the centered phase, and `to_snic`
now stores the four UZ locations. The blocked LPC curves also move in their
last digits: final (g_h,current) is (15.9420397525,99.8773794775) for soma and
(15.9424458153,99.9584150441) for dend. No numerical tolerance is widened.

**Record before the update.** The first full comparison had 150 checks and 36 failures:
32 retained-baseline comparisons plus four reference checks in eight quantities. The
maintainer approved adopting AUTO-07p's condition on 2026-10-17. The
[before-update impact report](studies/w437-phase-impact.md) and its
[raw measurements](studies/w437-phase-impact.json) preserve that result.
The sixteen approved baselines were then updated with `autocompare.py --update`;
all their special-point type sequences and label IDs stay unchanged. Each uses
periodic corrections, including curves started from periodic points. The sizes
below compare paired stored indices, so adaptive step changes are included and
are not independent-reference error estimates.

| Description | Points before / after | Changed paired rows | Max norm delta | Max period delta | Threads equal |
|---|---:|---:|---:|---:|---|
| forced_pp_ns | 264 / 262 | 205 | 0.00240022 | 2.62013e-14 | True |
| forced_pp_pd | 706 / 706 | 649 | 0.00526007 | 3.75255e-14 | True |
| hill_cycles_pd | 151 / 151 | 103 | 3.4861e-13 | 3.55271e-15 | True |
| hill_cycles_torus | 196 / 196 | 148 | 7.67053e-13 | 3.55271e-15 | True |
| ode_lecar | 79 / 79 | 29 | 6.94955e-07 | 0.00024097 | True |
| ode_ma23 | 613 / 613 | 290 | 6.51003e-06 | 0.00125482 | True |
| ode_motoneuron6c | 1239 / 1239 | 19 | 1.26329e-05 | 1.3651e-05 | True |
| res_cycles | 376 / 376 | 328 | 5.98905e-07 | 7.90479e-13 | True |
| tstauto_brper | 49 / 49 | 27 | 3.02891e-12 | 3.14593e-12 | True |
| tstauto_frcp | 103 / 103 | 29 | 1.73922e-10 | 3.60672e-09 | True |
| tstauto_lamom | 70 / 70 | 28 | 1.9984e-15 | 4.44089e-15 | True |
| tstauto_lamom2 | 60 / 60 | 28 | 5.41145e-12 | 2.1358e-11 | True |
| tstauto_lecar | 83 / 83 | 29 | 6.95434e-07 | 0.000241136 | True |
| tstauto_lo | 50 / 50 | 28 | 1.57652e-14 | 2.04281e-14 | True |
| tstauto_pp2 | 130 / 130 | 29 | 0.0132213 | 59.7399 | True |
| tstauto_torx | 81 / 81 | 28 | 9.54792e-15 | 3.01981e-14 | True |

Only `forced_pp_ns` changes point count (264 to 262): `ns_up` crosses its
parameter stopping bound after 29 rather than 31 points. Its special labels
and reference curves still agree. The approved sixteen-model update compared
8496 points at one and automatic threads, with 16 cross-thread checks and no
failures. The final full comparison below checks every reference again.

The four reference failures were traced without widening any tolerance:

- `forced_pp_pd` upper `pd_up` R2: par2 differs by 2.27e-5 (bound 2.10e-5).
  Both use `abs(EV(3))-1`. The imposed and remaining -1 multipliers coalesce;
  their numerical tests and location histories are sensitive to mesh and step.
  Halving DSMAX reduces the discrepancy to 1.15e-5; NTST 160 reverses its
  sign (2.80e-5); EPSS 1e-7 does not produce an xppautX R2 label.
- Mu26 UZ spectra: control at 1, 0.1, 0.07 pA and blocked at 1 pA exceed
  their bounds. NTST 50/100/200/400 sweeps are nonmonotonic. At NTST 200,
  changing only the Jacobian to exact moves the neutral multiplier toward its
  mathematical value 1 (blocked 0.07: 1.00497448741 to 1.00000003469), with
  norm changes below 2e-10. This isolates finite-difference sensitivity of the
  long-period variational problem; mesh refinement alone does not resolve it.
- Blocked norm at 0.07 pA: the default-history discrepancy is 8.34e-5 against
  6.03e-5 allowed. Relocating exactly that mark from the nearby saved 0.1 pA
  orbit reduces it to 7.70e-7 at NTST 50, 4.88e-9 at 100 and 2.22e-9 at 200.
  The cause is coarse adaptive mesh/history, separate from multiplier accuracy.
  W471 ([#534](https://github.com/MuhammadMoustafa/xppautX/issues/534)), which
  multiplies out Mu26's x^3 and x^4 instead of calling pow, brings it within
  the bound at the default history, and the trace was removed as stale.
- `tstauto_pp2` first LP: centered (parameter,norm,period) is
  (0.7080732822986838,0.4180552886559583,79.8935775600), against AUTO-07p
  (0.7080732823,0.41805535218,79.89348504). XPPAUT's uncentered result
  (0.708072,0.4280795,64.656143027) deliberately differs in norm and period.
  The four obsolete AUTO-07p LP exceptions are removed; both LPs are compared.
  NTST 60/EPS 1e-9 finds no LP through period 84.4, so the coarse NTST 15
  folds are not established physical bifurcations.

The [trace study](studies/w437-reference-traces.md) and
[raw values](studies/w437-reference-traces.json) retain configurations, negative
results and spectra. Numerical traces name only the failing quantity, keeping
labels and the other quantities checked. A trace must still exceed its original
bound; a resolved or unused trace fails as stale. All eight AUTO-07p UZ parameter
locations remain checked. These discrepancies are traced, not declared equal.

The XPPAUT driver has one Extend path and one per-run timeout recovery. A run
that reaches 600 s is aborted through XPPAUT's own Escape handling and marked
unavailable, preserving earlier runs; both blocked LPC runs exercised this path.
Their more detailed W436 diagnostic reasons are retained. New diagnostic text is
sliced for every run, including Extend, because reused branch/point numbers could
otherwise replace a new UZ spectrum with an older cycles spectrum. All other
Mu26 reference point and curve records remain byte for byte unchanged.

Final committed-HEAD gates ran one at a time: zero compiler warnings under
`-Werror`, all 58 unit binaries passing (70 fold/phase/derived checks); the full
AUTO comparison has 60 descriptions, 9 skipped, 18674 points and 150 checks,
zero untraced failures; all 286 `autocheck` assertions pass; all 216 example
models pass (215 matching outputs, with `ml-range.odex` intentionally writing
none). The table above is copied from that full `--table` output: 48 AUTO-07p
references / 176 compared points and 42 XPPAUT references / 120 points, with
28 and 54 explicit traced entries respectively. The eight out-of-bound numerical
quantities remain documented, not counted as equal. Four quantity-trace mutation
checks pass. The fixed control driver reproduces the corrected crop byte for
byte. The final documentation amendment records these results without changing
the tested code, descriptions, baselines or references.

**The LAPACK build after W437** (coordinator, on the merged tip 0c6b5936): `tools/wslrun.sh
tools/auto-profile/modeldb_check.sh` still finds one fold search and no step back in the default, LIBM=system and
LAPACK builds. `tools/auto-profile/diagram_diff.py build/lapack/xppautX` gives 2 AUTO-07p differences, both at
forced_pp_pd's upper `pd_up` R2, the point traced above (before W437 it was already the largest fraction, 0.908 of
its tolerance). With LAPACK's eigenvalue routines its par2 differs by 1.10e-5, inside the 2.10e-5 bound, so the
default build's par2 trace reads as stale there, and its multipliers differ by 4.69e-5 against 4.29e-5 allowed. The
two coalescing -1 multipliers make the location depend on the eigenvalue routine as it depends on the mesh and the
step; every other description has 0 differences (forced_pp_ns: 265 LAPACK points against the default's 262, all
quantities within tolerance). EISPACK stays the default, and its comparison is the one the traces hold exact.

## W455: the exact Jacobian against two programs' differences ([#518](https://github.com/MuhammadMoustafa/xppautX/issues/518))

W354's continuation comparison (series 20261021b) gated every row against XPPAUT 8.0 and AUTO-07p. xppautX's finite-difference
rows passed everywhere; its exact-Jacobian rows (`jacobian: exact`, every thread count) failed on P2, E2 and T1 and passed on
P1, E1 and E3. The exact rows are the ones that are right.

| Quantity | exact, converged | exact, as run | `difference`, converged | XPPAUT 8.0 | AUTO-07p |
|---|---|---|---|---|---|
| motoneuron6c, HB at stored point 362: ampPar | 0.1737640862 | 0.1737640407 | 0.1737627148 | 0.1737628 | 0.1737626685 |
| the period at it (gate tolerance 0.264) | 608.83265 | 608.8645 | 607.28611 | 607.215 | 607.3187 |
| mu26 blocked, first fold (stored point 16): SmIapp | 0.0657989595 | 0.0657989595 | 0.0657989566 | 0.06579896 | 0.065798956561 |
| its norm (gate tolerance 5.6e-5) | 99.22379848 | 99.22379876 | 99.22409174 | 99.22409 | 99.224092019 |

"Converged" is EPSL = EPSU = EPSS 1e-7, DS 0.002, DSMAX 0.01 for motoneuron6c and 1e-10, 0.001 and 0.05 for mu26; "as run" is
the description's. Three checks that the exact Jacobian is the right one: (1) an independent double-precision evaluation, from
AUTO-07p's own Fortran `FUNC` of the model (the benchmark's `probe.f90`), of the equilibrium by Newton and the Hopf point by
bisection on the real part of the critical pair, with a centred Richardson-extrapolated Jacobian, gives ampPar 0.1737640862 and
period 608.8326, xppautX's converged exact numbers to every printed digit; (2) `tests/test_exact_jacobian.cpp` now compares
the exact Jacobian of mu26 and motoneuron6c, at a point away from the initial one, with central differences: 7e-8 and 1.3e-7
relative to the larger of 1 and the entry; (3) the difference in the two programs' rows is independent of the tolerances
(the converged rows differ as the as-run ones do) and is reproduced, to every printed digit, by AUTO's own step rule applied to the independent evaluator.

**Why the differences differ**: both programs difference the right-hand side centrally with one step for every state,
1e-7 times the largest state plus 1 ([AUTO-07p finding 7](auto07p-findings.md#7-the-finite-difference-jacobian-steps-every-state-by-the-largest-ones-scale),
[XPPAUT finding 74](xppaut-findings.md#74-autos-finite-difference-jacobian-steps-every-state-by-the-largest-ones-scale)), 7e-6 on
motoneuron6c, whose gating variables and concentrations are 1e-5 to 1e-22. The Hopf point moves 1.4e-6 in ampPar, and the
period at a Hopf point there changes by about 7e5 per unit of it: 0.25 percent. Mu26's fold norm moves 2.9e-4 and its Hopf curve by up to
8e-4 in SmIapp (6e-5 against XPPAUT). The gate's period and norm tolerances (position error along the branch times the
branch's own slope, plus Newton's) do not include the Jacobian's error, which for a spectrum they do (`FINITE_DIFFERENCE`),
and could not include a Hopf period's sensitivity.

**P2's periodic special point.** Exact labels BP at point 3 of the cycles from the Hopf point, the difference rows, XPPAUT and
AUTO-07p an LP. Neither is real: the parameter is monotonic from the Hopf point (0.5727814554, 0.5727816054, 0.5727852281), and with EPS
1e-8 and DS -0.002 neither Jacobian labels anything in 60 points, at NTST 100 and 200 alike. The point is 1.5e-7 in ampPar and
8e-6 in norm from the start; both tests change sign between points 2 and 3, the fold function (the tangent's parameter
component, +4.6e-6 then -5.8e-6) and the branch-point function (a determinant ratio of 1e-16 to 1e-17, noise), and which
one is located first depends on the Jacobian: the fold's location does not converge (its values -3.8e-5, +1.4e-5, -1.3e-5,
-1.5e-5, ...), the branch point's is taken. The table above (W431) lists this point as a fold of cycles; it is the start of
the branch, in all four programs. W458 found the two causes and removed both from the core
([W458](#w458-the-first-steps-of-cycles-from-a-hopf-point-521)).

**How the gate should judge exact rows** (a proposal for the benchmark's driver, `paper/benchmarks/comparison/continuation.py`,
not made here): a program that differences cannot judge a Jacobian that does not, wherever a quantity depends on the Jacobian's
accuracy (a Hopf period, a fold's norm, a Hopf curve). An exact row is equal work when (1) each of its runs has the special points of xppautX's
`difference` pilot, in order (the pilot is gated against both programs), and (2) its values are within the existing tolerances of a converged exact run of the same description (EPSL, EPSU,
EPSS and DS, DSMAX divided by 1e3 and 10, as above, one run per workload), compared by `compare_references` as a reference of
its own. (1) fails on P2's cycle start, which is a start-of-branch artefact of both Jacobians: until it is dropped in the core
(a separate card: a test function that changes sign between the first two points of a branch started from a Hopf point
should not label), (1) skips the first two points after a Hopf start.

## W456: the five GH on motoneuron6c's Hopf locus are not generalised Hopf points ([#519](https://github.com/MuhammadMoustafa/xppautX/issues/519))

W354's T2 workload (series 20261021b) continues the Hopf locus of motoneuron6c in (CaDen_Max, ampPar) from the fifth HB
(ampPar 0.5728), the order that W431 reversed so both programs orient the curve by ampPar. xppautX and XPPAUT 8.0 label no
codimension-two point; AUTO-07p labels five GH, at points 63, 67, 71, 76 and 83 of the arm that runs to ampPar 0.82:

| AUTO-07p's point | CaDen_Max | ampPar | first Lyapunov coefficient, independent, at that place |
|---|---|---|---|
| 63 | 9.6795e-5 | 0.739173 | +1.2527e-3 |
| 67 | 8.1050e-5 | 0.748771 | +9.136e-4 |
| 71 | 6.7135e-5 | 0.759599 | +1.0513e-3 |
| 76 | 5.9025e-5 | 0.773125 | +1.0228e-3 |
| 83 | 3.8473e-5 | 0.795370 | +9.210e-4 |

(AUTO-07p's `hb_up` is our `hb_down`. At each label's CaDen_Max the Hopf value of ampPar is found again, within 3e-7 of AUTO-07p's.)
The independent coefficient: at each label and at each of our 89 points of the arm (CaDen_Max fixed), the equilibrium and the Hopf value of ampPar
are found again in quadruple precision from AUTO-07p's own Fortran `FUNC` of the model (compiled with `-freal-8-real-16`), the
Jacobian, and the bilinear and trilinear forms by central differences in quadruple precision with steps 1e-8 and 3e-9 (the two
agree to 2e-4), and the coefficient taken from Kuznetsov's formula,
`l1 = Re[<p, C(q,q,conj q)> - 2<p, B(q, A^-1 B(q, conj q))> + <p, B(conj q, (2 i w - A)^-1 B(q,q))>] / (2 w)`,
`A q = i w q`, `A^T p = -i w p`, `<p,q> = 1`, with `|q| = 1` as xppautX's `lyapunov` normalises. Over the 89 points it is
+8.5e-4 to +1.9e-3, never zero or negative, and differs from xppautX's value (the GH test function, logged at `IID` 2) by 1.5e-4 relative at most.
The coefficient between the labels is no different from the coefficient at them (+9.1e-4 to +1.25e-3 over our points 63 to 83), so the
five sign changes AUTO-07p's test function shows within 0.056 of ampPar are noise of its own, as in
[finding 2](auto07p-findings.md#2-a-newton-residual-contaminates-cp-and-gh-test-functions). The tolerance is the cause:
rerunning AUTO-07p 0.9.4 on T2's own description (the `eq.f90` and constants files the pilot made) with EPSL = EPSU = EPSS 1e-4
labels the same five GH at the same points; at 1e-7 and at 1e-9 it labels none, and the arm ends at the same place
(ampPar 0.81858 and 0.81870).

xppautX's own value does not change with the Jacobian: with `jacobian: exact` the coefficient is +8.5e-4 to +1.9e-3 as well.
T2's `differences` therefore take five entries for the reference `.auto07p`, side `reference`, type `GH`, index 1 to 5, on
the run `hb_down`: the gate compares AUTO-07p's `hb_up` with our `hb_down` (the two arms of one curve, opposite sign
convention, its note), and a traced difference belongs to the arm it is compared with. With the five entries the gate reports no difference
for T2 on the pilot's data (checked with `compare_references` on the pilot's diagram, `reference.json`s and ranges: one
difference without them, none with them).

## W457: the exact rows are judged against a converged exact run ([#520](https://github.com/MuhammadMoustafa/xppautX/issues/520))

W455 showed that XPPAUT's and AUTO-07p's finite-difference Jacobians, not xppautX's exact one, are off on P2, E2 and T1, so a
row with the exact Jacobian cannot be judged against them. The continuation study's driver
(`paper/benchmarks/comparison/continuation.py`, git-ignored with the rest of `paper/benchmarks/`) now gates the rows of the
group `jacobian: exact` (xppautX's `xppautx-exact-*`, AUTO-07p's `auto07p-exact-1`, W459) differently; the finite-difference rows keep the
gate of every earlier series (every program against the two others' pilots).

An exact row is equal work when (1) its special points are those of xppautX's own finite-difference pilot, in order, and
(2) its values are within the gate's tolerances (`compare_run`'s and `compare_curve`'s, the row's own EPS and DSMAX, unchanged) of a
converged run of xppautX's exact Jacobian on the same description: EPSL, EPSU and EPSS divided by 1000, DS, DSMIN and DSMAX by 10,
NMX times 10. That run is a pilot of its own, untimed, in the workload's folder (`pilot-xppautx-converged/`, with `converged.json`).
(2) is one `compare_references` call: a row of xppautX as `ours` against the converged run in the form of a reference file (the
points `labelled` gives, every stored point's [parameter, second parameter, norm]), a row of AUTO-07p as the reference against the
converged run's diagram as `ours`. (1) is checked once, on the pilots: the converged run's special points must equal the
finite-difference pilot's, and (2) makes every row's equal to the converged run's. The first two points after a Hopf start
are left out of both (W458's artefact: a test function that changes sign between a Hopf-started branch's first two points labels
there, BP in xppautX and LP in AUTO-07p on P2's cycles, none at EPS/1000 and DS/10: the stored points 2 and 3 of a periodic run started at HB).
A converged run that fails (the tight tolerances of ode_mu26_control and ode_mu26_blocked do not converge on the cycles near the
SNIC, even with the tolerances divided by 10 only: "no convergence even at the smallest step", branches 49 and 50, near the end of the cycles run) is "unavailable" in `converged.json` and the
workload's exact rows keep the finite-difference gate; the fixed-step P1 has none (its step is the workload). A deliberately wrong
exact row fails: xppautX's finite-difference pilot run as an exact row is refused on E2's HB period (607.319 against the converged 608.833, tolerance 0.26).

Every row, finite-difference or exact, now carries its error against the converged run (W445's error beside the time):
`converged_error` (the worst difference of each compared quantity: `par`, `norm`, `period`, `spectrum`, `curve par`, ..., of the run and
the runs it started from) and `converged_error_ratio` (the worst difference over its tolerance) in `runs.json` and `summary.csv`.
On the pilots, against the converged run (E2 and P2's steady run, the Hopf point at stored point 362 of the as-run diagram):

| Row | HB period (converged 608.83265) | HB ampPar (converged 0.1737640862) | norm | worst over tolerance |
|---|---|---|---|---|
| xppautX, finite differences | 607.3187, 1.51 off | 1.42e-6 | 6.2e-3 | 5.7 |
| XPPAUT 8.0 | 607.2151, 1.62 | 5.6e-6 | 6.2e-3 | 6.3 |
| AUTO-07p, finite differences | 607.3187, 1.51 | 1.42e-6 | 6.2e-3 | 5.9 |
| xppautX, exact | 608.8645, 0.032 | 7.4e-7 | 9.9e-5 | 0.12 |
| AUTO-07p, exact (W459) | 608.8645, 0.032 | 7.4e-7 | 9.9e-5 | 0.12 |

On T1 (the first fold of mu26 blocked, SmIapp 0.0657989595 and norm 99.22379848 converged) the norm is off by 2.9e-4 in the finite-difference rows
(ratio 5.2, XPPAUT 5.2 and 1.2 on the Hopf curve) and by 3.6e-6 in the exact rows (0.58; the curve 0.001). T2 passes its finite-difference
gate with W456's five traced `GH` differences, and its exact row passes against its converged run.

A bug of W454's `--reuse` row loop, found by testing it here: an imported row's `reusable_row` result was unpacked as `found, why`
(the records and the folder) and used as the records, so no row of an earlier series could be imported; it is `recs, source` now. `--pilots` (with
`--only`) runs a workload's pilots and gates (the converged and exact pilots included) on the full descriptions, untimed, then stops.

## W459: AUTO-07p's exact derivatives for motoneuron6c and Mu26 blocked ([#522](https://github.com/MuhammadMoustafa/xppautX/issues/522))

The `auto07p-exact-1` row (`JAC = 1`: AUTO-07p's `DFDU` and `DFDP`) ran on P1 and E1 only, with PY_S1Bf's hand-written `py_s1bf.c`. It runs on
P2, E2 and T1 as well, with an equation file generated from the model by `paper/benchmarks/auto/auto07p/exact_source.py`: the expanded right-hand
side of `tools/auto_reference.py`'s Fortran `FUNC` (`Equations.expand`, `conditionals`, `ordered_assigns`), differentiated by forward mode over the
model's own formulas, in C, straight-line, one temporary per operation, a derivative only where it is not zero (`if C then A else B`: the
branch taken; `exp`, `tanh`, `abs`, `max` and powers are the functions the two models need; another is refused by name). `DFDU(i, j)` is
filled for every state, and `DFDP(i, k)` for the parameters the description's runs continue (ampPar on P2 and E2; SmIapp and SmHxgMax on T1),
AUTO adding the period's column itself, as for P1. xppautX's own symbolic Jacobian (`core/expr_derivative.cpp`) emits no expressions, and a core feature for a
benchmark's input is not small.

Every derivative is checked before AUTO-07p runs (`exact_source.check`, the workload's `exact/exact-check.json`; a failure stops the workload):
the file is compiled with a driver and each column of `DFDU` and `DFDP` compared with Richardson-extrapolated central differences (step
max(1e-3 |x|, 1e-8)) at the start and at three points off it (each state scaled by its own factor, up to 7 percent), relative to the larger of 1 and the
derivative as `tests/test_exact_jacobian.cpp`'s; `ijac` 1 and 2 must agree, and the C right-hand side's values at the start must equal the
Fortran `FUNC`'s to `check_rhs`'s 1e-12 of the terms' sizes. Largest relative error: motoneuron6c (25 states, ampPar) 4.7e-7 in the states, 1.9e-9 in
the ampPar column, the right-hand sides equal to the last bit; Mu26 blocked (28 states, SmIapp and SmHxgMax) 5.4e-8 (the SmHxgMax column), the right-hand
sides within 4e-19. The errors are the differences' own (their step), the tolerance 1e-6. On the pilots the three programs' exact rows
agree: the table of W457, and T1's, P2's cycles, all in the gate's tolerances of the converged run.

## W474: AUTO-07p's nested dissection for the collocation blocks ([#537](https://github.com/MuhammadMoustafa/xppautX/issues/537))

`brbd` (core/autlib2.cpp) no longer eliminates the condensed blocks in AUTO2000's chain (block 1 into 2, the result into 3, and so
on) but in AUTO-07p's REDUCER tree (solvebv.f90): blocks lo..hi split at (lo+hi)/2, both halves reduced, then their interface;
back substitution (BCKSUBR) walks the same tree down. The tree depends on NTST alone, so it is AUTO-07p's own order on one
thread and xppautX's on every thread count (AUTO-07p cuts its ranges by its thread count). Pivoting is AUTO2000's complete
pivoting over the two blocks' rows, as before. A deliberate numerical change, once: 25 diagram baselines were rewritten
(explicit_homoclinic, forced_pp_ns, forced_pp_pd, hill_cycles_pd, hill_cycles_torus, lorenz_homoclinic, ode_lecar, ode_ma23,
ode_motoneuron6c, ode_mu26_blocked, ode_mu26_control, res_cycles and thirteen tstauto descriptions). Labelled points move by at most
one stored point (three on forced_pp_ns, two on forced_pp_pd), and one point's stability sign flips among motoneuron6c's near-unit
multipliers.
The full comparison: 60 descriptions, 9 skipped, 18676 points, 150 checks, no untraced failure; examples unchanged.

Five quantities left their bounds, each where a Newton tolerance is amplified, and are traced (their descriptions' `differences`,
no tolerance widened). To tell rounding from a lost accuracy, each was recomputed in three orders (AUTO2000's chain, the tree, and a
tree split at (lo+hi-1)/2) and with the run's EPSL and EPSU tightened from 1e-7 to 1e-10:

| Quantity | Chain | Tree | Other tree | Tree, chain at 1e-10 |
|---|---|---|---|---|
| forced_pp_pd pd_up R2 par (bound 2.0e-6) | 1.6e-6 | 3.6e-6 | no R2 | 6.2e-7, 2.4e-6 |
| forced_pp_pd pd_down R2 spectrum (bound 8.3e-5) | 5.7e-5 | 3.2e-4 | 4.7e-5 | both near -1 +- 1e-5i, inside |
| ode_mu26_blocked to_snic 0.07 pA UZ norm (bound 6.0e-5) | 5.0e-5 | 8.0e-5 | 8.3e-5 | 1.1e-5, 7.3e-6 (the orders agree to 4e-6) |
| ode_mu26_blocked to_snic 1 pA UZ spectrum, against XPPAUT (bound 2.2e-5) | 1.4e-5 | 3.4e-5 | 5.0e-6 | (W437's neutral multiplier) |

pd_up's R2 norm follows its par (3.5e-6, bound 1.8e-6). At the R2 two multipliers coalesce at -1, so their split moves as the square
root of a perturbation: the 1e-7 Newton tolerance alone allows about 3e-4. Next to the SNIC (period 2715) the collocation system is
nearly singular, so where Newton stops depends on the order. With the tighter tolerances the tree is as close to AUTO-07p as the
chain or closer.

Times (Ubuntu, best of three, `autocompare.compute`): ode_mu26_blocked's steady and cycles runs 11.54, 6.96, 4.85 s at 1, 2 and 4
threads before, 11.44, 6.39, 4.07 s after (2.81 times faster at four threads instead of 2.38); motoneuron6c's steady and periodic runs
2.95, 2.45, 2.19 s before, 2.93, 2.37, 2.09 s after. Four chunks (the tree's second level) go to the workers and the three merges above
them run on the caller; eight chunks change no number and measured 1.7% slower at four threads, 1.2% faster at three.

## W458: the first steps of cycles from a Hopf point ([#521](https://github.com/MuhammadMoustafa/xppautX/issues/521))

W455 found an LP (difference Jacobian, XPPAUT 8.0, AUTO-07p) or a BP (exact Jacobian) at the third point of motoneuron6c's
cycles from the Hopf point, 1.5e-7 in ampPar from it, where ampPar rises monotonically and nothing is labelled at EPSL = EPSU
1e-8 and DS -0.002. The maintainer's decision (2026-10-25): where AUTO-07p labels a point that is not there, xppautX does
not follow it.

**What the test functions do at a Hopf start.** Both the fold test (the parameter component of the branch's tangent) and
the branch-point test (a ratio of determinants, the Newton system's over its condensed block's) vanish at the start by
construction: the cycles leave the Hopf point with the parameter stationary, and the Hopf point is where the constant
solutions cross the cycles' equations. Every test starts at 0, so the first step never labels (AUTO-07p's `TEST(I)=0`,
[bvp.f90:178](../reference/auto-07p/src/bvp.f90#L178), with the sign check at
[bvp.f90:1325](../reference/auto-07p/src/bvp.f90#L1325)); the first comparison is the second step, between the second and
third point. Two separate errors gave the second point's value the wrong sign there:

1. **The fold test was read from the predictor's Jacobian.** AUTO (AUTO-07p's Newton, [bvp.f90:711-724](../reference/auto-07p/src/bvp.f90#L711),
   and ours) solves for the tangent with the last factorization, at the iterate before the final correction. At the second
   point the corrector starts from the eigen-cycle and converges in one correction (motoneuron6c: 1.7e-6 relative, 1e-6 in
   ampPar), more than the branch has moved since the start (1.5e-7), and near the start the tangent is ill-conditioned.
   The fold test was +4.57e-6 at the second point, -1.6e-5 at the third; factored at the point it is -2.5e-5, the sign of
   every later point's. Refactoring at the second point alone does not do: the location then compares that value with
   the third point's and its own iterates', still taken at their predictors, and on the Bautin model below it ran into the first step.
2. **The phase condition's orientation followed the eigen-cycle, not DS.** The start's `upoldp` is the eigen-cycle's
   derivative; the first cycle is the equilibrium plus DS times the eigen-cycle, so from the second point on the phase
   condition is the derivative of a cycle oriented by the sign of DS. With DS < 0 the phase row, and with it the
   determinant, changed sign between the second and third point: motoneuron6c's BP test -1.16e-16, then +1.85e-16 (with DS
   +0.02 it keeps its sign: -1.16e-16, -2.09e-17, -5.16e-16). The determinant is linear in that row; nothing else reads its sign.

**The rule.** The start's phase condition is oriented by the sign of DS (`auto_hopf_mesh`), and on a branch started at a
Hopf point (`stpnps`, `stpnwp`) the points of the first two steps, and every point of a location between them, are
factored again at the converged point before their tests and multipliers are read (`stepbv`, `hopf_start_steps`), the
rule W421 made for every equilibrium. The orientation changes no number but the determinant's sign (multiplying a row by
-1 changes no pivot), so a DS > 0 run is untouched.

**Rules considered and not taken.** A test whose two values lie below a noise level: the fold test is the parameter
component of a unit tangent, about 1e-5 all along motoneuron6c's cycles (their parameter barely moves), and no level
separates the spurious change from a real fold of such a branch. A zero located at the start or within the first step:
the spurious zeros were located at 0.07 percent of the step (LP), 22 to 26 percent into it (BP, exact Jacobian) and,
with DS 0.002, before the second point; a position does not tell them from a real fold. A sign change that disappears at
a tighter tolerance: true (W455), but it needs a second run. Skipping the second step's comparison: the Bautin fold below
lies in that step and would be lost. All steps at the converged point's Jacobian: the whole comparison took 45 percent
longer at one thread (197 s against 136 s), and on `tstauto_pp2`'s cycles near their homoclinic orbit (NTST 15) it
labelled an LP at stored point 24, where the parameter rises monotonically (0.7080693, 0.7080721, 0.7080730).

**A real fold close to the start is kept** (`tests/auto/bautin_near_hopf.json`, the Bautin normal form
`r' = r (b1 + b2 r^2 - r^4)` with b2 = 0.0032, `tools/models/bautin_near_hopf.odex`): its fold of cycles is at amplitude
0.04, b1 = -2.56e-6, between the second (0.02) and third (0.06) point of the cycles from the Hopf point at DS 0.02.

| Run | fold: norm | fold: b1 | other labels |
|---|---|---|---|
| exact | 0.04 | -2.56e-6 | |
| xppautX, DS 0.02 and -0.02 | 0.0399989 | -2.5599999923e-6 | none |
| xppautX before W458, DS 0.02 | 0.0404764 | -2.55853e-6 | none |
| AUTO-07p 0.9.4, DS 0.02 | 0.040476392578 | -2.5585301701e-6 | none |
| AUTO-07p 0.9.4, DS -0.02 | 0.018453107838 (before the second point) | -9.7370305335e-7 | BP at norm 0.02027 |
| XPPAUT 8.0, DS 0.02 and -0.02 | 0.0404764 | -2.55853e-6 | none |

AUTO-07p's DS < 0 run labels a BP that is not there and the LP in the first step, and misses the fold
([auto07p-findings.md](auto07p-findings.md#8-the-first-comparison-of-cycles-from-a-hopf-point-reads-a-predictors-fold-test-and-a-misoriented-phase-condition));
the description traces both, and the last-iterate programs' fold 1.2 percent off in amplitude.

**motoneuron6c** labels nothing at the start of its cycles with either Jacobian at the description's DS -0.02, at DS
-0.002 and 0.002, at DS -0.002 with EPSL = EPSU 1e-9, at DS ±0.0002 with the difference Jacobian, and on P2's run to
ampPar 0.62 (108 points). One case remains: the exact Jacobian at DS +0.0002 (a hundredth of the description's) labels an
LP at the third point, where the computed branch's parameter does dip, by 7e-11 (second point 0.572781455613, the LP
0.572781455540, third point 0.572781456017):
a fold of the computed points, within the Hopf start's own accuracy, that no test of the branch tells from a real fold of that size.

**Baselines.** `ode_motoneuron6c` loses the LP at the third point of its cycles (label 12; the points after it follow
from the third point instead of the located one, and the later labels renumber; its second point's multipliers change as
below); its two references trace their LP. Seventeen more descriptions change only the multipliers of the second and
third points of their cycles from a Hopf point (forced_pp_ns, forced_pp_pd, hill_cycles_pd, hill_cycles_torus, ode_lecar,
ode_ma23, ode_mu26_blocked, ode_mu26_control, res_cycles, tstauto_brper, tstauto_frcp, tstauto_lamom, tstauto_lamom2,
tstauto_lecar, tstauto_lo, tstauto_pp2, tstauto_torx): the trivial multiplier, 1 for a cycle, was up to 2.3e-4 from 1
(tstauto_lamom2's third point; ode_lecar's second 1.3e-4) and is within 6.1e-9 of it at every one of these 35 points:
closer at 29, the same on the two Mu26 descriptions, and at two, already within 2.4e-12, it moves by rounding
(forced_pp_pd's second point to 1.6e-12, tstauto_lo's to 2.3e-11). No point of any branch moves, no label is added or removed
elsewhere, and the AUTO-07p and XPPAUT comparisons pass (61 descriptions, 153 checks). The time does not change (one
factorization more for each of the first two points of such a branch).

## Start validation (W495, [#559](https://github.com/MuhammadMoustafa/xppautX/issues/559))

Before continuation, xppautX checks an initial steady state's largest absolute
RHS residual against the run's EPSU (the state tolerance). A labelled restart
or an orbit start does not undergo this equilibrium test. Parameter bounds
apply to both model parameters in a two-parameter run; an initial primary
parameter on a bound must have Ds pointing into the interval. A second
parameter's direction is determined by the computed tangent, rather than Ds.
The start's norm is the solver's own Euclidean (equilibrium) or collocation
L2 (orbit) norm, including only the original model states.

Norm Min and Norm Max are retained. Their named defaults are 0 and 1e100:
a norm cannot be negative, and the wide upper default avoids truncating an
ordinary growing branch at XPPAUT's former default of 1000. These are
xppautX defaults; AUTO-07p's A0/A1 remain explicitly configurable bounds.
No baseline is rewritten for a start that fails validation.

## W518: the global ends of a periodic branch ([#582](https://github.com/MuhammadMoustafa/xppautX/issues/582))

AUTO-07p 0.9.4 and XPPAUT 8.0 detect no global bifurcation. A branch of cycles whose period grows
without bound, towards a saddle-node on an invariant circle (SNIC), a homoclinic orbit or a heteroclinic
cycle, shows only as a period growing until the step limit or no convergence (EP or MX), with no
word on why. xppautX stops the branch where the period has grown over 5 points to twice the branch's
shortest and the orbit lingers (some mesh point's speed |f| at most 1e-6 of the orbit's fastest),
finds the equilibria the slow stretches of the orbit lie near (Newton's method with the
Sing pts settings, the eigenvalues at the root, or at the slowest point itself when there is none) and stores the
end as `snic` (one equilibrium with a zero eigenvalue, within 1e-3 of the spectral radius), `homoclinic`
(one saddle) or `heteroclinic` (two or more saddles), or as an end of branch that says "period growing
without bound" when none fits (core/auto_global.h gives each number its reason). This is a deliberate
difference: where the references run on, xppautX stops at the first point whose orbit is that slow.

Against the references nothing else moves. `tools/autocompare.py` leaves the three new types out of the
comparison of special points, as it does EP (`GLOBAL_ENDS`), so a stop that AUTO-07p labels EP or MX is
not a missing or extra special point; every AUTO-07p and XPPAUT quantity still passes (the two `ode_mu26`
descriptions' marks at 5, 1, 0.1 and 0.07 pA come before the end). Of the 61 baselines, two changed
on purpose, only in their last rows:

| Description | Run | Before | Now |
|---|---|---|---|
| `ode_mu26_control` | `to_snic` | 170 points, ending EP at the step limit with period 31307 | 153 points, ending `snic` (label 21) at SmIapp = 0.00633838, period 4055.09: the fold of the equilibria is at 0.0058127 |
| `ode_mu26_blocked` | `to_snic` | 170 points, ending EP at the step limit with period 35081 | 155 points, ending `snic` (label 20) at SmIapp = 0.0667654, period 4974.13 |

The rows before each end, and every later run of `ode_mu26_blocked`, are byte for byte the old ones. Mu26's
slow variables (calcium, gating) hold its cycles at a thousandth to a hundred-thousandth of their fastest speed
for most of the period, with no equilibrium near, from the first points; a threshold of 1e-3 stopped them at
SmIapp = 22.95, period 71, naming a slow mode's eigenvalue (a hundredth of the spectral radius) a fold's:
that is why the threshold is a millionth and the zero eigenvalue a thousandth of the radius.

Three new small descriptions, with no reference file (`auto07p_skip`, `xppaut_skip`: no reference
end to compare), hold the three ends, from tools/models: `global_snic` (r' = r (mu - r^2), phi' = 1 - r x:
the cycles of radius sqrt(mu) from the Hopf point at mu = 0 have period 2 pi / sqrt(1 - mu^2), a SNIC at mu = 1),
`global_homoclinic` (the double-well Duffing oscillator x'' = x - x^3 + mu x' - x'^3: the cycles around the centre
(1, 0) grow into the origin's saddle at mu = 0.3287) and `global_heteroclinic` (x'' = -x + x^3 + mu x' - x'^3:
the cycles around the origin meet the saddles (+-1, 0) at mu = 0.3314). `tools/servercheck.py`'s `stops`
section runs each and checks the stop's reason, its words and the stored type.

## W537: every point type from a published model, with an AUTO-07p reference ([#602](https://github.com/MuhammadMoustafa/xppautX/issues/602))

Before the W354 comparison with XPPAUT and AUTO-07p, each point type xppautX names (`core/auto_codes.h`, `auto_point_names`)
is to be checked by a published model or a named demo, never one made up for the case (AGENTS.md). The audit of the 55 models the
descriptions run (tests/auto/README.md, "Where each model comes from") found 7 published, 39 named demos and 9 made up, and four gaps.
Each model's source is also the first comment of its file.

| Type | Now checked by | AUTO-07p | XPPAUT 8.0 |
|---|---|---|---|
| `generalized_hopf` (GH) | `bykov_hopf`: the Bykov-Yablonskii-Kim model of CO oxidation on platinum, with the equations and values of Kuznetsov's MatCont Tutorial IV (https://dercole.faculty.polimi.it/tds/matcont/6/2params.pdf); the original is V. I. Bykov, G. S. Yablonskii, V. F. Kim, Doklady AN SSSR (Chemistry) 242(3), 1978, 637-639, as en.wikipedia.org/wiki/Grigoriy_Yablonsky gives it | labels the two published Bautin points 0.002 and 0.007 off in Q2, and three more; with EPS 1e-9 exactly the two (below) | skipped: its steps along the curves are ten times shorter and it detects no codimension-two point (finding 71) |
| `bogdanov_takens`, `cusp` | `bykov_hopf` (the tutorial's two BT and one CP), `tstauto_ab_fold` | BT agree; CP: three labels, none at the cusp (below) | as `ab`'s |
| `homoclinic` | `lrz_cycles`: the Lorenz equations (Lorenz 1963, J. Atmos. Sci. 20(2) 130-141) at sigma = 10, beta = 8/3, AUTO-07p's `lrz` demo: the cycles of the Hopf point at r = 24.7369 end at r = 13.9265574; `lorenz_homoclinic` (HomCont) puts the orbit at r = 13.92655741 and the literature at 13.926 | the cycles to period 5e4, trimmed where xppautX ends them | same |
| `heteroclinic` | `pp2_heteroclinic`: AUTO-07p's `pp2` demo (Doedel, J. Math. Biol. 20 (1984) 1-14), at the demo's NTST 90: the cycles of its Hopf point end on the heteroclinic cycle between the saddles (0.1276, 0) and (0.6518, 0) at quota 0.7080728, as the demo's text says | the cycles agree to quota 0.7080728 (its first label, an LP at period 138.6, is past the end) | two BP at periods 76 and 99 that neither other program labels |
| `resonance_1_1`, `_3`, `_4`, `fold_flip` | `hen_lp`, `hen_tr`, `hen_pd`: the Henon map of AUTO-07p's `hen` demo (Govaerts et al. 2007, section 5.1, r = 0) | all agree | no resonance or fold-flip label (finding 71); `hen_tr`'s XPPAUT comparison is skipped for its V-shaped norm at the 1:4 resonance |
| `snic` | `ode_mu26_control`, `ode_mu26_blocked` (the maintainer's model, published as code only), `global_snic` (made up) | no label of its own: compared through the diagram around the end (W518) | as W518 |
| `double_neimark_sacker` (NSNS), `fold_neimark_sacker`, `flip_neimark_sacker`, `zero_hopf` | only `henon_rot` (the Henon map beside an invented rotation) and `zero_hopf` (made up): no published model found | | |

NSNS is the one type no description's baseline holds: it needs a map of dimension four with two independent Neimark-Sacker pairs,
and `henon_rot_tr`'s torus-torus point (TTR) lies in its skipped `down` run (W405). A search for a published map found MatCont's
manual (a normal form, not a model), Habib, Rega and Stepan 2012 (a normal-form analysis) and the Leslie-Gower juvenile/adult
competition map of Govaerts et al. 2007, section 5.2 (Table 5.1), whose published thresholds (c_yj0 = 0.474477674,
c_jy0 = 0.4571312026) do not follow from the printed equations and values (they give 0.7627 and 0.2451: the product agrees, the ratio does
not), so it cannot stand as a model until the two are reconciled. No model was invented.

**The tutorial's Bykov numbers and ours.** Tutorial IV's Hopf and fold points of the equilibrium curve from Q2 = 2.204678 (K = 0.4) are reproduced
to the printed digits (Hopf at Q2 1.051557 and 1.040991, folds at 1.042049 and 1.0522); its BT at (1.417628, 0.971397) and (1.161199, 0.722339),
its cusp (1.006408, 0.355991) and its two GH at (0.891319, 0.232487) and (0.924255, 0.305879) are found by xppautX at (1.417632, 0.971408),
(1.161201, 0.722349), (1.006408, 0.355991) and (0.891324, 0.232490), (0.924256, 0.305880). AUTO-07p's BT agree (to 1e-5); its GH and CP do not
(docs/auto07p-findings.md, finding 2): at the description's tolerances 1e-4 it labels five GH, two of them near the published ones (0.893416, 0.233823 and
0.917241, 0.300840) and three not (0.78621, 0.81482, 0.94617), and three CP, the first 0.002 from the cusp (1.00845); with EPSL = EPSU = EPSS 1e-9 it labels
exactly the two published GH and no CP. The description holds those differences exactly (`generalized_hopf` 2, 3, 5 and `cusp` 2, 3 of the reference, and
`par`, `par2`, `norm` of ours' two GH and its cusp), so they fail if the reference or xppautX moves.

**The global ends against AUTO-07p.** AUTO-07p and XPPAUT end a branch of cycles near a homoclinic or heteroclinic orbit at their step limit,
through ever slower cycles whose labels (LP, BP, MX at periods of 5e3 to 5e4 for the Lorenz cycles) belong to the mesh. xppautX stops at the first cycle that is slow enough
(W518). Since a reference run goes on past that end, `tools/autocompare.py` (`stop_at_global_end`) compares the reference's special points and curve only up to the sample
nearest ours' last point when ours ends on SNIC, HOC or HEC, and says nothing of what follows; a reference with no curve is compared whole. The `lrz_cycles`
cycles' last sample (r = 13.926557423, norm 5.4166) lies 4e-4 from AUTO-07p's sample at r = 13.926557443, norm 5.4162; HomCont's orbit at r = 13.92655741 is
1.3e-8 from the end.

`lrz_cycles`' one traced difference: both AUTO-07p and XPPAUT label a BP at the third point of the cycles of the Hopf point (r = 24.7262, period 0.6532),
where the second multiplier of the new cycle (1.0004) has only just left the trivial one; at DS -0.1 instead of -0.5 the label is at r = 24.7364, so it
follows the first steps and not the branch. xppautX labels none. `pp2_heteroclinic`'s: XPPAUT labels two BP before the end that AUTO-07p and xppautX do not
(not traced further).

**A reference tool fix.** AUTO-07p prints a multiplier of 1.8e308 (an infinite one, near a homoclinic orbit) as `1.79769+308`, without the `E`;
`tools/auto_reference.py` (`fortran_number`) now reads it, where it stopped with a `ValueError`.

**Made-up models a published one could replace.** `global_homoclinic` by `lrz_cycles`, `global_heteroclinic` by `pp2_heteroclinic`, `explicit_homoclinic` (HomCont, closed form) by `lorenz_homoclinic`,
`henon_rot`'s R1-R4 and fold-flip by the `hen_*` descriptions (its torus-torus, fold-torus and flip-torus points have no published model), `bautin_near_hopf` (a fold of cycles by a Hopf point) by `ode_lecar` or `tstauto_lamom2`
(cycle folds of published or demo models; its Bautin normal form itself checks GH's `l1 = 2*b`, which `bykov_hopf` now also reaches), `global_snic` by `ode_mu26_*`. Nothing was removed: the maintainer decides. Neither `zero_hopf`,
`res_cycles` nor `hill_cycles` has a published replacement (the 1:1, 1:3 and 1:4 resonances of cycles and the zero-Hopf point).

## W539: every AUTO-07p reference regenerated with the period, each difference explained ([#604](https://github.com/MuhammadMoustafa/xppautX/issues/604))

An `.auto07p` curve sample is now `[parameter, second parameter, norm, period]` (W538 needs the period of each sample to cut a
reference where ours ends); `tools/autocompare.py` reads four values for `.auto07p` and three for `.xppaut` and keeps the period apart. All
the 55 references were regenerated with `tools/auto_reference.py --program auto07p` (AUTO-07p 0.9.4) and every difference from the
committed files traced to one of these causes (55 files; a description with `auto07p_skip` keeps none):

| Cause | Files |
|---|---|
| the period only | 4: `bautin_near_hopf`, `bykov_hopf`, `lrz_cycles`, `pp2_heteroclinic` |
| a curve added where the file had none (the tool records one for every run; most files were written before it did) | 48 (all but the 7 of this row and the next) |
| multipliers added: the tool read the end of a special point's block only for a two-letter type, so a map's LPD, R2, R3 (and any point of a three-character or letter-digit type) had no spectrum; fixed in `tools/auto_reference.py` (`SUMMARY`) | 3: `hen_lp`, `hen_pd`, `hen_tr` |
| the start moved: a run starts from the first row of the description's `.diagram.csv`, which W495 (2026-10-26, a steady-state start refined by Newton) changed in 9 descriptions after their reference was written: `izh_class2` (start norm 66.178735 to 66.181911), `tstauto_eiz-hom`, `tstauto_wcbiwave`, `tstauto_san`, `ode_mu26_blocked`, `ode_mu26_control`, `explicit_homoclinic`, `lorenz_homoclinic`, `tstauto_torx_homcont`; the first point, and every number that follows it, moves by the start's shift (1e-5 for `izh_class2`) and the output grid and its labels with it | 9 (among the 48) |
| a fault of the tool: a HomCont description's equilibrium run started from its orbit's initial state (STPNT's integration at T = 0), not from our start; it now runs a second equation module, `eqs`, with the start | the 3 homoclinic ones above |
| a fault of the tool: it wrote a reference for a description with `auto07p_skip` (`global_snic`, `global_homoclinic`, `global_heteroclinic`, `ode_tstheti`, `tstauto_tstheti_homcont`, `tstauto_tsthom2`): it refuses them now, as it does `skip` | 0 files |
| a fault of the tool: `tools/odex_model.py` did not read a quoted set name (`set "vvst" {`), which refused `ode_lecar` and `tstauto_lecar` | 0 files (they ran) |

Two differences were real AUTO-07p behaviour with the moved start, and are kept as traced differences of their descriptions:
`tstauto_san`'s branch point at par0 = 1 is the end's (AUTO-07p's finding 9: the 50th step reaches Par Max 1 at 1.0000001, and AUTO-07p relabels it EP; its earlier
reference had it only because its start was 1.8e-4 off the equilibrium); `ode_mu26_blocked`'s cusp on `lp_soma` is dropped after ITMX iterations of a
test that is 6e-11 of noise (finding 10), and its neutral multiplier at the 0.1 pA mark is 1.00003 (1.00001 before; the exact 1) in the same finite-difference
way as the 1 pA mark's.

**Resolved (W538).** `ode_mu26_blocked`'s `to_snic` curve failed `tools/autocompare.py` against its new reference with the cut by the nearest sample in (parameter, norm), and also with the last sample whose period does not pass ours: our end lies between two reference samples (the snic at period 4974, the samples at 4212 and 5012), so a cut at a sample keeps one that lies past our end or drops the segment that reaches it. The cut now interpolates the reference's last segment at our period (W538 section); it passes.
## W538: a heteroclinic end on a coarse mesh ([#603](https://github.com/MuhammadMoustafa/xppautX/issues/603))

The pp2 demo's cycles (`pp2_heteroclinic`, NTST 90) end on the heteroclinic cycle between the saddles (0.1276, 0) and
(0.6518, 0), and the end is named `heteroclinic`. At NTST 15 (`pp2_heteroclinic_coarse`, the same run with `mesh_intervals` 15)
the same end was named `homoclinic`. The cause (traced by a temporary print of every mesh point's speed |f|, removed): the
global end looks at the mesh points only. At NTST 15 the orbit's slowest mesh point (speed 2.1e-7 against 0.22, which just
passes the 1e-6 criterion) lies at the saddle (0.6518, 0), and the saddle (0.1276, 0) is passed at speeds of 0.013 and more:
no mesh point of the coarse mesh is slow enough there, so `auto_slow_points` returned one point and one saddle was seen
(`auto_global_kind`: one saddle, homoclinic).

The fix (core/auto_global.cpp): the slow stretches are searched as before, and Newton's method is also started from every other
local minimum of the speed around the cycle. Such a minimum counts only for a saddle it converges to, and only when the root
lies within `auto_global_near_orbit` (1e-2, core/auto_global.h) of the orbit's extent from the minimum: a Newton jump further
is another equilibrium the orbit is nowhere near. The rest is unchanged: the 1e-6 criterion still decides whether the branch
is a global end at all, and a local minimum that converges to a focus or a node is ignored as before (it counts at NTST 90 only
through its slow stretches, so the focus near the cycle (1/3, 0.277) of this model does not make the end `end`).

With the fix, NTST 15 names the end `heteroclinic` (label 15, quota 0.7080698 against NTST 90's 0.7080728, the coarse
mesh's own position of the end). Measured on the way: the local minima converge to the two saddles at distances of 2e-6 to
4e-4 of the orbit's extent (0.78), while the same rule on ode_mu26_control's SNIC orbit lands 0.28 to 0.52 of its extent from
the equilibria it jumps to, so 1e-2 separates them with margin on both sides. Without the distance rule the mu26 SNIC became
`end` (its three far saddles added to the fold). The baselines are unchanged: `tools/autocompare.py` (71 models, 9 skipped,
0 failures, byte for byte), `tools/examples_check.sh` (215 of 216 match, ml-range.odex has no output by design), servercheck
(2681 checks). New baseline: `tests/auto/pp2_heteroclinic_coarse.diagram.csv`.

Its AUTO-07p reference (`pp2_heteroclinic_coarse.auto07p`, made with the built AUTO-07p and the period column, W539) is
compared with the cut of `stop_at_global_end`: our end is where the reference's period, along its samples, reaches our last
period, and the last segment whose period passes it is cut there by linear interpolation (the reference's period is not
monotone early on, so the last segment, not the first, is taken). Cut that way, the comparison passes except for one
difference (`differences`): AUTO-07p labels a fold of the cycles at point 93 (period 62.9, norm 0.42775, parameter
0.7080717), which xppautX does not label at this step size. The two runs share the constants (NTST 15, NCOL 4, DS 0.025,
DSMIN 1e-5, DSMAX 0.025, NMX 800, NPR 100, ISP 2, IAD 3, IADS 1, EPSL/EPSU/EPSS 1e-4, MXBF 0) and agree to 1e-6 in norm
through point 41.

**The cause, traced by W540 ([#605](https://github.com/MuhammadMoustafa/xppautX/issues/605)).** W538 took the difference for a
pair of folds inside one step; temporary traces of the fold test in cnrlbv and lcspbv showed otherwise. AUTO-07p's fold lies
between our points 91 and 92, where our test stays positive (2.9e-4, 8.6e-5) and the parameter rises: no test on stored points
sees it. At point 93 (93 is a multiple of IAD 3) the mesh is adapted, and the step 93 to 94 moves the parameter by -3.3e-6 and
the test from 1.1e-4 to 1.6e-3; a retake from point 93 at any step from 3e-3 down to 1.2e-5 lands on the same point, the
projection of the re-meshed profile. So at NTST 15 the mesh's own noise is larger than the folds' scale, and the difference
is recorded as that, a property of the coarse mesh. A rule that retakes a step whose parameter moves against an unchanged test
sign fired only on this noise (and on two steps of 1e-9 and 1e-10) and made the branch wander (261 points, 14 special points
against 6), so none was kept. Two real faults were found on the way, each its own card: after a located zero the next step's
test starts at 0 and cannot see a second zero (at DS 0.0125 a third fold between the two labelled ones is missed; W541,
[#606](https://github.com/MuhammadMoustafa/xppautX/issues/606)), and at DS 0.025 the step 94 to 95 changes the test's sign but
its location fails after ITMX iterations and the fold is dropped (W542, [#607](https://github.com/MuhammadMoustafa/xppautX/issues/607)).

## W541: a zero right after a located zero ([#606](https://github.com/MuhammadMoustafa/xppautX/issues/606))

Both locators (lcspae, lcspbv) store the test as 0 when a location converges, as AUTO-07p's LCSPAE/LCSPBV and XPPAUT's do
(docs/auto07p-findings.md 11, docs/xppaut-findings.md 101). The next step's sign test then rejects either sign against 0, so a
second crossing in that step is invisible. Confirmed by a temporary trace on `pp2_heteroclinic_coarse` with DS 0.0125 (a probe
copy of the description, not kept): fold A at parameter 0.7080724009, the discarded end q -4.0e-5, the next end +3.7e-4.

**Tried: store the discarded endpoint's test instead of 0** (`*q = saved_q` in both locators; the unit fixture's two paths
check it). The probe then labels three folds (A, B and the missing C, 0.7080722 to 0.7080731), so the blindness is real. But
the full `tools/autocompare.py` run changed three baselines, and none of the changes can be shown to be a zero:

- `tstauto_frcp`: a second HB at b = 0.74345 (HB 0.7377 is the equilibrium's only Hopf point; the (v, w) pair's real part is
  -0.99 there, from the Jacobian of the model). The test is the real part of the pair `auto_hopf_pair` picks: at 0.7434 the
  chosen pair changes (-0.98 to +1.0, the x-y pair's real part), a discontinuity of the test, and the location "converges" on
  it (q after location 1). The old stored 0 hid it by accident, in the one step where it happens; a dense run (DS 0.002) of
  the same description also loses it.
- `global_heteroclinic`: four cycle branch points at b = 0.3314461 to 0.3314462 next to the heteroclinic end, where the
  branch-point test's values are 1e13 to 1e17 and change sign erratically (the bordered determinant of an almost singular
  system); the baseline has none.
- `pp2_heteroclinic_coarse` (DS 0.025): the run stops at point 94 ("no convergence even at the smallest step"), after
  locations that converge with the test's noise (1e-5 to 1e-3, W540's mesh noise) as their value.

A fix that finds C must therefore tell a crossing of a smooth test from a jump or noise; the stored zero hides both, so
removing it alone is not enough. Not applied: the baselines stay as they were. A design that does is the follow-up (a located
zero checked by the test's value at the located point against its bracket, or a bracket for the next step that keeps the
discarded endpoint and the located point's true value, 0, apart).

**Resolved (W545):** the discarded end's test is kept, and the zero it brackets is accepted only as a crossing of a smooth test
(docs/auto-references.md W545 below).

## W542: a fold whose location fails ([#607](https://github.com/MuhammadMoustafa/xppautX/issues/607))

When a location ends at ITMX (or its corrector or test fails), lcspae/lcspbv restore the step end and the run goes on without
the point; the run already says so once (`Br 3 Pt 91: a fold (LP) between points 91 and 92 was not located (not in
max_location_iterations = 8 iterations); it is missing from the diagram`, in the AUTO log and `auto_stop_note`), which is the
"every stop says why" part of the card.

**Tried: retake the step from its start with half the step** (down to DSMIN; both loops, the test values and the point code
restored, the first approximation extrapolated as a retried solve does). On `pp2_heteroclinic_coarse` (DS 0.025) the retake
locates a fold at point 95 (parameter 0.7080717016, norm 0.41986, period 80.6), the parameter AUTO-07p's fold has (0.7080717,
at its period 62.9, norm 0.42775: the folds of this mesh-noise region are many, W540). But the run then wanders in the noise
(folds at 97 and 100, then no convergence at point 103) instead of ending on the heteroclinic cycle at point 96.

The full `tools/autocompare.py` (71 models) showed the retake is not harmless: 16 failures in five models. `ode_lecar` and
`tstauto_lecar` lose the cycle fold AUTO-07p has (0.248414) and label two branch points (0.24870 at point 22, 0.24843 at
point 29) that are noise of the branch-point test near the fold: the locations that were dropped before were such noise
brackets, and a shorter bracket locates them. `henon_rot_th` changes the order of R2 and BP, `pp2_heteroclinic` labels an LP
AUTO-07p does not, `global_heteroclinic` changes at point 45. None of these can be shown to be a zero of a smooth test, so
the code is not changed and no baseline moves. A retake needs the same first step as W541: telling a crossing of a smooth test
from noise or a jump (a located point whose test value is not small against its bracket; the test's own continuity), before
a failed location is given another try.


**Resolved (W545):** a failed location's bracket is bisected and what it ends on is accepted only as a crossing of a smooth test
(the W545 section below).

## W545: a special point's test tells a smooth crossing from a jump or noise ([#610](https://github.com/MuhammadMoustafa/xppautX/issues/610))

W541 (a zero right after a located zero) and W542 (a location that ends at ITMX) each found real folds and each labelled points that
are not zeros of a smooth test, so neither landed. Both now land, each only for the brackets a rule accepts. The rule, the cases that
chose it, what it measured over the 71 models and what it leaves out are below; the code is `locate_special_point`
(core/autlib1.cpp), `auto_crossing_smooth` and `auto_ratio_in_scale` (core/auto_c.h), the tests tests/test_auto_location.cpp.

**The rule.** A search leaves a bracket (the test's value and its arclength at two samples, opposite signs) and ends on a point. Its
slope across the step, `|q_end - q_start| / step_length`, and its slope across the last bracket of the search,
`|q1 - q0| / width` (a tolerance wide, or the width Mueller's method left), say whether the sign change is a crossing of a smooth test:
a smooth crossing has about the same slope at both scales; a jump or noise has a slope that grows as the bracket shrinks; a touching (a
double zero that the samples straddle) has one that shrinks. A new zero is accepted when the ratio of the two slopes is within a factor
(`auto_crossing_slope_ratio_max`, 3) either way. One rule for every test, equilibria and cycles. It needs no solve the search did not
make. Two parts are specific to a test's construction: a cycle's branch-point test, AUTO's `DET0/DET`, a ratio of consecutive
determinants (about 1 along a branch), is also required to be of a size a crossing within the step gives (`auto_ratio_in_scale`:
its values at the ends below the step's length in location tolerances, since beyond that an end lies within one tolerance of the
determinant's zero and the secant method on a ratio with a pole puts the zero a step away); and a zero found by the first secant step
(no search) is not accepted for a new bracket, since that step measures no slope (it lands within the tolerance of the end, which a
test at its noise does by chance).

**What the fixes do on it.** A: a located zero leaves the test at the discarded end of its step as the test's value (with its
arclength from the located point), where AUTO-07p stores 0, so a second zero ahead of it is a sign change of the next step; the
bracket's start is that sample. B: a search that ends at ITMX bisects the bracket Mueller's iteration holds (it stays a sign change,
and the state is at one end), down to the location tolerance, instead of giving the point up; only a bracket inside the step. Both
are accepted by the rule or restored exactly as for a point given up (the same saved state), and a refused zero is one note, like
`special_point_lost`: `Br 3 Pt 36: a fold (LP) between points 36 and 37 was not accepted at parameter 0.7080731095: its test is not
a smooth crossing: across the last 6.4e-05 of the search its slope is 51.9 times its slope across the step, more than 3 either way;
it is missing from the diagram` (servercheck's "stops AUTO refused zero"). The rule gates the new brackets only: a zero AUTO-07p's own
search converges on is accepted as before (below, what gating those changes).

**Candidates weighed, with the cases' numbers** (each from the trace of the locations, a temporary print in lcspae/lcspbv, removed):

- *The located point's own test value against the bracket's.* frcp's pair-switch jump ends on q = +1.0 against a bracket of -0.52 and
  +1.0 (the value is of their size), but pp2 coarse's real fold C has -6.1e-6 against -4.0e-5 and +3.7e-4, and the mesh-noise folds
  of the baseline (point 40: -2.9e-4 against -1.3e-4 and +7.3e-5) pass or fail it by the sign of the noise; global_heteroclinic's branch points end
  at 1.6e9 and 3.6e13 against brackets of 2.4e13 and 5.9e14 (6.5e-5 and 0.06 of them). No threshold separates them. Dropped.
- *Mueller's iterates converging geometrically.* The noise folds converge in two iterations as real ones do (baseline pp2 coarse
  points 35 and 40 in two, lecar's real cycle fold 0.248414 in two), and the jump converges (its bracket collapses on it). Dropped.
- *The Hopf pair's identity at both ends.* Right for frcp (the pair switches), and blind to every other test: the other jumps
  (global_heteroclinic's folds, pp2's) are not pair switches. The slope ratio finds frcp too (51.7). Not needed.
- *The branch-point test's magnitude.* Right for global_heteroclinic (1e13 to 1e18) and lecar's noise branch points (1e7 to 1e8),
  but the test is a ratio only for cycles; kept, as the one per-test part above, together with the slope ratio.
- *A noise floor tied to EPSS or EPSL.* The Newton tolerances bound a step's correction, not the test's noise: at the end of a long
  cycle branch the parameter moves by 1e-12 per point against EPSL 1e-4, and every fold test there is below its own noise. No floor
  built from them separates the real fold C from the noise next to it. Dropped.

**Measured** (the 71 models of tests/auto at their own settings; `tools/autocompare.py` 0 failures before and after). The slope
ratio, final over step, of every located zero of the baseline: 171 of 182 lie in 1/3 to 3 (the ordinary tests' real zeros, 0.7 to
2.3), 11 above, the fold curve's cusp (3.1), two PD/TR tests (4.9 to 10.7) and the baseline's own noise (pp2 coarse 3.2 to 34, global_heteroclinic
5.6 to 23). The new zeros, with 390 brackets searched against 382 before:

| model, branch | point | test | path | ratio | |
|---|---|---|---|---|---|
| tstauto_frcp, equilibria | 38 | Hopf | after a located zero | 51.7 (jump) | refused |
| global_heteroclinic, cycles | 30, 32 | fold | ITMX, bisected | 692, 986 (jump) | refused |
| global_heteroclinic, cycles | 30 | branch point | after a located zero | values 2.4e13, -5.9e14 against 500 | refused |
| henon_rot_th, turn | 33 | branch point | ITMX, bisected | 1.1e-6 (touching) | refused |
| pp2_heteroclinic_coarse, cycles | 36, 41 | fold | after a located zero | 51.9, 29.7 (noise) | refused |
| pp2_heteroclinic_coarse, cycles | 94 | fold | ITMX, bisected | 112 (jump at the step's start) | refused |
| ode_lecar, tstauto_lecar, pp2_heteroclinic | 20, 23, 24 | branch point | ITMX | the iteration left the step | as before, given up |
| pp2_coarse_second_fold (new) | 49 | fold | after a located zero | 1.02 | accepted |
| pp2_coarse_bisected_fold (new) | 80 | fold | ITMX, bisected | 1.76 | accepted |

The cases the card named come out right: frcp's jump, global_heteroclinic's new branch points and pp2 coarse's new noise folds are refused;
the real cycle fold of lecar (0.248414) is a baseline zero and stays; henon_rot_th's branch point, which the bisection locates in front of the resonance R2 (3.14153626,
then R2 is not labelled, which AUTO-07p does: R2 and BP at 3.14159132), is a touching and refused, so the baseline keeps R2 and BP as AUTO-07p labels them. No baseline moves. The constant 3 sits above the accepted new zeros'
1.02 and 1.76 and the ordinary baseline range (0.7 to 2.3) and far below the nearest refused ratio (29.7); a PD/TR zero or a cusp new to
a search could be refused (a label missing, never an extra one).

**Real new zeros, proved** by the parameter's turning, which is independent of the test (a fold is where the parameter along the
branch stops rising and falls; the stored points on both sides show it). Two descriptions of AUTO-07p's pp2 demo (Doedel 1984) at
NTST 15, the mesh of pp2_heteroclinic_coarse, keep them. `pp2_coarse_second_fold` (step 0.0125, Max Steps 50): the parameter along the
cycles at points 47 to 50 is 0.70807234, 0.70807240 (a fold AUTO-07p's own search labels, not a turn), 0.70807272 (the new fold C, the maximum) and 0.70807239; the
old code stores 0 after the first fold and does not see C. `pp2_coarse_bisected_fold` (step 0.0075, Max Steps 86): the search between points 79 and 80
uses up its eight iterations (the test is flat and creeps, -1.4e-5 to +6.9e-7) and AUTO-07p gives it up; bisected it ends at 0.70807259, the parameter's maximum
(0.70807259 at point 79, 0.70807250 at point 81). Neither is in AUTO-07p's own reference (it shares the blind spots, and at this mesh the two programs'
histories differ: W540). W547 checked the ODE's own cycles: pp2 has no fold between its Hopf point and the heteroclinic cycle, so these are
turns of the NTST 15 discretization's branch, not folds of pp2 (the W547 section).

**What it leaves out.**

- *The rule does not gate a zero AUTO-07p's own search converges on.* Gating those too (a temporary experiment, every located zero
  with at least one iteration) changed six models and lost real zeros: forced_pp_ns and forced_pp_pd (their PD/TR points, which AUTO-07p labels
  too), henon_rot_th (the BP after R2), ode_mu26_blocked (the cusp, ratio 3.1), global_heteroclinic, and made pp2_heteroclinic_coarse stop at
  point 94 with no convergence. The baseline's own noise labels (pp2 coarse's folds at points 29 and 41, where the parameter keeps rising; the branch point at 31, where the
  parameter is at its maximum; global_heteroclinic's two branch points) stay as they were: no rule found here removes them
  without removing a real one, and they are AUTO-07p's own.
- *The last stretch before a global end.* On NTST 90 the parameter moves by 1e-12 over the last points and every fold test there is
  below its noise; the old path labels folds there already (pp2_heteroclinic at step 0.0375: a fold at period 74.9 where the parameter
  still rises). A and B add nothing there since the first-secant rule: before it, one such fold changed where the global end was
  found and the run went on to label three more.
- *The NTST 15 mesh near the heteroclinic cycle is chaotic, and its folds are the adapted mesh's.* With Mesh Adapt Every 0 the same cycles
  have one fold, at 0.70804468, and none near 0.70807, at the steps 0.0075, 0.0125 and 0.025, old code and new alike (the same diagrams, each
  ending on the heteroclinic cycle): the folds near 0.70807 are what the mesh adaptation every third point makes of the branch (W540), and a run's passage depends on every point it inserts, since the adaptation
  lands on other points. Of 16 steps from 0.006 to 0.03 the old code completes 10 and the new 7: at 0.009, 0.012 and 0.0125 the new
  code locates a fold the old does not and the next adaptation, in the long-period part of the branch, does not converge ("no
  convergence even at the smallest step"), where the old run ends on the heteroclinic cycle before reaching it. A perturbation
  study (174 runs: every description's steps scaled by 0.5, 0.75 and 1.5) changed two, both pp2 at NTST 15: this one (step 0.0125,
  no convergence) and tstauto_pp2 at the same step, which runs on past the old end and labels more.

**Cost** (callgrind, docs/studies/w545-speed.md): P1, E1, P2, E2 and T1 +0.00% to +0.10%; the 62 runnable descriptions at one thread
+0.04% in all; only a model that searches more (global_heteroclinic +11%, 16 solves) pays.

## W547: every bracket of a cycle's branch-point test is gated, and what no rule can gate ([#612](https://github.com/MuhammadMoustafa/xppautX/issues/612))

W545 gated only the zeros new to AUTO-07p. W547 asked for every located zero to pass the rule, no real point lost, each point checked by
a method independent of xppautX and AUTO-07p both. One part lands; the rest is shown impossible with the slopes a search measures.

**What changed.** A cycle's branch-point test (AUTO-07p's FNBPBV, `AP%DET/DET`, a ratio of consecutive determinants:
[toolboxbv.f90:404](../reference/auto-07p/src/toolboxbv.f90#L404)) brackets a zero only while its values at the ends of the step are of
a size a crossing within the step gives (`auto_ratio_in_scale`), now for every bracket, AUTO-07p's own included; a bracket beyond it is
refused before any search, with a note (`its test is a ratio of consecutive determinants, and its values ... are beyond what a crossing
within it gives`). Over the descriptions at their steps and at 0.5, 0.75 and 1.5 times them (the 64 that run, 4 x 64 runs), the real
branch points' values reach 244 times the step's length (res_cycles at 1.5 times), the refused ones 2.7e7 and more (pp2 at NTST 15
2.7e7 to 3.6e9, lecar 1.6e9 and 1e10, global_heteroclinic 4.9e14 and 9.2e16); the threshold, one over the location tolerance, is 1e4 at
special_point_tolerance 1e-4. Where the rule now stops a search before it starts, in lecar's two, pp2_heteroclinic's and
pp2_coarse_bisected_fold's, the search gave up at ITMX before: the same diagram, a note that says why, fewer solves.

**The independent check.** Both models whose diagrams change are planar, where a cycle has one nontrivial Floquet multiplier,
m = exp(integral over the period of div f): a fold or a branch point of cycles needs m = 1 (a sign change of log m), a period doubling
does not exist. Two scratch scripts compute the ODE's own cycles (not kept: they serve these two models only): global_heteroclinic's by
scipy's `solve_bvp` (tolerance 1e-9, up to 366,588 mesh nodes; continued in the amplitude, then in the period), pp2's as the fixed points of
the Poincare return map on u0 = 1/3 (DOP853 at rtol 1e-13, brentq on the log of the crossing height), log m by quadrature of div f.

| model, period | ODE parameter | ODE log m |
|---|---|---|
| global_heteroclinic, 15.78 / 16.79 / 17.80 | 0.3314609 / 0.3314707 / 0.3314667 | -0.32 / +0.005 / +0.34 (the fold, at the parameter's maximum) |
| global_heteroclinic, 25.9 / 30.9 / 37.0 / 40.0 | 0.33144651 / 0.33144621 / 0.33144619 / 0.331446193 (to the heteroclinic cycle at 0.3314461934) | +3.0 / +4.7 / +6.7 / +7.7, rising |
| pp2, 10.44 / 13.44 / 21.6 / 38.7 | 0.672 / 0.69 / 0.705 / 0.708 | -0.006 / -0.40 / -1.6 / -4.6 |
| pp2, 53.4 / 68.2 / 80.2 | 0.70807 / 0.7080727 / 0.7080728 | -7.4 / -10.2 / -12.5, falling |

global_heteroclinic's cycles have one fold, at 0.3314707 (period 16.8), and pp2's none between the Hopf point (0.6715938, where log m is 0)
and the heteroclinic cycle: the period grows and the parameter rises monotonically to its end. Every other fold or branch point on these
branches is the discretization's.

**Per point** (xppautX before is master's tip; AUTO-07p is 0.9.4 at the description's constants, built from reference/auto-07p in a scratch copy):

| model (mesh), point | xppautX before | xppautX now | AUTO-07p | independent | |
|---|---|---|---|---|---|
| global_heteroclinic (30), fold at 0.3314707, T 16.77 | LP 25 | LP 25 | LP 25 | log m crosses 0 at 0.3314707 | real, kept |
| global_heteroclinic (30), 0.33144612, T 31.0 | BP 30 | refused, note | BP 30 | m = e^4.7 = 110 (and no ODE cycle below 0.3314461934) | spurious, removed |
| global_heteroclinic (30), 0.33144607, T 37.5 | BP 32 | refused, note | BP 32 | m = e^6.6 | spurious, removed |
| global_heteroclinic (30), the end | HEC 34 at 0.33144663, T 58.4 | HEC 32 at 0.33144432, T 58.3 | BP 35, 37, 39, 41, 43, LP 44, 49, BP 52, 56, MX 58 (T 7829) | heteroclinic cycle at 0.3314461934 | both ends a coarse mesh's |
| global_heteroclinic (90), 0.33144657, T 25.6 | BP 29 | refused, note | BP 29 | m = e^3.0 | spurious, removed |
| global_heteroclinic (90), T 28.9 and 42.3 | LP 30, LP 32 | not labelled (another path) | LP 30, 32, then 19 more LP and BP to T 2073 | m = e^3.7, e^5.6 | spurious |
| global_heteroclinic (90), the end | HEC 34 at 0.3314461933 | HEC 32 at 0.3314461932 | MX 100 | 0.3314461934 | |
| pp2 (15), step 0.025, 0.70807364, T 94.4 | BP 31, then back along the cycles to the Hopf point (LP 69 there), LP 36, 41, HEC 96 | refused, note; the run then stops, no convergence even at the smallest step (point 46) | BP 31, LP 36, 41, 70 (the Hopf point), 93, MX 100 | no ODE cycle at this parameter (above 0.7080728); m < 4e-6 at T 80 | spurious |
| pp2 (15), folds near 0.70807 at every step | labelled | labelled | labelled | no fold | the mesh's (below) |
| lamom2, step 1.5 times, fold at a = -0.25 | LP | LP | (not run) | exact: a = s^2 - s, s = r^2, minimum -1/4 | real; its slope ratio is 5.5 |
| forced_pp_ns, PD at m = 0.46243 | PD 87 | PD 87 | PD | (x, y) multipliers -0.99996, -1.021 at 0.46243; -0.951, -1.075 at 0.47049 (DOP853, fsolve) | real; its slope ratio is 8.9 |
| forced_pp_pd, R2 at (eps, b0) = (0.55102, 0.34013) | R2 26 | R2 26 | R2 | multipliers -1, -0.9972 at point 25; -1.000008 +- 4.7e-5 i at 26; -1, -1.015 at 27 | real; its slope ratio is 10.7 |

**pp2 at NTST 15.** The folds a coarse run labels near the heteroclinic cycle are turns of the discretized branch, not of the ODE's:
at period 80 the NTST 15 branch is at 0.70807328, the ODE's at 0.7080728, an error larger than the folds' own spread; with
Mesh Adapt Every 0 the same branch has one fold, at 0.70804468, where the ODE has none either. So W545's two descriptions
(pp2_coarse_second_fold, pp2_coarse_bisected_fold) show the location of a turn of the discretized branch (the parameter turns along
the stored points), not a fold of pp2; AUTO-07p labels folds there too (its LP 27, 29, 36, 41, 93 at step 0.025). A run's passage there
is chaotic: of 16 steps from 0.006 to 0.03 master completes 8 and this card 7, the same labels at the other 15; at 0.025 master's run
labelled BP 31, turned back along the cycles through the Hopf point (a "fold" at 0.6715940, period 10.39, where the cycles shrink to the
equilibrium) and ended on a "heteroclinic cycle" at 0.70806976, 3e-6 from the ODE's; now the branch point is refused and the next
steps, on cycles of period 70 to 85 the 15 intervals do not resolve, stop with "no convergence even at the smallest step", which says why.
The ODE's cycles exist and are strongly stable there (m = e^-10): the failure is the mesh's, not ours; at NTST 90 (pp2_heteroclinic)
the run ends on the heteroclinic cycle at 0.7080728, the ODE's. pp2_heteroclinic_coarse now runs at step 0.0225 (both builds: one fold, the mesh's, as
AUTO-07p's LP 27; the end named heteroclinic, the description's purpose), with a new AUTO-07p reference at that step.

**What no rule gates** (step 4 of the card). The slope rule (W545, factor 3) gating AUTO-07p's own zeros loses real points, for every
kind, in the measured runs (the 64 descriptions at their steps and at 0.5, 0.75 and 1.5 times them, every bracket traced):
- folds: lamom2's fold at a = -1/4 (exact) at 1.5 times the step has ratio 5.5; global_heteroclinic's spurious folds 5.6 and up,
  pp2_heteroclinic's (NTST 90, near its end) 3.5 and 66, pp2 at NTST 15 3.2 to 116: real and spurious overlap from 3.2 to 5.6;
- PD and TR: forced_pp's real PD and R2 at 4.1 to 10.7 (the test is the distance from the unit circle of the multiplier picked nearest
  it, and the pick switches where a pair meets the real axis, as at 0.46243 above);
- the cusp: Mu26's at 3.1 (its test at its noise floor, 1e-12, finding 2);
- a map's branch point: henon_rot_th's at R2, 2.7e-7 (the step starts on the test's zero, at the R2 point).
Changing a threshold or measuring the slope over the last two brackets (a smooth test's bracket slopes converge) were tried on the same
traces: the second refuses forced_pp_pd's R2 (10.7) and henon_rot_th's branch point. So W545's rule stays on the new zeros only, and
the branch-point scale rule, whose real and spurious values are five decades apart, on every bracket.

## W549: ModelDB 267710's periodic run makes no fold search ([#614](https://github.com/MuhammadMoustafa/xppautX/issues/614))

W548 found `tools/auto-profile/modeldb_check.sh` (W429's guard) failing: the 20-point periodic run of
`examples/ode/motoneuron6c.odex` made 0 fold searches where W429 expected 1, in the default, LIBM=system and LAPACK builds,
with the grabbed Hopf point at ampPar 0.5727816 where W412's runs had 0.5727871. Two cards, both with their reasons in
this file and the studies, explain both numbers; an independent check settles which behaviour is right.

**Why the numbers changed.** The Hopf point: W358's and W412's 0.5727871 came from the steady run before W421 (the equilibria's
tests read at the converged point; [docs/studies/w358-auto-profile.md](studies/w358-auto-profile.md): 5.45e-6 from where AUTO-07p
puts it, which made the spurious searches near the start); W425's run after W421 already had 0.57278166, 8.5e-10 from AUTO-07p's.
The search: W429's expected one was AUTO-07p's own, the fold test's sign change between the second and third point of the cycles
(W431's "fold of cycles"), and [W458](#w458-the-first-steps-of-cycles-from-a-hopf-point-521) removed it: it factors the first two
steps' points again at the converged point and orients the start's phase condition by DS, and its commit says
`ode_motoneuron6c` "loses the LP at the third point of its cycles". At W543's a0710479 (the first tip the ported drivers run on)
the run already makes 0 searches. Older tips could not be run (the drivers speak the old protocol), so the first tip without the search
is W458's by its own record, not by a run of its parent.

**The independent check** (numpy/scipy over the right-hand side AUTO-07p's `6c.c` holds, compiled apart from either program's solver;
Newton continuation of the equilibria in ampPar, eigenvalues of a central-difference Jacobian):

| | Hopf ampPar | the first periodic point |
|---|---|---|
| check (leading pair crosses, Re -2.8e-10, omega 1.2858) | 0.5727814553 | |
| xppautX now | 0.5727816610 (2.1e-7 from the check, inside the finite-difference Jacobian's error of 1.4e-6, above) | 0.57278166098 |
| AUTO-07p 0.9.4 (paper/benchmarks/modeldb-267710/auto07p, NMX 20) | 5.72782e-1 (grab 0.5727817) | LP at point 3, 0.572782 |
| xppautX in W412's runs (before W421) | 0.5727871 | searches near the start |

On the cycles, `(a - HB) / A^2` with A half the range of the first state's maximum and minimum over the cycle is 5.149e-4, 5.150e-4,
5.151e-4 at points 2, 3 and 4 (the normal form's constant), 5.19e-4 at point 10 and 5.34e-4 at point 20: the parameter is a smooth,
monotonic function of the amplitude, so there is no fold of cycles in the 20 points, and the LP AUTO-07p labels at point 3 (and the search
W429 expected) is the start's artefact (the first point sits 2e-7 from the true Hopf point, so the second is reached with the
parameter dipping 5.6e-8). Floquet multipliers from the monodromy matrix were not computed: LSODA failed on the perturbed gating variables
of this model, and the monotonic law needs no second run.

**The decision.** The expectation was wrong and the new behaviour is right: `modeldb_check.sh` now expects 0 searches and 0
backward steps in the three builds (W549). Nothing in the core changed; the guard still catches a build that loses W458's rule or
the start's accuracy.


## W550: XPPAUT's marked to_snic run of Mu26 depends on its start ([#615](https://github.com/MuhammadMoustafa/xppautX/issues/615))

The W354 series of 2026-10-30 found `ode_mu26_control` and `ode_mu26_blocked` failing their gate: "traced difference not present
any more" for XPPAUT 8.0's marked to_snic run (W437), and the curve 0.55 off.

**What changed.** Nothing XPPAUT is given but its start. The binary, the settings dialogs (the driver checks every AutoNum value and
the mark count against the b-file's constants) and the marks are the same; the W437 driver and description, run again from
a copy of that tree, reproduce W437's stored reference exactly (LP 0.9990257 pA, a second 5 pA mark, EP 18.30 pA). The start is the
first point of the description's diagram baseline (`first_point`), the equilibrium at 0 pA, and that moved between W437 and today
(V -70.30083920658 to -70.30066290344 mV, every state by 1e-6 to 1e-4 relative): the model.ode files XPPAUT is given differ only in
the 28 `init` lines. XPPAUT's marked run is not stable under that: a failed location of the 1 pA mark leaves the displaced iterate
installed (finding 47), and where the branch turns afterwards depends on it.

| run | W437 start | today's start |
|---|---|---|
| control, XPPAUT | UZ 5, LP 0.9990257, UZ 5, EP 18.30 | UZ 5, LP 0.09994824, UZ 0.9999985, EP 4.54 |
| blocked, XPPAUT | UZ 5, UZ 0.9999987, LP 0.1009347, LP 0.07001995, EP 0.0805 | UZ 5, UZ 0.9999991, LP 0.1010236, EP 0.84 |
| xppautX, either start | UZ 5, 1, 0.1, 0.07, SNIC | the same |

**The independent check** (`docs/studies/floquet_check.py`): the right-hand side is the model's own equations (the Fortran FUNC the
AUTO-07p reference is given, with exact derivatives from `paper/benchmarks/auto/auto07p/exact_source.py`, which W459 checks against
Richardson differences), the cycle is found by Newton's method on the return map to the section V = -20 mV, and the monodromy matrix is the
solution of the variational equations over its period (LSODA, rtol 1e-11); no continuation, no collocation, no finite-difference
Jacobian in the multipliers. A second monodromy by central differences of the flow is printed beside it.

| run, SmIapp (pA) | period | multipliers, exact Jacobian | difference Jacobian's flow |
|---|---|---|---|
| control 4.999999 | 281.73353 | 0.99999999, 0.0932, 0.0917, 0.0300 | 0.998, 0.0932, ... |
| control 0.09994824 (XPPAUT's LP) | 949.92713 | 1, 4.2e-4, 3.6e-4, -7.8e-6 | 0.908, 4.4e-4, ... |
| control 0.9990257 (W437's LP) | 504.12584 | 0.99999997, 0.01569, 0.01483, 0.00186 | 0.996, 0.0157, ... |
| control 0.5 | 617.09845 | 0.99999993, 0.0061, 0.0059, 4.5e-4 | 1.011, ... |
| control 4.539555 (XPPAUT's end) | 294.13126 | 0.99999999, 0.0845, 0.0829, 0.0258 | 1.0005, ... |
| blocked 0.1010236 (XPPAUT's LP) | 1359.66259 | 0.99999986, 1.1e-5, 1.1e-5, -1.2e-6 | 1.05, ... |
| blocked 0.1009347 (W437's LP) | 1360.65798 | 1.0000004, 1.1e-5, 1.1e-5, -1.5e-6 | 1.29, ... |
| blocked 0.07001995 (W437's LP) | 2710.46870 | 0.99999447, -2.1e-6, -4.4e-10, 2.3e-10 | 2.96, ... |
| blocked 0.8399 (XPPAUT's end) | 560.48803 | 0.99999992, 0.0100, 0.0099, 9.2e-4 | 1.029, ... |

The trivial multiplier is exactly 1 and the others are far inside the unit circle at every parameter, XPPAUT's own listings agree
(0.0932108, 0.09171, 0.03004 at 5 pA), and the difference Jacobian's flow is off in the trivial one by as much as a factor of 3 over a
period of 2710: the same sensitivity W437 traced. All five XPPAUT LPs (both starts) are therefore labels where a cycle has no fold
and the branch is a smooth monotone descent; no label of xppautX's is in dispute, and its run is the same with the exact Jacobian
(`make`'s default and `jacobian exact`: control UZ 4.999999970, 0.9999999355, 0.0999996694, 0.0699999969, SNIC 0.0063386; blocked the same
four marks, SNIC 0.066765; identical labels to the difference run). AUTO-07p has no exact-Jacobian row for either description
(`EXACT_AUTO07P` names P1, E1, P2, E2, T1), none was added.

**The decision.** The new run is XPPAUT's behaviour for the description's settings, from the start the baseline gives, and the
reference was stale, not the driver wrong: `tests/auto/ode_mu26_{control,blocked}.xppaut` are regenerated
(`tools/auto_reference.py --program xppaut --only ode_mu26_control,ode_mu26_blocked`, 25 minutes, the lpc runs
still recorded as unavailable), and with the tool's current format they also keep the curve of every one-parameter run. The traced
entries follow the new run: ours UZ 3 and 4 and XPPAUT's LP are missing in the other program (the 0.1 and 0.07 pA marks it never reaches, the
spurious LP), the 1 pA mark's neutral multiplier differs in the control run (XPPAUT 1.000055, ours 0.999861, both the finite
difference's: exact 1) and no longer in the blocked run (the entry W474 added is removed). A curve of XPPAUT's that reverses
and stops short has no counterpart to our samples below its turn; autocompare's curve comparison cannot trace that, so a description now
has `curve_runs_on` (reference, run, cause): the samples of ours off that reference's curve are the difference, and the entry says it is gone when none is.
The reference's own samples are still all checked against ours, including its way back up. Finding 73 holds the new native run.

**Gates.** `tools/autocompare.py`: 73 models, 0 failures. The comparison's pilots (`continuation.py --pilots --only ode_mu26_control,ode_mu26_blocked`):
the gates against XPPAUT and AUTO-07p pass for both; the converged exact pilot is unavailable for the control run ("no convergence even at the smallest
step" at tolerance 1e-10) and the blocked one's stage after the gate stops on a harness error: the XPPAUT pilot's reference.json leaves
out the lpc runs (`lpc_soma`, `lpc_dend`) that `checked_reference` wants for the converged comparison (a gap of the drivers, not of this card; not changed).

## W551: P1's branch point at the third point of PY_S1Bf's cycles ([#616](https://github.com/MuhammadMoustafa/xppautX/issues/616))

The W354 series' pilot of P1 (PY_S1Bf: `Iapp` from 20000 pA down to the Hopf point at 16409.41, then the cycles from it, DS -1,
NTST 100) failed its gate: XPPAUT 8.0 labels a BP in the periodic run and xppautX labels nothing. AUTO-07p labels it too
(point 3, 16394.432, period 2.16155; XPPAUT point 3, 16394.43). Neither is real.

**The independent check** (`docs/studies/floquet_check.py --odex PY_S1Bf.odex --c paper/benchmarks/auto/auto07p/py_s1bf.c`): PY_S1Bf's
hand-written exact Jacobian (W269, checked against differences by jac_check.c), the cycle by Newton's method on the return map to a
section of V, the monodromy matrix from the variational equations (LSODA, rtol 1e-11), and, beside it, a central-difference monodromy.

| Iapp | period | multipliers, exact Jacobian | central-difference flow, the second |
|---|---|---|---|
| 16409 | 2.15755 | 1.0000708 +- 1.4e-4 i, 0.97738, 0.89774 | 1.0000644 +- 1.6e-4 i |
| 16400 | 2.16007 | 1, 0.99679, 0.97736, 0.89762 | 0.99679 |
| 16394.43 (the BP) | 2.16156 | 1, 0.99490, 0.97734, 0.89756 | 0.99489938 |
| 16392.93 | 2.16196 | 1, 0.99439, 0.97734, 0.89754 | 0.99435542 |
| 16380 | 2.16542 | 1, 0.99003, 0.97731, 0.89738 | 0.98994748 |
| 16358.65 | 2.17122 | 1, 0.98286, 0.97732, 0.89712 | 0.98289094 |
| 16300 | 2.18762 | 1, 0.97692, 0.96418, 0.89639 | 0.97691785 |

At the Hopf point (16409.41) the second multiplier is exactly 1 (e^(i w T) = 1: the amplitude direction), so a family born there starts
with it at 1, and the pair at 16409 (0.41 from it) is that double root split by the integration error (about the square root of 1e-8).
Beyond it (1 - m)/(16409.41 - Iapp) is 3.41e-4, 3.40e-4, 3.40e-4, 3.39e-4 and 3.38e-4 (16400 to 16358.65): the multiplier leaves 1 linearly
from the Hopf point, as the normal form says, and does not return: no branch point of cycles. XPPAUT's and AUTO-07p's own listings at the BP
(0.9949, 0.9773, 0.8976) are the exact ones, so the differenced Jacobian does not explain the label; what does is the start, as in
[W458](#w458-the-first-steps-of-cycles-from-a-hopf-point-521): the label sits at the third point of a run with DS < 0 and the
multiplier near 1 makes the branch-point test, a ratio of determinants, ill-conditioned at the first points (xppautX refactors them at the
converged point and orients the phase condition by DS). That DS < 0 is the cause in XPPAUT (finding 86) and AUTO-07p (their finding 8) is
consistent with all of it and was not isolated by a DS > 0 run.

**Labels by Jacobian** (the pilots): xppautX with the difference Jacobian and with the exact one: none (its W458 start); AUTO-07p with the
difference Jacobian: BP at 16394.431997; AUTO-07p with the hand-written exact derivatives (JAC 1): the same BP at 16394.431997, so differencing is
not what labels it, and the exact-Jacobian pilot passes its gate against xppautX's converged run only because the stored points 2 and 3 of a branch
of cycles started at a Hopf point are not compared (`start_artefact`, W458); XPPAUT (difference only, it takes no derivatives): BP at 16394.43.
The BP is a start artefact of the programs' first steps, as W458 found for motoneuron6c, not of the Jacobian.

**Gates.** P1's `periodic` run now has both traced references (`differences` in `workloads()`, `.xppaut` and `.auto07p`, reference side, BP 1);
`continuation.py --pilots --only P1`: the gates against XPPAUT and AUTO-07p, the converged exact pilot and xppautX's and AUTO-07p's exact pilots against
it pass; P1's fixed-step row (`--pilots` stops before it: the same description with DS and DSMIN = DSMAX = 1, NMX 100000, run by a script that
calls `workload(..., 'fixed')` on it) passes its gates too (the XPPAUT run takes 7 minutes).
