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
     modules the studies need (numpy, matplotlib, mpmath) at the versions `uv.lock` holds, the
     same on every machine; `--locked` refuses a lock that disagrees with `pyproject.toml`;
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
dependencies numpy, matplotlib and mpmath; the repository's own checks use the standard library
and do not need it). uv finds it from any folder inside the checkout, `paper/` included. Run a
driver with `uv run python PATH ARGS...` (for example `uv run python
tools/auto-profile/eig_accuracy.py DIR`), not with the system `python3`, so the versions are the
locked ones. A driver that needs only the standard library runs either way. To change a version,
edit `pyproject.toml`, run `uv lock`, and commit both files; a series states the versions its
machine record lists.

`--hashes` prints the tree hashes of `reference/` to update the pins when a pinned source
changes on purpose.
