# The comparison machine (W413, #475)

The comparison studies (W354 stage 2, W378) run on the Ubuntu machine, not on the
development laptop: the studies need the machine to themselves for hours, and the laptop
overheats. This page is for whoever sets that machine up. The studies' drivers and plan are
in `paper/` (git-ignored; how it gets there is the maintainer's choice).

## Steps

1. A clone of this repository on the machine's own disk (not a mounted or network disk), at
   the commit the studies name, with no uncommitted tracked change.
2. `tools/comparison-setup.sh --check` lists every missing tool and prints the one
   `sudo apt install ...` line for them, and the install line of `uv` if it is missing
   (`curl -LsSf https://astral.sh/uv/install.sh | sh`). The script never runs sudo, apt or an
   installer: run the lines yourself. Use gcc/g++/gfortran 15 if the distribution has them (the 2026-10-11 runs used
   15.2); another version is recorded in the machine record and is a decision to state with
   the results, not a failure.
3. `tools/comparison-setup.sh` (no argument) then does, in order and stopping at the first
   failure:
   - `--check`, as above;
   - `--sources`: rebuilds the git-ignored `reference/`: `xppaut-8.0` (this repository's
     `git archive c021b51...`), `xppaut-master` (ermentrout/xppaut at its tip, 9068d8a) and
     `auto-07p` (auto-07p/auto-07p at 44cc1c4: 0.9.4, untagged). Each tree is compared with
     a hash pinned in the script; a tree that already has it is kept, one that has not is a
     refusal (nothing is deleted: move it away);
   - `--python`: `uv sync --locked`, which makes `.venv` in the checkout with the Python
     modules the studies need (numpy, matplotlib, mpmath, scipy) at the versions `uv.lock` holds, the
     same on every machine; `--locked` refuses a lock that disagrees with `pyproject.toml`.
     The Python itself is `.python-version`'s, the latest stable release (3.14; W420), which
     uv installs where the system has another;
   - `--build`: xppautX (`git archive HEAD`, `WINDOW=0`, `-g -O2`, no ccache), XPPAUT 8.0
     (W238's command), AUTO-07p (configure `--enable-gui=no --enable-plaut=no
     --enable-plaut04=no`, as W269; built serially, its Makefile races under `-j`) under
     `~/xppautx-comparison/build` (`--root DIR` changes it). A build with the right stamp is
     kept. libtcc is vendored in `third_party/tinycc` and is built with xppautX;
   - `--record`: `~/xppautx-comparison/provenance/machine-<UTC>.json`: CPU model, online
     CPUs, `lscpu -e`, SMT, kernel, governor, scaling driver, turbo, memory, compilers, tool
     versions, uv's version and the installed Python modules' versions, the source commits and tree hashes. Copy it beside the results of every
     series.
4. Before every timed series: `tools/comparison-setup.sh --ready`. It refuses unless the
   governor is `performance` (set it as root: `sudo cpupower frequency-set -g performance`)
   and no other process uses more than 5% of one CPU during a 0.5 s sample. This
   sample sees only processes running at that moment; it cannot predict work
   started later. Nothing else runs on the machine during
   a series.

## Running the Python drivers

The root of the checkout holds `pyproject.toml` and `uv.lock` (not a package: only the
dependencies numpy, matplotlib, mpmath and scipy; the repository's own checks use the standard library
and do not need it). uv finds it from any folder inside the checkout, `paper/` included. Run a
driver with `uv run python PATH ARGS...` (for example `uv run python
tools/auto-profile/eig_accuracy.py DIR`, or `uv run python tools/python-integration/integrate.py
model.odex --outfile output.dat`), not with the system `python3`, so the versions are the
locked ones. A driver that needs only the standard library runs either way. To change a version,
edit `pyproject.toml`, run `uv lock`, and commit both files; a series states the versions its
machine record lists.

The Python integration comparison (W427, [#489](https://github.com/MuhammadMoustafa/xppautX/issues/489)):
Python is a timed program of it, with xppautX's own methods written in Python with numpy
(`tools/python-integration/`; no SciPy in them), and SciPy's DOP853 at rtol 1e-12 is the separate,
untimed accuracy reference. The scripts time nothing; the timing series is W354 stage 2's.
- `uv run python tools/python-integration/integrate.py MODEL.odex [--method KEY] [--setfile FILE] [--outfile FILE]`
  runs the model and writes the columns xppautX's `output.dat` has, at the same stored times. The
  method is the model's own unless `--method` names another of `euler`, `runge_kutta`, `discrete`,
  `backward_euler`, `qualrk`, `dopri8` (the keys a `.odex` writes). Each method does the operations of its
  routine in `core/` in the same order (`odesol2.cpp`, `stiff.cpp`, `dormpri.cpp`, with the events of
  `flags.cpp` and the dense events of `integrate.cpp`); the settings are the model's options over
  xppautX's own defaults, which are read from `core/` and not written again, and `--setfile` takes an
  XPPAUT set file as xppautX does (numerics, initial values, parameters).
- `uv run python tools/python-integration/scipy_integrate.py MODEL.odex [--method KEY] [--setfile FILE] [--outfile FILE]`
  runs SciPy's DOP853 on the model (to each event, the event's assignments made, and on) and writes
  the same columns at the stored times of the run of `--method`. A map has no solution to integrate.
- `uv run python tools/python-integration/check.py --bin ./xppautX [--only NAME,...] [--method KEY,...|all] [--model FILE] [--screen DIR]`
  runs xppautX and the Python run on the comparison's models (the five-model screen, from
  `paper/benchmarks/five-model-screen/models` or `--screen`, Izhikevich's 28 sets, `ma23`, `mu26`) and
  **fails** unless both have the same rows and time column and every value is within a step of the
  single precision float xppautX stores (and the rounding of the `%.8g` it prints). The two do the
  same arithmetic, so only the last bit of a library function (xppautX's are correctly rounded) can
  differ. A model that amplifies that bit (`episodic`'s irregular firing, a spike's upstroke under
  `dopri8`) must agree as far as Python runs with every derivative off by up to an ulp agree with the
  unperturbed run; the line says to which time. With `--method` the methods are run on the chosen
  models (xppautX on a copy of the model with that method); each line also gives the distance of the
  run from DOP853 and, for a model with events, from its spike times. A run takes minutes (DOP853 at
  1e-12 on `episodic` and `mu26` most of it); `--only` runs a part.

`--hashes` prints the tree hashes of `reference/` to update the pins when a pinned source
changes on purpose.

## W354 stage 2: one foreground sequence (W439, #501)

Copy the entire current `paper/` beside this clean checkout on Ubuntu, including
W269's saved results and `benchmarks/large-stiff/`. Use the final merged tip;
W269's tip is never rebuilt or timed. The commands below are for the maintainer
on that machine. W439 installs nothing and runs no timing series on the laptop.
Run `--check` first if dependencies are missing; it prints the additional apt/uv
install lines to run before proceeding.

```sh
tools/comparison-setup.sh --check
# Run the apt/uv lines it prints, then W378's additional install line:
sudo apt install build-essential cmake libsuitesparse-dev time valgrind
tools/comparison-setup.sh

# W378's pinned CVODE 7.9.0, with KLU. Keep the archive and CMakeCache.txt.
mkdir -p "$HOME/w378-deps"
curl -fL https://github.com/LLNL/sundials/releases/download/v7.9.0/cvode-7.9.0.tar.gz \
  -o "$HOME/w378-deps/cvode-7.9.0.tar.gz"
printf '%s  %s\n' ae1602334e17b410462fda3d08cfe6ec4610f2e76e50f775fd64c1a4a5671a8e \
  "$HOME/w378-deps/cvode-7.9.0.tar.gz" | sha256sum -c -
tar -xf "$HOME/w378-deps/cvode-7.9.0.tar.gz" -C "$HOME/w378-deps"
cmake -S "$HOME/w378-deps/cvode-7.9.0" -B "$HOME/w378-deps/sundials-build" \
  -DCMAKE_INSTALL_PREFIX="$HOME/w378-deps/sundials-install" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS='-ffp-contract=off' \
  -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF \
  -DSUNDIALS_PRECISION=DOUBLE -DSUNDIALS_INDEX_SIZE=64 \
  -DSUNDIALS_ENABLE_CVODE=ON -DSUNDIALS_ENABLE_MPI=OFF -DSUNDIALS_ENABLE_OPENMP=OFF \
  -DSUNDIALS_ENABLE_KLU=ON -DSUNDIALS_ENABLE_SUNLINSOL_KLU=ON \
  -DKLU_INCLUDE_DIR=/usr/include/suitesparse \
  -DSUNDIALS_ENABLE_C_EXAMPLES=OFF -DSUNDIALS_ENABLE_CXX_EXAMPLES=OFF \
  -DSUNDIALS_ENABLE_EXAMPLES_INSTALL=OFF
cmake --build "$HOME/w378-deps/sundials-build" -j4
cmake --install "$HOME/w378-deps/sundials-build"
uv run python paper/benchmarks/large-stiff/build.py \
  --sundials "$HOME/w378-deps/sundials-install" --out build/w378

# Set performance governor as root, fix turbo on or off, then leave the machine idle.
sudo cpupower frequency-set -g performance
TIP=$(git rev-parse --short=12 HEAD)
XX="$HOME/xppautx-comparison/build/xppautX-$TIP/xppautX"
XPP="$HOME/xppautx-comparison/build/xppaut-8.0/xppaut"
export AUTO_DIR="$HOME/xppautx-comparison/build/auto-07p"
OUT="paper/benchmarks/results/w354-ubuntu-$(date -u +%Y%m%d)"
uv run python paper/benchmarks/comparison/run.py \
  --out "$OUT" --xppautx "$XX" --xppaut "$XPP" --auto-dir "$AUTO_DIR" \
  --worker build/w378/worker
```

This runs, serially: Terman stage 0 and convergence; screen plus sandbox;
ModelDB integration; Mu26/Ma23 integration; all 28 Izhikevich sets; W427 Python
methods (and untimed DOP853); bifurcation; size scaling; the unchanged W378 kit.
Integration has no thread rows. Continuation has 1/2/4/auto for xppautX,
1/2/4 for AUTO-07p, both diagram delivery choices, FD headlines and the
settled exact rows. Physical cores come from that study's machine record,
one CPU per socket/core pair. No SMT sibling is used as another core.

Each study gets `STUDY/attempt-NNN/` with its own `machine.json`, provenance,
input hashes, raw outputs, `runs.json`, numerical gates and summaries. The
root `status.json` records commands, failures and completed studies. A failed
study does not erase another's results or prevent the later studies running.
Repeat the same command to resume; add `--study continuation` (or any study
name printed by `--help`) to retry one study only. A retry gets a new attempt;
completed studies are retained. Another tip, binary, driver or selection
requires another `--out`. The runner calls `--record` for every study and
`--ready` before every timed series. Readiness refusal is a failure to resolve,
with no automatic sleep/retry. Run in the foreground of tmux; no concurrent runs.

`summary.json` and `summary.csv` use five successes and sample SD (`ddof=1`);
partial or numerically unequal rows get no timing summary. Warm-ups are kept
as repetition 0. Raw run-window wall/CPU and seconds per point are separate
from time-to-ready; AUTO-07p also records wrapper wall versus executable
CPU (`RUSAGE_CHILDREN`, after equation compilation). Integration's run row is
the full silent process, as in W269; its one-step ready row is separate.
XPPAUT's final timed key has no key delay; file completion is observed by the
shared driver's condition poll, so its wall resolution includes that poll.
No time, throughput, pilot size or readiness observation is a numerical gate.

Terman refuses timing without `comparison/terman-criteria.json` and a passing
convergence record, applies the selected tolerance and checks the timed runs'
readouts against DOP853. A failure stays a reported convergence failure.
Its claim is numerical convergence of the archive's behaviour, neither a
Figure 3 reproduction nor validation of the 31 repaired SET labels' intent.
W269's SMT-thread timings remain history; current event digests are compared
with its saved digests, with changes or differing windows reported explicitly.
`modeldb-267710/auto07p/odex_to_c.py` is retired; every FD AUTO-07p equation
comes from `tools/auto_reference.py`. AUTO-07p's exact rows (`JAC = 1`) run P1 and E1 from PY_S1Bf's C file and P2, E2 and T1
from one generated from the model with its derivatives, checked against central differences (W459); exact rows are judged against a
converged exact run, and every row carries its error against it (W457, docs/auto-references.md).
The default P2 window ends at ampPar .62, before XPPAUT's spurious BP near .6211;
all programs use that parameter bound, not a matching point count.

Planning allowance, **not Ubuntu timing evidence**, excluding installs/builds
and manual readiness waits; revise it from the machine's pilots:

| Study | Allowance |
|---|---:|
| Terman saved readouts and full convergence axes | 0.5–3 h |
| Screen/sandbox and ModelDB integration | 0.25–1 h |
| Mu26/Ma23 and 28 Izhikevich integration cases | 0.5–2 h |
| W427 Python methods and accuracy references | 3–20 h |
| All continuation workloads, Jacobians, events and threads | 5–30 h |
| Size scaling | 1–3 h |
| W378 check, pilot, measure, cache | 0.5–2.1 h |
| **Total (rounded)** | **11–62 h** |

The Python method matrix and Mu26's full continuation/thread/Jacobian matrix
dominate. W269's small timing allowance omitted those rows. A nonconvergent
fixed-step pilot or failed agreement gate is reported without equal-work
timings; this can shorten the run but does not establish a speed advantage.
