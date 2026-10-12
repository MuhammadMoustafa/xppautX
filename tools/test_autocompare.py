"""Regression checks for the baseline check's acceptance and text contract."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from autocompare import BASE, Diagram, Number, difference, protocol_integer, read_description, text_value


class AutoCompare(unittest.TestCase):
    def test_json_numbers_retain_the_protocol_spelling(self):
        values = json.loads('[1e-07,1.000000,-0,-0.0,7]', parse_float=Number, parse_int=protocol_integer)
        self.assertEqual([text_value(v) for v in values], ['1e-07', '1.000000', '-0', '-0.0', '7'])
        self.assertEqual(values[-1] + 1, 8)

    def point_run(self, pt, xs, new=False):
        return dict(br=1, pt=pt, ty=1, stable=True, periodic=False, d=1, c=20,
                    lw=2, jac='difference', x=xs, y=[Number('-0.0')] * len(xs),
                    **({'new': 1} if new else {}))

    def add(self, start, runs):
        return dict(ev='diagram', op='add', view=0, **{'from': start}, runs=runs)

    def test_batching_does_not_change_the_baseline(self):
        xs = [Number('1e-07'), Number('1.000000'), Number('-0.0')]
        whole = Diagram().apply([self.add(0, [self.point_run(1, xs, True)])])
        batches = Diagram().apply([self.add(0, [self.point_run(1, xs[:1], True)]),
                                   self.add(1, [self.point_run(2, xs[1:])])])
        self.assertEqual(whole.text(), batches.text())
        self.assertIn(b'x=1e-07', whole.text())
        self.assertIn(b'x=1.000000', whole.text())
        self.assertIn(b'x=-0.0', whole.text())

    def test_labels_low_values_and_restart_survive_reset(self):
        run = self.point_run(1, [Number('1'), Number('2')], True)
        run.update(y2=[Number('-1'), Number('-2')], lab=[[1, 4, 'HB']], **{'from': 3})
        dg = Diagram().apply([self.add(0, [run])])
        self.assertEqual(dg.points[1]['sym'], 'HB')
        self.assertEqual(dg.points[1]['lab'], 4)
        self.assertEqual(dg.points[0]['from'], 3)
        self.assertEqual(dg.points[1]['from'], 0)
        self.assertEqual(dg.points[1]['y2'], '-2')
        dg.apply([dict(ev='diagram', op='reset', view=0, keep=1),
                  self.add(1, [self.point_run(2, [Number('3')])])])
        self.assertEqual(len(dg.points), 2)
        self.assertEqual(dg.points[1]['x'], '3')

    def test_new_protocol_data_is_not_silently_dropped(self):
        run = self.point_run(1, [Number('1')])
        run['future_numeric_field'] = 0
        with self.assertRaisesRegex(ValueError, 'unrecorded diagram fields'):
            Diagram().apply([self.add(0, [run])])
        with self.assertRaisesRegex(ValueError, 'noncontiguous'):
            Diagram().apply([self.add(1, [self.point_run(1, [Number('1')])])])

    def test_difference_names_both_lines_and_missing_endpoint(self):
        self.assertEqual(difference('model', b'a\nb\n', b'a\nc\n'),
                         'model: first differing point 2\n  want: b\n  got:  c')
        self.assertIn('point 3\n  want: (end of diagram)\n  got:  c',
                      difference('model', b'a\nb\n', b'a\nb\nc\n'))

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
