#!/usr/bin/env python3
"""tools/python-integration/check.py: the Python integrators against xppautX, and against SciPy's DOP853 (W427, #489).

    uv run python tools/python-integration/check.py --bin ./xppautX.exe [--only NAME,...] [--method KEY,...|all] [--model FILE]...
                                                    [--screen DIR]

For each model of the integration comparison (the five-model screen, Izhikevich's 28 sets, ma23 and mu26) it runs
xppautX (--silent, the model's file and set file) and the Python integrator of the same method, and fails unless
  - the two outputs have the same rows, and the same time column, and
  - every value of the Python output is within FLOAT_STEPS step of a single-precision float (and the rounding of the
    digits printed) of xppautX's.
The two programs do the same arithmetic step for step, so what is left is the C library's functions (xppautX's are
correctly rounded) against Python's in the model's formulas: a rounding of a double, which gives the same single-precision
floats but for a value either side of a rounding to single precision. Unless a model amplifies that, the agreement is
on every row. A model that does amplify it (episodic's irregular firing; a spike's upstroke under DoPri8(3)) leaves
the agreement at some row, and for those the requirement is that the agreement lasts as far as the Python run is itself
insensitive: SENSITIVITY_PROBES runs of Python with every derivative off by up to one ulp are compared with the
unperturbed run, and the agreement with xppautX must reach the earliest row at which any of them differs by more than
that. A model that stops agreeing sooner is a failure.
With --method, each of those methods is run on the models chosen (the model's own method otherwise); xppautX is
given the same method in a copy of the model. SciPy's DOP853 (rtol 1e-12) is then run at the stored times of the
Python run and the distance of the Python output from it is printed: how far the method is from the solution (and, for
a model with events, from its spike times). It is not timed, and nothing of it is required but that it runs.
"""

import argparse
import copy
from pathlib import Path
import re
import sys

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / 'tools'))  # odex_model

from driver import FAILURES, METHODS, load, run_model
from scipy_integrate import NoSolution, run_dop853
from xppclient import silent_rows

# the agreement required of a value: this many steps of the single-precision float xppautX stores (core/storage.h) at the
# larger of the two, and one unit of the last digit it prints (%.8g, core/data_formats.cpp: PRINT_DIGITS) for the rounding
# of that. Doubles a last bit apart give the same float but for one either side of a rounding, and then the next float.
FLOAT_STEPS = 1
PRINT_DIGITS = 8
# subtracting two printed values has an error of about 1e-9 of a unit; the requirement is this much more than the limit
SUBTRACTION_FUZZ = 1e-6
# the runs of Python with perturbed derivatives that measure a model's sensitivity (their random numbers' seeds)
SENSITIVITY_PROBES = 8
# one ulp of a double, relative
ULP = sys.float_info.epsilon
# the five-model screen's folder in a checkout that has paper/ (git-ignored: copied by hand)
SCREEN = Path('paper') / 'benchmarks' / 'five-model-screen' / 'models'
SCREEN_MODELS = ('amari', 'clustor', 'episodic', 'kuramot100', 'lecdiff200')
IZHIKEVICH = 'izh_*.odex'
BACKWARD_EULER_MODELS = ('ma23', 'mu26')


class Case:
    def __init__(self, name, path, setfile=None):
        self.name, self.path, self.setfile = name, Path(path), setfile


def cases(screen, extra):
    out = []
    for name in SCREEN_MODELS:
        setfile = screen / (name + '.ode.set')
        out.append(Case(name, screen / (name + '.odex'), setfile if setfile.exists() else None))
    ode = REPO_ROOT / 'examples' / 'ode'
    out += [Case(p.stem, p) for p in sorted(ode.glob(IZHIKEVICH))]
    out += [Case(name, ode / (name + '.odex')) for name in BACKWARD_EULER_MODELS]
    out += [Case(Path(p).stem, Path(p).resolve()) for p in extra]
    return out


def with_method(text, key):
    """The model's text with the integration method key (docs/odex.md "Options")."""
    m = re.search(r'(integration\s*:\s*\{)([^{}]*)\}', text)
    if m:
        body = re.sub(r'\bmethod\s*:\s*\w+\s*,?', '', m.group(2))
        return text[:m.start()] + m.group(1) + ' method: %s,' % key + body + '}' + text[m.end():]
    if re.search(r'^options\s*\{', text, re.M):
        return re.sub(r'^options\s*\{', 'options { integration: { method: %s }, ' % key, text, count=1, flags=re.M)
    return text + '\noptions { integration: { method: %s } }\n' % key


def run_xppautx(binary, case, method):
    """The output.dat of xppautX on the case, with the method if given: its rows as an array."""
    arguments = ['--setfile', str(case.setfile)] if case.setfile else []
    return np.array(silent_rows(binary, case.path, arguments, edit=(lambda text: with_method(text, method)) if method else None), ndmin=2)


def printed(rows):
    """The values as xppautX prints them: single precision, %.8g."""
    return np.array([[float('%.*g' % (PRINT_DIGITS, v)) for v in row] for row in rows.astype(np.float32)])


def limits(a, b):
    """How much each pair of values may differ: FLOAT_STEPS steps of a single-precision float and a unit of the last digit printed."""
    larger = np.maximum(np.abs(a), np.abs(b))
    with np.errstate(all='ignore'):  # a pair of zeros has no unit and no difference
        unit = 10.0 ** (np.floor(np.log10(larger)) - (PRINT_DIGITS - 1))
    return FLOAT_STEPS * np.spacing(larger.astype(np.float32)).astype(np.float64) + unit


def disagreement(a, b):
    """The (row, column) pairs where printed rows a and b differ by more than limits() (any difference of the time)."""
    off = ~(np.abs(a - b) <= limits(a, b) * (1 + SUBTRACTION_FUZZ))  # a NaN is a difference
    off[:, 0] = a[:, 0] != b[:, 0]
    return np.argwhere(off)


def perturbed(model, seed):
    """The model with every derivative off by up to one ulp (a different one each time it is worked out)."""
    other, random = copy.copy(model), np.random.default_rng(seed)

    def rhs(t, y):
        f = model.rhs(t, y)
        return f * (1 + ULP * random.integers(-1, 2, size=f.shape))
    other.rhs = rhs
    return other


def insensitive_rows(model, numerics, method, reference):
    """How many rows of the printed run `reference` stay within the limits of a run with perturbed derivatives,
    at the earliest of SENSITIVITY_PROBES such runs."""
    rows = len(reference)
    for seed in range(SENSITIVITY_PROBES):
        probe = printed(run_model(perturbed(model, seed), numerics, method).rows)
        if probe.shape != reference.shape:
            return 0
        bad = disagreement(reference, probe)
        if len(bad):
            rows = min(rows, int(bad[0][0]))
    return rows


def judge(model, numerics, method, run, xpp):
    """(None, result text) when the run meets the requirement, else (the reason, '')."""
    if run.rows.shape != xpp.shape:
        return 'rows %s (Python) against %s (xppautX)' % (run.rows.shape, xpp.shape), ''
    py = printed(run.rows)
    bad = disagreement(py, xpp)
    if not len(bad):
        return None, 'ok  %.1e' % float(np.abs(py - xpp).max())
    agreed = int(bad[0][0])
    r, c = bad[0]
    where = '%s at row %d (t=%s): Python %s, xppautX %s' % (run.columns[c], r, xpp[r, 0], py[r, c], xpp[r, c])
    sensitive = insensitive_rows(model, numerics, method, py)
    if agreed < sensitive:
        return '%d values differ, the first %s; a run with perturbed derivatives agrees to row %d' % (len(bad), where, sensitive), ''
    return None, 'ok to t=%s of %s (ulp noise changes the run from t=%s)' % (xpp[max(agreed - 1, 0), 0], xpp[-1, 0], xpp[min(sensitive, len(xpp) - 1), 0])


def reference_report(model, numerics, run, method):
    """What SciPy's DOP853 says of the Python run: the largest distance of its values and of its event times."""
    if method == 'discrete':
        return 'map'
    try:
        rows, fired = run_dop853(model, numerics, run.times)
    except NoSolution as e:
        return 'no reference: %s' % e
    if rows.shape != run.rows.shape:
        return 'FAIL: DOP853 gives %s rows, the run %s' % (rows.shape[0], run.rows.shape[0])
    text = '%.2e' % float(np.abs(rows - run.rows.astype(np.float64)).max())
    if fired or run.fired:
        same = len(fired) == len(run.fired)
        text += ', %d spikes%s' % (len(run.fired), '' if same else " against DOP853's %d" % len(fired))
        if same and fired:
            text += ', spike times to %.2e' % max(abs(a[0] - b[0]) for a, b in zip(fired, run.fired))
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--bin', type=Path, default=REPO_ROOT / ('xppautX.exe' if sys.platform == 'win32' else 'xppautX'),
                        help='the xppautX program (default xppautX in the checkout)')
    parser.add_argument('--only', help='comma-separated model names')
    parser.add_argument('--method', help="comma-separated method keys (%s) or all: each on the models chosen; default each model's own" % ', '.join(METHODS))
    parser.add_argument('--model', action='append', default=[], metavar='FILE', help="a model more than the comparison's")
    parser.add_argument('--screen', type=Path, default=REPO_ROOT / SCREEN, help="the five-model screen's folder (default %(default)s)")
    args = parser.parse_args()
    chosen = set(args.only.split(',')) if args.only else None
    methods = [None] if not args.method else (list(METHODS) if args.method == 'all' else args.method.split(','))
    if any(m is not None and m not in METHODS for m in methods):
        parser.error('methods are %s' % ', '.join(METHODS))
    todo = [c for c in cases(args.screen.resolve(), args.model) if not chosen or c.name in chosen]
    if any(c.name in SCREEN_MODELS and not c.path.exists() for c in todo):
        parser.error("the five-model screen's models are not in %s (paper/ is copied by hand; --screen names the folder, --only leaves them out)" % args.screen)
    failures = skipped = 0
    print('%-30s %-15s %6s  %-12s %s' % ('model', 'method', 'rows', 'vs xppautX', 'vs DOP853 (largest difference, spikes)'), flush=True)
    for case in todo:
        model, numerics = load(case.path, case.setfile)
        for method in methods:
            key = method or numerics.method
            if method and ((method == 'discrete') != (numerics.method == 'discrete') or case.setfile):
                skipped += 1  # a map is not a continuous model, and a set file sets the method
                continue
            try:
                xpp = run_xppautx(args.bin, case, method)
                run = run_model(model, numerics, method)
                problem, result = judge(model, numerics, method, run, xpp)
                failures += problem is not None
                print('%-30s %-15s %6d  %-12s %s' % (case.name, key, len(xpp), 'FAIL' if problem else result,
                                                    'not run' if problem else reference_report(model, numerics, run, key)), flush=True)
                if problem:
                    print('    ' + problem, flush=True)
            except FAILURES + (ValueError,) as e:
                failures += 1
                print('%-30s %-15s %6s  %-12s %s' % (case.name, key, '', 'FAIL', e), flush=True)
    note = ', %d skipped (a map or a set file: not the method asked)' % skipped if skipped else ''
    print('%d failed%s' % (failures, note) if failures else 'all agree' + note)
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
