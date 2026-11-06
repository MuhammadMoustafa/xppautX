# W435 Integrating independent subsystems apart: study (#497)

Read-only study (maintainer, 2026-10-17: "each system can run in parallel"). No code changed.
Code is read at master 7e44b886, which holds W433 (52c1265c, #495). The counts come from
master's own binary: a clean copy of `git archive master` built with UCRT gcc 16, then
`xppautX --check` on every model file in the repository (section 4).

## 1. What W433 gives today

- **The split.** A load gives each state a component: two states are joined when an
  equation reads one of them, through the fixed variables, functions and tables
  (`equation_reads` and `connected_components` in core/expr_native.cpp and xpp_util).
  The result is `Model::subsystems` (core/model.h:165), empty when the model is one
  subsystem. `Model::subsystem_joined_by` (model.h:166) names what joins the rest:
  global flags, boundary conditions, or a right-hand side that is not compiled or not
  pure (`subsystem_joined_by`, expr_native.cpp:765; `compile_model` sets both, :1018).
- **Who uses it.** The implicit solvers factor their Jacobian block by block through
  `xpp::BlockLu` (core/xpp_math.h): Backward Euler (`one_bak_step`, odesol2.cpp),
  Rosenbrock (dense.h), Stiff, Gear, CVODE dense and the adjoints. The explicit
  Runge-Kutta solvers (DoPri5, DoPri8) need no Jacobian and are not split.
- **The run's join.** `subsystem_split` (equations, run; numerics_settings.h:26) chooses
  whether a Poincare section or a stop also joins the states in the reports
  (`run_joined_by`, expr_native.cpp:985). The solvers keep the equations' split.
- **W433's measured gain** (100 van der Pol pairs, 200 states, `--alone`, median of 3,
  seconds): backward_euler 0.43 to 0.23, rosenbrock 0.91 to 0.43, gear 0.21 to 0.089,
  stiff 0.45 to 0.21, cvode 0.064 to 0.053. The matrix is factored per block, so the
  bits are the same; nothing runs on a thread yet.

## 2. The questions

### 2.1 Each component on its own thread for the whole run

Two cases differ, and the card that builds this must choose by case.

**Fixed step, explicit (DoPri-type with a fixed h): the same bits.** A component's
right-hand side reads only its own states, the fixed variables and t (that is what
makes it a component). Each state's arithmetic is the same whether the other components
are computed in the same loop or on another thread. Two details must hold: t must be
computed from the step index (t0 + k*h, as the joint loop does), not accumulated per
thread, and the compiled right-hand side must be pure. `my_rhs` writes the fixed
variables into `Session::parser` (my_rhs.cpp:60-90), so threads need the compiled,
pure path that `autox_rhs_pure` already names (`rhs_impurity`, expr_native.cpp:720-770).
Models that are not pure stay one thread.

**Implicit, and any adaptive step: not the same bits.** `one_bak_step` iterates Newton
until the sum of the absolute corrections over all states is below `eul_tol`
(`err<eul_tol||err1<eul_tol`). That test is joint. A split loop would stop each
component at its own iteration count, so the values differ inside the tolerance.
Rosenbrock, Gear and CVODE's weighted RMS norm are joint in the same way and need the
same check on the card. A per-component test is a deliberate difference from XPPAUT
and must be a named choice (`subsystem_newton`: joint, the default, or each), recorded
in docs/xppautx-vs-xppaut.md. Adaptive explicit steps are chosen from the error over
all states; each component with its own step gives other steps than XPPAUT, so that is
a named option at most (the same rule the row states).

**Threads and their cost.** A fixed-step explicit component needs no synchronisation
between its steps, so the only join is at the end (or at each output). Its work is
small for the decoupled models that exist (section 4: most components are one or two
states), so a thread per component costs more than it saves there. The pool and its
thread count are the ones W247 and W270 give autoX, and W434's stage 2 plan (sweeps on
the same pool, results in serial order) is the other user.

### 2.2 Output on one time grid

- **Fixed step:** one grid for all components. Each thread writes its own column buffer;
  the caller merges them into `data_store` in time order, as autoX replays its shared
  updates on the caller (AGENTS, Threads). No thread writes the Session's rows.
- **Adaptive steps:** the components' steps differ, so each one's values at the common
  output times come from its own dense output (DoPri5/8 interpolants, W377 stage 2,
  docs/studies/w377-dense-output.md). The output times stay the joint run's.
- **Events, stops and flags:** these join the components by definition (section 2.4),
  so they stay on one thread.

### 2.3 One-way coupling by strongly connected components

W433 splits by undirected components: a model where x drives y, and y does not feed
back, is one subsystem today. Splitting by the strongly connected components of the
directed "reads" graph (Tarjan's algorithm, 1972) gives blocks in a block-triangular
order: each block reads only earlier blocks.

- For one-way coupling a single sweep in block order is exact (no iteration): block k
  reads block k-1's stored trajectory on the same grid. Fixed-step blocks pipeline
  across threads with a lag of one step.
- Two-way coupled blocks stay joint, or use waveform relaxation: iterate the blocks
  over a time window, Gauss-Seidel style, until the windows agree (Lelarasmee,
  Ruehli and Sangiovanni-Vincentelli, 1982). Gear and Wells (1984) give the multirate
  view, where each block takes its own steps and they are coupled by interpolation;
  that is the same question as adaptive own steps above.
- For algebraic parts the same block-triangular form is Modelica's BLT (block lower
  triangular) ordering of the equations: the block-triangular form of the sparse
  incidence matrix (Pothen and Fan, 1990; the sparse-matrix background is Duff, Erisman
  and Reid, 1986). A cycle inside a block is broken by tearing (Elmqvist and Otter,
  1994), iterating on the tear variables. A DAE solve or an algebraic equation already keeps
  a model one subsystem (W433), so tearing is not needed for the card's first step.
- Burrage (1995) classifies parallelism across the system (the stages or the components
  of one step evaluated together), across the method and across the step. A component
  split is the cheapest form of across-the-system parallelism: it needs no change of
  method, only of the loop.
- Parareal (Lions, Maday and Turinici, 2001) parallelises in time within one block. It
  is not a way to run independent subsystems apart and no decoupled model in the corpus
  is long enough for it; it is not proposed here.

### 2.4 What joins parts at run time

Load time (W433's `subsystem_joined_by`, expr_native.cpp:765): global flags (events),
boundary conditions, delays, DAE solve variables, algebraic equations, networks, Markov
variables, integral equations, a rebuilt function table, and a right-hand side that is
not compiled or not pure (shifts, random numbers).

Run time: a Poincare section or a stop (`subsystem_split: run`, `run_joined_by`), since
one variable's section or stop gates every other. Parameter sweeps (W434) are a
separate parallelism over runs and compose with this one; they do not join states.

## 3. What the card would have to build (if it is built)

1. Choose the component split for threads by the case (2.1): fixed-step explicit, pure
   right-hand side, `subsystem_split: equations`; everything else one thread.
2. Per-thread work arrays and a per-thread copy of the right-hand side's variables;
   the caller merges columns in time order.
3. Bit-identity check: examples md5s, goldencheck, odexcheck, the autoX baseline, a
   threaded and an unthreaded run of every decoupled model compared byte for byte.
4. A `tsancheck` run (the reader threads and the new workers touch only their own
   memory), and a timing series `--alone` (W433's method).
5. Named choices for anything that changes a number (`subsystem_newton`, adaptive own
   steps), each with its line in docs/xppautx-vs-xppaut.md.

## 4. How often real models decouple

Method: every model file in the repository (`.ode` where a `.odex` sits beside it, the
`.odex` alone otherwise; 397 files), each given to `xppautX --check` with
`--check` run once per file under `tools/heavy.sh`. The reported `subsystems`
diagnostic (info) means the load found more than one subsystem; without it the model is
one subsystem (joined, or its equations read each other). The program does not report
the reason for a coupled model through `--check`, so the joins in section 2.4 are not
counted per model here.

| Group | Files | Did not load | Decoupled | Coupled or one |
|---|---|---|---|---|
| examples/ode | 171 | 6 | 3 | 162 |
| examples/tstauto | 29 | 1 | 2 | 26 |
| examples/canonical | 16 | 0 | 0 | 16 |
| paper/benchmarks | 69 | 3 | 0 | 66 |
| docs/upstream | 19 | 2 | 0 | 17 |
| docs/studies | 1 | 0 | 0 | 1 |
| contrib/cuda | 4 | 3 | 0 | 1 |
| tools/models (test models) | 88 | 2 | 12 | 74 |
| **Total** | **397** | **17** | **17** | **363** |

Of the 380 files that load, 17 decouple (4.5%). The demo models that decouple:

- examples/ode: `ai` (6 subsystems), `logbif` (2), `nthet` (100 subsystems of 2 states).
- examples/tstauto: `bigaut` (350 subsystems, one scalar equation each: `x[j]' = -x[j]+p`),
  `dumb` (195).
- tools/models: 12, among them `henon_rot`, `res_cycles` and `zero_hopf`, which the
  autoX descriptions in tests/autox run, and models made to test the split
  (`subsystems*`, `multiwin`, `saddle_manifolds*`, `w377-dense`).

Of the 57 distinct models that tests/autox's descriptions run, 4 decouple (`henon_rot`,
`res_cycles`, `zero_hopf`, `dumb`). Among the published demos in examples/ and paper/,
no model in the paper benchmarks, canonical set or upstream docs decouples.

The 17 that do not load are not counted as coupled. 15 are models the parser or the
importer refuses: 10 for syntax or names (`candelator`, `hedge`, `tstar`, the three
`pHtools` files, `tsthom3`, `clustor`, `cuplamdif`, `koho3`), and 5 that use compiled
functions (the three `contrib/cuda` files, two `docs/upstream` files). 2 are malformed on
purpose (`malformed_unbalanced`, `input_bounds_delay_bad`). The 10 syntax failures are
outside this study; they are not decoupling questions and are not analysed here.

Reading: decoupling is rare among real models (3 of 165 loadable examples/ode files,
2 of 28 examples/tstauto), and where it occurs the components are tiny. The
measurement the split most needs is how many of the 363 coupled models are
block-triangular (one-way), which `--check` does not report. Section 5's first card
measures it.

## 5. Recommendation

Do not build the thread split now. The decoupled models are few and their components
small, so threads would gain little on the corpus; the implicit solvers and adaptive
steps would change numbers, so they need named choices first. The one measurement that
could change this is the count of one-way (block-triangular) coupling, which W433 does
not see.

Proposed cards (names only; not roadmap rows, the numbers are to be assigned):

1. **Count the block-triangular coupling.** `--check` reports the strongly connected
   blocks and their order for a coupled model; run over the 363 coupled files, with
   the count of those with more than one block. Docs and a diagnostic; no solver change.
2. **Fixed-step explicit components on threads**, only if (1) finds real models with
   large blocks that decouple or pipeline. Bit-identical by the 2.1 case; tsancheck; a
   `--alone` timing series.
3. **`subsystem_newton` (joint, each)**, a named choice for the implicit solvers, only
   if a model needs per-component Newton for speed. Recorded in
   docs/xppautx-vs-xppaut.md.
4. **Adaptive own steps with dense output**, a named option, only with a model that
   needs it; on W377's dense output.

Parareal and waveform relaxation are not proposed: no model in the corpus needs them.

## 6. Method notes and what was not checked

- Searched: the W433 code (expr_native.cpp subsystem functions, Model::subsystems,
  BlockLu in xpp_math, the implicit solvers' Newton loops in odesol2.cpp, the
  subsystem_split setting in numerics_settings.h), the W433 manual section, and W434's
  and W377's studies for the threading and dense-output parts. Nothing was reused or
  written: this is a docs card.
- Not checked: the thread-per-component timing (no code exists to time), the reason of
  each coupled model (section 4), and the Newton and norm tests of Rosenbrock, Gear and
  CVODE line by line (the card would read them).
- The literature is cited from the authors' papers and books; the page numbers were not
  checked against the originals in this session.

## References

- Burrage, K. (1995). *Parallel and Sequential Methods for Ordinary Differential Equations*. Oxford University Press.
- Duff, I. S., Erisman, A. M. and Reid, J. K. (1986). *Direct Methods for Sparse Matrices*. Oxford University Press.
- Elmqvist, H. and Otter, M. (1994). Methods for tearing systems of equations in object-oriented modeling. *Proceedings of ESM'94*, Barcelona.
- Gear, C. W. and Wells, D. R. (1984). Multirate linear multistep methods. *BIT Numerical Mathematics*, 24(4), 484-502.
- Lelarasmee, E., Ruehli, A. E. and Sangiovanni-Vincentelli, A. L. (1982). The waveform relaxation method for time-domain analysis of large scale integrated circuits. *IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems*, 1(3), 131-145.
- Lions, J.-L., Maday, Y. and Turinici, G. (2001). A "Parareal" in time discretization of PDE's. *Comptes Rendus de l'Academie des Sciences, Serie I*, 332(7), 661-668.
- Pothen, A. and Fan, C.-J. (1990). Computing the block triangular form of a sparse matrix. *ACM Transactions on Mathematical Software*, 16(4), 303-324.
- Tarjan, R. E. (1972). Depth-first search and linear graph algorithms. *SIAM Journal on Computing*, 1(2), 146-160.
