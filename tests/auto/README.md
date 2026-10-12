# AUTO numerical baseline (W299, #357)

One description per model in `examples/tstauto/`, plus each `examples/ode/`
model with an `auto` options group. Names include the example folder to
distinguish the two Morris–Lecar models. The driver verifies this inventory
before starting any server, including when `--only` selects a subset.

`model` is relative to the repository. `reason` explains the continuation;
optional `parameters` and `initial_conditions` set the starting state.
`runs` lists named runs in order. Each names its `kind` in W306's terms,
its `parameter` (or two `parameters` for `two_parameter` / `fixed_period`),
and its per-run `auto` settings. `ds` includes the direction. `nmax`, `npr`,
`ntst`, `dsmin`, `dsmax`, `parmin` and `parmax` are explicit. The driver
maps these names to today's protocol fields, not to recording keys.

A `start` is `{"state":"current"}`, optionally with `"settle":30`
(integrate for 30 model-time units, then copy the ending values to the
initial conditions). For periodic/BVP starts that integration supplies
the orbit too. A stored start is `{"run":"steady","type":"HB","index":1}`:
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
