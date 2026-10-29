#!/usr/bin/env -S uv run --locked python
"""W252 (#308): how far the exact-Jacobian AUTO branches are from the finite-difference
ones, from ../auto-parallel/results/NAME/runs.json: per run (steady, periodic), the
number of points, the labels, and the largest difference in x (Iapp) and y (the
plotted norm) between points of the same number.

    uv run --locked python compare.py NAME [REFERENCE_VARIANT]
"""
import json, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
runs = json.loads((HERE.parent / 'auto-parallel' / 'results' / sys.argv[1] / 'runs.json').read_text())
ref_name = sys.argv[2] if len(sys.argv) > 2 else 'pure-fa'
first = {}
for r in runs:
    first.setdefault(r['variant'], r)
ref = first[ref_name]
for name, r in first.items():
    for run in ('steady', 'periodic'):
        a, b = ref[run]['points'], r[run]['points']
        n = min(len(a), len(b))
        dx = max(abs(p[2] - q[2]) for p, q in zip(a[:n], b[:n]))
        dy = max(abs(p[3] - q[3]) for p, q in zip(a[:n], b[:n]))
        print(f"{name:12s} {run:8s} {len(b):4d} points (ref {len(a)}), labels {r[run]['labels']}, "
              f"max |dIapp| {dx:.3g}, max |dy| {dy:.3g}")
