#!/usr/bin/env python3
"""W378 (#440): plots and tables of the large-stiff timed series.

Reads a finished stage-2 results folder written by paper/benchmarks/large-stiff/
study.py (runs.json, drift.json, cache-summary.json, factors/*/result.json) and
writes three PNG plots into docs/studies/w378/. Every number comes from those
files. `--tables` also prints the Markdown tables that docs/studies/w378-large-stiff.md
quotes, so the study's numbers can be regenerated here.

Run: uv run --locked python tools/studies/w378_plots.py [--tables] [--data DIR]
"""
import argparse
import json
import math
import statistics
import sys
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
DEFAULT_DATA = Path.home()/'xppautx-comparison/final/large-stiff/attempt-001/results'
DEFAULT_OUT = REPO/'docs/studies/w378'
FAMILIES = ('cable', 'ring')
FAMILY_TITLES = {'cable': 'Cable (Dirichlet ends, 1 coupling)', 'ring': 'Morris-Lecar ring (half-bandwidth 4)'}
SOLVERS = ('legacy-dense', 'legacy-band', 'klu', 'gmres')
DENSE = 'legacy-dense'
LABELS = {'legacy-dense': 'dense (legacy)', 'legacy-band': 'band (legacy)', 'klu': 'KLU (sparse LU)',
          'gmres': 'GMRES, band-LU preconditioner'}
REPEATS = 5                      # measured fresh processes per row (study.py REPEATS)
KIB_PER_MIB = 1024
DOUBLE_BYTES = 8                 # the dense matrix is double precision
CACHE_LINE_BYTES = 64            # every level of the profiled cache (callgrind's desc: lines)
DPI = 110                        # small PNGs, well under 200 KB each


def fail(message):
    print(f'w378_plots: {message}', file=sys.stderr)
    raise SystemExit(1)


def load(data):
    """Rows keyed by (family, n, solver), each checked to hold its warm-up and five measured runs."""
    runs = json.loads((data/'runs.json').read_text())
    groups = {}
    for r in runs:
        if r['warmup']:
            continue
        groups.setdefault((r['family'], r['n'], r['solver']), []).append(r)
    rows = {}
    for key, rs in groups.items():
        family, n, solver = key
        rs.sort(key=lambda r: r['repetition'])
        if len(rs) != REPEATS:
            fail(f'{key}: {len(rs)} measured runs, expected {REPEATS}')
        if any(r['returncode'] != 0 or not r['accuracy_pass'] or 'wall' not in r for r in rs):
            fail(f'{key}: a measured run failed or has no timing')
        if any(r['stats'] != rs[0]['stats'] for r in rs):
            fail(f'{key}: solver counts differ between repetitions')
        walls = [r['wall'] for r in rs]
        rss = [r['peak_rss_kib'] for r in rs]
        rows[key] = dict(
            family=family, n=n, solver=solver, runs=len(rs), tolerance=rs[0]['tolerance'],
            median=statistics.median(walls), sd=statistics.stdev(walls), lo=min(walls), hi=max(walls),
            rss_mean=statistics.mean(rss)/KIB_PER_MIB, rss_sd=statistics.stdev(rss)/KIB_PER_MIB,
            along=max(r['error']['along'] for r in rs), endpoint=max(r['error']['endpoint'] for r in rs),
            stats=rs[0]['stats'])
    expected = {(f, n, s) for f in FAMILIES for n in sizes_of(rows) for s in SOLVERS}
    missing = expected - set(rows)
    if missing:
        fail(f'missing rows: {sorted(missing)}')
    for key in sorted(rows):
        family, n, solver = key
        factor = json.loads((data/'factors'/f'{family}-{n}-{solver}'/'result.json').read_text())
        if factor.get('returncode') != 0 or factor['factorizations'] < 1:
            fail(f'{key}: factor diagnostic run has no factorizations')
        rows[key]['factorizations'] = factor['factorizations']
        rows[key]['factor_total'] = factor['factor_wall_total']
        rows[key]['factor_mean'] = factor['factor_wall_mean']
    return rows


def sizes_of(rows):
    return sorted({n for (_, n, _) in rows})


def loglog_slope(rows, family, solver, sizes, key):
    xs = [math.log(n) for n in sizes]
    ys = [math.log(rows[(family, n, solver)][key]) for n in sizes]
    return statistics.linear_regression(xs, ys).slope


def plot_time(rows, sizes, out):
    fig, axes = plt.subplots(1, 2, figsize=(9.6, 4.2))
    for ax, family in zip(axes, FAMILIES):
        for solver in SOLVERS:
            med = [rows[(family, n, solver)]['median'] for n in sizes]
            low = [rows[(family, n, solver)]['median']-rows[(family, n, solver)]['lo'] for n in sizes]
            high = [rows[(family, n, solver)]['hi']-rows[(family, n, solver)]['median'] for n in sizes]
            ax.errorbar(sizes, med, yerr=[low, high], marker='o', ms=4, capsize=2, label=LABELS[solver])
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_xticks(sizes, [str(n) for n in sizes])
        ax.minorticks_off()
        ax.set_xlabel('states n')
        ax.set_ylabel('wall time per run (s), median, min-max bars')
        ax.set_title(FAMILY_TITLES[family], fontsize=10)
        ax.grid(True, which='major', alpha=0.3)
    axes[0].legend(fontsize=8, loc='upper left')
    fig.suptitle('Whole-process wall time, equal-accuracy settings (5 runs each)', fontsize=10)
    fig.tight_layout()
    fig.savefig(out/'w378-time.png', dpi=DPI)
    plt.close(fig)


def plot_speedup(rows, sizes, out):
    fig, axes = plt.subplots(1, 2, figsize=(9.6, 4.2))
    for ax, family in zip(axes, FAMILIES):
        dense = {n: rows[(family, n, DENSE)] for n in sizes}
        for solver in SOLVERS[1:]:
            med = [dense[n]['median']/rows[(family, n, solver)]['median'] for n in sizes]
            # bars: the dense median over the solver's fastest and slowest run
            low = [m-dense[n]['median']/rows[(family, n, solver)]['hi'] for m, n in zip(med, sizes)]
            high = [dense[n]['median']/rows[(family, n, solver)]['lo']-m for m, n in zip(med, sizes)]
            ax.errorbar(sizes, med, yerr=[low, high], marker='o', ms=4, capsize=2, label=LABELS[solver])
        ax.axhline(1.0, color='0.4', lw=0.8, ls='--')
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_xticks(sizes, [str(n) for n in sizes])
        ax.minorticks_off()
        ax.set_xlabel('states n')
        ax.set_ylabel('speed-up over dense (x)')
        ax.set_title(FAMILY_TITLES[family], fontsize=10)
        ax.grid(True, which='major', alpha=0.3)
    axes[0].legend(fontsize=8, loc='upper left')
    fig.suptitle('Speed-up over the legacy dense solver (above 1: faster than dense)', fontsize=10)
    fig.tight_layout()
    fig.savefig(out/'w378-speedup.png', dpi=DPI)
    plt.close(fig)


def plot_memory(rows, sizes, out):
    fig, axes = plt.subplots(1, 2, figsize=(9.6, 4.2))
    for ax, family in zip(axes, FAMILIES):
        for solver in SOLVERS:
            mem = [rows[(family, n, solver)]['rss_mean'] for n in sizes]
            sd = [rows[(family, n, solver)]['rss_sd'] for n in sizes]
            ax.errorbar(sizes, mem, yerr=sd, marker='o', ms=4, capsize=2, label=LABELS[solver])
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_xticks(sizes, [str(n) for n in sizes])
        ax.minorticks_off()
        ax.set_xlabel('states n')
        ax.set_ylabel('peak resident memory (MiB), mean and SD')
        ax.set_title(FAMILY_TITLES[family], fontsize=10)
        ax.grid(True, which='major', alpha=0.3)
    axes[0].legend(fontsize=8, loc='upper left')
    fig.suptitle('Peak resident set size of the whole worker process (5 runs each)', fontsize=10)
    fig.tight_layout()
    fig.savefig(out/'w378-memory.png', dpi=DPI)
    plt.close(fig)


def md(headers, body):
    lines = ['| ' + ' | '.join(headers) + ' |', '|' + '|'.join('---' for _ in headers) + '|']
    lines += ['| ' + ' | '.join(row) + ' |' for row in body]
    return '\n'.join(lines)


def g(x, digits=4):
    return f'{x:.{digits}g}'


def print_tables(rows, sizes, data):
    drift = json.loads((data/'drift.json').read_text())
    cache = json.loads((data/'cache-summary.json').read_text())
    print('### Timing (median of 5, sample SD, min-max, speed-up over dense)\n')
    body = []
    for family in FAMILIES:
        for n in sizes:
            dense = rows[(family, n, DENSE)]
            for solver in SOLVERS:
                r = rows[(family, n, solver)]
                body.append([family, str(n), LABELS[solver], f"{r['runs']} + 1 warm-up", g(r['median']),
                             g(r['sd']), f"{g(r['lo'])}-{g(r['hi'])}", f"{dense['median']/r['median']:.3g}x"])
    print(md(['family', 'n', 'method', 'runs', 'median s', 'SD s', 'min-max s', 'speed-up vs dense'], body) + '\n')

    print('### Accuracy and counts (worst of 5 runs; counts are identical across repetitions)\n')
    body = []
    for family in FAMILIES:
        for n in sizes:
            for solver in SOLVERS:
                r = rows[(family, n, solver)]
                s = r['stats']
                body.append([family, str(n), LABELS[solver], g(r['tolerance'], 2), g(r['along'], 3), g(r['endpoint'], 3),
                             str(s['steps']), str(s['rhs']), str(s['newton']), str(s['jacobians']), str(s['setups']),
                             str(s.get('linear_iterations') if s.get('linear_iterations') is not None else '-'),
                             str(s.get('preconditioner_solves') if s.get('preconditioner_solves') is not None else '-'),
                             str(r['factorizations'])])
    print(md(['family', 'n', 'method', 'rtol=atol', 'error along', 'error endpoint', 'steps', 'rhs calls', 'Newton',
              'Jacobians', 'setups', 'Krylov iterations', 'preconditioner solves', 'factorizations (own run)'], body) + '\n')

    print('### Peak memory (MiB, mean and SD of 5 runs)\n')
    body = []
    for family in FAMILIES:
        for n in sizes:
            dense = rows[(family, n, DENSE)]
            for solver in SOLVERS:
                r = rows[(family, n, solver)]
                body.append([family, str(n), LABELS[solver], f"{r['rss_mean']:.1f}", f"{r['rss_sd']:.2f}",
                             f"{r['rss_mean']/dense['rss_mean']:.3g}x"])
    print(md(['family', 'n', 'method', 'mean MiB', 'SD MiB', 'ratio to dense'], body) + '\n')

    print('### Factorization diagnostics (separate instrumented runs)\n')
    body = []
    for family in FAMILIES:
        for n in sizes:
            for solver in SOLVERS:
                r = rows[(family, n, solver)]
                share = r['factor_total']/r['median']
                body.append([family, str(n), LABELS[solver], str(r['factorizations']), f"{r['factor_mean']*1e6:.1f}",
                             f"{r['factor_total']*1e3:.2f}", f"{100*share:.1f}%"])
    print(md(['family', 'n', 'method', 'factorizations', 'mean factor us', 'total factor ms',
              'total factor / median run wall'], body) + '\n')

    print('### Log-log slope of median wall time over n\n')
    body = []
    for family in FAMILIES:
        for solver in SOLVERS:
            body.append([family, LABELS[solver], f"{loglog_slope(rows, family, solver, sizes, 'median'):.2f}",
                         f"{loglog_slope(rows, family, solver, sizes, 'rss_mean'):.2f}"])
    print(md(['family', 'method', 'time exponent', 'memory exponent'], body) + '\n')

    print('### Dense LU cache simulation (n=largest, 3 DenseFactor calls per family)\n')
    body = []
    for c in cache:
        per = c['misses_per_factorization']
        tot = c['totals']
        matrix_lines = c['n']*c['n']*DOUBLE_BYTES/CACHE_LINE_BYTES
        body.append([c['family'], str(c['n']), str(c['factorizations']), f"{tot['Ir']/c['factorizations']:.4g}",
                     f"{per['D1mr']:.4g}", f"{per['D1mw']:.4g}", f"{per['DLmr']:.4g}", f"{per['DLmw']:.4g}",
                     f"{matrix_lines:.4g}"])
    print(md(['family', 'n', 'factorizations', 'instructions per factorization', 'D1 read misses',
              'D1 write misses', 'LL read misses', 'LL write misses', 'n*n*8/64 matrix lines'], body) + '\n')

    print('### Calibration drift (dense, first case, one run at start and end)\n')
    start, end = drift['start']['wall'], drift['end']['wall']
    print(f'start {g(start)} s, end {g(end)} s, change {100*(end/start-1):+.1f}%\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--data', type=Path, default=DEFAULT_DATA, help='the stage-2 results folder')
    ap.add_argument('--out', type=Path, default=DEFAULT_OUT, help='where the PNG plots go')
    ap.add_argument('--tables', action='store_true', help='print the Markdown tables the study quotes')
    a = ap.parse_args()
    rows = load(a.data)
    sizes = sizes_of(rows)
    a.out.mkdir(parents=True, exist_ok=True)
    plot_time(rows, sizes, a.out)
    plot_speedup(rows, sizes, a.out)
    plot_memory(rows, sizes, a.out)
    for name in ('w378-time.png', 'w378-speedup.png', 'w378-memory.png'):
        print(f'wrote {a.out/name} ({(a.out/name).stat().st_size} bytes)')
    if a.tables:
        print_tables(rows, sizes, a.data)


if __name__ == '__main__':
    main()
