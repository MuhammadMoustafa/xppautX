# AUTO checked against AUTO-07p and XPPAUT 8.0

W299 ([#357](https://github.com/MuhammadMoustafa/xppautX/issues/357)) keeps our own AUTO diagrams byte for
byte, which guards a change but not the numbers. W300
([#358](https://github.com/MuhammadMoustafa/xppautX/issues/358)) checks them against two programs that
were not written by us: **AUTO-07p 0.9.3** (the maintained AUTO, a newer line than XPPAUT's) and **XPPAUT
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
  solution file's PAR(11).
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

26 models have an AUTO-07p reference (44 special points compared) and 24 an XPPAUT 8.0 reference (44 points); 24 of the 28 runnable descriptions hold both. Every comparison agrees, four descriptions only with a traced difference (below): no tolerance was widened to get there.

| Model | Special points in the references | AUTO-07p | XPPAUT 8.0 |
|---|---|---|---|
| ode/del_log | steady: EP | agrees (1 points; par 0, norm 4.9e-12) | agrees (1 points; par 0, norm 2.2e-08) |
| ode/lecar | steady: EP HB HB; cycles: - | agrees, 1 traced (3 points; par 5.5e-05, norm 1.5e-06, period 0.0017, spectrum 0.00041) | agrees (3 points; par 3.8e-08, norm 4.4e-08, period 5.7e-09, spectrum 4.1e-08) |
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
| frcp | steady: EP HB; cycles: - | agrees (2 points; par 5.2e-06, norm 1e-13, period 2.1e-06, spectrum 0.00093) | agrees (2 points; par 3e-08, norm 4.6e-08, period 2.5e-12, spectrum 4.1e-06) |
| lamom | steady: EP HB; cycles: - | agrees (2 points; par 3.4e-18, norm 0, period 2e-11, spectrum 1e-14) | agrees (2 points; par 2.7e-16, norm 0, period 2e-11, spectrum 1e-14) |
| lamom2 | steady: EP HB; cycles: LP | agrees (3 points; par 9.1e-13, norm 1.3e-11, period 2e-11, spectrum 3.8e-06) | agrees (3 points; par 6.8e-09, norm 4e-08, period 2e-11, spectrum 2.3e-08) |
| lecar | steady: EP HB HB; cycles: - | agrees, 1 traced (3 points; par 5.5e-05, norm 1.6e-06, period 0.0017, spectrum 0.00041) | agrees (3 points; par 4.6e-08, norm 4.1e-08, period 1.8e-09, spectrum 4.7e-08) |
| lo | steady: EP HB; cycles: - | agrees (2 points; par 2.7e-18, norm 0, period 2e-11, spectrum 9.8e-15) | agrees (2 points; par 2.5e-16, norm 0, period 2e-11, spectrum 2.5e-16) |
| pen | steady: EP | agrees (1 points; par 0, norm 3.6e-12) | agrees (1 points; par 0, norm 4.9e-09) |
| pf | steady: EP BP | agrees (2 points; par 2.7e-18, norm 0, spectrum 9.8e-15) | agrees (2 points; par 2.5e-16, norm 0, spectrum 2.2e-16) |
| pp2 | steady: EP HB BP; cycles: LP | agrees, 3 traced (3 points; par 1.8e-12, norm 3.3e-12, period 6.3e-11, spectrum 5e-07) | agrees (4 points; par 4.8e-08, norm 4.9e-07, period 2.2e-10, spectrum 2.9e-07) |
| san | steady: EP BP | agrees (2 points; par 2.4e-12, norm 2.8e-15, spectrum 1.3e-07) | agrees (2 points; par 1.7e-08, norm 4.1e-11, spectrum 1.3e-07) |
| she | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| simplefold | steady: EP LP LP | agrees (3 points; par 4.8e-12, norm 4.5e-11, spectrum 8.6e-11) | agrees (3 points; par 2.1e-08, norm 4.3e-08, spectrum 2.7e-13) |
| torx | steady: EP HB BP; cycles: - | agrees (3 points; par 4.7e-12, norm 0, period 3.3e-12, spectrum 3.9e-07) | agrees (3 points; par 2.2e-08, norm 0, period 3.3e-12, spectrum 3.7e-08) |
| tsthom | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |
| wcbiwave | steady: EP | agrees (1 points; par 0, norm 0) | agrees (1 points; par 0, norm 0) |

Cells: `agrees` (every compared quantity inside its tolerance), the number of points compared and the
largest absolute difference per quantity (`par`, `norm`, `period`, `spectrum`: the eigenvalue or multiplier
pair furthest apart); `traced` counts the points of that description recorded below. The second column lists
the special points the references hold, run by run (EP: the run's start). Where the code path is shared the
differences are roundoff: XPPAUT 8.0 and xppautX agree to 5e-7 or better (XPPAUT's own table and files print
7 digits). AUTO-07p, which prints 11, agrees with both to 5e-6 or better at starts, folds and branch points (1e-9 in the parameter and norm, the rest being the six digits of its spectra),
and at Hopf points to 5.5e-5 in the parameter, 1.7e-3 in the period and 9.3e-4 in the spectrum: a located
point is only as good as the location tolerance (`EPSS` 1e-4), and the two secant iterations stop at
different places inside it.

## Disagreements, traced

Three causes in four descriptions, all AUTO-07p against both XPPAUT 8.0 and xppautX; none is a difference of tolerance.

1. **Morris-Lecar cycles, `ode_lecar` and `tstauto_lecar`: AUTO-07p finds an LP of the cycles, ours
   and XPPAUT's run do not.** At NTST 15 the cycles pass a region where the largest multiplier is 3e6
   and the BP test function changes sign by noise at point 21; the location of that special point does not
   converge in ITMX iterations. AUTO-07p then sets the solution back to the last converged point
   (`bvp.f90` `LCSPBV`), AUTO2000 leaves the displaced iterate and goes on from it
   ([autlib1.c:6498](../reference/xppaut-8.0/autlib1.c#L6498), [findings #47](xppaut-findings.md#47-auto-a-special-point-whose-location-does-not-converge-leaves-the-iterate-behind)),
   so the branch turns round and zigzags over its visited part. **AUTO-07p is right**: the fold is there
   at NTST 60 and EPS 1e-9 in AUTO-07p and in xppautX alike (iapp 0.24841347, norm 0.381815, period
   25.7824), and xppautX with the restore added in a scratch build labels it at NTST 15 at 0.248413878
   (AUTO-07p 0.248413876), period 25.7578 (25.7582). XPPAUT 8.0's diagnostics show the same note at the
   same point. Not fixed here: the restore changes the numbers of every run that meets a failed location
   and is a W302 stage.
2. **Predator-prey `tstauto_pp2` cycles: ours has one LP (point 24), AUTO-07p two (points 27 and 29).**
   Past a period of about 60 the cycles are a near-homoclinic orbit that 15 mesh intervals do not resolve:
   at NTST 60 and EPS 1e-9 neither ours nor AUTO-07p finds any LP up to period 84.4, and they agree there
   (parameter 0.7080728, norm 0.426543, period 84.43). At NTST 15 the fold function dips to -7.9e-7 at
   point 24 in XPPAUT's own file (ours labels the LP), to +8.9e-8 in AUTO-07p's (no sign change): both are
   far below Newton's tolerance. What is printed beyond it is the mesh's, so the three points are recorded
   as traced and not compared.
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
- **AUTO-07p not run** (`auto07p_skip`): `ode/tstheti` and `tstauto_tsthom2`. Their projection boundary
  conditions (`hom_bcs`) are XPPAUT's own construct and their start is the orbit XPPAUT integrates; AUTO-07p's
  homoclinic and heteroclinic continuation takes the orbit and its equilibria through its own constants.
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

## What it chooses for W302

The table needs one stage of AUTO-07p's newer algorithms: restoring the state when the location of a
special point does not converge (`lcspae`, `lcspbv`; finding #47), which changes the baselines of the
Morris-Lecar descriptions and of `tstauto_pp2` (its path changes after the spurious LP). Nothing else the 26 models
compare differs beyond roundoff and the location tolerance; a Hopf point that a step lands on exactly
(brper) is one program's luck, not an algorithm.
