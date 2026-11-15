"""Tests of the Python package python/xppautx (its Client; arrays.py, W306, #365) against a built xppautX: XPPAUTX_BIN names it (default ./xppautX).
Run by autoxcheck's `script` section, so every platform's CI runs them. The models are AUTO-07p's ab demo (examples/tstauto/ab.odex)
and the published Morris-Lecar model (examples/ode/lecar.odex)."""
import os, shutil, sys, tempfile, unittest
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'python'))
from xppautx import Client, XppError
from xppautx.arrays import Session, table

BINARY = os.path.abspath(os.environ.get('XPPAUTX_BIN', './xppautX'))
AB = os.path.join(ROOT, 'examples', 'tstauto', 'ab.odex')
# autoX's settings for the ab model's first branch (tests/autox/tstauto_ab.json)
AB_RUN = dict(step_size=0.02, min_step_size=1e-5, max_step_size=0.02, max_steps=100, output_every=20, parameter_min=0, parameter_max=1.5,
              mesh_intervals=15, max_branch_switches=0, follow_all_branches=False)


class Table(unittest.TestCase):
    def test_numbers_are_floats_and_text_stays_text(self):
        t = table('branch,type,x\n1,end,0.5\n1,regular,1e-3\n')
        self.assertEqual(t['branch'].dtype, np.float64)
        self.assertEqual(list(t['type']), ['end', 'regular'])
        self.assertEqual(list(t['x']), [0.5, 0.001])


class Library(unittest.TestCase):
    def test_autox_run_and_diagram_as_arrays(self):
        with Session(AB, binary=BINARY) as s:
            s.autox_axes('p0')
            result = s.autox_run('equilibrium', replace_diagram=True, **AB_RUN)
            self.assertEqual((result['points_computed'], result['branches_tracked']), (100, 1))
            d = s.diagram()
            self.assertEqual(len(d['param1']), 100)
            self.assertEqual(d['type'][0], 'end')
            self.assertEqual(d['param1'][0], 0)
            self.assertIn('fold', list(d['type']), 'the branch turns at the ab model\'s fold')
            self.assertEqual(set(s.eigenvalues()['kind']), {'eigenvalue'})

    def test_values_and_integration_as_arrays(self):
        with Session(os.path.join(ROOT, 'examples', 'ode', 'lecar.odex'), binary=BINARY) as s:
            data = s.integrate(total_time=10)
            self.assertEqual(data['T'][0], 0)
            self.assertAlmostEqual(data['T'][-1], 10)
            self.assertEqual(data['V'].shape, data['T'].shape)
            first = data['V'].copy()
            self.assertEqual(s.set_initial(v=-0.2)['V'], -0.2)
            self.assertNotEqual(s.integrate()['V'][0], first[0])

    def test_a_model_runs_in_a_scratch_copy_removed_at_close(self):
        folder = tempfile.mkdtemp(prefix='xpplibrary')
        self.addCleanup(shutil.rmtree, folder, ignore_errors=True)
        shutil.copy(os.path.join(ROOT, 'examples', 'ode', 'lecar.odex'), folder)
        with Session(os.path.join(folder, 'lecar.odex'), binary=BINARY) as s:
            s.integrate(total_time=5)
            scratch = s.cwd
            self.assertNotEqual(os.path.abspath(scratch), folder)
            self.assertTrue(os.path.isdir(scratch))
        self.assertEqual(os.listdir(folder), ['lecar.odex'])
        self.assertFalse(os.path.exists(scratch))

    def test_a_refused_command_raises_with_the_core_error(self):
        with Session(AB, binary=BINARY) as s:
            with self.assertRaises(XppError) as caught:
                s.autox_run('periodic', label=99)
            self.assertIn('No point is labelled 99', str(caught.exception))
            self.assertEqual(caught.exception.error['status'], 'error')
            with self.assertRaises(XppError):
                s.set_parameters(nothere=1)

    def test_a_script_by_its_runs(self):
        folder = tempfile.mkdtemp(prefix='xpplibrary')
        try:
            shutil.copy(AB, folder)
            script = os.path.join(folder, 'ab.autox')
            settings = ', '.join('%s: %s' % (k, str(v).lower() if isinstance(v, bool) else v) for k, v in AB_RUN.items())
            with open(script, 'w', encoding='utf-8') as f:
                f.write('version: 4,\nmodel: "ab.odex",\nruns: [ { name: "steady", kind: equilibrium, parameters: ["p0"], settings: { %s } } ]\n' % settings)
            with Session(script, binary=BINARY) as s:
                self.assertEqual(s.script()['runs'], ['steady'])
                self.assertEqual(s.script()['completed'], [])
                self.assertEqual(s.run_script()['runs_completed'], 1)
                self.assertEqual(s.script()['completed'], ['steady'])
                self.assertEqual(len(s.diagram()['param1']), 100)
            with self.assertRaises(XppError) as caught:
                Session(os.path.join(folder, 'missing.autox'), binary=BINARY)
            self.assertIn('missing.autox', str(caught.exception))
        finally:
            shutil.rmtree(folder, ignore_errors=True)


class ClientTests(unittest.TestCase):
    """The package's own Client (python/xppautx): the process, the commands, the asks, the errors, the quit"""
    LECAR = os.path.join(ROOT, 'examples', 'ode', 'lecar.odex')

    def setUp(self):
        self.folder = tempfile.mkdtemp(prefix='xppclientlib')
        self.addCleanup(shutil.rmtree, self.folder, ignore_errors=True)
        self.model = os.path.join(self.folder, 'lecar.odex')
        shutil.copy(self.LECAR, self.folder)

    def test_a_command_waits_for_its_done_and_returns_its_result(self):
        with Client(self.model, binary=BINARY) as x:
            x.start_events()
            self.assertEqual(x.command('run.initial')['steps'], 601)
            self.assertEqual(x.command('data.page', from_row=0, count=1)['total_rows'], 601)
            self.assertEqual(x.events_of_last[-1]['ev'], 'done')

    def test_an_error_keeps_its_fields_and_a_model_that_does_not_load_its_place(self):
        with Client(self.model, binary=BINARY) as x:
            x.start_events()
            with self.assertRaises(XppError) as caught:
                x.command('value.set_parameters', values={'nothere': 1})
            self.assertTrue(caught.exception.error['error'])
        bad = os.path.join(self.folder, 'bad.odex')
        with open(bad, 'w', encoding='utf-8') as f:
            f.write('version 4\npar a = 1\nv\' = nonsense(\n')
        with self.assertRaises(XppError) as caught:
            Client(bad, binary=BINARY).start_events()
        self.assertIn('bad.odex:4', str(caught.exception))

    def test_an_ask_is_answered_as_the_caller_says_or_refused(self):
        with Client(self.model, binary=BINARY) as x:
            x.start_events()
            x.command('session.save_as', file='a.snapx', confirm=True)
            with self.assertRaises(XppError) as caught:  # the replace question, with no answer given
                x.command('session.save_as', file='a.snapx')
            self.assertIn('asked a question', str(caught.exception))
            self.assertEqual(x.command('run.initial')['steps'], 601)  # the program was not left waiting on it
            asked = []
            x.command('session.save_as', file='a.snapx', answers={'choice': lambda e: asked.append(e['kind']) or {'key': 'y'}})
            self.assertTrue(asked)

    def test_quit_ends_the_process_and_no_binary_is_searched_for(self):
        x = Client(self.model, binary=BINARY)
        x.start_events()
        x.close()
        self.assertFalse(x.alive())
        self.assertEqual(x.proc.returncode, 0)
        saved = os.environ.pop('XPPAUTX', None)
        try:
            with self.assertRaises(XppError):
                Client(self.model)
        finally:
            if saved is not None:
                os.environ['XPPAUTX'] = saved


if __name__ == '__main__':
    unittest.main()
