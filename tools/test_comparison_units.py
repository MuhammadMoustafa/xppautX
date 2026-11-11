#!/usr/bin/env -S uv run --locked python
"""W699: the comparison's units run on their own. A fake driver (the drivers' unit protocol: --list, --plan, --only, --reuse) stands in
for integration.py: one unit fails and the others finish, a resume reruns only that unit, a change to one unit's key reruns only it,
and a plan that fails stops the run before any unit runs. No program, no timing."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

COMPARISON = Path(__file__).resolve().parents[1] / 'paper/benchmarks/comparison'
sys.path.insert(0, str(COMPARISON))
import run

FAKE = '''
import argparse, json, os, sys
from pathlib import Path
p = argparse.ArgumentParser()
for flag in ('--out', '--only', '--reuse'):
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
    would_run = [] if earlier == [dict(v=key)] else ['key changed']
    (out / 'plan.json').write_text(json.dumps(dict(workload=unit, would_run=would_run, imported=len(earlier))))
    sys.exit(0)
with open(control['log'], 'a') as log:
    log.write('%s reuse=%s\\n' % (unit, Path(a.reuse).name if a.reuse else None))
if unit in control['fail']:
    sys.exit('the unit raised')
(out / 'runs.json').write_text(json.dumps([dict(workload=unit, reuse_key=dict(v=key))]))
'''


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

    def drive(self, *flags, fail=(), keys=None, plan_crash=False):
        # run.py's main against the fake driver; returns (units the driver ran, the exit message or None).
        log = self.root / 'ran.log'
        log.unlink(missing_ok=True)
        self.control.write_text(json.dumps(dict(fail=list(fail), keys=keys or {}, log=str(log), plan_crash=plan_crash)))
        argv = ['run.py', '--out', str(self.out), '--xppautx', str(self.root / 'xppautx'), '--xppaut', str(self.root / 'xppaut'),
                '--auto-dir', str(self.root), '--study', 'screen', '--check', *flags]
        message = None
        with mock.patch.object(sys, 'argv', argv), mock.patch.object(run, 'HERE', self.root / 'drivers'), \
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


if __name__ == '__main__':
    unittest.main()
