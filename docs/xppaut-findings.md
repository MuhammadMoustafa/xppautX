# XPPAUT findings

Bugs, wrong results and arbitrary limits found in XPPAUT 8.x while
modernizing it as xppautX, for the paper (maintainer, 2026-10-01). Each
entry: what XPPAUT does, the evidence, what xppautX does instead, and the
card and commit. Add an entry whenever a card finds one; keep the evidence
reproducible (a model, a command, the numbers).

| # | Area | Finding | Evidence | xppautX | Card |
|---|---|---|---|---|---|
| 1 | Model options | `@ newt_iter`, `@ newt_tol`, `@ jac_eps` and `@ poistop` in a model never take effect: each option clears one "already set" flag (`NEWT_ITER`, `NEWT_TOL`, `JAC_EPS`, `POISTOP`) while the defaults that follow test another flag for the same value (`EVEC_ITER`, `EVEC_ERR`, `NEWT_ERR`, ...), so the default overwrites what the model asked for (load_eqn.c's set_option against set_all_vals). | dae.ode, dae_ex3.ode, canonical/exdaebvp.ode give different results once the options apply; dae_ex3 fails at t=0.45 with the defaults and runs to t=3.65 with its own settings. Independent check against a Python solution: W126. | One table drives every option, one flag per option (W119). | W119 |
| 2 | Model options | `histlo2`, `histhi2`, `histbins2`, `histcol2`, `speccol2` are parsed by nothing: unreachable. `parmin` shares XMAX's flag. `POIEXT` is never reset or parsed. The `yhi` default writes the 3D box's `y_3d[0]` instead of `y_3d[1]`. | Read in load_eqn.c (set_option, set_all_vals); W119's option table round-trip test. | Each works, with its own flag. | W119 |
| 3 | Names | Names of variables, parameters and functions were cut at fixed lengths (dialogs, forms, file columns). | tools/models/longnames.ode (200-character names). | No limit; fixed-width columns shorten with `~`. | W76 |
