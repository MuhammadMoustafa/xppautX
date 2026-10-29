"""Untimed harness checks: synthetic measurements only, no solver or clock."""
import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import study


class Protocol(unittest.TestCase):
    def test_cache_misses_per_factorization(self):
        with tempfile.TemporaryDirectory(dir='build') as tmp:
            path=Path(tmp)/'callgrind.out'
            path.write_text('events: Ir Dr Dw I1mr D1mr D1mw ILmr DLmr DLmw\n'
                            'summary: 0\n'
                            'totals: 300 150 90 3 30 12 0 9 6\n')
            result=study.cache_counts(path, 3)
            self.assertEqual(result['factorizations'], 3)
            self.assertEqual(result['misses_per_factorization'],
                             dict(I1mr=1., D1mr=10., D1mw=4., ILmr=0., DLmr=3., DLmw=2.))
            path.write_text('events: Ir\ntotals: 0\n')
            with self.assertRaises(ValueError):
                study.cache_counts(path, 3)

    def test_time_parser(self):
        with tempfile.TemporaryDirectory(dir='build') as tmp:
            path = Path(tmp)/'time.txt'
            path.write_text('User time (seconds): 2.50\nSystem time (seconds): 0.25\n'
                            'Elapsed (wall clock) time (h:mm:ss or m:ss): 1:02.30\n'
                            'Maximum resident set size (kbytes): 12345\n')
            self.assertEqual(study.parse_time(path), dict(wall=62.3, cpu=2.75, peak_rss_kib=12345))

    def exercise(self, fail):
        with tempfile.TemporaryDirectory(dir='build') as tmp:
            base = Path(tmp).resolve()
            pilot = base/'pilot'
            (pilot/'models').mkdir(parents=True)
            cases = []
            text=''.join(str(k/100)+' '+' '.join(['0']*6)+'\n' for k in range(101))
            for family in ('cable', 'ring'):
                path=pilot/'models'/f'{family}-6.odex'
                path.write_text('# synthetic\n')
                study.save(path.with_suffix('.json'), dict(n=6, total=1.))
                ref=pilot/f'{family}-6/reference-half'
                ref.mkdir(parents=True)
                (ref/'trajectory.dat').write_text(text)
                cases.append(dict(family=family, n=6, model=str(path), status='pass',
                                  model_sha256=study.digest(path), metadata_sha256=study.digest(path.with_suffix('.json')),
                                  reference_sha256=study.digest(ref/'trajectory.dat'),
                                  solvers={s: dict(tolerance=1e-5) for s in study.SOLVERS}))
            study.save(pilot/'correctness.json', dict(cases=cases, omitted=[]))
            study.save(pilot/'identity.json', dict(test=True))
            calls=[]

            def fake_run(worker, model, solver, tol, folder, **kwargs):
                calls.append((solver, folder, kwargs))
                folder.mkdir(parents=True, exist_ok=False)
                (folder/'trajectory.dat').write_text(text)
                if fail and folder.name == '1-klu' and 'cable-6' in str(folder):
                    raise RuntimeError('synthetic failure')
                return dict(solver=solver, folder=str(folder), tolerance=tol,
                            wall=1., cpu=.9, peak_rss_kib=100, stats={})

            args=argparse.Namespace(worker=base/'worker', out=base/'results', pilot=pilot,
                                    sizes=[6], cpu=2, machine_record=base/'machine.json')
            with patch.object(study, 'available', return_value=study.SOLVERS), \
                 patch.object(study, 'identity', return_value=dict(test=True)), \
                 patch.object(study, 'machine'), patch.object(study, 'run', side_effect=fake_run), \
                 patch.object(study.subprocess, 'run'), \
                 patch.object(study.subprocess, 'check_output', return_value=''):
                study.measure(args)
            results=json.loads((args.out/'runs.json').read_text())
            self.assertEqual(len(results), 48)
            self.assertEqual(sum(r['warmup'] for r in results), 8)
            self.assertEqual(sum(not r['accuracy_pass'] for r in results), int(fail))
            for family in ('cable', 'ring'):
                for rep in range(5):
                    group=[r['solver'] for r in results if r['family']==family and r['repetition']==rep]
                    shift=rep%4
                    self.assertEqual(group, study.SOLVERS[shift:]+study.SOLVERS[:shift])
            factors=[c for c in calls if c[2].get('factors')]
            self.assertEqual(len(factors), 8)
            self.assertTrue(all(not c[2].get('measure') for c in factors))
            self.assertTrue(all(c[2]['cpu']==2 for c in calls))
            self.assertEqual(len({str(c[1]) for c in calls}), len(calls))
            summary=json.loads((args.out/'summary.json').read_text())
            klu=next(r for r in summary if r['family']=='cable' and r['solver']=='klu')
            self.assertEqual(klu['passed'], 4 if fail else 5)
            self.assertEqual('wall_mean' in klu, not fail)
            self.assertEqual(len(json.loads((args.out/'crossings.json').read_text())), 12)

    def test_rotating_fresh_runs(self):
        self.exercise(False)

    def test_failed_run_is_kept_and_not_summarized(self):
        self.exercise(True)


if __name__ == '__main__':
    unittest.main()
