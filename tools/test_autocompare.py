"""Regression checks for the baseline check's acceptance and text contract."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from autocompare import BASE, Diagram, compare_curve, compare_run, difference, labelled, read_description, read_reference, reference_spectrum, restore_start, table_row


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
        for edit in ('unknown', 'zero_step', 'future_run', 'nan', 'skip_and_runs', 'outside', 'step_alone', 'zero_settle_step'):
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
            elif edit == 'step_alone':  # the step of a settle the start does not have
                d['runs'][0]['start'] = dict(state='current', step=0.001)
            elif edit == 'zero_settle_step':
                d['runs'][0]['start'] = dict(state='current', settle=10, step=0)
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

    def test_a_table_row_lists_the_stored_points_and_each_references_result(self):
        d = dict(runs=[dict(name='steady', kind='equilibrium'), dict(name='skipped', kind='periodic', skip='x')])
        rows = {'steady': self.rows()}
        self.assertEqual([s for s, _ in labelled(rows['steady'], 'equilibrium')], ['EP', 'LP'])
        measures = [('.auto07p', 'steady', 'par', 'LP 6', 1e-7, 1e-4), ('.auto07p', 'steady', 'par', 'EP 1', 0.0, 1e-4),
                    ('.auto07p', 'steady', 'norm', 'LP 6', 2e-7, 1e-4)]
        self.assertEqual(table_row('m', d, rows, ['.auto07p'], measures, []),
                         '| m | steady: EP LP | agrees (2 points; par 1e-07, norm 2e-07) | not run |')
        self.assertEqual(table_row('m', d, rows, ['.auto07p'], measures, ['m AUTO-07p/steady x']).split(' | ')[2],
                         'differs: 1 (2 points; par 1e-07, norm 2e-07)')

    def test_a_homcont_runs_first_point_is_its_start(self):
        rows = [self.row(9, 1.0, 0.86), self.row(0, 1.1, 0.85), self.row(9, 3.0, 0.7)]
        for kind in ('homoclinic', 'heteroclinic', 'bvp'):
            self.assertEqual(labelled(rows, kind), [('EP', 0)])
        self.assertEqual(labelled(rows, 'periodic'), [])

    def test_a_later_run_from_the_current_state_starts_from_the_descriptions(self):
        class Server:
            def __init__(self):
                self.sent = []
            def send(self, **command):
                self.sent.append(command)
            def collect(self, done, timeout):
                return [dict(ev='idle')], dict(ev='idle')
        values = [dict(kind='par', name='q', value=1.0)]
        current, grabbed = dict(start=dict(state='current')), dict(start=dict(run='steady', type='EP', index=1))
        for r, ranges, sent in ((current, {}, []), (grabbed, {'steady': (0, 3)}, []),
                                (current, {'steady': (0, 3)}, [dict(cmd='set', values=values)])):
            s = Server()
            restore_start(s, r, values, ranges)
            self.assertEqual(s.sent, sent)

    def test_map_intersections_use_full_codes_before_the_last_digit(self):
        rows=[self.row(code,0.,0.) for code in (23,28,77,78,83,87,88,-25,-76,-87,-88)]
        for row in rows: row['lab']='1'
        self.assertEqual([kind for kind,_ in labelled(rows,'two_parameter')],
                         ['LTR','LPD','PTR','LPD','LTR','PTR','TTR','R1','R2','R3','R4'])

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


class Curves(unittest.TestCase):
    AUTO = dict(dsmax=0.1, epsl=1e-4, epsu=1e-4, epss=1e-4, parmin=0.05, parmax=1.4)
    WIDE = dict(AUTO, parmin=0.0, parmax=3.0)

    @staticmethod
    def rows(vertices):
        return [dict(continued_parameter=str(a), continued_parameter2=str(b), norm=str(n), itp='0') for a, b, n in vertices]

    @staticmethod
    def parabola(steps, shift=0.0):
        """a smooth curve: the second parameter a + a^2, the norm 2a, a every 0.05 from `shift`"""
        return [(a, a + a * a, 2 * a) for a in (shift + 0.05 * i for i in range(steps))]

    def test_a_curve_matches_at_other_steps_and_differs_beyond_the_tolerance(self):
        ours = self.parabola(40)
        ref = [list(v) for v in self.parabola(30, 0.013)]  # other step positions on the same curve
        diffs, measures = compare_curve(self.rows(ours), self.AUTO, ref, 'm')
        self.assertEqual(diffs, [])
        self.assertEqual([m[0] for m in measures], ['curve par', 'curve par2', 'curve norm'])
        ref[10][1] += 0.05
        diffs, _ = compare_curve(self.rows(ours), self.AUTO, ref, 'm')
        self.assertTrue(any('curve par2: 1 of 30 reference samples' in d and 'the first, sample 11' in d for d in diffs), diffs)

    def test_straight_curve_does_not_project_on_newton_roundoff(self):
        ours = [(0.3 + (i % 2) * 1e-8, 0.05 * i, 0.5 + (i % 3) * 1e-8) for i in range(41)]
        ref = [(0.3 - (i % 2) * 1e-8, 0.1 * i, 0.5) for i in range(21)]
        self.assertEqual(compare_curve(self.rows(ours), self.WIDE, ref, 'line')[0], [])
        ref[10] = (0.31, 1., 0.5)
        self.assertTrue(compare_curve(self.rows(ours), self.WIDE, ref, 'line')[0])

    def test_closed_curve_matches_both_arcs_with_different_steps(self):
        import math
        def circle(count):
            return [(1 + 0.1 * math.cos(2 * math.pi * i / count),
                     1 + 0.1 * math.sin(2 * math.pi * i / count), 0.5 + (i % 2) * 1e-9)
                    for i in range(count + 1)]
        self.assertEqual(compare_curve(self.rows(circle(120)), self.WIDE, circle(160), 'lens')[0], [])
        wrong = [(a, b + 0.01, n) for a, b, n in circle(160)]
        self.assertTrue(compare_curve(self.rows(circle(120)), self.WIDE, wrong, 'lens')[0])

    def test_a_curve_that_stops_short_or_runs_on_is_a_difference(self):
        ours = self.parabola(40)
        for ref in (self.parabola(10), self.parabola(60)):
            diffs, _ = compare_curve(self.rows(ours), self.WIDE, [list(v) for v in ref], 'm')
            self.assertTrue(diffs, len(ref))

    def test_two_runs_of_nmax_steps_are_compared_where_both_went(self):
        ours = self.parabola(40)
        longer = [(a, a + a * a, 2 * a) for a in (0.051 * i for i in range(40))]  # longer steps, same curve
        nmax = dict(self.WIDE, nmax=40)
        self.assertTrue(compare_curve(self.rows(ours), self.WIDE, [list(v) for v in longer], 'm')[0])
        self.assertEqual(compare_curve(self.rows(ours), nmax, [list(v) for v in longer], 'm')[0], [])
        self.assertEqual(compare_curve(self.rows(longer), nmax, [list(v) for v in ours], 'm')[0], [])
        # a run that stopped before its NMX-th point, or a curve that leaves the other before its end, still differs
        self.assertTrue(compare_curve(self.rows(ours[:30]), nmax, [list(v) for v in longer], 'm')[0])
        bent = [(a, b + (0.02 if a > 1.5 else 0), n) for a, b, n in longer]
        self.assertTrue(compare_curve(self.rows(ours), nmax, [list(v) for v in bent], 'm')[0])

    def test_only_samples_inside_the_first_parameters_bounds_are_compared(self):
        ours = self.parabola(40)
        ref = [list(v) for v in self.parabola(60)]
        self.assertTrue(compare_curve(self.rows(ours), self.WIDE, ref, 'm')[0])
        diffs, _ = compare_curve(self.rows(ours), dict(self.WIDE, parmax=1.9), ref, 'm')
        self.assertEqual(diffs, [])

    def test_the_two_arcs_of_a_cusp_are_told_apart_by_their_norm(self):
        upper = [(0.1 * i, 0.1 * i, 1.0 + 0.01 * i) for i in range(20)]
        lower = [(0.1 * i, 0.1 * i, 0.0 + 0.01 * i) for i in range(20)]
        diffs, _ = compare_curve(self.rows(upper + lower[::-1]), self.WIDE, [list(v) for v in upper + lower[::-1]], 'm')
        self.assertEqual(diffs, [])
        diffs, _ = compare_curve(self.rows(upper), self.WIDE, [list(v) for v in lower], 'm')
        self.assertTrue(diffs)

    def test_start_vertex_does_not_hide_a_valid_coarse_chord_on_a_later_lap(self):
        # A closed lens revisits its left tip with a coarser step. The sample
        # near the starting vertex is inside the later chord's curvature bounds.
        ref = [(0., 0., 1.), (.02, .1, 1.), (.2, .2, 1.), (.3, 0., 1.),
               (.2, -.2, 1.), (.02, -.1, 1.), (.02, .1, 1.), (.2, .2, 1.)]
        ours = [(0., -.01, 1.), *ref]
        self.assertEqual(compare_curve(self.rows(ours), self.WIDE, ref, 'closed lens')[0], [])

    def test_cycle_branch_point_retains_its_composite_start(self):
        rows = self.rows([(0., 0., 0.), (1., 1., 1.)])
        rows[0]['itp'] = '86'  # BP on a torus curve
        ref = [dict(type='BP', point=1, par=0., par2=0., norm=0., period=None, values=[])]
        self.assertEqual(compare_run(rows, self.AUTO, 'two_parameter', ref, 'cycle')[0], [])

    def test_a_codim_two_point_of_the_reference_is_a_difference_until_traced(self):
        rows = [dict(itp='0', continued_parameter=str(a), norm=str(a), per='0.0', evr1='0', evi1='0') for a in range(5)]
        ref = [dict(type='BT', branch=1, label=2, point=2, par=1.0, par2=1.0, norm=1.0, period=None, spectrum='', values=[])]
        diffs, _ = compare_run(rows, self.AUTO, 'two_parameter', ref, 'm')
        self.assertIn("special points ['BT'], ours []", diffs[0])
        known = [dict(side='reference', type='BT', index=1, cause='traced')]
        self.assertEqual(compare_run(rows, self.AUTO, 'two_parameter', ref, 'm', known)[0], [])

    def test_a_curve_reference_is_read_strictly(self):
        d = json.loads((BASE / 'tstauto_ab_fold.json').read_text())
        sample = [[0.1, 14.0, 1.5], [0.2, 13.0, 1.6]]
        good = dict(format=1, source='x', model=d['model'], runs=[
            dict(name='steady', parameter='p0', points=[]),
            dict(name='fold', parameters=['p0', 'p1'], points=[], curve=sample)])
        with tempfile.TemporaryDirectory(prefix='xppautocompare-test-') as tmp:
            path = Path(tmp) / 'ab.auto07p'
            for edit in (lambda r: r['runs'][1].pop('curve'), lambda r: r['runs'][1].update(parameters=['p0', 'p2']),
                         lambda r: r['runs'][1].update(curve=[[0.1, 14.0]] * 2), lambda r: r['runs'][1].update(curve=sample[:1]),
                         lambda r: r['runs'][0].update(curve=sample), lambda r: r['runs'][1]['points'].append(dict(type='XX')),
                         lambda r: r['runs'][1].update(parameter='p0')):
                bad = copy.deepcopy(good)
                edit(bad)
                path.write_text(json.dumps(bad), encoding='utf-8')
                with self.assertRaises(ValueError):
                    read_reference(path, d)
            path.write_text(json.dumps(good), encoding='utf-8')
            self.assertEqual(read_reference(path, d)['runs'][1]['curve'], sample)

    def test_a_start_is_grabbed_from_the_points_its_run_left(self):
        dg = Diagram()
        dg.apply([dict(ev='diagram', op='add', view=0, **{'from': 0},
                       runs=[dict(br=1, pt=1, x=[0.0] * 3, y=[0.0] * 3, lab=[[1, 4, 'HB']])])])
        dg.runs['steady'] = [dict(p) for p in dg.points]
        # a curve on other axes redraws the diagram and labels only what it plots
        dg.apply([dict(ev='diagram', op='add', view=0, **{'from': 0},
                       runs=[dict(br=1, pt=1, x=[0.0] * 3, y=[0.0] * 3, lab=[])])])
        self.assertEqual([p['sym'] for p in dg.points], ['', '', ''])
        self.assertEqual([p['sym'] for p in dg.runs['steady']], ['', 'HB', ''])


if __name__ == '__main__':
    unittest.main()
