"""Regression checks for the baseline check's acceptance and text contract."""
import copy
import json
import math
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import numpy as np

import autocompare
from autocompare import BASE, Diagram, Matcher, check_size, compare_curve, compare_references, compare_run, difference, fit_segments, labelled, numbered_points, point_count, read_description, read_reference, reference_spectrum, restore_start, table_row


class AutoCompare(unittest.TestCase):
    def point_run(self, pt, n, lab=()):
        return dict(br=1, pt=pt, x=[0.0] * n, y=[0.0] * n, lab=list(lab))

    def add(self, start, runs):
        return dict(ev='diagram', op='add', view='diagram_1', **{'from': start}, runs=runs)

    def test_batching_does_not_change_the_points(self):
        whole = Diagram().apply([self.add(0, [self.point_run(1, 3, [[1, 4, 'hopf']])])])
        batches = Diagram().apply([self.add(0, [self.point_run(1, 1)]),
                                   self.add(1, [self.point_run(2, 2, [[0, 4, 'hopf']])])])
        self.assertEqual(whole.points, batches.points)
        self.assertEqual(whole.points[1], dict(lab=4, sym='HB'))
        self.assertEqual(whole.points[0], dict(lab=0, sym=''))

    def test_points_sent_without_data_are_counted_and_checked_against_the_saved_diagram(self):
        saved = b'branch,1,0.5\nbranch,2,0.6\ncalc,ibr,lab\nx,1,0\nx,1,3\nx,2,0\n'
        self.assertEqual(point_count(saved), 3)
        self.assertEqual(point_count(b'calc,ibr,lab\n'), 0)
        diagram = Diagram()
        diagram.points, diagram.unread = [dict(lab=0, sym='')], 2
        self.assertEqual(diagram.size, 3)
        check_size(3, diagram)
        with self.assertRaisesRegex(RuntimeError, 'holds 4 points, the branches ended at 3'):
            check_size(4, diagram)

    def test_a_cycle_curve_is_the_only_run_whose_last_point_number_is_not_its_count(self):
        self.assertTrue(numbered_points(dict(kind='equilibrium', start={})))
        self.assertTrue(numbered_points(dict(kind='two_parameter', start=dict(type='HB'))))
        self.assertFalse(numbered_points(dict(kind='two_parameter', start=dict(type='PD'))))

    def test_reset_keeps_the_points_before(self):
        dg = Diagram().apply([self.add(0, [self.point_run(1, 2, [[1, 4, 'hopf']])])])
        dg.apply([dict(ev='diagram', op='reset', view='diagram_1', keep=1), self.add(1, [self.point_run(2, 1)])])
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
        return [self.row('end', 0.0, 0.0)] + [self.row('regular', 0.2 * i, 0.4 * i, ev=(0.1, 0.0)) for i in range(1, 5)] +             [self.row('fold', 1.0, 2.0, ev=(1.0, 0.0)), self.row('regular', 1.2, 2.4, ev=(-0.1, 0.0))]

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
        rows = [self.row('end', 1.0, 0.86), self.row('regular', 1.1, 0.85), self.row('end', 3.0, 0.7)]
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
        rows=[self.row(name,0.,0.) for name in ("fold_neimark_sacker", "fold_flip", "flip_neimark_sacker", "fold_flip", "fold_neimark_sacker", "flip_neimark_sacker", "double_neimark_sacker", "resonance_1_1", "resonance_1_2", "resonance_1_3", "resonance_1_4")]
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


def plain_project(point, polyline, tolerances):
    """Fit every segment, the first of the best wins: the definition `Matcher` must give the same answer as."""
    distance, residual = fit_segments(point, polyline[:-1], polyline[1:], tolerances)
    i = int(np.argmin(distance))
    return i, residual[i].tolist()


def brute_project(point, polyline, tolerances):
    """project() one segment at a time in plain Python: the definition its vectorised form must equal bit for bit."""
    best = None
    for i, bounds in enumerate(tolerances):
        a, b = polyline[i], polyline[i + 1]
        along = [(b[k] - a[k]) / bounds[k] for k in range(3)]
        length = sum(x * x for x in along)
        t = 0.0 if length == 0 else min(1.0, max(0.0, sum((point[k] - a[k]) / bounds[k] * along[k] for k in range(3)) / length))
        lo, hi = 0.0, 1.0
        for k in range(3):
            delta = b[k] - a[k]
            if delta:
                ends = [(point[k] - a[k] + sign * bounds[k]) / delta for sign in (-1, 1)]
                lo, hi = max(lo, min(ends)), min(hi, max(ends))
            elif abs(point[k] - a[k]) > bounds[k]:
                lo, hi = 1.0, 0.0
        if lo <= hi:
            t = min(hi, max(lo, t))
        residual = [point[k] - (a[k] + t * (b[k] - a[k])) for k in range(3)]
        distance = max(abs(residual[k]) / bounds[k] for k in range(3))
        if best is None or distance < best[0]:
            best = distance, i, residual
    return best[1], best[2]


class Projection(unittest.TestCase):
    def test_the_vectorised_projection_equals_the_plain_loop_on_a_closed_curve_with_laps_and_ties(self):
        lap = [(math.cos(2 * math.pi * i / 12), math.sin(2 * math.pi * i / 12), 0.1 * (i % 3)) for i in range(12)]
        # three identical laps (equal distances: the lowest segment wins), one with a repeated vertex (a segment of length 0),
        # one straight run along an axis (a zero step in two coordinates)
        polyline = lap * 3 + [lap[0], lap[0], (2.0, 0.0, 0.0), (3.0, 0.0, 0.0)]
        tolerances = [[0.02 * (1 + (i + k) % 4) for k in range(3)] for i in range(len(polyline) - 1)]
        tolerances[3] = tolerances[3 + 12] = tolerances[3 + 24] = [0.05, 0.03, 0.04]
        points = [lap[5], (0.3, 0.2, 0.1), (1.0, 0.0, 0.0), (2.5, 0.01, 0.0), (2.5, 0.5, 0.0), (-3.0, 2.0, 1.0),
                  (0.5 * (lap[2][0] + lap[3][0]), 0.5 * (lap[2][1] + lap[3][1]), 0.1)]
        for point in points:
            self.assertEqual(plain_project(np.array(point), np.array(polyline), np.array(tolerances)), brute_project(point, polyline, tolerances))
        # a sample on a vertex shared by identical laps goes to the first lap
        self.assertEqual(plain_project(np.array(lap[5]), np.array(polyline), np.array(tolerances))[0], 4)


    def test_searching_for_the_segment_gives_the_one_of_fitting_them_all_on_random_curves_with_ties_and_degenerate_segments(self):
        rng = np.random.default_rng(463)
        with mock.patch.object(autocompare, 'INDEXED_FROM', 0):
            for trial in range(300):
                # coordinates on a coarse grid (exact ties, repeated vertices, steps of zero in a coordinate), laps repeated,
                # bounds from a few values, a scale from 1e-9 to 1e9 (the rounding allowance against the coordinates' size)
                scale = 10.0 ** rng.integers(-9, 10)
                walk = np.cumsum(rng.integers(-2, 3, size=(int(rng.integers(2, 60)), 3)), axis=0) * scale
                polyline = np.vstack([walk] * int(rng.integers(1, 4)) + [walk[:1]])
                tolerances = rng.choice([0.4, 0.5, 1.0, 1.5, 3.0], size=(len(polyline) - 1, 3)) * scale
                tolerances[::7] = tolerances[0]
                matcher = Matcher(polyline)
                self.assertTrue(matcher.searched)
                for _ in range(20):
                    point = np.array(rng.choice(polyline) if rng.random() < 0.5 else rng.integers(-8, 9, size=3) * scale * 0.5, dtype=float)
                    i, residual = plain_project(point, polyline, tolerances)
                    # the bounds as the caller gives them: of some rows, of the largest of each coordinate for None
                    got = matcher.project(point, lambda rows: tolerances.max(axis=0, keepdims=True) if rows is None else tolerances[rows])
                    self.assertEqual(got[:2], (i, residual), (trial, point))
                    self.assertTrue((got[2] == tolerances[i]).all())
        self.assertFalse(Matcher(np.array([[0.0, 0.0, 0.0], [math.inf, 1.0, 1.0], [2.0, 2.0, 2.0]])).searched)
        self.assertFalse(Matcher(walk).searched)  # short: fitted whole


class Curves(unittest.TestCase):
    AUTO = dict(dsmax=0.1, epsl=1e-4, epsu=1e-4, epss=1e-4, parmin=0.05, parmax=1.4)
    WIDE = dict(AUTO, parmin=0.0, parmax=3.0)

    @staticmethod
    def rows(vertices):
        return [dict(continued_parameter=str(a), continued_parameter2=str(b), norm=str(n), itp='regular') for a, b, n in vertices]

    @staticmethod
    def parabola(steps, shift=0.0):
        """a smooth curve: the second parameter a + a^2, the norm 2a, a every 0.05 from `shift`"""
        return [(a, a + a * a, 2 * a) for a in (shift + 0.05 * i for i in range(steps))]

    def test_a_curve_gives_the_same_differences_and_measures_whether_or_not_its_segments_are_searched(self):
        rng = np.random.default_rng(526)
        for trial in range(40):
            ours = [tuple(v) for v in np.cumsum(rng.integers(-1, 4, size=(int(rng.integers(2, 50)), 3)), axis=0) * 0.01]
            ours = ours * int(rng.integers(1, 3))
            ref = [list(v) for v in np.array(ours)[rng.permutation(len(ours))[:int(rng.integers(2, len(ours) + 1))]] + rng.integers(-1, 2, size=3) * 1e-4]
            if trial % 3 == 0:
                ref = [list(v) for v in ours[::-1]]
            for auto in (self.WIDE, dict(self.WIDE, nmax=len(ours)), dict(self.WIDE, parmin=0.02, parmax=0.1)):
                for digits in (None, 6):
                    whole = compare_curve(self.rows(ours), auto, ref, 'm', digits)
                    with mock.patch.object(autocompare, 'INDEXED_FROM', 0):
                        self.assertEqual(compare_curve(self.rows(ours), auto, ref, 'm', digits), whole, trial)

    def test_a_curve_matches_at_other_steps_and_differs_beyond_the_tolerance(self):
        ours = self.parabola(40)
        ref = [list(v) for v in self.parabola(30, 0.013)]  # other step positions on the same curve
        diffs, measures = compare_curve(self.rows(ours), self.AUTO, ref, 'm')
        self.assertEqual(diffs, [])
        self.assertEqual([m[0] for m in measures], ['curve par', 'curve par2', 'curve norm'])
        ref[10][1] += 0.05
        diffs, _ = compare_curve(self.rows(ours), self.AUTO, ref, 'm')
        self.assertTrue(any('curve par2: 1 of 30 reference samples' in d and 'the first, sample 11' in d for d in diffs), diffs)

    def test_xppaut_printed_unit_does_not_hide_a_larger_curve_error(self):
        # A constant norm has zero chord error: %14.5E alone loses these digits.
        ours = [(x, 0.3, 0.41826665768) for x in (0.2, 0.4, 0.8)]
        ref = [[x, y, float('%14.5E' % n)] for x, y, n in ours]
        auto = dict(self.AUTO, epsl=1e-9, epsu=1e-9)
        self.assertTrue(compare_curve(self.rows(ours), auto, ref, 'rounded')[0])
        self.assertEqual(compare_curve(self.rows(ours), auto, ref, 'rounded', printed_digits=6)[0], [])
        for point in ref:
            point[2] += 1e-6  # more than half a printed unit; the constant offset has no chord error
        self.assertTrue(compare_curve(self.rows(ours), auto, ref, 'wrong', printed_digits=6)[0])

    def test_xppaut_rounding_uses_the_reference_exponent(self):
        # Crossing a decimal power must not borrow the unrounded side's tenfold larger unit.
        ours = [(x, 0.3, 0.1000002) for x in (0.2, 0.4, 0.8)]
        ref = [[x, y, 0.0999999] for x, y, _ in ours]
        auto = dict(self.AUTO, epsl=1e-9, epsu=1e-9)
        self.assertTrue(compare_curve(self.rows(ours), auto, ref, 'boundary', printed_digits=6)[0])

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
        rows[0]['itp'] = 'cycle_branch_point_from_torus'  # BP on a torus curve
        ref = [dict(type='BP', point=1, par=0., par2=0., norm=0., period=None, values=[])]
        self.assertEqual(compare_run(rows, self.AUTO, 'two_parameter', ref, 'cycle')[0], [])

    def test_a_codim_two_point_of_the_reference_is_a_difference_until_traced(self):
        rows = [dict(itp='regular', continued_parameter=str(a), norm=str(a), per='0.0', evr1='0', evi1='0') for a in range(5)]
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
                         lambda r: r['runs'][0].update(curve=sample[:1]), lambda r: r['runs'][1]['points'].append(dict(type='XX')),
                         lambda r: r['runs'][1].update(parameter='p0')):
                bad = copy.deepcopy(good)
                edit(bad)
                path.write_text(json.dumps(bad), encoding='utf-8')
                with self.assertRaises(ValueError):
                    read_reference(path, d)
            path.write_text(json.dumps(good), encoding='utf-8')
            self.assertEqual(read_reference(path, d)['runs'][1]['curve'], sample)
            # a one-parameter run may carry its branch samples too (W439's whole-branch gate)
            branch = copy.deepcopy(good)
            branch['runs'][0]['curve'] = sample
            path.write_text(json.dumps(branch), encoding='utf-8')
            self.assertEqual(read_reference(path, d)['runs'][0]['curve'], sample)

    def test_a_start_is_grabbed_from_the_points_its_run_left(self):
        dg = Diagram()
        dg.apply([dict(ev='diagram', op='add', view='diagram_1', **{'from': 0},
                       runs=[dict(br=1, pt=1, x=[0.0] * 3, y=[0.0] * 3, lab=[[1, 4, 'hopf']])])])
        dg.runs['steady'] = [dict(p) for p in dg.points]
        # a curve on other axes redraws the diagram and labels only what it plots
        dg.apply([dict(ev='diagram', op='add', view='diagram_1', **{'from': 0},
                       runs=[dict(br=1, pt=1, x=[0.0] * 3, y=[0.0] * 3, lab=[])])])
        self.assertEqual([p['sym'] for p in dg.points], ['', '', ''])
        self.assertEqual([p['sym'] for p in dg.runs['steady']], ['', 'HB', ''])


class Arms(unittest.TestCase):
    AUTO = dict(dsmax=0.1, epsl=1e-4, epsu=1e-4, epss=1e-4, parmin=-3.0, parmax=3.0)

    @staticmethod
    def arm(sign):
        """one arm of a curve from the origin: the second parameter a + a^2, a every 0.05 on the side `sign`"""
        return [(sign * 0.05 * i, sign * 0.05 * i + (0.05 * i) ** 2, 2 * sign * 0.05 * i) for i in range(20)]

    def compare(self, theirs_up, theirs_down, kind='two_parameter'):
        start = dict(run='steady', type='HB', index=1)
        d = dict(runs=[dict(name='up', kind=kind, start=start, auto=self.AUTO), dict(name='down', kind=kind, start=start, auto=self.AUTO)])
        rows = {n: Curves.rows(v) for n, v in (('up', self.arm(1)), ('down', self.arm(-1)))}
        ref = {'.auto07p': dict(runs=[dict(name='up', points=[], curve=[list(v) for v in theirs_up]),
                                      dict(name='down', points=[], curve=[list(v) for v in theirs_down])])}
        notes = []
        with mock.patch('autocompare.stored_rows', return_value=rows):
            diffs, _ = compare_references('T', d, b'', {}, ref, notes)
        return diffs, notes

    def test_the_two_arms_of_one_curve_may_be_named_the_other_way_round(self):
        diffs, notes = self.compare(self.arm(1), self.arm(-1))
        self.assertEqual((diffs, notes), ([], []))
        diffs, notes = self.compare(self.arm(-1), self.arm(1))
        self.assertEqual(diffs, [])
        self.assertEqual(len(notes), 2, notes)
        self.assertIn('its up overlaps our down', notes[0])

    def test_a_curve_that_is_neither_arm_is_still_a_difference(self):
        bent = [(a, b + 0.05, n) for a, b, n in self.arm(1)]
        diffs, _ = self.compare(bent, self.arm(-1))
        self.assertTrue(diffs)

    def test_a_skipped_arm_is_not_paired(self):
        # henon_rot_tr's down is skipped: no rows of ours, and the reference keeps only up (read_reference)
        start = dict(run='steady', type='HB', index=1)
        d = dict(runs=[dict(name='up', kind='two_parameter', start=start, auto=self.AUTO),
                       dict(name='down', kind='two_parameter', start=start, auto=self.AUTO, skip='traced')])
        ref = {'.auto07p': dict(runs=[dict(name='up', points=[], curve=[list(v) for v in self.arm(1)])])}
        with mock.patch('autocompare.stored_rows', return_value={'up': Curves.rows(self.arm(1))}):
            diffs, _ = compare_references('T', d, b'', {}, ref, [])
        self.assertEqual(diffs, [])

    def test_runs_that_are_not_curves_are_paired_by_name_only(self):
        diffs, notes = self.compare(self.arm(-1), self.arm(1), kind='equilibrium')
        self.assertEqual(notes, [])


class Stability(unittest.TestCase):
    # AUTO-07p's d-file: the located HB at point 3591, then point 13590, printed 3591 again (modulo 9999)
    D = '''   1  3591         Eigenvalue  1:   1.00000E-01  -2.00000E+00
   1  3591         Eigenvalue  2:   1.00000E-01   2.00000E+00
 ==> Location of special point :  Iteration   0  Step size =   4.14209E-01
   1  3591   1        1.64094E+04   1.97136E+01
   1  3591         Eigenvalue  1:  -3.19686E-09  -2.91214E+00
   1  3591         Eigenvalue  2:  -3.19686E-09   2.91214E+00
   1  3591         Fold Function:   1.00000E+00
  BR    PT  TY  LAB    PAR(1)        L2-NORM         U(1)
   1 -3591  HB    2   1.64094E+04   1.97136E+01  -1.96243E+01
   1  3591         Eigenvalue  1:   8.39045E-01  -2.34610E+00
   1  3591         Eigenvalue  2:   8.39045E-01   2.34610E+00
   1  3591        0   6.41041E+03   2.22004E+01  -2.21265E+01
'''

    def test_a_special_point_keeps_its_own_block_when_its_number_is_printed_again(self):
        from auto_reference import stability
        self.assertEqual(stability(self.D), {(1, 2): ('Eigenvalue', [[-3.19686e-09, -2.91214], [-3.19686e-09, 2.91214]])})


if __name__ == '__main__':
    unittest.main()
