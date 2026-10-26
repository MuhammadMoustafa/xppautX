# W495: AUTO start checks (#559)

The run setup shares parameter and norm checks between the algebraic and
boundary-value drivers. The equilibrium helper (autevd.cpp) uses AUTO's own
`func` residual and the run's **EPSU**, read as a bound on the largest
absolute equation residual. It skips labelled restarts, map starts and orbit
starts. Periodic restarts additionally require a Hopf point or an orbit
(negative AUTO branch).

## Decisions (maintainer, 2026-10-26)

1. A steady-state start from initial conditions (irs 0) whose largest
   residual is above EPSU is not refused at once. It is refined by Newton,
   with the Sing pts settings (`rooter`, the owner of Sing pts' Newton, from
   equilibria.cpp), at the run's parameter values, and the run starts from the
   converged point, written back into the start before the norm check.
   It is refused only when Newton does not converge: the error names the
   equation with the largest residual at the start (its model line), that
   residual, the Newton cause and the Newton settings, and suggests Sing pts
   or integrating to a steady state. A refined start logs once at INFO:
   "AUTO start refined by Newton: largest residual R -> r".
2. Par Min/Max bound only the principal parameter, `icp[0]`, as AUTO-07p does
   (reference/auto-07p/src/ae.f90:957 tests U(NDIM+1) only). The second
   parameter of a two-parameter run is not checked.

## Implementation notes

- `worst_residual` (equilibria.cpp) is shared: the start's worst equation and
  the refined residual use it; W495's own residual search is removed.
- Newton's settings are `s.numerics.equilibria` (newton_tolerance,
  jacobian_step, search_limit, max_iterations), the Sing pts' own.
- The session's constants are set from `par` by `func` (as AUTO does on every
  call), so Newton's right-hand side is at the run's parameter values. stpnt
  copies the session's constants into `par`, so they are equal here; nothing
  is restored.
- A failed Newton puts the start back as given before the error.

## Validation

- Build: zero warnings; unit tests all passed.
- `autocompare` (after the follow-up of 2026-10-26): 61 models, 9 skipped,
  153 checks, 5 failures:
  - `ode_mu26_blocked`, `ode_mu26_control` (threads 1 and auto, 4 checks):
    their steady start is refined (confirmed: with the refinement switched
    off for a diagnostic run, `ode_mu26_control` matches its baseline and
    AUTO-07p byte for byte), so the HB point of the steady branch moves by
    about 4e-8 and the baseline differs at point 1. Not updated: the
    baselines are to stay unchanged, so this needs a maintainer decision.
    Restart from the cycles' end point (to_snic) is now allowed: the
    refusal looks at the stored label point's branch sign.
  - `tstauto_san` against AUTO-07p and XPPAUT 8.0: a later special point,
    the BP, is in both references and not found by xppautX. Not relaxed.
- `servercheck`: 1512 checks, all passed.
- The AUTO-07p and XPPAUT comparisons leave a refined run's first point out
  (autoinfo `start_refined`); every later point is compared as before.
- `autocheck diagram`: all passed (earlier run). `examples_check`,
  `deadcode`, `deadcheck`, `dupcheck`, `errorcheck`: as before.

## Baselines rewritten (autocompare --update)

Each refined start is the converged equilibrium, so the first point moved.
Its norm (column 14 of the diagram, the value autocompare compares) changed:

- explicit_homoclinic: 0.000141 -> 2.2e-08
- izh_class2: 66.1787 -> 66.1819
- lorenz_homoclinic: 0.000198 -> 6.4e-09
- tstauto_eiz-hom: 0.051285 -> 0.051635
- tstauto_san: 0.000255 -> 4.9e-08
- tstauto_torx_homcont: 0.00100 -> 1.3e-19
- tstauto_wcbiwave: 0 -> 0.0106

## Searched and reused

Searched AUTO setup, `func`/`stpnt`, the parameter layouts, `model_place`,
Error rendering, Sing pts' Newton (`rooter`, `newton`, `worst_residual`,
`newton_cause`, `newton_error`) and the numerical owner's norm routines.
Reused those rather than writing a Newton loop or a residual search.
