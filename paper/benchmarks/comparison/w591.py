#!/usr/bin/env -S uv run --locked python
"""W591 (#657): peak resident memory of each program on the comparison's workloads, untimed.

    w591.py integration --final F --out O   # every integration row of the frozen series: its run and its one-step (ready) row
    ... --programs xppautx   # only these programs' rows (W594: xppautX alone on a newer binary; F's status.json names each study's attempt)
    w591.py continuation --xppautx X --xppaut P --auto-dir A --out O   # each timed continuation workload, the three programs

The integration rows are the frozen series' own: each row's folder keeps the inputs and command.json the timed run used, so the same
command is run again here (the pinning prefix dropped, since rows run side by side) under `/usr/bin/time -f %M`, whose figure is
the child's ru_maxrss (a forked Python's own is not: `time` forks a small process, the program execs from it). One run per row
and a second to show it is stable; a row whose timed run took over LONG_SECONDS runs once. Results: O/integration.json.

The continuation workloads are the frozen series' timed ones (completion.json): each description (window.json, the timing
repetitions removed) runs once per program through the series' own functions, xppautX by comparison.compute (one thread,
difference Jacobian: the row xppautx-fd-1-none; its peak is the server's VmHWM, the kernel's own peak-resident counter, read after
each run), XPPAUT 8.0 by continuation.foreign (VmHWM read before the driver kills it), AUTO-07p by continuation.foreign with
its `auto` command under /usr/bin/time (the wrapper, its compile and its executable: the largest). The start-up footprint is the
same three on E3-50 stopped after START_POINTS points. Results: O/continuation.json.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from common import REPO, environment, save, short_home
from comparison_measure import run_foreground
import auto_reference as reference
import autoxcompare as comparison
import continuation as series
from odex_model import Odex

STUDIES = ('screen', 'modeldb', 'research', 'izhikevich', 'python', 'scaling')
OUTPUTS = ('output.dat', 'output.dat.gz', 'run.log', 'process.json', 'result.json', 'command.json')  # what a row's run wrote
LONG_SECONDS = 60  # a row longer than this runs once: a second run of a half-hour Python integration shows nothing more
WORKERS = 4  # untimed runs, side by side: memory does not depend on the load
TIMEOUT = 4 * 3600  # hang guard only
START_POINTS = 5  # the start-up footprint's run: E3-50 stopped after this many points


def peak_kib(argv, folder):
    """Run argv in folder under /usr/bin/time; (exit code, peak resident KiB of the program)."""
    log = folder / 'time.txt'
    with short_home() as home, (folder / 'run.log').open('w') as out:
        code = subprocess.run(['/usr/bin/time', '-f', '%M', '-o', str(log), *argv], cwd=folder, env=environment(home), stdout=out,
                              stderr=subprocess.STDOUT, timeout=TIMEOUT).returncode
    return code, int(log.read_text().split()[-1])


def replay(row, scratch):
    source = Path(row['folder'])
    argv = json.loads((source / 'command.json').read_text())
    if argv[0] == 'taskset':
        argv = argv[3:]
    folder = Path(tempfile.mkdtemp(prefix='w591-', dir=scratch))
    try:
        for f in source.iterdir():
            if f.name not in OUTPUTS and f.is_file():
                shutil.copy2(f, folder)
        code, kib = peak_kib(argv, folder)
        return dict(exit_code=code, peak_kib=kib, output=(folder / 'output.dat').exists())
    finally:
        shutil.rmtree(folder)


def attempt(final, study):
    """The folder of the study's attempt that status.json names (a composed folder maps each study to its real attempt)."""
    return Path(json.loads((final / 'status.json').read_text())['studies'][study]['out'])


def rows(final, programs):
    found = []
    for study in STUDIES:
        base = attempt(final, study)
        timed = {(r['workload'], r['engine']): r.get('wall_seconds_mean') for r in json.loads((base / 'summary.json').read_text()) if r['row'] == 'run' and not r.get('sweep')}
        for workload in sorted((base / 'runs').iterdir()):
            rep = workload / '1'
            if not rep.is_dir():
                continue  # the python study's diverged runs have only a warm-up: no timed row (their data is extra-runs/python)
            for engine in sorted(p for p in rep.iterdir() if (p / 'run').is_dir() and p.name in programs):
                for kind in ('ready', 'run'):
                    if (engine / kind / 'command.json').exists():
                        found.append(dict(study=study, workload=workload.name, program=engine.name, row=kind, folder=str(engine / kind),
                                          timed_wall=timed.get((workload.name, engine.name))))
    return found


def integration(a):
    scratch = Path(tempfile.mkdtemp(prefix='w591-scratch-'))
    todo = rows(a.final, a.programs)
    todo.sort(key=lambda r: -(r['timed_wall'] or 0))  # the long ones first, so they overlap the short ones

    def one(row):
        runs = [replay(row, scratch)]
        if row['row'] == 'ready' or (row['timed_wall'] or 0) <= LONG_SECONDS:
            runs.append(replay(row, scratch))
        print(row['study'], row['workload'], row['program'], row['row'], [r['peak_kib'] for r in runs], flush=True)
        return dict(row, runs=runs)
    try:
        with ThreadPoolExecutor(WORKERS) as pool:
            results = list(pool.map(one, todo))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    save(a.out / 'integration.json', results)


OLD_ROOT, NEW_ROOT = '/home/muhammad/xppautx-comparison/w583-final-2/', str(Path.home() / 'xppautx-comparison/final') + '/'
PEAKS = {}


def hwm_kib(pid):
    for line in (Path('/proc') / str(pid) / 'status').read_text().splitlines():
        if line.startswith('VmHWM:'):
            return int(line.split()[1])


class XppautProcess(subprocess.Popen):
    """XPPAUT's driver ends it with kill(): read the process's peak first."""
    def kill(self):
        if str(self.args[0]).endswith('xppaut') and self.poll() is None:
            PEAKS['xppaut'] = max(PEAKS.get('xppaut', 0), hwm_kib(self.pid))
        super().kill()


class Subprocess:
    """auto_reference's `subprocess`, with XPPAUT's process watched; everything else is the real module."""
    Popen = XppautProcess

    def __getattr__(self, name):
        return getattr(subprocess, name)


def auto_under_time(argv, **kwargs):
    log = Path(tempfile.mkdtemp(prefix='w591-time-')) / 'time.txt'
    try:
        p = run_foreground(['/usr/bin/time', '-f', '%M', '-o', str(log), *argv], **kwargs)
        PEAKS['auto07p'] = max(PEAKS.get('auto07p', 0), int(log.read_text().split()[-1]))
        return p
    finally:
        shutil.rmtree(log.parent)


def xppautx_peak(a, d):
    """(peak KiB of the server after the last run, KiB when AUTO is open before any) for one description."""
    seen = {}

    class Observer:
        def created(self, server):
            self.server = server

        def ready(self, server):
            seen['open'] = hwm_kib(server.proc.pid)

        def finished(self, server, run, emitted, ranges):
            seen['peak'] = hwm_kib(server.proc.pid)
    with short_home() as home:
        comparison.compute(str(a.xppautx), copy.deepcopy(d), 1, env=environment(home), run=tempfile.mkdtemp(prefix='w591-x-'),
                           diagram_data=False, jacobian='difference', observer=Observer())
    return seen['peak'], seen['open']


def foreign_peak(a, d, program, inputs, scratch):
    PEAKS.pop(program, None)
    folder = scratch / ('f%d' % len(list(scratch.iterdir())))
    series.foreign(a, series.given(copy.deepcopy(d), program), program, inputs, folder, False)
    return PEAKS[program]


def workloads(final):
    base = attempt(final, 'continuation')
    timed = {}
    for r in json.loads((base / 'summary.json').read_text()):
        if r['row'] == 'run' and r['wall_seconds_mean']:
            key = (r['workload'].split('/')[0], r['workload'].split('/')[1], {'xppautx-fd-1-none': 'xppautx', 'xppaut-fd': 'xppaut', 'auto07p-fd-1': 'auto07p'}.get(r['engine']))
            timed[key] = timed.get(key, 0) + r['wall_seconds_mean']
    for done in sorted(base.glob('*/*/completion.json')):
        if json.loads(done.read_text())['timed']:
            folder = done.parent
            d = json.loads((folder / 'window.json').read_text().replace(OLD_ROOT, NEW_ROOT))['description']
            if not Path(d['model']).is_absolute():
                d['model'] = str(REPO / d['model'])  # the tests/auto descriptions name their model from the repository
            d['runs'] = [r for r in d['runs'] if '-repeat-' not in r['name']]
            d['differences'] = [v for v in d.get('differences', []) if '-repeat-' not in v['run']]
            yield folder, d, timed


def continuation(a):
    reference.subprocess = Subprocess()
    reference.run_foreground = auto_under_time
    scratch = Path(tempfile.mkdtemp(prefix='w591-c-'))
    results, startup = [], {}
    earlier = a.out / 'continuation.json'
    if earlier.exists():  # a stopped run goes on from its saved rows
        results, startup = json.loads(earlier.read_text())['rows'], json.loads(earlier.read_text())['startup']
    a.auto_dir, a.xppaut = (x and x.resolve() for x in (a.auto_dir, a.xppaut))  # xppautX alone needs neither
    chosen = [p for p in ('xppautx', 'xppaut', 'auto07p') if p in a.programs]  # the continuation study has no Python row

    def inputs(folder, d):
        odex = Odex(Path(d['model']))
        return odex, reference.Equations(odex, d.get('parameters', {})), series.start_values((folder / 'pilot-xppautx/diagram.csv').read_bytes()), False
    try:
        found = list(workloads(a.final))
        one = next(f for f in found if f[0].parent.name == 'E3-50')
        d = copy.deepcopy(one[1])
        for r in d['runs']:
            r['autox']['max_steps'] = START_POINTS  # XPPAUT 8.0 never ends a run with NMX 1; five points compute next to nothing
        for program in () if startup else chosen:
            startup[program] = [xppautx_peak(a, d)[0] if program == 'xppautx' else foreign_peak(a, d, program, inputs(one[0], d), scratch) for _ in range(2)]
            print('startup', program, startup[program], flush=True)
        for folder, d, timed in found:
            name, mode = folder.parent.name, folder.name
            for program in chosen:
                row = dict(workload=name, mode=mode, program=program)
                if any(r['workload'] == name and r['mode'] == mode and r['program'] == program for r in results):
                    continue
                why = None if program == 'xppautx' else series.refused(name, d, program, True)
                if why:
                    results.append(dict(row, reason=why))
                    continue
                count = 1 if timed.get((name, mode, program), 0) > LONG_SECONDS else 2
                run = ((lambda: xppautx_peak(a, d)[0]) if program == 'xppautx' else (lambda: foreign_peak(a, d, program, inputs(folder, d), scratch)))
                row['runs'] = [run() for _ in range(count)]
                results.append(row)
                print(name, mode, program, row['runs'], flush=True)
                save(a.out / 'continuation.json', dict(startup=startup, rows=results))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    save(a.out / 'continuation.json', dict(startup=startup, rows=results))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('part', choices=('integration', 'continuation'))
    p.add_argument('--final', type=Path, default=Path.home() / 'xppautx-comparison/final')
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--programs', nargs='+', choices=('xppautx', 'xppaut', 'auto07p', 'python'), default=('xppautx', 'xppaut', 'auto07p', 'python'))
    p.add_argument('--xppautx', type=Path)
    p.add_argument('--xppaut', type=Path)
    p.add_argument('--auto-dir', type=Path)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    {'integration': integration, 'continuation': continuation}[a.part](a)


if __name__ == '__main__':
    main()
