#!/usr/bin/env -S uv run --locked python
"""W269 (#326): ModelDB 267710's AUTO workflow timed in XPPAUT 8.0 (auto_xppaut.py),
xppautX (auto_xppautx.py: finite-difference and exact Jacobian on 1, 2 and 4 threads, and
the interpreter) and AUTO-07p (auto07p/: 1, 2 and 4 threads, finite differences), on WSL,
with W246's protocol (../auto/bench.py's series: one excluded warm-up, then REPEATS
measured fresh processes per engine, the order rotating each repetition; one-thread engines
pinned to CPU 2, n threads to CPUs 2..2+n-1).

    uv run --locked python auto_bench.py --xppaut XPPAUT --xppautx XPPAUTX --auto07p-src DIR --name NAME [--engines ...]

DIR is a copy of auto07p/ on WSL's own disk; AUTO_DIR must name the AUTO-07p build.
"""
import argparse, shutil, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'auto'))
import bench  # noqa: E402  W246's runner: pinning, AUTO-07p, the series and its summary
import auto_xppaut, auto_xppautx  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--xppaut', required=True)
    ap.add_argument('--xppautx', required=True)
    ap.add_argument('--auto07p-src', required=True, type=Path)
    ap.add_argument('--name', required=True)
    ap.add_argument('--repeats', type=int, default=bench.REPEATS)
    ap.add_argument('--engines', required=True, help='comma-separated: xppaut, xppautx-{fd,exact}-Nt, '
                    'xppautx-interp-1t, auto07p-Nt')
    a = ap.parse_args()
    out = HERE / 'results' / a.name
    out.mkdir(parents=True, exist_ok=False)
    tmp = Path(tempfile.mkdtemp(prefix='w269-pin-'))
    engines = {'xppaut': lambda: auto_xppaut.measure(bench.pinned(a.xppaut, bench.CPU, tmp)),
               **bench.xppautx_engines(auto_xppautx.measure, a.xppautx, tmp)}
    for n in bench.THREADS:
        engines[f'auto07p-{n}t'] = lambda n=n: bench.auto07p(a.auto07p_src, n, name='6c')
    try:
        bench.series(engines, a.engines.split(','), a.repeats, out, runs_kept=('integrate', 'steady', 'periodic'))
    finally:
        shutil.rmtree(tmp)


if __name__ == '__main__':
    main()
