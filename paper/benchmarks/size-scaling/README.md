# Size scaling: XPPAUT 8.0 and xppautX (W241, #297; Windows W242, #298)

Question (maintainer, 2026-10-09): does xppautX's speed advantage grow with
the size of the model? The five-model screen
([../five-model-screen](../five-model-screen/README.md), W238) timed whole
processes, so start-up, loading and output were mixed with integration, and
its models differed in more than size.

## Design

- Three families, each one of XPPAUT's own bundled examples resized
  (`scaling.py`, `FAMILIES`): `kuramoto` (kuramot100.ode, all-to-all `sum`,
  n^2 terms per evaluation, RK4), `lecdiff` (lecdiff200.ode, a diffusive
  Morris-Lecar chain, 2n states, RK4), `amari` (amari.ode, a neural field
  through a `conv` special, Euler). Sizes stay under MAXODE (5000 variables,
  the same limit in both programs).
- Each case runs at three durations: one step (`step`: start-up, load,
  output set-up), `T` and `2T`. Both T and 2T write 200 rows (`nout`), so
  output costs the same in both. Integration cost per unit of model time is
  (mean 2T - mean T) / T; the fixed cost is the one-step mean.
- `calibrate` picks T per case so XPPAUT integrates about 3 s in a 2T run
  (probes, `calibration.json`), writes `models/` (the .ode and xppautX's
  conversion of it) and `cases.json`. Every platform runs these same files.
- `run`: one excluded warm-up and five measured fresh processes per case,
  duration and engine, engine order alternating; one pinned CPU (2), fresh
  HOME, no DISPLAY, LC_ALL=C. The repetition-1 outputs of T and 2T are
  compared between the programs with the screen's `compare` (exact, or the
  screen's tolerance 5e-6 + 5e-5|x|); every output's hash (CRs removed) is
  kept to show each program repeats itself and to compare platforms.
- Ratios: `integration_ratio` = XPPAUT's per-time cost / xppautX's (> 1:
  xppautX integrates faster), with its standard error from the two means'
  standard errors; `whole_run_ratio_2T` is the plain wall-time ratio of the
  2T runs; `fixed_difference_s` is xppautX's one-step time minus XPPAUT's;
  `linearity` (T - step) / (2T - T) near 1 says the cost is linear in T.

## Builds

WSL (`results/wsl`): the pinned commits and build commands of
[../five-model-screen/provenance.json](../five-model-screen/provenance.json)
(XPPAUT 8.0 c021b511, xppautX 88e09741, gcc 15.2, -g -O2,
-ffp-contract=off), built by that folder's `build.py`.

Windows (`results/windows`, W242): XPPAUT 8.0 and xppautX.exe, both built with MSYS2 UCRT gcc 16, 64-bit, -g -O2
-ffp-contract=off at the pinned commits (XPPAUT natively through
[../windows-build/build.sh](../windows-build/build.sh), W244 #300; the
distributed 32-bit XPPAUT was not used for timing).

## Reproduce

    uv run --locked python scaling.py calibrate --legacy XPPAUT --modern XPPAUTX   # once; refuses an existing models/
    uv run --locked python scaling.py run --legacy XPPAUT --modern XPPAUTX --result-name NAME
    uv run --locked python scaling.py summarize --result-name NAME

Run on a quiet machine, one heavy job at a time; under WSL the work folders
are in /tmp (ext4).

## Results (2026-10-09)

20 cases x 3 durations x 2 programs x (1 warm-up + 5) runs per platform, all
valid; every program repeats its own output exactly; the two programs'
outputs are identical at every size on both platforms, and each program's
outputs are identical between WSL and Windows (60/60 hashes each). Figure:
`../../figures/size-scaling.pdf` (`../plots.py size-scaling`).

Integration speed, XPPAUT time / xppautX time (> 1: xppautX faster),
inverse-variance mean over each family's sizes:

| Family | WSL (gcc 15.2) | Windows (UCRT gcc 16) |
|---|---:|---:|
| kuramoto (10-400 states) | 1.053 +/- 0.028 | 0.966 +/- 0.006 |
| lecdiff (50-4800 states) | 1.079 +/- 0.020 | 1.042 +/- 0.005 |
| amari (100-2400 states) | 1.037 +/- 0.020 | 1.026 +/- 0.001 |

- The maintainer's hypothesis (xppautX's advantage grows with size) is not
  supported: no family's ratio rises with size on either platform. xppautX
  integrates 3-8% faster on lecdiff and amari; on kuramoto (sin-heavy) it is
  5% faster on WSL and 3-5% slower on Windows at 200-400 oscillators (its
  correctly rounded sin against UCRT's is the likely reason; not measured).
- Start-up and load (one-step run) are where xppautX is slower: about 25 ms
  more on a small model on both platforms, growing with size (WSL lecdiff
  4800 states: 817 vs 627 ms; Windows: 718 vs 623 ms). Both programs' load
  time grows faster than linearly. Card W245 (#301).
- W238's "slower" lecdiff200, amari and clustor were whole-process timings
  of runs of 0.2-0.6 s, where this fixed cost decides the total.
- Windows timings are far less noisy than WSL's (standard errors 0.001-0.06
  against 0.02-0.17).
- These fixed-step models (RK4, Euler) give the same output on WSL and
  Windows in XPPAUT too: last-bit differences between math libraries do not
  grow there, and the output keeps six digits. Terman's adaptive QualRK does
  amplify them (../windows-build/terman-platforms, handoff section 7a).


## W269 (#326, 2026-10-11): the compiled build, three engines, past MAXODE

The same study after xppautX began compiling each model at load (W258/W259/
W266, TinyCC; `--no-compile` interprets) and lost its MAXODE (W243). Engines:
XPPAUT 8.0, xppautX compiled (`modern`), xppautX `--no-compile` (`interp`),
the order rotating each repetition (`scaling.py run --engines
legacy,modern,interp`; two engines keep W241's alternation).

- Cases: W241's 20, restaged (`scaling.py restage`: the same `.ode` files
  into `models-w269/`, converted again by the W269 build, since today's xppautX
  refuses the older `.odex`; `cases-w269.json`, the same T), plus four sizes
  past XPPAUT's MAXODE of 5000 variables (`scaling.py extend --sizes
  lecdiff:3200,6400 amari:3200,6400`): lecdiff 6400 and 12800 states, amari
  3200 and 6400 (6400 and 12800 variables with its fixed ones), T sized on
  xppautX's interpreter (`--probe-engine interp`, about 3 s of integration in
  a 2T run, as W241 sized on XPPAUT). XPPAUT refuses each of them (the case's
  `legacy: false`, its words in `legacy_refusal`); they run on xppautX alone.
  Larger sizes were not added: lecdiff 12800's one-step run already takes
  0.7 s and kuramoto's cost grows as n^2.
- Builds: WSL as `../five-model-screen/provenance-w269.json` (XPPAUT 8.0
  c021b511, xppautX `0643234f`, gcc 15.2); Windows XPPAUT 8.0 as W242 (the
  same binary, sha256 `5b0bf424...`) and xppautX.exe `0643234f` built with
  UCRT gcc 16 (`mingw32-make -j4 xppautx BUILDDIR=build/ucrt WERROR=1`).
  Each result folder has `provenance.json`.
- `summarize` gives each pair's ratios (`integration_ratio` is XPPAUT/compiled
  as before; `_interp_modern`, `_legacy_interp` the others) and checks each
  pair's outputs; it reproduces W241's `summary.json` exactly from W241's
  runs.

Results: `results/w269-wsl-2026-10-11/`, `results/w269-windows-2026-10-11/`
(`runs.json`, `comparison.json`, `summary.json`/`.csv`, `run.log`,
`provenance.json`). Every run valid; every output of the three engines
identical at every size, on both platforms (`outputs` below: each pair exact).

Integration speed, WSL (per-time cost ratio, > 1: the second engine faster;
one-step run = start-up, load and output set-up):

| Family | Size | States | XPPAUT / compiled | --no-compile / compiled | XPPAUT / --no-compile | one-step run ms (XPPAUT, compiled, --no-compile) | outputs |
|---|---:|---:|---:|---:|---:|---|---|
| kuramoto | 10 | 10 | 2.30 +/- 0.07 | 2.10 +/- 0.06 | 1.10 +/- 0.01 | 6, 23, 21 | exact |
| kuramoto | 25 | 25 | 2.40 +/- 0.02 | 2.16 +/- 0.02 | 1.11 +/- 0.01 | 6, 24, 23 | exact |
| kuramoto | 50 | 50 | 2.39 +/- 0.03 | 2.15 +/- 0.02 | 1.11 +/- 0.01 | 8, 26, 25 | exact |
| kuramoto | 100 | 100 | 2.25 +/- 0.04 | 2.05 +/- 0.04 | 1.10 +/- 0.01 | 13, 32, 31 | exact |
| kuramoto | 200 | 200 | 2.26 +/- 0.02 | 2.08 +/- 0.02 | 1.08 +/- 0.01 | 32, 46, 51 | exact |
| kuramoto | 400 | 400 | 2.29 +/- 0.01 | 2.16 +/- 0.03 | 1.06 +/- 0.02 | 102, 91, 122 | exact |
| lecdiff | 25 | 50 | 3.72 +/- 0.03 | 3.41 +/- 0.03 | 1.09 +/- 0.01 | 7, 25, 23 | exact |
| lecdiff | 50 | 100 | 3.74 +/- 0.02 | 3.39 +/- 0.01 | 1.10 +/- 0.01 | 8, 27, 25 | exact |
| lecdiff | 100 | 200 | 3.62 +/- 0.03 | 3.38 +/- 0.12 | 1.07 +/- 0.04 | 11, 32, 29 | exact |
| lecdiff | 200 | 400 | 3.54 +/- 0.02 | 3.22 +/- 0.02 | 1.10 +/- 0.01 | 18, 42, 37 | exact |
| lecdiff | 400 | 800 | 3.22 +/- 0.30 | 2.92 +/- 0.27 | 1.10 +/- 0.01 | 38, 61, 54 | exact |
| lecdiff | 800 | 1600 | 3.53 +/- 0.05 | 3.27 +/- 0.05 | 1.08 +/- 0.01 | 105, 101, 84 | exact |
| lecdiff | 1600 | 3200 | 3.64 +/- 0.13 | 3.44 +/- 0.15 | 1.06 +/- 0.03 | 288, 186, 163 | exact |
| lecdiff | 2400 | 4800 | 3.67 +/- 0.19 | 3.73 +/- 0.20 | 0.98 +/- 0.02 | 595, 284, 244 | exact |
| amari | 100 | 100 | 1.04 +/- 0.01 | 1.00 +/- 0.01 | 1.03 +/- 0.01 | 9, 28, 26 | exact |
| amari | 200 | 200 | 0.99 +/- 0.01 | 0.96 +/- 0.01 | 1.03 +/- 0.01 | 15, 36, 32 | exact |
| amari | 400 | 400 | 0.98 +/- 0.01 | 0.96 +/- 0.01 | 1.02 +/- 0.01 | 29, 48, 42 | exact |
| amari | 800 | 800 | 0.96 +/- 0.01 | 0.94 +/- 0.01 | 1.03 +/- 0.01 | 68, 72, 60 | exact |
| amari | 1600 | 1600 | 0.91 +/- 0.01 | 0.89 +/- 0.01 | 1.02 +/- 0.01 | 171, 123, 106 | exact |
| amari | 2400 | 2400 | 0.88 +/- 0.01 | 0.88 +/- 0.01 | 1.00 +/- 0.01 | 341, 192, 150 | exact |
| lecdiff | 3200 | 6400 | n/a | 2.45 +/- 0.15 | n/a | -, 372, 318 | exact |
| lecdiff | 6400 | 12800 | n/a | 2.27 +/- 0.16 | n/a | -, 762, 658 | exact |
| amari | 3200 | 3200 | n/a | 0.88 +/- 0.01 | n/a | -, 240, 192 | exact |
| amari | 6400 | 6400 | n/a | 0.87 +/- 0.01 | n/a | -, 497, 391 | exact |

| Family | XPPAUT / compiled | --no-compile / compiled | XPPAUT / --no-compile |
|---|---:|---:|---:|
| kuramoto (10-400 states) | 2.301 +/- 0.008 | 2.135 +/- 0.010 | 1.099 +/- 0.004 |
| lecdiff (50-4800 states) | 3.628 +/- 0.010 | 3.336 +/- 0.011 | 1.094 +/- 0.003 |
| lecdiff past MAXODE (6400-12800 states) | n/a | 2.365 +/- 0.108 | n/a |
| amari (100-2400 states) | 0.948 +/- 0.003 | 0.942 +/- 0.002 | 1.022 +/- 0.003 |
| amari past MAXODE (3200-6400 states) | n/a | 0.876 +/- 0.009 | n/a |

Windows, the family means (the per-size table: `../w269_tables.py scaling`):

| Family | XPPAUT / compiled | --no-compile / compiled | XPPAUT / --no-compile |
|---|---:|---:|---:|
| kuramoto (10-400 states) | 2.037 +/- 0.026 | 2.176 +/- 0.026 | 0.956 +/- 0.008 |
| lecdiff (50-4800 states) | 3.335 +/- 0.041 | 3.373 +/- 0.036 | 1.002 +/- 0.009 |
| lecdiff past MAXODE (6400-12800 states) | n/a | 2.291 +/- 0.042 | n/a |
| amari (100-2400 states) | 1.006 +/- 0.008 | 0.995 +/- 0.005 | 1.003 +/- 0.008 |
| amari past MAXODE (3200-6400 states) | n/a | 0.950 +/- 0.015 | n/a |

- No ratio rises with size, as in W241: the compiled build's advantage is a
  constant factor per family. kuramoto (sin-heavy, n^2 sum) 2.3x faster than
  XPPAUT on WSL, 2.0x on Windows; lecdiff 3.6x / 3.3x; amari none (0.95x /
  1.01x).
- amari is slower compiled than interpreted (WSL 0.94x, falling to 0.87x
  past MAXODE; Windows 0.99x and 0.95x): its time is the `conv` network,
  which is not compiled, and the compiled build adds cost around it. Reported
  as a speed problem (W269 changes no code).
- lecdiff's compile speed-up drops past MAXODE (2.9-3.7x up to 4800 states,
  2.2-2.5x at 6400 and 12800, both platforms).
- Start-up: at small sizes xppautX's one-step run is still ~17-20 ms longer
  than XPPAUT's (WSL); from about 1600 states on it is shorter (lecdiff 4800
  states: 284 ms compiled against XPPAUT's 595 ms on WSL, 341 against 739 on
  Windows), the reverse of W241 (817 vs 627 ms): W245's work. Compiling at load
  costs up to about 100 ms more than `--no-compile` (lecdiff 12800 states: 762 vs 658
  ms).
- Windows had one-off stalls (lecdiff100 T compiled and legacy, lecdiff200 T
  compiled: one run 1-1.4 s longer than its five siblings), kept as measured:
  they inflate those sizes' standard errors (6.6, 38.7), so the
  inverse-variance family means hardly weigh them.
