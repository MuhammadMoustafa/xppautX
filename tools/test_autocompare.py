"""Regression checks for the baseline check's acceptance and text contract."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from autocompare import BASE, Diagram, compare_run, difference, read_description, read_reference, reference_spectrum


class AutoCompare(unittest.TestCase):
    def point_run(self, pt, n, lab=()):
        return dict(br=1, pt=pt, x=[0.0] * n, y=[0.0] * n, lab=list(lab))

    def add(self, start, runs):
        return dict(ev='diagram', op='add', view=0, **{'from': start}, runs=runs)

    def test_batching_does_not_change_the_points(self):
        whole = Diagram().apply([self.add(0, [self.point_run(1, 3, [[1, 4, 'HB']])])])
        batches = Diagram().apply([self.add(0, [self.point_run(1, 1)]),
                                   self.add(1, [self.point_run(2, 2, [[0, 4, 'HB']])])])
        self.assertEqual(whole.points, batches.points)
        self.assertEqual(whole.points[1], dict(lab=4, sym='HB'))
        self.assertEqual(whole.points[0], dict(lab=0, sym=''))

    def test_reset_keeps_the_points_before(self):
        dg = Diagram().apply([self.add(0, [self.point_run(1, 2, [[1, 4, 'HB']])])])
        dg.apply([dict(ev='diagram', op='reset', view=0, keep=1), self.add(1, [self.point_run(2, 1)])])
        self.assertEqual(dg.points, [dict(lab=0, sym=''), dict(lab=0, sym='')])
        with self.assertRaisesRegex(ValueError, 'noncontiguous'):
            Diagram().apply([self.add(1, [self.point_run(1, 1)])])

    def test_difference_names_both_lines_and_missing_endpoint(self):
        self.assertEqual(difference('model', b'h\nb\n', b'h\nc\n'),
                         'model: first difference at point 1\n  want: b\n  got:  c')
        self.assertIn('point 2\n  want: (end of diagram)\n  got:  c',
                      difference('model', b'h\nb\n', b'h\nb\nc\n'))
        self.assertIn('the header', difference('model', b'h\n', b'g\n'))

    def test_bad_descriptions_fail_before_a_server_starts(self):
        original = json.loads((BASE / 'tstauto_lo.json').read_text())
        bad = []
        for edit in ('unknown', 'zero_step', 'future_run', 'nan', 'skip_and_runs', 'outside'):
            d = copy.deepcopy(original)
            if edit == 'unknown':
                d['runs'][0]['auto']['nmx'] = 10
            elif edit == 'zero_step':
                d['runs'][0]['auto']['ds'] = 0
            elif edit == 'future_run':
                d['runs'][0]['start'] = dict(run='future', type='HB', index=1)
            elif edit == 'nan':
                d['initial_conditions'] = {'x': float('nan')}
            elif edit == 'skip_and_runs':
                d['skip'] = 'not converged'
            else:
                d['model'] = '../lecar.odex'
            bad.append(d)
        with tempfile.TemporaryDirectory(prefix='xppautocompare-test-') as tmp:
            path = Path(tmp) / 'description.json'
            for d in bad:
                with self.subTest(d=d):
                    path.write_text(json.dumps(d), encoding='utf-8')
                    with self.assertRaises(ValueError):
                        read_description(path)
            path.write_text('{"model":"a","model":"b"}', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'duplicate field'):
                read_description(path)


class References(unittest.TestCase):
    AUTO = dict(dsmax=0.1, epsl=1e-4, epsu=1e-4, epss=1e-4)

    def row(self, itp, par, norm, per=0.0, ev=(0.0, 0.0)):
        return dict(itp=str(itp), continued_parameter=str(par), norm=str(norm), per=str(per), evr1=str(ev[0]), evi1=str(ev[1]))

    def rows(self):
        """a branch: start, ordinary points, a limit point (2) at 1.0 where the norm is 2.0"""
        return [self.row(9, 0.0, 0.0)] + [self.row(0, 0.2 * i, 0.4 * i, ev=(0.1, 0.0)) for i in range(1, 5)] +             [self.row(2, 1.0, 2.0, ev=(1.0, 0.0)), self.row(0, 1.2, 2.4, ev=(-0.1, 0.0))]

    def point(self, t, par, norm, label=1, point=1, values=(), spectrum=''):
        return dict(type=t, branch=1, label=label, point=point, par=par, norm=norm, period=None, spectrum=spectrum,
                    values=[list(v) for v in values])

    def test_agreement_within_the_tolerance_and_differences_beyond_it(self):
        ref = [self.point('EP', 0.0, 0.0), self.point('LP', 1.0 + 1e-6, 2.0 - 1e-6, 5, 5, [(0.0, 0.0)], 'eigenvalue')]
        diffs, measures = compare_run(self.rows(), self.AUTO, 'equilibrium', ref, 'm')
        self.assertEqual(diffs, [])
        self.assertEqual([m[0] for m in measures], ['par', 'norm', 'par', 'norm', 'spectrum'])
        ref[1]['par'] = 1.01
        diffs, _ = compare_run(self.rows(), self.AUTO, 'equilibrium', ref, 'm')
        self.assertEqual(len(diffs), 1)
        self.assertIn('LP (stored point 6) par', diffs[0])

    def test_a_missing_or_extra_special_point_is_a_difference(self):
        ref = [self.point('EP', 0.0, 0.0)]
        diffs, _ = compare_run(self.rows(), self.AUTO, 'equilibrium', ref, 'm')
        self.assertIn("special points ['EP'], ours ['EP', 'LP']", diffs[0])
        ref = [self.point('EP', 0.0, 0.0), self.point('LP', 1.0, 2.0), self.point('HB', 1.1, 2.2)]
        diffs, _ = compare_run(self.rows(), self.AUTO, 'equilibrium', ref, 'm')
        self.assertIn("special points ['EP', 'LP', 'HB'], ours ['EP', 'LP']", diffs[0])

    def test_a_traced_difference_is_exactly_one_unmatched_point(self):
        ref = [self.point('EP', 0.0, 0.0)]
        known = [dict(side='ours', type='LP', index=1, cause='traced')]
        self.assertEqual(compare_run(self.rows(), self.AUTO, 'equilibrium', ref, 'm', known)[0], [])
        ref = [self.point('EP', 0.0, 0.0), self.point('LP', 1.0, 2.0), self.point('LP', 1.1, 2.2), self.point('LP', 1.2, 2.4)]
        known = [dict(side='reference', type='LP', index=2, cause='traced'), dict(side='reference', type='LP', index=3, cause='traced')]
        self.assertEqual(compare_run(self.rows(), self.AUTO, 'equilibrium', ref, 'm', known)[0], [])
        ref = [self.point('EP', 0.0, 0.0)]
        known = [dict(side='ours', type='LP', index=2, cause='traced')]
        diffs, _ = compare_run(self.rows(), self.AUTO, 'equilibrium', ref, 'm', known)
        self.assertIn('not present any more', diffs[0])

    def test_an_equilibrium_eigenvalue_is_compared_as_the_multiplier_xppaut_stores(self):
        values, size = reference_spectrum(self.point('LP', 1, 1, values=[(0.0, 0.0), (-0.1, 0.0)], spectrum='eigenvalue'))
        self.assertEqual(values[1], (1.0, 0.0))
        self.assertAlmostEqual(values[0][0], 0.9048374180359595)
        self.assertEqual(size, 0.1)
        values, _ = reference_spectrum(self.point('LP', 1, 1, values=[(0.5, 0.0)], spectrum='multiplier'))
        self.assertEqual(values, [(0.5, 0.0)])

    def test_reference_files_are_read_strictly(self):
        d = json.loads((BASE / 'tstauto_ab.json').read_text())
        good = dict(format=1, source='x', model=d['model'], runs=[dict(name='steady', parameter='p0', points=[])])
        with tempfile.TemporaryDirectory(prefix='xppautocompare-test-') as tmp:
            path = Path(tmp) / 'ab.auto07p'
            for edit in (lambda r: r.update(format=2), lambda r: r.update(model='examples/tstauto/lo.odex'),
                         lambda r: r['runs'][0].update(name='cycles'), lambda r: r.update(extra=1),
                         lambda r: r['runs'][0]['points'].append(dict(type='XX'))):
                bad = copy.deepcopy(good)
                edit(bad)
                path.write_text(json.dumps(bad), encoding='utf-8')
                with self.assertRaises(ValueError):
                    read_reference(path, d)
            path.write_text(json.dumps(good), encoding='utf-8')
            self.assertEqual(read_reference(path, d)['model'], d['model'])


if __name__ == '__main__':
    unittest.main()
