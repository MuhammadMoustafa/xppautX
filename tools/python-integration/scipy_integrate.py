#!/usr/bin/env python3
"""tools/python-integration/scipy_integrate.py: SciPy's DOP853 as the accuracy reference of a run (W427, #489).

    uv run python tools/python-integration/scipy_integrate.py MODEL.odex [--method KEY] [--setfile FILE] [--outfile FILE]

The model's equations integrated by scipy.integrate.solve_ivp, method DOP853 at a tolerance far
below any of the methods' own, and written as xppautX's output.dat at the stored times of the run
of --method (the model's own by default). Not timed and not one of the compared programs: it says
how far each program's method is from the solution. A model with events is integrated up to each
event, the event's assignments made, and integrated on. A map has no solution to integrate.
"""

import argparse
from pathlib import Path
import sys

import numpy as np
from scipy.integrate import solve_ivp

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/: odex_model

from driver import FAILURES, format_rows, load, run_model

# the reference's tolerance (relative and absolute): many orders below the error of any method compared with it here (a few 1e-4 at best)
REFERENCE_TOLERANCE = 1e-12
# a model with events gets steps no longer than this: the equations may blow up in finite time past an event's threshold
# (Izhikevich's v' = 0.04 v^2 + ... does), and a long trial step towards it would overflow before the event is seen
EVENT_MAX_STEP = 0.1


class NoSolution(Exception):
    """SciPy cannot integrate the model."""


def run_dop853(model, numerics, times):
    """The rows (time, variables, auxiliary quantities) of the model at the given times, and (time, event number)
    of each event that fired."""
    if numerics.method == 'discrete':
        raise NoSolution('a map has no solution to integrate')
    events = []
    for number, e in enumerate(model.events):
        def crossing(t, y, e=e):
            return e.cond(t, y)
        crossing.terminal = True
        crossing.direction = e.sign
        events.append(crossing)
    t, y = numerics.start_time, model.y0.copy()
    times, rows, fired = list(times), [], []
    while len(rows) < len(times):
        wanted = times[len(rows):]
        solution = solve_ivp(model.rhs, (t, wanted[-1]), y, method='DOP853', t_eval=wanted, events=events or None,
                             rtol=REFERENCE_TOLERANCE, atol=REFERENCE_TOLERANCE, max_step=EVENT_MAX_STEP if events else np.inf)
        if not solution.success:
            raise NoSolution(solution.message)
        rows += [np.concatenate(([ti], yi, model.aux(ti, yi))) for ti, yi in zip(solution.t, solution.y.T)]
        if solution.status != 1:
            break
        # an event stopped the integration: the earliest one, then its assignments, then on from there
        crossed = min((te[0], i) for i, te in enumerate(solution.t_events) if len(te))
        t = crossed[0]
        y = model.events[crossed[1]].act(t, solution.y_events[crossed[1]][0])
        fired.append((t, crossed[1]))
    return np.array(rows), fired


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('model', type=Path, help='the .odex model')
    parser.add_argument('--method', help="the method whose run's stored times to use (default the model's own)")
    parser.add_argument('--setfile', type=Path, help='an XPPAUT set file to apply (as xppautX --setfile)')
    parser.add_argument('--outfile', type=Path, default=Path('output.dat'), help='the output (default output.dat)')
    args = parser.parse_args()
    try:
        model, numerics = load(args.model, args.setfile)
        rows, _ = run_dop853(model, numerics, run_model(model, numerics, args.method).times)
    except FAILURES + (NoSolution,) as e:
        sys.exit('%s: %s' % (args.model, e))
    args.outfile.write_text(format_rows(rows), encoding='utf-8')


if __name__ == '__main__':
    main()
