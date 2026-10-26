# W495: AUTO start checks (#559)

The run setup shares parameter and norm checks between the algebraic and
boundary-value drivers. The equilibrium-only helper uses the existing
`func` RHS evaluator and the run's **EPSU**, interpreted as a bound on the
largest absolute equation residual. It skips all labelled restarts, map
starts and orbit starts. The existing problem-function table remains the
source of supported two-parameter kinds. Periodic restarts additionally
require a Hopf point or an orbit (negative AUTO branch).

Norm defaults remain settings: named constants 0 and 1e100. Norm checks use
`dnrm2` for algebraic states and the existing `rnrmsq` collocation quadrature
for orbits, over the original model dimension. The primary parameter on a
bound uses Ds for the inward-direction test; the second parameter is checked
for membership only, because its direction comes from the tangent.

## Validation and compatibility

- Build: zero warnings; 59 unit suites, 91,270 assertions, zero failures.
- `servercheck --only stops`: 152 checks, zero failures, including one case
  per new validation and a periodic restart of a steady-state EP.
- Full `servercheck`: 1,453 checks, 5 failures: four diagram/grab checks
  depend on an invalid default lecar start; one expects the replaced
  first-point Par Min stop rather than the new pre-run error.
- Web build/check/typecheck pass; 362 web unit tests pass. Deadcheck,
  dupcheck, aliascheck, errorcheck and findingscheck pass.
- `autocompare`: 61 models, 9 skipped, 141 checks, 24 failures (each at
  one and automatic threads), all refused starts; no baseline was changed.
- `autocheck`: its first section has four failures and aborts after seven
  checks: its default lecar initial conditions are not an equilibrium,
  so its expected diagram and subsequent grab do not exist.

Models refused for a residual above EPSU: `explicit_homoclinic`, `izh_class2`,
`lorenz_homoclinic`, `ode_mu26_blocked`, `ode_mu26_control`, `tstauto_eiz-hom`,
`tstauto_san`, `tstauto_torx_homcont`, `tstauto_wcbiwave`.
Models refused for a parameter outside Par Min/Max: `ode_motoneuron6c`,
`tstauto_ab_fold`, `tstauto_ab_hopf`. In particular, the ab descriptions use
p1 = 14 with the primary p0 bounds [0.02, 0.2] or [0.02, 0.25].

Question for the maintainer: should the two parameters have separate bounds?
This card follows the requested shared interval and leaves every existing
baseline unchanged. Should rounded or integrated starts be refined with
Sing pts in the comparison descriptions? That changes their first points
and needs an intentional baseline decision, rather than silently relaxing
the EPSU check.

Searched AUTO setup, `func`/`stpnt`, parameter layouts, diagram accessors,
`model_place`, Error rendering and the numerical owner's norm routines.
Reused these owners rather than duplicating equation evaluation, model-line
lookup or collocation norm logic. A simpler post-run MX diagnostic was
rejected because it would still begin the invalid run. No reader, input
allocation bound or path policy was changed.
