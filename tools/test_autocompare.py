"""Regression checks for the baseline check's acceptance and text contract."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from autocompare import BASE, Diagram, difference, read_description


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


if __name__ == '__main__':
    unittest.main()
