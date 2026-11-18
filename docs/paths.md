# Paths used often

Where things are, so nobody searches for them (maintainer, 2026-11-06). A path
you had to search for goes here once you find it. **local** means git-ignored
or outside the repository: it exists on the machine named, not in a clone.

Checkouts: the Ubuntu machine's is `/home/muhammad/muhammad/git_repos/xppautX`
(`~` below is `/home/muhammad`); the Windows laptop's is `C:\gitRepos\xppautX`.

## The board

| What | Path |
|---|---|
| Cards, their order and status | `docs/roadmap.md` (the plan at its top) |
| Include graph of core/ (cycles, reach, modules), `tools/depgraph.py` | `docs/deps.html` |
| Studies and design proposals | `docs/studies/` (`w<card>-<topic>.md`) |
| XPPAUT's bugs and limits | `docs/xppaut-findings.md` |
| AUTO-07p's bugs | `docs/auto07p-findings.md` |
| What differs from XPPAUT | `docs/xppautx-vs-xppaut.md` |
| Background-task register (local) | `.claude/background-tasks.md` in the main checkout |
| Card worktrees (local) | `../xppautX-W<card>` beside the main checkout |
| Task-board scripts (local) | `~/.claude/skills/task-board/scripts/` (`codex_limits.py --probe`, `codex_card.sh`, `merge_card.sh`) |

## The comparison (W354)

| What | Path |
|---|---|
| The kit: drivers, report script, how to run | `paper/benchmarks/comparison/` (`run.py`, `final_report.py`, `watch.py`, `README.md`); a results folder is per unit: `<study>/<unit>/attempt-NNN/` and `done.json` (W699) |
| The server study (W295: `--server` from Python against pure Python, 1-16 processes) | `paper/benchmarks/comparison/server.py` (units `lecar`, `lecdiff200`, `wave`), its worker `server_worker.py`, its report `server_report.py`, the shared row comparison `tools/exportcompare.py`; run it with `run.py --study server` |
| The rerun plan (local) | `paper/benchmarks/rerun-plan.md` |
| **The frozen final run (W557)** | deleted 2026-11-17 (W807, the maintainer's yes) with W617's superseded folders and the aborted starts; what study scripts read of it is kept in `~/xppautx-comparison/results/extra-runs/w557-frozen/` (continuation E1, ode_mu26_blocked and ode_mu26_control; large-stiff's results); its committed report stays |
| **The comparison's folder of record (local, Ubuntu)** | `~/xppautx-comparison/results/` (W807, #888): W617's final run in the per-unit layout `<study>/<unit>/attempt-NNN/` with `done.json`, `extra-runs/` and `overlays/`, every file a real hard link (one inode each) made by `paper/benchmarks/comparison/build_results.py`; what `final_report.py --final`, `tools/test_final_report.py` and `run.py --reuse` read. It replaces `w617-units/`, `w617-final/`, `w617/`, `w617-rerun/` and the `w617-aborted-*` folders (deleted 2026-11-17, the maintainer's yes); binary `~/xppautx-comparison/build/xppautX-5975912bc81b/xppautX`; report `paper/benchmarks/results/report-w617/report.html` (main checkout) |
| The binaries it ran (local, Ubuntu) | `~/xppautx-comparison/build/` (`xppautX-<commit>`, `xppaut-8.0`, `auto-07p`) |
| **The latest report (local)** | `paper/benchmarks/results/report-20261102/report.html` (W589; its data `data/report.json`, figures `figures/paper/`, `figures/appendix/`) |
| Earlier reports (local) | `paper/benchmarks/results/report-<date>/` |
| The paper | `paper/main.tex`, `paper/figures/`, `paper/refs.bib` |
| Large stiff models' kit (local) | `paper/benchmarks/large-stiff/`; SUNDIALS 7.9.0 with KLU in `~/w378-deps/sundials-install` |
| The timed run's machine check | `tools/comparison-setup.sh --ready` |

## References (local, git-ignored)

| What | Path |
|---|---|
| XPPAUT 8.0, the source xppautX forked | `reference/xppaut-8.0/` |
| XPPAUT's GitHub master | `reference/xppaut-master/` |
| AUTO-07p's source | `reference/auto-07p/` (Fortran in `src/`) |

## Builds and checks

| What | Path |
|---|---|
| WSL's verify clone (Windows laptop, local) | `~/.cache/xppautx-verify/<checkout folder>` |
| Example outputs' md5 baseline | `tests/examples.md5` |
| autoX's diagram baselines | `tests/autox/*.diagram.csv` |
| Golden pictures | `tests/golden/` |
