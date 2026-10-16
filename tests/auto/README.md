# AUTO numerical baseline (W299, #357)

One description per model in `examples/tstauto/`, plus each `examples/ode/`
model with an `auto` options group. Names include the example folder to
distinguish the two Morris–Lecar models. The driver verifies this inventory
before starting any server, including when `--only` selects a subset. A model
may have more descriptions than that one (W362: `tstauto_ab_fold`,
`tstauto_ab_hopf`, whose names say what they continue).

`model` is relative to the repository. `reason` explains the continuation;
optional `parameters` and `initial_conditions` set the starting state.
`runs` lists named runs in order. Each names its `kind` in W306's terms,
its `parameter` (or two `parameters` for `two_parameter` / `fixed_period`),
and its per-run `auto` settings. `ds` includes the direction. `nmax`, `npr`,
`ntst`, `dsmin`, `dsmax`, `parmin` and `parmax` are explicit. The driver
maps these names to today's protocol fields, not to recording keys.

A `start` is `{"state":"current"}`, optionally with `"settle":30`
(integrate for 30 model-time units, then copy the ending values to the
initial conditions) and `"step":0.001`, the settle's integration step
(default: the model's). A run from the current state after another run
starts again from the description's parameters and initial conditions,
as AUTO-07p starts each of its runs (W365). For periodic/BVP starts that
integration supplies the orbit too; a HomCont run's first point is that
orbit uncorrected in both programs, and `tools/auto_reference.py`
integrates AUTO-07p's with the same step. A stored start is `{"run":"steady","type":"HB","index":1}`:
the first HB in that named run, grabbed by its label. Indices are one-based
within the named run, not across the whole diagram. `switch:true` requests
branch switching. A model or individual run may instead carry `skip` with
an explicit reason; invalid descriptions always fail, including skips.

Each `.diagram.csv` is the diagram as a session file keeps it (`auto/diagram.csv`
in a `.snapx`, written by `session` `save` after the runs): a header, then one row
per stored point in stored order, with its branch, label, type, stability, every
parameter, every variable's value, maximum, minimum and mean, and its
eigenvalues, each number written to round-trip exactly. A change in the last
bit of any of them is a difference.

Run `python3 tools/autocompare.py --server ./xppautX` (`.exe` on Windows).
`--only tstauto_lo,ode_lecar` selects descriptions. Both 1 and automatic
threads must match the same baseline. `--update` writes a model only after
both counts agree and prints each changed model. Use it only for an
intended numerical change. `--keep DIR` retains both differing computed
diagrams and expected diagrams for the CI artifact. Times are measurements;
completion waits for the command's idle event, with a safety timeout only.

`mxbf:0` keeps these short continuations on the requested branch; labelled
Hopf starts are continued explicitly. The large linear fixtures retain
their 195 and 350 variables. The projection boundary fixtures exercise
homoclinic/heteroclinic equations as BVPs. Nine models are explicitly
skipped: the unfinished `tsthom3` draft, three rotating-phase models whose
supplied equilibrium starts fail, three Volterra integral models, and two
random delay-network models. Their descriptions give the specific reasons.

## Other programs' special points (W300, #358)

Beside a description's diagram, `NAME.auto07p` and `NAME.xppaut` hold the special points (type, parameter,
norm, period, eigenvalues or multipliers; not the step sequence) that AUTO-07p 0.9.4 and XPPAUT 8.0 find
for the same problem. `tools/autocompare.py` compares ours with them within a tolerance stated per quantity
([docs/auto-references.md](../../docs/auto-references.md)) on every run, from the committed files alone.
Regenerate them only on a machine that has the program, with `python3 tools/auto_reference.py --program
auto07p|xppaut [--only NAME,...]` (its docstring says what each needs); a changed file is a changed
reference, to be explained. A description without a file says why in `auto07p_skip` / `xppaut_skip`
(the two projection boundary-value fixtures for AUTO-07p, and the BVP fixtures for XPPAUT, whose
boundary-value run starts from its integrated storage); a difference found, traced and kept is an entry
of the description's `differences` (run, side, type, index among that type, cause), held exact:
the check fails if the difference goes away.

A `two_parameter` run (W362, #424) continues a labelled point of an earlier run (`start`: its run, type
LP or HB, index) in its two `parameters`. Its reference run holds `parameters`, the special points with
`par2` (BT, CP, ZH and GH on equilibrium curves; R1-R4 on cycle curves,
W364/W400; W404 adds map R1-R4 and LPD/LTR/PTR/TTR) and `curve`, every step of the other program as `[parameter, second parameter, norm]`; the curves
are matched by where they lie, not by step (docs/auto-references.md "Two-parameter curves").

## References xppautX cannot run yet (W402, #464)

A reference file holds every run of its description. A run skipped in the description (its reason names the card
that fixes it) is read with the file but not compared; the card that makes xppautX run it drops the skip
(docs/auto-references.md, "W402").

W403 (#465) adds references from published models and models of our own (`res_cycles`, `forced_pp_pd`, `forced_pp_ns`,
`lorenz_homoclinic`, `explicit_homoclinic`; docs/auto-references.md, "W403"): the curves xppautX runs but does not label, and the
HomCont runs it could not follow were skipped with PENDING W365 until W365 (#427) made them agree; the curves
stay skipped with PENDING W405.

W404 (#466) runs the Henon-with-rotation map descriptions against AUTO-07p:
fixed-point PD/TR labels and both map fold/flip directions, plus the torus up direction.
The torus down run retains a traced skip at its singular R1 endpoint: model Jacobian
roundoff displaces that label, and AUTO-07p's subsequent BP is missing even with exact
derivatives. `autocheck codim` independently checks the closed-form curves and torus
labels before that endpoint, named controls, and session round trips. No tolerance is widened.
