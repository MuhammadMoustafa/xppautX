#!/usr/bin/env python3
"""tools/python-integration/integrators.py: xppautX's integration methods, step for step, in Python with numpy (W427, #489).

Euler, Runge-Kutta (4), discrete maps, backward Euler (Newton's iteration on a finite-difference
Jacobian, LINPACK's LU), QualRK (the adaptive Cash-Karp step) and DoPri8(3) (Dormand-Prince 8(5,3)
with its dense output). Each does the operations of its routine in core/ in the order core/ does
them (core/odesol2.cpp, core/stiff.cpp, core/dormpri.cpp), so a run is the same arithmetic as
xppautX's up to the library functions of the model's formulas. A method that cannot go on raises
IntegrationError with xppautX's words. No SciPy.
"""

import math
import sys

import numpy as np

import dop853_coefficients as K

# core/stiff.cpp: QualRK's step control (Numerical Recipes' rkqs) and its limit on steps between two output times
SAFETY, PGROW, PSHRNK, ERRCON, TINY, MAXSTP = 0.9, -0.2, -0.25, 1.89e-4, 1.0e-30, 100000
# core/stiff.cpp: a rejected step is shortened to no less than this fraction, an accepted one lengthened to at most this multiple
SHRINK_LIMIT, GROW_LIMIT = 0.1, 5.0
# core/dormpri.h: the step attempts (accepted and rejected) of one run
DOPRI_RUN_ATTEMPTS = 100000
# core/dormpri.cpp dop853's defaults, as the solver calls it (every argument 0): the machine precision it assumes, the safety
# factor, the bounds of a step's change, and Lund stabilisation's beta (Hairer, Norsett and Wanner)
UROUND, DP_SAFE, DP_FAC1, DP_FAC2, DP_BETA = 2.3e-16, 0.9, 0.333, 6.0, 0.0
# core/dormpri.cpp DormandPrince: a step is aimed at most this many maximum steps ahead of the last
HORIZON_STEPS = 2
# core/integrate.h: the pieces a step's interval is cut in, to look for a sign change of an event's condition
DENSE_EVENT_SUBDIVISIONS = 32
# core/integrate.cpp dense_event: events closer than this many ulps of the time are simultaneous
SIMULTANEOUS_TIME_ULPS = 128
# core/xpp_math.cpp bracketed_root: iterations at most, the shortest bracket (in ulps of the time) and how far from an end a secant may land
ROOT_ITERATIONS, ROOT_ROUNDOFF_ULPS, GUARD_FRACTION = 128, 8, 0.25


class IntegrationError(Exception):
    """A method cannot go on: the message is xppautX's."""


def sign(a, b):
    """core's SIGN: |a| with the sign of b (b >= 0 is positive)."""
    return abs(a) if b >= 0.0 else -abs(a)


def total(values):
    """The sum of the values, left to right as a loop does it (numpy's own sum adds pairwise)."""
    return float(np.cumsum(values)[-1])


# ---- the fixed-step methods: one step of dt, as core/odesol2.cpp takes it ----

def euler_step(rhs, t, y, dt):
    yp = rhs(t, y)
    return t + dt, y + dt * yp


def discrete_step(rhs, t, y, dt):
    yp = rhs(t, y)
    return t + dt, yp


def rk4_step(rhs, t, y, dt):
    k = rhs(t, y)
    y0 = y + dt * k / 6.00
    y2 = y + dt * k * 0.5
    t1 = t + .5 * dt
    k = rhs(t1, y2)
    y0 = y0 + dt * k / 3.00
    y2 = y + .5 * dt * k
    k = rhs(t1, y2)
    y0 = y0 + dt * k / 3.000
    y2 = y + dt * k
    t2 = t + dt
    k = rhs(t2, y2)
    return t2, y0 + dt * k / 6.00


def sgefa(a):
    """LINPACK's LU of the square array a, in place, with partial pivoting (core/xpp_math.cpp sgefa): (pivots, info),
    info -1 when it succeeded, else the index of a zero pivot."""
    n = len(a)
    ipvt, info = [0] * n, -1
    for k in range(n - 1):
        l = k + int(np.argmax(np.abs(a[k:, k])))
        ipvt[k] = l
        if a[l, k] == 0.0:
            info = k
            continue
        if l != k:
            a[l, k], a[k, k] = a[k, k], a[l, k]
        a[k + 1:, k] *= -1.0 / a[k, k]
        if l != k:
            a[[k, l], k + 1:] = a[[l, k], k + 1:]
        a[k + 1:, k + 1:] += np.outer(a[k + 1:, k], a[k, k + 1:])
    ipvt[n - 1] = n - 1
    if a[n - 1, n - 1] == 0.0:
        info = n - 1
    return ipvt, info


def sgesl(a, ipvt, b):
    """Solves a x = b with sgefa's factors, x replacing b (core/xpp_math.cpp sgesl)."""
    n = len(a)
    for k in range(n - 1):
        l = ipvt[k]
        t = b[l]
        if l != k:
            b[l] = b[k]
            b[k] = t
        b[k + 1:] += t * a[k + 1:, k]
    for k in range(n - 1, -1, -1):
        b[k] = b[k] / a[k, k]
        b[:k] += -b[k] * a[:k, k]


def jacobian(rhs, t, x, f0, eps):
    """core/jacobian.cpp: the forward-difference Jacobian at x, f0 = rhs(t, x); entry [j, i] is the change of equation j with variable i."""
    n = len(x)
    out = np.empty((n, n))
    xp = x.copy()
    for i in range(n):
        r = eps * max(eps, abs(x[i]))
        xp[i] = x[i] + r
        out[:, i] = (rhs(t, xp) - f0) / r
        xp[i] = x[i]
    return out


def backward_euler_step(rhs, t, y, dt, numerics):
    """core/odesol2.cpp one_bak_step: the implicit step, Newton's iteration from y."""
    t = t + dt
    yp2 = rhs(t, y)
    yg = y.copy()
    iterations = 0
    while True:
        yp = rhs(t, yg)
        errvec = yg - .5 * dt * (yp + yp2) - y
        err1 = total(np.abs(errvec))
        jac = jacobian(rhs, t, yg, yp, numerics.singpt_jacobian_epsilon) * (-.5 * dt)
        jac[np.diag_indices_from(jac)] += 1.0
        ipvt, info = sgefa(jac)
        if info != -1:
            raise IntegrationError('BackEul: Singular Jacobian')
        sgesl(jac, ipvt, errvec)
        err = total(np.abs(errvec))
        yg = yg - errvec
        if err < numerics.eul_tol or err1 < numerics.eul_tol:
            return t, yg
        iterations += 1
        if iterations > numerics.eul_iter:
            raise IntegrationError('BackEul: Too many iterates')


# ---- QualRK: core/stiff.cpp's adaptive Cash-Karp step ----

def rkck(rhs, y, dydx, x, h):
    """One Cash-Karp step: the new state and the error estimate."""
    a2, a3, a4, a5, a6 = 0.2, 0.3, 0.6, 1.0, 0.875
    b21 = 0.2
    b31, b32 = 3.0 / 40.0, 9.0 / 40.0
    b41, b42, b43 = 0.3, -0.9, 1.2
    b51, b52, b53, b54 = -11.0 / 54.0, 2.5, -70.0 / 27.0, 35.0 / 27.0
    b61, b62, b63, b64, b65 = 1631.0 / 55296.0, 175.0 / 512.0, 575.0 / 13824.0, 44275.0 / 110592.0, 253.0 / 4096.0
    c1, c3, c4, c6 = 37.0 / 378.0, 250.0 / 621.0, 125.0 / 594.0, 512.0 / 1771.0
    dc5 = -277.00 / 14336.0
    dc1, dc3, dc4, dc6 = c1 - 2825.0 / 27648.0, c3 - 18575.0 / 48384.0, c4 - 13525.0 / 55296.0, c6 - 0.25
    ak2 = rhs(x + a2 * h, y + b21 * h * dydx)
    ak3 = rhs(x + a3 * h, y + h * (b31 * dydx + b32 * ak2))
    ak4 = rhs(x + a4 * h, y + h * (b41 * dydx + b42 * ak2 + b43 * ak3))
    ak5 = rhs(x + a5 * h, y + h * (b51 * dydx + b52 * ak2 + b53 * ak3 + b54 * ak4))
    ak6 = rhs(x + a6 * h, y + h * (b61 * dydx + b62 * ak2 + b63 * ak3 + b64 * ak4 + b65 * ak5))
    yout = y + h * (c1 * dydx + c3 * ak3 + c4 * ak4 + c6 * ak6)
    yerr = h * (dc1 * dydx + dc3 * ak3 + dc4 * ak4 + dc5 * ak5 + dc6 * ak6)
    return yout, yerr


def rkqs(rhs, y, dydx, x, htry, eps, yscal):
    """core/stiff.cpp rkqs: a step of at most htry that meets the tolerance: (x, y, the step taken's successor)."""
    h = htry
    while True:
        ytemp, yerr = rkck(rhs, y, dydx, x, h)
        errmax = float(np.max(np.abs(yerr / yscal))) / eps
        if errmax > 1.0:
            htemp = SAFETY * h * errmax ** PSHRNK
            h = max(htemp, SHRINK_LIMIT * h) if h >= 0.0 else min(htemp, SHRINK_LIMIT * h)
            if x + h == x:
                raise IntegrationError('QualRK: stepsize is close to 0')
            continue
        hnext = SAFETY * h * errmax ** PGROW if errmax > ERRCON else GROW_LIMIT * h
        return x + h, ytemp, hnext


def qualrk_advance(rhs, t, y, hguess, tout, numerics):
    """core/stiff.cpp gadaptive: from t to tout in steps of its own choosing: (tout, y, the next step to try)."""
    x1, x2 = t, tout
    x, h = x1, sign(hguess, x2 - x1)
    for _ in range(MAXSTP):
        dydx = rhs(x, y)
        yscal = np.abs(y) + np.abs(dydx * h) + TINY
        if (x + h - x2) * (x + h - x1) > 0.0:
            h = x2 - x
        x, y, hnext = rkqs(rhs, y, dydx, x, h, numerics.tolerance, yscal)
        if (x - x2) * (x2 - x1) >= 0.0:
            return x2, y, sign(hnext, x2 - x1)
        if abs(hnext) <= numerics.min_step:
            raise IntegrationError('QualRK: Step size too small')
        h = hnext
    raise IntegrationError('QualRK: Too many steps')


# ---- DoPri8(3): core/dormpri.cpp ----

def accumulate(acc, terms, f):
    """acc + c1*f[k1] + c2*f[k2] + ..., added left to right."""
    for c, k in terms:
        acc = acc + c * f[k]
    return acc


def combine(terms, f):
    """c1*f[k1] + c2*f[k2] + ..., added left to right."""
    (c, k), rest = terms[0], terms[1:]
    return accumulate(c * f[k], rest, f)


def coefficients(prefix, row, columns):
    """K.<prefix><row><column> with the column, for each column: dopcor's names (a31..., d41...)."""
    return [(getattr(K, '%s%d%d' % (prefix, row, c)), c) for c in columns]


# dopcor's stages 3 to 12: (the node, the earlier stages it combines); a stage's state is y + h*(the sum of a*f)
STAGES = {3: (K.c3, [1, 2]), 4: (K.c4, [1, 3]), 5: (K.c5, [1, 3, 4]), 6: (K.c6, [1, 4, 5]), 7: (K.c7, [1, 4, 5, 6]),
          8: (K.c8, [1, 4, 5, 6, 7]), 9: (K.c9, [1, 4, 5, 6, 7, 8]), 10: (K.c10, [1, 4, 5, 6, 7, 8, 9]),
          11: (K.c11, [1, 4, 5, 6, 7, 8, 9, 10])}
LAST_STAGE = [1, 4, 5, 6, 7, 8, 9, 10, 11]  # stage 12, at the end of the step
# the stages 14 to 16 evaluated for the dense output once a step is accepted, the same way
EXTRA_STAGES = {14: (K.c14, [1, 7, 8, 9, 10, 11, 12, 13]), 15: (K.c15, [1, 6, 7, 8, 11, 12, 13, 14]),
                16: (K.c16, [1, 6, 7, 8, 9, 13, 14, 15])}
# the new state's weights (dopcor's k4 before the step is accepted), the two error estimates' terms
WEIGHTS = [(K.b1, 1)] + [(getattr(K, 'b%d' % i), i) for i in range(6, 13)]
ERROR_TERMS = [(K.er1, 1)] + [(getattr(K, 'er%d' % i), i) for i in range(6, 13)]
# the dense polynomial's terms 5 to 8 are rows 4 to 7 of d, from the stages 1, 6..12 and, once the extra stages are in, 13..16
DENSE_ROWS = (4, 5, 6, 7)
DENSE_STAGES, DENSE_EXTRA_STAGES = [1, 6, 7, 8, 9, 10, 11, 12], [13, 14, 15, 16]


def hinit(rhs, x, y, posneg, f0, hmax, atol, rtol):
    """core/dormpri.cpp hinit: the first step, from the size of y and its derivative and an Euler step's second difference."""
    iord = 8
    sk = atol + rtol * np.abs(y)
    sqr = f0 / sk
    dnf = total(sqr * sqr)
    sqr = y / sk
    dny = total(sqr * sqr)
    h = 1.0E-6 if (dnf <= 1.0E-10 or dny <= 1.0E-10) else math.sqrt(dny / dnf) * 0.01
    h = sign(min(h, hmax), posneg)
    f1 = rhs(x + h, y + h * f0)
    sqr = (f1 - f0) / sk
    der2 = math.sqrt(total(sqr * sqr)) / h
    der12 = max(abs(der2), math.sqrt(dnf))
    h1 = max(1.0E-6, abs(h) * 1.0E-3) if der12 <= 1.0E-15 else (0.01 / der12) ** (1.0 / float(iord))
    return sign(min(100.0 * abs(h), min(h1, hmax)), posneg)


def bracketed_root(f, a, b, fa, fb):
    """core/xpp_math.cpp bracketed_root: where f changes sign between a and b (values fa, fb)."""
    for _ in range(ROOT_ITERATIONS):
        if fa == 0:
            return a
        if fb == 0:
            return b
        mid = a + (b - a) * 0.5
        if mid == a or mid == b or abs(b - a) <= ROOT_ROUNDOFF_ULPS * sys.float_info.epsilon * max(1.0, abs(a), abs(b)):
            return mid
        c = a - fa * (b - a) / (fb - fa)
        u = (c - a) / (b - a)
        if not math.isfinite(c) or u < GUARD_FRACTION or u > 1 - GUARD_FRACTION:
            c = mid
        fc = f(c)
        if fc == 0:
            return c
        if (fa < 0) == (fc < 0):
            a, fa = c, fc
        else:
            b, fb = c, fc
    return a + (b - a) * 0.5


class Dop853:
    """core/dormpri.cpp's dop853 and its Solver: Dormand-Prince 8(5,3) one accepted step at a time (dopcor), the
    step's end kept as the internal state (FSAL: the derivative there starts the next step), the output times
    sampled from the step's dense polynomial, and an event's crossing found in that polynomial (core/integrate.cpp
    dense_event). Stiffness detection, which only logs, is not carried."""
    def __init__(self, model, flags, numerics):
        self.model, self.flags, self.numerics = model, flags, numerics
        self.begin()

    def begin(self):
        """A fresh integration."""
        self.valid, self.resume, self.attempts = False, False, 0
        self.event, self.events_checked = None, False
        self.returned, self.returned_time = None, None

    def step(self, xend):
        """dopcor: one accepted step from the internal state: sets endpoint, internal_time and the dense polynomial."""
        rhs, num = self.model.rhs, self.numerics
        x, y = self.internal_time, self.endpoint
        posneg = sign(1.0, xend - x)
        expo1 = 1.0 / 8.0 - DP_BETA * 0.2
        facc1, facc2 = 1.0 / DP_FAC1, 1.0 / DP_FAC2
        hmax = abs(num.max_step)
        if self.resume:
            facold, h, f = self.facold, self.next_h, self.f
        else:
            facold, f = 1.0E-4, {1: rhs(x, y)}
            h = hinit(rhs, x, y, posneg, f[1], hmax, num.abs_tolerance, num.tolerance)
        reject = last = False
        while True:
            if self.attempts >= DOPRI_RUN_ATTEMPTS:
                raise IntegrationError('DoPri8(3): Run exceeded %d DoPri step attempts (accepted and rejected)' % DOPRI_RUN_ATTEMPTS)
            if 0.1 * abs(h) <= abs(x) * UROUND:
                raise IntegrationError('DoPri8(3): Step size too small')
            if (x + 1.01 * h - xend) * posneg > 0.0:
                h = xend - x
                last = True
            self.attempts += 1
            f = {1: f[1]}
            f[2] = rhs(x + K.c2 * h, y + h * K.a21 * f[1])
            for stage, (node, columns) in STAGES.items():
                f[stage] = rhs(x + node * h, y + h * combine(coefficients('a', stage, columns), f))
            xph = x + h
            f[12] = rhs(xph, y + h * combine(coefficients('a', 12, LAST_STAGE), f))
            k4 = combine(WEIGHTS, f)
            k5 = y + h * k4
            sk = num.abs_tolerance + num.tolerance * np.maximum(np.abs(y), np.abs(k5))
            sqr = (k4 - K.bhh1 * f[1] - K.bhh2 * f[9] - K.bhh3 * f[12]) / sk
            err2 = total(sqr * sqr)
            sqr = combine(ERROR_TERMS, f) / sk
            err = total(sqr * sqr)
            deno = err + 0.01 * err2
            if deno <= 0.0:
                deno = 1.0
            err = abs(h) * err * math.sqrt(1.0 / (deno * float(len(y))))
            fac11 = err ** expo1
            fac = max(facc2, min(facc1, fac11 / facold ** DP_BETA / DP_SAFE))
            hnew = h / fac
            if err <= 1.0:
                f[13] = rhs(xph, k5)
                self.dense(y, k5, h, f, x)
                self.f, self.endpoint, self.xold, self.internal_time = {1: f[13]}, k5, x, xph
                if last:
                    return
                if abs(hnew) > hmax:
                    hnew = posneg * hmax
                if reject:
                    hnew = posneg * min(abs(hnew), abs(h))
                self.facold, self.next_h, self.resume = max(err, 1.0E-4), hnew, True
                return
            hnew = h / min(facc1, fac11 / DP_SAFE)
            reject, last = True, False
            h = hnew

    def dense(self, y, k5, h, f, x):
        """The accepted step's interpolation polynomial (rcont1..rcont8), with the stages 14 to 16 it needs."""
        rhs = self.model.rhs
        ydiff = k5 - y
        bspl = h * f[1] - ydiff
        rcont = [y, ydiff, bspl, ydiff - h * f[13] - bspl]
        rcont += [combine(coefficients('d', row, DENSE_STAGES), f) for row in DENSE_ROWS]
        for stage, (node, columns) in EXTRA_STAGES.items():
            f[stage] = rhs(x + node * h, y + h * combine(coefficients('a', stage, columns), f))
        for i, row in enumerate(DENSE_ROWS):
            rcont[4 + i] = h * accumulate(rcont[4 + i], coefficients('d', row, DENSE_EXTRA_STAGES), f)
        self.rcont = rcont

    def sample(self, time):
        """The state at time within the step just taken."""
        if time == self.internal_time:
            return self.endpoint.copy()
        u = (time - self.xold) / (self.internal_time - self.xold)
        v = 1 - u
        r1, r2, r3, r4, r5, r6, r7, r8 = self.rcont
        return r1 + u * (r2 + v * (r3 + u * (r4 + v * (r5 + u * (r6 + v * (r7 + u * r8))))))

    def dense_event(self, start, until):
        """core/integrate.cpp dense_event: the earliest crossing of an event's condition in (start, until] in the
        step's polynomial, with every event that crosses at that time: (time, [events]) or None."""
        direction = 1 if until > start else -1
        coincidence = SIMULTANEOUS_TIME_ULPS * sys.float_info.epsilon * max(1.0, abs(start), abs(until))
        found = None
        for i, e in enumerate(self.model.events):
            def value(time):
                return e.cond(time, self.sample(time))
            a = start
            fa = value(a)
            for j in range(1, DENSE_EVENT_SUBDIVISIONS + 1):
                b = until if j == DENSE_EVENT_SUBDIVISIONS else start + (until - start) * j / DENSE_EVENT_SUBDIVISIONS
                fb = value(b)
                if (e.sign >= 0 and fa < 0 and fb >= 0) or (e.sign <= 0 and fa > 0 and fb <= 0):
                    root = bracketed_root(value, a, b, fa, fb)
                    if (root - start) * direction > coincidence:
                        if not found or (root - found[0]) * direction < -coincidence:
                            found = (root, [])
                        if abs(root - found[0]) <= coincidence:
                            found[1].append(i)
                        break
                a, fa = b, fb
        return found

    def advance(self, t, y, tout):
        """core/dormpri.cpp DormandPrince::step_by: from (t, y) on to tout, over as many steps as it takes, an event's
        crossing taken as it comes: (tout, y)."""
        num = self.numerics
        direction = 1 if tout > t else -1
        if self.valid and (t != self.returned_time or not np.array_equal(self.returned, y) or (self.internal_time - t) * direction < 0):
            self.valid = False
        if not self.valid:
            self.endpoint, self.internal_time, self.resume = y.copy(), t, False
        while (t - tout) * direction < 0:
            if not self.valid or (self.internal_time - t) * direction <= 0:
                self.step(self.internal_time + direction * HORIZON_STEPS * abs(num.max_step))
                self.valid, self.events_checked = True, False
            until = self.internal_time if (self.internal_time - tout) * direction < 0 else tout
            if not self.events_checked:
                self.event, self.events_checked = self.dense_event(t, self.internal_time), True
            reached = self.event is not None and (self.event[0] - until) * direction <= 0
            t = self.event[0] if reached else until
            y = self.sample(t)
            if reached:
                self.events_checked = False
                y = self.flags.apply_dense(y, t, self.event[1])
                # a parameter reset also invalidates the derivative kept from the last step; so does any event
                self.valid, self.resume, self.internal_time, self.endpoint = False, False, t, y.copy()
            self.check_state(y, t)
        self.returned, self.returned_time = y.copy(), t
        return t, y

    def check_state(self, y, t):
        for name, value in zip(self.model.states, y):
            if not math.isfinite(value) or abs(value) > self.numerics.bound:
                raise IntegrationError('DoPri8(3): %s is not finite or exceeds bound at t = %s' % (name, repr(t)))
