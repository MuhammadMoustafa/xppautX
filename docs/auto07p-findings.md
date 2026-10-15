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
