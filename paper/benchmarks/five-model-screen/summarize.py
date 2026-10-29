#!/usr/bin/env -S uv run --locked python
"""Summarize existing runs without rerunning or changing the simulations."""
import argparse, gzip, hashlib, json
from pathlib import Path

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('results',type=Path,nargs='?',default=Path(__file__).resolve().parent/'results')
a=ap.parse_args();root=a.results
summaries=json.loads((root/'summary.json').read_text())
comparisons=json.loads((root/'comparison.json').read_text())['comparisons']
runs=json.loads((root/'runs.json').read_text())
lines=['# Five-model timing screen results','', 'Linux/WSL, matched GCC 15.2 builds, headless whole-process wall time. Mean +/- sample SD, n=5; one excluded warm-up.','', '| Model | XPPAUT 8.0 (s) | xppautX (s) | Legacy / modern | Exported values |','|---|---:|---:|---:|---|']
for name in dict.fromkeys(s['model'] for s in summaries):
    pair={s['engine']:s for s in summaries if s['model']==name}
    old=pair['legacy'];new=pair['modern']
    checks=[c for c in comparisons if c['model']==name and c['repetition']>0]
    exact=len(checks)==5 and all(c.get('exact_values',False) and c.get('time_grid_equal',False) and c.get('same_shape',False) for c in checks)
    def cell(s):
        mean=s['wall_seconds_mean'];sd=s['wall_seconds_sample_sd']
        return f'{mean:.6f} +/- {sd:.6f}' if mean is not None else 'incomplete'
    ratio=old['wall_seconds_mean']/new['wall_seconds_mean'] if old['wall_seconds_mean'] is not None and new['wall_seconds_mean'] else None
    ratio_text=f'{ratio:.3f}x' if ratio else 'not available'
    lines.append(f"| {name} | {cell(old)} | {cell(new)} | {ratio_text} | {'Exact at export precision' if exact else 'MISMATCH / not validated'} |")
lines += ['', 'A ratio above 1 means xppautX was faster; below 1 means it was slower. A mismatched model is not a validated equal-result speed comparison.', '', '## Numerical checks', '']
details=[]
for c in comparisons:
    if c['repetition']!=1:continue
    name=c['model']
    item=dict(c)
    if c.get('both_runs_valid'):
        with gzip.open(root/f'{name}-legacy-1.dat.gz','rt') as fa,gzip.open(root/f'{name}-modern-1.dat.gz','rt') as fb:
            initial_error=None;first_difference=None
            for row,(aa,bb) in enumerate(zip(fa,fb)):
                va=[float(x) for x in aa.split()];vb=[float(x) for x in bb.split()]
                if row==0:initial_error=max(abs(x-y) for x,y in zip(va[1:],vb[1:]))
                if first_difference is None and va!=vb:first_difference={'zero_based_row':row,'time':va[0]}
        item.update(initial_max_abs_error=initial_error,first_exported_difference=first_difference)
        lines.append(f"- {name}: maximum absolute error {'not finite' if c['max_abs_error'] is None else format(c['max_abs_error'],'.9g')}; {c['outside_tolerance']} / {c['values']} state values outside screening tolerance; initial maximum error {initial_error:.9g}.")
    details.append(item)
measured=[r for r in runs['runs'] if r['repetition']>0]
lines += ['',f"Completed valid runs: {sum(r['valid'] for r in measured)}/{len(measured)}. Deterministic within each program/model: {sum(s['repeated_output_identical'] for s in summaries)}/{len(summaries)} combinations.", '', 'Terman must be investigated before claiming cross-program reproducibility. Other models matching text exports does not establish internal bitwise identity or reproduce a published scientific result.', '', 'See ../README.md for the input repairs, timing scope, and limitations.']
(root/'report.md').write_text('\n'.join(lines)+'\n')
(root/'numerical-details.json').write_text(json.dumps(details,indent=2)+'\n')
print('\n'.join(lines[:11]))