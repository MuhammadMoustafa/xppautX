#!/usr/bin/env -S uv run --locked python
"""W269 (#326): the Markdown tables of the compiled-build reruns, read from the saved
results (no runs): the five-model screen and the sandbox model, ModelDB 267710's
integration, the size scaling (WSL and Windows) and the AUTO runs.

    uv run --locked python w269_tables.py [screen|mdbint|scaling|auto|mdbauto|compile ...]
"""
import json, statistics, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DATE = '2026-10-11'
ENGINES = ('legacy', 'modern', 'interp')
LABEL = {'legacy': 'XPPAUT 8.0', 'modern': 'xppautX compiled', 'interp': 'xppautX --no-compile'}


def agreement(comparisons, model, left, right):
    """the measured repetitions' agreement of one pair: exact, within the screen's tolerance, or the largest difference"""
    cs = [c for c in comparisons if c['model'] == model and c['repetition'] > 0
          and tuple(c.get('pair', ('legacy', 'modern'))) == (left, right)]
    if not cs or not all(c.get('both_runs_valid') for c in cs):
        return 'not compared'
    if all(c['exact_values'] for c in cs):
        return 'exact'
    worst = max(c['max_abs_error'] for c in cs)
    if all(c['within_screen_tolerance'] for c in cs):
        return f'within tolerance (max {worst:.3g})'
    return f'differs (max {worst:.3g}, {max(c["outside_tolerance"] for c in cs)} outside)'


def integration_table(folder):
    summary = json.loads((folder / 'summary.json').read_text())
    comparisons = json.loads((folder / 'comparison.json').read_text())['comparisons']
    by = {(s['model'], s['engine']): s for s in summary}
    models = list(dict.fromkeys(s['model'] for s in summary))
    rows = ['| Model | XPPAUT 8.0 | xppautX compiled | xppautX --no-compile | XPPAUT / compiled | --no-compile / compiled |'
            ' compiled vs XPPAUT | compiled vs --no-compile |', '|---|---:|---:|---:|---:|---:|---|---|']
    for m in models:
        cell = {e: by.get((m, e)) for e in ENGINES}
        fmt = {e: (f"{c['wall_seconds_mean']:.3f} +/- {c['wall_seconds_sample_sd']:.3f}" if c and c['wall_seconds_mean'] else 'n/a')
               for e, c in cell.items()}
        ratio = lambda a, b: (f"{cell[a]['wall_seconds_mean'] / cell[b]['wall_seconds_mean']:.2f}x"
                              if cell[a] and cell[b] and cell[a]['wall_seconds_mean'] and cell[b]['wall_seconds_mean'] else 'n/a')
        rows.append(f"| {m} | {fmt['legacy']} | {fmt['modern']} | {fmt['interp']} | {ratio('legacy', 'modern')} | "
                    f"{ratio('interp', 'modern')} | {agreement(comparisons, m, 'legacy', 'modern')} | "
                    f"{agreement(comparisons, m, 'modern', 'interp')} |")
    determinism = all(s['repeated_output_identical'] for s in summary)
    valid = sum(s['successful'] for s in summary), sum(s['attempted'] for s in summary)
    return '\n'.join(rows) + f"\n\nWall seconds, mean +/- sample SD of five measured runs. Valid runs {valid[0]}/{valid[1]}; " \
        f"every engine repeats its own output in all five runs: {determinism}."


def weighted(values):
    """inverse-variance mean and its standard error of (value, se) pairs"""
    w = [1 / se ** 2 for _, se in values]
    mean = sum(v * x for (v, _), x in zip(values, w)) / sum(w)
    return mean, (1 / sum(w)) ** .5


def scaling_table(platform):
    rows = json.loads((HERE / 'size-scaling' / 'results' / f'w269-{platform}-{DATE}' / 'summary.json').read_text())
    out = ['| Family | Size | States | XPPAUT / compiled | --no-compile / compiled | XPPAUT / --no-compile |'
           ' one-step run ms (XPPAUT, compiled, --no-compile) | outputs |', '|---|---:|---:|---:|---:|---:|---|---|']
    for r in rows:
        f = lambda s: (f"{r['integration_ratio' + s]:.2f} +/- {r['integration_ratio_se' + s]:.2f}"
                       if 'integration_ratio' + s in r else 'n/a')
        ms = ', '.join(f"{r[e + '_fixed_s'] * 1000:.0f}" if e + '_fixed_s' in r else '-' for e in ENGINES)
        outs = r.get('outputs_2T', '') or ''
        out.append(f"| {r['family']} | {r['size']} | {r['states']} | {f('')} | {f('_interp_modern')} | "
                   f"{f('_legacy_interp')} | {ms} | {outs or r.get('outputs_2T_interp_modern', '')} |")
    out += ['', '| Family | XPPAUT / compiled | --no-compile / compiled | XPPAUT / --no-compile |', '|---|---:|---:|---:|']
    for fam in dict.fromkeys(r['family'] for r in rows):
        # the sizes XPPAUT runs, then (xppautX alone) those past its MAXODE
        for label, part in ((fam, [r for r in rows if r['family'] == fam and 'integration_ratio' in r]),
                            (fam + ' past MAXODE', [r for r in rows if r['family'] == fam and 'integration_ratio' not in r])):
            if not part:
                continue
            cells = []
            for s in ('', '_interp_modern', '_legacy_interp'):
                vals = [(r['integration_ratio' + s], r['integration_ratio_se' + s]) for r in part if 'integration_ratio' + s in r]
                cells.append('%.3f +/- %.3f' % weighted(vals) if vals else 'n/a')
            sizes = f"{min(r['states'] for r in part)}-{max(r['states'] for r in part)} states"
            out.append(f"| {label} ({sizes}) | {' | '.join(cells)} |")
    return '\n'.join(out)


def auto_table(folder):
    s = json.loads((folder / 'summary.json').read_text())
    base = s.get('xppaut', {}).get('periodic_wall_mean')
    out = ['| Engine | Periodic run, wall s | CPU s | points | XPPAUT / engine (wall) | labelled points (periodic) |',
           '|---|---:|---:|---:|---:|---|']
    for e, v in s.items():
        labels = ', '.join(f'{t} {x:.6g}' for _, t, x in v['periodic_labels'])
        speed = f"{base / v['periodic_wall_mean']:.1f}x" if base else 'n/a'
        out.append(f"| {e} | {v['periodic_wall_mean']:.2f} +/- {v['periodic_wall_sd']:.2f} | "
                   f"{v['periodic_cpu_mean']:.2f} +/- {v['periodic_cpu_sd']:.2f} | {v['periodic_points']} | {speed} | {labels} |")
    return '\n'.join(out)


def compile_table(path):
    out = ['| Model | Programs compiled | Not compiled | Exact Jacobian |', '|---|---:|---|---|']
    for r in json.loads(Path(path).read_text()):
        jac = (f"{r['exact_jacobian']['derivatives']} derivatives" if r['exact_jacobian']
               else '; '.join(str(x) for x in r['exact_jacobian_refused']))
        out.append(f"| {r['model']} | {r['compiled']} of {r['programs']} | {len(r['not_compiled']) or 'none'} | {jac} |")
    return '\n'.join(out)


def main():
    parts = sys.argv[1:] or ['screen', 'mdbint', 'scaling', 'auto', 'mdbauto', 'compile']
    for p in parts:
        print(f'## {p}\n')
        if p == 'screen':
            print(integration_table(HERE / 'five-model-screen' / f'results-w269-{DATE}'))
        elif p == 'mdbint':
            print(integration_table(HERE / 'modeldb-267710' / 'results' / f'integrate-wsl-{DATE}'))
        elif p == 'scaling':
            for platform in ('wsl', 'windows'):
                if (HERE / 'size-scaling' / 'results' / f'w269-{platform}-{DATE}' / 'summary.json').exists():
                    print(f'### {platform}\n\n' + scaling_table(platform) + '\n')
        elif p == 'auto':
            print(auto_table(HERE / 'auto' / 'results' / f'w269-wsl-{DATE}'))
        elif p == 'mdbauto':
            print(auto_table(HERE / 'modeldb-267710' / 'results' / f'auto-wsl-{DATE}'))
        elif p == 'compile':
            for f in (HERE / 'five-model-screen' / f'results-w269-{DATE}' / 'compile-counts.json',
                      HERE / 'modeldb-267710' / 'results' / 'compile-counts.json'):
                if f.exists():
                    print(compile_table(f) + '\n')
        print()


if __name__ == '__main__':
    main()
