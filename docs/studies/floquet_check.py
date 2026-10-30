"""An independent check of a cycle's Floquet multipliers (W550 #615, W551 #616).

A fold of cycles has a multiplier at 1 besides the trivial one, a branch point of cycles too, a period doubling
one at -1. This computes the multipliers of the cycle the model's own equations give at a parameter, with none
of xppautX's, XPPAUT's or AUTO-07p's continuation or collocation, and with the Jacobian differentiated from the
equations, not differenced (XPPAUT's and AUTO-07p's step 1e-7*(1+|u|) is too large for PY_S1Bf's tiny states,
docs/auto-references.md W455):

  - the equations are a C file with exact derivatives: PY_S1Bf's is the hand-written
    paper/benchmarks/auto/auto07p/py_s1bf.c (`--c`), any other model's is paper/benchmarks/auto/auto07p/exact_source.py's
    forward-mode differentiation of the model's own formulas (W459, checked there against Richardson differences);
    compiled here and called through ctypes;
  - the cycle is found by Newton's method on the return map to a section (a state crossing a level upward), from the
    state of the nearest point of the diagram baseline, the map's derivative from the variational equations below;
  - the monodromy matrix is the solution of the variational equations Y' = J(x) Y, Y(0) = I, over the period of
    that return, integrated column by column with the state (the equations are linear in Y; nothing in
    the solution is differenced); its eigenvalues are the multipliers. `--difference` also prints the multipliers of the
    monodromy matrix got by central differences of the flow, to show what differencing does to them.

    uv run --locked python docs/studies/floquet_check.py --description ode_mu26_control --section SmV=-20 \
        --parameter SmIapp 0.09994824 0.9990257
    uv run --locked python docs/studies/floquet_check.py --odex PY_S1Bf.odex --c paper/benchmarks/auto/auto07p/py_s1bf.c \
        --diagram diagram.csv --section v --parameter Iapp 16394.43 16409

Prints the period and the largest multipliers (by modulus) at each parameter value.
"""
import argparse
import csv
import re
import ctypes
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from scipy.integrate import solve_ivp

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'paper/benchmarks/auto/auto07p'))
import auto_reference as ar  # noqa: E402
import exact_source  # noqa: E402
from odex_model import Odex  # noqa: E402

AUTO_INCLUDE = Path.home() / 'xppautx-comparison/build/auto-07p/include'  # auto_f2c.h (integer, doublereal)
RTOL, ATOL = 1e-11, 1e-13
RELATIVE_STEP = 1e-6  # of a state's size, for --difference's central differences
STATE_FLOOR = 1e-2  # a state smaller than this is perturbed by RELATIVE_STEP times this
CONVERGED = 1e-9  # the section's return state, relative to the state (floor 1e-3), below which the cycle is found
MIN_PERIOD = 1.0  # a return sooner than this is the section's own crossing at the start of the integration
WARMUP = 3  # forward returns before Newton's method
MAX_NEWTON = 30  # Newton steps on the return map before the cycle is called not found
MAX_TIME = 1e6  # of one return
SHOWN = 4  # multipliers printed


def compile_equations(text_path, scratch):
    subprocess.run(['gcc', '-std=gnu17', '-O2', '-ffp-contract=off', '-fPIC', '-shared', '-I', str(AUTO_INCLUDE), str(text_path),
                    '-lm', '-o', str(scratch / 'libmodel.so')], check=True, capture_output=True)
    return ctypes.CDLL(str(scratch / 'libmodel.so'))


def nearest_cycle_point(diagram, odex, slot, target):
    """The state of the periodic point of a run continuing the parameter in `slot` (AUTO's PAR number) nearest `target`."""
    with open(diagram, newline='') as f:
        rows = [r for r in csv.DictReader(line for line in f if not line.startswith('branch,')) if float(r['per']) > 0 and int(r['icp1']) == slot]
    row = min(rows, key=lambda r: abs(float(re.split(r'[;\s]', r['values'])[0]) - target))
    return np.array([float(row['u0.' + s.upper()]) for s in odex.states])


class UserFunctions(ctypes.Structure):
    """auto_f2c.h's `user_function_list`: the equation file's functions are static, reached through this table."""
    _fields_ = [(name, ctypes.c_void_p) for name in ('func', 'stpnt', 'bcnd', 'icnd', 'fopt', 'pvls')] + [('uses_fortran', ctypes.c_int)]


FUNC = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, *[ctypes.c_void_p] * 3, ctypes.c_int, *[ctypes.c_void_p] * 3)
STPNT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_double, ctypes.c_void_p, ctypes.c_void_p)


def pointer(array):
    return array.ctypes.data_as(ctypes.c_void_p)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument('--description', help="a tests/auto description's name: its model, parameters and diagram baseline")
    source.add_argument('--odex', help='a model (.odex); needs --diagram')
    p.add_argument('--c', help='a hand-written C equation file with exact derivatives (default: generated from the model)')
    p.add_argument('--diagram', help='a diagram.csv whose periodic points start the cycle')
    p.add_argument('--section', required=True, help="NAME[=LEVEL]: the state crossing LEVEL upward once per cycle (default: the start state's own value)")
    p.add_argument('--parameter', required=True, help='the parameter checked')
    p.add_argument('--difference', action='store_true', help="also the multipliers of a central-difference monodromy matrix")
    p.add_argument('values', nargs='+', type=float)
    a = p.parse_args()
    if a.description:
        d = ar.read_description(ROOT / 'tests/auto' / (a.description + '.json'))
        odex, parameters, diagram = Odex(ROOT / d['model']), d.get('parameters', {}), ROOT / 'tests/auto' / (a.description + '.diagram.csv')
    else:
        d = dict(runs=[dict(parameter=a.parameter)])
        odex, parameters, diagram = Odex(Path(a.odex)), {}, Path(a.diagram)
    name, _, level = a.section.partition('=')
    section_index = odex.states.index(name)
    n = len(odex.states)
    with tempfile.TemporaryDirectory(prefix='floquet-') as tmp:
        scratch = Path(tmp)
        eqs = ar.Equations(odex, parameters)
        if a.c:
            c_file = Path(a.c)
        else:
            c_file = scratch / 'exact.c'
            c_file.write_text(exact_source.source(eqs, [0.0] * n, d))
        lib = compile_equations(c_file, scratch)
        par = np.zeros(max(eqs.npar, ar.PERIOD))
        user = UserFunctions.in_dll(lib, 'user')
        compiled_func, compiled_stpnt = FUNC(user.func), STPNT(user.stpnt)
        compiled_stpnt(n, 0.0, pointer(np.zeros(n)), pointer(par))
        for k, v in parameters.items():
            par[eqs.slot[k] - 1] = v
        slot = eqs.slot[a.parameter] - 1
        icp = np.array([slot + 1], dtype=np.intc)

        def func(u, ijac):
            f, dfdu, dfdp = np.empty(n), np.zeros((n, n), order='F'), np.zeros((n, max(slot + 1, 1)), order='F')
            compiled_func(n, pointer(u), pointer(icp), pointer(par), ijac, pointer(f), pointer(dfdu), pointer(dfdp))
            return f, dfdu

        def rhs(t, u):
            return func(u, 0)[0]

        def variational(t, w):
            f, j = func(w[:n], 1)
            return np.concatenate([f, j @ w[n:]])

        def flow(x, time, events=None):
            return solve_ivp(rhs, (0, time), x, method='LSODA', rtol=RTOL, atol=ATOL, events=events)

        for target in a.values:
            par[slot] = target
            x = nearest_cycle_point(diagram, odex, slot + 1, target)
            at = float(level) if level else x[section_index]

            def crossing(t, u):
                return u[section_index] - at
            crossing.direction = 1
            crossing.terminal = True

            def monodromy(x0, period):
                """the variational equations over `period`, one column at a time, with the state"""
                return np.array([solve_ivp(variational, (0, period), np.concatenate([x0, np.eye(n)[:, k]]), method='LSODA', rtol=RTOL, atol=ATOL).y[n:, -1]
                                 for k in range(n)]).T

            def section_return(x0):
                """the time and state of the next upward crossing after x0 (a state on the section or not): the first
                MIN_PERIOD is integrated before the crossing is watched, so the one x0 is on does not count"""
                x_after = flow(x0, MIN_PERIOD).y[:, -1]
                r = flow(x_after, MAX_TIME, crossing)
                if not len(r.t_events[0]):
                    raise SystemExit('no crossing of %s = %r up to t = %g at %s = %r' % (name, at, MAX_TIME, a.parameter, target))
                return MIN_PERIOD + r.t_events[0][0], r.y_events[0][0]

            x0 = section_return(x)[1]
            unit = np.zeros(n)
            unit[section_index] = 1.0
            for _ in range(WARMUP):  # a few returns of the flow, which pull a stable cycle's neighbours in
                x0 = section_return(x0)[1]
            for _ in range(MAX_NEWTON):
                # Newton's method on the return map P (x0 on the section): DP = (I - f(x1) e^T / (e . f(x1))) Phi, e the section's state
                period, x1 = section_return(x0)
                if np.max(np.abs(x1 - x0) / np.maximum(np.abs(x0), 1e-3)) < CONVERGED:
                    break
                f1 = func(x1, 0)[0]
                dp = (np.eye(n) - np.outer(f1, unit) / f1[section_index]) @ monodromy(x0, period)
                step = np.linalg.lstsq(dp - np.eye(n), -(x1 - x0), rcond=None)[0]
                x0 = x0 + step - unit * step[section_index]  # stay on the section
            else:
                raise SystemExit('the cycle did not converge in %d Newton steps at %s = %r' % (MAX_NEWTON, a.parameter, target))
            monodromies = {'exact': monodromy(x0, period)}
            if a.difference:
                phi = np.empty((n, n))
                for i in range(n):
                    h = RELATIVE_STEP * max(abs(x0[i]), STATE_FLOOR)
                    ends = []
                    for sign in (1, -1):
                        start = x0.copy()
                        start[i] += sign * h
                        ends.append(flow(start, period).y[:, -1])
                    phi[:, i] = (ends[0] - ends[1]) / (2 * h)
                monodromies['difference'] = phi
            for kind, phi in monodromies.items():
                multipliers = sorted(np.linalg.eigvals(phi), key=lambda m: -abs(m))
                print('%s %r: period %.8f, %s Jacobian, largest multipliers %s' % (a.parameter, target, period, kind, ', '.join(
                    '%.8g%+.3gj' % (m.real, m.imag) if m.imag else '%.8g' % m.real for m in multipliers[:SHOWN])))


if __name__ == '__main__':
    main()
