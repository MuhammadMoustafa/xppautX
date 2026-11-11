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
| The kit: drivers, report script, how to run | `paper/benchmarks/comparison/` (`run.py`, `final_report.py`, `README.md`) |
| The rerun plan (local) | `paper/benchmarks/rerun-plan.md` |
| **The frozen final run (local, Ubuntu)** | `~/xppautx-comparison/final/` (one folder per study: `screen`, `continuation`, `python`, `scaling`, `large-stiff`, ...; `status.json`) |
| **W617's final run (local, Ubuntu)** | `~/xppautx-comparison/w617-final/` (composed: `status.json` maps each study to its attempt folder in `~/xppautx-comparison/w617/` and `w617-rerun/` (continuation, large-stiff), `<study>/attempt-001` links to it; `extra-runs/`: w591 = the frozen rows with xppautX's rerun on its binary, W594, the rest linked to `final/`); binary `~/xppautx-comparison/build/xppautX-5975912bc81b/xppautX`; report `paper/benchmarks/results/report-w617/report.html` (main checkout) |
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
