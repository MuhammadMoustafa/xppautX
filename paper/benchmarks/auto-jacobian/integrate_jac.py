#!/usr/bin/env -S uv run --locked python
"""W252 (#308): the stiff integration of ../rhs-ceiling (PY_S1Bf at Iapp = 2000 pA, the
model's own settings: Stiff, tol 1e-8, 2000 ms, 200001 rows) with the compiled pure
right-hand side (XPP_HAND_RHS=4), its Jacobian by finite differences (stiff.cpp's
jacobn, n + 2 calls) or exact (XPP_JAC=1). One excluded warm-up and REPEATS runs each,
alternating, pinned to one CPU; then the two trajectories compared column by column.

    uv run --locked python integrate_jac.py --proto XPPAUTX_PROTO --name NAME
"""
import argparse, json, os, statistics, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'rhs-ceiling'))
import ceiling  # noqa: E402

REPEATS = 5
COLUMNS = ['t', 'v', 'm', 'h', 'n', 'pM', 'aA', 'bA', 'pNa']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--proto', required=True)
    ap.add_argument('--name', required=True)
    a = ap.parse_args()
    out = HERE / 'results' / a.name
    out.mkdir(parents=True, exist_ok=False)
    runs, rows = [], {}
    with tempfile.TemporaryDirectory(prefix='w252-') as tmp:
        model = ceiling.spike_model(tmp)
        for rep in range(REPEATS + 1):
            for jac in (('0', '1') if rep % 2 == 0 else ('1', '0')):
                os.environ['XPP_JAC'] = jac
                r = ceiling.integrate(os.path.abspath(a.proto), '4', model)
                runs.append({'jac': jac, 'rep': rep, 'warmup': rep == 0, **r})
                print(f"rep {rep} jac={jac}: {r['cpu']:.2f} s CPU, md5 {r['md5'][:12]}", flush=True)
                rows[jac] = [list(map(float, line.split())) for line in (model.parent / 'output.dat').open()]
    summary = {}
    for jac in ('0', '1'):
        x = [r['cpu'] for r in runs if r['jac'] == jac and not r['warmup']]
        summary['exact' if jac == '1' else 'finite differences'] = {
            'cpu_mean': statistics.mean(x), 'cpu_se': statistics.stdev(x) / len(x) ** .5,
            'md5': sorted({r['md5'] for r in runs if r['jac'] == jac})}
    fd, ex = rows['0'], rows['1']
    summary['max_abs_diff'] = {c: max(abs(p[k] - q[k]) for p, q in zip(fd, ex)) for k, c in enumerate(COLUMNS)}
    spikes = lambda r: sum(1 for i in range(1, len(r)) if r[i][1] > 0 >= r[i - 1][1])
    summary['spikes'] = {'finite differences': spikes(fd), 'exact': spikes(ex)}
    (out / 'runs.json').write_text(json.dumps(runs, indent=1))
    (out / 'summary.json').write_text(json.dumps(summary, indent=1))
    print(json.dumps(summary, indent=1))


if __name__ == '__main__':
    main()
