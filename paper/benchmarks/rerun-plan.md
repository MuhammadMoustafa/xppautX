# W354 (#416) stage 1: plan for rerunning the comparison with XPPAUT 8.0 and AUTO-07p

Read-only study plan, 2026-10-14. Nothing was built, run or timed to write it. Sources: BENCHMARK_HANDOFF.md
(H), the READMEs under benchmarks/, docs/auto-references.md (AR), tools/autocompare.py, tools/auto_reference.py,
tests/auto/, AGENTS.md "XPPAUT itself", docs/roadmap.md, `git log 0643234f..HEAD`. "(inferred)" marks my
reading, not a fact in a file. Stage 2 (the runs) starts only on the merged tip after W302's stages and
W329, W330, W334, W337 (roadmap row W354).

**Revised 2026-10-16 (W413, #475).** The runs happen on the maintainer's Ubuntu machine, not on the laptop
(which overheats). This revision replaces WSL ext4 and `tools/heavy.sh --alone` with that machine and "nothing
else running" (section 4), adds step 0, the setup script (section 4), folds W378 in as section 6, and turns the
Windows series into a question for the maintainer (question 13). Nothing else of the plan is decided again here;
section 1's table still describes W269's own conditions.

## 1. What W269 measured, and what could move it

W269 (#326, 2026-10-11, xppautX `0643234f`, XPPAUT 8.0 `c021b511`, AUTO-07p 0.9.3; H 7f) repeated, with three
engines (XPPAUT, xppautX compiled, xppautX `--no-compile`), on WSL ext4 under `~/w269`, `heavy.sh --alone` (W269's conditions; stage 2 runs on the Ubuntu machine),
CPU 2, fresh HOME, LC_ALL=C, one excluded warm-up and five measured fresh processes, engine order rotating,
sample SD:

| Study (folder) | Models and settings | Measured | Rerun reason |
|---|---|---|---|
| Five-model screen + sandbox (five-model-screen/, `screen.py`) | episodic 120 states QualRK; lecdiff200 RK4; kuramot100 RK4; amari Euler; clustor QualRK; PY_S1Bf stiff 1e-8 2000 ms (README "W269") | whole process wall (start, load, run, write), exact/tolerance agreement | integration code was not touched by the cards below, so this checks "no number moved, no speed moved" |
| Size scaling (size-scaling/, `scaling.py`) | kuramoto 10-400, lecdiff 50-4800 (+6400, 12800), amari 100-2400 (+3200, 6400); step, T, 2T; WSL and Windows | per-time cost (2T-T)/T and one-step run | same; Windows has xppautX and XPPAUT builds only (no AUTO) |
| ModelDB 267710 integration (modeldb-267710/integrate.py) | rest and fire, 600 ms backward Euler, 25 states | whole process wall | same |
| AUTO, PY_S1Bf (auto/, `bench.py`, `hopf_*.py`) | steady from Iapp 20000 to the HB, periodic to ParMin 200; Ntst 100, Ncol 4, Ds -1, Dsmin 1e-4, Dsmax 50, EPS 1e-4, JAC 0/1, 1/2/4 threads, 14 engines | periodic run CPU and wall; points and labels; diagram digests | the AUTO chain and W302 touch exactly this |
| AUTO, ModelDB 267710 (modeldb-267710/auto_bench.py) | integrate, steady in ampPar, rightmost Hopf, Ds -0.02, periodic; 11 engines | wall, s per point, points, labels | same |
| Concurrent AUTO (auto/concurrent/, W248) | N = 2, 4 jobs at once | wall, speed-up, failures | not in W354's goal; keep W248, rerun only if the maintainer asks |

Changes on master since `0643234f` (git log), by kind:

- **Numerics on purpose (diagrams may differ):** W326 (restore after a failed special-point location; changed
  ode_lecar, tstauto_lecar, tstauto_pp2, ode_tstheti: AR "W302's first stage"); W341 (an MX end point keeps its
  own solution's stability: only the MX row's four spectrum columns); W343 (nonfinite solutions refused, retries
  with smaller steps; only on failure); W302's later stages (not carded yet: the roadmap row names only the first;
  each rewrites W299's baseline). W269's digests are the check: PY_S1Bf periodic `a1bd8f24...` (FD, bit-identical
  for interpreter, compiled, 1/2/4 threads) and `f34cbad2...` (exact) (README auto "W269"). A changed digest must be traced
  to one of these cards before any time is reported.
- **Layout/refactor, numbers must not change, speed may:** W298 (AUTO limits lifted, tables sized from the
  model), W332 (MPI plumbing out), W333 (copies merged), W336 (EISPACK keeps no state, workers see a const
  Session), W339/W340 (model parameters kept apart from AUTO's unknowns), W327/W328 (bounded import, stream
  positions checked), W335 (diagram caches moved to the client), and the pending W329 (typed codes), W330 (axes by
  kind), W334 (owned data and views), W337 (numerics in xpp_math, "each exact algorithm kept"). Their guard is
  autocompare's byte-exact baseline at 1 and automatic threads; only a timing series shows a cost (a view or
  bounds check in `setubv`/`conpar`/`brbd` would be a few percent; inferred).
- **Speed on purpose:** W325 (workers chosen once per run, one thread below ndim^2*NTST*NCOL = 1024). Its
  commit message gives PY_S1Bf 5.194/2.211 s (1 thread/automatic, UCRT) but the paper's folder has no record of
  it (auto-parallel/results holds wsl, wsl-jac, wsl-tcc only, 2026-10-10).
- **Format/protocol, breaks the drivers and staged models:** W317 (option group `auto:` is now `continuation`;
  old names refused: the staged `auto/models/w269/PY_S1Bf_bench.odex` uses `auto:` and will not load), W312
  (`runnow` removed), W313, W315, W318 (set syntax), W339 (no AUTO parameter selection; old session files
  refused). `models/` must be restaged from the `.ode` files by the new build, as W269 did for W246's. The
  drivers use `execute_run`-style key/answer commands that autocompare calls "the replaceable adapter to
  pre-W306 AUTO keys" (tools/autocompare.py:182); W306 and W353 (command registry) will change them.
- **Not in the way:** W350-W351, W346-W349, W319/W320 (UI and set commands).

Material in the existing record that is stale, broken or inconsistent (also listed in my report):

1. H section 1 and 9.4 say no continuation timing exists and the legacy AUTO harness is unresolved; H 7c-7f,
   `hopf_xppaut.py` (W246, Xvfb + xdotool) and `tools/auto_reference.py` (W300) did both. five-model-screen README
   "Threads" still says AUTO is not parallelised (W247). H's header says "Updated: 2026-10-09".
2. Thread pinning differs: W251 pins N threads to distinct physical cores (`CPUS = [2,4,6,...]`, comment
   "siblings are 2-3, 4-5", auto-parallel/parallel.py:19); W269 pins CPUs 2..2+N-1 (`bench.py:45`), i.e. SMT
   siblings on this 2-threads-per-core CPU (provenance `lscpu`; sibling numbering inferred from that comment).
   W269's thread scaling (xppautX 2.17x on 4, AUTO-07p 3.35x) may be understated for both.
3. ModelDB's periodic branch is not matched: XPPAUT and xppautX stop at point 145 (MX 0.62035 vs 0.62090),
   xppautX exact at 143, AUTO-07p at 355 (README modeldb "What the workflow finds"); the exact run labels a
   fold BP 0.572782 where the others label LP. Per-point times compare different branch segments.
4. provenance.json: `auto07p.configure` holds the head of config.log, not configure's options; `version` is
   "0.9.3:"; the XPPAUT binary reports "XPPAUT Version 8.1"; no AUTO-07p source hash, no libtcc version, no
   check that the xppautX tree was clean (`XPPAUTX_COMMIT=0643234f` only).
5. XPPAUT's driver polls every 0.25 s and sleeps 0.3 s per key (`hopf_xppaut.py` POLL, `keys`): fine for a
   22 s run, not for the steady-state runs (XPPAUT 0.65 s, xppautX 0.02-0.05 s, AUTO-07p 0.44 s with Python
   start-up; README auto "W269"). Those numbers are not timings of continuation.
6. AGENTS.md says "At most two heavy runs" and `tools/heavy.sh` says `HEAVY_SLOTS=3` (2026-10-14); `--alone`
   takes every slot either way.

## 2. The matched continuation timing

Goal: equilibrium and periodic-orbit continuation, three programs, the same branch, then the time.

**Workloads (proposal; items 1-4 are the W246/W269 ones, the rest new; T1 and T2 the two-parameter ones):**

| # | Run | Why | State |
|---|---|---|---|
| P1 | PY_S1Bf periodic from the HB at 16409.42 pA to ParMin 200 (336 points, BP 16394.4, EP 192.813) | already matched in all three programs (README auto) | rerun |
| E1 | PY_S1Bf steady from 20000 down to the HB (403 points), repeated k times in one session | one run is 20-50 ms in xppautX; k sized so the window is >= 1 s (inferred) | new timing |
| P2 | ModelDB 267710 periodic over a matched window: stop all programs by a parameter bound (e.g. ParMin just past the fold at 0.62421 on the way back), not by NMX | the three programs part at the BP near 0.6211 (item 3 above); a bound defines the end point independent of the step sequence | new window |
| E2 | ModelDB steady in ampPar (1081 points, five Hopf points, same in all three programs) | matched already; 0.2-1.9 s | new timing |
| E3 | a dense-size ladder: `tstauto/bigaut` (350 variables) and a resized chain, e.g. 50, 100, 200, 350 variables (NAUTO was 400 in XPPAUT; W298 lifted xppautX's) | AUTO's equilibrium cost is dense linear algebra, which the integration studies never touch (inferred) | new |
| T1 | Mu26 blocked's HB locus in (g_h soma, current): `tests/auto/ode_mu26_blocked.json` run `hb_soma`, from the steady run's first HB (13.2657 pA), 40 points, g_h 0 to 20, DSMAX 2; repeated k times in one session like E1 if one run is under 1 s | a two-parameter curve solves the extended Hopf system (a bordered Jacobian about twice the size), which no one-parameter workload touches; all three programs run it (xppautX, AUTO-07p ISW=2, XPPAUT 8.0's Two Param through W428's driver, #490) | new (maintainer, 2026-10-17); waits for W428 |
| T2 | ModelDB 267710's HB locus in (`ampPar`, `CaDen_Max`) from the HB at 0.57279 (P2's start), `CaDen_Max` (the dendritic L-type Ca density) 0 to 2x nominal, bounded by NMX; the model as examples/ode/motoneuron6c.odex (W431, #493, settled 2026-10-17) | the same extended Hopf system on a model many times Mu26's size, where a two-parameter curve's cost shows; all three programs, like T1 | new (maintainer, 2026-10-17); waits for W431 and W428 |
| V1 | Morris-Lecar `lecar` and `simplefold`, `tsthom` (reference/xppaut-8.0/tstauto) | too small to time (microseconds per point); the harness check: the committed `tests/auto/*.auto07p` and `.xppaut` already give their special points | validation only |

**How each program is driven with no person:**

- xppautX: `--server` (tools/xppclient.py `Server`), as `hopf_xppautx.py` and `autocompare.execute_run` do:
  `auto op=set numerics=...`, `auto op=grab`, the Run key and `answer`; the run ends at the command's `done`.
  The run descriptions of tests/auto (`kind`, `parameter`, `auto{ds,dsmin,dsmax,nmax,npr,ntst,parmin,parmax,...}`,
  `start{run,type,index}`) name a run in W306's terms, so only the adapter changes when W306/W353 lands.
  Threads: `auto op=set threads=N` in a fresh `XPP_CONFIG_DIR`; Jacobian: `set kind=num name=jacobian`.
  Exact diagram: the session file's `auto/diagram.csv` (every value round-trips), not the 7-digit events.
- XPPAUT 8.0 (Linux, on the Ubuntu machine): `hopf_xppaut.py`: private Xvfb display, `xdotool` keys File/Auto, Run, Grab (Tab to the
  label, Return), Run. AUTO's constants come from the model's `@` options (no dialogs); the ones XPPAUT cannot
  set (THL {11:0}, ISP, ILP, IAD 3, IID 2, ITMX 8, ITNW 7, NWTN 3, A0 0, A1 1000: `auto_reference.py` FIXED) are
  AUTO-07p's too. Constants are read back from the `.b` header and must be the run's (W300 did this). Its
  diagram is the point table on stdout (7 digits) and the diagnostics file. One Xvfb display and one HOME per
  run (W248, finding 8: a shared HOME corrupts concurrent runs). Replace the fixed 0.3 s key sleeps with
  a wait for the pop-up window (the AGENTS "sleep is not a synchronisation" rule; the paper folder is outside
  sleepcheck, but the same reasoning holds) and the 0.25 s poll with a finer one (inferred: 20 ms).
- AUTO-07p 0.9.3: `auto NAME.auto` (`py_s1bf.auto`: `run(e=..., c=...)`, `run(eq('HB1'), c=...)`), equation
  object compiled with `gcc -std=gnu17 -O2`, `OMP_NUM_THREADS=N`, JAC 0 or 1, constants in `c.NAME.eq/.po` from
  the same table as the other two. Equations: today hand-written (`py_s1bf.c`) or generated
  (`modeldb-267710/auto07p/odex_to_c.py`); `tools/auto_reference.py` has a third translator (.odex to Fortran).
  One generator should serve all (open question 8).

**Matching, before any time is compared (a gate, per workload and engine):**

1. Same start: the Newton-corrected first point, or the same Hopf label; same constants (printed from each
   program's own header).
2. Same number of points and the same special points in the same order (type, count), per run.
3. Special points within tools/autocompare.py's tolerance formula, AR "The tolerance": `2*EPSS*(1+DSMAX)*g +
   2*EPS*(1+|q|)` per quantity (parameter, norm, period, spectrum), against the committed-style `.auto07p` and
   `.xppaut` records that `auto_reference.py` writes; XPPAUT's 7 printed digits and AUTO-07p's 6 digits for spectra
   are inside that formula. Not widened to make a point pass.
4. Every point's parameter and norm within the same formula along the whole branch (W269 only checked labels,
   point counts and, for xppautX, digests), and xppautX's own digest equal to W269's, or the difference traced.
5. A run that fails the gate is reported with its numbers and is not timed as equal work. Where a program stops
   elsewhere (item 3 above) the shared prefix or the parameter-bounded window is the unit.

Two matching modes, both reported: (A) free adaptive steps (W246/W269's way: agreement is checked, not forced);
(B) a work-controlled variant for PY_S1Bf, `DSMIN = DSMAX = |DS|`, `IADS 0`, so each program takes the same N
points and the same Newton count (inferred: needs a pilot to see that it converges). The mesh adaptation is
`IAD 3` in all three; AUTO-07p's newer algorithms (AR) are what W302 adopts, so after W302 (A) compares like
with like less than before.

**What is timed.** Per run, the window from the Run key (xppautX: the `answer` that starts it; XPPAUT: the last
xdotool key; AUTO-07p: the `run()` call) to the closing EP/MX line or the command's `done`: wall (monotonic) and the program's
CPU (xppautX, XPPAUT: process; AUTO-07p: the executable via RUSAGE_CHILDREN, as W246 did, plus its Python wrapper's
wall reported separately). Start-up, model load, File/Auto, Grab and the steady run before a periodic one are
excluded and stated; xppautX's process start-up and load are already in the screen/scaling studies. Report per
branch and per point (seconds / points). For xppautX add one variant with no `diagram` data subscription, since
XPPAUT's drawing runs in the Xvfb process and is not in its CPU (inferred) while xppautX's JSON events are in its
process. Threads 1/2/4 for xppautX compiled and AUTO-07p; the interpreter and exact Jacobian as in W269.

## 3. Terman episodic: readouts and convergence

Existing evidence: H 5-7, terman-investigation/report.md. At tolerance 1e-3 XPPAUT and xppautX differ (max 3.116,
17,369 of 599,760 values outside) because of the math library; at 1e-5/1e-7/1e-9 cross-program differences
are 6e-5/4e-6/8e-6, but each program against its own 1e-9 run still moves by 130 / 111 / 3.19 (tolerances 1e-3 /
1e-5 / 1e-7). Pointwise convergence is therefore not shown, and three XPPAUT builds give three trajectories (H 7a).

**Stage 0, no run (H section 8 "evidence-only"):** compute the readouts below from the saved gzip arrays: the
original screen (results/), the controls and 1e-9 refinement (terman-investigation/), the five platform outputs
(windows-build/terman-platforms/). It shows whether the readouts agree where the trajectories do not.

**Readouts (proposal; the biology is the archive's: 10 STN cells v1-v10, 10 GPe cells vg1-vg10, 120 states, an
"episodic" rhythm with irregular firing inside each episode, episodic.ode header):**

1. Spike times of each STN and GPe cell: upward crossing of a threshold chosen from the traces and fixed before
   the study, linearly interpolated. The stored step is 0.4 (output dt), so timing is limited to that unless the
   output is refined; whether a finer `dt` changes QualRK's step sequence must be tested (inferred: it may).
2. Spike counts per cell and per population over the run.
3. Episode boundaries from a population signal (e.g. STN spike rate in a window or the synaptic variables),
   giving episode count, durations and inter-episode intervals.
4. Within an episode: spikes per cell, inter-spike intervals (mean, CV), STN-to-GPe latency.
5. The final state vector at T = 1998.8 as the pointwise (non-statistical) measure, reported with its
   sensitivity (item 8 below).
6. Sensitivity of the model itself: the same solver with initial values perturbed by 1e-9 and 1e-12. If the
   readouts of items 1-4 move at that scale, convergence can only be claimed for statistics (counts, durations,
   rates), not for individual spike times.

**Convergence, shown in each program before any timing:**

- Tolerance ladder 1e-3, 1e-4, ..., 1e-9 (QualRK uses the scalar tolerance only; the SET's absolute tolerance
  is ignored, H 6.4), each program against its own next-finer run, readouts items 1-4.
- Step axes held at XPPAUT's SET values and then varied: `dtmax` 1, 0.5, 0.25; `dtmin`; output `dt` (0.4, 0.1).
- Method axis: QualRK against another adaptive method both programs have (Stiff, Dormand-Prince 8(3) in
  XPPAUT's list, inferred) at the same tolerance.
- An independent reference outside both programs: SciPy DOP853 at rtol 1e-12 on the converted equations, for the
  readouts, not timed (maintainer, 2026-10-17, W427 #489).
- Python as a timed program (W427, #489): our own integrators in Python with numpy, each of the programs' methods
  (the method axis), to show the speed difference against an interpreted implementation.
- Predeclared acceptance (before the runs), e.g. counts equal; durations within 1%; spike-time shift below half
  an output interval for two consecutive refinements; the chosen tolerance is the loosest meeting it in both
  programs, not "where they agree" (H 9.2).
- Only then a five-repeat timing at the chosen settings, with the original 1e-3 run kept as the historical case.
  Report failed runs and the cross-platform readouts (Linux, UCRT x64, 32-bit XPPAUT, xppautX) beside it.

## 4. Run protocol

- **Step 0, the machine (W413, #475): `tools/comparison-setup.sh`.** On the Ubuntu machine, from a clone of the
  repository on its own disk at the tip below (clean tracked tree): `--check` names the missing tools in one
  `sudo apt install` line (installing is the maintainer's); `--sources` rebuilds `reference/` (xppaut-8.0 from
  `git archive c021b51`, xppaut-master at 9068d8a, AUTO-07p at 44cc1c4, each against a pinned tree hash);
  `--build` builds xppautX (`WINDOW=0`, `-g -O2`, no ccache, `XPPAUTX_COMMIT`), XPPAUT 8.0 (W238's command) and
  AUTO-07p (W269's configure options) under `~/xppautx-comparison/build`; `--record` writes the machine record
  (CPU model and count, SMT, kernel, governor, turbo, compilers, tool versions, source commits and hashes) to
  `~/xppautx-comparison/provenance/machine-<UTC>.json`, copied beside every result folder. Any missing piece stops
  it with what and how to get it; no study is skipped silently. docs/comparison-machine.md has the steps. The
  drivers then run with `--xppautx`, `--xppaut` and `AUTO_DIR` set to what `--build` prints. Note for the
  maintainer: the local AUTO-07p is the 0.9.4 line (v0.9.3-82), W269 used 0.9.3 (question 16).
- Tip: a clean checkout of the merged master after W302's declared last stage and W329, W330, W334, W337 (a
  tag or commit named in the report); `git status` clean; `build.py`'s `--tmp` and `--repo` are replaced by step 0
  (its W269 argv is kept inside the script).
- **The machine to itself.** No `tools/heavy.sh` (that lock is the laptop's, between agents). Instead, before
  every series: `tools/comparison-setup.sh --ready`, which refuses unless the CPU governor is `performance`
  (the maintainer sets it as root) and no other process uses more than 10% of a CPU; the machine record shows the
  turbo state, which is fixed for the whole stage (on or off, stated with the results). Nothing else is started
  on the machine during a series: no builds, checks, editors or desktop work; connect over ssh and leave. Series
  run one at a time, in the foreground of a `tmux` session or with `nohup`, and are not watched by polling,
  never side by side. No background `sleep` or `until` polls.
- Per engine and workload: one excluded warm-up, then five measured fresh processes (W269); engine order rotating
  each repetition; sample SD (ddof 1); timing never decides pass or fail; a failed run is kept. For runs under
  about 1 s: repeat the branch k times inside the window (section 2) rather than adding processes (inferred).
- CPU: one CPU for one thread (CPU 2, `taskset`, a wrapper that execs the program so the PID is the program's); for
  N threads one logical CPU on each of N distinct physical cores (question 10); `lscpu -e` recorded.
- Environment: fresh HOME, `XPP_CONFIG_DIR`, LC_ALL=C, no DISPLAY except XPPAUT's private Xvfb, OMP/OPENBLAS
  threads 1 except where the threads are the variable.
- Drift control (inferred): the same short calibration run (e.g. kuramot100 `--silent`, XPPAUT and xppautX) at the
  start and end of each series and before/after any pause; report its spread, never gate on it. The record gives
  the governor and turbo state; `lscpu -e` (in the machine record) shows the physical-core numbering for pinning.
- Provenance as W269 (`provenance_w269.py`: written UTC, commits, build argv, platform, python, uname, compiler,
  CPU, binary sha256 and versions, AUTO-07p dir/version) plus what it lacked: a clean-tree flag and `git describe`,
  AUTO-07p source hash and configure options, gfortran version, libtcc version (vendored: the xppautX commit's),
  and the input hashes (`input-validation.json` style); all of it but the inputs is the step-0 machine record,
  which also holds uv's version and the locked Python modules (W417, #479).
- Python drivers that need numpy, matplotlib or mpmath (plots.py, eig_accuracy.py) run as `uv run --locked python ...`
  from the clone, after `tools/comparison-setup.sh --python` (`uv sync --locked`, pyproject.toml and uv.lock at
  the repository root); standard-library drivers run either way.
- Results: new folders, W269's untouched: `five-model-screen/results-w354-<date>/`,
  `size-scaling/results/w354-ubuntu-<date>/` (and `-windows-` only if question 13 says so), `auto/results/w354-ubuntu-<date>/`,
  `modeldb-267710/results/*-w354-<date>/`, a new `continuation/` (README, drivers, `results/w354-ubuntu-<date>/`:
  runs.json with every point and digest, summary.json, run.log, provenance.json, the matching gate's report),
  `terman-investigation/readouts-w354/`. Tables regenerated by a `w354_tables.py` from saved results (copy of
  `w269_tables.py`, reading the saved JSON, no runs). Update BENCHMARK_HANDOFF.md (section 7g) only afterwards.
- Order (inferred): stage 0 readouts on saved data; one untimed pilot per workload to test the gate and the
  restaged models (digests against W269's); AUTO studies; ModelDB integration and the five-model screen; size
  scaling last, then W378 (section 6). Machine time, scaled from W269's runs on the laptop (inferred; the Ubuntu
  machine's speed is not known until step 0's pilot): AUTO PY_S1Bf about 25 min, ModelDB AUTO about 40 min
  (XPPAUT's 148 s x 6), screen about 15 min, size scaling about 2 h per platform.

## 5. Open questions for the maintainer

1. **Which tip, which columns?** Rerun on the new tip and keep W269 (`0643234f`) only as history, or also rerun
   `0643234f` in the same session? Recommend: new tip only, plus XPPAUT and AUTO-07p rerun in the same series
   (the Ubuntu machine is a different machine from W269's, so XPPAUT and AUTO-07p are rerun there anyway); W269's digests are the comparison for xppautX.
   **Settled (maintainer, 2026-10-18): the final tip only;** XPPAUT 8.0 and AUTO-07p rerun in the same series,
   W269 kept as history (its digests show which xppautX results changed), 0643234f not timed again.
2. **Is W302 finished?** Only its first stage (W326) is carded; later stages change numbers on purpose and
   W354 waits for them. Recommend: you name the last stage and its commit, and the report lists each stage's
   effect on agreement with AUTO-07p (AR) rather than keeping a pre-W302 switch.
   **Settled (maintainer, 2026-10-18): W302 is finished** (stages 1-8 done, the last W365, 0557526e; the AUTO
   cards since, to W438, also changed numbers on purpose). No stage is measured apart: the final tip is compared
   with AUTO-07p and XPPAUT 8.0, and each change's effect is cited from docs/auto-references.md's records.
3. **Scope.** Full W269 repeat, or the AUTO studies plus a digest/hash check of the integration studies? The
   integration code is untouched in the list above. Recommend: AUTO studies in full; screen, sandbox and ModelDB
   integration timed (about 30 min); size scaling last and optional.
   **Settled (maintainer, 2026-10-18): everything in one run on the final tip** except concurrent AUTO: the
   five-model screen and sandbox, ModelDB integration, Mu26 and Ma23 (section 7: integration and all three
   bifurcation programs), Izhikevich's sets, the Python comparison (W427: our methods in Python with numpy, timed;
   SciPy DOP853 the accuracy reference), the AUTO studies (PY_S1Bf, ModelDB), then size scaling, then W378
   (section 6). Concurrent AUTO is not rerun: W248 already showed XPPAUT 8.0's concurrent runs overwrite each
   other's fort.* files in $HOME. Izhikevich's three AUTO descriptions (izh_class1_integrator, izh_class2,
   izh_resonator) are compared for agreement in all three programs, untimed (2-3 points, milliseconds).
4. **Workloads.** P1, E1, P2 (parameter-bounded window), E2, E3 (size ladder), V1 as harness validation only?
   Recommend yes; E3 only if XPPAUT's 400-variable limit lets 350 run.
   **Settled (maintainer, 2026-10-18):** timed in all three programs: P1, E1, P2, E2, E3 (50/100/200/350), T1,
   T2 (sized by a pilot), Mu26's full descriptions (to_snic included; the two LPC curves in xppautX and AUTO-07p
   only, XPPAUT 8.0's past 600 s reported as such) and Ma23's steady and cycles. V1 is a driver check only.
5. **What is timed?** Run window only, per run and per point, AUTO-07p executable CPU with its Python wrapper
   reported apart, xppautX with and without the diagram subscription? Recommend yes; start-up stays in the
   screen and scaling studies.
   **Settled (maintainer, 2026-10-18): yes**, the run window, wall and CPU, per run and per point; xppautX with
   and without the diagram events (the second the like-for-like row). Plus a **time to ready** row for every model
   and program: integration from process start to the first step (xppautX start, load, TinyCC compile; XPPAUT
   `-silent` one step; Python's interpreter, imports, load); bifurcation until a run can begin (xppautX the same
   plus opening AUTO, by `--server`'s events; XPPAUT process start to its window ready, the driver's key pauses
   excluded; AUTO-07p Python start-up plus compiling the equations). **Threads:** xppautX's AUTO at 1, 2, 4 and
   `auto` (the default, W325), AUTO-07p at 1, 2, 4; integration is serial everywhere, no thread rows.
6. **Matching mode.** Free adaptive steps with the agreement gate (A) plus the fixed-step variant (B) on PY_S1Bf?
   Recommend both, A the headline.
   **Settled (maintainer, 2026-10-18): both, (A) the headline,** (B) on PY_S1Bf only after a pilot converges.
7. **Agreement gate.** Reuse autocompare's per-quantity tolerance (AR) for the whole branch rather than W269's
   labels-and-counts? Recommend yes; one definition of "the same branch" for tests and paper.
   **Settled (maintainer, 2026-10-18): yes**, autocompare's per-quantity tolerance along the whole branch, traced
   differences only as recorded; a workload that fails is reported with its numbers and not timed as equal work.
8. **AUTO-07p's equations.** One generator (extend `tools/auto_reference.py`'s .odex to Fortran, or
   `odex_to_c.py`) checked against xppautX's right-hand side at the start point to 1e-12 relative, the hand-written
   Jacobian kept only for PY_S1Bf? Recommend one generator, so no benchmark carries its own translator.
   **Settled (maintainer, 2026-10-18): one generator, `tools/auto_reference.py`'s .odex to Fortran,** for every
   workload, PY_S1Bf included, its right-hand side checked against xppautX's at the start point to 1e-12 relative
   before each series; py_s1bf.c stays only for the exact-Jacobian row; odex_to_c.py is retired.
9. **Jacobian kinds in the headline.** Finite differences on both sides as the headline, exact as the second
   row (AUTO-07p has an exact Jacobian only for PY_S1Bf)? Recommend yes (as W269).
   **Settled (maintainer, 2026-10-18): yes**, finite differences in all three the headline; the exact row has
   xppautX on every workload (its generated Jacobian) and AUTO-07p on PY_S1Bf only.
10. **Thread pinning.** Distinct physical cores (W251) for every thread count, and W269's thread rows
    marked as measured on SMT siblings? Recommend yes, after `lscpu -e`.
    **Settled (maintainer, 2026-10-18): yes**, N threads on N distinct physical cores from the machine record's
    `lscpu -e`, the same cores for xppautX and AUTO-07p; one fixed core for one thread; W269's thread rows marked
    as measured on SMT siblings wherever cited.
11. **Terman readouts and criteria.** Approve the readouts (section 3), the predeclared criteria and an
    independent reference solver outside the repository? Recommend yes; criteria written into a file before the
    first run.
    **Settled (maintainer, 2026-10-18): approved as section 3 proposes**, the criteria written into a file before
    the first run, SciPy DOP853 at 1e-12 the independent reference; if the 1e-9 perturbation moves spike times, the
    claim is limited to the statistics (counts, durations, rates).
12. **Terman's claim.** Limit it to numerical convergence of the archive's behaviour (not Figure 3, not the 31
    relabelled parameters' intent, H 6.3)? Recommend yes.
    **Settled (maintainer, 2026-10-18): Terman stays, with that limited claim:** numerical convergence of the
    archive's behaviour in both programs, not Figure 3, not the 31 repaired labels' intent; both limits stated
    beside the result. It is the paper's evidence for reproducibility (three XPPAUT builds, three trajectories;
    xppautX's bits the same on every platform) and for the 1e-3 tolerance's dependence on the math library.
13. **The Windows series: run on the laptop as before.** (Settled by the maintainer, 2026-10-16.)
    The Windows series are size scaling on Windows (`scaling.py`, xppautX and XPPAUT builds), the Windows builds
    (UCRT gcc 16, `mingw32-make -j4 xppautx BUILDDIR=build/ucrt WERROR=1`; windows-build/) and the cross-platform
    Terman readouts' Windows platforms. The laptop overheats under long series (resets on 2026-09-29 and
    2026-10-10), so a Windows series would run there under `tools/heavy.sh --alone` and the machine's mains and
    power-plan state recorded, as W269 did; dropping them loses the Windows column of the paper. Either way the
    Ubuntu machine runs no Windows series. Earlier recommendation, kept for reference: xppautX-only on Windows
    (XPPAUT's AUTO cannot run natively; the Xming route is untested, AGENTS), diagram digest compared with
    Linux's, Xming left out.
14. **Driver stability.** W306/W353 will replace the key/answer adapter. Do stage 2 on a tip where the AUTO
    commands are settled, or port the drivers once? Recommend: if W306 is not in by then, run on the current
    adapter and say so; do not change protocol mid-series.
    **Settled (maintainer, 2026-10-18): today's adapter,** stated in the method section; no protocol change during
    the series; the drivers are ported once when W306 lands.
15. **Interactive responsiveness** (H 9.5) and **concurrent AUTO** (W248): outside W354? Recommend separate cards.
    **Settled (maintainer, 2026-10-18): both left out**, concurrent integration too (separate processes run side by
    side safely in both programs, so it measures the machine, not the programs; W434's in-program sweeps are not
    in this comparison).
16. **AUTO-07p version: 0.9.4 only, never 0.9.3** (settled by the maintainer, 2026-10-16, as W357 decided for the
    references). The pinned 44cc1c4 is 0.9.4: its CHANGELOG opens with 0.9.4, and the repository has no 0.9.4 tag,
    so `git describe` names it v0.9.3-82. W269's 0.9.3 numbers stay in the archive as history; no 0.9.3 column.
17. **How paper/ gets to the machine** (W413 report, options A-C): the maintainer's choice; the plan needs the
    drivers, this plan, the staged models and W269's results (for the digests) there before step 0's pilot.
    **Settled (maintainer, 2026-10-18): copied by the maintainer.** Copy it again before stage 2 starts, for the
    edits made since (this plan's settled answers, W378's paper/benchmarks/large-stiff/).

## 6. W378 (#440): large stiff models, measured on the same machine

Source: docs/roadmap.md row W378 and issue #440 (maintainer, 2026-10-15; waits for W413). A study with a
recommendation, docs only: which linear solve to vendor, with numbers. It runs after the integration studies of
section 4, under the same protocol (`--ready`, nothing else running, five measured fresh processes after a
warm-up, engine order rotating, sample SD, timing never decides pass or fail, machine record beside the results).

- **Why.** Every stiff method (Gear, Stiff, Rosenbrock, BackEul, CVODE dense or banded) factors a dense Jacobian,
  O(n^3): about 1.25e11 operations per factorization at n=5000, which W243's lifted limit allows. Network
  models' Jacobians are sparse. W34/W183 rejected SUNDIALS on the example models, all small; this case was not
  tested. W374 (closed with a measurement, no change) found the dense LU's row/column order did not pay at n=100
  and said the stride may matter at n in the thousands: this study measures that too.
- **Models.** A stiff network of 1000 to 5000 variables in a ladder (e.g. n = 1000, 2000, 3000, 5000): a cable and
  a coupled Morris-Lecar ring with a stiff time scale; the coupling gives a banded Jacobian (a ring has two
  corner entries: banded needs the width to cover them or a permuted ordering; say which). Models are written by
  a generator in the study folder, checked to load in xppautX and to give the same trajectory at a loose
  tolerance in all solvers.
- **Solvers.** CVODE dense, CVODE banded (where the coupling allows), and SUNDIALS CVODE with KLU (sparse LU) and
  with GMRES (Krylov, preconditioned), at equal accuracy against a reference solution (a tight-tolerance
  run of one solver, whose own convergence is shown by halving the tolerance). The sparsity pattern comes from
  the exact Jacobian program (`expr_derivative`) where it exists. SUNDIALS (with KLU, SuiteSparse) is built
  by the study for the measurement only, outside xppautX's tree; nothing is vendored by the study.
- **Measured.** Wall and CPU time per run and per factorization; peak memory (`/usr/bin/time -v` maximum resident
  set); steps, Newton and linear iterations, Jacobian evaluations; the error against the reference at the
  end point and along the run. Dense LU also under callgrind's cache simulation at the largest n (W374's
  method) for the stride question. One thread, pinned to one CPU (as section 4); a GMRES row's preconditioner
  stated.
- **Result.** The break-even n where sparse or Krylov beats dense, in time and memory, per model class, with the
  numbers; the recommendation of what to vendor verbatim (AGENTS.md "Vendor outside code verbatim") or that
  nothing pays. Recorded in docs/ (the W378 study page); no change to core/ in the study.
- **Setup needs beyond step 0.** SuiteSparse (KLU) and SUNDIALS sources or packages: named by the study's own
  check (an `apt` line for the maintainer), not added to `tools/comparison-setup.sh` until a second card needs them.


## 7. Research neuron models: Mu26, Ma23 and Izhikevich's (W415, #477; W416, #478; maintainer, 2026-10-16)

The maintainer added these models to the study set on 2026-10-16. They are in the repository, so the clone has them:

- **Mu26** (`examples/ode/mu26.odex`), a two-compartment PVN parvocellular neuron, run in two conditions:
  - **control:** the steady branch in the applied current, then the cycles from its HB;
  - **g_h blocked:** the same two runs, plus the HB loci in g_h soma and in g_h dend, and the rheobase-LP locus through its CP.

  The descriptions are tests/auto/ode_mu26_control.json and ode_mu26_blocked.json. W415 timed all three Mu26 runs at about 111 s on Linux with 1 thread, through autocompare.
- **Ma23** (`examples/ode/ma23.odex`, the PVIN of Ma et al. 2023): the steady branch in the applied current, then the cycles from its HB (tests/auto/ode_ma23.json), about 3 s.
  - E_Ca = 68 mV matching Table 1 (W424, #486; the source XPP export had 130 mV).
- **Izhikevich's 28 parameter sets** (`examples/ode/izh_*.odex`, `paper/benchmarks/izhikevich/` with the .ode files and compare.sh):
  - integration only, against XPPAUT 8.0;
  - the event-reset models are the comparison's event-handling case.

**What to time:**
- **Integration:** xppautX against XPPAUT 8.0, as the five-model screen does.
- **The AUTO runs:** all three programs, xppautX (at 1 and automatic threads), XPPAUT 8.0's AUTO and AUTO-07p 0.9.4, from the thesis repository's run scripts for Mu26. Ma23's AUTO-07p setup in parvocell_model is a different parameter set, so its reference is made from the .ode.

The comparison's scope (maintainer, 2026-10-17): integration is xppautX against XPPAUT 8.0 and Python (our own integrators with numpy, timed, per method; SciPy DOP853 the untimed reference; W427, #489); bifurcation is xppautX against XPPAUT 8.0 and AUTO-07p 0.9.4. Torus models join the study later, so the torus fixes come before the comparison too.

**Pending first:**
- W421 (#483): the test functions at the converged Jacobian. It moves the last digits of every diagram.
- W422 (#484): Mu26's XPPAUT AUTO reference. Needed: XPPAUT is one of the bifurcation comparison's three programs.
- W423 (#485): Mu26's LPC loci and the branches to the SNIC.
- W428 (#490): the XPPAUT driver continues two-parameter curves, so Mu26 blocked's HB and LP loci get XPPAUT 8.0's reference (until then they are compared with AUTO-07p only).
- W425 (#487): ModelDB's spurious searches near its Hopf start (P2's work count).
- W419 (#481): the torus resonance test past pi, before the torus models join.
- W424 (#486, done at review): Ma23 at E_Ca 68; XPPAUT 8.0 now labels its HB, so Ma23 has all three programs' references.


## 8. W439 implementation of the settled plan (2026-10-18, #501)

The current entry point is `comparison/run.py`, with the exact foreground command sequence
and install lines in `docs/comparison-machine.md`. W269's old drivers and recorded results
are historical inputs, not stage-2 entry points. No timing series ran on the laptop.

| Settled point | Implementation |
|---|---|
| Final tip, scope, serial order, resume | `run.py`: Terman stage 0/convergence, screen+sandbox, ModelDB integration, Mu26/Ma23, 28 Izhikevich sets, W427 methods, continuation, scaling, unchanged W378 kit. Each study has numbered attempts and a persistent status file; other studies survive a failure. No concurrent integration or AUTO. |
| Workloads | `continuation.py`: P1/E1, P2/E2, E3 50/100/200/350, T1, pilot-sized T2, full Mu26 including to_snic and LPC, Ma23; V1 and Izhikevich descriptions untimed. XPPAUT LPC rows explicitly past 600 s, citing W436/W437. P2 ends at common ampPar .62 on the ascending branch, before the legacy spurious BP, rather than equal NMX. T2 continues CaDen_Max in [0,.000336], with ampPar second. |
| Run timing and ready | `comparison_measure.RunClock` observes the shared key/answer adapter. The reference driver optionally records AUTO-07p run() wall/child CPU after compiling, plus wrapper wall and ready. XPPAUT clocks its final zero-delay key; all earlier key pauses are excluded. Integration records whole-process and separate one-step ready rows. Per-point times and raw repeats are saved. |
| Process protocol | One warm-up, five fresh measured processes, rotating order, sample SD; fresh HOME/config, locale C, BLAS one thread. No partial timing means. E1/T1 repeat within each session, resized from the fastest warm-up, without a timing acceptance test. T2's pilot increases points while preserving its parameter window. |
| Threads/pinning | `common.physical_cores` parses recorded lscpu -e CPU/SOCKET/CORE/ONLINE, and chooses one logical CPU per physical core. xppautX 1/2/4/auto and AUTO-07p 1/2/4. Every integration process uses the same fixed single core and has no thread row. W269's SMT-sibling condition is stated with historical digests. |
| Matching A/B | Adaptive headline; PY_S1Bf fixed DSMIN=DSMAX=abs(DS), IADS=0 second series only after its pilot gate. Failures retain numbers and have no equal-work timing summary. Shared `autocompare.compare_run/compare_curve` checks special quantities and both directions of whole-branch polylines. The reference reader now permits whole one-parameter branches too. No tolerance is widened. |
| Generator and RHS | `auto_reference.Equations` owns every Fortran RHS, including PY_S1Bf; `check_rhs` evaluates its compiled FUNC and the same expanded RHS with xppautX's evaluator at the start, retaining both vectors under the strict 1e-12 relative gate (zero separately). `odex_to_c.py` deleted. Handwritten py_s1bf.c is used only for its exact-Jacobian row. |
| Jacobians | FD in all three programs is the headline. xppautX exact on every timed continuation workload; AUTO-07p exact only PY_S1Bf. The same shared numerical gates apply to those rows. |
| Terman | `terman-criteria.json` predates new runs. Stage 0 computes spikes/counts, episodes/durations/intervals, within-episode ISI mean/CV/counts, STN-GPe latency, final states and output dt from saved arrays. `terman.py` runs 1e-3..1e-9, dtmax 1/.5/.25, dtmin 1e-12/1e-13, output dt .4/.1, DoPri8, 1e-9/1e-12 initial perturbations and W427 SciPy DOP853 at 1e-12. Two consecutive refinements plus reference must meet predeclared criteria; sensitivity restricts the claim to statistics. Timing refuses absent criteria/acceptance and applies the chosen tolerance. Claim limits are saved beside every acceptance result. |
| Staging and history | Every model used in the new series is converted by the current binary before timing; originals/SET repairs/conversion logs and input hashes are retained. Native continuation event digests use W269's original point extractor and are compared with saved runs; changed digests or different windows are reported with source records and the numerical-change documentation to inspect, never silently called unchanged. |
| Machine and layout | A new --record for each study, copied into each attempt; --ready before each timed series. Root `results/w354-ubuntu-DATE/STUDY/attempt-NNN/` retains raw data, gates, provenance and summary JSON/CSV; `status.json` records resume state. W378 retains its own pilot/results/cache layout inside its attempt. |

Core presentation change needed by this plan: the old `data.events=["diagram"]` token did
nothing; diagrams were always sent. `data.diagram=false` now explicitly stops diagram
collection/delivery, and true reconstructs the full diagram from the stored data. The
numerical diagram is unchanged; `tools/servercheck.py --only diagram` checks equality, opt-out and resubscription.
The no-events paper row asserts that no diagram event was delivered. The numerical/check
paths otherwise call `autocompare.compute/execute_run` and `auto_reference.reference`.
The simpler alternative, composing the old W269 shell commands, was rejected because it
could neither suppress actual diagram work nor use one whole-branch numerical gate.

Laptop validation is one short untimed check per driver, through `comparison/check.sh` and
wslrun; The wrapper runs `uv sync --locked` in the WSL clone and every Python driver through
`uv run --locked python`, with the selected environment interpreter inherited by timed
Python children. No system packages are installed; missing uv requires the maintainer. Full Ubuntu results and scientific acceptance remain the stage-2 work.


W439 review follow-up: `servercheck.py --only diagram` owns the protocol checks. One
`point_symbol` function serves both labelled-point comparison and no-diagram grabs.
The 0.5 s CPU readiness sample is a current observation, not a prediction of later
work; its 10% threshold is the tool's screening choice, not a maintainer decision.
Smoke artifacts are retained under `build/w439-*`, never shipped in paper/.
The broad equilibrium Extend-key change was reverted. No committed description
restarts an equilibrium run from another equilibrium run. The paper's E1 repetition
restarts an ordinary EP with Run alone: XPPAUT 8.0 `auto_nox.c:2006-2008` calls
`auto_extend_ss()` directly, without a menu. The added `V1-E1-restart` short driver
case checks this path in all three programs; it changes no saved reference.

The locked-environment checks also cover `V1-fixed` (IADS=0 with equal step bounds)
and `V1-E1-restart` in all three programs. Repeated info-strip values are requested
explicitly when their change-only event did not repeat. W427's numerics reader now
supplies XPPAUT method keys from the core registry, avoiding a second spelling table.
Every repeated branch gets the same whole-branch gate; unavailable references other
than the explicitly excluded LPC rows cannot qualify as equal work. P1/P2 pilots
must reach a parameter bound; the fixed P1 pilot has enough point budget for unit
steps instead of silently comparing a truncated NMX prefix. Foreground child process
groups are cleaned up on exceptions so a failed AUTO wrapper leaves no executable
or compiler running. These checks and outputs remain untimed on the laptop.

T2 keeps W431's arclength steps (+/-0.1, DSMAX .5), rather than treating DS
as a calcium-density increment. Both directions start from the same fifth HB,
with CaDen_Max first and bounded to [0,0.000336]; the pilot scales both point
budgets and step sizes together. Old GH exception indices belong to a different
parameter order/window and are not transferred: fresh discrepancies stay visible.

Native time-to-ready starts at xppclient.Server's launch timestamp, after its scratch
folder and model copy, so client staging is outside the reported startup window.
