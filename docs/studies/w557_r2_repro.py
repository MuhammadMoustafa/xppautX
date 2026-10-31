"""Minimal reproduction of a missed R2 on a period-doubling curve of cycles (W557, #622).

tests/auto/forced_pp_pd.json with the mesh of the cycles and of both period-doubling curves cut from 40 to 15 intervals: the curve
`pd_up` passes the 1:2 resonance (R2) at (eps, b0) = (1.94893, 2.57025), where the multiplier -1 the curve imposes meets the other
one, and xppautX labels nothing there; AUTO-07p 0.9.4 labels it on the same description. An independent computation (the
multipliers of the forced predator-prey model's Poincare map, docs/studies/w557_special_point_check.py) puts the multipliers
at -1 and -1 there and shows the curve itself is right to 1e-6.

    uv run --locked python docs/studies/w557_r2_repro.py [--server ./xppautX]

Prints the labels of pd_up. xppautX's own log (--server --verbose) says why: "Br 3 Pt 204: a period doubling or torus bifurcation (PD, TR)
between points 204 and 205 was not located (not in max_location_iterations = 8 iterations); it is missing from the diagram".
"""
import argparse
import copy
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import autocompare  # noqa: E402

MESH = 15  # at 40 (the description's) the R2 is labelled; at 80, 60, 30, 20 too


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', default=str(ROOT / 'xppautX'))
    a = p.parse_args()
    d = copy.deepcopy(autocompare.read_description(ROOT / 'tests/auto/forced_pp_pd.json'))
    for r in d['runs']:
        if r['name'] in ('cycles', 'pd_up', 'pd_down'):
            r['auto']['mesh_intervals'] = MESH
    d['runs'] = [r for r in d['runs'] if r['name'] != 'pd_down']
    data, _, _, ranges = autocompare.compute(a.server, d, 1)
    rows = autocompare.stored_rows(data, ranges)['pd_up']
    labels = [(autocompare.code_of(r['itp']), i, round(autocompare.quantity(r, 'par'), 5), round(float(r.get('continued_parameter2', 'nan')), 5))
              for i, r in enumerate(rows) if autocompare.code_of(r['itp']) not in ('',)]
    print('pd_up: %d points, labelled %s' % (len(rows), labels))
    print('R2 labelled' if any(l[0] == 'R2' for l in labels) else 'NO R2 near (1.94893, 2.57025): missed')


if __name__ == '__main__':
    main()
