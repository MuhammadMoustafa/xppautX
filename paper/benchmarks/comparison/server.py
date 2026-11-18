#!/usr/bin/env -S uv run --locked python
"""W295 (#353): xppautX's `--server` driven from Python and run in parallel, against pure Python. The study's unit is a model (W699).

A model of examples/ode, published (named below), is run over one parameter grid, GRID_POINTS values of one parameter, by six arms,
each with 1, 2, 4, 8 and 16 worker processes (the grid shared out round-robin, a worker pinned to its CPU: the physical cores first,
then their hyperthreads), the whole sweep's wall time and CPU time measured from outside (the machine's clock protocol: a warm-up, then
five fresh sweeps, rotating order) and each worker's start-up and runs by the worker itself (server_worker.py, which describes the arms).
What the timed run will read: the per-run overhead (a persistent --server against a process a run, and against Python), and the sweep's
throughput as processes are added.

Before any timed sweep an untimed agreement pass runs every arm over the whole grid on one process and compares every row with
xppautX's own (`--silent`, storage_precision double): the RK4 arms to exportcompare's 1e-9, scipy within the sum of the two errors its pilot measured (at
each value the tolerance at which RK45's error against DOP853 is no more than RK4's there, W814). A timed sweep is equal work
when its workers' last rows are the agreement pass's. `--check` is the whole study once, untimed, on a small grid with 1 and 2
processes: a check of the arms and the machinery, never a measurement.
"""
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import sys
import threading
import time
import traceback
from importlib import metadata

from common import (HERE, LONG_RUN_REASON, REPO, TOOLS, before_series, command, environment, initialize, is_long, measure_sha, parser, progress,
                    repetitions, reuse_key, rotation, save, sha, summarize, tree_sha, write_plan)
from integration import import_rows, importable
sys.path.insert(0, str(TOOLS))
from exportcompare import on_runge_kutta, with_double_storage

WORKER = HERE / 'server_worker.py'
GRID_POINTS = 64  # a multiple of every process count, so every worker has the same number of runs
CHECK_GRID_POINTS = 4  # the untimed check's grid
PROCESS_COUNTS = (1, 2, 4, 8, 16)  # #353 Q5: the i7-11700F's cores, then their hyperthreads
CHECK_PROCESS_COUNTS = (1, 2)
LANGUAGES = ('python', 'python_numpy', 'python_numba')  # xppautX's own export targets (W764, W766)
SLICES = 'Equations written as slices'  # python_numpy's header, when the model has an array line

# The models: published ones of examples/ode that the export accepts, each with the one parameter its grid sweeps and its range.
MODELS = {
    'lecar': dict(source='lecar.odex', parameter='iapp', low=0.0, high=0.3,
                  why='the Morris-Lecar membrane oscillator (Koch & Segev, Methods in Neuronal Modeling); the applied current iapp from rest '
                      'through the oscillating range to the quenched one: both regimes, so a run is not one regime\'s cost'),
    'lecdiff200': dict(source='lecdiff200.odex', parameter='iapp', low=0.0, high=0.2,
                       why='XPPAUT\'s lecdiff demo, 200 Morris-Lecar cells coupled by diffusion (400 states): the large network, and the one an '
                           'array-slice export (python_numpy) is for; iapp from rest to oscillation'),
    'wave': dict(source='wave.odex', parameter='d', low=0.05, high=0.4,
                 why='XPPAUT\'s wave demo, a chain of 21 coupled excitable cells, array lines and a threshold function (heav): a second array '
                     'model with another kind of right-hand side; the coupling d'),
}
# the matched tolerance's ladder, the reference and the agreement's slack are server_worker.py's, with their reasons


def grid(a, model):
    n = CHECK_GRID_POINTS if a.check else GRID_POINTS
    return [model['low'] + (model['high'] - model['low']) * k / (n - 1) for k in range(n)]


def process_counts(a):
    return CHECK_PROCESS_COUNTS if a.check else PROCESS_COUNTS


def export(a, folder, language):
    sub = folder / language
    sub.mkdir()
    shutil.copy2(folder / 'model.odex', sub / 'model.odex')
    p = subprocess.run([str(a.xppautx), '--convert', 'model.odex', '--to', language], cwd=sub, capture_output=True, text=True)
    if p.returncode or not (sub / 'model.py').exists():
        raise RuntimeError('%s: --convert --to %s failed: %s' % (folder.name, language, (p.stdout + p.stderr)[-500:]))
    return (sub / 'model.py').read_text(encoding='utf-8')


def stage(a, name):
    """The model on RK4 and double storage (what the exports integrate, and what lets the rows be compared), and its three exports;
    the folder, and the arms this model has (the numpy one only when the model has an array line)."""
    folder = a.out / 'inputs' / name
    folder.mkdir(parents=True)
    text = (REPO / 'examples/ode' / MODELS[name]['source']).read_text(encoding='utf-8')
    (folder / 'model.odex').write_text(with_double_storage(on_runge_kutta()(text)), encoding='utf-8', newline='')
    sliced = {language: SLICES in export(a, folder, language) for language in LANGUAGES}
    arms = ['xppautx_server', 'xppautx_silent', 'export_python', *(['export_python_numpy'] if sliced['python_numpy'] else []), 'export_python_numba', 'scipy']
    return folder, arms


def versions(*names):
    return {n: metadata.version(n) for n in names}


def program_sha(a, arm):
    """What a row of this arm ran: the xppautX build (every arm's script is its export), the worker, and, by arm, the client or the
    interpreter and libraries that run the script."""
    parts = [('xppautX', sha(a.xppautx).encode()), ('worker', WORKER.read_bytes())]
    if arm == 'xppautx_server':
        parts += [(f.name, f.read_bytes()) for f in sorted((REPO / 'python/xppautx').glob('*.py'))] + [('python', sys.version.encode())]
    if arm.startswith('export_') or arm == 'scipy':
        parts += [('python', sys.version.encode()), ('libraries', json.dumps(versions('numpy', 'numba', 'scipy')).encode())]
    return tree_sha(parts)


def sweep_key(a, arm, folder, n, settings):
    # what a sweep's wall time depended on: the arm's program, the measurement code and the study's settings, the input files, the CPUs
    measure = tree_sha([('code', measure_sha(sweep).encode()), ('settings', json.dumps(settings, sort_keys=True).encode())])
    return reuse_key(a, arm, folder, measure, a.cpu_order[:n])


def key_of(folder, n, settings):
    # importable's key function for this unit's sweeps of n processes (the key does not depend on the row's own model: the unit's inputs do)
    return lambda a, arm, _model: sweep_key(a, arm, folder, n, settings)


def cpu_for(a, k):
    # the worker k's CPU: the physical cores, then the hyperthreads; a check on a smaller machine wraps round
    return a.cpu_order[k % len(a.cpu_order)]


def worker_jobs(folder, arm, n, values, job):
    """Worker k's folder holds its job: the grid indices k, k+n, ... and their values"""
    folders = []
    for k in range(n):
        f = folder / ('w%d' % k)
        f.mkdir(parents=True)
        save(f / 'job.json', dict(job, arm=arm, indices=list(range(len(values)))[k::n], values=values[k::n]))
        folders.append(f)
    return folders


def sweep(a, folder, arm, n, values, job):
    """One sweep: n workers started together, each pinned, run to their end; the wall and CPU time of the whole, the workers' own
    results. The clock covers the processes' start-up (the workers' imports, a server's load): what a user waits for."""
    folders = worker_jobs(folder, arm, n, values, job)
    ready, ended, outcome = threading.Barrier(n + 1), [0.0] * n, [None] * n

    def work(k):
        ready.wait()
        outcome[k] = command([sys.executable, WORKER, 'job.json'], folders[k], cpus=[cpu_for(a, k)], env=environment(folders[k] / 'home'))
        ended[k] = time.perf_counter()
    threads = [threading.Thread(target=work, args=(k,)) for k in range(n)]
    for t in threads:
        t.start()
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    ready.wait()
    start = time.perf_counter()
    for t in threads:
        t.join()
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    results = [json.loads((f / 'result.json').read_text()) if (f / 'result.json').exists() else None for f in folders]
    return dict(wall_seconds=max(ended) - start, cpu_seconds=after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime,
                exit_codes=[o['exit_code'] for o in outcome], workers=results, folders=folders)


def mean(numbers):
    return sum(numbers) / len(numbers) if numbers else None


def runs_of(result):
    return sorted((r for w in result['workers'] if w for r in w['runs']), key=lambda r: r['index'])


def agreement(a, name, folder, arms, values, base):
    """The untimed pass: every arm over the whole grid on one process, rows compared with xppautX's own. Returns {arm: its summary};
    raises when an arm disagrees."""
    root = a.out / 'agreement' / name
    summary = {}
    for arm in ['xppautx_silent', *[x for x in arms if x != 'xppautx_silent']]:
        job = dict(base, keep_rows=True)
        if arm != 'xppautx_silent':
            job['reference'] = str(root / 'xppautx_silent/w0')
            if arm == 'scipy':
                pilot_folder = root / 'pilot'
                pilot_folder.mkdir(parents=True)
                save(pilot_folder / 'job.json', dict(base, arm='pilot', values=values, indices=list(range(len(values))), reference=job['reference'], keep_rows=False))
                code = command([sys.executable, WORKER, 'job.json'], pilot_folder, cpus=[cpu_for(a, 0)], env=environment(pilot_folder / 'home'))
                if code['exit_code']:
                    raise RuntimeError('%s: the scipy pilot failed: %s' % (name, (pilot_folder / 'run.log').read_text()[-1500:]))
                points = json.loads((pilot_folder / 'result.json').read_text())['points']
                job.update(rtols=[p['rtol'] for p in points], bounds=[p['rk4_error'] + p['scipy_error'] for p in points])
                rk4 = [p['rk4_error'] for p in points]
                progress(a, '%s: scipy matched at each value, rtol %g to %g (RK4 error %.3g to %.3g)' % (name, max(job['rtols']), min(job['rtols']), min(rk4), max(rk4)))
        result = sweep(a, root / arm, arm, 1, values, job)
        runs = runs_of(result)
        bad = [r for r in runs if r['where']] if arm != 'xppautx_silent' else []
        largest = max((r['largest'] or 0.0 for r in runs), default=0.0)
        valid = result['exit_codes'] == [0] and len(runs) == len(values) and all(r['finite'] for r in runs)
        summary[arm] = dict(valid=valid and not bad, runs=len(runs), largest_relative_difference=None if arm == 'xppautx_silent' else largest,
                            disagreements=[dict(index=r['index'], where=r['where']) for r in bad[:5]], rows={r['index']: r['rows'] for r in runs},
                            last_rows={str(r['index']): r['last_row'] for r in runs}, **({'tolerances': job['rtols'], 'bounds': job['bounds']} if arm == 'scipy' else {}))
        progress(a, '%s: agreement %s: %s over %d runs%s' % (name, arm, 'ok' if summary[arm]['valid'] else 'DISAGREES', len(runs),
                                                           '' if arm == 'xppautx_silent' else ', largest relative difference %.3g' % largest))
        if not summary[arm]['valid']:
            raise RuntimeError('%s: %s does not agree with xppautX: %s' % (name, arm, summary[arm]['disagreements'] or result['exit_codes']))
    shutil.rmtree(root / 'xppautx_silent' / 'w0', ignore_errors=True)  # the reference's run folders (output.dat of every value) are not kept
    save(a.out / 'agreement' / (name + '.json'), summary)
    return summary


def same_last_rows(runs, summary, values):
    """The timed sweep did the agreement pass's work: every value run once, with the rows' count and last row the agreement pass's"""
    return len(runs) == len(values) and all(r['rows'] == summary['rows'][r['index']] and r['last_row'] == summary['last_rows'][str(r['index'])] and r['finite'] for r in runs)


def record_of(name, arm, n, rep, result, summary, values, key):
    runs = runs_of(result)
    workers = [w for w in result['workers'] if w]
    valid = result['exit_codes'] == [0] * n and len(workers) == n
    rec = dict(workload='%s-n%d' % (name, n), engine=arm, row='run', repetition=rep, processes=n, runs=len(runs), valid=valid,
               equal_work=valid and same_last_rows(runs, summary, values), wall_seconds=result['wall_seconds'], cpu_seconds=result['cpu_seconds'],
               wall_per_point=result['wall_seconds'] / len(values), cpu_per_point=result['cpu_seconds'] / len(values),
               runs_per_second=len(values) / result['wall_seconds'], per_run_seconds=mean([r['seconds'] for r in runs]),
               startup_seconds=mean([w['startup_seconds'] for w in workers]), first_run_seconds=mean([w['runs'][0]['seconds'] for w in workers if w['runs']]),
               fetch_seconds=mean([r['fetch_seconds'] for r in runs if r['fetch_seconds'] is not None]) if arm == 'xppautx_server' else None,
               reuse_key=key, tolerances=summary.get('tolerances'))
    return rec


def run_unit(a, name, records):
    model = MODELS[name]
    folder, arms = stage(a, name)
    values = grid(a, model)
    a.cpu_order = a.cores + [c for c in sorted(os.sched_getaffinity(0)) if c not in a.cores]
    if a.timed and len(a.cpu_order) < max(PROCESS_COUNTS):
        raise RuntimeError('the study pins up to %d workers, one CPU each: this machine has %d' % (max(PROCESS_COUNTS), len(a.cpu_order)))
    settings = dict(parameter=model['parameter'], values=values, process_counts=list(process_counts(a)), model=name, arms=arms, versions=versions('numpy', 'numba', 'scipy'))
    base = dict(inputs=str(folder), model='model.odex', parameter=model['parameter'], binary=str(a.xppautx), keep_rows=False)
    if a.plan:
        return plan_unit(a, name, folder, arms, settings)
    progress(a, '%s: parameter %s over %d values in [%g, %g]; arms %s; processes %s' % (name, model['parameter'], len(values), model['low'], model['high'], ', '.join(arms), process_counts(a)))
    summary = agreement(a, name, folder, arms, values, base)
    before_series(a)
    long_runs = set()
    for rep in repetitions(a):
        progress(a, '%s: repetition %d of %d%s' % (name, rep, a.repeats, ' (warm-up, not counted)' if rep == 0 else ''))
        for n in process_counts(a):
            workload = '%s-n%d' % (name, n)
            for arm in rotation(arms, rep):
                if rep > 1 and (arm, n) in long_runs:
                    continue  # a sweep whose warm-up was long is counted once (W452)
                key = sweep_key(a, arm, folder, n, settings)
                found, why = importable(a, workload, arm, rep, {'run': folder}, key_of=key_of(folder, n, settings))
                if found:
                    records.extend(import_rows(a, found, workload, arm, rep))
                    progress(a, '%s %s rep %d: imported from %s' % (workload, arm, rep, a.reuse))
                    continue
                job = dict(base, **({'rtols': summary[arm]['tolerances']} if arm == 'scipy' else {}))
                result = sweep(a, a.out / 'runs' / workload / str(rep) / arm / 'run', arm, n, values, job)
                rec = record_of(name, arm, n, rep, result, summary[arm], values, key)
                if is_long(rec):
                    long_runs.add((arm, n))
                    rec['repetitions_reason'] = LONG_RUN_REASON
                records.append(rec)
                save(a.out / 'runs.json', records)
                progress(a, '%s %s rep %d: %.3f s for %d runs (%.2f runs/s), per run %.4f s, start-up %.3f s%s' % (
                    workload, arm, rep, rec['wall_seconds'], len(values), rec['runs_per_second'], rec['per_run_seconds'], rec['startup_seconds'],
                    '' if rec['equal_work'] else ' NOT EQUAL WORK'))
    return all(r['equal_work'] for r in records if r['workload'].startswith(name + '-n'))


def plan_unit(a, name, folder, arms, settings):
    """--plan: what an earlier attempt of the unit supplies and what would run, by the keys a run uses; nothing is run."""
    if not a.reuse:
        return write_plan(a, name, ['no earlier attempt'], 0)
    imported, would_run = 0, []
    for rep in repetitions(a):
        for n in process_counts(a):
            for arm in arms:
                found, why = importable(a, '%s-n%d' % (name, n), arm, rep, {'run': folder}, key_of=key_of(folder, n, settings))
                if found:
                    imported += 1
                else:
                    would_run.append('%s n=%d repetition %d: %s' % (arm, n, rep, why))
    write_plan(a, name, would_run, imported)


def main():
    p = parser(__doc__)
    a = p.parse_args()
    initialize(a)
    a.program_sha = {arm: (lambda arm=arm: program_sha(a, arm)) for arm in ('xppautx_server', 'xppautx_silent', 'export_python', 'export_python_numpy', 'export_python_numba', 'scipy')}
    names = [n for n in MODELS if not a.only or n in a.only.split(',')]
    if a.list:
        print(json.dumps(names))
        return
    records, passed, failures = [], True, []
    for name in names:
        try:
            progress(a, 'WORKLOAD %s' % name)
            ok = run_unit(a, name, records)
            passed &= bool(ok) or a.plan
        except Exception as e:
            progress(a, 'FAILED %s: %s' % (name, e))
            save(a.out / 'failures' / (name + '.json'), dict(error=str(e), type=type(e).__name__, traceback=traceback.format_exc()))
            failures.append(name)
            passed = False
            if a.stop_on_failure:
                break
    if a.plan:
        return
    summarize(records, a.out)
    save(a.out / 'completion.json', dict(passed=passed, check=a.check, rows=len(records), driver_failures=failures))
    if failures or not passed:
        sys.exit('server study: a sweep was not equal work or a unit failed; retained results name them')


if __name__ == '__main__':
    main()
