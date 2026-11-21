#!/usr/bin/env -S uv run --locked python
"""W699: the comparison's units run on their own. A fake driver (the drivers' unit protocol: --list, --plan, --only, --reuse) stands in
for integration.py: one unit fails and the others finish, a resume reruns only that unit, a change to one unit's key reruns only it,
and a plan that fails stops the run before any unit runs. No program, no timing."""
import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

COMPARISON = Path(__file__).resolve().parents[1] / 'paper/benchmarks/comparison'
sys.path.insert(0, str(COMPARISON))
import common
import run

FAKE = '''
import argparse, json, os, sys
from pathlib import Path
p = argparse.ArgumentParser()
for flag in ('--out', '--only', '--reuse', '--repeats'):
    p.add_argument(flag)
p.add_argument('--list', action='store_true')
p.add_argument('--plan', action='store_true')
a, _ = p.parse_known_args()
control = json.loads(Path(os.environ['FAKE_CONTROL']).read_text())
if a.list:
    print(json.dumps(['a', 'b', 'c']))
    sys.exit(0)
out = Path(a.out)
out.mkdir()
(out / 'provenance.json').write_text('{}')
unit, key = a.only, control['keys'].get(a.only, 1)
if a.plan:
    if control.get('plan_crash'):
        sys.exit('the reuse path raised')
    rows = Path(a.reuse) / 'runs.json'  # a failed attempt may have written none, as the real drivers allow
    earlier = [r['reuse_key'] for r in json.loads(rows.read_text())] if rows.exists() else []
    would_run = control['entries'].get(unit) or ([] if earlier == [dict(v=key)] else ['key changed'])
    (out / 'plan.json').write_text(json.dumps(dict(workload=unit, would_run=would_run, imported=len(earlier))))
    sys.exit(0)
with open(control['log'], 'a') as log:
    log.write('%s reuse=%s\\n' % (unit, Path(a.reuse).name if a.reuse else None))
if unit in control['fail']:
    sys.exit('the unit raised')
calls = sum(1 for line in open(control['log']) if line.split()[0] == unit)  # this unit's runs so far, this one included
stalled = calls in control.get('stalled', {}).get(unit, [])  # W845's rows: the evidence's E1 numbers, a stalled and a level one-CPU row
(out / 'runs.json').write_text(json.dumps([dict(workload=unit, reuse_key=dict(v=key), engine='xppautx-fd-1-diagram', row='run', repetition=1,
                                                wall_seconds=2.982 if stalled else 2.085, cpu_seconds=2.04, pinned_cpus=[0])]))
times = control.get('times', {}).get(unit)  # [mean, sd], or {repetitions: [mean, sd]} for a unit whose pass 1 and rest differ
if times is not None:
    times = times[str(a.repeats or 5)] if isinstance(times, dict) else times
    (out / 'summary.json').write_text(json.dumps([dict(workload=unit, engine='xppautx', row='run', wall_seconds_mean=times[0],
                                                       wall_seconds_sample_sd=times[1], counted_repetitions=int(a.repeats or 5))]))
'''


def fake_machine(path, timed):
    common.save(path, dict(check_only=True))
    return path


class Sampler:
    # run.py's machine snapshots as a stub (W829: no real load, no sleep). Each timed run reads one snapshot before and one after. Between
    # them the machine used seconds[k] of CPU in a process the process table never shows (a short-lived one, seen only in /proc/stat's
    # busy total), in run k; none past the list. The series itself is the one process of the table, using no CPU.
    def __init__(self, seconds=()):
        self.seconds, self.snapshots = list(seconds), 0

    def __call__(self):
        run_index, after = divmod(self.snapshots, 2)
        self.snapshots += 1
        used = self.seconds[run_index] if run_index < len(self.seconds) else 0
        return dict(busy=1000 + (int(used * common.CLOCK_TICKS) if after else 0), table={os.getpid(): (1, 1, 0, 0, 'python')})


class Units(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix='w699-test-')
        self.root = Path(self.scratch.name)
        (self.root / 'drivers').mkdir()
        (self.root / 'drivers/integration.py').write_text(FAKE)
        for name in ('xppautx', 'xppaut'):
            (self.root / name).write_text(name)
        self.control = self.root / 'control.json'
        self.out = self.root / 'results'

    def tearDown(self):
        self.scratch.cleanup()

    def drive(self, *flags, fail=(), keys=None, plan_crash=False, entries=None, check=True, times=None, out=None, competing=(), stalled=None):
        # run.py's main against the fake driver; returns (units the driver ran, the exit message or None). competing: the CPU a
        # stub of the process snapshots gives another process during each timed run (Sampler; none by default, so nothing is disturbed).
        log = self.root / 'ran.log'
        log.unlink(missing_ok=True)
        self.control.write_text(json.dumps(dict(fail=list(fail), keys=keys or {}, log=str(log), plan_crash=plan_crash, entries=entries or {},
                                                times=times or {}, stalled=stalled or {})))
        argv = ['run.py', '--out', str(out or self.out), '--xppautx', str(self.root / 'xppautx'), '--xppaut', str(self.root / 'xppaut'),
                '--auto-dir', str(self.root), '--study', 'screen', *(['--check'] if check else []), *flags]
        message = None
        # the machine record of a timed run comes from the setup script on the Ubuntu machine: the fake records its own
        with mock.patch.object(sys, 'argv', argv), mock.patch.object(run, 'HERE', self.root / 'drivers'), \
                mock.patch.object(run, 'machine_record', side_effect=fake_machine), \
                mock.patch.object(run, 'machine_snapshot', Sampler(competing)), \
                mock.patch.dict(os.environ, FAKE_CONTROL=str(self.control)):
            try:
                run.main()
            except SystemExit as e:
                message = str(e)
        return (log.read_text().splitlines() if log.exists() else []), message

    def done(self):
        return sorted(p.parent.name for p in (self.out / 'screen').glob('*/done.json'))

    def test_a_failed_unit_does_not_stop_the_others_and_a_resume_reruns_only_it(self):
        ran, message = self.drive(fail=['b'])
        self.assertEqual(ran, ['a reuse=None', 'b reuse=None', 'c reuse=None'])
        self.assertEqual(self.done(), ['a', 'c'])
        self.assertIn('screen/b', message)  # the end lists the failed unit and exits non-zero
        failure = json.loads((self.out / 'screen/b/attempt-001.failure.json').read_text())
        self.assertEqual((failure['unit'], failure['exit_code']), ('b', 1))
        self.assertIn('the unit raised', failure['log_tail'][-1])
        ran, message = self.drive()
        self.assertEqual(ran, ['b reuse=attempt-001'])  # a and c are skipped; b imports from its failed attempt
        self.assertIsNone(message)
        self.assertEqual(self.done(), ['a', 'b', 'c'])
        ran, _ = self.drive()
        self.assertEqual(ran, [])

    def test_a_changed_key_reruns_only_its_unit_in_the_same_folder(self):
        self.drive()
        ran, _ = self.drive(keys={'c': 2})
        self.assertEqual(ran, ['c reuse=attempt-001'])
        self.assertEqual(self.done(), ['a', 'b', 'c'])
        self.assertEqual(sorted(p.name for p in (self.out / 'screen/c').glob('attempt-0*') if p.is_dir()), ['attempt-001', 'attempt-002'])
        self.assertEqual(json.loads((self.out / 'screen/c/done.json').read_text())['attempt'], 'attempt-002')

    def test_a_plan_that_fails_stops_before_any_unit_runs(self):
        self.drive()
        with self.assertRaises(RuntimeError):
            self.drive(plan_crash=True)
        self.assertEqual((self.root / 'ran.log').exists(), False)

    def test_plan_alone_runs_nothing(self):
        self.drive()
        ran, _ = self.drive('--plan', keys={'a': 2})
        self.assertEqual(ran, [])
        self.assertEqual(json.loads((self.out / 'screen/a/plan.json').read_text())['would_run'], ['key changed'])

    def test_a_row_of_another_program_that_cannot_be_reused_stops_the_run(self):
        # W802: xppautX's own rows may run; a row of XPPAUT, AUTO-07p or Python never does, so the run stops, naming each one.
        self.drive()
        entries = {'a': ['xppautx repetition 0: earlier run row differs in program', 'row xppautx-fd-1: differs in program'],
                   'b': ['python repetition 1: earlier run row differs in program', 'row auto07p-fd-2: differs in inputs', 'xppaut pilot: no earlier pilot']}
        ran, message = self.drive('--plan', entries=entries, check=False)
        self.assertEqual(ran, [])
        self.assertEqual(run.rows_to_run({('screen', 'a'): entries['a'], ('screen', 'b'): entries['b'], ('server', 'x'): None, ('screen', 'c'): None}),
                         [('screen', 'b', e) for e in entries['b']] + [('screen', 'c', 'no earlier attempt: every row of every program would run')])
        for expected in ('screen/b: python repetition 1', 'screen/b: row auto07p-fd-2', 'screen/b: xppaut pilot'):
            self.assertIn(expected, message)
        self.assertNotIn('screen/a', message)
        ran, message = self.drive(entries=entries)  # a check times nothing: it is not stopped
        self.assertIsNone(message)

    def test_an_import_is_a_hard_link_not_a_copy(self):
        # W802: an imported run takes no space of its own and survives the removal of the attempt it came from.
        earlier = self.root / 'earlier/run'
        earlier.mkdir(parents=True)
        (earlier / 'output.dat').write_text('1 2\n')
        common.link_tree(earlier, self.root / 'new/run')
        common.link_tree(earlier / 'output.dat', self.root / 'new/one.dat')
        self.assertEqual((earlier / 'output.dat').stat().st_ino, (self.root / 'new/run/output.dat').stat().st_ino)
        self.assertEqual((earlier / 'output.dat').stat().st_ino, (self.root / 'new/one.dat').stat().st_ino)


    def series_with_a_set_up(self):
        # An earlier series (a check's folder, so nothing is blocked): b has a counted xppautX row, a none; and b reads a as its set-up.
        ref = self.root / 'ref'
        self.drive(out=ref, times={'b': [1.0, 0.0]})
        needs = lambda run_self, study, unit: ('screen', 'a', '--setup') if (study, unit) == ('screen', 'b') else None
        return ref, mock.patch.object(run.Run, 'needs', needs)

    def test_pass_1_runs_a_units_set_up_before_the_unit_that_reads_it(self):
        # W826: a unit reads its set-up unit's attempt (needs), so pass 1 runs the set-up first; the set-up has no reference to judge
        # against, so it gives no verdict of its own and stops nothing. Before the fix pass 1 ran b before a: "not run: its set-up unit
        # screen/a is not done", and pass 2 ran both.
        ref, patch = self.series_with_a_set_up()
        with patch:
            _, message = self.drive('--reuse', str(ref), check=False, times={'b': [1.0, 0.0]})
        self.assertIsNone(message)
        log = (self.out / 'run.log').read_text().splitlines()
        self.assertNotIn('FAILED', '\n'.join(log))
        self.assertEqual([line for line in log if line.startswith('pass 1 ')], ['pass 1 screen/a: no reference', 'pass 1 screen/b: same'])
        self.assertEqual(self.done(), ['a', 'b', 'c'])

    def test_a_plan_of_a_unit_whose_set_up_is_not_done_plans_its_rows_against_the_set_ups_earlier_attempt(self):
        # W827: a plan that found the set-up not done named that as its one would-run reason, so its rows were never planned, while the
        # run imported them. The plan takes the set-up's earlier attempt in --reuse, the attempt the run imports from until it is done.
        ref, patch = self.series_with_a_set_up()
        with patch:
            _, message = self.drive('--plan', '--reuse', str(ref), check=False, times={'b': [1.0, 0.0]})
        self.assertIsNone(message)
        self.assertEqual(json.loads((self.out / 'screen/b/plan.json').read_text())['would_run'], [])


class EarlierAttempt(unittest.TestCase):
    # W827: a run imports rows only from an attempt that ended (its done.json names it, or its failure record is beside it). An
    # interrupted attempt (the W718 run stopped at XPPAUT repetition 4: provenance, no done, no failure) is partial and never imported.
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix='w827-attempt-')
        self.unit = Path(self.scratch.name) / 'screen/episodic'
        for name in ('attempt-001', 'attempt-002'):
            (self.unit / name).mkdir(parents=True)
            common.save(self.unit / name / 'provenance.json', {})

    def tearDown(self):
        self.scratch.cleanup()

    def test_an_interrupted_attempt_is_never_imported(self):
        self.assertIsNone(run.earlier_attempt(self.unit, None, 'screen'))
        common.save(self.unit / 'done.json', dict(attempt='attempt-001'))  # the unit's done record names the first attempt
        self.assertEqual(run.earlier_attempt(self.unit, None, 'screen').name, 'attempt-001')  # attempt-002 has neither: skipped

    def test_a_failed_attempt_still_supplies_its_rows_to_a_resume(self):
        common.save(self.unit / 'attempt-002.failure.json', dict(unit='episodic'))
        self.assertEqual(run.earlier_attempt(self.unit, None, 'screen').name, 'attempt-002')


class ProgramKeys(unittest.TestCase):
    # W827 (#908): a row is keyed by what its program reads (XPPAUT: the .ode, its includes and SET; xppautX: the .odex; Python: the
    # .odex's numbers), its command line and its program. The plan and the run key rows by the same function (integration.current_key).
    ODEX = 'options: { step.max: 1 }\n'

    def setUp(self):
        import integration
        self.integration = integration
        self.scratch = tempfile.TemporaryDirectory(prefix='w827-test-')
        self.root = Path(self.scratch.name)
        for name in ('xppautx', 'xppaut'):
            (self.root / name).write_text(name)
        self.a = argparse.Namespace(xppautx=self.root / 'xppautx', xppaut=self.root / 'xppaut', cores=[0], check=False, study='screen',
                                    reuse=self.root / 'earlier')
        self.folder = self.root / 'inputs/episodic'
        self.folder.mkdir(parents=True)
        self.model = self.folder / 'episodic.ode'
        self.model.write_text('dif(x)=-x\n')
        (self.folder / 'episodic.ode.set').write_text('# Old ICs\n1.0 x\n')
        self.odex(self.ODEX)

    def tearDown(self):
        self.scratch.cleanup()

    def odex(self, text):
        # what xppautX's conversion writes beside the model: the .odex it reads, and the log and hashes it writes too
        (self.folder / 'episodic.odex').write_text(text)
        (self.folder / 'episodic.conversion.log').write_text('converted %s\n' % len(text))
        (self.folder / 'hashes.json').write_text('{"n": %d}\n' % len(text))

    def key(self, engine):
        return self.integration.current_key(self.a, engine, self.model)

    def test_a_converter_change_reruns_no_xppaut_row(self):
        xppaut, xppautx = self.key('xppaut'), self.key('xppautx')
        self.odex(self.ODEX + '# a new option the converter writes\n')
        self.assertEqual(self.key('xppaut'), xppaut)  # XPPAUT never reads the .odex
        self.assertNotEqual(self.key('xppautx')['inputs'], xppautx['inputs'])  # xppautX reads it

    def test_a_change_of_the_model_or_its_set_reruns_xppaut(self):
        xppaut = self.key('xppaut')
        self.model.write_text('dif(x)=-2*x\n')
        self.assertNotEqual(self.key('xppaut')['inputs'], xppaut['inputs'])
        self.model.write_text('dif(x)=-x\n')
        (self.folder / 'episodic.ode.set').write_text('# Old ICs\n2.0 x\n')
        self.assertNotEqual(self.key('xppaut')['inputs'], xppaut['inputs'])

    def test_a_python_row_ignores_a_default_option_and_a_label_but_not_a_number(self):
        python = self.key('python')['inputs']
        self.odex('options: { step.max: 1, global.min_step_fraction: 1e-10 }\n')  # written at its default (W804)
        self.assertEqual(self.key('python')['inputs'], python)
        self.odex('options: { step.max: 1, array_plot.top_label: "a title" }\n')  # a label changes no number (W806)
        self.assertEqual(self.key('python')['inputs'], python)
        self.odex('options: { step.max: 2 }\n')
        self.assertNotEqual(self.key('python')['inputs'], python)

    def test_an_earlier_xppaut_row_recorded_under_other_inputs_is_imported_when_xppaut_reads_the_same(self):
        # The plan and the run both import through importable: a row kept under the old keys (its recorded inputs are a digest of
        # every copied file) is imported when the files XPPAUT reads are the same, whatever the .odex is.
        key = self.key('xppaut')
        earlier = self.a.reuse / 'runs/episodic/0/xppaut'
        for row in ('run', 'ready'):
            folder = earlier / row
            folder.mkdir(parents=True)
            for f in self.folder.iterdir():
                if f.suffix in ('.ode', '.set', '.inc', '.odex'):
                    (folder / f.name).write_text(f.read_text())
            common.save(folder / 'command.json', ['taskset', '-c', '0', *self.integration.engine_argv(self.a, folder / 'episodic.ode', 'xppaut', folder / 'episodic.ode.set')])
        records = [dict(workload='episodic', engine='xppaut', row=row, repetition=0, valid=True,
                        reuse_key=dict(key, inputs='a digest of every copied file, as recorded before W827')) for row in ('run', 'ready')]
        common.save(self.a.reuse / 'provenance.json', {})
        common.save(self.a.reuse / 'runs.json', records)
        self.odex(self.ODEX + '# a converter change XPPAUT does not read\n')
        found, why = self.integration.importable(self.a, 'episodic', 'xppaut', 0, {'run': self.model, 'ready': self.model})
        self.assertIsNone(why)
        self.assertEqual(sorted(found), ['ready', 'run'])


class LadderReuse(unittest.TestCase):
    # W805: a program's climb (its rungs, the tolerance it reached, its timed rows) is taken from an earlier ladder when its program,
    # measurement and inputs are unchanged and the reference is; a changed program's climb is not, and the others' still are.
    def setUp(self):
        import integration
        self.integration = integration
        self.scratch = tempfile.TemporaryDirectory(prefix='w805-test-')
        self.root = Path(self.scratch.name)
        self.hashes = {'xppautx': 'x1', 'xppaut': 'p1', 'python': 'y1'}
        self.a = argparse.Namespace(out=self.root / 'out', reuse=self.root / 'earlier', cores=[0], check=False, study='python',
                                    xppautx=self.root / 'xppautx', xppaut=self.root / 'xppaut',
                                    program_sha={e: (lambda e=e: self.hashes[e]) for e in self.hashes})
        self.rungs = []
        for tolerance in (1e-4, 1e-6):
            folder = self.root / 'inputs' / ('%g' % tolerance)
            folder.mkdir(parents=True)
            (folder / 'episodic.odex').write_text('dt=0.05 tol=%g\n' % tolerance)
            self.rungs.append((tolerance, folder / 'episodic.ode'))
        self.reuse_ladder()

    def tearDown(self):
        self.scratch.cleanup()

    def reuse_ladder(self):
        # An earlier ladder: xppaut reaches at the first rung, python at the second, xppautX at the second too.
        earlier = self.a.reuse / 'ladder'
        rows = []
        for engine, reached_at in (('xppautx', 1e-6), ('xppaut', 1e-4), ('python', 1e-6)):
            for tolerance, model in self.rungs:
                folder = earlier / 'episodic' / ('%g' % tolerance) / engine
                folder.mkdir(parents=True)
                # a rung's folder as launch() leaves it: the copied input and the command line it ran
                (folder / 'episodic.odex').write_text(model.with_suffix('.odex').read_text())
                argv = ['taskset', '-c', '0', *self.integration.engine_argv(self.a, folder / 'episodic.ode', engine, folder / 'episodic.ode.set')]
                common.save(folder / 'command.json', argv)
                reached = tolerance == reached_at
                rows.append(dict(engine=engine, tolerance=tolerance, valid=True, reached=reached,
                                 reuse_key=self.integration.current_key(self.a, engine, model)))
                if reached:
                    break
        earlier.mkdir(parents=True, exist_ok=True)
        (earlier / 'episodic.json').write_text(json.dumps(dict(rungs=rows)))

    def test_a_climb_is_imported_for_each_program_whose_key_is_unchanged(self):
        ladder = self.integration.ladder_import
        rows, reached = ladder(self.a, 'episodic', 'python', self.rungs, True)
        self.assertEqual((len(rows), reached), (2, 1e-6))
        rows, reached = ladder(self.a, 'episodic', 'xppaut', self.rungs, True)
        self.assertEqual((len(rows), reached), (1, 1e-4))

    def test_a_changed_program_climbs_again_and_the_others_do_not(self):
        self.hashes['xppautx'] = 'x2'  # xppautX changed: its own rungs run, and only its own
        self.a.__dict__.pop('_executable_sha')  # the program hashes are read once per run
        ladder = self.integration.ladder_import
        rows, reason = ladder(self.a, 'episodic', 'xppautx', self.rungs, True)
        self.assertEqual((rows, reason), (None, 'rung 0.0001 differs in program'))
        self.assertIsNotNone(ladder(self.a, 'episodic', 'python', self.rungs, True)[0])
        self.assertIsNotNone(ladder(self.a, 'episodic', 'xppaut', self.rungs, True)[0])

    def test_a_changed_reference_takes_no_climb(self):
        rows, reason = self.integration.ladder_import(self.a, 'episodic', 'python', self.rungs, False)
        self.assertEqual((rows, reason), (None, 'the reference differs'))

    def test_a_changed_input_takes_no_climb(self):
        (self.rungs[1][1].parent / 'episodic.odex').write_text('dt=0.05 tol=2e-6\n')
        rows, reason = self.integration.ladder_import(self.a, 'episodic', 'python', self.rungs, True)
        self.assertEqual((rows, reason), (None, 'rung 1e-06 differs in inputs'))

    def test_a_reused_reference_and_rung_keep_this_runs_own_keys_for_a_later_run(self):
        # W842: a reference (and a rung whose earlier row kept no key) imported from --reuse is linked, so the next run reuses from this
        # attempt. Its key is saved at the import: derived from this attempt's provenance it never matched (the provenance is not that
        # of the run it was linked from), and the unit was planned to run its ladder again.
        model = self.rungs[0][1]
        key = self.integration.reference_key(self.a, model)
        earlier = self.a.reuse / 'ladder' / 'episodic' / 'reference'
        earlier.mkdir(parents=True)
        (earlier / 'episodic.odex').write_text(model.with_suffix('.odex').read_text())
        (earlier / 'result.json').write_text(json.dumps(dict(valid=True)))
        folder = self.a.out / 'ladder' / 'episodic' / 'reference'
        self.a.out.mkdir()
        # W617's reference kept no key: it is reused by the derived key, which reference_reused is made to answer here
        with mock.patch.object(self.integration, 'progress'), mock.patch.object(self.integration, 'reference_reused', return_value=True):
            _, reused = self.integration.reference_run(self.a, model, folder)
        self.assertTrue(reused)
        self.assertEqual(json.loads((folder / self.integration.REFERENCE_KEY).read_text()), key)
        later = argparse.Namespace(**dict(vars(self.a), out=self.root / 'next', reuse=self.a.out))
        self.assertTrue(self.integration.reference_reused(later, later.out / 'ladder' / 'episodic' / 'reference', model))
        # a rung's row, imported when the earlier row kept no key, carries the key it was matched on
        rows = json.loads((self.a.reuse / 'ladder' / 'episodic.json').read_text())['rungs']
        for row in rows:
            row.pop('reuse_key')
        (self.a.reuse / 'ladder' / 'episodic.json').write_text(json.dumps(dict(rungs=rows)))
        self.assertEqual(self.integration.imported_rung_row(self.a, rows[0], dict(self.rungs))['reuse_key'],
                         self.integration.current_key(self.a, rows[0]['engine'], dict(self.rungs)[rows[0]['tolerance']]))


class ReferenceModelKey(unittest.TestCase):
    # W806: a reference's model key leaves out the array plot's labels (DISPLAY_OPTIONS, each named with its reason) and nothing else.
    def setUp(self):
        import terman
        self.terman = terman
        self.scratch = tempfile.TemporaryDirectory(prefix='w806-test-')
        self.root = Path(self.scratch.name)

    def tearDown(self):
        self.scratch.cleanup()

    def odex(self, text):
        path = self.root / 'episodic.odex'
        path.write_text(text)
        return path

    def test_a_label_only_difference_keeps_the_key_and_a_number_changes_it(self):
        before = '  plot: {\n    colormap: jet, array_plot.row_skip: 8, array_plot.z_min: 0, array_plot.z_max: 1,\n  },\n'
        labelled = '  plot: {\n    colormap: jet, array_plot.row_skip: 8, array_plot.z_min: 0, array_plot.z_max: 1, array_plot.top_label: "index",\n' \
                   '    array_plot.side_label: "time, s",\n  },\n'
        other_labels = labelled.replace('index', 'column').replace('time, s', 'seconds')
        key = self.terman.model_key
        self.assertEqual(key(self.odex(before)), key(self.odex(labelled)))
        self.assertEqual(key(self.odex(labelled)), key(self.odex(other_labels)))
        self.assertNotEqual(key(self.odex(before)), key(self.odex(before.replace('z_max: 1', 'z_max: 2'))))
        self.assertNotEqual(key(self.odex(before)), key(self.odex(labelled.replace('z_max: 1', 'z_max: 2'))))
        self.assertNotEqual(key(self.odex(before)), key(self.odex(before.replace('colormap: jet', 'colormap: hot'))))

    def test_every_display_option_is_named_with_its_reason(self):
        self.assertTrue(all(reason.strip() for reason in common.DISPLAY_OPTIONS.values()))
        self.assertEqual(sorted(common.DISPLAY_OPTIONS), ['array_plot.bottom_label', 'array_plot.side_label', 'array_plot.top_label'])


class OdexBeforeW809(unittest.TestCase):
    # W809 renamed and reordered the equilibria and continuation options: a .odex converted now is read as the one written before
    # it (its bytes keyed an earlier row), and a .odex written before it reads as itself.
    BEFORE = ('options {\n  equilibria: { search_limit: 5000, range_low: 0 },\n'
              '  continuation: { max_steps: 200, step_size: 0.02, special_points: "cusp,zero_hopf" },\n}\n')
    NOW = ('options {\n  equilibria: { newton.search_limit: 5000, range.low: 0 },\n'
           '  continuation: { special_points: "cusp,zero_hopf", limits.steps: 200, step.initial: 0.02 },\n}\n')

    def test_a_odex_converted_now_reads_as_before(self):
        self.assertEqual(common.odex_before_w809(self.NOW), self.BEFORE)

    def test_a_odex_written_before_reads_as_itself(self):
        self.assertEqual(common.odex_before_w809(self.BEFORE), self.BEFORE)

    def test_a_converted_group_keeps_its_order_and_wraps_where_the_converter_wrapped_it(self):
        # examples/ode/ma23.ode's continuation options, converted now and before W809 (research/ma23's earlier inputs)
        now = ('  continuation: {\n    limits.parameter_max: 1, limits.parameter_min: -10, output.every: 100000, limits.steps: 1000,\n'
               '    mesh.intervals: 100, step.max: 0.1,\n  }')
        self.assertEqual(common.odex_before_w809(now),
                         '  continuation: {\n    parameter_max: 1, parameter_min: -10, output_every: 100000, max_steps: 1000, mesh_intervals: 100,\n'
                         '    max_step_size: 0.1,\n  }')


class ArgvPaths(unittest.TestCase):
    # W805: an earlier row's command line is compared without the program's own paths: a Python run's interpreter and script (a
    # worktree's files) differ between checkouts, its program hash covers them; its model and the pinned CPU must still agree.
    def setUp(self):
        import integration
        self.integration = integration
        self.scratch = tempfile.TemporaryDirectory(prefix='w805-argv-')
        self.root = Path(self.scratch.name)
        self.a = argparse.Namespace(cores=[0], xppautx=self.root / 'xppautX', xppaut=self.root / 'xppaut')
        self.folder = self.root / 'row'
        self.folder.mkdir()
        (self.folder / 'episodic.ode').write_text('')
        (self.folder / 'episodic.odex').write_text('')

    def tearDown(self):
        self.scratch.cleanup()

    def write(self, argv):
        (self.folder / 'command.json').write_text(json.dumps(argv))

    def test_a_python_run_from_another_checkout_has_the_same_command_line(self):
        now = self.integration.engine_argv(self.a, self.folder / 'episodic.ode', 'python', self.folder / 'x.set')
        self.write(['taskset', '-c', '0', '/elsewhere/.venv/bin/python3', '/elsewhere/tools/python-integration/integrate.py', *now[2:]])
        self.assertTrue(self.integration.argv_unchanged(self.a, 'python', self.folder, 'episodic.ode'))

    def test_a_changed_model_or_cpu_is_a_different_command_line(self):
        now = self.integration.engine_argv(self.a, self.folder / 'episodic.ode', 'python', self.folder / 'x.set')
        self.write(['taskset', '-c', '0', '/elsewhere/python3', '/elsewhere/integrate.py', 'other.odex'])
        self.assertFalse(self.integration.argv_unchanged(self.a, 'python', self.folder, 'episodic.ode'))
        self.write(['taskset', '-c', '3', '/elsewhere/python3', '/elsewhere/integrate.py', *now[2:]])
        self.assertFalse(self.integration.argv_unchanged(self.a, 'python', self.folder, 'episodic.ode'))

    def test_an_xppautx_run_drops_only_its_binary(self):
        now = self.integration.engine_argv(self.a, self.folder / 'episodic.ode', 'xppautx', self.folder / 'x.set')
        self.write(['taskset', '-c', '0', '/elsewhere/xppautX', *now[1:]])
        self.assertTrue(self.integration.argv_unchanged(self.a, 'xppautx', self.folder, 'episodic.ode'))
        self.write(['taskset', '-c', '0', '/elsewhere/xppautX', 'other.odex', '--silent'])
        self.assertFalse(self.integration.argv_unchanged(self.a, 'xppautx', self.folder, 'episodic.ode'))


class PythonEquivalence(unittest.TestCase):
    """W803: python_equivalence.py's record licenses an earlier program hash as today's for the Python arm only, and only when
    every unit was identical and both hashes are still the ones the record names."""

    def check(self, **changes):
        record = dict(all_identical=True, earlier_program_sha256='old', current_program_sha256='now')
        record.update(changes)
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(common, 'EQUIVALENCE', Path(folder) / 'e.json'), \
                mock.patch.object(common, 'python_sha', return_value='now'):
            common.EQUIVALENCE.write_text(json.dumps(record))
            return {engine: common.earlier_program(engine, 'old') for engine in ('python', 'xppautx', 'xppaut', 'auto07p')}

    def test_the_python_arm_alone_takes_the_earlier_hash_as_current(self):
        self.assertEqual(self.check(), dict(python='now', xppautx='old', xppaut='old', auto07p='old'))

    def test_a_record_with_a_difference_licenses_nothing(self):
        self.assertEqual(self.check(all_identical=False)['python'], 'old')

    def test_a_record_of_other_hashes_licenses_nothing(self):
        self.assertEqual(self.check(current_program_sha256='other')['python'], 'old')
        self.assertEqual(self.check(earlier_program_sha256='other')['python'], 'old')

    def test_no_record_licenses_nothing(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(common, 'EQUIVALENCE', Path(folder) / 'none.json'):
            self.assertEqual(common.earlier_program('python', 'old'), 'old')


class Verdicts(unittest.TestCase):
    # W818 (#899): each unit's verdict against the same unit of a reference run, the two passes, the stop and the restart after it.
    setUp, tearDown, drive, done = Units.setUp, Units.tearDown, Units.drive, Units.done
    SAME = [1.0, 0.02]  # the reference's mean and sample SD: a difference under the 25% margin is the same
    ALL = dict(a=SAME, b=SAME, c=SAME)

    def make_reference(self, *timed):
        # The reference run as an earlier series left it (W802 forbids a first run with no earlier attempt, so it is written
        # directly): every unit done with its attempt, the counted rows only for the units named in `timed`.
        self.reference_dir = self.root / 'reference'
        for unit in 'abc':
            attempt = self.reference_dir / 'screen' / unit / 'attempt-001'
            attempt.mkdir(parents=True)
            common.save(attempt / 'provenance.json', {})
            common.save(attempt / 'runs.json', [dict(workload=unit, reuse_key=dict(v=1))])
            if unit in (timed or 'abc'):
                common.save(attempt / 'summary.json', [dict(workload=unit, engine='xppautx', row='run', wall_seconds_mean=self.SAME[0],
                                                            wall_seconds_sample_sd=self.SAME[1], counted_repetitions=5)])
            common.save(attempt.parent / common.DONE, dict(attempt='attempt-001'))

    def series(self, times, *flags):
        return self.drive('--reuse', str(self.reference_dir), *flags, times=times, check=False)

    def verdict(self, unit):
        return json.loads((self.out / 'screen' / unit / 'done.json').read_text())['verdict']

    def test_units_the_same_as_the_reference_pass_two_and_record_the_verdict(self):
        self.make_reference()
        ran, message = self.series(self.ALL)
        self.assertIsNone(message)
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001', 'c reuse=attempt-001'] * 2)  # pass 1, then pass 2
        self.assertEqual([self.verdict(u) for u in 'abc'], ['same'] * 3)
        self.assertEqual(json.loads((self.out / 'screen/a/done.json').read_text())['attempt'], 'attempt-001')
        self.assertTrue((self.out / 'screen/a/pass1/summary.json').exists())  # pass 1's check is kept, not counted
        self.assertEqual(sorted(p.name for p in (self.out / 'screen/a').glob('attempt-*') if p.is_dir()), ['attempt-001'])
        log = (self.out / 'run.log').read_text()
        self.assertIn('pass 1 screen/a: same', log)
        self.assertIn('pass 2 screen/c: same', log)
        ran, message = self.series(self.ALL)  # a resume: every unit is done
        self.assertEqual((ran, message), ([], None))

    def test_a_faster_unit_is_faster_and_the_series_goes_on(self):
        self.make_reference()
        ran, message = self.series(dict(a=[0.5, 0.01], b=self.SAME, c=self.SAME))
        self.assertIsNone(message)
        self.assertEqual(self.verdict('a'), 'faster')
        self.assertEqual(self.verdict('b'), 'same')
        ratio = json.loads((self.out / 'screen/a/done.json').read_text())['rows'][0]['ratio']
        self.assertAlmostEqual(ratio, 0.5)

    def test_a_slower_unit_stops_the_series_and_no_pass_two_runs(self):
        self.make_reference()
        ran, message = self.series(dict(a=self.SAME, b=[2.0, 0.02], c=self.SAME))
        self.assertIn('screen/b', message)
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001'])  # c never ran, pass 2 never ran
        self.assertEqual(json.loads((self.out / 'stopped.json').read_text())['slower'], ['screen/b'])
        self.assertEqual(list((self.out / 'screen').glob('*/done.json')), [])
        self.assertIn('pass 1 screen/b: slower', (self.out / 'run.log').read_text())

    def test_keep_going_judges_every_unit_of_pass_one_and_still_runs_no_pass_two(self):
        self.make_reference()
        ran, message = self.series(dict(a=self.SAME, b=[2.0, 0.02], c=self.SAME), '--keep-going')
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001', 'c reuse=attempt-001'])
        self.assertIn('screen/b', message)
        self.assertTrue((self.out / 'stopped.json').exists())

    def test_a_unit_the_reference_does_not_have_is_no_reference_never_a_failure(self):
        self.make_reference('a', 'b')  # c's reference has no counted row
        ran, message = self.series(self.ALL)
        self.assertIsNone(message)
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001'] + ['a reuse=attempt-001', 'b reuse=attempt-001', 'c reuse=attempt-001'])
        self.assertEqual(self.verdict('c'), 'no reference')
        self.assertEqual(self.verdict('a'), 'same')

    def test_after_a_stop_the_fix_starts_the_series_again_from_its_first_unit(self):
        # b is the same on its pass 1 and slower on its pass 2: a and b are done before the stop (pass 2 reached them).
        self.make_reference()
        slower_later = dict(a=self.SAME, b={'1': self.SAME, '5': [2.0, 0.02]}, c=self.SAME)
        ran, message = self.series(slower_later)
        self.assertIn('screen/b', message)
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001', 'c reuse=attempt-001', 'a reuse=attempt-001', 'b reuse=attempt-001'])
        self.assertEqual(self.verdict('b'), 'slower')
        self.assertTrue((self.out / 'screen/a/done.json').exists())
        # The fix: a new binary, the same units now the same. The series starts from its first unit, and nothing done
        # before the stop is kept: a is run again as its second attempt.
        (self.root / 'xppautx').write_text('fixed binary')
        ran, message = self.series(self.ALL)
        self.assertIsNone(message)
        self.assertEqual([r.split()[0] for r in ran], ['a', 'b', 'c', 'a', 'b', 'c'])  # pass 1 and pass 2, from the first unit
        self.assertEqual(json.loads((self.out / 'screen/a/done.json').read_text())['attempt'], 'attempt-002')
        self.assertFalse((self.out / 'stopped.json').exists())
        self.assertIn('restart: the series stopped at a slower unit', (self.out / 'run.log').read_text())
        self.assertEqual([self.verdict(u) for u in 'abc'], ['same'] * 3)


class AgainstPrograms(unittest.TestCase):
    # W830: judge also compares xppautX's fastest row with each other program's fastest row of the same attempt (W617's own
    # times, never rerun). Behind is reported in `against` and `behind`; the unit's verdict stays slower, faster, same or no reference.

    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix='w830-test-')
        self.root = Path(self.scratch.name)
        self.count = 0

    def tearDown(self):
        self.scratch.cleanup()

    def attempt(self, *rows):
        # An attempt folder whose summary.json holds rows given as (engine, mean, sd, sweep); workload 'm', row 'run'.
        self.count += 1
        folder = self.root / ('attempt-%03d' % self.count)
        folder.mkdir()
        common.save(folder / 'summary.json', [dict(workload='m', engine=e, row='run', wall_seconds_mean=m, wall_seconds_sample_sd=sd,
                                                   counted_repetitions=5, sweep=sweep) for e, m, sd, sweep in rows])
        return folder

    def against(self, result, program):
        return [r for r in result['against'] if r['program'] == program]

    def test_a_row_slower_than_the_reference_is_slower_and_also_behind_another_program(self):
        ref = self.attempt(('xppautx', 1.0, 0.02, False))
        folder = self.attempt(('xppautx', 2.0, 0.02, False), ('xppaut', 1.5, 0.02, False))
        result = common.judge(folder, ref)
        self.assertEqual(result['verdict'], 'slower')
        self.assertEqual(result['behind'], 1)
        self.assertEqual([(r['program'], r['verdict']) for r in result['against']], [('xppaut', 'behind')])
        self.assertAlmostEqual(result['against'][0]['ratio'], 2.0 / 1.5)

    def test_a_row_behind_another_program_is_reported_and_the_unit_keeps_its_verdict(self):
        ref = self.attempt(('xppautx', 1.0, 0.02, False))
        folder = self.attempt(('xppautx', 1.0, 0.02, False), ('xppaut', 0.5, 0.02, False), ('auto07p-fd', 0.9, 0.02, False))
        result = common.judge(folder, ref)
        self.assertEqual(result['verdict'], 'same')  # the verdict against W617 is unchanged: behind is not a stop
        self.assertEqual(result['behind'], 1)
        self.assertEqual([(r['program'], r['verdict']) for r in result['against']], [('auto07p', 'level'), ('xppaut', 'behind')])
        self.assertEqual(self.against(result, 'xppaut')[0]['other_mean'], 0.5)

    def test_a_row_within_the_spread_of_another_program_is_level(self):
        ref = self.attempt(('xppautx', 1.0, 0.02, False))
        folder = self.attempt(('xppautx', 1.0, 0.02, False), ('xppaut', 1.1, 0.02, False))  # 0.909 is within 25% either way
        result = common.judge(folder, ref)
        self.assertEqual(result['verdict'], 'same')
        self.assertEqual((result['behind'], [r['verdict'] for r in result['against']]), (0, ['level']))

    def test_a_unit_with_no_reference_is_judged_against_the_other_programs_alone(self):
        folder = self.attempt(('xppautx', 1.0, 0.02, False), ('xppautx-threads', 0.8, 0.02, False), ('python-numpy', 0.5, 0.02, False),
                              ('python-numba', 2.0, 0.02, False))
        result = common.judge(folder, None)
        self.assertEqual(result['verdict'], 'no reference')
        self.assertEqual(result['rows'], [])
        self.assertEqual(result['behind'], 1)
        [row] = result['against']
        self.assertEqual((row['engine'], row['program'], row['other_engine']), ('xppautx-threads', 'python', 'python-numpy'))
        self.assertAlmostEqual(row['ratio'], 1.6)

    def test_a_unit_with_no_reference_and_no_other_programs_is_no_reference_with_nothing_behind(self):
        folder = self.attempt(('xppautx', 1.0, 0.02, False))
        result = common.judge(folder, None)
        self.assertEqual((result['verdict'], result['against'], result['behind']), ('no reference', [], 0))

    def test_sweep_rows_are_never_compared(self):
        folder = self.attempt(('xppautx', 1.0, 0.02, False), ('xppaut', 0.1, 0.0, True), ('xppaut', 1.0, 0.02, False),
                              ('xppautx', 0.1, 0.0, True))
        result = common.judge(folder, None)
        self.assertEqual([(r['other_mean'], r['verdict']) for r in result['against']], [(1.0, 'level')])
        self.assertEqual(result['behind'], 0)


class Disturbance(unittest.TestCase):
    # W829 (#910): a timed unit during which other processes used the CPU is disturbed: not judged, run again once --ready passes,
    # and the series stops with its reason when a rerun is disturbed too. The snapshots are stubs (Sampler), so nothing is timed.
    setUp, tearDown, drive, done = Units.setUp, Units.tearDown, Units.drive, Units.done
    make_reference, verdict, SAME, ALL = Verdicts.make_reference, Verdicts.verdict, Verdicts.SAME, Verdicts.ALL

    def series(self, times, *flags, competing=(), stalled=None):
        return self.drive('--reuse', str(self.reference_dir), *flags, times=times, competing=competing, stalled=stalled, check=False)

    def record(self, unit, name='pass1.competing.json'):
        return json.loads((self.out / 'screen' / unit / name).read_text())

    def test_a_disturbed_unit_runs_again_once_and_is_judged_only_on_the_quiet_run(self):
        # Timed runs in order: a, b (disturbed: 5 s of other CPU), b again (quiet), c; then pass 2 quiet.
        self.make_reference()
        ran, message = self.series(self.ALL, competing=[0, 5])
        self.assertIsNone(message)
        b = 'b reuse=attempt-001'
        self.assertEqual(ran, ['a reuse=attempt-001', b, b, 'c reuse=attempt-001'] + ['a reuse=attempt-001', b, 'c reuse=attempt-001'])
        log = (self.out / 'run.log').read_text().splitlines()
        self.assertEqual(len([line for line in log if line.startswith('pass 1 screen/b: disturbed: other processes used')]), 1)
        self.assertIn('runs again once --ready passes', '\n'.join(log))
        self.assertEqual([line for line in log if line.startswith('pass 1 screen/b: same')], ['pass 1 screen/b: same'])  # judged once
        self.assertEqual(self.record('b')['verdict'], 'quiet')  # the rerun's own record
        self.assertEqual([self.verdict(u) for u in 'abc'], ['same'] * 3)
        self.assertFalse((self.out / 'stopped.json').exists())  # a disturbed unit never writes the slower stop

    def test_a_unit_disturbed_twice_stops_the_series_with_its_reason(self):
        self.make_reference()
        ran, message = self.series(self.ALL, competing=[0, 5, 5])
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001', 'b reuse=attempt-001'])  # no c, no pass 2
        self.assertIn('screen/b', message)
        self.assertIn('other processes used the CPU during a unit twice', message)
        self.assertFalse((self.out / 'stopped.json').exists())
        self.assertEqual(self.done(), [])  # a was judged in pass 1 (pass1.json), nothing reached done.json
        self.assertTrue((self.out / 'screen/a/pass1.json').exists())
        self.assertFalse((self.out / 'screen/b/pass1.json').exists())
        self.assertEqual(self.record('b')['verdict'], 'disturbed')
        self.assertGreater(self.record('b')['competing_fraction'], self.record('b')['bound'])
        log = (self.out / 'run.log').read_text()
        self.assertIn('pass 1 screen/b: disturbed: other processes used', log)
        self.assertIn('disturbed again, the series stops here', log)
        self.assertIn('disturbed: 1 timed run(s) in this folder', log)

    def test_a_unit_disturbed_again_in_pass_2_stops_the_series_and_keeps_what_was_done(self):
        self.make_reference()
        ran, message = self.series(self.ALL, competing=[0, 0, 0, 0, 5, 5])  # pass 1 quiet; pass 2: a quiet, b disturbed twice
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001', 'c reuse=attempt-001'] + ['a reuse=attempt-001', 'b reuse=attempt-001',
                                                                                                      'b reuse=attempt-001'])
        self.assertIn('screen/b', message)
        self.assertEqual(self.done(), ['a'])  # pass 2's a is done; b and c are not
        self.assertEqual(sorted(p.name for p in (self.out / 'screen/b').glob('attempt-*') if p.is_dir()), ['attempt-001', 'attempt-002'])
        self.assertEqual(self.record('b', 'attempt-002.competing.json')['verdict'], 'disturbed')
        self.assertFalse((self.out / 'stopped.json').exists())

    def test_a_disturbed_attempt_is_kept_as_history_and_never_imported(self):
        # pass 2: b's first attempt is disturbed, its rerun (attempt-002) is quiet and is the unit's result.
        self.make_reference()
        ran, message = self.series(self.ALL, competing=[0, 0, 0, 0, 5])
        self.assertIsNone(message)
        self.assertEqual(json.loads((self.out / 'screen/b/done.json').read_text())['attempt'], 'attempt-002')
        self.assertEqual(sorted(p.name for p in (self.out / 'screen/b').glob('attempt-*') if p.is_dir()), ['attempt-001', 'attempt-002'])
        self.assertEqual(self.record('b', 'attempt-001.competing.json')['verdict'], 'disturbed')
        self.assertEqual(self.record('b', 'attempt-002.competing.json')['verdict'], 'quiet')
        self.assertEqual(run.earlier_attempt(self.out / 'screen/b', None, 'screen').name, 'attempt-002')  # attempt-001 never ended

    def test_a_stalled_row_inside_a_unit_disturbs_it_though_the_unit_wide_check_is_quiet(self):
        # W845 (#926): w718-20261120f's E1 pass 1: a one-CPU row of wall 2.982 s and cpu 2.04 s (level rows: 2.085 and 2.01) in a unit
        # whose competing CPU was 8% of one CPU. b's first run has that row: disturbed, run again, judged only on the second.
        self.make_reference()
        ran, message = self.series(self.ALL, stalled={'b': [1]})
        self.assertIsNone(message)
        b = 'b reuse=attempt-001'
        self.assertEqual(ran[:4], ['a reuse=attempt-001', b, b, 'c reuse=attempt-001'])
        log = (self.out / 'run.log').read_text()
        self.assertIn('pass 1 screen/b: disturbed: other processes used', log)
        self.assertIn('1 row(s) stalled, the first xppautx-fd-1-diagram repetition 1 waited 0.94 s off the CPU', log)
        self.assertEqual(self.record('b')['verdict'], 'quiet')

    def test_a_unit_with_a_stalled_row_twice_stops_the_series(self):
        self.make_reference()
        ran, message = self.series(self.ALL, stalled={'b': [1, 2]})
        self.assertEqual(ran, ['a reuse=attempt-001', 'b reuse=attempt-001', 'b reuse=attempt-001'])
        self.assertIn('other processes used the CPU during a unit twice', message)
        self.assertEqual(self.record('b')['stalled'][0]['engine'], 'xppautx-fd-1-diagram')
        self.assertFalse((self.out / 'screen/b/pass1.json').exists())

    def test_a_row_on_several_threads_or_of_another_program_is_not_judged_by_its_gap(self):
        gap = dict(workload='m', wall_seconds=3.0, cpu_seconds=2.0)
        rows = [dict(gap, engine='xppautx-fd-2-none', pinned_cpus=[0, 1]), dict(gap, engine='xppaut-fd', pinned_cpus=[0]),
                dict(gap, engine='auto07p-fd-1', pinned_cpus=[0]), dict(gap, engine='xppautx-fd-1-none'),
                dict(gap, engine='xppautx-fd-1-none', pinned_cpus=[0], wall_seconds=2.49)]
        self.assertEqual(common.stalled_rows(rows), [])

    def test_a_short_lived_process_seen_only_in_the_machine_total_disturbs_its_unit(self):
        # A build step that starts and exits inside the unit never shows in the process table: only /proc/stat's busy total has its CPU.
        self.make_reference()
        ran, message = self.series(self.ALL, competing=[0, 5, 5])
        self.assertIn('screen/b', message)
        self.assertEqual(self.record('b')['processes'], [])  # named by no process: the total is all there is
        self.assertGreater(self.record('b')['competing_cpu_seconds'], 4.0)
        self.assertIn('none named', (self.out / 'run.log').read_text())

    def test_a_quiet_unit_is_judged_as_before_and_records_its_competing_cpu(self):
        self.make_reference()
        ran, message = self.series(self.ALL)
        self.assertIsNone(message)
        done = json.loads((self.out / 'screen/a/done.json').read_text())
        self.assertEqual((done['verdict'], done['competing']['verdict'], done['competing']['competing_fraction']), ('same', 'quiet', 0))
        self.assertEqual(self.record('a', 'attempt-001.competing.json')['verdict'], 'quiet')

    def test_a_check_times_nothing_and_is_never_disturbed(self):
        ran, message = self.drive(competing=[5, 5, 5])  # a check: no timed run, so no snapshot and no record
        self.assertIsNone(message)
        self.assertEqual(list((self.out / 'screen').glob('*/*.competing.json')), [])


class CompetingCpu(unittest.TestCase):
    # W829: the machine's busy CPU minus the series' own, on readings written out by hand: nothing is read from the machine but this process.
    def readings(self):
        # Ticks: the series is run.py (pid me) and its live driver (40) and that driver's child (41), which started inside the unit.
        # Visible competing work: gradle (20, own CPU 1000 ticks), its javac (30, new) and ssh (50, new). Invisible: 200 ticks in the busy
        # total only (a short-lived process). The series' own CPU moves by 320 ticks: run.py's reaped children (9 - 5), the driver's
        # (34 - 20) and the new child's 300.
        me = os.getpid()
        before = dict(busy=10000, table={me: (1, 100, 10, 5, 'run.py'), 40: (me, 400, 20, 0, 'driver'), 20: (1, 200, 500, 0, 'gradle')})
        after = dict(busy=10000 + 320 + 1000 + 400 + 100 + 200,
                     table={me: (1, 100, 12, 9, 'run.py'), 40: (me, 400, 30, 4, 'driver'), 41: (40, 450, 300, 0, 'xppautx'),
                            20: (1, 200, 1500, 0, 'gradle'), 30: (20, 300, 400, 0, 'javac'), 50: (1, 500, 100, 0, 'ssh')})
        return me, before, after

    def test_the_series_tree_is_the_series_and_gradle_is_not(self):
        me, before, after = self.readings()
        self.assertEqual(common.process_tree(after['table'], me), {me, 40, 41})
        used = common.competing_cpu(before['table'], after['table'], {me, 40, 41})
        self.assertEqual(sorted(used), [20, 30, 50])  # gradle, its javac and ssh: the series' own three pids are not competing

    def test_the_machine_total_less_the_series_is_competing_and_the_largest_users_are_named(self):
        me, before, after = self.readings()
        t = common.CLOCK_TICKS
        record = common.competing_record(before, after, wall=10.0)
        self.assertAlmostEqual(record['busy_cpu_seconds'], 2020 / t)
        self.assertAlmostEqual(record['series_cpu_seconds'], 320 / t)
        self.assertAlmostEqual(record['competing_cpu_seconds'], 1700 / t)  # visible 1500 and invisible 200 ticks
        self.assertAlmostEqual(record['competing_fraction'], 1700 / t / 10.0)
        self.assertEqual(record['verdict'], 'disturbed')
        self.assertEqual([p['name'] for p in record['processes']], ['gradle', 'javac', 'ssh'])  # best effort: the series is never named

    def test_the_series_own_work_is_not_competing(self):
        # Only the series used the CPU: the busy total moved by exactly the series' own ticks, reaped ones included.
        me, before, after = self.readings()
        only_series = dict(busy=before['busy'] + 320, table={me: after['table'][me], 40: after['table'][40], 41: after['table'][41]})
        record = common.competing_record(dict(busy=before['busy'], table={me: before['table'][me], 40: before['table'][40]}), only_series, wall=10.0)
        self.assertEqual((record['competing_cpu_seconds'], record['verdict'], record['processes']), (0, 'quiet', []))

    def test_a_skew_between_the_two_reads_is_clamped_not_negative(self):
        me, before, after = self.readings()
        skewed = dict(busy=before['busy'] + 300, table=after['table'])  # the total read a tick or two before the series' ticks
        record = common.competing_record(before, skewed, wall=10.0)
        self.assertEqual((record['competing_cpu_seconds'], record['verdict']), (0, 'quiet'))

    def test_the_bound_is_half_a_cpu_and_only_passing_it_disturbs(self):
        t, me = common.CLOCK_TICKS, os.getpid()
        at = int(common.DISTURBED_CPU_FRACTION * 10 * t)  # ten seconds of wall at the bound
        before = dict(busy=0, table={me: (1, 100, 0, 0, 'run.py')})
        self.assertEqual(common.competing_record(before, dict(busy=at, table=before['table']), wall=10.0)['verdict'], 'quiet')
        self.assertEqual(common.competing_record(before, dict(busy=at + 1, table=before['table']), wall=10.0)['verdict'], 'disturbed')
        self.assertEqual(common.DISTURBED_CPU_FRACTION, 0.5)

    def test_a_reused_pid_with_another_start_time_is_a_new_process(self):
        me, before, after = self.readings()
        after['table'][20] = (1, 999, 3 * common.CLOCK_TICKS, 0, 'bash')  # pid 20 was reused: its ticks are all new
        self.assertAlmostEqual(common.competing_cpu(before['table'], after['table'], {me})[20][0], 3.0)

    def test_the_machine_reads_of_this_process(self):
        self.assertGreaterEqual(common.busy_ticks(), 0)
        table = common.process_table()
        me = os.getpid()
        self.assertEqual(table[me][0], os.getppid())  # the parent of this test is not changed by it
        self.assertIsInstance(table[me][4], str)  # the command name
        self.assertIn(me, common.process_tree(table, os.getppid()))
        self.assertEqual(common.machine_snapshot()['table'][me][0], os.getppid())


class AutoThreads(unittest.TestCase):
    # W843 (#924): the auto rows' count is xppautX's own, read with its process allowed the comparison's cores. The stub
    # program reports the size of its affinity, as W817's xppautX counts it, so no build is needed; the count read under
    # the pilot's one core would be 1 and the rows would run on core 0 alone.
    STUB = '#!%s\nimport json, os, sys\nprint(json.dumps(dict(ev="done")), flush=True)\nfor _ in sys.stdin:\n' \
           '    print(json.dumps(dict(ev="autox.options", threads_auto=len(os.sched_getaffinity(0)))), flush=True)\n' \
           '    print(json.dumps(dict(ev="done")), flush=True)\n'

    def test_the_count_is_read_with_the_comparison_cores_allowed(self):
        import continuation
        allowed = sorted(os.sched_getaffinity(0))
        cores = allowed[:4]
        with tempfile.TemporaryDirectory() as tmp:
            stub = Path(tmp) / 'xppautX'
            stub.write_text(self.STUB % sys.executable)
            stub.chmod(0o755)
            a = argparse.Namespace(xppautx=stub, cores=cores)
            self.assertEqual(continuation.auto_threads(a, 'examples/ode/lecar.odex'), len(cores))
        self.assertEqual(sorted(os.sched_getaffinity(0)), allowed)  # the caller's affinity is put back


if __name__ == '__main__':
    unittest.main()
