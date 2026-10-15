#!/usr/bin/env python3
"""W358 (#420): how a comparison build's AUTO diagrams differ from the default build's and from AUTO-07p's.

    python3 tools/auto-profile/diagram_diff.py BINARY [--only NAME,...]

For each tests/auto description (tools/autocompare.py's, run at 1 thread through the same driver), the
diagram the BINARY stores is compared with
  - tests/auto/NAME.diagram.csv, the default build's baseline: the number of stored points and,
    over the points both have, the largest difference of each quantity (continued parameter, norm,
    period, the eigenvalues or multipliers) as |a-b| / (1 + |a|);
  - tests/auto/NAME.auto07p, AUTO-07p's special points, with autocompare's tolerances: the number
    of differences it reports and the largest difference as a fraction of its tolerance.
Prints one markdown table row per description and a total line. The default build gives all zeros
(the baseline is its own output). Run it through tools/heavy.sh.
"""
import argparse, csv, io, os, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import autocompare as ac  # noqa: E402

def spectrum(r):
    """a point's eigenvalues or multipliers sorted, flattened as real, imaginary parts: the order a
    routine returns them in is its own and not a difference"""
    n = sum(1 for k in r if k.startswith('evr'))
    # real parts rounded for the sort only, so a conjugate pair whose real parts differ in the last bit stays a pair
    pairs = sorted(((float(r['evr%d' % i]), float(r['evi%d' % i])) for i in range(1, n + 1)),
                   key=lambda z: (round(z[0], 9), z[1]))
    return [x for z in pairs for x in z]


DETAIL = False
QUANTITIES = (('continued parameter', lambda r: [float(r['values'].split(';')[0])] if r.get('values') else []),
              ('norm', lambda r: [float(r['norm'])]),
              ('period', lambda r: [float(r['per'])]),
              ('eigenvalues, as sets', lambda r: spectrum(r)),
              ('state values', lambda r: [float(v) for k, v in r.items() if k.startswith('u') and '.' in k]))


def points(data):
    """a session diagram's stored points, as dicts of text (its branch header lines left out)"""
    lines = [l for l in data.decode('utf-8').replace('\r', '').splitlines() if not l.startswith('branch,')]
    return list(csv.DictReader(io.StringIO('\n'.join(lines))))


def largest(a, b):
    """the largest |x-y| / (1+|x|) of two lists of numbers"""
    return max((abs(x - y) / (1 + abs(x)) for x, y in zip(a, b)), default=0.0)


def against_baseline(name, data):
    ours, want = points(data), points((ac.BASE / (name + '.diagram.csv')).read_bytes().replace(b'\r', b''))
    worst = {q: 0.0 for q, _ in QUANTITIES}
    for i, (r, w) in enumerate(zip(ours, want)):
        for q, get in QUANTITIES:
            d = largest(get(w), get(r))
            if d > worst[q]:
                worst[q] = d
                if DETAIL and d > 1e-6:
                    print(f'  {name} point {i + 1} {q}: default {get(w)} this {get(r)} (itp {w["itp"]} / {r["itp"]})')
    return len(ours), len(want), worst


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('binary')
    ap.add_argument('--only')
    ap.add_argument('--detail', action='store_true', help='print each point that sets a new largest difference')
    args = ap.parse_args()
    global DETAIL
    DETAIL = args.detail
    os.chdir(ac.ROOT)
    descriptions = {p.stem: ac.read_description(p) for p in sorted(ac.BASE.glob('*.json'))}
    references = ac.read_references(descriptions)
    names = args.only.split(',') if args.only else [n for n, d in descriptions.items() if 'skip' not in d]
    print('| description | points (this / default) | ' + ' | '.join(q for q, _ in QUANTITIES) +
          ' | AUTO-07p: differences | largest difference / tolerance |')
    print('|---|---|' + '---:|' * len(QUANTITIES) + '---:|---:|')
    failed = 0
    for name in names:
        d = descriptions[name]
        data, count, _, ranges = ac.compute(args.binary, d, 1)
        n, m, worst = against_baseline(name, data)
        diffs, measures = ac.compare_references(name, d, data, ranges, {k: v for k, v in references[name].items()
                                                                       if k == '.auto07p'})
        ratio = max((diff / tol for _, _, _, _, diff, tol in measures if tol > 0), default=0.0)
        failed += len(diffs)
        print(f'| {name} | {n} / {m} | ' + ' | '.join(f'{worst[q]:.2e}' for q, _ in QUANTITIES) +
              f' | {len(diffs)} | {ratio:.3f} |', flush=True)
    print(f'AUTO-07p differences in total: {failed}')


if __name__ == '__main__':
    main()
