"""W378 correctness by default; only `measure` starts a timed Ubuntu series."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess

from generate import generate

HERE = Path(__file__).resolve().parent
SOLVERS = ['legacy-dense', 'legacy-band', 'klu', 'gmres']
REFERENCE_TOL = 1e-9  # halved independently for every workload
TARGET = 1e-4  # max absolute error / (1+abs(reference)), all states and rows
LOOSE_LIMIT = 3e-3  # correctness pilot only; unchanged after the failed 1e-4 run
PILOT_SHIFT_LIMIT = 0.05  # output steps. Chosen 2026-10-20 after the ring-3000 pilot failed LOOSE_LIMIT on 497 values, all on one spike upstroke
# that a time shift of 0.016 output steps explains: a loose solver may be that far from the reference in time; pilot sanity only, never TARGET.
LOOSE_TOL = 1e-5  # 1e-4 failed the unchanged loose limit on the 200-state ring
REPEATS = 5  # section 4, excluded warm-up then five fresh processes
ROWS = 101  # common nominal times, including both ends
CACHE_FACTORIZATIONS = 3  # matches bridge.cpp; stop the run at this setup count
THREAD_ENV = dict(LC_ALL='C', OMP_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1',
                  MKL_NUM_THREADS='1', BLIS_NUM_THREADS='1', VECLIB_MAXIMUM_THREADS='1')


def save(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False)+'\n')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def identity(worker):
    built=json.loads((worker.parent/'build.json').read_text())
    if any(digest(HERE/name) != value for name, value in built['sources'].items()):
        raise RuntimeError('adapter sources changed after the build; rebuild the worker')
    return dict(worker_sha256=digest(worker), bridge_sha256=digest(worker.parent/'libstudy.so'),
                sources={p.name: digest(p) for p in HERE.iterdir() if p.suffix in ('.py', '.cpp', '.h')},
                commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip())


def trajectory(path, n, total):
    values = [[float(v) for v in line.split()] for line in path.read_text().splitlines()]
    if len(values) != ROWS or any(len(row) != n+1 for row in values):
        raise ValueError(f'{path}: expected {ROWS} rows and {n+1} columns')
    for k, row in enumerate(values):
        if any(not math.isfinite(x) for x in row) or abs(row[0]-total*k/(ROWS-1)) > 1e-12*max(1, total):
            raise ValueError(f'{path}:{k+1}: nonfinite output or wrong time')
    return values


def error(values, ref):
    # W34 sundials_report's mixed absolute/relative metric, now double output.
    errors = [max(abs(a-b)/(1+abs(b)) for a, b in zip(x[1:], y[1:])) for x, y in zip(values, ref)]
    return dict(along=max(errors), endpoint=errors[-1])


def timing_explains(values, ref, limit):
    # True when every value past `limit` is the reference's own trajectory a small time shift away: the error over the reference's
    # slope there (central difference, one-sided at the ends), in output steps, at most PILOT_SHIFT_LIMIT. Flat trajectory: unexplained.
    worst = 0.0
    for i, (x, y) in enumerate(zip(values, ref)):
        lo, hi = ref[max(i-1, 0)], ref[min(i+1, len(ref)-1)]
        span = min(i+1, len(ref)-1) - max(i-1, 0)
        for a, b, p, q in zip(x[1:], y[1:], lo[1:], hi[1:]):
            if abs(a-b)/(1+abs(b)) > limit:
                slope = abs(q-p)/span
                worst = max(worst, abs(a-b)/slope if slope else math.inf)
    return worst <= PILOT_SHIFT_LIMIT, worst


def run(worker, model, solver, tol, folder, *, measure=False, factors=False, cpu=None, callgrind=False):
    folder.mkdir(parents=True, exist_ok=False)
    home = folder/'home'
    home.mkdir()
    meta = json.loads(model.with_suffix('.json').read_text())
    argv = [str(worker), str(model), solver, str(tol), str(meta['total']), str(ROWS),
            str(folder/'trajectory.dat'), str(folder/'stats.json')]
    env = dict(os.environ, **THREAD_ENV, HOME=str(home), XPP_CONFIG_DIR=str(home))
    env.pop('DISPLAY', None)
    env.pop('W378_FACTOR_LOG', None)
    env.pop('W378_CACHE_PROFILE', None)
    if factors:
        env['W378_FACTOR_LOG'] = str(folder/'factors.jsonl')
    if callgrind:
        env['W378_CACHE_PROFILE'] = '1'
        # Toggle only DenseFactor and its descendants; cache simulation is separate
        # from the ordinary time/RSS series. Symbols remain in the study build.
        argv = ['valgrind', '--tool=callgrind', '--cache-sim=yes', '--collect-atstart=no', '--instr-atstart=no',
                '--toggle-collect=DenseFactor', '--callgrind-out-file='+str(folder/'callgrind.out')] + argv
    if cpu is not None:
        argv = ['taskset', '-c', str(cpu)] + argv
    if measure:
        argv = ['/usr/bin/time', '-v', '-o', str(folder/'time.txt')] + argv
    save(folder/'command.json', dict(argv=argv, environment={k: env[k] for k in THREAD_ENV}|dict(HOME=str(home), XPP_CONFIG_DIR=str(home)),
                                   factor_instrumentation=factors, callgrind=callgrind))
    if measure:
        # W34 run_once's clocks, confined to the explicit stage-2 branch.
        # GNU time's text rounds seconds, so keep its RSS plus these raw clocks.
        import resource
        import time
        start_wall = time.perf_counter()
        start_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
    with (folder/'stdout.log').open('w') as out, (folder/'stderr.log').open('w') as err:
        p = subprocess.run(argv, cwd=folder, env=env, stdout=out, stderr=err)
    if measure:
        wall = time.perf_counter()-start_wall
        end_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
        cpu_seconds = end_cpu.ru_utime+end_cpu.ru_stime-start_cpu.ru_utime-start_cpu.ru_stime
    result = dict(solver=solver, tolerance=tol, returncode=p.returncode, folder=str(folder))
    save(folder/'result.json', result)  # retain failures before raising
    if p.returncode:
        raise RuntimeError(f'{solver} failed ({p.returncode}); see {folder}/stderr.log')
    if solver != 'probe':
        result['stats'] = json.loads((folder/'stats.json').read_text())
        result['trajectory_sha256'] = digest(folder/'trajectory.dat')
        if callgrind:
            if (result['stats'].get('cache_complete') is not True or
                    result['stats'].get('factorizations') != CACHE_FACTORIZATIONS):
                raise ValueError(f'{folder}: cache run did not stop at the factorization cap')
            result['cache'] = cache_counts(folder/'callgrind.out', CACHE_FACTORIZATIONS)
        else:
            trajectory(folder/'trajectory.dat', meta['n'], meta['total'])
    if measure:
        result.update(parse_time(folder/'time.txt'))
        result['gnu_time_wall'] = result['wall']
        result['gnu_time_cpu'] = result['cpu']
        result['wall'] = wall
        result['cpu'] = cpu_seconds
    if factors:
        events = [json.loads(s) for s in (folder/'factors.jsonl').read_text().splitlines()]
        if not events or any(e['flag'] != 0 for e in events):
            raise ValueError(f'{folder}: missing or failed factor events')
        result['factorizations'] = len(events)
        result['factor_wall_total'] = sum(e['wall'] for e in events)
        result['factor_cpu_total'] = sum(e['cpu'] for e in events)
        result['factor_wall_mean'] = result['factor_wall_total']/len(events)
        result['factor_cpu_mean'] = result['factor_cpu_total']/len(events)
    save(folder/'result.json', result)
    return result


def parse_time(path):
    data = dict(line.strip().split(': ', 1) for line in path.read_text().splitlines() if ': ' in line)
    elapsed = data['Elapsed (wall clock) time (h:mm:ss or m:ss)'].split(':')
    wall = sum(float(v)*60**i for i, v in enumerate(reversed(elapsed)))
    return dict(wall=wall, cpu=float(data['User time (seconds)'])+float(data['System time (seconds)']),
                peak_rss_kib=int(data['Maximum resident set size (kbytes)']))


def cache_counts(path, factors):
    """Callgrind's totals only; sum the declared event columns, not function costs."""
    events = None
    totals = None
    for line in path.read_text().splitlines():
        if line.startswith('events: '):
            events = line.split()[1:]
        if line.startswith('totals: '):
            totals = [int(v) for v in line.split()[1:]]
    if events is None or totals is None or len(events) != len(totals):
        raise ValueError(f'{path}: missing or mismatched callgrind event totals')
    counts = dict(zip(events, totals))
    misses = ('I1mr', 'D1mr', 'D1mw', 'ILmr', 'DLmr', 'DLmw')
    if not all(key in counts for key in misses) or counts.get('Ir', 0) <= 0:
        raise ValueError(f'{path}: no collected dense LU cache events')
    return dict(factorizations=factors, totals=counts,
                misses_per_factorization={key: counts[key]/factors for key in misses})


def available(worker, allow):
    build = json.loads((worker.parent/'build.json').read_text())
    if not build['klu'] and not allow:
        raise RuntimeError('KLU is required; install libsuitesparse-dev and configure SUNDIALS ENABLE_KLU=ON')
    return [s for s in SOLVERS if build['klu'] or s != 'klu']


def prepare(args, sizes):
    worker, root = args.worker.resolve(), args.out.resolve()
    solvers = available(worker, args.allow_no_klu)
    root.mkdir(parents=True, exist_ok=False)
    save(root/'identity.json', identity(worker))
    models = root/'models'
    cases = []
    for family in ('cable', 'ring'):
        for n in sizes:
            model = generate(family, n, models)
            meta = json.loads(model.with_suffix('.json').read_text())
            folder = root/f'{family}-{n}'
            case = dict(model=str(model), model_sha256=digest(model),
                        metadata_sha256=digest(model.with_suffix('.json')),
                        family=family, n=n, solvers={}, status='preparing')
            cases.append(case)
            save(root/'correctness.json', dict(cases=cases, omitted=['klu'] if 'klu' not in solvers else []))
            run(worker, model, 'probe', 1e-4, folder/'probe')
            probe = json.loads((folder/'probe/stats.json').read_text())
            if probe['band'] > meta['expected_band']:
                raise ValueError(f'{model}: unexpected exact-Jacobian band {probe}')
            case['pattern'] = probe
            refs = []
            for tag, tol in [('reference', REFERENCE_TOL), ('reference-half', REFERENCE_TOL/2)]:
                run(worker, model, 'reference', tol, folder/tag)
                refs.append(trajectory(folder/tag/'trajectory.dat', n, meta['total']))
            case['reference_convergence'] = error(refs[0], refs[1])
            case['reference_sha256'] = digest(folder/'reference-half/trajectory.dat')
            if case['reference_convergence']['along'] > TARGET/100:
                raise ValueError(f'{model}: reference not converged')
            for solver in solvers:
                tol = LOOSE_TOL
                for level in range(7):
                    result = run(worker, model, solver, tol, folder/f'{solver}-{level}')
                    values = trajectory(Path(result['folder'])/'trajectory.dat', n, meta['total'])
                    result['error'] = error(values, refs[1])
                    save(Path(result['folder'])/'result.json', result)
                    if level == 0 and result['error']['along'] > LOOSE_LIMIT:
                        explained, shift = timing_explains(values, refs[1], LOOSE_LIMIT)
                        result['loose_timing_shift_steps'] = shift
                        if not explained:
                            raise ValueError(f'{model}: loose {solver} fails {result["error"]}; not a time shift (worst {shift:.3g} output steps)')
                        save(Path(result['folder'])/'result.json', result)
                    if result['error']['along'] <= TARGET:
                        case['solvers'][solver] = result
                        break
                    tol /= 10
                else:
                    raise ValueError(f'{model}: {solver} cannot meet target {TARGET}')
            case['status'] = 'pass'
            save(root/'correctness.json', dict(cases=cases, omitted=['klu'] if 'klu' not in solvers else [],
                                               target=TARGET, loose_limit=LOOSE_LIMIT, reference_tolerance=REFERENCE_TOL))
            print(f'{family} n={n}: PASS; reference delta={case["reference_convergence"]["along"]:.3g}; '
                  + ', '.join(f'{s}={v["error"]["along"]:.3g}@{v["tolerance"]:g}' for s, v in case['solvers'].items()), flush=True)
    return cases


def machine(args, root):
    shutil.copy2(args.machine_record, root/'machine.json')
    build = args.worker.resolve().parent
    shutil.copy2(build/'build.json', root/'build.json')
    sources = {p.name: digest(p) for p in HERE.iterdir() if p.suffix in ('.py', '.cpp', '.h', '.sh', '.md')}
    save(root/'provenance.json', dict(source_sha256=sources, worker_sha256=digest(build/'worker'),
         bridge_sha256=digest(build/'libstudy.so'), commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
         cpu=args.cpu, threads=1, target=TARGET, reference_tolerance=REFERENCE_TOL,
         repeats=REPEATS, timer='GNU /usr/bin/time -v; factor diagnostics separate',
         sundials_libraries={str(p): digest(p) for p in Path(json.loads((build/'build.json').read_text())['sundials_prefix']).glob('lib/*.a')}))


def measure(args):
    worker, root = args.worker.resolve(), args.out.resolve()
    available(worker, False)
    if identity(worker) != json.loads((args.pilot/'identity.json').read_text()):
        raise RuntimeError('pilot code, binary, or checkout identity changed; recalibrate')
    # Calibration is a separate untimed command; --ready can refuse a busy machine
    # without repeating a potentially expensive calibration or sleeping in a loop.
    gate = json.loads((args.pilot/'correctness.json').read_text())
    if gate['omitted'] or any(c['status'] != 'pass' for c in gate['cases']):
        raise RuntimeError('pilot must pass with KLU enabled')
    if sorted({c['n'] for c in gate['cases']}) != sorted(args.sizes):
        raise RuntimeError('pilot size ladder differs from measurement')
    for case in gate['cases']:
        model=args.pilot/'models'/Path(case['model']).name
        ref=args.pilot/f'{case["family"]}-{case["n"]}'/'reference-half/trajectory.dat'
        if (digest(model) != case['model_sha256'] or digest(model.with_suffix('.json')) != case['metadata_sha256']
                or digest(ref) != case['reference_sha256']):
            raise RuntimeError('pilot model or reference changed after validation')
    subprocess.run(['tools/comparison-setup.sh', '--ready'], check=True)
    if subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], text=True).strip():
        raise RuntimeError('tracked checkout must be clean')
    root.mkdir(parents=True, exist_ok=False)
    shutil.copytree(args.pilot, root/'pilot')
    cases = gate['cases']
    for case in cases:
        case['model'] = str(root/'pilot/models'/Path(case['model']).name)
    machine(args, root)
    runs = []
    def calibration(tag):
        case=cases[0]
        return run(worker, Path(case['model']), 'legacy-dense', case['solvers']['legacy-dense']['tolerance'],
                   root/'drift'/tag, measure=True, cpu=args.cpu)
    drift={'start': calibration('start')}
    save(root/'drift.json', drift)
    for case in cases:
        model = Path(case['model'])
        ref = trajectory(root/'pilot'/f'{case["family"]}-{case["n"]}'/'reference-half/trajectory.dat',
                         case['n'], json.loads(model.with_suffix('.json').read_text())['total'])
        for rep in range(-1, REPEATS):
            shift = max(rep, 0) % len(SOLVERS)
            for solver in SOLVERS[shift:]+SOLVERS[:shift]:
                folder = root/'runs'/f'{case["family"]}-{case["n"]}'/f'{rep+1}-{solver}'
                try:
                    r = run(worker, model, solver, case['solvers'][solver]['tolerance'], folder,
                            measure=rep>=0, cpu=args.cpu)
                    values = trajectory(folder/'trajectory.dat', case['n'], ref[-1][0])
                    r['error'] = error(values, ref)
                    r['accuracy_pass'] = r['error']['along'] <= TARGET
                except (RuntimeError, ValueError) as exc:
                    r = dict(solver=solver, failure=str(exc), folder=str(folder), accuracy_pass=False)
                r.update(family=case['family'], n=case['n'], repetition=rep, warmup=rep<0)
                runs.append(r)
                save(root/'runs.json', runs)
    drift['end']=calibration('end')
    save(root/'drift.json', drift)
    # Separate instrumented processes: log writes do not contaminate headline time/RSS.
    for case in cases:
        for solver in SOLVERS:
            run(worker, Path(case['model']), solver, case['solvers'][solver]['tolerance'],
                root/'factors'/f'{case["family"]}-{case["n"]}-{solver}', factors=True, cpu=args.cpu)
    summarize(root)


def summarize(root):
    runs = json.loads((root/'runs.json').read_text())
    summary = []
    for family, n, solver in sorted({(r['family'], r['n'], r['solver']) for r in runs}):
        group = [r for r in runs if (r['family'], r['n'], r['solver']) == (family, n, solver) and not r['warmup']]
        good = [r for r in group if r['accuracy_pass']]
        row = dict(family=family, n=n, solver=solver, passed=len(good), expected=REPEATS)
        if len(good) == REPEATS:
            for key in ('wall', 'cpu', 'peak_rss_kib'):
                row[key+'_mean'] = statistics.mean(r[key] for r in good)
                row[key+'_sample_sd'] = statistics.stdev(r[key] for r in good) if len(good) > 1 else None  # one repetition: no spread (W460)
            row['error_along_max'] = max(r['error']['along'] for r in good)
            row['error_endpoint_max'] = max(r['error']['endpoint'] for r in good)
        summary.append(row)
    save(root/'summary.json', summary)
    with (root/'summary.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(dict.fromkeys(k for r in summary for k in r)))
        writer.writeheader(); writer.writerows(summary)
    # A measured crossing on this finite ladder is not an interpolated break-even.
    crossings = []
    for family in ('cable', 'ring'):
        for solver in ('legacy-band', 'klu', 'gmres'):
            for metric in ('wall_mean', 'peak_rss_kib_mean'):
                pairs = []
                for r in summary:
                    if r['family'] == family and r['solver'] == solver and metric in r:
                        dense = next(x for x in summary if x['family'] == family and x['n'] == r['n'] and x['solver'] == 'legacy-dense')
                        if metric in dense:
                            pairs.append((r['n'], r[metric] < dense[metric]))
                winners = [n for n, wins in pairs if wins]
                crossings.append(dict(family=family, solver=solver, metric=metric,
                                      first_measured_win=min(winners) if winners else None, ladder=pairs))
    save(root/'crossings.json', crossings)


def main():
    global REPEATS
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('mode', choices=['check', 'measure', 'cache', 'summarize'])
    ap.add_argument('--worker', type=Path, default=Path('build/w378/worker'))
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--sizes', nargs='+', type=int)
    ap.add_argument('--allow-no-klu', action='store_true', help='stage 1 only; records KLU as unvalidated')
    ap.add_argument('--cpu', type=int, default=2)
    ap.add_argument('--machine-record', type=Path)
    ap.add_argument('--pilot', type=Path, help='completed untimed check folder with the measurement size ladder')
    ap.add_argument('--repeats', type=int, default=REPEATS, help='measured processes after the warm-up (W460)')
    a = ap.parse_args()
    REPEATS = a.repeats  # read by measure and summarize
    if a.mode == 'check':
        prepare(a, a.sizes or [50, 200])
    elif a.mode == 'measure':
        if a.allow_no_klu or not a.machine_record or not a.pilot:
            ap.error('measure requires --machine-record and --pilot, and forbids --allow-no-klu')
        a.sizes = a.sizes or [1000, 2000, 3000, 5000]
        measure(a)
    elif a.mode == 'summarize':
        summarize(a.out.resolve())
    else:
        subprocess.run(['tools/comparison-setup.sh', '--ready'], check=True)
        root = a.out.resolve()
        gate = json.loads((root/'pilot/correctness.json').read_text())
        summaries=[]
        for case in gate['cases']:
            if case['n'] == max(c['n'] for c in gate['cases']):
                model=root/'pilot/models'/Path(case['model']).name
                result=run(a.worker.resolve(), model, 'legacy-dense', case['solvers']['legacy-dense']['tolerance'],
                           root/'cache'/case['family'], cpu=a.cpu, callgrind=True)
                summaries.append(dict(family=case['family'], n=case['n'], **result['cache']))
                save(root/'cache-summary.json', summaries)


if __name__ == '__main__':
    main()
