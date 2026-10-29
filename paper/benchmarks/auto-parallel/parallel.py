#!/usr/bin/env -S uv run --locked python
"""W251 (#307): the parallel collocation prototype on W246's AUTO run
(../auto/hopf_xppautx.py, PY_S1Bf_bench.odex: steady state, then the periodic branch
from the first HB). Each variant is a binary, XPP_HAND_RHS, XPP_FA_FIX and
XPP_AUTO_THREADS, pinned to one CPU per thread on distinct physical cores. One
excluded warm-up and REPEATS measured runs per variant, order rotating. Every
variant's whole diagram (its digest) must equal the interpreter's.

    uv run --locked python parallel.py --stock XPPAUTX --proto XPPAUTX_PROTO --name NAME
"""
import argparse, json, os, statistics, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'auto'))
import hopf_xppautx  # noqa: E402

REPEATS = 5
CPUS = [2, 4, 6, 8, 10, 12, 14, 16]  # one logical CPU per physical core (siblings are 2-3, 4-5, ...)

# name: (binary, XPP_HAND_RHS, XPP_FA_FIX, threads)
VARIANTS = {
    'interpreter': ('stock', '0', '0', 1),
    'interpreter-fa': ('proto', '0', '1', 1),
    'compiled': ('proto', '1', '0', 1),
    'pure': ('proto', '4', '0', 1),
    'pure-fa': ('proto', '4', '1', 1),
    'pure-fa-2t': ('proto', '4', '1', 2),
    'pure-fa-4t': ('proto', '4', '1', 4),
    'pure-fa-8t': ('proto', '4', '1', 8),
    # W250's TinyCC instead of gcc (a prototype built with apply_parallel.py --tcc)
    'tcc-pure-fa': ('proto', '5', '1', 1),
    'tcc-pure-fa-2t': ('proto', '5', '1', 2),
    'tcc-pure-fa-4t': ('proto', '5', '1', 4),
    # W252's exact Jacobian (a prototype built with ../auto-jacobian/apply_jacobian.py):
    # changes the iterates, so its diagram is compared, not required equal
    'jac': ('proto', '4', '1', 1, '1'),
    'jac-2t': ('proto', '4', '1', 2, '1'),
    'jac-4t': ('proto', '4', '1', 4, '1'),
}


def wrapper(folder, name, binary, mode, fa, threads, jac='0'):
    w = Path(folder) / f'run-{name}'
    cpus = ','.join(map(str, CPUS[:threads]))
    w.write_text(f'#!/bin/sh\nXPP_HAND_RHS={mode} XPP_FA_FIX={fa} XPP_AUTO_THREADS={threads} XPP_JAC={jac} '
                 f'exec taskset -c {cpus} {binary} "$@"\n')
    w.chmod(0o755)
    return str(w)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--stock', required=True)
    ap.add_argument('--proto', required=True)
    ap.add_argument('--name', required=True)
    ap.add_argument('--repeats', type=int, default=REPEATS)
    ap.add_argument('--variants', default=','.join(VARIANTS))
    a = ap.parse_args()
    names = a.variants.split(',')
    assert names[0] == 'interpreter' and set(names) <= set(VARIANTS)
    out = HERE / 'results' / a.name
    out.mkdir(parents=True, exist_ok=False)
    bins = {'stock': os.path.abspath(a.stock), 'proto': os.path.abspath(a.proto)}
    runs, log = [], open(out / 'run.log', 'w')
    with tempfile.TemporaryDirectory(prefix='w251-') as tmp:
        wrappers = {n: wrapper(tmp, n, bins[VARIANTS[n][0]], *VARIANTS[n][1:]) for n in names}
        for rep in range(a.repeats + 1):
            for v in names[rep % len(names):] + names[:rep % len(names)]:
                r = hopf_xppautx.measure(wrappers[v])
                runs.append({'variant': v, 'rep': rep, 'warmup': rep == 0, **r})
                p = r['periodic']
                msg = (f"rep {rep} {v}: periodic {p['wall']:.2f} s wall, {p['cpu']:.2f} s CPU, "
                       f"{p['labels'][-1]}, digest {p['digest'][:12]} (steady {r['steady']['digest'][:12]})")
                print(msg, flush=True)
                log.write(msg + '\n')
                (out / 'runs.json').write_text(json.dumps(runs, indent=1))
    summary = {}
    for v in names:
        rs = [r for r in runs if r['variant'] == v and not r['warmup']]
        s = summary[v] = {'threads': VARIANTS[v][3]}
        for k in ('wall', 'cpu'):
            x = [r['periodic'][k] for r in rs]
            s[k + '_mean'], s[k + '_se'] = statistics.mean(x), statistics.stdev(x) / len(x) ** .5
        s['digests'] = sorted({(r['steady']['digest'], r['periodic']['digest']) for r in rs})
    (out / 'summary.json').write_text(json.dumps(summary, indent=1))
    base = summary['interpreter']
    for v, s in summary.items():
        same = s['digests'] == base['digests']
        print(f"{v:15s} periodic {s['wall_mean']:6.2f} +/- {s['wall_se']:.2f} s wall (x{base['wall_mean'] / s['wall_mean']:.2f}), "
              f"{s['cpu_mean']:6.2f} s CPU; diagram {'identical' if same else 'DIFFERS'} to the interpreter's")


if __name__ == '__main__':
    main()
