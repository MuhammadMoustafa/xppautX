"""A missed R2 on a period-doubling curve of cycles (W557, #622; fixed by W565, #630).

tests/auto/forced_pp_pd.json with the mesh of the cycles and of the period-doubling curve `pd_up` cut from 40 to NTST intervals and
the curve's step size set to DS: the curve passes the 1:2 resonance (R2) at (eps, b0) = (1.94893, 2.57025), where the multiplier -1 the
curve imposes meets the other one, and AUTO-07p 0.9.4 labels it in all 21 runs of NTST 10, 15, 20, 30, 40, 60, 80 and DS 0.005, 0.01, 0.02. An
independent computation (the multipliers of the forced predator-prey model's Poincare map, docs/studies/w557_special_point_check.py)
puts the multipliers at -1 and -1 there and shows the curve itself is right to 1e-6. xppautX missed it at NTST 15 / DS 0.005 and
NTST 10 / DS 0.02 (W565: its test, |m|-1 of the multiplier that is not the imposed one, jumps there where the two swap roles).

    uv run --locked python docs/studies/w557_r2_repro.py [--server ./xppautX] [--mesh N --step DS]

Prints, per setting, whether pd_up has an R2 label within 1e-3 of the point; exits 1 when one is missing.
"""
import argparse
import copy
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import autocompare  # noqa: E402

MESHES = (10, 15, 20, 30, 40, 60, 80)
STEPS = (0.005, 0.01, 0.02)
R2_AT = (1.94893, 2.57025)  # (eps, b0)
R2_TOLERANCE = 1e-3


def r2_labels(server, mesh, step):
    d = copy.deepcopy(autocompare.read_description(ROOT / 'tests/auto/forced_pp_pd.json'))
    d['runs'] = [r for r in d['runs'] if r['name'] != 'pd_down']
    for r in d['runs']:
        if r['name'] in ('cycles', 'pd_up'):
            r['auto']['mesh_intervals'] = mesh
        if r['name'] == 'pd_up':
            r['auto']['step_size'] = step
    data, _, _, ranges = autocompare.compute(server, d, 1)
    rows = autocompare.stored_rows(data, ranges)['pd_up']
    return [(round(autocompare.quantity(r, 'par'), 5), round(float(r.get('continued_parameter2', 'nan')), 5))
            for r in rows if autocompare.code_of(r['itp']) == 'R2']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', default=str(ROOT / 'xppautX'))
    p.add_argument('--mesh', type=int)
    p.add_argument('--step', type=float)
    a = p.parse_args()
    missed = 0
    for mesh in (a.mesh,) if a.mesh else MESHES:
        for step in (a.step,) if a.step else STEPS:
            labels = r2_labels(a.server, mesh, step)
            found = [l for l in labels if abs(l[0] - R2_AT[0]) < R2_TOLERANCE and abs(l[1] - R2_AT[1]) < R2_TOLERANCE]
            missed += not found
            print('NTST %2d DS %.3f: R2 labels %s: %s' % (mesh, step, labels, 'ok' if found else 'MISSED'))
    sys.exit(1 if missed else 0)


if __name__ == '__main__':
    main()
