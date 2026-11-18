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
(out / 'runs.json').write_text(json.dumps([dict(workload=unit, reuse_key=dict(v=key))]))
times = control.get('times', {}).get(unit)  # [mean, sd], or {repetitions: [mean, sd]} for a unit whose pass 1 and rest differ
if times is not None:
    times = times[str(a.repeats or 5)] if isinstance(times, dict) else times
    (out / 'summary.json').write_text(json.dumps([dict(workload=unit, engine='xppautx', row='run', wall_seconds_mean=times[0],
                                                       wall_seconds_sample_sd=times[1], counted_repetitions=int(a.repeats or 5))]))
'''


def fake_machine(path, timed):
    common.save(path, dict(check_only=True))
    return path


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

    def drive(self, *flags, fail=(), keys=None, plan_crash=False, entries=None, check=True, times=None, out=None):
        # run.py's main against the fake driver; returns (units the driver ran, the exit message or None).
        log = self.root / 'ran.log'
        log.unlink(missing_ok=True)
        self.control.write_text(json.dumps(dict(fail=list(fail), keys=keys or {}, log=str(log), plan_crash=plan_crash, entries=entries or {},
                                                times=times or {})))
        argv = ['run.py', '--out', str(out or self.out), '--xppautx', str(self.root / 'xppautx'), '--xppaut', str(self.root / 'xppaut'),
                '--auto-dir', str(self.root), '--study', 'screen', *(['--check'] if check else []), *flags]
        message = None
        # the machine record of a timed run comes from the setup script on the Ubuntu machine: the fake records its own
        with mock.patch.object(sys, 'argv', argv), mock.patch.object(run, 'HERE', self.root / 'drivers'), \
                mock.patch.object(run, 'machine_record', side_effect=fake_machine), \
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
        self.assertTrue(all(reason.strip() for reason in self.terman.DISPLAY_OPTIONS.values()))
        self.assertEqual(sorted(self.terman.DISPLAY_OPTIONS), ['array_plot.bottom_label', 'array_plot.side_label', 'array_plot.top_label'])


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
    SAME = [1.0, 0.02]  # the reference's mean and sample SD: a difference under the 10% margin is the same
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


if __name__ == '__main__':
    unittest.main()
