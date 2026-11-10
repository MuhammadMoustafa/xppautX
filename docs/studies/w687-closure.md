# W687: is a cycle branch's closure real? (#762)

W686 (#761) found that in ModelDB 267710's follow-all run (`tests/autox/ode_motoneuron6c_follow_all.json`, 25 states,
`examples/ode/motoneuron6c.odex`) the +DS run from a branch point of cycles ended `branch_closed` at its third point.
The same-point rule (core/autox_ends.h, every parameter and state within 8 * EPSS * (1 + |a|) of the start's, EPSS 1e-4)
accepted it, though the cycles differ visibly (VS -49.340 against -49.360). This settles whether each closure of that run is real,
from the model's own equations and none of xppautX's, XPPAUT's or AUTO-07p's continuation.

## The check

`uv run --locked python tools/autox-profile/closure_check.py [--diagram CSV]` (the numbers below are its output on the
baseline before and after the change).

- The model is tools/python-integration's translation of the .odex (its right-hand side, tools/python-integration/model.py), with
  the formulas put in dependency order (the model defines `G_S_Ah` after the first formula that reads it; xppautX evaluates in
  definition order and reads the previous call's value there). Before it is trusted the script integrates the first stored cycle
  (period 607.3) with xppautX itself (`--silent`, fourth order Runge-Kutta, dt 0.001, 607 319 rows) and with scipy LSODA
  (rtol 1e-11, atol 1e-13): the largest scaled difference along the way is 6.2e-8 (xppautX prints eight digits).
- Each stored cycle (its first mesh state `u0`, its period) is integrated over the stored period: the return error is 1e-10 for the
  start and end of the branches below, so the stored points are cycles of the model at their parameters.
- Two cycles are the same only at one parameter, and the stored points sit at parameters a few 1e-7 apart on a family that is nearly
  vertical there (a fold of cycles 7e-6 away), where that is a large step along it. So the closing cycle is refined to the exact cycle
  at the start's parameter: Newton's method on the shooting equations Phi_T(x) = x with the phase fixed, the period unknown, the
  flow's derivative by central differences (residual 1e-10 or below for the stable cycles).
- The refined cycles are compared by period and by the smallest scaled distance of the end's state from the start's dense orbit.
  Floquet multipliers (the monodromy matrix, by central differences of the flow) are given; those of the unstable cycles (multipliers
  of 8 to 25 and more) are not accurate this way (a multiplier of -486 and one of 644 for one cycle) and are not used.

## Result, before the change (1767 points)

| branch | points | closing cycle against the start's, at the start's parameter | period difference | same cycle? |
|---|---|---|---|---|
| -6 (label 15 to 36) | 39 | 1.4e-5 scaled (the residual of this unstable cycle's refinement is 1.4e-7) | 7e-6 of the period | yes |
| -7 (label 18 to 37) | 3 | 1.1e-4 scaled; VS range 0.0126 against 0.052 | 3.03e-3 (1.25e-4 of the period) | no |
| -8 (label 31 to 41) | 24 | 2.2e-10 scaled | 3.6e-9 of the period | yes |

Branch -7's three points are the start (a branch point of cycles: multipliers 1.00012 and 0.99978 besides the trivial), a point
at 0.194333 whose orbit is a constant (VS -49.36425 at both its extremes, its multipliers the equilibrium's: the corrector fell onto
the family of equilibria beyond the Hopf point) and a point at 0.1942746531 on a cycle of VS range 0.0126, a located special point
of this branch (multipliers 0.99992 +- 2e-5i, two near 1). Refined at the start's parameter 0.19427446062 it is a cycle of period
24.19079, the start's 24.19382 (the periods are exact to 1e-9): two different cycles of the model at one parameter. The closure was
spurious. Branches -6 and -8 return to their start: -8 to 2e-10 of its size, -6 to the precision of an unstable cycle's refinement.

## Why the rule accepted it, and the rule now

The same-point rule's unit for a state is EPSS * (1 + |a|), the state's size, not the cycle's. Branch -7's cycles are 0.052 in VS
at -49: a unit of 0.005 and a tolerance of 0.04 (8 units) are almost the cycle's size, and its largest difference of an extreme
(0.0218) is 4.8 units. The period does not tell them apart in the rule's units either (1.22 of 8). The cycle's own size does: in
units of at most the start's largest range the difference is 3 800 units, where the real closures are 3.3 (-6) and 0.18 (-8).

The closing test (core/autox_ends.cpp `gap_from_start`, and the test of whether the branch has left its start) measures a cycle's
states in units of EPSS * min(1 + |a|, the start's largest range) (`AutoxBranchStart::orbit_size`), with the same factor 8. The
factor and EPSS are unchanged, and a steady state's closure (no size) is as before. The queue's start-made test is not changed:
the three branch points of cycles of this model (labels 18, 33 and 39 of the six-job run) are one cycle located at three parameters
1e-8 apart on a family that is nearly vertical there, and their maxima differ by 2% of the cycle's size (a rule scaled by the size
would call them three cycles and start jobs W686 correctly drops).

## Result, after the change (1770 points)

| branch | points | closing cycle against the start's, at the start's parameter | period difference | closing units |
|---|---|---|---|---|
| -6 (label 15 to 36) | 39 | 1.4e-5 (as before) | 7e-6 | 3.3 |
| -7 (label 18 to 39) | 6 | 7.9e-8 scaled; VS range 0.0520 at both | 1.5e-7 of the period | 2.7 |
| -8 (label 31 to 43) | 24 | 2.2e-10 | 3.6e-9 | 0.18 |

Branch -7 goes on past its third point (a branch point, then a fold) and comes back to its start at its sixth: the closing cycle
refined at the start's parameter is 7.9e-8 of its size from the start's orbit and its period 1.5e-7 of it off, so this closure is
real. Labels after it shift by two (the branch point 37, the fold 38, the closure 39). `tests/autox/ode_motoneuron6c_follow_all.diagram.csv`
is the only baseline that changes (1767 to 1770 points); the other closures of the baselines are branches -6 and -8 (still closed, above) and
equilibria (`enz_follow_all`, `enz_isola`: no size, as before), and a rule that only refuses a closure cannot change a run that has none: the full autoxcompare,
autoxcheck, servercheck and examples_check are unchanged.
