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
   and the 1-minute load average is below 0.3. Nothing else runs on the machine during
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
