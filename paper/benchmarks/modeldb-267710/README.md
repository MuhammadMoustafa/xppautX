# ModelDB 267710: the 6-compartment motoneuron (W269, #326)

Moustafa et al. 2023, "Bifurcation analysis of spinal motoneuron firing
behaviour": a reduced 6-compartment motoneuron in XPPAUT, 25 ODEs (NEQ=25),
254 fixed quantities, 19 `.inc` files. Its README's workflow (integrate to
the steady state, AUTO's steady state in `ampPar`, grab a Hopf point, run
the periodic branch) is timed here in XPPAUT 8.0, xppautX and AUTO-07p, with
the maintainer's protocol (2026-10-10): every program on this machine, the
same settings, no timing from elsewhere.

## Files

- `source/`: ModelDBRepository/267710 at
  `2dd9ba299e56c3f2962f883afaada5a899c4e3f6`, untouched (the coordinator
  downloaded it with the maintainer's approval). The model is
  `source/XPPAUT Model/6cModel Xppaut/main.ode` and its `.inc` files.
- `models/6cModel/`: the staged copy every run reads (below).
- `integrate.py`: the integration, timed by the five-model screen's own
  runner (`../five-model-screen/screen.py`'s `measure`).
- `auto_xppautx.py`, `auto_xppaut.py`: the AUTO workflow in xppautX (over
  `--server`) and in XPPAUT 8.0 (its X11 window on a private Xvfb display,
  xdotool, W246's helpers in `../auto/hopf_xppaut.py`), the same keys.
- `auto07p/`: the same problem for AUTO-07p 0.9.3: `odex_to_c.py` writes
  `6c.c` from xppautX's conversion of the model, `c.6c.eq`/`c.6c.po` the
  constants, `6c.auto` the two runs, `start-row.dat` the start point.
- `auto_bench.py`: the AUTO series, W246's protocol (`../auto/bench.py`'s
  `series`).
- `results/`: `integrate-wsl-2026-10-11/`, `auto-wsl-2026-10-11/`, each with
  `provenance.json`.

## Staging: what was changed and why

The staged copy is `source/`'s 20 files plus:

1. `opt.inc` line 15, `@ OUTPUT=../simulations/xpp.dat`, is
   `@ OUTPUT=output.dat` in the staged copy: the original writes outside the
   model's folder into a folder that does not exist (XPPAUT 8.0 cannot write
   it; xppautX asks before writing outside, W286, and cannot create the
   folder yet, W303). Both programs get the same edited line. Nothing else
   in the model's files changed.
2. `rest.ode`: `main.ode` with `@ total=600` before `done` (the README's
   step 2, "running till 600 ms is enough"; `stim.inc` sets 200).
   `ampPar` = 0, the model's own backward Euler at dt 0.025.
3. `fire.ode` and `glob_fire.inc`: the same at `ampPar` = 0.3, where the
   cell fires repetitively (8 spikes in its last 300 ms; 0.1 rests, 0.6 and
   1.0 settle), an integration that is not at rest.
4. `rest.odex`, `fire.odex`: xppautX's conversion
   (`xppautX --convert --yes rest.ode`, the W269 build), which holds the
   included lines; its notes: `@ total_time` set more than once (XPP keeps
   the last value, as the model intends) and names written as declared
   (case).

## The AUTO workflow and what it shows

README: "Run time simulation till reaching steady state ... Open Auto and
run steady state ... Grab one of the two hopf bifurcation points and run
periodic". The model's own buttons are `int` (Initialconds/Go) and `intl`
(Initialconds/Last). AUTO starts from the initial values of the last
integration (XPPAUT's `last_ic`, autpp.c `stpnt`), so Go alone leaves AUTO
at the file's -70 mV, not an equilibrium: AUTO stops with MX at its second
point in xppautX (checked) and in AUTO-07p. The timed workflow is Go, Last
(the integration twice, 1200 ms), File/Auto, Run/Steady state, grab the
rightmost Hopf point, Numerics Ds = -0.02 (XPPAUT's default 0.02, negative
as the maintainer's protocol says), Run/Periodic.

## Builds and protocol

XPPAUT 8.0 c021b511 and xppautX `0643234f`, built as
`../five-model-screen/provenance-w269.json` says (WSL, gcc 15.2, -g -O2,
-ffp-contract=off; xppautX `WINDOW=0`), on WSL ext4 under `~/w269`;
AUTO-07p 0.9.3 as W246 built it (`~/auto-07p`, gfortran 15.2, OpenMP).
Each result folder has `provenance.json` (commits, compiler, CPU, binary
hashes). Sequential runs, `tools/heavy.sh --alone`, CPU 2 (n threads: CPUs
2..2+n-1), fresh HOME (and `XPP_CONFIG_DIR`), LC_ALL=C, one excluded
warm-up and five measured fresh processes per engine, the order rotating,
sample SD; timing is never a pass/fail.

## Integration (`integrate.py`, `results/integrate-wsl-2026-10-11/`)

600 ms of the model's own backward Euler (dt 0.025, 24001 rows, 25 states),
the whole process timed (start-up, load, integration, output), outputs
compared with the five-model screen's `compare`.

| Model | XPPAUT 8.0 | xppautX compiled | xppautX --no-compile | XPPAUT / compiled | --no-compile / compiled | compiled vs XPPAUT | compiled vs --no-compile |
|---|---:|---:|---:|---:|---:|---|---|
| rest | 10.629 +/- 0.039 | 2.820 +/- 0.012 | 8.840 +/- 0.092 | 3.77x | 3.14x | exact | exact |
| fire | 20.683 +/- 0.371 | 5.277 +/- 0.053 | 17.267 +/- 0.449 | 3.92x | 3.27x | exact | exact |

Wall seconds, mean +/- sample SD of five measured runs. Valid runs 30/30; every engine repeats its own output in all five runs: True.

- The three programs give identical exported values at rest and while firing
  (8 spikes in the last 300 ms at ampPar 0.3), in every run.
- xppautX compiled is 3.8-3.9x faster than XPPAUT 8.0 and 3.1-3.3x faster than
  its own interpreter: backward Euler forms a finite-difference Jacobian of
  the 25 states at every step, so nearly all the time is right-hand-side
  evaluations.
- Programs (`results/compile-counts.json`, `../five-model-screen/compile_counts.py`):
  290 of 290 compiled in both variants, none left to the interpreter; the
  exact Jacobian is available (83 nonzero derivatives, 1419 values).

## AUTO (`auto_bench.py`, `results/auto-wsl-2026-10-11/`)

Engines: `xppaut` (XPPAUT 8.0, Xvfb `:93`), `xppautx-interp-1t`
(`--no-compile`), `xppautx-fd-Nt` / `xppautx-exact-Nt` (compiled; N = 1, 2,
4 threads, the `auto_threads` preference set per run in a fresh
`XPP_CONFIG_DIR`; the Numerics' Jacobian set before AUTO opens), and
`auto07p-Nt` (AUTO-07p, finite differences, OMP_NUM_THREADS = N). AUTO-07p
has no exact-Jacobian run here: its Jacobian would have to be written by hand
for 25 states and 254 quantities (`py_s1bf.c` has one for the 8-state model).

AUTO-07p runs the same problem: `auto07p/odex_to_c.py` writes the model's
equations from `rest.odex` into `6c.c` in XPPAUT's order of evaluation (only
constants are read ahead, conn.inc's conductances, so each quantity is
computed once what it reads is), every number a double (an integer `1/225`
would be 0 in C), and its start point `start-row.dat`, the last row of the
600 ms rest integration (identical in XPPAUT and xppautX; 8 digits): the
right-hand side there is below 1.5e-4 in every component. Constants
`c.6c.eq`/`c.6c.po`: the model's (NTST 100, NMX 1e7, NPR 1e5, DSMAX 0.1,
ParMin -10, ParMax 1) and XPPAUT 8.0's defaults otherwise (NCOL 4, DS 0.02
then -0.02, DSMIN 0.001, EPSL/EPSU/EPSS 1e-4, IAD 3, MXBF 5, IID 2, ITMX 8,
ITNW 7, NWTN 3, A0 0, A1 1000, THL {11: 0}); `6c.auto` grabs the HB of
largest ampPar, as the others do.

| Engine | Periodic run, wall s | CPU s | points | XPPAUT / engine (wall) | labelled points (periodic) |
|---|---:|---:|---:|---:|---|
| xppaut | 147.57 +/- 2.15 | 147.24 +/- 2.14 | [145] | 1.0x | LP 0.572799, LP 0.624211, BP 0.621097, MX 0.620351 |
| xppautx-interp-1t | 72.39 +/- 0.48 | 72.39 +/- 0.48 | [145] | 2.0x | LP 0.572791, LP 0.624211, BP 0.621088, MX 0.620898 |
| xppautx-fd-1t | 38.24 +/- 0.30 | 38.24 +/- 0.30 | [145] | 3.9x | LP 0.572791, LP 0.624211, BP 0.621088, MX 0.620898 |
| xppautx-fd-2t | 21.65 +/- 0.34 | 38.98 +/- 0.53 | [145] | 6.8x | LP 0.572791, LP 0.624211, BP 0.621088, MX 0.620898 |
| xppautx-fd-4t | 12.76 +/- 0.39 | 38.83 +/- 0.89 | [145] | 11.6x | LP 0.572791, LP 0.624211, BP 0.621088, MX 0.620898 |
| xppautx-exact-1t | 19.33 +/- 0.23 | 19.33 +/- 0.23 | [143] | 7.6x | BP 0.572782, LP 0.624211, BP 0.621071, MX 0.621071 |
| xppautx-exact-2t | 11.75 +/- 0.12 | 19.58 +/- 0.16 | [143] | 12.6x | BP 0.572782, LP 0.624211, BP 0.621071, MX 0.621071 |
| xppautx-exact-4t | 7.74 +/- 0.07 | 19.47 +/- 0.10 | [143] | 19.1x | BP 0.572782, LP 0.624211, BP 0.621071, MX 0.621071 |
| auto07p-1t | 48.99 +/- 0.24 | 48.97 +/- 0.24 | [355] | 3.0x | LP 0.572782, LP 0.624211, BP 0.621098, BP 0.62107, LP 0.621069, BP 0.618754, BP 0.606883, BP 0.599044, BP 0.594678, MX 0.242454 |
| auto07p-2t | 25.99 +/- 0.39 | 51.92 +/- 0.78 | [355] | 5.7x | LP 0.572782, LP 0.624211, BP 0.621098, BP 0.62107, LP 0.621069, BP 0.618754, BP 0.606883, BP 0.599044, BP 0.594678, MX 0.242454 |
| auto07p-4t | 13.80 +/- 0.18 | 54.80 +/- 0.59 | [355] | 10.7x | LP 0.572782, LP 0.624211, BP 0.621098, BP 0.62107, LP 0.621069, BP 0.618754, BP 0.606883, BP 0.599044, BP 0.594678, MX 0.242454 |

| Engine | Integration (Go + Last), wall s | Steady state, wall s | Periodic, s per point |
|---|---:|---:|---:|
| xppaut | 21.47 | 1.95 | 1.018 |
| xppautx-interp-1t | 14.28 | 1.65 | 0.499 |
| xppautx-fd-1t | 4.34 | 0.72 | 0.264 |
| xppautx-fd-2t | 4.33 | 0.74 | 0.149 |
| xppautx-fd-4t | 4.34 | 0.73 | 0.088 |
| xppautx-exact-1t | 0.91 | 0.20 | 0.135 |
| xppautx-exact-2t | 0.91 | 0.20 | 0.082 |
| xppautx-exact-4t | 0.91 | 0.20 | 0.054 |
| auto07p-1t | (starts at the equilibrium) | 0.69 | 0.138 |
| auto07p-2t | (starts at the equilibrium) | 0.70 | 0.073 |
| auto07p-4t | (starts at the equilibrium) | 0.71 | 0.039 |

What the workflow finds (every run of each engine the same):

- The steady-state branch is the same 1081 points in all three programs:
  EP at 0, folds at 0.1743, 0.1735, 0.2435, -0.00378, and **five** Hopf
  points, at 0.17376, 0.23105, 0.19427, 0.04117 and 0.57279 (the README
  speaks of two: on the upper part of the branch, after the fold at
  -0.00378, there are two, 0.04117 and 0.57279), EP at 1.0074.
- From the rightmost Hopf point with Ds = -0.02, the periodic branch turns at
  once (a fold of cycles at 0.57279, point 3-5), climbs to a fold at 0.62421,
  meets a branch point near 0.6211 and stops: XPPAUT 8.0 and xppautX (finite
  differences) with MX at point 145 (0.62035 / 0.62090), xppautX with the
  exact Jacobian with MX at point 143 (0.62107). AUTO-07p passes that branch
  point (more BPs and a fold) and stops with MX at point 355, ampPar 0.2425.
  None reaches the other Hopf point (0.04117): the model's comment in
  opt.inc ("increasing parmax, NTST can solve MX problem") suggests its
  authors met the same stop. Not investigated further.
- xppautX's diagram is bit-identical between the interpreter and the compiled
  build and across 1, 2 and 4 threads (one digest per Jacobian kind); its
  finite-difference branch differs from XPPAUT's in the last digits (LP at
  point 5: 0.5727907 against 0.5727994; BP at 143 against 142), the math
  library and Jacobian differences W238/W239 measured.
- Speed of the periodic run (it stops at different points, so per point too):
  xppautX compiled is 3.9x faster than XPPAUT on one thread (38.2 s against
  147.6 s, the same 145 points), 11.6x on 4 threads; its interpreter 2.0x.
  The exact Jacobian halves it again (19.3 s, 143 points; 7.7 s on 4 threads,
  19x XPPAUT). Per point, xppautX compiled with the exact Jacobian (0.135 s on
  one thread, 0.054 s on four) is as fast as AUTO-07p with finite differences
  (0.138 s, 0.039 s); with finite differences on both sides AUTO-07p is 1.9x
  faster per point on one thread and 2.3x on four. AUTO-07p's threads scale
  3.55x on 4, xppautX's 3.0x (FD) and 2.5x (exact): this larger model leaves
  more of the work in the collocation than the 8-state PY_S1Bf.
- The integration before AUTO (Go then Last, 1200 ms): XPPAUT 21.5 s,
  xppautX interpreted 14.3 s, compiled 4.3 s, and 0.9 s when the exact
  Jacobian is chosen, which backward Euler then uses too.


