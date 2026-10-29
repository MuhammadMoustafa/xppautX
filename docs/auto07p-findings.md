# AUTO-07p findings

Bugs, wrong results, crashes, reads past an array and arbitrary limits found
in AUTO-07p itself while comparing it against xppautX, for the paper
(maintainer, 2026-10-14; [W367](https://github.com/MuhammadMoustafa/xppautX/issues/429)).
Each entry: what AUTO-07p does, the evidence (the model, the run, the
numbers), the lines in AUTO-07p's own source, what xppautX does instead, and the
card and commit. Add an entry whenever a card finds one; keep the evidence
reproducible.

How this file differs from related documents:
- Bugs found in XPPAUT's AUTO (AUTO2000, as XPPAUT ships it) stay in
  [xppaut-findings.md](xppaut-findings.md).
- Where xppautX deliberately differs from AUTO-07p (a method or a tolerance
  chosen differently, not a defect) stays in
  [auto-references.md](auto-references.md).

Each finding cites the lines in AUTO-07p's own source, never our refactored
files, so it is clear the behaviour is AUTO-07p's and not ours. A citation is a
relative link into the local copy kept out of git (`reference/` is in
`.gitignore`): `reference/auto-07p/` (AUTO-07p 0.9.4, git `44cc1c4`, 2025-06-26).
A citation reads `[file:line](../reference/auto-07p/src/file#Lline)`.

The entries are sections, not table rows: most need a paragraph of evidence.
The index gives one line each; its number links to the entry's section, and
every card (to its GitHub issue) and commit (to its GitHub commit) is a link,
in the index and the entries alike.

When the file was made (W367, 2026-10-14), the AUTO studies
([W355](studies/w355-auto07p-differences.md),
[W356](studies/w356-auto-location-07p.md),
[W358](studies/w358-auto-profile.md)) and
[auto-references.md](auto-references.md) noted no defect of AUTO-07p's own,
so the index started empty.

| # | Area | Finding | Card |
|---|---|---|---|
| [1](#1-an-equilibriums-period-depends-on-the-spectrums-order-outside-the-conservative-band) | Equilibria | equally damped complex pairs give a period that depends on the eigenvalue routine's order | [W369](https://github.com/MuhammadMoustafa/xppautX/issues/431) |
| [2](#2-a-newton-residual-contaminates-cp-and-gh-test-functions) | Equilibrium curves | a second difference that retains the equilibrium residual produces spurious CP/GH labels, and its noise next to a cusp can leave the cusp unlocated | [W364](https://github.com/MuhammadMoustafa/xppautX/issues/426), [W423](https://github.com/MuhammadMoustafa/xppautX/issues/485) |
| [3](#3-point-numbers-wrap-after-9999) | Output | printed point numbers repeat after 9999 although the continuation count and labels continue | [W384](https://github.com/MuhammadMoustafa/xppautX/issues/446) |
| [4](#4-a-torus-curves-result-depends-on-the-start-amplitude-of-its-eigenfunction) | Torus curves of cycles | R1 and R2 on res_cycles are found only from one start amplitude of the eigenfunction; 3 times larger and the run fails at its second point | [W405](https://github.com/MuhammadMoustafa/xppautX/issues/467) |
| [5](#5-a-torus-curves-resonance-test-is-blind-once-its-angle-leaves-0-to-pi) | Torus curves of cycles and maps | the R1-R4 test looks at the continued angle only between 0 and pi: past pi a curve crosses R3, R4 and R1 unlabelled | [W405](https://github.com/MuhammadMoustafa/xppautX/issues/467) |
| [6](#6-a-fold-or-flip-curves-resonance-test-gives-0-when-its-double-multiplier-splits) | Fold and flip curves of cycles | at R1/R2 roundoff splits the double +1/-1 into a conjugate pair; the test then returns 0 and the location takes it for the zero | [W405](https://github.com/MuhammadMoustafa/xppautX/issues/467) |
| [7](#7-the-finite-difference-jacobian-steps-every-state-by-the-largest-ones-scale) | Equilibria | the finite-difference Jacobian (JAC=0) steps every state by 1e-7 times the largest state, so a model with states of very different size has a Jacobian, and a Hopf point, that is off by an amount no tolerance changes | [W455 / #518](https://github.com/MuhammadMoustafa/xppautX/issues/518) |
| [8](#8-the-first-comparison-of-cycles-from-a-hopf-point-reads-a-predictors-fold-test-and-a-misoriented-phase-condition) | Cycles from a Hopf point | between the second and third point the fold test comes from the eigen-cycle predictor's Jacobian and, with DS < 0, the branch-point test's determinant from a phase condition of the wrong orientation: spurious LP or BP labels next to the start, and a real fold there missed or misplaced | [W458 / #521](https://github.com/MuhammadMoustafa/xppautX/issues/521) |
| [9](#9-a-special-point-located-on-a-branchs-last-step-is-relabelled-as-its-end) | Branch ends | a branch point, fold or Hopf point located on the step that also passes Par Min/Max, Norm Min/Max or NMX is labelled EP, so the special point is lost | [W495](https://github.com/MuhammadMoustafa/xppautX/issues/559) |

## 1. An equilibrium's period depends on the spectrum's order outside the conservative band

AUTO-07p's `FNHBAE` stores `PAR(11) = 2*pi/|imag|` from the complex
eigenvalue nearest the imaginary axis
([equilibrium.f90:565-570](../reference/auto-07p/src/equilibrium.f90#L565)).
Inside its conservative band (even dimension, trace below `HMACH` times
the largest entry, real parts within 1e-5 of zero) its sort orders tied
pairs by frequency, so the lowest frequency is chosen
([equilibrium.f90:437-475](../reference/auto-07p/src/equilibrium.f90#L437)).
Outside it, the sort compares real parts alone with `>=`, and the search
keeps the last eigenvalue with the smallest `|real|` (`AR.LE.AREV`,
[equilibrium.f90:477-499](../reference/auto-07p/src/equilibrium.f90#L477)).
Two pairs with equal real parts, such as two equally damped modes, are
then picked by the order LAPACK returned them in, and their real parts
differ by roundoff, so which period is stored is not defined by the
problem.

- **Evidence:** from the source; not run in AUTO-07p. The same selection
  in xppautX (XPPAUT's AUTO2000, [finding 56](xppaut-findings.md#56-auto-equilibrium-period-depends-on-spectrum-order))
  stored periods up to 3.06 relative apart on `tstauto_pen` between an
  EISPACK and a LAPACK build, whose two modes both have real part `-eps/2`.
- **xppautX:** treats real parts equal within `n*eps` of the spectrum's
  scale as tied, as well as AUTO-07p's band, and takes the lowest
  frequency among tied pairs, extending AUTO-07p's own rule inside the band.
- **Card:** [W369](https://github.com/MuhammadMoustafa/xppautX/issues/431).

## 2. A Newton residual contaminates CP and GH test functions

The cusp test adds `F(u+h*v)+F(u-h*v)` and divides by `h^2`, assuming
`F(u)=0` ([toolboxae.f90:838-845](../reference/auto-07p/src/toolboxae.f90#L838)).
The GH test's diagonal second derivative does the same
([equilibrium.f90:832-844](../reference/auto-07p/src/equilibrium.f90#L832));
its step is approximately `epsilon^(1/3)*(1+max|u|)`
([equilibrium.f90:708-712](../reference/auto-07p/src/equilibrium.f90#L708)).
A finite Newton residual contributes `2*F(u)/h^2`, which is not a second
derivative and can dominate the test. The mixed and cubic stencils cancel
the constant term already.

**Evidence:** the checked-in AUTO-07p 0.9.4 references
`tests/auto/tstauto_ab_fold.auto07p` and `tstauto_ab_hopf.auto07p` (generated
by W362, not rerun in W364's sandbox). On the fold AUTO-07p labels five CP,
but the model's analytic cusp is uniquely `(u1,u2)=(0.5,2)`,
`p0=exp(-2),p1=12`. Only its third CP is there. On the Hopf curve it labels
three GH at `p0=0.2284..0.2316,p1=11.31..11.23`.

For an independent check, parameterise the reaction's Hopf curve by
`t=p0*exp(u2)`: `u1=t/(1+t)`, `u2=(4+t)/3`,
`p1=(4+t)*(1+t)/t`, `omega^2=2*t-1`. Transform the linear part to
`xi'=-omega*eta, eta'=omega*xi`. The planar Lyapunov formula has the sign
of `24-10*t-8*t^2-t^3`, with positive prefactor
`t/(16*(2*t-1)*u1^2)`. This polynomial is strictly decreasing for `t>0`
and is negative throughout the reference cluster near `t=5.5`; those
labels are not generalised Hopf points.
[codim_coefficients.py](../tests/auto/codim_coefficients.py) independently
contracts the exact analytic tensors with complex left/right eigenvectors;
for unit complex eigenvector norm it gives `l1=-0.3606957776` at `t=5.5`.
The C++ coefficient is checked against this and two other analytic points,
and against the Bautin normal form's `l1=2*b`.

The same labels, with an independent first Lyapunov coefficient (W456, [#519](https://github.com/MuhammadMoustafa/xppautX/issues/519)):
ModelDB 267710's six-compartment motoneuron (25 states, `tests/auto/ode_motoneuron6c.json`, T2 of the benchmark), Hopf
locus in (CaDen_Max, ampPar) from the fifth HB. AUTO-07p 0.9.4 labels five GH at points 63, 67, 71, 76 and 83, ampPar 0.7392 to 0.7954
(CaDen_Max 9.68e-5 to 3.85e-5), five sign changes of its test function within 0.056 of ampPar. The coefficient from Kuznetsov's
formula with the multilinear forms differenced in quadruple precision, from the model's own Fortran `FUNC`, is +8.5e-4 to
+1.9e-3 at all 89 points of that arm, the labelled ones included (+1.25e-3, +9.1e-4, +1.05e-3, +1.02e-3, +9.2e-4, taken at AUTO-07p's own places); xppautX's
test function agrees to 1.5e-4 relative, with the difference or the exact Jacobian. With EPSL = EPSU = EPSS 1e-4 AUTO-07p labels the
five GH again on T2's own description; with 1e-7 and 1e-9 it labels none.

The same at a published model's two Bautin points (W537, [#602](https://github.com/MuhammadMoustafa/xppautX/issues/602)): the Bykov-Yablonskii-Kim CO-oxidation model
with the equations and values of Kuznetsov's MatCont Tutorial IV (`tests/auto/bykov_hopf.json`), whose Hopf curve in (Q2, K) has two Bautin points,
l1 = 0 at (0.891319, 0.232487) and (0.924255, 0.305879) in the tutorial. xppautX labels them at (0.891324, 0.232490) and (0.924256, 0.305880). AUTO-07p
0.9.4 at EPSL = EPSU = EPSS 1e-4 labels five GH, the two real ones off by 0.002 and 0.007 in Q2 (0.893416 and 0.917241) and three more
(0.78621, 0.81482 and 0.94617), and on the fold curve three CP, the nearest 0.002 from the tutorial's cusp (1.006408, 0.355991) (1.00845). With
EPSL = EPSU = EPSS 1e-9 on the same description it labels exactly the two GH, at (0.891319, 0.232487) and (0.924256, 0.305880), and no CP.

A cusp can also go unlabelled (W423, [#485](https://github.com/MuhammadMoustafa/xppautX/issues/485)); the noise below fits the residual term, though no experiment on AUTO-07p isolated it.
On Mu26's rheobase-fold curve in (current, g_h soma) (`tests/auto/ode_mu26_blocked.json`, `lp_soma` with DSMAX 1)
the cusp function changes sign between points 28 (-3.4e-8) and 29 (8.1e-8), and the location evaluates 8.9e-10,
-1.8e-10, 3.0e-10, -1.7e-10, -1.3e-10, -1.2e-10, -1.1e-10, -1.0e-10, -9.6e-11: next to the root the function jumps
between iterates by more than it changes along the curve (its slope there is about 1.4e-7 per unit of arclength),
so Mueller's method creeps from one side, uses up ITMX (8) and prints `NOTE:Possible special point`
([ae.f90:729](../reference/auto-07p/src/ae.f90#L729), [768](../reference/auto-07p/src/ae.f90#L768)); the cusp is
not labelled. At DSMAX 0.5 and 0.25 AUTO-07p labels it at 1.0811051 pA. xppautX labels it at 1.0811051 pA at DSMAX 1
and 0.5. At DSMAX 0.1 neither program reaches it within the run's 45 points (both end at 0.41587 pA, g_h -3.798),
so that run is not a miss.

**xppautX:** subtracts `2*F(u)` in the diagonal second difference. This
retains the same multilinear test function while cancelling its constant
Newton residual. It locates the true CP and BT within the unchanged
reference tolerances and emits none of the spurious CP/GH labels above.
The four extra CP and three GH remain explicit traced differences.

**Card:** [W364](https://github.com/MuhammadMoustafa/xppautX/issues/426), [W456](https://github.com/MuhammadMoustafa/xppautX/issues/519).

## 3. Point numbers wrap after 9999

AUTO-07p prints `MOD(NTOT-1,9999)+1`: point 10000 appears as 1 and
point 19999 as 1 again. The real continuation count still increases
([ae.f90:927](../reference/auto-07p/src/ae.f90#L927)); this changes its
printed identifier, not the solution or its label.

**Evidence:** source arithmetic, without a long continuation run. Diagnostic
output wraps at [ae.f90:243](../reference/auto-07p/src/ae.f90#L243),
with further sites in ae.f90, bvp.f90, equilibrium.f90, maps.f90,
periodic.f90, support.f90, toolboxae.f90 and toolboxbv.f90. The signed
fort.7 point wraps in [io.f90:1128-1132](../reference/auto-07p/src/io.f90#L1128);
the algebraic and collocation fort.8 headers do so in
[ae.f90:1031-1033](../reference/auto-07p/src/ae.f90#L1031) and
[bvp.f90:1713-1715](../reference/auto-07p/src/bvp.f90#L1713).

**Reader widths checked:** fort.7's Python reader splits whitespace and
converts PT with `int`
([parseB.py:886-907](../reference/auto-07p/python/auto/parseB.py#L886));
its numeric bulk reader also uses whitespace, not character slices
([parseB.py:317-335](../reference/auto-07p/python/auto/parseB.py#L317)).
The fort.8 reader uses `list(map(int, line.split()))`, so a point number
need not fit four or five characters
([parseS.py:72-84](../reference/auto-07p/python/auto/parseS.py#L72)).
These readers accept wider whitespace-separated fields. However, after
fully parsing a fort.7 branch, AUTO-07p reconstructs PT from the row index
and applies its own modulo again
([parseB.py:237-248](../reference/auto-07p/python/auto/parseB.py#L237));
its Python writer also wraps PT
([parseB.py:761-765](../reference/auto-07p/python/auto/parseB.py#L761)).
Readable input therefore does not remove wrapping inside AUTO-07p's tools.

**xppautX:** keeps the real count in fort.7/8/9 and the log, as the diagram,
info strip, table, protocol and session diagram already do. Integer fields
have explicit separators when the numbers outgrow their minimum widths.
Direct writer/reader unit tests cover 9999, 10000, 10001, 100000 and the
largest accepted integer count, including signed fort.7 points and both
fort.8 formulations; no long continuation is needed.

**Card:** [W384](https://github.com/MuhammadMoustafa/xppautX/issues/446).

## 4. A torus curve's result depends on the start amplitude of its eigenfunction

AUTO-07p starts a torus curve of cycles (STPNTR) from the cycle with the eigenfunction set to
`0.0001*cos(T*THETA)`, `0.0001*sin(T*THETA)`
([periodic.f90:1772-1773](../reference/auto-07p/src/periodic.f90#L1772)) and the norm unknown
`PAR(NPAR)` to zero ([periodic.f90:1780](../reference/auto-07p/src/periodic.f90#L1780)). The equations
for the eigenfunction are linear and homogeneous, so its size is arbitrary, but it is not harmless:
the Newton convergence test (EPSU) is absolute, and the system is singular at a 1:1 or 1:2 resonance
(R1, R2), where the difference-quotient Jacobian's noise, proportional to the eigenfunction, decides
whether the update can reach EPSU.

**Evidence** (tools/models/res_cycles.odex, tests/auto/res_cycles.json, AUTO-07p 0.9.4 through
tools/auto_reference.py, only the constant in STPNTR changed): at 0.0001 AUTO-07p loses its first
eight steps to non-convergence, ends with the eigenfunction's squared norm at 7.6e-7 (xppautX, which
converges at once, ends at 2.1e-2) and labels R1 (torus_down) and R2 and the BP (torus_up). At 0.0003,
0.001, 0.01 and 0.1 both torus runs end at their second point (MX). Which resonances a torus curve
shows therefore depends on a constant that the problem does not contain: AUTO-07p reaches R1 and R2
there because its start run happens to leave the eigenfunction small, not because its location is
sound. The points are real (the model gives R1 at c = -1, b = 1/4 and R2 at c = -0.625, b = 1/64 in
closed form), so xppautX's failure to locate them was its own limit. Since W405's second attempt xppautX
locates them without depending on the eigenfunction's size: a location stops within its tolerance short
of the 1:1 resonance, where the torus equations are singular ([auto-references.md](auto-references.md),
"W405 (second attempt)").

**Not tried:** a start amplitude between 1e-4 and 3e-4, or a different norm start; the result above
is two outcomes (found, failed), not a curve of how the loss depends on the amplitude.

**Card:** [W405 / #467](https://github.com/MuhammadMoustafa/xppautX/issues/467).

## 5. A torus curve's resonance test is blind once its angle leaves 0 to pi

A torus curve continues the angle theta of its multipliers `exp(+-i theta)` as an unknown (PAR(12) for cycles,
the last unknown for maps), started from the folded angle `ABS(ATAN2(...))` of the torus point, between 0 and pi
([periodic.f90:2196](../reference/auto-07p/src/periodic.f90#L2196)). Along the curve theta moves on smoothly and can
leave that interval. The resonance test is `THETA*(THETA-PI/2)*(THETA-2PI/3)*(THETA-PI)`
([periodic.f90:2044](../reference/auto-07p/src/periodic.f90#L2044), maps:
[maps.f90:350](../reference/auto-07p/src/maps.f90#L350)) and the label is `NINT(THETA*6/PI)` = 0, 3, 4 or 6
([periodic.f90:2152](../reference/auto-07p/src/periodic.f90#L2152),
[maps.f90:339](../reference/auto-07p/src/maps.f90#L339)): neither sees a resonance at 4 pi/3 (1:3), 3 pi/2 (1:4) or 2 pi
(1:1), the same resonances reached from the other side.

**Evidence** (tools/models/res_cycles.odex, tests/auto/res_cycles.json, torus_up): the pair's angle is
`2 pi - 2 pi sqrt(2w)` on the curve `c = -mu - w`, `b = w^2`, so after R2 at w = 1/8 (theta = pi) the curve crosses
R3 at w = 1/18 (theta = 4 pi/3, (c, b) = (-0.5556, 0.003086)) and R4 at w = 1/32 (theta = 3 pi/2, (-0.53125, 9.77e-4)),
and reaches R1 at w = 0 (theta = 2 pi). AUTO-07p's curve passes through the first two (its points 60 and 71:
(-0.55566, 0.0030978) and (-0.53033, 9.199e-4)) without a label, and labels the third BP, its branch-point test
changing sign there. xppautX's test has the same angles (auto_codim.cpp `resonance_angles`), so it matches
AUTO-07p's labels; testing the angle modulo 2 pi would label all three, as the closed form says.

**Card:** [W405 / #467](https://github.com/MuhammadMoustafa/xppautX/issues/467) (found);
[W419 / #481](https://github.com/MuhammadMoustafa/xppautX/issues/481) (what xppautX does now): the test is AUTO-07p's
polynomial of the angle taken modulo 2 pi and folded into 0 to pi, its sign flipped where the fold turns at a resonance
it tests (0 for 1:1, pi for 1:2), so from 0 to pi it is AUTO-07p's bit for bit and past pi it changes sign at every
resonance. torus_up labels R3 at c = -0.555556 and R4 at -0.531250, torus_down R4, R3, R2 and R3 past 0, and
tests/auto/henon_rot_th.json, a map's torus curve whose angle is a parameter, R3 at 4 pi/3 and R4 at 3 pi/2: each at
the closed form's place (`autocheck codim`), a traced difference from AUTO-07p (docs/auto-references.md, W419). At
b = 0 the BP keeps the label of the coincident R1.

## 6. A fold or flip curve's resonance test gives 0 when its double multiplier splits

On a fold (LP) or flip (PD) curve of cycles AUTO-07p tests the 1:1 or 1:2 resonance as `|mu| - 1` of the
multiplier next to the curve's imposed +1 or -1. It finds the imposed one among the real multipliers only
([periodic.f90:1936-1938](../reference/auto-07p/src/periodic.f90#L1936)) and computes the test only if that
one is within 0.05 of +1/-1 ([periodic.f90:2047-2053](../reference/auto-07p/src/periodic.f90#L2047)); otherwise
the test keeps its initial 0 ([periodic.f90:1849](../reference/auto-07p/src/periodic.f90#L1849)). At the resonance
the two multipliers form a Jordan block, and roundoff splits them into a conjugate pair near +1/-1: no real
multiplier is near the imposed one, the test is 0, and the location (Mueller's step with `Q = 0` is 0,
[bvp.f90:1415](../reference/auto-07p/src/bvp.f90#L1415)) stops there as converged
([bvp.f90:1422](../reference/auto-07p/src/bvp.f90#L1422)).

**Evidence** (tools/models/forced_predator_prey.odex, tests/auto/forced_pp_pd.json, pd_up): AUTO-07p labels the far
R2 at b0 = 2.5702457 with the multipliers -0.99999 +- 6.99e-5 i, a split pair, so the point is the first location
iterate whose pair had split. xppautX did the same before W405 at another iterate, b0 = 2.5701605 with
-1.000036 +- 2.24e-4 i: where the location stops depends on which iterate splits the pair first, not on the
resonance. When the split pair stands for the imposed multiplier and its modulus is tested (the modulus of a
conjugate pair is the square root of its product, which roundoff does not split), the location goes on to
`|mu| = 1`: b0 = 2.5702266, with -1.0000001 +- 3.9e-5 i (xppautX since W405).

The same 0 stands for "does not apply" in every test that has a case where it does not (the Hopf test with no
complex pair, the Floquet test of a planar orbit or with an inaccurate trivial multiplier, a resonance test with
every kind switched off, the branch-point test of a singular P1), and any of them can end a location as converged
at an iterate where it does not apply. xppautX since W419 returns a named value there, not a number, and a location
neither starts from it nor stops on it: one that meets it gives the point up, as after a solve that does not
converge (docs/auto-references.md, W419).

**Card:** [W405 / #467](https://github.com/MuhammadMoustafa/xppautX/issues/467),
[W419 / #481](https://github.com/MuhammadMoustafa/xppautX/issues/481).

## 7. The finite-difference Jacobian steps every state by the largest one's scale

With no user Jacobian (`JAC=0`) AUTO-07p's `FUNI` differences `FUNC` centrally, with one step for every component:
`EP = HMACH*(1+UMX)`, `UMX` the largest `|u(i)|` and `HMACH = 1e-7`
([interfaces.f90:86-103](../reference/auto-07p/src/interfaces.f90#L86)). A model whose states differ in size by many
orders (a membrane voltage near -70 and gating variables or concentrations of 1e-5 to 1e-22) steps each small state by
more than its own size: motoneuron6c's `EP` is 7.1e-6. The Jacobian's error is then that of the model's nonlinearity at
the largest state's scale, whatever `EPSL`, `EPSU`, `EPSS` and the continuation step are.

**Evidence** (ModelDB 267710, `tests/auto/ode_motoneuron6c.json`, the steady run's Hopf point at stored point 362; the
right-hand side is the Fortran `FUNC` of AUTO-07p's own run, `eq.f90`):

| Jacobian | Hopf ampPar | period at it |
|---|---|---|
| independent: Newton and the eigenvalues of a centred, Richardson-extrapolated Jacobian of that `FUNC`, double precision | 0.1737640862 | 608.8326 |
| xppautX `jacobian: exact`, EPSL = EPSU = EPSS 1e-7, DS 0.002, DSMAX 0.01 | 0.1737640862 | 608.83265 |
| xppautX `jacobian: exact`, the description's EPS 1e-4 | 0.1737640407 | 608.8645 |
| AUTO-07p, the description's EPS 1e-4 (XPPAUT 8.0: 0.1737628, 607.215) | 0.1737626685 | 607.3187 |
| xppautX `jacobian: difference` (AUTO's step), EPS 1e-7 as above | 0.1737627148 | 607.28611 |
| the independent evaluator with AUTO's rule: centred, `EP = 1e-7*(1+max\|u\|)` | 0.1737627148 | 607.2861 |
| the same with `HMACH` 1e-8 | 0.1737640725 | 608.8171 |

The two kinds of Jacobian differ the same way at every tolerance, AUTO's rule reproduces its numbers to every printed digit,
and the error shrinks with `HMACH`: it is the step, not Newton's tolerance. The Hopf point moves 1.4e-6 in ampPar (8e-6
relative), and the period at a Hopf point changes by about 7e5 per unit of ampPar there, so the period is off by 1.5, 0.25
percent. On Mu26 with blocked h-current (`tests/auto/ode_mu26_blocked.json`, the steady run's first fold) the two
Jacobians give the fold at SmIapp 0.0657989595 with norm 99.22379848 (exact) and 99.22409174 (difference), both at EPS 1e-10,
DS 0.001, DSMAX 0.05. xppautX's exact Jacobian is the derivative of the right-hand side itself and agrees with a central
difference of it to 1.3e-7 (motoneuron6c) and 7e-8 (Mu26), relative to the larger of 1 and the entry, at a point away
from the initial one (`tests/test_exact_jacobian.cpp`).

**xppautX:** keeps AUTO's rule as its `difference` Jacobian (the default, so a run reproduces AUTO-07p's and XPPAUT's
numbers, docs/auto-references.md) and offers the exact one (W260). The rule is XPPAUT's own as well
([xppaut-findings.md](xppaut-findings.md#74-autos-finite-difference-jacobian-steps-every-state-by-the-largest-ones-scale)).

**Card:** [W455 / #518](https://github.com/MuhammadMoustafa/xppautX/issues/518).

## 8. The first comparison of cycles from a Hopf point reads a predictor's fold test and a misoriented phase condition

Every test of a periodic run starts at 0 ([bvp.f90:178](../reference/auto-07p/src/bvp.f90#L178)), so the first step from a
Hopf point never labels ([bvp.f90:1325](../reference/auto-07p/src/bvp.f90#L1325)); the second step compares the second
point's values with the third's. Both of the tests that vanish at a Hopf point by construction, the fold's and the branch
point's, can have the wrong sign at the second point, for two reasons:

- the fold function `AP%FLDF` is the tangent solved with the last Newton factorization (`IFST=0`,
  [bvp.f90:711-724](../reference/auto-07p/src/bvp.f90#L711)), the iterate before the final correction. At the second point
  that is the eigen-cycle predictor `STHOPF` builds, the corrector converges in one correction, and the correction moves
  the parameter further than the branch has moved since the start, where the tangent is ill-conditioned (the Hopf point is
  where the constant solutions cross the cycles' equations);
- `STHOPF` sets `UPOLDP`, the derivative the next point's phase condition uses, from the eigen-cycle with one orientation
  ([bvp.f90:956](../reference/auto-07p/src/bvp.f90#L956)), but the first cycle is the equilibrium plus `DS` times the
  eigen-cycle, so from the third point on the phase condition follows the sign of `DS`. With `DS` < 0 the phase row, and
  the determinant in `FNBPBV`'s ratio ([toolboxbv.f90:397-405](../reference/auto-07p/src/toolboxbv.f90#L397)), change sign
  between the second and third point.

**Evidence.** The Bautin normal form `r' = r (b1 + b2 r^2 - r^4)`, b2 = 0.0032 (`tests/auto/bautin_near_hopf.json`, the
equations translated by `tools/auto_reference.py`): its fold of cycles is at amplitude 0.04, b1 = -2.56e-6, between the
second and third point at DS 0.02. With DS 0.02 AUTO-07p labels it at norm 0.040476392578, b1 = -2.5585301701e-6, 1.2
percent off in amplitude; with DS -0.02 it labels a BP at norm 0.020270071998 (just past the second point, 0.02) and an LP
at norm 0.018453107838, before the second point, and the fold at 0.04 is not labelled. On ModelDB 267710's cycles from
the Hopf point at ampPar 0.5728 (`tests/auto/ode_motoneuron6c.json`, DS -0.02) it labels an LP at the third point, 1.5e-7
in ampPar from the Hopf point, where ampPar rises monotonically; there xppautX's fold test was +4.57e-6 at the second point
from the last iterate's factorization and -2.5e-5 from the point's own, the sign of every later point's (-1.6e-5,
-6.3e-5, ...), and the branch-point test -1.16e-16, then +1.85e-16 (with DS +0.02: -1.16e-16, -2.09e-17, -5.16e-16).

**xppautX:** orients the start's phase condition by the sign of DS, and factors the points of the first two steps of a
branch from a Hopf point, and those of a location between them, again at the converged point before reading their tests
and multipliers (docs/auto-references.md, W458). It locates the Bautin fold at norm 0.0399989, b1 = -2.5599999923e-6, in
both directions, and labels nothing at motoneuron6c's start. XPPAUT 8.0 reads the fold test the same way
([xppaut-findings.md](xppaut-findings.md#86-the-fold-test-at-the-first-point-of-cycles-from-a-hopf-point-is-read-from-the-predictors-jacobian)).

**Card:** [W458 / #521](https://github.com/MuhammadMoustafa/xppautX/issues/521).

## 9. A special point located on a branch's last step is relabelled as its end

When the step that reaches a branch point, fold or Hopf point also takes the continuation parameter past Par Min or Par
Max (or the norm past Norm Min or Norm Max, or the point count to NMX), the point is located and then relabelled: the
end check overwrites its type with EP ([ae.f90:957-961](../reference/auto-07p/src/ae.f90#L957) for equilibria,
[bvp.f90:1574-1577](../reference/auto-07p/src/bvp.f90#L1574) for periodic orbits), so the special point appears
nowhere in the output. Evidence: `tests/auto/tstauto_san.json` continues the trivial branch of the saddle normal form in
par0 with Par Max 1, where the branch point is (det of the Jacobian is proportional to par0^2-1). From the exact
equilibrium the steps land on multiples of 0.02, the 50th at par0 = 1.0000001: the branch point is located there and
labelled EP. AUTO-07p's own reference run labels the branch point only because its start, 1.8e-4 off the equilibrium,
shifts its steps so that one lands at 0.99999998, inside the bound. With Par Max 1.5 the same branch point is
labelled BP at the same point.

**xppautX:** a special point located on the last step keeps its label; the branch still ends there, and its stop note
names the bound that was passed (core/autlib1.cpp, stplae and stplbv).

**Card:** [W495 / #559](https://github.com/MuhammadMoustafa/xppautX/issues/559).
