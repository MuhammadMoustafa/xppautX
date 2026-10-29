#!/usr/bin/env -S uv run --locked python
"""W246 (#302): time PY_S1Bf_bench's steady-state and periodic AUTO runs in
XPPAUT 8.0 (hopf_xppaut.py), xppautX (hopf_xppautx.py) and AUTO-07p (auto07p/),
on WSL. One excluded warm-up, then REPEATS measured fresh processes per engine,
engine order rotating per repetition; single-threaded engines pinned to CPU, AUTO-07p's
threads to CPU..CPU+n-1. Writes results/<name>/runs.json and summary.json.

    uv run --locked python bench.py --xppaut XPPAUT --xppautx XPPAUTX --auto07p-src DIR --name NAME [--repeats 5]

DIR is a copy of auto07p/ on WSL's own disk; AUTO_DIR must name the AUTO-07p build.
"""
import argparse, functools, json, os, re, shutil, statistics, subprocess, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
CPU = 2
THREADS = (1, 2, 4)
REPEATS = 5


def pinned(binary, cpus, folder):
    """a wrapper that execs binary on the given CPUs, so its PID (and CPU time) is the program's"""
    w = Path(folder) / f'pinned-{Path(binary).name}-{cpus}'
    w.write_text(f'#!/bin/sh\nexec taskset -c {cpus} {os.path.abspath(binary)} "$@"\n')
    w.chmod(0o755)
    return str(w)


def auto07p(src, threads, jac=0, name='py_s1bf'):
    """AUTO-07p's two runs from src (NAME.c, c.NAME.eq/.po, NAME.auto; W269: name, for
    ../modeldb-267710/auto07p): {run: {wall, cpu, labels}}"""
    work = Path(tempfile.mkdtemp(prefix='w246-a07-'))
    try:
        for f in src.iterdir():
            shutil.copy(f, work)
        if jac:  # W269: AUTO's JAC=1, py_s1bf.c's hand-written dfdu/dfdp (its FD path, ijac 0, is W246's)
            for c in (f'c.{name}.eq', f'c.{name}.po'):
                text = (work / c).read_text()
                assert text.count('JAC = 0') == 1, c
                (work / c).write_text(text.replace('JAC = 0', 'JAC = 1'))
        # auto_f2c.h's K&R declarations need gnu17 under gcc 15 (README); AUTO's make then keeps this object
        subprocess.run(['gcc', '-std=gnu17', '-g', '-O2', '-I', os.path.join(os.environ['AUTO_DIR'], 'include'),
                        '-c', f'{name}.c', '-o', f'{name}.o'], cwd=work, check=True)
        env = {**os.environ, 'OMP_NUM_THREADS': str(threads)}
        cpus = f'{CPU}-{CPU + threads - 1}' if threads > 1 else str(CPU)
        out = subprocess.run(['taskset', '-c', cpus, 'auto', f'{name}.auto'], cwd=work, env=env,
                             capture_output=True, text=True, check=True).stdout
    finally:
        shutil.rmtree(work)
    res, run = {}, None
    for line in out.splitlines():
        f = line.split()
        if line.startswith('RUN '):
            run = f[1]
            res[run] = {'labels': []}
        elif line.startswith('TIMING '):
            res[f[1]].update(wall=float(f[2]), cpu=float(f[3]))
        elif run and len(f) > 4 and f[0].lstrip('-').isdigit() and f[2] in ('EP', 'HB', 'BP', 'LP', 'MX', 'PD', 'TR', 'UZ'):
            res[run]['labels'].append((abs(int(f[1])), f[2], float(f[4])))
    return res


def xppautx_run(measure, binary, tmp, threads, jacobian, extra_args=()):
    """W269: measure(binary, ...) of xppautX on threads CPUs from CPU, with a fresh HOME and
    XPP_CONFIG_DIR (the thread count is a preference: never the user's own), AUTO's threads
    and the model's Jacobian set"""
    home = Path(tempfile.mkdtemp(prefix='w269-home-'))
    try:
        cpus = f'{CPU}-{CPU + threads - 1}' if threads > 1 else str(CPU)
        env = {'HOME': str(home), 'XPP_CONFIG_DIR': str(home / 'config'), 'LC_ALL': 'C'}
        (home / 'config').mkdir()
        return measure(pinned(binary, cpus, tmp), threads=threads, jacobian=jacobian,
                       extra_args=extra_args, env=env)
    finally:
        shutil.rmtree(home)


def xppautx_engines(measure, binary, tmp):
    """W269: xppautX's engines: finite-difference and exact Jacobians on THREADS threads, and
    the interpreter (--no-compile, which has no exact Jacobian) on one"""
    engines = {}
    for n in THREADS:
        for jac, label in (('difference', 'fd'), ('exact', 'exact')):
            engines[f'xppautx-{label}-{n}t'] = lambda n=n, jac=jac: xppautx_run(measure, binary, tmp, n, jac)
    engines['xppautx-interp-1t'] = lambda: xppautx_run(measure, binary, tmp, 1, 'difference', ['--no-compile'])
    return engines


def series(engines, names, repeats, out, runs_kept=('steady', 'periodic')):
    """one excluded warm-up and repeats measured runs per engine, the order rotating each
    repetition; every run into out/runs.json as it ends, then the summary (out/summary.json)"""
    unknown = [e for e in names if e not in engines]
    if unknown:
        raise ValueError(f'unknown engines {unknown}; known: {list(engines)}')
    runs = []
    log = open(out / 'run.log', 'w')
    for rep in range(repeats + 1):
        order = names[rep % len(names):] + names[:rep % len(names)]
        for e in order:
            r = engines[e]()
            row = {'engine': e, 'rep': rep, 'warmup': rep == 0, **r}
            runs.append(row)
            msg = f"rep {rep} {e}: periodic {r['periodic']['cpu']:.2f} s CPU, {r['periodic']['wall']:.2f} s wall, " \
                  f"{max(p for p, _, _ in r['periodic']['labels'])} points"
            print(msg, flush=True)
            log.write(msg + '\n')
            (out / 'runs.json').write_text(json.dumps(runs, indent=1))
    summary = {}
    for e in names:
        rs = [r for r in runs if r['engine'] == e and not r['warmup']]
        if not rs:
            continue  # W269: a warm-up-only pilot (--repeats 0) has nothing to summarize
        summary[e] = {}
        for run in runs_kept:
            if run not in rs[0]:
                continue  # W269: AUTO-07p starts from the equilibrium; it integrates nothing first
            for k in ('cpu', 'wall'):
                v = [r[run][k] for r in rs]
                summary[e][f'{run}_{k}_mean'] = statistics.mean(v)
                summary[e][f'{run}_{k}_se'] = statistics.stdev(v) / len(v) ** .5
                summary[e][f'{run}_{k}_sd'] = statistics.stdev(v)  # W269: the sample SD too
            if 'labels' not in rs[0][run]:
                continue  # a run with no AUTO points (ModelDB's integration)
            pts = {max(p for p, _, _ in r[run]['labels']) for r in rs}
            summary[e][f'{run}_points'] = sorted(pts)
            summary[e][f'{run}_labels'] = rs[0][run]['labels']
            # W269: one whole-diagram digest per engine shows each repeats itself (xppautX's runs only)
            summary[e][f'{run}_digests'] = sorted({r[run]['digest'] for r in rs if 'digest' in r[run]})
            summary[e][f'{run}_label_sets'] = len({json.dumps(r[run]['labels']) for r in rs})
    (out / 'summary.json').write_text(json.dumps(summary, indent=1))
    for e, s in summary.items():
        print(f"{e:18s} periodic {s['periodic_cpu_mean']:8.3f} +/- {s['periodic_cpu_sd']:.3f} s CPU, "
              f"{s['periodic_wall_mean']:8.3f} +/- {s['periodic_wall_sd']:.3f} s wall, points {s['periodic_points']}")
    return runs, summary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--xppaut', required=True)
    ap.add_argument('--xppautx', required=True)
    ap.add_argument('--auto07p-src', required=True, type=Path)
    ap.add_argument('--name', required=True)
    ap.add_argument('--repeats', type=int, default=REPEATS)
    ap.add_argument('--engines', help="W269: comma-separated engine names (default W246's); "
                    'xppautx-fd-Nt / xppautx-exact-Nt / xppautx-interp-1t, auto07p-Nt / auto07p-jac-Nt')
    ap.add_argument('--model', default=str(HERE / 'models' / 'PY_S1Bf_bench.ode'),
                    help="xppautX's model (W269: models/w269/, converted by the xppautX it times)")
    a = ap.parse_args()
    out = HERE / 'results' / a.name
    out.mkdir(parents=True, exist_ok=False)
    import hopf_xppaut, hopf_xppautx
    tmp = Path(tempfile.mkdtemp(prefix='w246-pin-'))
    engines = {'xppaut': lambda: hopf_xppaut.measure(pinned(a.xppaut, CPU, tmp)),
               'xppautx': lambda: hopf_xppautx.measure(pinned(a.xppautx, CPU, tmp), a.model),
               **xppautx_engines(functools.partial(hopf_xppautx.measure, model=a.model), a.xppautx, tmp)}
    for n in THREADS:
        engines[f'auto07p-{n}t'] = lambda n=n: auto07p(a.auto07p_src, n)
        engines[f'auto07p-jac-{n}t'] = lambda n=n: auto07p(a.auto07p_src, n, jac=1)
    names = a.engines.split(',') if a.engines else ['xppaut', 'xppautx'] + [f'auto07p-{n}t' for n in THREADS]
    try:
        series(engines, names, a.repeats, out)
    finally:
        shutil.rmtree(tmp)


if __name__ == '__main__':
    main()
