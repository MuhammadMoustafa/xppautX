#!/usr/bin/env -S uv run --locked python
"""W248 (#304): concurrent AUTO runs, XPPAUT 8.0 against xppautX.

N copies of one AUTO job (the benchmark's steady-state run, then the periodic branch from
the first HB: ../hopf_xppaut.py, ../hopf_xppautx.py) or N jobs with different parameters
(ParMin 200, 400, 800, 1600 pA) are started at once under one user and one HOME, then the same
N one after another. Each run's diagram is compared with the same run alone.

    uv run --locked python concurrent.py --program xppaut|xppautx --binary BIN --platform NAME --out DIR
        [--reps 5] [--ns 2,4] [--limit 300]

A run is a child process of this script (--child), so each XPPAUT has its own Xvfb display
and each xppautX its own server; the parent only starts, waits and compares.
"""
import argparse, gzip, hashlib, json, os, platform, shutil, signal, statistics, subprocess, sys, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
AUTO = HERE.parent
sys.path.insert(0, str(AUTO))
PARMINS = (200, 400, 800, 1600)  # pA: the four "different" jobs; the first is the benchmark's own
NPR = 10  # printed/stored every 10th point, so XPPAUT's table (its only text output) holds a diagram, not three lines
THREADS = 1  # xppautX's AUTO threads per process: N processes use N cores
GRAB_TABS = 8  # XPPAUT's grab: EP (label 1), then a label every NPR points up to point 70, then the HB (point 78)
HB_PAR, HB_TOL = 16409.42, 20  # pA: the periodic run starts at the first HB (its first labelled point, the BP, is 15 pA below), whatever else happened
POLL = 0.1  # s: poll interval of the waits for a condition
BASE = AUTO / 'models' / 'PY_S1Bf_bench'


def variant_files(folder, binary, parmins=PARMINS):
    """the benchmark model with ParMin (and NPR) changed, as .ode (XPPAUT) and, converted once by
    xppautX itself (--convert, outside any timing), .odex"""
    folder.mkdir(parents=True, exist_ok=True)
    text = BASE.with_suffix('.ode').read_text()
    assert 'parmin=200' in text and 'npr=5000' in text
    for p in parmins:
        ode = folder / f'PYbench_pm{p}.ode'
        ode.write_text(text.replace('parmin=200', f'parmin={p}').replace('npr=5000', f'npr={NPR}'))
        if binary:
            subprocess.run([binary, '--convert', '--yes', ode.name], cwd=folder, check=True, capture_output=True)


# ---------------------------------------------------------------- the child: one run

def child(a):
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))
    out = {}
    try:
        if a.program == 'xppaut':
            os.environ['W248_DISPLAY'] = a.display
            import hopf_xppaut
            hopf_xppaut.measure(a.binary, model=a.model, home=a.home, limit=a.limit, ready=a.ready, go=a.go, out=out, tabs=GRAB_TABS)
        else:
            import hopf_xppautx
            env = {'HOME': a.home, 'XPP_CONFIG_DIR': a.config_dir}
            hopf_xppautx.measure(a.binary, model=a.model, threads=THREADS, env=env, run_dir=a.run_dir, limit=a.limit,
                                 ready=a.ready, go=a.go, out=out)
        out['ok'] = True
    except BaseException as e:  # a failed run is a result
        out['ok'] = False
        out.setdefault('_error', repr(e))
    Path(a.out).write_text(json.dumps(out))


# ---------------------------------------------------------------- the parent

def spawn(a, group, i, model, run_dir, home):
    base = group / f'run{i}'
    cmd = [sys.executable, str(Path(__file__).resolve()), '--child', '--program', a.program, '--binary', a.binary,
           '--model', str(model), '--home', str(home), '--run-dir', str(run_dir), '--display', f':{77 + i}',
           '--config-dir', str(group / f'cfg{i}'), '--ready', str(base) + '.ready', '--go', str(group / 'go'),
           '--out', str(base) + '.json', '--limit', str(a.limit)]
    (group / f'cfg{i}').mkdir()
    return subprocess.Popen(cmd, stdout=open(str(base) + '.out', 'w'), stderr=subprocess.STDOUT)


def wait_all(procs, timeout):
    end = time.monotonic() + timeout
    for p in procs:
        try:
            p.wait(max(1, end - time.monotonic()))
        except subprocess.TimeoutExpired:
            p.terminate()
            try:
                p.wait(20)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()


def snapshot(home):
    """name -> (size, sha256) of every file under home"""
    return {str(f.relative_to(home)): (f.stat().st_size, hashlib.sha256(f.read_bytes()).hexdigest())
            for f in sorted(Path(home).rglob('*')) if f.is_file()}


def group_run(a, models, parmins, mode, tag):
    """N runs (parmins gives each its ParMin) started at once ('concurrent') or one after another
    ('sequential') under one HOME. Returns {runs: [per-run dict], elapsed, files}"""
    group = Path(tempfile.mkdtemp(prefix=f'w248-{tag}-'))
    home = group / 'home'
    home.mkdir()
    run_dir = group / 'models'  # the folder every run of the group loads its model from
    run_dir.mkdir()
    ext = '.ode' if a.program == 'xppaut' else '.odex'
    for p in set(parmins):
        shutil.copy(models / f'PYbench_pm{p}{ext}', run_dir)
    model = lambda p: run_dir / f'PYbench_pm{p}{ext}'  # noqa: E731
    results = []
    t0 = time.time()
    if mode == 'concurrent':
        procs = [spawn(a, group, i, model(p), run_dir, home) for i, p in enumerate(parmins)]
        end = time.monotonic() + 120
        while not all((group / f'run{i}.ready').exists() or procs[i].poll() is not None for i in range(len(procs))):
            if time.monotonic() > end:
                break
            time.sleep(POLL)
        (group / 'go').write_text('go')
        wait_all(procs, 2 * a.limit + 120)
    else:
        (group / 'go').write_text('go')  # no barrier for a run alone
        for i, p in enumerate(parmins):
            wait_all([spawn(a, group, i, model(p), run_dir, home)], 2 * a.limit + 120)
    elapsed = time.time() - t0
    for i, p in enumerate(parmins):
        f = group / f'run{i}.json'
        r = json.loads(f.read_text()) if f.exists() else {'ok': False, '_error': 'no result (child killed)'}
        r['parmin'] = p
        results.append(r)
    out = {'runs': results, 'elapsed': elapsed, 'files': snapshot(home), 'run_dir_files': sorted(f.name for f in run_dir.iterdir()),
           'config_dirs': {f'cfg{i}': sorted(f.name for f in (group / f'cfg{i}').iterdir()) for i in range(len(parmins))}
           if a.program == 'xppautx' else None}
    shutil.rmtree(group, ignore_errors=True)
    return out


def diagram(r, run):
    """the comparable content of one finished run: XPPAUT's table rows, xppautX's points"""
    return r[run]['rows'] if 'rows' in r.get(run, {}) else r[run]['points']


def floats(row):
    toks = row.split() if isinstance(row, str) else row
    out = []
    for t in toks:
        try:
            out.append(float(t))
        except (TypeError, ValueError):
            pass
    return out


def compare(ref, r):
    """{run: {equal, n, n_ref, differing, first, max_abs}} of r's diagrams against the alone reference ref"""
    out = {}
    for run in ('steady', 'periodic'):
        if run not in r:
            out[run] = {'equal': False, 'missing': True}
            continue
        d, dr = diagram(r, run), diagram(ref, run)
        bad = [i for i in range(min(len(d), len(dr))) if d[i] != dr[i]]
        mx = 0.0
        for i in bad:
            fa, fb = floats(d[i]), floats(dr[i])
            mx = max([mx] + [abs(x - y) for x, y in zip(fa, fb)] + ([float('inf')] if len(fa) != len(fb) else []))
        out[run] = {'equal': not bad and len(d) == len(dr), 'n': len(d), 'n_ref': len(dr), 'differing': len(bad),
                    'first': bad[0] if bad else None, 'max_abs': mx if bad else 0.0}
    return out


def job_wall(r):
    return r['periodic']['t1'] - r['steady']['t0'] if r.get('ok') else None


def start_par(r):
    """the parameter value of the periodic run's first point (XPPAUT: its first labelled one)"""
    d = diagram(r, 'periodic')[0]
    return float(d.split()[4]) if isinstance(d, str) else d[2]


def judge(a, refs, files_ref, skip_files, g, out_dir, tag):
    """add per-run comparisons to a group result; keep the diagrams of a run that differs"""
    for r in g['runs']:
        if not r.get('ok'):
            r['verdict'] = 'failed'
            r['job_wall'] = None
            continue
        r['job_wall'] = job_wall(r)
        r['periodic_start'] = start_par(r) if r['periodic'].get('rows') or r['periodic'].get('points') else None
        r['compare'] = compare(refs[r['parmin']], r)
        r['verdict'] = 'equal' if all(c['equal'] for c in r['compare'].values()) else 'different'
        if r['verdict'] == 'different':
            with gzip.open(out_dir / f'diff-{tag}-pm{r["parmin"]}.json.gz', 'wt') as f:
                json.dump({k: r[k] for k in ('steady', 'periodic')}, f)
    ok = [r for r in g['runs'] if r.get('ok')]
    g['n'] = len(g['runs'])
    g['n_ok'] = len(ok)
    g['n_equal'] = sum(r['verdict'] == 'equal' for r in g['runs'])
    g['job_walls'] = [r['job_wall'] for r in ok]
    if g['mode'] == 'concurrent':
        g['wall'] = max(r['periodic']['t1'] for r in ok) - min(r['steady']['t0'] for r in ok) if ok else None
    else:
        g['wall'] = sum(r['job_wall'] for r in ok) if len(ok) == g['n'] else None
    if files_ref is not None:  # XPPAUT: the files it left in the shared HOME against the same runs alone
        want = {k: v for p in {r['parmin'] for r in g['runs']} for k, v in files_ref[p].items()
                if k not in skip_files and (k.startswith('PYbench') or len(set(g['parmins'])) == 1)}
        g['files_vs_alone'] = {'same': sorted(k for k in want if g['files'].get(k) == want[k]),
                               'different': sorted(k for k in want if k in g['files'] and g['files'][k] != want[k]),
                               'missing': sorted(k for k in want if k not in g['files']),
                               'extra': sorted(k for k in g['files'] if k not in want and k not in skip_files)}
    for r in g['runs']:
        for run in ('steady', 'periodic'):
            if run in r:
                r[run].pop('points', None)
                r[run].pop('rows', None)
    return g


def stats(v):
    return {'mean': statistics.mean(v), 'sd': statistics.stdev(v) if len(v) > 1 else None, 'n': len(v), 'values': v} if v else None


def summarize(runs):
    """mean and sample SD of the measured (non-warm-up) repetitions per (kind, N)"""
    out = {}
    for key in sorted({(g['kind'], g['n']) for g in runs}):
        gs = [g for g in runs if (g['kind'], g['n']) == key and not g['warmup']]
        row = {}
        for mode in ('alone', 'concurrent', 'sequential'):
            ms = [g for g in gs if g['mode'] == mode]
            if not ms:
                continue
            good = [g['wall'] for g in ms if g['wall'] is not None and g['n_ok'] == g['n']]
            any_ok = [g['wall'] for g in ms if g['wall'] is not None and g['n_ok'] > 0]
            row[mode] = {'wall': stats(good), 'wall_of_survivors': stats(any_ok), 'groups': len(ms), 'groups_complete': len(good),
                         'runs': sum(g['n'] for g in ms), 'runs_failed': sum(g['n'] - g['n_ok'] for g in ms),
                         'runs_different': sum(g['n_ok'] - g['n_equal'] for g in ms),
                         'runs_equal': sum(g['n_equal'] for g in ms)}
        if 'concurrent' in row and 'sequential' in row and row['concurrent']['wall'] and row['sequential']['wall']:
            row['speedup'] = row['sequential']['wall']['mean'] / row['concurrent']['wall']['mean']
        out[f'{key[0]}-N{key[1]}'] = row
    return out


def parent(a):
    out_dir = Path(a.out)
    out_dir.mkdir(parents=True, exist_ok=False)
    models = Path(tempfile.mkdtemp(prefix='w248-models-'))
    variant_files(models, a.binary if a.program == 'xppautx' else None)
    ns = [int(n) for n in a.ns.split(',')]
    log = open(out_dir / 'run.log', 'w')

    def say(msg):
        print(msg, flush=True)
        log.write(msg + '\n')
        log.flush()

    runs, refs, files_ref = [], {}, {} if a.program == 'xppaut' else None
    alone_repeat, skip_files = {}, set()
    items = [('same', 1, [PARMINS[0]])]
    for n in ns:
        items += [('same', n, [PARMINS[0]] * n), ('diff', n, list(PARMINS[:n]))]
    for rep in range(a.reps + 1):
        if rep == 0:  # the references: each ParMin alone in a fresh HOME, twice (the second shows what is reproducible alone)
            for p in PARMINS:
                for again in (False, True):
                    g = group_run(a, models, [p], 'sequential', f'ref{p}')
                    r = g['runs'][0]
                    if not r.get('ok'):
                        raise SystemExit(f'reference run ParMin {p} failed: {r.get("_error")}')
                    if abs(start_par(r) - HB_PAR) > HB_TOL:
                        raise SystemExit(f'reference run ParMin {p}: the periodic run starts at {start_par(r)}, not the HB')
                    if not again:
                        refs[p] = r
                        if files_ref is not None:
                            files_ref[p] = g['files']
                        continue
                    same = all(c['equal'] for c in compare(refs[p], r).values())
                    unstable = sorted(k for k, v in g['files'].items() if files_ref is not None and files_ref[p].get(k) != v)
                    alone_repeat[p] = {'diagram_equal': same, 'files_not_reproducible': unstable}
                    say(f'reference ParMin {p}: second alone run {"equals" if same else "DIFFERS from"} the first; '
                        f'files that differ alone: {unstable}')
                    skip_files |= {f'{k}' for k in unstable}
        for kind, n, parmins in items:
            modes = ['alone'] if n == 1 else (['concurrent', 'sequential'] if rep % 2 == 0 else ['sequential', 'concurrent'])
            for mode in modes:
                tag = f'r{rep}-{kind}{n}-{mode}'
                g = group_run(a, models, parmins, 'concurrent' if mode == 'concurrent' else 'sequential', tag)
                g.update(kind=kind, mode=mode, rep=rep, warmup=rep == 0, parmins=parmins)
                judge(a, refs, files_ref, skip_files, g, out_dir, tag)
                runs.append(g)
                say(f'rep {rep} {kind} N={n} {mode}: wall {g["wall"] and round(g["wall"], 2)} s, ok {g["n_ok"]}/{g["n"]}, '
                    f'equal to alone {g["n_equal"]}/{g["n"]}' +
                    (f', files vs alone: {len(g["files_vs_alone"]["different"])} different, {len(g["files_vs_alone"]["missing"])} missing'
                     if 'files_vs_alone' in g else ''))
                (out_dir / 'runs.json').write_text(json.dumps(runs, indent=1))
    (out_dir / 'alone-repeat.json').write_text(json.dumps({'by_parmin': alone_repeat, 'files_excluded_from_comparison': sorted(skip_files)}, indent=1))
    with gzip.open(out_dir / 'references.json.gz', 'wt') as f:
        json.dump({str(p): {k: r[k] for k in ('steady', 'periodic')} for p, r in refs.items()}, f)
    (out_dir / 'summary.json').write_text(json.dumps(summarize(runs), indent=1))
    prov = {'program': a.program, 'binary': a.binary, 'binary_sha256': hashlib.sha256(Path(a.binary).read_bytes()).hexdigest(),
            'platform': a.platform, 'system': platform.platform(), 'python': sys.version, 'cpu_count': os.cpu_count(),
            'reps': a.reps, 'ns': ns, 'limit_s': a.limit, 'parmins': PARMINS, 'npr': NPR, 'xppautx_auto_threads': THREADS,
            'when': time.strftime('%Y-%m-%d %H:%M:%S')}
    (out_dir / 'run-provenance.json').write_text(json.dumps(prov, indent=1))
    shutil.rmtree(models, ignore_errors=True)
    say(json.dumps({k: {m: (v[m]['wall'] and round(v[m]['wall']['mean'], 2)) for m in ('alone', 'concurrent', 'sequential') if m in v}
                    | {'speedup': v.get('speedup')} for k, v in summarize(runs).items()}, indent=1))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--child', action='store_true')
    ap.add_argument('--program', choices=('xppaut', 'xppautx'), required=True)
    ap.add_argument('--binary', required=True)
    ap.add_argument('--platform', default='')
    ap.add_argument('--out')
    ap.add_argument('--reps', type=int, default=5)
    ap.add_argument('--ns', default='2,4')
    ap.add_argument('--limit', type=float, default=300)  # s per run; alone a run takes ~25 s
    for k in ('model', 'home', 'run-dir', 'display', 'config-dir', 'ready', 'go'):
        ap.add_argument(f'--{k}')
    a = ap.parse_args()
    a.binary = os.path.abspath(a.binary)
    child(a) if a.child else parent(a)


if __name__ == '__main__':
    main()
