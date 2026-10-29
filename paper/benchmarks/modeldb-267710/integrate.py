#!/usr/bin/env -S uv run --locked python
"""W269 (#326): ModelDB 267710's integration timed as the five-model screen times its
models (../five-model-screen/screen.py's measure: a warm-up and five measured fresh
processes per engine, the order rotating, CPU 2, fresh HOME, LC_ALL=C, outputs compared).

The model as its README runs it before AUTO: models/6cModel/rest.ode (main.ode with
@ total=600; the model's own backward Euler, dt 0.025, ampPar = 0), and with
--cases fire too (fire.ode: the same at ampPar = 0.3 through glob_fire.inc, where the cell
fires repetitively: 8 spikes in its last 300 ms).
XPPAUT 8.0 reads the .ode and its .inc files, xppautX its conversion (the .odex holds
the included lines).

    uv run --locked python integrate.py --legacy XPPAUT --modern XPPAUTX --result-name NAME [--cases rest fire]
"""
import argparse, os, shutil, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'five-model-screen'))
import screen  # noqa: E402  one owner of the timed runs and the comparison

MODEL_DIR = HERE / 'models' / '6cModel'
INCLUDES = sorted(p.name for p in MODEL_DIR.glob('*.inc'))
STATES = 25  # NEQ=25 in XPPAUT's load report; no aux, so 26 output columns
CASES = {
    'rest': dict(name='rest', states=STATES, total=600, dt=0.025, method='backeul', files=INCLUDES),
    'fire': dict(name='fire', states=STATES, total=600, dt=0.025, method='backeul', files=INCLUDES),
}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--legacy', type=Path, required=True)
    ap.add_argument('--modern', type=Path, required=True)
    ap.add_argument('--result-name', required=True)
    ap.add_argument('--cases', nargs='+', default=['rest'], choices=list(CASES))
    ap.add_argument('--engines', default='legacy,modern,interp')
    ap.add_argument('--cpu', type=int, default=2)
    ap.add_argument('--timeout', type=float, default=screen.TIMEOUT)
    ap.add_argument('--pilot-only', action='store_true')
    a = ap.parse_args()
    target = HERE / 'results' / a.result_name
    if target.exists():
        raise FileExistsError(f'{target}: refusing to overwrite; choose another --result-name')
    if a.cpu not in os.sched_getaffinity(0):
        raise ValueError('CPU unavailable')
    engines = a.engines.split(',')
    binary = {e: (a.legacy if e == 'legacy' else a.modern).resolve() for e in engines}
    cases = [CASES[c] for c in a.cases]
    with tempfile.TemporaryDirectory(prefix='w269-mdb-int-') as tmp:
        results = screen.measure(cases, engines, binary, Path(tmp), MODEL_DIR, a.cpu, a.timeout, a.pilot_only)
        shutil.copytree(results, target)
    print('Saved', target, flush=True)


if __name__ == '__main__':
    main()
