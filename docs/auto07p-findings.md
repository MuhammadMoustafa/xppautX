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
| [2](#2-a-newton-residual-contaminates-cp-and-gh-test-functions) | Equilibrium curves | a second difference that retains the equilibrium residual produces spurious CP/GH labels | [W364](https://github.com/MuhammadMoustafa/xppautX/issues/426) |
| [3](#3-point-numbers-wrap-after-9999) | Output | printed point numbers repeat after 9999 although the continuation count and labels continue | [W384](https://github.com/MuhammadMoustafa/xppautX/issues/446) |
| [4](#4-a-torus-curves-result-depends-on-the-start-amplitude-of-its-eigenfunction) | Torus curves of cycles | R1 and R2 on res_cycles are found only from one start amplitude of the eigenfunction; 3 times larger and the run fails at its second point | [W405](https://github.com/MuhammadMoustafa/xppautX/issues/467) |

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

**xppautX:** subtracts `2*F(u)` in the diagonal second difference. This
retains the same multilinear test function while cancelling its constant
Newton residual. It locates the true CP and BT within the unchanged
reference tolerances and emits none of the spurious CP/GH labels above.
The four extra CP and three GH remain explicit traced differences.

**Card:** [W364](https://github.com/MuhammadMoustafa/xppautX/issues/426).

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
closed form), so xppautX's failure to locate them is its own limit, kept as a skip in tests/auto
(W405).

**Not tried:** a start amplitude between 1e-4 and 3e-4, or a different norm start; the result above
is two outcomes (found, failed), not a curve of how the loss depends on the amplitude.

**Card:** [W405 / #467](https://github.com/MuhammadMoustafa/xppautX/issues/467).
