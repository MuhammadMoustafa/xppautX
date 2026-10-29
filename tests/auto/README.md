# AUTO numerical baseline (W299, #357)

One description per model in `examples/tstauto/`, plus each `examples/ode/`
model with an `auto` options group. Names include the example folder to
distinguish the two Morris–Lecar models. The driver verifies this inventory
before starting any server, including when `--only` selects a subset. A model
may have more descriptions than that one (W362: `tstauto_ab_fold`,
`tstauto_ab_hopf`, whose names say what they continue). W416 (#478): the three
`izh_*` descriptions run `tools/models/izh_rest.odex`, the smooth part of
Izhikevich's simple model (the reset takes no part in a rest state's
bifurcation), with the parameters of the `examples/ode/izh_2004*` panel they
name: the fold of class 1 and the integrator, the Hopf point of class 2 and
the resonator.
W518 (#582): `global_snic`, `global_homoclinic` and `global_heteroclinic` run the three models
of `tools/models/` named so, whose cycles from a Hopf point end on a SNIC, a homoclinic orbit and a
heteroclinic cycle: the run stops there with the new point type (`snic`, `homoclinic`, `heteroclinic`),
which neither reference has, so they carry `auto07p_skip` and `xppaut_skip`.

`model` is relative to the repository. `reason` explains the continuation;
optional `parameters` and `initial_conditions` set the starting state.
`runs` lists named runs in order. Each names its `kind` in W306's terms,
its `parameter` (or two `parameters` for `two_parameter` / `fixed_period`),
and its per-run `auto` settings, under AUTO's setting names (W531). `step_size` includes the direction.
`max_steps`, `output_every`, `mesh_intervals`, `min_step_size`, `max_step_size`, `parameter_min` and `parameter_max` are explicit. The driver
maps these names to today's protocol fields, not to recording keys. Optional per-run
`marks: [["parameter", value], ...]` locates user points (UZ); the same marks
are passed to AUTO-07p as UZR and to XPPAUT through AutoPer (W437).

A `start` is `{"state":"current"}`, optionally with `"settle":30`
(integrate for 30 model-time units, then copy the ending values to the
initial conditions) and `"step":0.001`, the settle's integration step
(default: the model's). A run from the current state after another run
starts again from the description's parameters and initial conditions,
as AUTO-07p starts each of its runs (W365). For periodic/BVP starts that
integration supplies the orbit too; a HomCont run's first point is that
orbit uncorrected in both programs, and `tools/auto_reference.py`
integrates AUTO-07p's with the same step. A stored start is `{"run":"steady","type":"hopf","index":1}`:
the first hopf point in that named run, grabbed by its label. Indices are one-based
within the named run, not across the whole diagram. A periodic run from an HB
starts the cycles; from another point of a periodic run (its end EP, say) it
extends them (AUTO's Extend, W423). `switch:true` requests
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

`max_branch_switches: 0` keeps these short continuations on the requested branch; labelled
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
the check fails if the difference goes away. A numerical discrepancy adds `quantity` (`par`, `par2`,
`norm`, `period` or `spectrum`) to an `ours` entry: only that quantity is traced,
the label and every other quantity stay checked, and the check fails if the
quantity now agrees so the exception must be removed.

A `two_parameter` run (W362, #424) continues a labelled point of an earlier run (`start`: its run, type
fold or hopf, index) in its two `parameters`. Its reference run holds `parameters`, the special points with
`par2` (BT, CP, ZH and GH on equilibrium curves; R1-R4 on cycle curves,
W364/W400; W404 adds map R1-R4 and LPD/LTR/PTR/TTR) and `curve`, every step of the other program, `[parameter, second parameter, norm, period]` in an `.auto07p` file (AUTO-07p's PERIOD column; 0 for an equilibrium) and `[parameter, second parameter, norm]` in an `.xppaut` one (W539: one format per program); the curves
are matched by where they lie, not by step (docs/auto-references.md "Two-parameter curves").

## References xppautX cannot run yet (W402, #464)

A reference file holds every run of its description. A run skipped in the description (its reason names the card
that fixes it) is read with the file but not compared; the card that makes xppautX run it drops the skip
(docs/auto-references.md, "W402").

W403 (#465) adds references from published models and models of our own (`res_cycles`, `forced_pp_pd`, `forced_pp_ns`,
`lorenz_homoclinic`, `explicit_homoclinic`; docs/auto-references.md, "W403"): the curves xppautX runs but does not label, and the
HomCont runs it could not follow were skipped with PENDING W365 until W365 (#427) made them agree; the curves
were skipped with PENDING W405 until W405 (#467) located their resonances where the torus equations are singular.

W404 (#466) runs the Henon-with-rotation map descriptions against AUTO-07p:
fixed-point PD/TR labels and both map fold/flip directions, plus the torus up direction.
The torus down run retains a traced skip at its singular R1 endpoint: since W405 xppautX's
R1 is 3.7e-7 from (-1,1), but AUTO-07p's R1 and the BP after it lie 1.9e-5 along the fold
branch its step entered there. `autocheck codim` independently checks the closed-form curves and torus
labels before that endpoint, named controls, and session round trips. No tolerance is widened.

W415 (#477) adds three descriptions from the maintainer's published research models (`examples/ode/mu26.odex`, the
two-compartment PVN neuron of Mu26 in its control and h-blocked conditions, and `ma23.odex`, the PV interneuron of Ma
et al. 2023; each also kept as the `.ode` it was converted from): the applied current from rest to the Hopf point, the
cycles from it, and for the blocked condition the Hopf and rheobase-fold loci in the h conductances. All agree with
AUTO-07p; XPPAUT 8.0's reference is skipped with its reason (docs/auto-references.md, "W415").

W423 (#485) extends both Mu26 descriptions' cycles down to the rheobase fold (`to_snic`, the source's
`run_bifurcation_*_extend.auto`) and adds the folds of cycles in the h conductances (`lpc_soma`, `lpc_dend`), skipped
for their cost with AUTO-07p's reference (docs/auto-references.md, "W423").

## Where each model comes from (W537, #602)

The comparison with XPPAUT and AUTO-07p (W354) rests on real models only (AGENTS.md): a model is a published one (its paper,
equations and parameter values cited in its file), a named demo of AUTO-07p or XPPAUT, or it is made up, and a made-up one is
kept beside the published models that reach the same point types: the redundancy checks accuracy (maintainer, 2026-10-29). The source is also the first comment of each model file. Of the 55
models the descriptions run, 7 are published, 39 are named demos (XPPAUT 8.0's `tstauto/` and `ode/` examples, of which `pp2` and
`hen` are AUTO-07p demos with their papers) and 9 are made up. The point types are the ones the baselines hold
(`.diagram.csv`, column `itp`, the `_from_` start of a two-parameter point dropped); `end`, `user_point`,
`output` and `no_convergence` are left out.

| Model | Source | Descriptions | Point types reached |
|---|---|---|---|
| `examples/ode/cuplamdif.odex` | demo: XPPAUT 8.0's ode/cuplamdif.ode | `ode_cuplamdif` | none |
| `examples/ode/del_log.odex` | demo: XPPAUT 8.0's ode/del_log.ode | `ode_del_log` | none |
| `examples/ode/gausvolt.odex` | demo: XPPAUT 8.0's ode/gausvolt.ode | `ode_gausvolt` | none |
| `examples/ode/lecar.odex` | demo: XPPAUT 8.0's ode/lecar.ode | `ode_lecar` | cycle_fold, hopf |
| `examples/ode/ma23.odex` | published: Ma et al. 2023, Table 1 | `ode_ma23` | fold, hopf |
| `examples/ode/motoneuron6c.odex` | published: Moustafa et al. 2023, ModelDB 267710 | `ode_motoneuron6c` | fold, hopf |
| `examples/ode/mu26.odex` | published (as code; counted as published, maintainer 2026-10-29): the maintainer's two-compartment PVN neuron (github.com/MuhammadMoustafa/pvn-hcn-marmoset-model) | `ode_mu26_blocked`, `ode_mu26_control` | cusp, cycle_fold, fold, hopf, snic |
| `examples/ode/osc.odex` | demo: XPPAUT 8.0's ode/osc.ode | `ode_osc` | none |
| `examples/ode/tstdel.odex` | demo: XPPAUT 8.0's ode/tstdel.ode | `ode_tstdel` | none |
| `examples/ode/tstdelsp.odex` | demo: XPPAUT 8.0's ode/tstdelsp.ode | `ode_tstdelsp` | none |
| `examples/ode/tstheti.odex` | demo: XPPAUT 8.0's ode/tstheti.ode | `ode_tstheti` | none |
| `examples/ode/tstvol3.odex` | demo: XPPAUT 8.0's ode/tstvol3.ode | `ode_tstvol3` | none |
| `examples/tstauto/ab.odex` | demo: XPPAUT 8.0's tstauto/ab.ode | `tstauto_ab`, `tstauto_ab_fold`, `tstauto_ab_hopf` | bogdanov_takens, cusp, fold, hopf |
| `examples/tstauto/bigaut.odex` | demo: XPPAUT 8.0's tstauto/bigaut.ode | `tstauto_bigaut` | none |
| `examples/tstauto/bistable.odex` | demo: XPPAUT 8.0's tstauto/bistable.ode | `tstauto_bistable` | none |
| `examples/tstauto/bob.odex` | demo: XPPAUT 8.0's tstauto/bob.ode | `tstauto_bob` | none |
| `examples/tstauto/brper.odex` | demo: XPPAUT 8.0's tstauto/brper.ode | `tstauto_brper` | cycle_branch_point, hopf |
| `examples/tstauto/dumb.odex` | demo: XPPAUT 8.0's tstauto/dumb.ode | `tstauto_dumb` | none |
| `examples/tstauto/dumbvp.odex` | demo: XPPAUT 8.0's tstauto/dumbvp.ode | `tstauto_dumbvp` | none |
| `examples/tstauto/eiz-hom.odex` | demo: XPPAUT 8.0's tstauto/eiz-hom.ode | `tstauto_eiz-hom` | none |
| `examples/tstauto/exp.odex` | demo: XPPAUT 8.0's tstauto/exp.ode | `tstauto_exp` | none |
| `examples/tstauto/frcp.odex` | demo: XPPAUT 8.0's tstauto/frcp.ode | `tstauto_frcp` | hopf |
| `examples/tstauto/lamom.odex` | demo: XPPAUT 8.0's tstauto/lamom.ode | `tstauto_lamom` | hopf |
| `examples/tstauto/lamom2.odex` | demo: XPPAUT 8.0's tstauto/lamom2.ode | `tstauto_lamom2` | cycle_fold, hopf |
| `examples/tstauto/lecar.odex` | demo: XPPAUT 8.0's tstauto/lecar.ode | `tstauto_lecar` | cycle_fold, hopf |
| `examples/tstauto/lo.odex` | demo: XPPAUT 8.0's tstauto/lo.ode | `tstauto_lo` | hopf |
| `examples/tstauto/pen.odex` | demo: XPPAUT 8.0's tstauto/pen.ode | `tstauto_pen` | fold |
| `examples/tstauto/pf.odex` | demo: XPPAUT 8.0's tstauto/pf.ode | `tstauto_pf` | branch_point |
| `examples/tstauto/phs2.odex` | demo: XPPAUT 8.0's tstauto/phs2.ode | `tstauto_phs2` | none |
| `examples/tstauto/phsmod.odex` | demo: XPPAUT 8.0's tstauto/phsmod.ode | `tstauto_phsmod` | none |
| `examples/tstauto/phssimp.odex` | demo: XPPAUT 8.0's tstauto/phssimp.ode | `tstauto_phssimp` | none |
| `examples/tstauto/pp2.odex` | demo: XPPAUT's tstauto/pp2 = AUTO-07p's pp2 demo (Doedel, J. Math. Biol. 20, 1984) | `pp2_heteroclinic`, `pp2_heteroclinic_coarse` (NTST 15, W538), `tstauto_pp2` | branch_point, cycle_fold, heteroclinic, hopf |
| `examples/tstauto/san.odex` | demo: XPPAUT 8.0's tstauto/san.ode | `tstauto_san` | branch_point |
| `examples/tstauto/she.odex` | demo: XPPAUT 8.0's tstauto/she.ode | `tstauto_she` | none |
| `examples/tstauto/simplefold.odex` | demo: XPPAUT 8.0's tstauto/simplefold.ode | `tstauto_simplefold` | fold |
| `examples/tstauto/torx.odex` | demo: XPPAUT 8.0's tstauto/torx.ode | `tstauto_torx`, `tstauto_torx_homcont` | branch_point, cycle_fold, hopf |
| `examples/tstauto/tstheti_homcont.odex` | demo: derived from XPPAUT 8.0's ode/tstheti.ode | `tstauto_tstheti_homcont` | none |
| `examples/tstauto/tsthom.odex` | demo: XPPAUT 8.0's tstauto/tsthom.ode | `tstauto_tsthom` | none |
| `examples/tstauto/tsthom2.odex` | demo: XPPAUT 8.0's tstauto/tsthom2.ode | `tstauto_tsthom2` | none |
| `examples/tstauto/tsthom3.ode` | demo: XPPAUT 8.0's tstauto/tsthom3.ode | `tstauto_tsthom3` | none |
| `examples/tstauto/wcbiwave.odex` | demo: XPPAUT 8.0's tstauto/wcbiwave.ode | `tstauto_wcbiwave` | none |
| `tools/models/bautin_near_hopf.odex` | made up: the Bautin normal form with chosen parameters | `bautin_near_hopf` | cycle_fold, hopf |
| `tools/models/bykov.odex` | published: the Bykov-Yablonskii-Kim CO-oxidation model, equations and values of Kuznetsov's MatCont Tutorial IV | `bykov_hopf` | bogdanov_takens, cusp, fold, generalized_hopf, hopf |
| `tools/models/explicit_homoclinic.odex` | made up: a 3D system with a closed-form homoclinic orbit | `explicit_homoclinic` | none |
| `tools/models/forced_predator_prey.odex` | published: Kuznetsov, Muratori and Rinaldi 1992, values of MatCont Tutorial V | `forced_pp_ns`, `forced_pp_pd` | cycle_branch_point, hopf, neimark_sacker, period_doubling, resonance_1_2 |
| `tools/models/global_heteroclinic.odex` | made up: a planar oscillator with two saddles | `global_heteroclinic` | cycle_branch_point, cycle_fold, heteroclinic, hopf |
| `tools/models/global_homoclinic.odex` | made up: a damped double-well Duffing oscillator | `global_homoclinic` | cycle_fold, homoclinic, hopf |
| `tools/models/global_snic.odex` | made up: a polar normal form | `global_snic` | hopf, snic |
| `tools/models/hen.odex` | demo: AUTO-07p's hen demo (the Henon map; Govaerts et al. 2007, section 5.1) | `hen_lp`, `hen_pd`, `hen_tr` | fold, fold_flip, neimark_sacker, period_doubling, resonance_1_1, resonance_1_2, resonance_1_3, resonance_1_4 |
| `tools/models/henon_rot.odex` | made up: AUTO-07p's hen demo's Henon map with an added rotation | `henon_rot_lp`, `henon_rot_pd`, `henon_rot_th`, `henon_rot_tr` | branch_point, flip_neimark_sacker, fold, fold_flip, fold_neimark_sacker, neimark_sacker, period_doubling, resonance_1_1, resonance_1_2, resonance_1_3, resonance_1_4 |
| `tools/models/hill_cycles.odex` | made up: a Hopf normal form with a Hill equation | `hill_cycles_pd`, `hill_cycles_torus` | cycle_branch_point, hopf, neimark_sacker, period_doubling, resonance_1_2, resonance_1_3, resonance_1_4 |
| `tools/models/izh_rest.odex` | published: Izhikevich 2004 (IEEE TNN 15:1063), Fig. 1 panels G, H, K, L | `izh_class1_integrator`, `izh_class2`, `izh_resonator` | fold, hopf |
| `tools/models/lorenz_homoclinic.odex` | published: Lorenz 1963, sigma = 10, beta = 8/3 (AUTO-07p's lrz demo); r = 13.926 (Sparrow 1982) | `lorenz_homoclinic`, `lrz_cycles` | homoclinic, hopf |
| `tools/models/res_cycles.odex` | made up: a Hopf normal form beside a Bogdanov-Takens system | `res_cycles` | cycle_branch_point, hopf, neimark_sacker, resonance_1_1, resonance_1_2, resonance_1_3, resonance_1_4 |
| `tools/models/zero_hopf.odex` | made up: a fold normal form beside a Hopf normal form | `zero_hopf` | fold, zero_hopf |

### The point types and the models that check them (W537)

`S` marks a published model, `D` a named demo, `M` a made-up one; the first of each list is the one that stands for the type.
docs/auto-references.md, "W537", gives each type's AUTO-07p and XPPAUT outcome.

| Point type | Models |
|---|---|
| `generalized_hopf` (GH) | `bykov` S (two Bautin points of MatCont Tutorial IV) |
| `homoclinic` | `lorenz_homoclinic` S (`lrz_cycles`), `global_homoclinic` M |
| `heteroclinic` | `pp2` D (`pp2_heteroclinic`, and at NTST 15 `pp2_heteroclinic_coarse`), `global_heteroclinic` M |
| `snic` | `mu26` S, `global_snic` M |
| `bogdanov_takens`, `cusp` | `bykov` S, `ab` D (cusp also `mu26`) |
| `resonance_1_1` (R1), `resonance_1_3` (R3), `resonance_1_4` (R4), `fold_flip` | `hen` D, `henon_rot`, `res_cycles`, `hill_cycles` M |
| `resonance_1_2` (R2), `period_doubling`, `neimark_sacker` | `forced_predator_prey` S, `hen` D, `henon_rot`, `hill_cycles`, `res_cycles` M |
| `zero_hopf` (ZH) | `zero_hopf` M only |
| `fold_neimark_sacker` (LPNS), `flip_neimark_sacker` (PDNS) | `henon_rot` M only |
| `double_neimark_sacker` (NSNS) | none: `henon_rot_tr`'s torus-torus point is in its skipped `down` run |
| `hopf`, `fold`, `branch_point`, `cycle_fold`, `cycle_branch_point` | published, demo and made-up models alike (the table above) |
