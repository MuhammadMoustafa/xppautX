#!/usr/bin/env python3
"""tools/python-integration/flags.py: a model's global events, as core/flags.cpp runs them (W427, #489).

After a step the events' conditions are compared with those before it; where one crosses, the step
is cut back to the crossing by linear interpolation, the event's assignments are made there and the
method steps on from it for the rest of the step (`flag_loop`). The same assignments are made at a
crossing a dense output found (`apply_dense`, core/flags.cpp apply_dense_flags).
"""

import sys


# core/flags.cpp: the smallest change of a condition that counts as a crossing, and how near 0 a sign-0 condition must be
CROSSING_CHANGE = 1e-10
ZERO_REACHED = 5e-16
# core/flags.cpp: a step that keeps crossing is given up after this many events beyond the model's number of events
EXTRA_EVENTS = 2


class Flags:
    """The state core/flags.cpp keeps for each event: its condition before and after a step, and whether the step crossed it."""
    def __init__(self, model, stol):
        self.events, self.stol = model.events, stol
        n = len(self.events)
        self.f0, self.f1, self.hit, self.tstar = [0.0] * n, [0.0] * n, [0] * n, [2.0] * n
        self.fired = []  # (time, event number) of every event that fired, for comparing spike times

    def start(self, t, y):
        """The conditions at the start of a run. xppautX does this twice: usual_integrate_stuff and integrate()."""
        for _ in range(2):
            crossed = self.step(y, y, t, t)
            if crossed:
                y = crossed[1]
        return y

    def step(self, yold, ynew, told, tnew):
        """core/flags.cpp one_flag_step: None when the step from (told, yold) to (tnew, ynew) crossed no event, else
        the time and state at the crossing, with the event's assignments made, and the fraction of the step it took."""
        if not self.events:
            return None
        dt = tnew - told
        smin = 2.0
        for i, e in enumerate(self.events):
            self.hit[i], self.tstar[i] = 0, 2.0
            f0 = self.f0[i] = self.f1[i]
            f1 = self.f1[i] = e.cond(tnew, ynew)
            change = abs(f1 - f0)
            if e.sign == 1 and f0 < 0.0 and f1 > 0.0 and change > CROSSING_CHANGE:
                self.hit[i], self.tstar[i] = 1, f0 / (f0 - f1)
            elif e.sign == -1 and f0 > 0.0 and f1 <= 0.0 and change > CROSSING_CHANGE:
                self.hit[i], self.tstar[i] = 1, f0 / (f0 - f1)
            elif e.sign == 0 and abs(f1) < ZERO_REACHED:
                self.hit[i], self.tstar[i] = 1, told
            smin = min(smin, self.tstar[i])
        smin = self.stol if smin < self.stol else (1 + self.stol) * smin
        if smin > 1.0:
            return None
        tnew = told + dt * smin
        ynew = yold + smin * (ynew - yold)
        return tnew, self.apply(ynew, tnew, smin), smin

    def apply(self, y, time, smin):
        """core/flags.cpp apply_flag_hits: the assignments of the events hit (each at its own pass, a pass
        for those a pass's assignments made cross), the conditions after them kept for the next step."""
        events = self.events
        for i, e in enumerate(events):
            self.f0[i] = e.cond(time, y)
        ncycle = 0
        while True:
            ncycle += 1
            for i, e in enumerate(events):
                if self.hit[i] == ncycle and self.tstar[i] <= smin:
                    y = e.act(time, y)
                    self.fired.append((time, i))
            crossed = False
            for i, e in enumerate(events):
                f1 = self.f1[i] = e.cond(time, y)
                if self.hit[i] > 0:
                    continue
                f0, change = self.f0[i], abs(f1 - self.f0[i])
                if ((e.sign == 1 and f0 <= 0.0 and f1 >= 0.0) or (e.sign == -1 and f0 >= 0.0 and f1 <= 0.0)
                        or (e.sign == 0 and f0 * f1 <= 0 and (f1 != 0 or f0 != 0))) and change > CROSSING_CHANGE:
                    self.tstar[i], self.hit[i], crossed = smin, ncycle + 1, True
            if not crossed:
                return y

    def apply_dense(self, y, time, hits):
        """core/flags.cpp apply_dense_flags: the events in hits crossed at time, in the state y."""
        self.hit, self.tstar = [0] * len(self.events), [0.0] * len(self.events)
        for i in hits:
            self.hit[i] = 1
        return self.apply(y, time, 0.0)


def flag_loop(flags, step, t, y, dt, tend=None):
    """core/flags.cpp flag_loop: the method's step(t, y, dtt) -> (t, y), repeated from each event the step crosses
    for the rest of dt (up to the model's number of events and EXTRA_EVENTS), or to tend where the step reaches it."""
    nstep = 0
    dtt = dt
    while True:
        told, yold = t, y
        t, y = step(t, y, dtt)
        crossed = flags.step(yold, y, told, t)
        if not crossed:
            return t, y
        t, y, fraction = crossed
        nstep += 1
        dtt = (1 - fraction) * dt
        if tend is not None and t == tend:
            return t, y
        if nstep > len(flags.events) + EXTRA_EVENTS:
            sys.stderr.write('warning: working too hard?? smin=%g\n' % fraction)
            return t, y
