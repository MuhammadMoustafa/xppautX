#!/usr/bin/env python3
"""tools/python-integration/driver.py: a run of a model, as xppautX's integrate loop makes it (W427, #489).

core/integrate.cpp integrate() and usual_integrate_stuff(): the number of steps from the total time and
dt, a fixed-step method taking store_every steps between the rows it stores and an adaptive one going on
to each output time, the auxiliary quantities worked out at every stored point, a state NaN or past the
bound stopping the run, the rows below the transient left out, and each row stored as single precision
floats (core/storage.h), which is what xppautX's output.dat holds.
"""

from collections import namedtuple

import numpy as np

from flags import Flags, flag_loop
from integrators import Dop853, IntegrationError, backward_euler_step, discrete_step, euler_step, qualrk_advance, rk4_step
from model import ModelError, compile_model
from numerics import Unreadable, apply_xppaut_set, numerics_of
from odex_model import Odex, Refused

# the rows stored (single precision, as xppautX stores them), their column names, the times of the rows as the run had them
# (double precision) and the (time, event number) of each event that fired
Run = namedtuple('Run', 'rows columns times fired')

FIXED_STEP = {'euler': euler_step, 'runge_kutta': rk4_step, 'discrete': discrete_step, 'backward_euler': backward_euler_step}
ADAPTIVE = ('qualrk', 'dopri8')
METHODS = tuple(FIXED_STEP) + ADAPTIVE
# what a run raises when it cannot be made: a model or file that is not translated or read, a method that cannot go on
FAILURES = (Refused, Unreadable, IntegrationError, ModelError)


def load(path, setfile=None):
    """The compiled model and the numerics of the model in path, under an XPPAUT set file if given."""
    odex = Odex(path)
    numerics = numerics_of(odex)
    if setfile:
        apply_xppaut_set(odex, numerics, setfile)
    return compile_model(odex), numerics


def run_model(model, numerics, method=None):
    """The rows of a run of the model with the numerics (a method of METHODS overrides the numerics')."""
    method = method or numerics.method
    if method not in METHODS:
        raise Refused('the method %s is not translated (%s are)' % (method, ', '.join(METHODS)))
    if numerics.dt <= 0:
        raise Refused('a step dt of %g: only a forward step is translated' % numerics.dt)
    flags = Flags(model, numerics.stol)
    t, y = numerics.start_time, flags.start(numerics.start_time, model.y0.copy())
    names = ['t'] + model.states + model.aux_names
    rows, times = [], []

    def store(t, y):
        aux = model.aux(t, y)
        x = np.concatenate((y, aux))
        for name, value in zip(names[1:], x):
            if np.isnan(value):
                raise IntegrationError('%s is NaN at t = %s' % (name, repr(t)))
            if abs(value) > numerics.bound:
                raise IntegrationError('%s out of bounds at t = %s' % (name, repr(t)))
        if not abs(t) < numerics.transient_time:
            rows.append(np.concatenate(([t], x)).astype(np.float32))
            times.append(t)

    if not abs(t) < numerics.transient_time:
        rows.append(np.concatenate(([t], y, model.aux(t, y))).astype(np.float32))
        times.append(t)
    # core/numerics.cpp do_meth: a map steps by 1, and a method that picks its own steps stores every output time
    dt = 1.0 if method == 'discrete' else numerics.dt
    store_every = numerics.store_every if method in FIXED_STEP else 1
    steps = int(numerics.total_time) if method == 'discrete' else int((numerics.total_time + abs(dt) * 0.1) / abs(dt))
    outputs = (steps + store_every - 1) // store_every
    rhs = model.rhs
    tzero, hguess = t, dt

    def step(t, y, dtt):
        """One step of a fixed-step method of dtt, or QualRK's advance on to tout (dtt is not used)."""
        nonlocal hguess
        if method == 'backward_euler':
            return backward_euler_step(rhs, t, y, dtt, numerics)
        if method == 'qualrk':
            tnext, ynext, hguess = qualrk_advance(rhs, t, y, hguess, tout, numerics)
            return tnext, ynext
        return FIXED_STEP[method](rhs, t, y, dtt)

    solver = Dop853(model, flags, numerics) if method == 'dopri8' else None
    for count in range(outputs):
        tout = tzero + dt * (count + 1)
        if solver:
            t, y = solver.advance(t, y, tout)
        elif method == 'qualrk':
            t, y = flag_loop(flags, step, t, y, 0.0, tend=tout) if model.events else step(t, y, 0.0)
        else:
            for _ in range(store_every):
                t, y = flag_loop(flags, step, t, y, dt) if model.events else step(t, y, dt)
        store(t, y)
    return Run(np.array(rows), names, times, flags.fired)


def format_rows(rows):
    """xppautX's output.dat: each value as %.8g and a blank after it (core/data_formats.cpp), a row to a line."""
    return ''.join(''.join('%.8g ' % value for value in row) + '\n' for row in rows)
