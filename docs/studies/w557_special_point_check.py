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
  hen_pd, hen_tr  AUTO-07p's hen demo, the map (x, y) -> (y, alpha - beta x - y^2): the fixed point's Jacobian
      [[0, 1], [-beta, -2 y]] has det beta and trace -2 y, so the flip curve is alpha = 3 (1 + beta)^2 / 4 and the
      torus curve beta = 1 (|alpha| below 3: |trace| < 2).
  explicit_homoclinic  tools/models/explicit_homoclinic.odex (x = 3/2 sech^2(sqrt(q) t/2) is homoclinic at k = 0 for every q): a sample's
      distance from the curve is |k|.
  lorenz_homoclinic  The Lorenz equations (sigma 10): the branch x > 0 of the origin's unstable manifold, integrated with
      scipy, crosses the plane x = 0 for r above the homoclinic value and does not below it (the crossing's y tends to 0
      as r falls to it: the orbit runs down the z axis, the origin's stable direction -b). For each sample (r, b) this
      bisects, at that b, for the r where the crossing appears: the sample's distance from the curve.

    uv run --locked python docs/studies/w557_special_point_check.py --attempt DIR --workload forced_pp_pd [--runs pd_up,pd_down]

Prints, per run and program, the number of samples checked, the largest and root-mean-square distance and where the largest is.
"""
import argparse
import ctypes
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import brentq

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import autocompare  # noqa: E402

RTOL, ATOL = 1e-12, 1e-14  # of every integration here
NEWTON_TOLERANCE = 1e-12  # of the Poincare map's fixed point
MAX_NEWTON = 40  # steps before a fixed point is called not found
STRIDE = 1  # every STRIDE-th sample of a curve is checked (--stride)
LORENZ_TIME = 40.0  # the longest the unstable manifold is followed before it is said not to return
LORENZ_BISECTIONS = 36  # halvings of the bracket around the homoclinic value (the bracket is 1e-6 or more wide)
SEARCH = (1e-6, 1e-4, 1e-3, 1e-2)  # half-widths of the brackets tried when solving for a parameter that puts a sample on the curve
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

    def multipliers(self, eps, b0):
        s = self.state.copy()
        for _ in range(MAX_NEWTON):
            image, M = poincare(s, eps, b0, self.m, self.phase)
            residual = image - s
            if np.abs(residual).max() < NEWTON_TOLERANCE:
                self.state = s
                return M
            try:
                s = s - np.linalg.solve(M - np.eye(2), residual)
            except np.linalg.LinAlgError:  # a multiplier 1: a fold of cycles, no Newton step
                break
        raise ArithmeticError('no period-1 cycle at eps %.12g b0 %.12g' % (eps, b0))


def flip_condition(M):
    """Zero on the period-doubling curve (a real multiplier -1) and, past its R2 point where the two multipliers near -1
    have become a complex pair, on the unit circle: where the curve the programs follow continues as the pair's |mu| = 1."""
    ev = np.linalg.eigvals(M)
    if abs(ev[0].imag) > 0:
        return abs(ev[0]) - 1
    return min(ev, key=lambda v: abs(v + 1)).real + 1


def torus_condition(M):
    return np.linalg.det(M) - 1


def distance_to_curve(condition, cycle, eps, b0):
    """The smaller of the parameter changes (b0 at the sample's eps, eps at its b0) that make `condition` zero; nan if neither is bracketed."""
    best, kept = [], cycle.state.copy()
    for axis in (0, 1):
        def f(v, axis=axis):
            return condition(cycle.multipliers(*((v, b0) if axis == 0 else (eps, v))))
        centre = (eps, b0)[axis]
        for half in SEARCH:
            lo, hi = centre - half, centre + half
            try:
                if f(lo) * f(hi) < 0:
                    best.append(abs(brentq(f, lo, hi, xtol=1e-14, rtol=1e-13) - centre))
                    break
            except ArithmeticError:
                break
    cycle.state = kept  # the cycles of the brackets' ends are not the sample's
    cycle.multipliers(eps, b0)
    return min(best) if best else math.nan


def stored(workload_dir, run):
    data = (workload_dir / 'pilot-xppautx' / 'diagram.csv').read_bytes()
    rows = autocompare.stored_rows(data, json.loads((workload_dir / 'pilot-xppautx' / 'run-ranges.json').read_text()))
    return rows[run]


def curves(workload_dir, run, stride=1):
    """{program: [(par, par2, ...), ...]} of a run: xppautX's stored rows, and each reference's curve."""
    out = {}
    rows = stored(workload_dir, run)
    out['xppautX'] = [(autocompare.quantity(r, 'par'), autocompare.quantity(r, 'par2')) for r in rows if 'continued_parameter2' in r]
    for program, folder in (('XPPAUT', 'pilot-xppaut'), ('AUTO-07p', 'pilot-auto07p')):
        path = workload_dir / folder / 'reference.json'
        if path.exists():
            ref = next(r for r in json.loads(path.read_text())['runs'] if r['name'] == run)
            if 'unavailable' not in ref and ref.get('curve'):
                out[program] = [tuple(s[:2]) for s in ref['curve']]
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
                out[program] = p['par']
                break
    return out


def report(label, run, program, distances, samples):
    d = np.array(distances)
    ok = np.isfinite(d)
    if not ok.any():
        print('%-14s %-9s %-9s %4d samples, none could be checked' % (label, run, program, len(d)))
        return None
    k = int(np.nanargmax(d))
    print('%-14s %-9s %-9s %4d samples (%d unchecked)  max %.3e at (%.6g, %.6g)  rms %.3e' % (
        label, run, program, len(d), int((~ok).sum()), d[k], samples[k][0], samples[k][1], math.sqrt(np.nanmean(d[ok] ** 2))))
    return dict(program=program, run=run, samples=len(d), unchecked=int((~ok).sum()), max=float(d[k]), at=list(map(float, samples[k][:2])), rms=float(math.sqrt(np.nanmean(d[ok] ** 2))))


def predator_prey(workload_dir, runs, condition, special, start_type):
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
                    distances.append(distance_to_curve(condition, cycle, eps, b0))
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
        results = predator_prey(folder, runs, flip_condition, ('period_doubling', 'PD'), 'forced_pp_pd')
    elif case == 'forced_pp_ns':
        results = predator_prey(folder, runs, torus_condition, ('neimark_sacker', 'TR'), 'forced_pp_ns')
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
