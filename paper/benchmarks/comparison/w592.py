#!/usr/bin/env -S uv run --locked python
"""W592 (#658): xppautX on several cores, on the frozen series' build; XPPAUT and AUTO-07p are not run (W248: their concurrent runs break).

    w592.py threads --out O --xppautx X --machine-record M   # one AUTO continuation on 1, 2 and 4 threads: time, speed-up, diagram bytes
    w592.py jobs    --out O --xppautx X --machine-record M   # N = 1, 2, 4 integration jobs (kuramot100, k swept) started together, one HOME
    w592.py auto    --out O --xppautx X                      # N AUTO jobs together: auto/concurrent/concurrent.py (W248's driver, its set of ParMin)

threads reuses continuation.native (the series' own run of a description) on the descriptions the series saved (window.json);
jobs reuses integration.convert and common.environment/short_home/physical_cores; auto runs W248's own driver as it is.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace

from common import BENCH, REPO, before_series, environment, physical_cores, save, sha, short_home, wait_until_ready
from continuation import aggregate, native
from integration import convert

FROZEN = Path.home() / 'xppautx-comparison/final'
SERIES = FROZEN / 'continuation/attempt-001'
OLD_ROOT, NEW_ROOT = '/home/muhammad/xppautx-comparison/w583-final-2/', str(FROZEN) + '/'  # the series moved its folder after it ran
# The continuation workloads with enough work per mesh interval to gain: workload, mode folder, target runs (None: every run).
THREAD_WORKLOADS = (('P1', 'adaptive', ['periodic']), ('P1', 'fixed', ['periodic']), ('P2', 'adaptive', ['periodic']),
                    ('T1', 'adaptive-repeat-47', ['hb_soma']), ('ode_mu26_control', 'adaptive', None), ('ode_mu26_blocked', 'adaptive', None))
THREADS = (1, 2, 4)
SWEEP_MODEL = BENCH / 'five-model-screen/sources/kuramot100.ode'  # Kuramoto's 100 oscillators (the screen's, published)
SWEEP_K = (1.0, 0.5, 1.5, 2.0)  # the coupling k swept; the first N are the N jobs
JOBS = (1, 2, 4)
GRID = BENCH / 'auto/concurrent/concurrent.py'


def stats(values):
    return dict(mean=statistics.mean(values), sd=statistics.stdev(values) if len(values) > 1 else None, values=values)


def cores_of(a):
    return physical_cores(json.loads(a.machine_record.read_text()))


def thread_description(name, mode):
    text = (SERIES / name / mode / 'window.json').read_text().replace(OLD_ROOT, NEW_ROOT)
    return json.loads(text)['description']


def threads(a):
    cores = cores_of(a)
    if len(cores) < max(THREADS):
        raise ValueError('the machine record has fewer than four physical cores')
    a.out.mkdir(parents=True)
    records, original = [], os.sched_getaffinity(0)
    work = Path(tempfile.mkdtemp(prefix='w592-'))
    before_series(a)
    try:
        for name, mode, targets in THREAD_WORKLOADS:
            d = thread_description(name, mode)
            names = [r['name'] for r in d['runs'] if 'skip' not in r and '-repeat-' not in r['name']]
            targets = targets or names
            for rep in range(a.repeats + 1):
                k = rep % len(THREADS)
                for n in THREADS[k:] + THREADS[:k]:
                    os.sched_setaffinity(0, cores[:n])
                    os.environ.update(OMP_NUM_THREADS=str(n), OPENBLAS_NUM_THREADS='1')
                    folder = work / ('%s-%s-%d-%d' % (name, mode, n, rep))
                    began = time.perf_counter()
                    result = native(a, copy.deepcopy(d), folder, n, False, 'difference', True)
                    made = aggregate(result, targets)
                    records.append(dict(workload=name, mode=mode, threads=n, repetition=rep, targets=made,
                                        wall_seconds=sum(v['wall_seconds'] for v in made.values()),
                                        diagram_sha256=hashlib.sha256(result['data']).hexdigest(), diagram_bytes=len(result['data']),
                                        process_wall_seconds=time.perf_counter() - began))
                    shutil.rmtree(folder)
                    print(name, mode, 'threads', n, 'rep', rep, round(records[-1]['wall_seconds'], 3), flush=True)
                    save(a.out / 'runs.json', records)
    finally:
        os.sched_setaffinity(0, original)
        shutil.rmtree(work, ignore_errors=True)
    summary = []
    for name, mode, _ in THREAD_WORKLOADS:
        rows = [r for r in records if r['workload'] == name and r['mode'] == mode]
        digests = {r['diagram_sha256'] for r in rows}
        row = dict(workload=name, mode=mode, identical=len(digests) == 1, diagram_sha256=sorted(digests), diagram_bytes=rows[0]['diagram_bytes'])
        for n in THREADS:
            row['wall_%d' % n] = stats([r['wall_seconds'] for r in rows if r['threads'] == n and r['repetition'] > 0])
        for n in THREADS[1:]:
            row['speedup_%d' % n] = row['wall_1']['mean'] / row['wall_%d' % n]['mean']
        summary.append(row)
    save(a.out / 'summary.json', summary)
    provenance(a, a.out)


def provenance(a, folder):
    save(folder / 'provenance.json', dict(argv=sys.argv, binary=str(a.xppautx), binary_sha256=sha(a.xppautx), repetitions=a.repeats,
         commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(), protocol='one warm-up (rep 0, excluded), counted repetitions, rotating order'))


def start(folder, argv, home, cpu):
    folder.mkdir(parents=True)
    out = (folder / 'run.log').open('w')
    return subprocess.Popen(['taskset', '-c', str(cpu), *map(str, argv)], cwd=folder, env=environment(home), stdout=out, stderr=subprocess.STDOUT)


def group(a, models, cores, ks, mode, home):
    """N jobs started together ('concurrent') or one after another ('sequential'), one HOME; (wall seconds, [output sha256 or None])."""
    base = Path(tempfile.mkdtemp(prefix='w592-g-'))
    argv = lambda k: [a.xppautx, models / ('k%s.odex' % k), '--silent', '--internset', '0']  # noqa: E731
    try:
        began = time.perf_counter()
        if mode == 'concurrent':
            procs = [start(base / str(i), argv(k), home, cores[i]) for i, k in enumerate(ks)]
            codes = [p.wait() for p in procs]
        else:
            codes = []
            for i, k in enumerate(ks):
                codes.append(start(base / str(i), argv(k), home, cores[i]).wait())
        wall = time.perf_counter() - began
        return wall, [sha(base / str(i) / 'output.dat') if c == 0 and (base / str(i) / 'output.dat').exists() else None for i, c in enumerate(codes)]
    finally:
        shutil.rmtree(base)


def jobs(a):
    cores = cores_of(a)
    a.out.mkdir(parents=True)
    models = Path(tempfile.mkdtemp(prefix='w592-m-'))
    text = SWEEP_MODEL.read_text()
    assert 'par amp=1,k=1' in text
    for k in SWEEP_K:
        ode = models / ('k%s.ode' % k)
        ode.write_text(text.replace('par amp=1,k=1', 'par amp=1,k=%s' % k))
        convert(SimpleNamespace(xppautx=a.xppautx), ode)
    before_series(a)
    runs = []
    try:
        with short_home() as home:
            alone = {}
            for k in SWEEP_K:  # each job alone, twice: the reference and its repeat
                first, second = (group(a, models, cores, [k], 'sequential', home) for _ in range(2))
                if first[1] != second[1] or None in first[1]:
                    raise RuntimeError('job k=%s: output not reproducible alone' % k)
                alone[k] = first[1][0]
            for rep in range(a.repeats + 1):
                for n in JOBS:
                    ks = SWEEP_K[:n]
                    for mode in (('concurrent', 'sequential') if rep % 2 == 0 else ('sequential', 'concurrent')):
                        wall, outputs = group(a, models, cores, ks, mode, home)
                        runs.append(dict(n=n, mode=mode, repetition=rep, wall_seconds=wall, equal_to_alone=[o == alone[k] for o, k in zip(outputs, ks)]))
                        print('jobs', n, mode, 'rep', rep, round(wall, 3), runs[-1]['equal_to_alone'], flush=True)
                        save(a.out / 'runs.json', runs)
    finally:
        shutil.rmtree(models)
    summary = []
    for n in JOBS:
        row = dict(n=n, parameter='k', values=list(SWEEP_K[:n]))
        for mode in ('concurrent', 'sequential'):
            rs = [r for r in runs if r['n'] == n and r['mode'] == mode and r['repetition'] > 0]
            row[mode] = stats([r['wall_seconds'] for r in rs])
            row[mode + '_all_equal'] = all(all(r['equal_to_alone']) for r in runs if r['n'] == n and r['mode'] == mode)
        row['throughput_concurrent'] = n / row['concurrent']['mean']
        row['throughput_sequential'] = n / row['sequential']['mean']
        row['speedup'] = row['sequential']['mean'] / row['concurrent']['mean']
        summary.append(row)
    save(a.out / 'summary.json', dict(model=str(SWEEP_MODEL.relative_to(REPO)), parameter='k', rows=summary, alone_output_sha256={str(k): v for k, v in alone.items()}))
    provenance(a, a.out)


def auto(a):
    wait_until_ready()
    subprocess.run([sys.executable, str(GRID), '--program', 'xppautx', '--binary', str(a.xppautx), '--platform', 'ubuntu-final', '--out', str(a.out),
                    '--reps', str(a.repeats), '--ns', '2,4'], check=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('part', choices=('threads', 'jobs', 'auto'))
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--xppautx', type=Path, required=True)
    p.add_argument('--machine-record', type=Path)
    p.add_argument('--repeats', type=int, default=5)
    a = p.parse_args()
    a.out, a.xppautx = a.out.resolve(), a.xppautx.resolve()
    if a.part != 'auto' and not a.machine_record:
        p.error('--machine-record is needed')
    # native() and group() read only a.xppautx; a.check is for before_series
    a.check = False
    {'threads': threads, 'jobs': jobs, 'auto': auto}[a.part](a)


if __name__ == '__main__':
    main()
