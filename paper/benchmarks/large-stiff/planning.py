"""Single n=200 planning probes, never a repeat timing series or benchmark result."""
import argparse
import json
import platform
from pathlib import Path
import time

import study
from generate import generate

PLANNING_N = 200  # short laptop probe; large runs remain Ubuntu-only


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--worker', type=Path, required=True)
    ap.add_argument('--correctness', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--cpu', type=int, default=2)
    args = ap.parse_args()
    worker, root = args.worker.resolve(), args.out.resolve()
    root.mkdir(parents=True, exist_ok=False)
    gate = json.loads(args.correctness.read_text())
    results = dict(purpose='one-shot planning estimates, not timing evidence',
                   platform=platform.platform(), n=PLANNING_N, cpu=args.cpu, probes=[])
    for family in ('cable', 'ring'):
        model = generate(family, PLANNING_N, root/'models')
        case = next(c for c in gate['cases'] if c['family']==family and c['n']==PLANNING_N)
        for solver in ('legacy-dense', 'legacy-band', 'gmres', 'reference'):
            tol = study.REFERENCE_TOL if solver=='reference' else case['solvers'][solver]['tolerance']
            start = time.perf_counter()
            result = study.run(worker, model, solver, tol, root/f'{family}-{solver}', cpu=args.cpu, factors=True)
            result['planning_wall_seconds'] = time.perf_counter()-start
            result.update(family=family, kind='native-whole-run')
            results['probes'].append(result)
            study.save(root/'planning.json', results)
        start = time.perf_counter()
        result = study.run(worker, model, 'legacy-dense', case['solvers']['legacy-dense']['tolerance'],
                           root/f'{family}-cache', cpu=args.cpu, callgrind=True)
        result['planning_wall_seconds'] = time.perf_counter()-start
        result.update(family=family, kind='cache-three-factors')
        results['probes'].append(result)
        study.save(root/'planning.json', results)
    for p in results['probes']:
        print(f'{p["family"]} {p["solver"]} {p["kind"]}: {p["planning_wall_seconds"]:.6f} s; {p["stats"]}')


if __name__ == '__main__':
    main()
