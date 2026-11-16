#!/usr/bin/env -S uv run --locked python
"""The server study's report section (W295, #353): from a run.py results folder, per model, the sweep's throughput (runs per second) of each
arm at each process count, its speed-up over one process, and the per-run time and start-up of one process, where the persistent
--server's saving against a process a run (and against the Python arms) is read. Read-only.

    uv run --locked python paper/benchmarks/comparison/server_report.py RESULTS [--out FILE.md]

Only repetitions the study counted are in the tables (a --check run has none: its tables say so). The means are the study's
summary.json's, with its sample SDs in the run's own folder.
"""
import argparse
import json
from pathlib import Path
import re

from common import done_attempts

ARMS = ('xppautx_server', 'xppautx_silent', 'export_python', 'export_python_numpy', 'export_python_numba', 'scipy')


def table(header, rows):
    return ['| ' + ' | '.join(header) + ' |', '|' + '---|' * len(header)] + ['| ' + ' | '.join(r) + ' |' for r in rows]


def number(value, digits=3):
    return '' if value is None else '%.*g' % (digits, value)


def unit_section(unit, attempt):
    summary = json.loads((attempt / 'summary.json').read_text()) if (attempt / 'summary.json').exists() else []
    by = {}
    for r in summary:
        by[r['engine'], int(re.search(r'-n(\d+)$', r['workload']).group(1))] = r
    out = ['## %s' % unit, '']
    counts = sorted({n for _, n in by})
    arms = [a for a in ARMS if any(k[0] == a for k in by)]
    if not by:
        return out + ['No counted repetitions (a check run, or the unit did not finish).', '']
    out += ['Sweep throughput, runs per second (speed-up over one process):', '']
    rows = []
    for arm in arms:
        one = by.get((arm, 1), {}).get('wall_per_point_mean')
        cells = []
        for n in counts:
            s = by.get((arm, n), {}).get('wall_per_point_mean')
            cells.append('' if not s else '%s (%s x)' % (number(1 / s), number(one / s, 2) if one else '-'))
        rows.append([arm, *cells])
    out += table(['arm', *['%d proc' % n for n in counts]], rows) + ['', 'One process: per-run time and start-up, seconds (the first run of a worker apart: it holds a script\'s compile):', '']
    rows = [[arm, number(by[arm, 1].get('per_run_seconds_mean')), number(by[arm, 1].get('startup_seconds_mean')), number(by[arm, 1].get('wall_seconds_mean'))]
            for arm in arms if (arm, 1) in by]
    out += table(['arm', 'per run', 'start-up', 'sweep wall'], rows)
    server, silent = by.get(('xppautx_server', 1)), by.get(('xppautx_silent', 1))
    if server and silent and server.get('per_run_seconds_mean') and silent.get('per_run_seconds_mean'):
        out += ['', 'Per-run overhead of a process a run over a persistent --server (one process): %s s a run.' % number(silent['per_run_seconds_mean'] - server['per_run_seconds_mean'])]
    return out + ['']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('results', type=Path)
    p.add_argument('--out', type=Path)
    a = p.parse_args()
    lines = ['# The server study', '']
    for unit, attempt in done_attempts(a.results, 'server').items():
        lines += unit_section(unit, attempt)
    text = '\n'.join(lines) + '\n'
    if a.out:
        a.out.write_text(text, encoding='utf-8')
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
