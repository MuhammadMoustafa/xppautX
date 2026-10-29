#!/usr/bin/env -S uv run --locked python
"""W248: what differs in the XPPAUT runs that finished with a different diagram: every kept
diff-*.json.gz against the reference of its ParMin, with and without the LAB column (the
label number AUTO gives a stored solution, column 4 of XPPAUT's table).

    uv run --locked python analyze.py results/wsl-xppaut
"""
import gzip, json, sys
from pathlib import Path

def lab_at(t):
    """the LAB column: XPPAUT leaves the type (TY) column empty for a plain point"""
    return 2 if t[2].lstrip('-').isdigit() else 3


d = Path(sys.argv[1])
refs = json.load(gzip.open(d / 'references.json.gz', 'rt'))
out = []
for f in sorted(d.glob('diff-*.json.gz')):
    pm = f.name.rsplit('-pm', 1)[1].split('.')[0]
    got = json.load(gzip.open(f, 'rt'))
    for run in ('steady', 'periodic'):
        a, b = got[run]['rows'], refs[pm][run]['rows']
        nolab = lambda rows: [t[:2] + t[lab_at(t) + 1:] for t in map(str.split, rows)]  # noqa: E731
        out.append({'file': f.name, 'run': run, 'rows': len(a), 'rows_ref': len(b),
                    'rows_differing': sum(x != y for x, y in zip(a, b)),
                    'rows_differing_ignoring_LAB': sum(x != y for x, y in zip(nolab(a), nolab(b))),
                    'LAB_offsets': sorted({int(x.split()[lab_at(x.split())]) - int(y.split()[lab_at(y.split())]) for x, y in zip(a, b)})})
(d / 'different-diagrams.json').write_text(json.dumps(out, indent=1))
for o in out:
    print(o)
