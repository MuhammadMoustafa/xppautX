#!/usr/bin/env -S uv run --locked python
"""W354 stage 2, in settled order; W699: one workload is the unit, each on its own, one after another.

A unit (a study's own name for a workload: common.py's comment) has the folder <out>/<study>/<unit>/ with its attempts
(attempt-NNN, the driver's output for that workload alone) and, once an attempt finished, done.json. A run first plans every
unit (a driver's --plan: what the unit's earlier attempt, or --reuse's, supplies, by the rows' own keys) and stops if a plan
fails, so a bug in the reuse path shows in minutes; it then runs each unit whose plan says something would run, with the
earlier attempt as its --reuse, and goes on after a failure. Resuming a folder skips the units whose plan runs nothing.
"""
import argparse
import functools
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from common import BENCH, DONE, FOREIGN_ENTRY, FROM_SCRATCH, HERE, PLAN, REPO, SHA_CACHE, STUDIES, TOOLS, module, physical_cores, save, sha, wait_until_ready

LOG_TAIL = 40  # lines of a failed attempt's log kept in its failure.json


def machine_record(path, timed):
    if timed:
        # W448: --ready (the governor, and no other process on the CPU) before each timed unit. The session supervising
        # the series (its desktop app) is busy only while it acts, so wait for idle: a poll for a condition, bounded.
        wait_until_ready()
        p = subprocess.run(['bash', str(TOOLS / 'comparison-setup.sh'), '--record'], cwd=REPO, check=True, capture_output=True, text=True)
        value = json.loads(Path(next(l.removeprefix('record: ') for l in p.stdout.splitlines() if l.startswith('record: '))).read_text())
    else:
        # Untimed WSL checks and plans cannot satisfy the Ubuntu governor/idle policy.
        value = dict(check_only=True, lscpu_extended=subprocess.check_output(['lscpu', '-e'], text=True))
    save(path, value)
    return path


def attempts(unit_dir):
    return sorted(unit_dir.glob('attempt-[0-9][0-9][0-9]'))


def earlier_attempt(unit_dir, reuse_root, study):
    # The unit's newest attempt that began a run (its provenance.json), else the same unit of --reuse's folder: what a run imports from.
    for root in (unit_dir, reuse_root / study / unit_dir.name if reuse_root else None):
        found = [f for f in reversed(attempts(root)) if (f / 'provenance.json').exists()] if root else []
        if found:
            return found[0]
    return None


class Run:
    def __init__(self, a):
        self.a = a
        self.scratch = Path(tempfile.mkdtemp(prefix='w699-'))
        os.environ[SHA_CACHE] = str(self.scratch / 'digests.json')  # every driver of this run reads the same binaries and models
        self.untimed = machine_record(self.scratch / 'machine.json', False)

    @functools.cached_property
    def stiff(self):
        sys.path.insert(0, str(BENCH / 'large-stiff'))  # study.py imports its generator beside it
        return module('stiff_study', BENCH / 'large-stiff/study.py')

    def argv(self, study, out, machine, *flags):
        a = self.a
        script = HERE / ('terman.py' if study == 'terman' else 'continuation.py' if study == 'continuation' else 'server.py' if study == 'server' else 'integration.py')
        argv = [sys.executable, '-X', 'faulthandler', str(script), '--out', str(out), '--xppautx', str(a.xppautx.resolve()),
                '--xppaut', str(a.xppaut.resolve()), '--auto-dir', str(a.auto_dir.resolve()), '--machine-record', str(machine), *flags]
        if study not in ('terman', 'continuation', 'server'):
            argv += ['--study', study]
        if a.check:
            argv += ['--check']
        if a.repeats is not None and study != 'terman':
            argv += ['--repeats', str(a.repeats)]
        return argv

    def units(self, study):
        if study == 'large-stiff':
            names = self.stiff.CHECK_UNITS if self.a.check else self.stiff.UNITS  # a check is the stage-1 smoke gate alone (W815)
            return [u for u in names if not self.a.only or u in self.a.only.split(',')]
        out = self.scratch / ('list-' + study)
        p = subprocess.run(self.argv(study, out, self.untimed, '--list'), cwd=REPO, capture_output=True, text=True)
        if p.returncode:
            raise RuntimeError('%s: listing its workloads failed:\n%s' % (study, (p.stdout + p.stderr)[-2000:]))
        shutil.rmtree(out, ignore_errors=True)
        names = json.loads(p.stdout.splitlines()[-1])
        return [n for n in names if not self.a.only or n in self.a.only.split(',')]

    def needs(self, study, unit):
        # A unit's set-up, another unit that must be done first: (study, unit) with the flag it is given as, or None.
        if study in ('screen', 'python') and unit.split('-')[0] == 'episodic' and not self.a.check:  # the Terman network, whatever the method
            return 'terman', 'episodic', '--terman'
        if study == 'large-stiff' and self.stiff.pilot_of(unit):
            return 'large-stiff', self.stiff.pilot_of(unit), '--pilot'  # a measure or cache unit reads its size's pilot (W815)
        return None

    def setup_flag(self, study, unit):
        # The set-up unit's attempt as the flag's value, or why there is none (the set-up failed or has not run).
        need = self.needs(study, unit)
        if not need:
            return [], None
        done = self.a.out / need[0] / need[1] / DONE
        if not done.exists():
            return [], 'its set-up unit %s/%s is not done' % need[:2]
        attempt = done.parent / json.loads(done.read_text())['attempt']
        return [need[2], str(attempt)], None

    def stiff_identity(self, unit):
        # A large-stiff unit's identity: the worker, the kit and the pilot it reads (never the commit, W815).
        flag, _ = self.setup_flag('large-stiff', unit)
        return self.stiff.unit_identity(self.a.worker.resolve(), Path(flag[1]) if flag else None)

    def stiff_argv(self, unit, out, machine, extra):
        # study.py's mode, family and size for the unit (W815); a check is the stage-1 gate, without KLU.
        a = self.a
        mode, family, n = self.stiff.unit_args(unit)
        argv = [sys.executable, '-X', 'faulthandler', str(BENCH / 'large-stiff/study.py'), 'check' if a.check else mode,
                '--worker', str(a.worker.resolve()), '--out', str(out), '--family', family, '--sizes', str(n)]
        if a.check:
            return argv + ['--allow-no-klu']
        cpu = str(physical_cores(json.loads(Path(machine).read_text()))[0]) if mode in ('measure', 'cache') else None
        if mode == 'measure':
            argv += ['--cpu', cpu, '--machine-record', str(machine), '--repeats', str(a.repeats or self.stiff.REPEATS)]
        if mode == 'cache':
            argv += ['--cpu', cpu]
        return argv + extra

    def plan(self, study, unit):
        # The reasons the unit would run (empty: its done record matches), or None when there is nothing earlier to plan against.
        a = self.a
        unit_dir = a.out / study / unit
        source = earlier_attempt(unit_dir, a.reuse, study) if study != 'large-stiff' else None
        done = unit_dir / DONE
        if study == 'large-stiff':
            # study.py's identity (the worker, its sources, its pilot) is its key; there are no rows to plan. Its pilot not done
            # yet: nothing earlier to plan against. Read even for a unit not yet done: a worker older than its sources stops
            # the plan, not the series hours in (W812).
            if self.setup_flag(study, unit)[1]:
                return None
            identity = self.stiff_identity(unit)
            if not done.exists():
                return None
            same = json.loads(done.read_text())['identity'] == identity
            return [] if same else ['the worker, its sources or its pilot changed since the unit finished']
        if source is None:
            return None
        flag, why = self.setup_flag(study, unit)
        if why:
            return [why]
        out = self.scratch / ('plan-%s-%s' % (study, unit))
        p = subprocess.run(self.argv(study, out, self.untimed, '--only', unit, '--plan', '--reuse', str(source), *flag), cwd=REPO, capture_output=True, text=True)
        if p.returncode:
            raise RuntimeError('%s/%s: its plan against %s failed:\n%s' % (study, unit, source, (p.stdout + p.stderr)[-3000:]))
        plan = json.loads((out / PLAN).read_text())
        save(unit_dir / PLAN, dict(plan, against=str(source)))
        shutil.rmtree(out)
        return plan['would_run']

    def execute(self, study, unit):
        # One attempt of the unit, foreground. True when it finished.
        a = self.a
        unit_dir = a.out / study / unit
        (unit_dir / DONE).unlink(missing_ok=True)
        attempt = unit_dir / ('attempt-%03d' % (len(attempts(unit_dir)) + 1))
        attempt.parent.mkdir(parents=True, exist_ok=True)
        flag, why = self.setup_flag(study, unit)
        if why:
            save(attempt.with_suffix('.failure.json'), dict(unit=unit, error='not run: ' + why))
            return False
        machine = machine_record(attempt.with_suffix('.machine.json'), not a.check)
        if study == 'large-stiff':
            argv = self.stiff_argv(unit, attempt, machine, flag)
        else:
            source = earlier_attempt(unit_dir, a.reuse, study)
            argv = self.argv(study, attempt, machine, '--only', unit, *flag, *(['--reuse', str(source)] if source else []))
            if a.stop_on_failure:
                argv += ['--stop-on-failure']
        # Foreground, serial, with the log kept even if the unit raises.
        log = attempt.with_suffix('.log')
        with log.open('w') as stream:
            code = subprocess.run(argv, cwd=REPO, stdout=stream, stderr=subprocess.STDOUT).returncode
        if code:
            tail = log.read_text(errors='replace').splitlines()[-LOG_TAIL:]
            save(attempt.with_suffix('.failure.json'), dict(unit=unit, exit_code=code, command=argv, log_tail=tail))
            return False
        save(unit_dir / DONE, self.done_record(study, unit, attempt, code))
        return True

    def done_record(self, study, unit, attempt, code):
        # The result and what it ran with: the binaries, the commit and the keys the rows recorded (W452, W575).
        a = self.a
        keys = []
        if (attempt / 'runs.json').exists():
            for r in json.loads((attempt / 'runs.json').read_text()):
                if r.get('reuse_key') and r['reuse_key'] not in keys:
                    keys.append(r['reuse_key'])
        record = dict(attempt=attempt.name, exit_code=code, check=a.check, keys=keys,
                      commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
                      binaries={str(b.resolve()): sha(b) for b in (a.xppautx, a.xppaut)})
        if study == 'large-stiff':
            record['identity'] = self.stiff_identity(unit)
        return record


def rows_to_run(todo):
    # (study, unit, entry) of every row of another program than xppautX that a plan would run, or a unit with no earlier attempt at all.
    blocked = []
    for (study, unit), would_run in todo.items():
        if would_run is None and study not in FROM_SCRATCH:
            blocked.append((study, unit, 'no earlier attempt: every row of every program would run'))
        blocked += [(study, unit, entry) for entry in would_run or [] if FOREIGN_ENTRY.match(entry)]
    return blocked


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--xppautx', type=Path, required=True)
    p.add_argument('--xppaut', type=Path, required=True)
    p.add_argument('--auto-dir', type=Path, required=True)
    p.add_argument('--worker', type=Path, default=Path('build/w378/worker'))
    p.add_argument('--study', choices=STUDIES, action='append', help='run only these studies, in settled order')
    p.add_argument('--stop-on-failure', action='store_true', help='stop at the first failed unit')
    p.add_argument('--reuse', type=Path, help='an earlier results folder (it may be unfinished): a unit of it that has no attempt here supplies its rows whose program, measurement code and inputs are unchanged, so they are imported, not run again')
    p.add_argument('--plan', action='store_true', help='only plan: for every unit, what its earlier attempt supplies and what would run; no timing, nothing run (W802: a plan that would run a row of XPPAUT, AUTO-07p or Python also stops, listing it)')
    p.add_argument('--check', action='store_true')
    p.add_argument('--repeats', type=int, help='counted repetitions per row (W460); the drivers default to five')
    p.add_argument('--only', help='select units by name (short checks: names within a study)')
    a = p.parse_args()
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=True)
    run = Run(a)
    failed = []
    try:
        studies = [s for s in STUDIES if not a.study or s in a.study]
        todo = {}
        for study in studies:
            for unit in run.units(study):
                todo[study, unit] = run.plan(study, unit)
        for (study, unit), would_run in todo.items():
            print('plan %s/%s: %s' % (study, unit, 'nothing earlier' if would_run is None else '%d would run' % len(would_run) if would_run else 'nothing would run'), flush=True)
        blocked = [] if a.check else rows_to_run(todo)  # a check times nothing: its smoke rows are the point
        if blocked:
            sys.exit('Stopped before anything was run: these rows of XPPAUT, AUTO-07p and Python cannot be reused, and none is ever run again '
                     '(W802); fix the reader or the key, or the maintainer decides to run them:\n' + '\n'.join('  %s/%s: %s' % row for row in blocked))
        if a.plan:
            return
        for (study, unit), would_run in todo.items():
            if would_run == [] and (a.out / study / unit / DONE).exists():
                print('RESUME done', study, unit, flush=True)
                continue
            ok = run.execute(study, unit)
            print(study, unit, 'done' if ok else 'FAILED', flush=True)
            if not ok:
                failed.append('%s/%s' % (study, unit))
                if a.stop_on_failure:
                    break
    finally:
        os.environ.pop(SHA_CACHE, None)
        shutil.rmtree(run.scratch, ignore_errors=True)
    if failed:
        sys.exit('Failed units (results retained, the rest were run; rerun the same command to resume): ' + ', '.join(failed))


if __name__ == '__main__':
    main()
