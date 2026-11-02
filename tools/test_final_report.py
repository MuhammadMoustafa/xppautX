#!/usr/bin/env -S uv run --locked python
"""W589: interpolation and profile checks, without benchmark processes."""
from pathlib import Path
import json
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'paper/benchmarks/comparison'))
from final_report import (compute, continuation_finding, equal_work_matches, gate_cell, interpolate, jobs_text, memory_ratios, memory_text,
                          normalized_view, profile, profile_spec, threads_text)


class MatchedAccuracy(unittest.TestCase):
    def test_power_law(self):
        points = [dict(error=1., time=100.), dict(error=.01, time=10000.)]
        self.assertAlmostEqual(interpolate(points, .1), 1000.)
        self.assertEqual(interpolate(points, 1.), 100.)
        self.assertEqual(interpolate(points, .01), 10000.)

    def test_no_extrapolation_or_zero_error(self):
        points = [dict(error=1., time=100.), dict(error=.01, time=10000.)]
        self.assertIsNone(interpolate(points, 10.))
        self.assertIsNone(interpolate(points, .001))
        self.assertIsNone(interpolate(points, 0.))
        self.assertIsNone(interpolate([], 1.))
        self.assertIsNone(interpolate([dict(error=0., time=1.), points[0]], .1))

    def test_single_and_duplicate_rungs(self):
        points = [dict(error=.1, time=4.), dict(error=.1, time=2.)]
        self.assertEqual(interpolate(points, .1), 2.)
        self.assertIsNone(interpolate(points[:1], .01))

    def test_nonmonotonic_curve(self):
        points = [dict(error=1., time=1.), dict(error=.01, time=100.), dict(error=1., time=10000.)]
        self.assertAlmostEqual(interpolate(points, .1), 10.)

    def test_profile_denominator_includes_failures(self):
        self.assertEqual(profile([1., 2., None, 4.], [.5, 1., 2., 4., 100.]), [0., .25, .5, .75, .75])
        self.assertEqual(profile([], [1.]), [])
        self.assertEqual(profile([None], [1., 100.]), [0., 0.])
        self.assertEqual(profile([.5, 1.], [.5, 1.]), [.5, 1.])

    def test_equal_work_gate_uses_timed_means_without_a_ladder(self):
        baseline = dict(equal_work=True, time=2., error=100.)
        other = dict(equal_work=True, time=6., error=.0001)
        row = dict(study='screen', workload='case', model='case', method='runge_kutta',
                   cells={'xppautx': dict(row=baseline, reason=''), 'xppaut': dict(row=other, reason='')})
        result = equal_work_matches([row])
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]['ratio'], 3.)
        self.assertEqual(result[0]['status'], 'matched by equal-work gate')

    def test_equal_work_failures_stay_in_denominator(self):
        def row(study, name, other):
            return dict(study=study, workload=name, model=name, method='euler',
                        cells={'xppautx': dict(row=dict(equal_work=True, time=2.), reason=''),
                               'xppaut': dict(row=other, reason='aborted' if other is None else ''),
                               'python': dict(row=None, reason='never reached reference')})
        results = equal_work_matches([row('screen', 'complete', dict(equal_work=True, time=4.)),
                                      row('screen', 'aborted', None),
                                      row('screen', 'rejected', dict(equal_work=False, time=1.)),
                                      row('python', 'never', None)])
        self.assertEqual([r['ratio'] for r in results], [2., None, None, None, None])
        self.assertEqual(profile([r['ratio'] for r in results], [1., 2., 100.]), [0., .2, .2])
        self.assertTrue(all(r['status'] == 'does not reach' for r in results[1:]))

    def test_profile_draws_other_programs_and_reference_without_bands(self):
        records = [dict(program='xppaut', ratio=2.), dict(program='xppaut', ratio=None)]
        view = profile_spec(records, 'test')
        self.assertTrue(view['reference'])
        self.assertEqual(len(view['traces']), 1)
        self.assertEqual(view['traces'][0]['name'], 'XPPAUT 8.0 (1 of 2 reach)')
        self.assertIsNone(view['traces'][0]['low'])
        self.assertIsNone(view['traces'][0]['high'])
        self.assertNotIn('xppautx', [t['program'] for t in view['traces']])

    def test_normalized_quantiles_require_three_curves_and_keep_gaps(self):
        curves = []
        for i, (lo, hi) in enumerate([(1., 2.), (1., 2.), (1., 2.), (10., 20.), (10., 20.)]):
            points = [dict(error=x, time=1., low=.9, high=1.1) for x in (lo, hi)]
            curves.append(dict(workload=str(i), points=points, normalized=points))
        view = normalized_view(curves, 'xppautx')
        population = view['population']
        for x, n, y in zip(population['grid'], population['contributors'], population['median']):
            if 1. <= x <= 2.:
                self.assertEqual(n, 3)
                self.assertEqual(y, 1.)
            else:
                self.assertIsNone(y)
        for trace in view['traces']:
            if trace.get('population'):
                self.assertGreaterEqual(min(trace['x']), 1.)
                self.assertLessEqual(max(trace['x']), 2.)


class GateAgreement(unittest.TestCase):
    def gate(self, **changes):
        return dict(passed=True, differences=[], measures=[], unavailable=[], traced_differences=[]) | changes

    def test_passed_gate_does_not_invent_extras(self):
        gate = self.gate(measures=[['.xppaut', 'cycles', 'par', 'HB 3', 0., .01],
                                  ['.xppaut', 'cycles', 'norm', 'HB 3', 0., .01],
                                  ['.xppaut', 'cycles', 'curve norm', 'ours sample 12', 0., .01]])
        cell = gate_cell('lrz_cycles/adaptive/cycles', 'fd', 'xppaut', gate, 'saved')
        self.assertEqual(cell['status'], 'found')
        self.assertEqual(cell['found'], 1)
        self.assertEqual(cell['differences'], [])

    def test_traces_belong_to_the_run_and_reference(self):
        traces = [dict(reference='.xppaut', run='cycles', cause='recorded BP cause'),
                  dict(reference='.auto07p', run='other', cause='another run')]
        cell = gate_cell('model/adaptive/cycles', 'fd', 'xppaut', self.gate(traced_differences=traces), 'saved')
        self.assertEqual(cell['status'], 'traced')
        self.assertEqual([t['cause'] for t in cell['traced']], ['recorded BP cause'])
        exact = gate_cell('model/adaptive/cycles', 'exact', 'auto07p', self.gate(traced_differences=[
            dict(reference='.converged', run='cycles', cause='exact-node cause')]), 'converged#exact_pilots')
        self.assertEqual(exact['status'], 'traced')
        self.assertEqual(exact['traced'][0]['cause'], 'exact-node cause')

    def test_designed_skip_is_unavailable_with_reason(self):
        cell = gate_cell('hen_tr/adaptive/up', 'fd', 'xppaut', self.gate(), 'saved', 'finding 71: designed skip')
        self.assertEqual(cell['status'], 'unavailable')
        self.assertIn('finding 71', cell['reason'])

    def test_different_uses_only_the_gates_own_words(self):
        words = 'model XPPAUT 8.0/cycles: special points [HB], ours []'
        gate = self.gate(passed=False, differences=[words])
        cell = gate_cell('model/adaptive/cycles', 'fd', 'xppaut', gate, 'saved')
        self.assertEqual(cell['status'], 'different')
        self.assertEqual(cell['differences'], [words])

    def test_continuation_title_covers_both_programs(self):
        title = continuation_finding([dict(program='xppaut', ratio=2.), dict(program='xppaut', ratio=3.),
                                      dict(program='auto07p', ratio=2.), dict(program='auto07p', ratio=.8)])
        self.assertIn('XPPAUT 8.0 on every paired run (2)', title)
        self.assertIn('AUTO-07p on 1 of 2 paired runs', title)

    @unittest.skipUnless((Path.home() / 'xppautx-comparison/final/status.json').exists(),
                         'frozen W583 data is local; synthetic gate projections run on every machine')
    def test_every_frozen_cell_matches_its_gate(self):
        data = compute(Path.home() / 'xppautx-comparison/final')
        cells, gates = data['agreement'], data['gates']
        self.assertEqual(len(cells), 292)
        for cell in cells:
            with self.subTest(workload=cell['workload'], program=cell['program'], method=cell['method']):
                if cell['status'] == 'unavailable':
                    self.assertTrue(cell['reason'])
                    if cell['gate_source'] is None:
                        continue  # no pilot gate exists for this program/Jacobian group
                    base = Path.home() / 'xppautx-comparison/final/continuation/attempt-001'
                    folder = base.joinpath(*cell['workload'].split('/')[:2])
                    description = json.loads((folder / 'description.json').read_text())
                    run = cell['workload'].rsplit('/', 1)[1]
                    dr = next(r for r in description['runs'] if r['name'] == run)
                    program = cell['program']
                    recorded = [description.get(program + '_skip'), dr.get('skip')]
                    for suffix in ('unsupported', 'not-gated'):
                        path = folder / f'pilot-{program}-{suffix}.json'
                        if path.exists():
                            recorded.append(json.loads(path.read_text())['reason'])
                    recorded += [u['reason'] for u in gates[cell['gate_source']]['unavailable']
                                 if u['program'] == '.' + program and u['run'] == run]
                    reference = folder / ('pilot-' + program + ('-exact' if cell['method'] == 'exact' else '')) / 'reference.json'
                    if reference.exists():
                        rr = next((r for r in json.loads(reference.read_text())['runs'] if r['name'] == run), None)
                        recorded.append(rr.get('unavailable') if rr else 'No saved reference pilot for this run')
                    else:
                        recorded.append('No saved reference pilot for this run')
                    self.assertIn(cell['reason'], recorded)
                    continue
                gate = gates[cell['gate_source']]
                if cell['status'] == 'different':
                    self.assertFalse(gate['passed'])
                    self.assertTrue(cell['differences'])
                    self.assertTrue(all(text in gate['differences'] for text in cell['differences']))
                else:
                    self.assertTrue(gate['passed'])
                    self.assertEqual(gate['differences'], [])
                    run = cell['workload'].rsplit('/', 1)[1]
                    traced = [t for t in gate['traced_differences'] if t['run'] == run and
                              (cell['method'] == 'exact' or t['reference'] == '.' + cell['program'])]
                    self.assertEqual(cell['status'], 'traced' if traced else 'found')
                    self.assertEqual(cell['traced'], traced)
        lookup = {(c['workload'], c['method'], c['program']): c for c in cells}
        for program in ('xppaut', 'auto07p'):
            self.assertEqual(lookup['lrz_cycles/adaptive/cycles_hb1', 'fd', program]['status'], 'traced')
        self.assertEqual(lookup['P2/adaptive/periodic', 'exact', 'auto07p']['status'], 'found')
        for run in ('steady', 'up', 'down'):
            c = lookup['hen_tr/adaptive/' + run, 'fd', 'xppaut']
            self.assertEqual(c['status'], 'unavailable')
            self.assertIn('finding 71', c['reason'])


class ExtraRuns(unittest.TestCase):
    """W592's and W591's tables, from synthetic saved rows."""
    def memory(self):
        def row(study, workload, program, kib, kind='run', second=None):
            return dict(study=study, workload=workload, program=program, row=kind, runs=[dict(peak_kib=kib)] + ([dict(peak_kib=second)] if second else []))
        integration = [row('screen', 'a', 'xppautx', 1000, second=1010), row('screen', 'a', 'xppaut', 3000, second=3000),
                       row('screen', 'b', 'xppautx', 2000), row('screen', 'b', 'xppaut', 1000),
                       row('screen', 'a', 'xppautx', 900, 'ready'), row('screen', 'a', 'xppaut', 1800, 'ready')]
        continuation = dict(startup=dict(xppautx=[10, 10], xppaut=[20, 20], auto07p=[40, 40]),
                            rows=[dict(workload='P1', mode='adaptive', program='xppautx', runs=[100]),
                                  dict(workload='P1', mode='adaptive', program='auto07p', runs=[400]),
                                  dict(workload='P1', mode='adaptive', program='xppaut', reason='refused')])
        return integration, continuation

    def test_ratio_is_other_over_xppautx_per_workload(self):
        m = memory_ratios(*self.memory())
        self.assertEqual(sorted(m['ratios']['screen']['xppaut']), [.5, 3000 / 1005])
        self.assertEqual(m['ratios']['continuation']['auto07p'], [4.])
        self.assertNotIn('xppaut', m['ratios']['continuation'])
        self.assertAlmostEqual(m['spread'], .01)
        self.assertEqual(m['startup']['screen'], {'xppautx': [900.], 'xppaut': [1800.]})

    def test_memory_text_counts_workloads_where_xppautx_is_not_larger(self):
        text = memory_text(*self.memory())
        self.assertIn('on 2 of 3 paired workloads', text)
        self.assertIn('| continuation | AUTO-07p | 1 | 4.00 |', text)

    def test_threads_and_jobs_tables_name_identity(self):
        row = dict(workload='P1', mode='fixed', identical=True, speedup_2=1.6, speedup_4=2.4, **{'wall_%d' % n: dict(mean=40. / n) for n in (1, 2, 4)})
        self.assertIn('| P1/fixed | 40.00 | 20.00 | 10.00 | 1.60 | 2.40 | yes |', threads_text([row]))
        self.assertIn('NO', threads_text([dict(row, identical=False)]))
        jobs = dict(rows=[dict(n=2, concurrent=dict(mean=4.), sequential=dict(mean=6.), speedup=1.5, concurrent_all_equal=True, sequential_all_equal=False)])
        self.assertIn('| 2 | 4.00 | 6.00 | 1.50 | NO |', jobs_text(jobs, {}))


if __name__ == '__main__':
    unittest.main()
