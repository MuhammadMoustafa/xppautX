#!/usr/bin/env -S uv run --locked python
"""W248: markdown tables of the measured repetitions from results/*/summary.json.

    uv run --locked python tables.py results/wsl-xppaut results/wsl-xppautx ...
"""
import json, sys

NAMES = {'same': 'same job', 'diff': 'different ParMin'}
for p in sys.argv[1:]:
    s = json.load(open(p + '/summary.json'))
    print(f'\n### {p}\n')
    print('| Jobs | N | Mode | Wall s, mean +/- SD | Complete groups | Runs | Failed | Different diagram | Equal | Speed-up |')
    print('|---|---:|---|---:|---:|---:|---:|---:|---:|---:|')
    for key, row in s.items():
        kind, n = key.rsplit('-N', 1)
        for mode in ('alone', 'concurrent', 'sequential'):
            if mode not in row:
                continue
            x = row[mode]
            w = x['wall']
            wall = f"{w['mean']:.2f} +/- {w['sd']:.2f}" if w and w['sd'] is not None else (f"{w['mean']:.2f}" if w else 'n/a')
            surv = x['wall_of_survivors']
            if not w and surv:
                wall += f" (survivors {surv['mean']:.1f})"
            sp = f"{row['speedup']:.2f}" if mode == 'concurrent' and 'speedup' in row else ''
            print(f"| {NAMES[kind]} | {n} | {mode} | {wall} | {x['groups_complete']}/{x['groups']} | {x['runs']} | "
                  f"{x['runs_failed']} | {x['runs_different']} | {x['runs_equal']} | {sp} |")
