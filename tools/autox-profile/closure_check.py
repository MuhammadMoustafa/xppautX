#!/usr/bin/env python3
"""W687 (#762): an independent check of the cycle branches a diagram calls closed, from the model's own equations.

    uv run --locked python tools/autox-profile/closure_check.py [--diagram CSV] [--model ODEX] [--binary xppautX] [--no-port-check]

A branch of cycles ends as `branch_closed` when it comes back to its start by autoX's same-point rule
(core/autox_ends.h). This asks, with none of xppautX's, XPPAUT's or AUTO-07p's continuation or collocation, whether
the cycle the branch ends at is the cycle it started from. The model's right-hand side is tools/python-integration's
translation of the .odex (formulas put in dependency order: the model defines G_S_Ah, say, after the first formula that
reads it, which xppautX's evaluation in definition order leaves to the previous call's value); for each closed branch and
each cycle it starts and ends at:

  - the cycle is integrated from the stored point over the stored period (scipy LSODA at RTOL, ATOL) and the return error
    says whether the stored point is a cycle of the model at its parameter;
  - it is refined to the exact cycle at the START's parameter value (Newton's method on the shooting equations Phi_T(x) = x
    with the phase fixed by f(x_stored) . (x - x_stored) = 0, the period T unknown, the flow's derivative differenced),
    since two cycles are the same only at one parameter: the stored points sit at parameters that differ by a few 1e-7
    and a cycle next to a Hopf point or a fold changes a lot over that;
  - the refined cycles are compared: the period, and how far the end's initial state lies from the start's orbit (the
    smallest scaled distance, over time shifts, of the end's state to the start's dense orbit);
  - the Floquet multipliers of each refined cycle (the monodromy matrix by central differences of the flow) and the
    multipliers' closeness to 1: a branch point of cycles has the trivial multiplier and one more at 1.

Before it trusts the port it integrates the first stored cycle with xppautX itself (`--silent`, fourth order Runge-Kutta
at dt) and compares the states along the way with the port's.
"""
import argparse
import csv
import math
from pathlib import Path
import sys

import numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import minimize_scalar

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/python-integration'))
from autoxcompare import diagram_rows  # noqa: E402
from model import compile_model  # noqa: E402
from odex_model import Odex  # noqa: E402
from xppclient import silent_rows  # noqa: E402

DIAGRAM = ROOT / 'tests/autox/ode_motoneuron6c_follow_all.diagram.csv'
MODEL = ROOT / 'examples/ode/motoneuron6c.odex'
PARAMETER = 'ampPar'  # the continued parameter of that run
RTOL, ATOL = 1e-11, 1e-13  # of every integration here (the stored cycles close to about 1e-10 of it)
RELATIVE_STEP = 1e-5  # of a state's size, for the flow's differences (central: the cycles' multipliers reach 25)
STATE_FLOOR = 1e-2  # a state smaller than this is perturbed by RELATIVE_STEP times this
NEWTON_STEPS = 30  # on the shooting equations, before a cycle is called not found
CONVERGED = 1e-9  # the shooting residual's largest scaled component below which the cycle is found
ORBIT_SAMPLES = 4000  # of the start's orbit, from which the nearest point to the end's state is refined
EPSS = 1e-4  # special_point_tolerance of the run (the default), the same-point rule's unit
SAME_POINT_FACTOR = 8  # autox_same_point_factor (core/autox_ends.h)
SHOWN_POINTS = 5  # a branch of more points shows only its first and last
PORT_STEP = 0.001  # dt of xppautX's Runge-Kutta run in the port check
PORT_TOLERANCE = 1e-6  # the largest scaled difference of the port and xppautX that is called agreement (xppautX prints 8 digits)


def in_dependency_order(odex):
    """The model's formulas with each placed after the formulas it reads."""
    table = dict(odex.assigns)
    deps = {n: {t for t in odex.expand(toks) if t in table and t != n} for n, toks in odex.assigns}
    order, seen = [], set()

    def visit(name):
        if name not in seen:
            seen.add(name)
            for d in sorted(deps[name]):
                visit(d)
            order.append(name)
    for name, _ in odex.assigns:
        visit(name)
    odex.assigns = [(n, table[n]) for n in order]


class Port:
    """The model compiled at a parameter value (compiled once per value)."""
    def __init__(self, path):
        self.path = path
        self.models = {}
        self.states = Odex(path).states

    def at(self, parameter):
        if parameter not in self.models:
            odex = Odex(self.path)
            in_dependency_order(odex)
            odex.pars[PARAMETER] = parameter
            self.models[parameter] = compile_model(odex)
        return self.models[parameter]


class Cycle:
    """A point of a branch of cycles: its parameter, its first mesh state and its period."""
    def __init__(self, row, states):
        self.parameter = float(row['values'].split(';')[0])
        self.x = np.array([float(row['u0.' + s.upper()]) for s in states])
        self.period = float(row['per'])
        self.hi = np.array([float(row['uhi.' + s.upper()]) for s in states])
        self.lo = np.array([float(row['ulo.' + s.upper()]) for s in states])
        self.label = (int(row['ibr']), int(row['ntot']), row['itp'], int(row['lab']))


def flow(model, x, time, dense=False):
    return solve_ivp(lambda t, y: model.rhs(t, y), (0, time), x, method='LSODA', rtol=RTOL, atol=ATOL, dense_output=dense)


def scaled(a, b):
    return np.max(np.abs(a - b) / (1 + np.abs(b)))


def step_of(x):
    return RELATIVE_STEP * np.maximum(np.abs(x), STATE_FLOOR)


def monodromy(model, x, period):
    """The flow's derivative over the period, by central differences"""
    cols = []
    for i in range(len(x)):
        h = step_of(x)[i]
        ends = []
        for sign in (1, -1):
            start = x.copy()
            start[i] += sign * h
            ends.append(flow(model, start, period).y[:, -1])
        cols.append((ends[0] - ends[1]) / (2 * h))
    return np.array(cols).T


def refine(model, cycle):
    """The exact cycle of the model near the stored one: Newton's method on (x, T) with the phase fixed. Returns x, T and
    the shooting residual reached: the steps stop at CONVERGED, or when one no longer lowers the residual by half (an
    unstable cycle's flow is differenced no better than that)"""
    n = len(cycle.x)
    x, period = cycle.x.copy(), cycle.period
    f_ref = model.rhs(0, cycle.x)
    last = math.inf
    for _ in range(NEWTON_STEPS):
        residual = flow(model, x, period).y[:, -1] - x
        size = np.max(np.abs(residual) / (1 + np.abs(x)))
        if size < CONVERGED or size > last / 2:
            return x, period, size
        last = size
        end = x + residual
        jac = np.zeros((n + 1, n + 1))
        jac[:n, :n] = monodromy(model, x, period) - np.eye(n)
        jac[:n, n] = model.rhs(0, end)
        jac[n, :n] = f_ref
        step = np.linalg.lstsq(jac, -np.concatenate([residual, [f_ref @ (x - cycle.x)]]), rcond=None)[0]
        x, period = x + step[:n], period + step[n]
    raise SystemExit('the cycle of %s did not converge in %d Newton steps' % (cycle.label, NEWTON_STEPS))


def extremes(model, x, period, index):
    """The smallest and largest value of a state over the cycle"""
    sol = flow(model, x, period, dense=True).sol
    values = sol(np.linspace(0, period, ORBIT_SAMPLES))[index]
    return values.min(), values.max()


def distance_to_orbit(model, orbit_x, period, x):
    """The smallest scaled distance of x from the orbit through orbit_x: over the shifts of the orbit's time."""
    sol = flow(model, orbit_x, period, dense=True).sol
    times = np.linspace(0, period, ORBIT_SAMPLES, endpoint=False)
    distances = [scaled(sol(t), x) for t in times]
    k = int(np.argmin(distances))
    around = (times[k] - period / ORBIT_SAMPLES, times[k] + period / ORBIT_SAMPLES)
    best = minimize_scalar(lambda t: scaled(sol(t % period), x), bounds=around, method='bounded', options={'xatol': 1e-12})
    return min(best.fun, distances[k])


def multipliers(model, x, period):
    return sorted(np.linalg.eigvals(monodromy(model, x, period)), key=lambda m: -abs(m))


def text(m):
    return '%.9g%+.3gj' % (m.real, m.imag) if abs(m.imag) > 1e-9 * max(1, abs(m.real)) else '%.9g' % m.real


def rule_gap(a, b, parameter_a, parameter_b):
    """core/autox_ends.cpp's autox_point_gap of two cycles: the largest ratio of a difference to EPSS * (1 + |a|) over the
    parameter and every state's maximum and minimum, in the units of the same-point rule (by_state) and of the closing test
    (W687: a state's unit is at most the start's largest range, closing_by_state)"""
    ratio = lambda p, q, cap=math.inf: abs(p - q) / (EPSS * min(1 + abs(p), cap))
    size = max(a.hi - a.lo)
    pairs = list(zip(np.concatenate([a.hi, a.lo]), np.concatenate([b.hi, b.lo])))
    return ratio(parameter_a, parameter_b), max(ratio(p, q) for p, q in pairs), max(ratio(p, q, size) for p, q in pairs)


def baseline_closures(directory):
    """Every closure in the baselines of a directory of descriptions (tests/autox/*.diagram.csv): the same-point rule's gap
    between the branch's start and the closing point, and for cycles how it compares with the start's own size: the largest
    difference of a maximum or minimum, and the cycles' amplitude (autox_orbit_amplitude: the largest range of a state,
    relative to 1 + |its mean|)"""
    for path in sorted(directory.glob('*.diagram.csv')):
        rows = diagram_rows(path.read_bytes())
        for closing in (r for r in rows if r['itp'] == 'branch_closed'):
            branch = [r for r in rows if r['ibr'] == closing['ibr'] and r['calc'] == closing['calc']]
            start = branch[0]
            names = [k[3:] for k in start if k.startswith('u0.')]
            periodic = closing['calc'] == 'periodic'
            values = lambda r: [float(r[k + '.' + n]) for k in (('uhi', 'ulo') if periodic else ('u0',)) for n in names]
            parameter = lambda r: float(r['values'].split(';')[0])
            ratio = lambda p, q: abs(p - q) / (EPSS * (1 + abs(p)))
            by_state = max(ratio(p, q) for p, q in zip(values(start), values(closing)))
            line = '%s branch %s (%s, %d points, closed at label %s): parameter %.3g, states %.3g of the unit' % (
                path.name[:-len('.diagram.csv')], closing['ibr'], closing['calc'], len(branch), closing['lab'],
                ratio(parameter(start), parameter(closing)), by_state)
            if periodic:
                hi, lo, bar = ([float(start[k + '.' + n]) for n in names] for k in ('uhi', 'ulo', 'ubar'))
                amplitude = max((h - l) / (1 + abs(b)) for h, l, b in zip(hi, lo, bar))
                shift = max(abs(p - q) for p, q in zip(values(start), values(closing)))
                line += '; amplitude %.3g, the largest difference of an extreme %.3g (%.3g of the largest range)' % (
                    amplitude, shift, shift / max(h - l for h, l in zip(hi, lo)))
            print(line)


def port_check(port, cycle, binary):
    """The first stored cycle integrated by xppautX (--silent, Runge-Kutta at PORT_STEP) and by the port, compared along the way."""
    states = port.states
    inits = ', '.join('%s = %r' % (s, float(v)) for s, v in zip(states, cycle.x))

    def edit(source):
        lines = [l for l in source.splitlines() if not l.startswith('init ')]
        text_ = '\n'.join(lines).replace('par %s = 0' % PARAMETER, 'par %s = %r' % (PARAMETER, cycle.parameter))
        text_ = text_.replace('options {', 'init %s\noptions {' % inits, 1)
        return text_.replace('dt: 0.025, method: backward_euler, total_time: 600',
                             'dt: %r, method: runge_kutta, total_time: %r' % (PORT_STEP, cycle.period))
    rows = np.array(silent_rows(str(binary), str(port.path), edit=edit))
    sol = flow(port.at(cycle.parameter), cycle.x, cycle.period, dense=True).sol
    worst = max(scaled(row[1:1 + len(states)], sol(row[0])) for row in rows[::100])
    return worst, len(rows)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--diagram', type=Path, default=DIAGRAM)
    p.add_argument('--model', type=Path, default=MODEL)
    p.add_argument('--binary', type=Path, default=ROOT / 'xppautX')
    p.add_argument('--no-port-check', action='store_true')
    p.add_argument('--baselines', type=Path, nargs='?', const=ROOT / 'tests/autox', help="only list the closures of a directory's diagram baselines")
    args = p.parse_args()
    if args.baselines:
        baseline_closures(args.baselines)
        return 0
    port = Port(args.model)
    rows = diagram_rows(args.diagram.read_bytes())
    cycles = [r for r in rows if r['calc'] == 'periodic']
    closed = [r for r in cycles if r['itp'] == 'branch_closed']
    if not args.no_port_check:
        first = Cycle(next(r for r in cycles if r['ntot'] == '1'), port.states)
        worst, count = port_check(port, first, args.binary)
        print('port check: xppautX %d rows over one period of the cycle %s, the largest scaled difference from scipy %.3g (agreement below %g)'
              % (count, first.label, worst, PORT_TOLERANCE))
        if worst > PORT_TOLERANCE:
            raise SystemExit('the port does not agree with xppautX')
    for end_row in closed:
        branch = [r for r in cycles if r['ibr'] == end_row['ibr']]
        start = Cycle(branch[0], port.states)
        print('\nbranch %d: %d points, started at label %s, closed at label %s' % (
            int(end_row['ibr']), len(branch), branch[0]['from'], end_row['lab']))
        points = [Cycle(r, port.states) for r in branch]
        end = points[-1]
        by_parameter, by_state, closing = rule_gap(start, end, start.parameter, end.parameter)
        print('  same-point rule: parameter %.3g, states %.3g of the unit (accepted up to %d), in the closing test\'s units %.3g; period differs by %.3g (%.3g of the unit)' % (
            by_parameter, by_state, SAME_POINT_FACTOR, closing, abs(start.period - end.period), abs(start.period - end.period) / (EPSS * (1 + start.period))))
        for point in (points if len(points) <= SHOWN_POINTS else [points[0], points[-1]]):
            model = port.at(point.parameter)
            residual = scaled(flow(model, point.x, point.period).y[:, -1], point.x)
            print('  point %d (%s): %s = %.12g, period %.9g, VS from %.6f to %.6f, return error of the stored cycle %.2g' % (
                point.label[1], point.label[2], PARAMETER, point.parameter, point.period, point.lo[0], point.hi[0], residual))
        model = port.at(start.parameter)
        refined = {}
        for name, point in (('start', start), ('end', end)):
            x, period, size = refine(model, point)
            refined[name] = (x, period)
            low, high = extremes(model, x, period, 0)
            mu = multipliers(model, x, period)
            nearest = sorted(mu, key=lambda m: abs(m - 1))[:3]
            print('  %s refined at %s = %.12g (shooting residual %.2g): period %.9g (stored %.9g), VS from %.6f to %.6f; largest multipliers %s; nearest to 1: %s' % (
                name, PARAMETER, start.parameter, size, period, point.period, low, high, ', '.join(text(m) for m in mu[:4]),
                ', '.join(text(m) for m in nearest)))
        (xa, ta), (xb, tb) = refined['start'], refined['end']
        print('  at the start\'s parameter the end is %.3g (scaled) from the start\'s orbit, its period %.3g (%.3g of the start\'s) off'
              % (distance_to_orbit(model, xa, ta, xb), abs(ta - tb), abs(ta - tb) / ta))
    return 0


if __name__ == '__main__':
    sys.exit(main())
