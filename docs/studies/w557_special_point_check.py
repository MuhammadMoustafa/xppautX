"""An independent check of the curves the comparison's special-point workloads continue (W557, #622).

For every sample of a program's curve (xppautX's diagram, XPPAUT's and AUTO-07p's reference curves of a workload's attempt
folder) this decides, from the model's own equations and nothing of any program's continuation, how far the sample lies from
the true curve, in the units of a parameter:

  forced_pp_pd, forced_pp_ns  The periodically forced predator-prey model (Kuznetsov, Muratori and Rinaldi 1992, MatCont
      Tutorial V). Its forcing circle (u, v) = sqrt(m) (cos, sin)(2 pi t + phase) is exact, so the (x, y) system is a planar
      system forced with period 1, and the cycle is a fixed point of its Poincare map over one period, found by Newton's
      method; the monodromy matrix is the solution of the variational equations with the Jacobian differentiated by hand (RK4, 8000 steps).
      The period-doubling curve is where that matrix has an eigenvalue -1, the Neimark-Sacker curve where its determinant is 1
      with a trace below 2 in modulus (a complex pair on the unit circle). For each sample (eps, b0) at the curve's fixed m
      this solves for b0 (or eps where the curve is steep) that makes the condition hold: the sample's distance from the curve.
      Where that gives no root, or one farther than AXIS_ENOUGH (a 1:2 resonance, where the test is positive on every axis line
      through the corner), the distance is the smallest one in the (eps, b0) plane to the union of the flip curve (a multiplier -1) and
      the torus curve (a complex pair with |mu| = 1), found along lines through the sample (plane_distance).
  hen_pd, hen_tr  AUTO-07p's hen demo, the map (x, y) -> (y, alpha - beta x - y^2): the fixed point's Jacobian
      [[0, 1], [-beta, -2 y]] has det beta and trace -2 y, so the flip curve is alpha = 3 (1 + beta)^2 / 4 and the
      torus curve beta = 1 (|alpha| below 3: |trace| < 2).
  explicit_homoclinic  tools/models/explicit_homoclinic.odex (x = 3/2 sech^2(sqrt(q) t/2) is homoclinic at k = 0 for every q): a sample's
      distance from the curve is |k|.
  lorenz_homoclinic  The Lorenz equations (sigma 10): the branch x > 0 of the origin's unstable manifold, integrated with
      scipy, crosses the plane x = 0 for r above the homoclinic value and does not below it (the crossing's y tends to 0
      as r falls to it: the orbit runs down the z axis, the origin's stable direction -b). For each sample (r, b) this
      bisects, at that b, for the r where the crossing appears: the sample's distance from the curve.

Each program's samples are its own, at the precision it wrote them: xppautX's stored diagram, AUTO-07p's reference curve (ten
digits), and XPPAUT's labelled points, read from the .s file of its series (ten digits, the file AUTO writes at each labelled point:
docs/xppaut-findings.md 104). XPPAUT's exported curve, printed to six digits, is not judged: the labelled points are the samples where
XPPAUT's accuracy is read. A labelled point belongs to a run when it lies within MEMBER_TOLERANCE of that run's exported curve, a test of
membership only. A sample's distance is the nearest zero of the condition on each axis (nearest_root), never a jump of the condition,
or, when that is missing or above AXIS_ENOUGH, the distance in the plane (plane_distance): never more than the axes' distance, and the same
where the curve is smooth and an axis crosses it.

    uv run --locked python docs/studies/w557_special_point_check.py --attempt DIR --workload forced_pp_pd [--runs pd_up,pd_down]

Prints, per run and program, the number of samples checked, the largest and root-mean-square distance and where the largest is, and
the source of the samples.
"""
import argparse
import ctypes
import functools
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import brentq, minimize_scalar

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import autocompare  # noqa: E402

RTOL, ATOL = 1e-12, 1e-14  # of every integration here
NEWTON_TOLERANCE = 1e-12  # of the Poincare map's fixed point
MAX_NEWTON = 40  # steps before a fixed point is called not found
MAX_HALVINGS = 10  # move_to's finest walk: 2^10 steps from one sample to the next
STRIDE = 1  # every STRIDE-th sample of a curve is checked (--stride)
LORENZ_TIME = 40.0  # the longest the unstable manifold is followed before it is said not to return
LORENZ_BISECTIONS = 36  # halvings of the bracket around the homoclinic value (the bracket is 1e-6 or more wide)
SEARCH = (1e-6, 1e-4, 1e-3, 1e-2)  # half-widths of the brackets tried when solving for a parameter that puts a sample on the curve
GRID = tuple(1e-7 * 10 ** (k / 8) for k in range(41))  # 1e-7 to 1e-2, eight steps a decade: nearest_root's outward search
AXIS_ENOUGH = 1e-7  # an axis distance this small or smaller is kept as the sample's distance (an upper bound, already within the check's accuracy); above it the plane's is looked for
LINES = 12  # directions (every 15 degrees, the axes among them) of plane_distance's coarse search; each line is searched on both sides, so 24 rays: the nearest curve point is within 7.5 degrees of one
ANGLE_TOLERANCE = 1e-3  # radians: the direction of the nearest curve point is refined to this (the distance's error is then below 1e-6 relative)
ROOT_RESIDUAL = 1e-7  # |condition| at an accepted root, in the condition's units: a jump of the condition is not a zero, and a bisection's error is far below this
MEMBER_TOLERANCE = 1e-3  # the largest distance of a labelled point from a run's exported curve for it to belong to that run (in the parameters' units)
XPPAUT_PAR_TOLERANCE = 1e-6  # relative: a value printed to six digits matches the ten-digit parameter vector of its labelled point to this
SOURCE = {'xppautX': 'stored diagram', 'XPPAUT': 'labelled points (.s, ten digits)', 'AUTO-07p': 'reference curve (ten digits)'}
SECOND = 'second parameter'


POINCARE_C = r"""
#include <math.h>
/* RK4 over [0, 1] of the forced predator-prey system and its variational equations (the Jacobian by hand). */
static void rhs(double t, const double *s, double *f, double eps, double b0, double m, double phase) {
    const double kk = 1.0, r = 6.28, a = 12.56, e = 1.0, d = 6.28;
    double x = s[0], y = s[1], u = sqrt(m) * cos(2 * M_PI * t + phase), D = b0 * (1 + eps * u);
    double g = a * x * y / (D + x), dgx = a * y * D / ((D + x) * (D + x)), dgy = a * x / (D + x);
    double j00 = r * (1 - 2 * x / kk) - dgx, j01 = -dgy, j10 = e * dgx, j11 = e * dgy - d;
    f[0] = r * x * (1 - x / kk) - g;
    f[1] = e * g - d * y;
    for (int c = 0; c < 2; c++) {
        double y0 = s[2 + c], y1 = s[4 + c];  /* column c of Y: Y = [[s2 s3], [s4 s5]] */
        f[2 + c] = j00 * y0 + j01 * y1;
        f[4 + c] = j10 * y0 + j11 * y1;
    }
}
void poincare(const double *x0, double eps, double b0, double m, double phase, int n, double *out) {
    double s[6] = {x0[0], x0[1], 1, 0, 0, 1}, k1[6], k2[6], k3[6], k4[6], w[6], h = 1.0 / n;
    for (int i = 0; i < n; i++) {
        double t = i * h;
        rhs(t, s, k1, eps, b0, m, phase);
        for (int j = 0; j < 6; j++) w[j] = s[j] + 0.5 * h * k1[j];
        rhs(t + 0.5 * h, w, k2, eps, b0, m, phase);
        for (int j = 0; j < 6; j++) w[j] = s[j] + 0.5 * h * k2[j];
        rhs(t + 0.5 * h, w, k3, eps, b0, m, phase);
        for (int j = 0; j < 6; j++) w[j] = s[j] + h * k3[j];
        rhs(t + h, w, k4, eps, b0, m, phase);
        for (int j = 0; j < 6; j++) s[j] += h / 6 * (k1[j] + 2 * k2[j] + 2 * k3[j] + k4[j]);
    }
    for (int j = 0; j < 6; j++) out[j] = s[j];
}
"""
STEPS = 8000  # RK4 steps per period of the forcing: the error is below 1e-11 (the check at 16000 steps agrees to that)
LIBRARY = []


def library():
    """The compiled map, once: gcc into a temporary folder, loaded through ctypes."""
    if not LIBRARY:
        folder = Path(tempfile.mkdtemp(prefix='w557-'))
        (folder / 'poincare.c').write_text(POINCARE_C)
        subprocess.run(['gcc', '-O2', '-ffp-contract=off', '-fPIC', '-shared', str(folder / 'poincare.c'), '-lm', '-o', str(folder / 'libpoincare.so')], check=True)
        lib = ctypes.CDLL(str(folder / 'libpoincare.so'))
        lib.poincare.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_int,
                                 ctypes.POINTER(ctypes.c_double)]
        LIBRARY.append(lib)
        shutil.rmtree(folder)  # loaded: the file is no longer needed
    return LIBRARY[0]


def poincare(state, eps, b0, m, phase):
    """The state after one period of the forcing and the monodromy matrix."""
    out = (ctypes.c_double * 6)()
    library().poincare((ctypes.c_double * 2)(*state), eps, b0, m, phase, STEPS, out)
    return np.array(out[:2]), np.array(out[2:]).reshape(2, 2)


class Cycle:
    """The period-1 cycle of the forced system, followed from the last one found (a Newton step from a nearby guess)."""

    def __init__(self, state, m, phase):
        self.state, self.m, self.phase = np.array(state, dtype=float), m, phase
        self.at = None  # the (eps, b0) of the last cycle found: where move_to starts from

    def multipliers(self, eps, b0):
        s = self.state.copy()
        for _ in range(MAX_NEWTON):
            image, M = poincare(s, eps, b0, self.m, self.phase)
            residual = image - s
            if np.abs(residual).max() < NEWTON_TOLERANCE:
                self.state, self.at = s, (eps, b0)
                return M
            try:
                s = s - np.linalg.solve(M - np.eye(2), residual)
            except np.linalg.LinAlgError:  # a multiplier 1: a fold of cycles, no Newton step
                break
        raise ArithmeticError('no period-1 cycle at eps %.12g b0 %.12g' % (eps, b0))

    def move_to(self, eps, b0):
        """The cycle at (eps, b0) when it is farther from the last one found than Newton's method reaches (XPPAUT's labelled points are
        up to 0.4 apart): followed from the last one along the straight segment in 2^k equal steps, k the first of 0 to MAX_HALVINGS
        at which every step converges. ArithmeticError if none does."""
        if self.at is None:
            self.multipliers(eps, b0)
            return
        start, state = np.array(self.at), self.state.copy()
        for k in range(MAX_HALVINGS + 1):
            try:
                for i in range(1, 2 ** k + 1):
                    self.multipliers(*(start + (np.array((eps, b0)) - start) * i / 2 ** k))
                return
            except ArithmeticError:
                self.state, self.at = state.copy(), tuple(start)  # the next, finer walk starts again from the last cycle found
        raise ArithmeticError('no period-1 cycle at eps %.12g b0 %.12g, nor on the way from eps %.12g b0 %.12g' % (eps, b0, *start))


def flip_condition(M):
    """Zero on the period-doubling curve (a real multiplier -1) and, past its R2 point where the two multipliers near -1
    have become a complex pair, on the unit circle: where the curve the programs follow continues as the pair's |mu| = 1."""
    ev = np.linalg.eigvals(M)
    if abs(ev[0].imag) > 0:
        return abs(ev[0]) - 1
    return min(ev, key=lambda v: abs(v + 1)).real + 1


def torus_condition(M):
    return np.linalg.det(M) - 1


def nearest_root(f, reset, admissible=lambda h: True):
    """The distance from a sample to the nearest zero of f, f(h) being the condition at offset h from it along a line: searched outward
    on GRID on each side, a sign change between two grid points solved by brentq and taken only if |f| there is ROOT_RESIDUAL or less,
    since a jump of f also changes sign (the flip test's switch between its real and complex forms, near a 1:2 resonance) and is not a
    zero, and admissible(h) holds (a torus curve is det = 1 only with a trace below 2 in modulus). Each side's walk starts from
    reset() (the sample's own cycle) and each step is seeded from the cycle the last step converged on. None if no zero lies
    within GRID on either side."""
    best = None
    for side in (-1.0, 1.0):
        reset()
        prev_h, prev_v = 0.0, f(0.0)
        for h in GRID:
            try:
                v = f(side * h)
            except ArithmeticError:  # no cycle on this step even from the sample's own: the side ends
                break
            if prev_v * v < 0:
                try:
                    root = brentq(f, side * prev_h, side * h, xtol=1e-14, rtol=1e-13)
                    ok = abs(f(root)) <= ROOT_RESIDUAL and admissible(root)
                except (ValueError, ArithmeticError):  # the bracket's ends changed sign under the solver: not a zero found
                    ok = False
                if ok:
                    d = abs(root)
                    best = d if best is None else min(best, d)
                    break
            prev_h, prev_v = h, v
    return best


def line_distance(cycle, own, point, direction, value, admissible=lambda M: True):
    """nearest_root along the line through point in direction (a unit vector): the nearest parameter change, either way, at which
    value(M) is zero, M the monodromy matrix of the cycle there and admissible(M) true."""
    def reset():
        cycle.state = own.copy()

    def monodromy(h):
        args = (point[0] + h * direction[0], point[1] + h * direction[1])
        try:
            return cycle.multipliers(*args)  # seeded from the last cycle this walk converged on
        except ArithmeticError:
            reset()  # retry once from the sample's own cycle
            return cycle.multipliers(*args)
    return nearest_root(lambda h: value(monodromy(h)), reset, lambda h: admissible(monodromy(h)))


def flip_curve(M):
    """Zero where a multiplier is -1 (the real flip curve), the product (mu1 + 1)(mu2 + 1)."""
    return np.linalg.det(M + np.eye(2))


def complex_pair(M):
    """Whether the multipliers are a complex pair or a double one, the only place det = 1 is a torus curve (|trace| <= 2)."""
    return abs(np.trace(M)) <= 2.0


PERIOD_DOUBLING_CURVES = ((flip_curve, lambda M: True), (torus_condition, complex_pair))
TORUS_CURVES = ((torus_condition, complex_pair),)


def plane_distance(cycle, own, point, union):
    """The smallest distance in the (eps, b0) plane from point to the union of the curves, each a (value, admissible) pair of functions of
    the monodromy matrix (line_distance). The distance to a curve is the least over directions of its nearest root along the line:
    the nearest curve point lies on some line through the sample, at its distance and on no line nearer. LINES directions are tried
    for each curve and the best one's refined (bounded golden-section search) to ANGLE_TOLERANCE; a curve with no root on any line
    within GRID counts as infinitely far. Uses only the model's equations: no program's curve enters. math.inf if no curve is found."""
    best = math.inf
    for value, admissible in union:
        def along(theta):
            d = line_distance(cycle, own, point, (math.cos(theta), math.sin(theta)), value, admissible)
            return math.inf if d is None else d
        coarse = [(along(math.pi * k / LINES), math.pi * k / LINES) for k in range(LINES)]
        d0, theta0 = min(coarse)
        if not math.isfinite(d0):
            continue
        half = math.pi / LINES
        refined = minimize_scalar(lambda t: min(along(t), 2 * d0), bounds=(theta0 - half, theta0 + half), method='bounded',
                                  options={'xatol': ANGLE_TOLERANCE})
        best = min(best, d0, refined.fun)
    return best


def distance_to_curve(condition, union, cycle, eps, b0):
    """The smaller of the parameter changes (b0 at the sample's eps, eps at its b0) that make `condition` zero, each the nearest zero
    on its axis (nearest_root); when neither axis has one, or the smaller is above AXIS_ENOUGH, the smaller of that and the plane's
    distance to the curves (plane_distance); nan if there is no root within GRID in the plane either."""
    best, kept = math.inf, cycle.state.copy()
    try:
        cycle.multipliers(eps, b0)
        own = cycle.state.copy()  # the sample's own cycle: where each walk starts, and where a failed step retries from
        for direction in ((1.0, 0.0), (0.0, 1.0)):
            d = line_distance(cycle, own, (eps, b0), direction, condition)
            if d is not None:
                best = min(best, d)
        if best > AXIS_ENOUGH:
            best = min(best, plane_distance(cycle, own, (eps, b0), union))
    finally:
        cycle.state = kept  # the cycles of the walks' ends are not the sample's, whether or not the search ended in an error
    cycle.multipliers(eps, b0)
    return best if math.isfinite(best) else math.nan


def stored(workload_dir, run):
    data = (workload_dir / 'pilot-xppautx' / 'diagram.csv').read_bytes()
    rows = autocompare.stored_rows(data, json.loads((workload_dir / 'pilot-xppautx' / 'run-ranges.json').read_text()))
    return rows[run]


def parameter_names(run):
    """The continuation parameters of a reference run, in its order (par, par2)."""
    return run['parameters'] if 'parameters' in run else [run['parameter']]


def read_s(path):
    """{label: (branch, par)} of an XPPAUT .s file (AUTO's fort.8 format, ten digits): each labelled point is a header of twelve
    integers (branch, point, type, label, ..., NTST, NCOL, NPARX), then its solution, then its NPARX parameters, which end its block."""
    blocks = []
    for line in path.read_text().splitlines():
        tok = line.split()
        if len(tok) == 12 and all(t.lstrip('-').isdigit() for t in tok):
            blocks.append(([int(t) for t in tok], []))
        elif blocks:
            blocks[-1][1].extend(float(t) for t in tok)
    return {hdr[3]: (hdr[0], vals[-hdr[11]:]) for hdr, vals in blocks}


def par_index(runs, blocks):
    """{name: index}: the index in a labelled point's parameter vector of each parameter the runs name. Their points' values are
    printed to six digits, so a value matches the point's own vector to XPPAUT_PAR_TOLERANCE (relative); a value that matches
    two indices, or a zero, is no evidence and is skipped. A name with no unambiguous index, or two, is an error."""
    index = {}
    for run in runs:
        names = parameter_names(run)
        for p in run['points']:
            if p['label'] not in blocks:
                continue
            par = blocks[p['label']][1]
            for name, key in zip(names, ('par', 'par2')):
                v = p.get(key)
                if not v:
                    continue
                hits = [i for i, x in enumerate(par) if abs(x - v) <= XPPAUT_PAR_TOLERANCE * abs(v)]
                if len(hits) != 1:  # equal values (a parameter at 1, say) tell nothing about which is which
                    continue
                if index.setdefault(name, hits[0]) != hits[0]:
                    sys.exit('%s: %s has two indices in the .s file' % (run['name'], name))
    for run in runs:
        for name in parameter_names(run):
            if name not in index:
                sys.exit('%s: no labelled point gives the index of %s' % (run['name'], name))
    return index


@functools.lru_cache(maxsize=None)
def xppaut_files(workload_dir):
    """{run: (labelled points, index)} for each reference run of XPPAUT: the labelled points {label: (branch, par)} of the .s file
    its series wrote, and the index (par_index) of its parameters in their vectors. A homoclinic run is in files/series-homoclinic
    when that folder exists, the others in files/series."""
    folder = workload_dir / 'pilot-xppaut'
    runs = json.loads((folder / 'reference.json').read_text())['runs']
    kinds = {r['name']: r['kind'] for r in json.loads((workload_dir / 'description.json').read_text())['runs']}
    homoclinic = folder / 'files' / 'series-homoclinic'
    by_series = {}
    for run in runs:
        series = homoclinic if kinds[run['name']] == 'homoclinic' and homoclinic.is_dir() else folder / 'files' / 'series'
        by_series.setdefault(series, []).append(run)
    out = {}
    for series, group in by_series.items():
        found = sorted(series.glob('*.s'))
        if len(found) != 1:
            sys.exit('%s: expected one .s file, found %d' % (series, len(found)))
        blocks = read_s(found[0])
        index = par_index(group, blocks)
        for run in group:
            out[run['name']] = (blocks, index)
    return out


def polyline_distance(point, curve):
    """The distance from a point to the polyline through a curve's samples (their first two columns)."""
    c = np.array([s[:2] for s in curve], dtype=float)
    if len(c) == 1:
        return float(np.linalg.norm(point - c[0]))
    a, ab = c[:-1], c[1:] - c[:-1]
    t = np.clip(((point - a) * ab).sum(1) / np.maximum((ab * ab).sum(1), 1e-300), 0.0, 1.0)
    return float(np.linalg.norm(point - (a + t[:, None] * ab), axis=1).min())


def xppaut_labelled(workload_dir, run):
    """[(par, par2)] of XPPAUT's labelled points that belong to a run, at ten digits (the .s file's own values): the points within
    MEMBER_TOLERANCE of the run's exported curve. The curve decides only which run a point belongs to; nothing is measured on it."""
    ref = next(r for r in json.loads((workload_dir / 'pilot-xppaut' / 'reference.json').read_text())['runs'] if r['name'] == run)
    names = parameter_names(ref)[:2]
    if len(names) < 2 or not ref.get('curve'):
        return []
    blocks, index = xppaut_files(workload_dir)[run]
    curve = [s[:2] for s in ref['curve']]
    out = []
    for label, (branch, par) in sorted(blocks.items()):
        values = np.array([par[index[n]] for n in names])
        if polyline_distance(values, curve) <= MEMBER_TOLERANCE:
            out.append(tuple(float(v) for v in values))
    return out


def curves(workload_dir, run, stride=1):
    """{program: [(par, par2, ...), ...]} of a run: xppautX's stored rows, AUTO-07p's reference curve and XPPAUT's labelled points."""
    out = {}
    rows = stored(workload_dir, run)
    out['xppautX'] = [(autocompare.quantity(r, 'par'), autocompare.quantity(r, 'par2')) for r in rows if 'continued_parameter2' in r]
    path = workload_dir / 'pilot-auto07p' / 'reference.json'
    if path.exists():
        ref = next(r for r in json.loads(path.read_text())['runs'] if r['name'] == run)
        if 'unavailable' not in ref and ref.get('curve'):
            out['AUTO-07p'] = [tuple(s[:2]) for s in ref['curve']]
    if (workload_dir / 'pilot-xppaut' / 'reference.json').exists():
        out['XPPAUT'] = xppaut_labelled(workload_dir, run)
        if not out['XPPAUT']:
            print('%-14s %-9s XPPAUT: no labelled point of the run lies on its curve (none checked)' % ('', run))
    return {program: samples[::stride] for program, samples in out.items()}


def special_value(workload_dir, run, kinds):
    """The parameter of the first point of a run with a type in kinds = (xppautX's name, the references' symbol): m at the PD or NS."""
    out = {}
    for r in stored(workload_dir, run):
        if r['itp'] == kinds[0]:
            out['xppautX'] = autocompare.quantity(r, 'par')
            break
    for program, folder in (('XPPAUT', 'pilot-xppaut'), ('AUTO-07p', 'pilot-auto07p')):
        if not (workload_dir / folder / 'reference.json').exists():
            continue
        ref = next(r for r in json.loads((workload_dir / folder / 'reference.json').read_text())['runs'] if r['name'] == run)
        for p in ref['points']:
            if p['type'] == kinds[1]:
                if program == 'XPPAUT':  # ten digits: the point's own vector, at the index of the run's parameter
                    blocks, index = xppaut_files(workload_dir)[run]
                    out[program] = blocks[p['label']][1][index[parameter_names(ref)[0]]]
                else:
                    out[program] = p['par']
                break
    return out


def report(label, run, program, distances, samples):
    d = np.array(distances)
    ok = np.isfinite(d)
    if not ok.any():
        print('%-14s %-9s %-9s %4d samples, none could be checked  [%s]' % (label, run, program, len(d), SOURCE[program]))
        return None
    k = int(np.nanargmax(d))
    print('%-14s %-9s %-9s %4d samples (%d unchecked)  max %.3e at (%.6g, %.6g)  rms %.3e  [%s]' % (
        label, run, program, len(d), int((~ok).sum()), d[k], samples[k][0], samples[k][1], math.sqrt(np.nanmean(d[ok] ** 2)), SOURCE[program]))
    return dict(program=program, run=run, source=SOURCE[program], samples=len(d), unchecked=int((~ok).sum()), max=float(d[k]),
                at=list(map(float, samples[k][:2])), rms=float(math.sqrt(np.nanmean(d[ok] ** 2))))


def predator_prey(workload_dir, runs, condition, union, special, start_type):
    results = []
    ms = special_value(workload_dir, 'cycles', special)
    for run in runs:
        first = stored(workload_dir, run)[0]
        for program, samples in curves(workload_dir, run, STRIDE).items():
            if not samples:
                continue
            # the starting cycle: xppautX's own first stored point of the curve (its phase and state), for every program
            phase = math.atan2(float(first['u0.V']), float(first['u0.U']))
            cycle = Cycle([float(first['u0.X']), float(first['u0.Y'])], ms[program], phase)
            distances = []
            for eps, b0 in samples:
                try:
                    cycle.move_to(eps, b0)
                    distances.append(distance_to_curve(condition, union, cycle, eps, b0))
                except ArithmeticError:
                    distances.append(math.nan)
            results.append(report(start_type, run, program, distances, samples))
    return results


def henon(workload_dir, runs, closed_form, in_range):  # a closed form of the curve: map or ODE
    results = []
    for run in runs:
        for program, samples in curves(workload_dir, run, STRIDE).items():
            if not samples:
                continue
            d = [abs(closed_form(a, b)) if in_range(a, b) else math.nan for a, b in samples]
            results.append(report('closed form', run, program, d, samples))
    return results


def lorenz_returns(r, b, s=10.0, eps=1e-7):
    """Whether the branch x > 0 of the origin's unstable manifold at (r, b) crosses x = 0 (it does for r above the homoclinic
    value, and then y < 0 there and tends to 0 with r; below it the branch ends at the equilibrium on its own side)."""
    A = np.array([[-s, s, 0], [r, -1, 0], [0, 0, -b]])
    w, v = np.linalg.eig(A)
    direction = v[:, int(np.argmax(w.real))].real
    direction = direction / np.linalg.norm(direction) * (1 if direction[0] > 0 else -1)

    def rhs(t, u):
        return [s * (u[1] - u[0]), r * u[0] - u[1] - u[0] * u[2], u[0] * u[1] - b * u[2]]

    def cross(t, u):
        return u[0] if t > 1.0 else 1.0  # not the departure from the origin
    cross.terminal, cross.direction = True, -1
    sol = solve_ivp(rhs, (0, LORENZ_TIME), eps * direction, events=cross, method='DOP853', rtol=RTOL, atol=1e-16)
    return len(sol.y_events[0]) > 0


def lorenz_distance(r, b):
    """|r - r*(b)|, r* the parameter where the branch changes from not returning to crossing x = 0: bisection to 1e-12."""
    here = lorenz_returns(r, b)
    for half in SEARCH + (1e-1, 1.0):
        other = r - half if here else r + half
        if lorenz_returns(other, b) != here:
            lo, hi = (other, r) if here else (r, other)  # lo: no crossing, hi: a crossing
            for _ in range(LORENZ_BISECTIONS):
                mid = (lo + hi) / 2
                lo, hi = (lo, mid) if lorenz_returns(mid, b) else (mid, hi)
            return abs((lo + hi) / 2 - r)
    return math.nan


def lorenz(workload_dir, runs):
    results = []
    for run in runs:
        for program, samples in curves(workload_dir, run, STRIDE).items():
            if not samples:
                continue
            results.append(report('lorenz', run, program, [lorenz_distance(r, b) for r, b in samples], samples))
    return results


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--attempt', type=Path, help='a continuation attempt folder: <attempt>/<workload>/adaptive')
    p.add_argument('--workload', help='the workload (the check chosen by it unless --case)')
    p.add_argument('--folder', type=Path, help='the workload folder itself (pilot-xppautx, pilot-auto07p, description.json), instead of --attempt')
    p.add_argument('--case', help='forced_pp_pd, forced_pp_ns, hen_pd, hen_tr, explicit_homoclinic or lorenz_homoclinic: the check, whatever the folder is called')
    p.add_argument('--runs', help='comma-separated curve runs (default: the workload\'s)')
    p.add_argument('--out', type=Path, help='write the summary as JSON')
    p.add_argument('--stride', type=int, default=1, help='check every n-th sample of each curve (the Lorenz check takes seconds a sample)')
    a = p.parse_args()
    global STRIDE
    STRIDE = a.stride
    folder = a.folder or a.attempt / a.workload / 'adaptive'
    case = a.case or a.workload
    description = json.loads((folder / 'description.json').read_text())
    runs = a.runs.split(',') if a.runs else [r['name'] for r in description['runs'] if r['kind'] in ('two_parameter', 'homoclinic')]
    if case == 'forced_pp_pd':
        results = predator_prey(folder, runs, flip_condition, PERIOD_DOUBLING_CURVES, ('period_doubling', 'PD'), 'forced_pp_pd')
    elif case == 'forced_pp_ns':
        results = predator_prey(folder, runs, torus_condition, TORUS_CURVES, ('neimark_sacker', 'TR'), 'forced_pp_ns')
    elif case == 'hen_pd':
        results = henon(folder, runs, lambda al, be: al - 0.75 * (1 + be) ** 2, lambda al, be: True)
    elif case == 'hen_tr':
        results = henon(folder, runs, lambda al, be: be - 1.0, lambda al, be: abs(al) < 3)
    elif case == 'explicit_homoclinic':
        results = henon(folder, runs, lambda q, k: k, lambda q, k: True)
    elif case == 'lorenz_homoclinic':
        results = lorenz(folder, runs)
    else:
        sys.exit('no independent check written for ' + str(case))
    if a.out:
        a.out.write_text(json.dumps([r for r in results if r], indent=1))


if __name__ == '__main__':
    main()
