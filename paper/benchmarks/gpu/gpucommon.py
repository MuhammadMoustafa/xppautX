"""What the W268 probes share: the model file's numbers, NVRTC builds, GPU timing, xppautX processes.

Scripts are run from anywhere with `uv run --locked --group gpu python paper/benchmarks/gpu/<script>.py`."""
import os
from pathlib import Path
import re
import statistics
import sys
import time

REPO = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(REPO / 'python'))
sys.path.insert(0, str(HERE))

import cupy as cp
import numpy as np

import core_math_device as dev
from xppautx import Client

DEFAULT_BINARY = REPO / 'xppautX'
NUMBER = r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?'


def binary(given=None):
    """The xppautX program: --bin, else $XPPAUTX, else ./xppautX of the checkout (a worktree has none: pass --bin)."""
    path = given or os.environ.get('XPPAUTX') or DEFAULT_BINARY
    if not Path(path).is_file():
        sys.exit('no xppautX program at %s: build one (make xppautx) or pass --bin' % path)
    return str(path)


def odex_numbers(path):
    """The `par` and `init` values of an .odex, and its integration total_time and dt, as written in the file."""
    text = Path(path).read_text()
    pars, inits = {}, {}
    for kind, target in (('par', pars), ('init', inits)):
        for line in re.findall(r'^%s\s+(.*)$' % kind, text, flags=re.M):
            for name, value in re.findall(r'(\w+)\s*=\s*(%s)' % NUMBER, line):
                target[name] = float(value)
    m = re.search(r'integration:\s*\{[^}]*?total_time:\s*(%s)[^}]*?dt:\s*(%s)' % (NUMBER, NUMBER), text)
    return pars, inits, (float(m.group(1)), float(m.group(2))) if m else None


def c_double(x):
    return repr(float(x))


def build(source, options, name):
    return cp.RawModule(code=source, options=options).get_function(name)


def gpu_seconds(launch, repeats):
    """Wall seconds of launch() followed by a device synchronisation, `repeats` times after one warm-up: the list."""
    launch()
    cp.cuda.Stream.null.synchronize()
    times = []
    for _ in range(repeats):
        t0 = time.perf_counter()
        launch()
        cp.cuda.Stream.null.synchronize()
        times.append(time.perf_counter() - t0)
    return times


def spread(times):
    return 'median %.4g s (min %.4g, max %.4g, %d repeats)' % (statistics.median(times), min(times), max(times), len(times))


def serve(model, bin_path):
    c = Client(model, binary=bin_path)
    c.start_events()
    return c


def ulps(a, b):
    """The distance in units in the last place between two arrays of doubles; 0 is identical bits."""
    ia, ib = np.asarray(a, np.float64).view(np.int64), np.asarray(b, np.float64).view(np.int64)
    ia = np.where(ia < 0, np.int64(-2**63) - ia, ia)
    ib = np.where(ib < 0, np.int64(-2**63) - ib, ib)
    return np.abs(ia - ib)
