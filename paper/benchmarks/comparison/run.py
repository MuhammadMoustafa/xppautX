#!/usr/bin/env -S uv run --locked python
"""W354 stage 2, in settled order; W699: one workload is the unit, each on its own, one after another.

A unit (a study's own name for a workload: common.py's comment) has the folder <out>/<study>/<unit>/ with its attempts
(attempt-NNN, the driver's output for that workload alone) and, once an attempt finished, done.json. A run first plans every
unit (a driver's --plan: what the unit's earlier attempt, or --reuse's, supplies, by the rows' own keys) and stops if a plan
fails, so a bug in the reuse path shows in minutes; it then runs each unit whose plan says something would run, with the
earlier attempt as its --reuse, and goes on after a failure. Resuming a folder skips the units whose plan runs nothing.

W818: each unit with a reference (the --reuse folder's counted xppautX rows) gets a verdict, faster, same or slower, against it,
written to run.log and the unit's done record. Pass 1 runs one repetition of those units (its pass1/ folder, never counted); pass 2
runs the rest of every pending unit, and only when pass 1 has no slower unit. A slower unit ends the series (--keep-going goes on
past it, to the end of pass 1); the next run of the folder starts again from its first unit.

W826: pass 1 runs each unit's set-up units (needs) before the unit, and they take part in pass 1 as well: a set-up unit with no
timed rows to judge has no verdict of its own, so it stops nothing. The set-up's pass-1 folder is what the unit reads in pass 1.

W829 (#910): while a timed unit runs, the CPU other processes used is measured from /proc (common.competing_record) and kept in
the attempt's `.competing.json`. A unit whose competing CPU passes DISTURBED_CPU_FRACTION is disturbed: no verdict, no done or
pass-1 record, run again once --ready passes; disturbed again, the series stops with the reason (DISTURBED_RUNS).
W845 (#926): so is a unit with a stalled row (common.stalled_rows: a one-CPU xppautX row waiting off the CPU for more than STALL_SECONDS),
which a few ms of competing CPU at a time leave under the unit-wide fraction.

W830: a unit's verdict also compares each xppautX row with the same row of every other program in its attempt (XPPAUT, AUTO-07p,
Python: W617's own times, never rerun). A row behind another program is reported (run.log, done.json's `against` and `behind`,
and the end-of-pass summary), never stopped on: a gap that grew since W617 is a slower verdict, which does stop the series.
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
import time

from common import BENCH, DONE, FOREIGN_ENTRY, FROM_SCRATCH, HERE, PASS1, PLAN, REPO, SHA_CACHE, STOPPED, STALL_SECONDS, STUDIES, TOOLS, competing_record, done_attempts, judge, machine_snapshot, module, physical_cores, save, sha, stalled_rows, timed_rows, wait_until_ready

LOG_TAIL = 40  # lines of a failed attempt's log kept in its failure.json
DISTURBED_RUNS = 2  # W829: a disturbed run is run again once --ready passes; this many disturbed runs in a row stop the series (never a loop)


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


def ended(unit_dir, attempt):
    # An attempt a run may import rows from: it began (provenance.json) and ended with a result the driver kept, done.json naming
    # it or its failure record beside it. An interrupted attempt has neither (the W718 run stopped at XPPAUT repetition 4): its rows
    # are partial, so they are never imported (W827).
    done = unit_dir / DONE
    named = done.exists() and json.loads(done.read_text())['attempt'] == attempt.name
    return (attempt / 'provenance.json').exists() and (named or attempt.with_suffix('.failure.json').exists())


def earlier_attempt(unit_dir, reuse_root, study):
    # The unit's newest attempt that ended (see ended), else the same unit of --reuse's folder: what a run imports from.
    for root in (unit_dir, reuse_root / study / unit_dir.name if reuse_root else None):
        found = [f for f in reversed(attempts(root)) if ended(root, f)] if root else []
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

    def argv(self, study, out, machine, *flags, repeats=None):
        a = self.a
        script = HERE / ('terman.py' if study == 'terman' else 'continuation.py' if study == 'continuation' else 'server.py' if study == 'server' else 'integration.py')
        argv = [sys.executable, '-X', 'faulthandler', str(script), '--out', str(out), '--xppautx', str(a.xppautx.resolve()),
                '--xppaut', str(a.xppaut.resolve()), '--auto-dir', str(a.auto_dir.resolve()), '--machine-record', str(machine), *flags]
        if study not in ('terman', 'continuation', 'server'):
            argv += ['--study', study]
        if a.check:
            argv += ['--check']
        repeats = a.repeats if repeats is None else repeats
        if repeats is not None and study != 'terman':
            argv += ['--repeats', str(repeats)]
        return argv

    def binaries(self):
        return {str(b.resolve()): sha(b) for b in (self.a.xppautx, self.a.xppaut)}

    def reference(self, study, unit):
        # The unit's attempt in the --reuse run, when it has counted xppautX rows to judge against (W818), else None.
        if self.a.check or not self.a.reuse:
            return None
        found = done_attempts(self.a.reuse, study).get(unit)
        return found if timed_rows(found) else None

    def pass1_current(self, study, unit):
        # The unit's pass-1 verdict, when it was judged for the binaries this run uses and its pass-1 folder is there (else it runs again).
        record = self.a.out / study / unit / PASS1
        if not record.exists() or not (self.a.out / study / unit / 'pass1').is_dir():
            return None
        value = json.loads(record.read_text())
        # W830: a record from before the comparison with the other programs has no `against`, so the unit runs again
        return value if value['binaries'] == self.binaries() and 'against' in value else None

    def pass1_units(self, pending):
        # W826: the units pass 1 runs, in order: each unit with a reference to judge against, preceded by its set-up unit when that
        # one is pending (a unit reads the set-up's attempt, so the set-up runs first; it is judged like any unit, or not at all).
        order = []
        for study, unit in pending:
            if not self.reference(study, unit):
                continue
            need = self.needs(study, unit)
            if need and (need[0], need[1]) in pending and (need[0], need[1]) not in order:
                order.append((need[0], need[1]))
            if (study, unit) not in order:
                order.append((study, unit))
        return order

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

    def setup_flag(self, study, unit, mode='run'):
        # The set-up unit's attempt as the flag's value, or why there is none. The set-up's done attempt when it has one. Otherwise,
        # by mode: 'quick' (pass 1, W826) the set-up's pass-1 folder, which pass 1 ran just before the unit; 'plan' (--plan) its
        # earlier attempt in --reuse, so the unit's rows are planned against the same set-up the run will use until it is done
        # (a set-up that is not done is no reason to plan nothing, W827); 'run' the failure of a set-up that has not run.
        need = self.needs(study, unit)
        if not need:
            return [], None
        set_study, set_unit, flag = need
        folder = self.a.out / set_study / set_unit
        if (folder / DONE).exists():
            return [flag, str(folder / json.loads((folder / DONE).read_text())['attempt'])], None
        if mode == 'quick' and (folder / 'pass1').is_dir():
            return [flag, str(folder / 'pass1')], None
        if mode == 'plan' and self.a.reuse:
            earlier = done_attempts(self.a.reuse, set_study).get(set_unit)
            if earlier:
                return [flag, str(earlier)], None
        return [], 'its set-up unit %s/%s is not done' % (set_study, set_unit)

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
        flag, why = self.setup_flag(study, unit, 'plan')
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

    def execute(self, study, unit, quick=False):
        # One unit's run, foreground: pass 1's quick check (one counted repetition, in unit/pass1: never a result, never counted) or
        # the next attempt, which is the unit's result. Returns (status, verdict, competing): 'done' with its verdict, 'failed', or
        # 'disturbed' (W829) when other processes used the CPU while it ran, DISTURBED_RUNS times in a row (the last record is kept).
        a = self.a
        unit_dir = a.out / study / unit
        source = earlier_attempt(unit_dir, a.reuse, study)  # before done.json goes: a rerun imports from the attempt it replaces (W827)
        if not quick:
            (unit_dir / DONE).unlink(missing_ok=True)
        for tries in range(1, DISTURBED_RUNS + 1):
            status, verdict, competing = self.run_once(study, unit, source, quick)
            if status != 'disturbed' or tries == DISTURBED_RUNS:
                return status, verdict, competing
            note(a, disturbed_line('pass 1' if quick else 'pass 2', study, unit, competing) + ': runs again once --ready passes')

    def run_once(self, study, unit, source, quick):
        # One run of the unit in its own attempt folder (pass 1's, for quick). Returns (status, verdict, competing), see execute.
        a = self.a
        unit_dir = a.out / study / unit
        if quick:
            attempt = unit_dir / 'pass1'
            shutil.rmtree(attempt, ignore_errors=True)
        else:
            attempt = unit_dir / ('attempt-%03d' % (len(attempts(unit_dir)) + 1))
        attempt.parent.mkdir(parents=True, exist_ok=True)
        flag, why = self.setup_flag(study, unit, 'quick' if quick else 'run')
        if why:
            save(attempt.with_suffix('.failure.json'), dict(unit=unit, error='not run: ' + why))
            return 'failed', None, None
        machine = machine_record(attempt.with_suffix('.machine.json'), not a.check)
        if study == 'large-stiff':
            argv = self.stiff_argv(unit, attempt, machine, flag)
        else:
            argv = self.argv(study, attempt, machine, '--only', unit, *flag, *(['--reuse', str(source)] if source else []),
                             repeats=1 if quick else None)
            if a.stop_on_failure:
                argv += ['--stop-on-failure']
        # Foreground, serial, with the log kept even if the unit raises.
        log = attempt.with_suffix('.log')
        code, competing = run_unit(argv, log, not a.check)
        if competing is not None:
            save(attempt.with_suffix('.competing.json'), competing)
        if code:
            tail = log.read_text(errors='replace').splitlines()[-LOG_TAIL:]
            save(attempt.with_suffix('.failure.json'), dict(unit=unit, exit_code=code, command=argv, log_tail=tail))
            return 'failed', None, competing
        if competing is not None:
            stalled = stalled_rows(json.loads((attempt / 'runs.json').read_text())) if (attempt / 'runs.json').exists() else []
            if stalled:  # W845: a stall inside the unit that W829's unit-wide fraction did not see disturbs it too
                competing = dict(competing, verdict='disturbed', stalled=[dict(workload=w, engine=e, repetition=n, gap_seconds=g) for w, e, n, g in stalled])
                save(attempt.with_suffix('.competing.json'), competing)
        if competing is not None and competing['verdict'] == 'disturbed':
            return 'disturbed', None, competing  # W829: not judged, no record that a later run would read
        verdict = judge(attempt, self.reference(study, unit))  # W818: each unit against the same unit of the reference run
        if quick:
            save(unit_dir / PASS1, dict(verdict, binaries=self.binaries(), competing=competing))
        else:
            save(unit_dir / DONE, self.done_record(study, unit, attempt, code, verdict, competing))
        return 'done', verdict, competing

    def done_record(self, study, unit, attempt, code, verdict, competing):
        # The result and what it ran with: the binaries, the commit and the keys the rows recorded (W452, W575), the verdict (W818) and
        # the CPU other processes used while it ran (W829).
        a = self.a
        keys = []
        if (attempt / 'runs.json').exists():
            for r in json.loads((attempt / 'runs.json').read_text()):
                if r.get('reuse_key') and r['reuse_key'] not in keys:
                    keys.append(r['reuse_key'])
        record = dict(attempt=attempt.name, exit_code=code, check=a.check, keys=keys, verdict=verdict['verdict'], rows=verdict['rows'],
                      against=verdict['against'], behind=verdict['behind'],
                      commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
                      binaries=self.binaries(), competing=competing)
        if study == 'large-stiff':
            record['identity'] = self.stiff_identity(unit)
        return record


def run_unit(argv, log, timed):
    # The unit's driver, foreground, its log kept. A timed unit also measures the CPU other processes used while it ran (W829):
    # two process snapshots around the run. Returns (exit code, competing record or None).
    before = machine_snapshot() if timed else None
    start = time.monotonic()
    with log.open('w') as stream:
        code = subprocess.run(argv, cwd=REPO, stdout=stream, stderr=subprocess.STDOUT).returncode
    wall = time.monotonic() - start
    return code, (competing_record(before, machine_snapshot(), wall) if timed else None)


def disturbed_line(label, study, unit, competing):
    # A disturbed unit's line: the competing CPU it met, the bound, and the processes that used it (W829).
    named = ', '.join('%s %.1f s' % (p['name'], p['cpu_seconds']) for p in competing['processes']) or 'none named'
    line = ('%s %s/%s: disturbed: other processes used %.2f CPUs over %.1f s (bound %.2f CPUs): %s'
            % (label, study, unit, competing['competing_fraction'], competing['wall_seconds'], competing['bound'], named))
    stalled = competing.get('stalled')
    return line + ('; %d row(s) stalled, the first %s repetition %s waited %.2f s off the CPU (bound %.2f s)'
                   % (len(stalled), stalled[0]['engine'], stalled[0]['repetition'], stalled[0]['gap_seconds'], STALL_SECONDS) if stalled else '')


def rows_to_run(todo):
    # (study, unit, entry) of every row of another program than xppautX that a plan would run, or a unit with no earlier attempt at all.
    blocked = []
    for (study, unit), would_run in todo.items():
        if would_run is None and study not in FROM_SCRATCH:
            blocked.append((study, unit, 'no earlier attempt: every row of every program would run'))
        blocked += [(study, unit, entry) for entry in would_run or [] if FOREIGN_ENTRY.match(entry)]
    return blocked


def note(a, text):
    # A line of the terminal and of the series' run.log (W818: the verdicts and the stop).
    print(text, flush=True)
    with (a.out / 'run.log').open('a') as stream:
        stream.write(text + '\n')


def verdict_lines(label, study, unit, verdict):
    # The unit's verdict and, per xppautX row, its mean, the reference's, the ratio and the threshold it was judged by (W818).
    lines = ['%s %s/%s: %s' % (label, study, unit, verdict['verdict'])]
    for r in verdict['rows']:
        lines.append('    %s %s %s: %.4g s, reference %.4g s, ratio %.3f, threshold +/-%.0f%%: %s'
                     % (r['engine'], r['workload'], r['row'], r['mean'], r['reference_mean'], r['ratio'], 100 * r['spread'], r['verdict']))
    against = verdict['against']
    if against:
        lines.append('    against the other programs: %d behind, %d level, %d ahead (reported, not a stop)'
                     % tuple(sum(1 for r in against if r['verdict'] == v) for v in ('behind', 'level', 'ahead')))
    for r in against:
        if r['verdict'] == 'behind':
            lines.append('    behind %s %s %s: xppautX %s %.4g s, %s %.4g s, ratio %.3f, threshold +/-%.0f%%'
                         % (r['program'], r['workload'], r['row'], r['engine'], r['mean'], r['other_engine'], r['other_mean'],
                            r['ratio'], 100 * r['spread']))
    return lines


def restart_after_stop(a):
    # W818 (the maintainer, 2026-11-18): a series that stopped at a slower unit starts again from its first unit once the fix is in.
    # The units done before the stop are not kept: their done and pass-1 records go (their attempts stay as history).
    marker = a.out / STOPPED
    if not marker.exists():
        return
    for record in list(a.out.glob('*/*/' + DONE)) + list(a.out.glob('*/*/' + PASS1)):
        record.unlink()
    marker.unlink()
    note(a, 'restart: the series stopped at a slower unit; every unit runs again from its first')


def run_pass(run, units, quick, slower, behind, failed, disturbed):
    # One pass over (study, unit) in order, each unit once, its verdict noted as it comes. A slower unit ends the pass unless
    # --keep-going; a failed unit goes on to the next unless --stop-on-failure; a disturbed unit (W829, disturbed again after its
    # rerun) always ends the pass. A unit behind another program (W830) is collected in behind and stops nothing. quick: pass 1,
    # one repetition, reused when judged.
    a = run.a
    label = 'pass 1' if quick else 'pass 2'
    for study, unit in units:
        kept = run.pass1_current(study, unit) if quick else None
        if kept is not None:
            verdict = kept
            note(a, 'pass 1 kept: %s/%s was judged for these binaries' % (study, unit))
        else:
            status, verdict, competing = run.execute(study, unit, quick=quick)
            if status == 'failed':
                failed.append('%s/%s' % (study, unit))
                note(a, '%s %s/%s FAILED' % (label, study, unit))
                if a.stop_on_failure:
                    break
                continue
            if status == 'disturbed':
                disturbed.append('%s/%s' % (study, unit))
                note(a, disturbed_line(label, study, unit, competing) + ': disturbed again, the series stops here')
                break
        for line in verdict_lines(label, study, unit, verdict):
            note(a, line)
        if verdict['behind'] and '%s/%s' % (study, unit) not in behind:  # pass 1 and pass 2 may both judge a unit
            behind.append('%s/%s' % (study, unit))
        if verdict['verdict'] == 'slower':
            slower.append('%s/%s' % (study, unit))
            if not a.keep_going:
                break


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--xppautx', type=Path, required=True)
    p.add_argument('--xppaut', type=Path, required=True)
    p.add_argument('--auto-dir', type=Path, required=True)
    p.add_argument('--worker', type=Path, default=Path('build/w378/worker'))
    p.add_argument('--study', choices=STUDIES, action='append', help='run only these studies, in settled order')
    p.add_argument('--stop-on-failure', action='store_true', help='stop at the first failed unit')
    p.add_argument('--keep-going', action='store_true', help='go on past a unit slower than its reference (W818; a slower unit still ends pass 1, so pass 2 does not run)')
    p.add_argument('--reuse', type=Path, help='an earlier results folder (it may be unfinished: a failed attempt supplies its rows, an interrupted one none, W827): a unit of it that has no attempt here supplies its rows whose program, measurement code and inputs are unchanged, so they are imported, not run again')
    p.add_argument('--plan', action='store_true', help='only plan: for every unit, what its earlier attempt supplies and what would run; no timing, nothing run (W802: a plan that would run a row of XPPAUT, AUTO-07p or Python also stops, listing it)')
    p.add_argument('--check', action='store_true')
    p.add_argument('--repeats', type=int, help='counted repetitions per row (W460); the drivers default to five')
    p.add_argument('--only', help='select units by name (short checks: names within a study)')
    a = p.parse_args()
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=True)
    run = Run(a)
    failed, slower, behind, disturbed = [], [], [], []
    try:
        if not a.plan:
            restart_after_stop(a)  # before the plan: a restarted series plans against no done record
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
        pending = []
        for (study, unit), would_run in todo.items():
            if would_run == [] and (a.out / study / unit / DONE).exists():
                print('RESUME done', study, unit, flush=True)
            else:
                pending.append((study, unit))
        # W818: pass 1, one repetition of each unit that has a reference to judge against; pass 2, every pending unit's rest,
        # only when pass 1 has no slower unit. Pass 1's runs are a check: the report never counts them.
        run_pass(run, run.pass1_units(pending), True, slower, behind, failed, disturbed)
        if not slower and not disturbed:
            run_pass(run, pending, False, slower, behind, failed, disturbed)
        if slower:
            save(a.out / STOPPED, dict(slower=slower, binaries=run.binaries()))
    finally:
        os.environ.pop(SHA_CACHE, None)
        shutil.rmtree(run.scratch, ignore_errors=True)
    # W829: the timed runs this folder holds that other processes disturbed, each one's own record (the report reads done.json only).
    count = sum(1 for f in a.out.glob('*/*/*.competing.json') if json.loads(f.read_text())['verdict'] == 'disturbed')
    if count:
        note(a, 'disturbed: %d timed run(s) in this folder met other processes on the CPU (their .competing.json says disturbed)' % count)
    if behind:
        note(a, 'behind another program (W830, reported, not a stop; each needs its card): ' + ', '.join(behind))
    if slower:
        sys.exit('Slower than the reference (ratios in run.log): ' + ', '.join(slower) + '. Fix it, then run the same command again: '
                 'the series starts from its first unit.')
    if disturbed:
        sys.exit('Stopped: other processes used the CPU during a unit twice (numbers in run.log): ' + ', '.join(disturbed) + '. Leave the '
                 'machine idle, then run the same command again: the units done stay.')
    if failed:
        sys.exit('Failed units (results retained, the rest were run; rerun the same command to resume): ' + ', '.join(failed))


if __name__ == '__main__':
    main()
