"""Tests of the Python library tools/xppautx.py (W306, #365) against a built xppautX: XPPAUTX_BIN names it (default ./xppautX).
Run by autoxcheck's `script` section, so every platform's CI runs them. The models are AUTO-07p's ab demo (examples/tstauto/ab.odex)
and the published Morris-Lecar model (examples/ode/lecar.odex)."""
import os, shutil, tempfile, unittest
import numpy as np
from xppautx import Session, XppError, table

BINARY = os.path.abspath(os.environ.get('XPPAUTX_BIN', './xppautX'))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
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


if __name__ == '__main__':
    unittest.main()
