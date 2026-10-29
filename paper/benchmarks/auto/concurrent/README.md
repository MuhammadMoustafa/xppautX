# Concurrent AUTO runs: XPPAUT 8.0 and xppautX (W248, #304)

Question (maintainer, 2026-10-09): several AUTO runs started at once under one user, with
each program. XPPAUT 8.0 writes AUTO's files (fort.3/7/8/9, `__tmp__`, the model's `.b`,
`.d`, `.s`) into `$HOME` under the same names whatever the model
([docs/xppaut-findings.md](../../../../docs/xppaut-findings.md) finding 8,
`auto_nox.c:609-620`); xppautX gives each process its own scratch folder (W19, W121b).
What happens to each run's diagram, do runs fail, and what does a user gain in wall time
from running bifurcations in parallel where it is safe?

## Method

- Job: W246's (`../README.md`): `models/PY_S1Bf_bench`, steady state from Iapp = 20000 pA
  down, grab the first HB, periodic branch down to ParMin. Driven as W246 drove it:
  XPPAUT through its X11 window with xdotool on a private Xvfb display (`../hopf_xppaut.py`),
  xppautX over `--server` (`../hopf_xppautx.py`). Both drivers gained optional arguments
  (model, HOME, start barrier, per-run limit) and keep their W246 behaviour by default.
  Two changes of the model for this card, identical for both programs: `npr=10` (XPPAUT's
  table, its only text output, then holds a diagram of about 80 points, not 3-5 labelled
  ones; XPPAUT's grab needs 8 Tab presses to reach the HB instead of 1) and, for the
  "different" jobs, `parmin` 200, 400, 800, 1600.
- Runs: N = 1 (alone), 2 and 4 (maintainer's limit: the laptop overheats). Two kinds:
  **same** (N copies of the ParMin 200 job) and **different** (the first N ParMins).
  Each group is started **concurrent** (every run is set up, then all start together on
  a go file) and **sequential** (the same runs one after another), in one fresh `$HOME`
  per group, which is that group's only HOME for XPPAUT (one HOME is the point). Each
  XPPAUT run has its own Xvfb display and working folder (a copy of its model); xppautX runs
  load their `.odex` from one shared folder. xppautX runs use 1 AUTO thread
  (`{"cmd":"auto","op":"set","threads":1}`, each with its own `XPP_CONFIG_DIR`), so N processes use N cores.
  No CPU pinning (N processes on a 32-thread machine).
- Wall time of a group: concurrent = last periodic run's end minus first steady run's
  start; sequential = the sum of each run's own job time (steady + periodic, process
  start-up and the File/Auto window excluded in both). 1 excluded warm-up, then 5 measured
  repetitions; mean and sample SD (ddof 1). Modes alternate order per repetition.
  A run that does not finish within 300 s (alone: 26 s) counts as failed; all of XPPAUT's
  failures were deaths of the process (SIGSEGV).
- Comparison: the reference of each ParMin is that job alone in a fresh HOME (run twice;
  the second run's diagram equals the first for every ParMin and both programs). XPPAUT's
  diagram is the AUTO table it prints on stdout (every row, as text: XPPAUT writes the
  table on its own stdout, so it is not itself shared); xppautX's is every diagram point
  of the `diagram` events (batch, point number, x, y, y2), compared exactly. A run is
  *equal* when both runs match the reference row for row. Files left in HOME are hashed
  and compared with those of a run alone (files that differ between two runs alone, `__tmp__`
  and some `.s`/fort.*, are excluded: `alone-repeat.json`).
- Builds: see `results/provenance.json`. XPPAUT 8.0 c021b511 and xppautX 0643234f (WSL gcc
  15.2, Windows MSYS2 UCRT gcc 16.1). **Not W246's xppautX (88e09741):** HEAD's AUTO is
  about five times faster on this job (5.1 s alone against 24 s), so the xppautX job is much
  shorter than XPPAUT's (25.6 s).
- Every series ran inside one `tools/heavy.sh --alone`.

## Results

(`tables.py` prints these from `results/*/summary.json`.)

#### XPPAUT 8.0, WSL (Linux, X11)

| Jobs | N | Mode | Wall s, mean +/- SD | Complete groups | Runs | Failed | Different diagram | Equal | Speed-up |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|
| different ParMin | 2 | concurrent | n/a (survivors 25.0) | 0/5 | 10 | 7 | 2 | 1 |  |
| different ParMin | 2 | sequential | 50.09 +/- 0.31 | 5/5 | 10 | 0 | 0 | 10 |  |
| different ParMin | 4 | concurrent | n/a (survivors 23.8) | 0/5 | 20 | 19 | 1 | 0 |  |
| different ParMin | 4 | sequential | 96.39 +/- 0.21 | 5/5 | 20 | 0 | 0 | 20 |  |
| same job | 1 | alone | 25.60 +/- 0.27 | 5/5 | 5 | 0 | 0 | 5 |  |
| same job | 2 | concurrent | 26.30 +/- 0.31 | 5/5 | 10 | 0 | 0 | 10 | 1.96 |
| same job | 2 | sequential | 51.45 +/- 0.95 | 5/5 | 10 | 0 | 0 | 10 |  |
| same job | 4 | concurrent | 26.22 +/- 0.15 | 3/5 | 20 | 2 | 0 | 18 | 3.93 |
| same job | 4 | sequential | 103.00 +/- 0.37 | 5/5 | 20 | 0 | 0 | 20 |  |

#### xppautX, WSL

| Jobs | N | Mode | Wall s, mean +/- SD | Complete groups | Runs | Failed | Different diagram | Equal | Speed-up |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|
| different ParMin | 2 | concurrent | 5.12 +/- 0.08 | 5/5 | 10 | 0 | 0 | 10 | 1.93 |
| different ParMin | 2 | sequential | 9.88 +/- 0.07 | 5/5 | 10 | 0 | 0 | 10 |  |
| different ParMin | 4 | concurrent | 5.05 +/- 0.05 | 5/5 | 20 | 0 | 0 | 20 | 3.74 |
| different ParMin | 4 | sequential | 18.90 +/- 0.20 | 5/5 | 20 | 0 | 0 | 20 |  |
| same job | 1 | alone | 5.06 +/- 0.05 | 5/5 | 5 | 0 | 0 | 5 |  |
| same job | 2 | concurrent | 5.08 +/- 0.07 | 5/5 | 10 | 0 | 0 | 10 | 1.98 |
| same job | 2 | sequential | 10.06 +/- 0.06 | 5/5 | 10 | 0 | 0 | 10 |  |
| same job | 4 | concurrent | 5.17 +/- 0.11 | 5/5 | 20 | 0 | 0 | 20 | 3.90 |
| same job | 4 | sequential | 20.17 +/- 0.08 | 5/5 | 20 | 0 | 0 | 20 |  |

#### xppautX, Windows (UCRT)

| Jobs | N | Mode | Wall s, mean +/- SD | Complete groups | Runs | Failed | Different diagram | Equal | Speed-up |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|
| different ParMin | 2 | concurrent | 5.37 +/- 0.07 | 5/5 | 10 | 0 | 0 | 10 | 1.94 |
| different ParMin | 2 | sequential | 10.40 +/- 0.08 | 5/5 | 10 | 0 | 0 | 10 |  |
| different ParMin | 4 | concurrent | 5.42 +/- 0.12 | 5/5 | 20 | 0 | 0 | 20 | 3.70 |
| different ParMin | 4 | sequential | 20.06 +/- 0.45 | 5/5 | 20 | 0 | 0 | 20 |  |
| same job | 1 | alone | 5.34 +/- 0.03 | 5/5 | 5 | 0 | 0 | 5 |  |
| same job | 2 | concurrent | 5.40 +/- 0.07 | 5/5 | 10 | 0 | 0 | 10 | 1.97 |
| same job | 2 | sequential | 10.63 +/- 0.08 | 5/5 | 10 | 0 | 0 | 10 |  |
| same job | 4 | concurrent | 5.40 +/- 0.07 | 5/5 | 20 | 0 | 0 | 20 | 3.94 |
| same job | 4 | sequential | 21.27 +/- 0.10 | 5/5 | 20 | 0 | 0 | 20 |  |



Speed-up = sequential mean / concurrent mean, over the complete groups.

Failures and wrong results of XPPAUT 8.0 (WSL; measured repetitions, `results/wsl-xppaut/runs.json`):

- Same job, N = 2: 10 of 10 finished, diagrams equal. N = 4: 2 of 20 died (SIGSEGV), the others
  equal; two of the five repetitions had a death.
- Different jobs, N = 2: 7 of 10 died (SIGSEGV, in several after AUTO printed `Restart label 9
  not found`), 2 finished with the **label number of every periodic point one too high**
  (`results/wsl-xppaut/different-diagrams.json`: with the LAB column ignored, 0 rows differ), 1 finished correctly.
  N = 4: 19 of 20 died, the other had the shifted labels. No group of different jobs ran through.
- Run one after another in the same HOME, every XPPAUT run (all groups) equals its run alone.
- The files left in HOME differ from a run alone's even when the diagrams agree (concurrent
  same-job groups: `.d`, `.s`, fort.*).

xppautX: no run failed, no diagram differed from its run alone on either platform; each
process used its own scratch folder `xppautoX-<pid>-0` (recorded per run in `runs.json`), and
the only files in the shared model folder are the models.

## Limitations

- XPPAUT's diagram is what it prints (the table of labelled and every-10th points), not
  its internal arrays or the restart files; the 'wrong' result found is in the label numbers.
  A death is a death of the process whatever the files held.
- One job family (an 8-state model, one HB), N <= 4, one machine, shared with other agents'
  light work (heavy runs excluded by the lock). The xppautX job is 5 s long, so its
  speed-ups carry start-up and barrier overhead; at N = 4 on 32 threads memory bandwidth
  and boost clocks are not an issue here, a larger N or model might differ.
- XPPAUT never ran on Windows: the native build (W244) links X11 stubs that abort when
  called, and AUTO needs XPPAUT's X11 windows. The Windows part is xppautX only.
- XPPAUT's deaths in the same job's N = 4 groups mean its speed-up there is over the
  3 of 5 groups in which all four finished.

## Files

- `concurrent.py` (the runner: `--program xppaut|xppautx`), `tables.py`, `analyze.py`.
- `results/<platform>-<program>/`: `runs.json` (every group: per-run times, labels, digests,
  verdicts, scratch folders, files left in HOME), `summary.json`, `run.log`,
  `run-provenance.json`, `alone-repeat.json`, `references.json.gz` (the four alone diagrams),
  `diff-*.json.gz` (diagrams of the runs that differ), `different-diagrams.json` (XPPAUT).
- `results/provenance.json`: builds, compilers, hashes, machine.
