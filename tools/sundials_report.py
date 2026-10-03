#!/usr/bin/env python3
"""W34 (#72): the tables of docs/sundials-eval.md, from the folder
tools/sundials_eval.py wrote.

  python3 tools/sundials_report.py results > tables.md

Needs numpy and scipy (the references of the models with no closed form:
DOP853 for the non-stiff ones, Radau for Robertson's). The error of a run is,
over every stored row and every state column, max |y - ref| / (1 + |ref|)
(the mixed absolute and relative measure, so a state of size 1e6 and one of
size 1e-6 count alike); the final error is the same over the last row only.
xppautX stores its columns in single precision (the time column too), so
that measure cannot see below about 1e-7: a model with a closed form carries
its error as aux columns worked out in double precision (osc, osc1e6, pr,
heat, heatband: the maximum over the aux columns and rows is the error, the
heat equation's a root mean square over its points), and the references are
compared at the nominal times k*dt, not the stored ones.
"""
import json, math, os, sys
import numpy as np
from scipy.integrate import solve_ivp


def load(path):
    rows = []
    with open(path) as f:
        for line in f:
            if line.strip():
                rows.append([float(x) for x in line.split()])
    return np.array(rows) if rows else np.zeros((0, 0))


# ---- the references of the models with no closed form in the model ----------

def ref_vdp(t):
    s = solve_ivp(lambda _, y: [y[1], (1 - y[0] ** 2) * y[1] - y[0]], (0, t[-1]), [2, 0], t_eval=t,
                  method='DOP853', rtol=1e-13, atol=1e-14)
    return s.y.T


def ref_robertson(t):
    def f(_, y):
        return [-0.04 * y[0] + 1e4 * y[1] * y[2], 0.04 * y[0] - 1e4 * y[1] * y[2] - 3e7 * y[1] ** 2, 3e7 * y[1] ** 2]

    def jac(_, y):
        return [[-0.04, 1e4 * y[2], 1e4 * y[1]], [0.04, -1e4 * y[2] - 6e7 * y[1], -1e4 * y[1]], [0, 6e7 * y[1], 0]]
    s = solve_ivp(f, (0, t[-1]), [1, 0, 0], t_eval=t, method='Radau', jac=jac, rtol=1e-12, atol=1e-16)
    return s.y.T


def ref_ball(t):
    """y and v of the bouncing ball (ball.odex), exactly"""
    g, h, e = 9.8, 10.0, 0.8
    y = np.zeros(len(t))
    v = np.zeros(len(t))
    bounce = [math.sqrt(2 * h / g)]      # the times of the bounces
    speed = [math.sqrt(2 * g * h)]       # the speeds they hit with
    while bounce[-1] < t[-1] + 1:
        s = e * speed[-1]
        bounce.append(bounce[-1] + 2 * s / g)
        speed.append(s)
    for i, tt in enumerate(t):
        if tt <= bounce[0]:
            y[i] = h - 0.5 * g * tt * tt
            v[i] = -g * tt
        else:
            k = max(j for j in range(len(bounce)) if bounce[j] <= tt)
            tau = tt - bounce[k]
            s = e * speed[k]
            y[i] = s * tau - 0.5 * g * tau * tau
            v[i] = s - g * tau
    return np.stack([y, v], 1)


REFS = {'vdp': ref_vdp, 'robertson': ref_robertson, 'ball': ref_ball}
REF_NAMES = {'vdp': 'scipy DOP853 1e-13', 'robertson': 'scipy Radau 1e-12', 'ball': 'exact'}
NSTATE = {'osc': 2, 'osc1e6': 2, 'vdp': 2, 'pr': 1, 'robertson': 3, 'heat': 100, 'heatband': 100, 'ball': 2}
# the models whose error is an aux column of the run itself
AUX_ERROR = {'osc', 'osc1e6', 'pr', 'heat', 'heatband'}
REF_TAGS = ('ref_sundials_sundials', 'ref_vendored_vendored', 'ref_dp83_vendored')


def err(y, ref):
    d = np.abs(y - ref) / (1 + np.abs(ref))
    return d.max(), d[-1].max()


def fmt(x):
    if x is None:
        return '-'
    if isinstance(x, str):
        return x
    return ('%.2e' % x) if x != 0 else '0'


def nominal(t):
    """the times k*dt the rows are at (the stored ones are single precision)"""
    dt = float('%.6g' % (t[1] - t[0]))
    return np.round(t / dt) * dt


def errors(name, d, ref, ns):
    """the error over the trajectory and at the end of a run's data d"""
    if name in AUX_ERROR and d.shape[0]:
        a = np.abs(d[:, 1 + ns:])
        return a.max(), a[-1].max()
    if name == 'poincare' and d.shape[0]:
        tk = 1.5 * math.pi + 2 * math.pi * np.arange(d.shape[0])
        return max(np.abs(d[:, 0] - tk).max(), np.abs(d[:, 2] - 1).max()), abs(d[-1, 0] - tk[-1])
    if ref is not None and d.shape[0] == ref.shape[0] and d.shape[1] > 1:
        return err(d[:, 1:1 + ref.shape[1]], ref)
    if ref is not None and d.shape[0]:
        return 'rows %d/%d' % (d.shape[0], ref.shape[0]), 'rows %d/%d' % (d.shape[0], ref.shape[0])
    return None, None


def main():
    folder = sys.argv[1]
    res = json.load(open(os.path.join(folder, 'results.json')))
    base = res['baseline']
    print('Process start (osc, total=0.1): %.3f s wall, %.3f s CPU; every time below has it left in.\n' % (base['time'], base['cpu']))
    for name, case in res.items():
        if name == 'baseline':
            continue
        runs = case['runs']
        data = {}
        for tag in runs:
            p = os.path.join(folder, name, tag, 'output.dat')
            data[tag] = load(p) if os.path.exists(p) else np.zeros((0, 0))
        ns = NSTATE.get(name)
        # the reference: the first tight run that completed (its rows give the times)
        good = [tag for tag in REF_TAGS if tag in data and runs[tag]['code'] == 0 and data[tag].shape[0] > 1]
        t = data[good[0]][:, 0] if good else None
        ref = None
        refname = ''
        cross = ''
        if name in AUX_ERROR:
            refname = 'exact (the aux columns, double precision)'
        elif name in REFS and t is not None:
            ref = REFS[name](nominal(t))
            refname = REF_NAMES[name]
        elif name not in ('poincare', 'osc1e6') and good:
            pick = [tag for tag in ('ref_dp83_vendored', 'ref_sundials_sundials') if tag in good]
            if pick:
                ref = data[pick[0]][:, 1:]
                ns = ref.shape[1]
                refname = pick[0].split('_')[1] + ' tight'
        if ref is not None:
            for tag in good:
                if data[tag].shape[0] == ref.shape[0] and data[tag].shape[1] > ref.shape[1]:
                    m, _ = err(data[tag][:, 1:1 + ref.shape[1]], ref)
                    cross += ' %s %s;' % (tag.split('_')[1], fmt(m))
        print('### %s\n' % name)
        if name == 'poincare':
            print('Reference: exact, crossings at t = 3 pi/2 + 2 pi k, y = 1 (error: the larger of the crossing time and y).\n')
        elif refname:
            print('Reference: %s.%s\n' % (refname, (' The tight runs against it (trajectory error):' + cross) if cross else ''))
        print('| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |')
        print('|---|---|---|---|---|---|---|---|---|---|---|---|---|')
        tags = sorted((k for k in runs if not k.startswith('ref_')), key=lambda k: (k.rsplit('_', 1)[0], k.rsplit('_', 1)[1]))
        for tag in tags + [k for k in runs if k.startswith('ref_')]:
            r = runs[tag]
            run_tag, backend = tag.rsplit('_', 1)
            if r['code'] != 0:
                e_t = e_f = 'exit %s, %d rows' % (r['code'], r['rows'])
            else:
                e_t, e_f = errors(name, data[tag], ref, ns)
            s = r['stats']
            print('| %s | %s | %d | %s | %s | %d | %d | %d | %d | %d | %d | %.3f | %.3f |' % (
                run_tag, backend, r['rows'], fmt(e_t), fmt(e_f), s['nst'], s['rhs'], s['nje'], s['nsetups'], s['netf'],
                s['ncfn'], r['cpu'], r['time']))
        for tag in tags:
            if runs[tag]['messages'] and name == 'blowup':
                print('\n%s: %s' % (tag, ' / '.join(runs[tag]['messages'])))
        print()


if __name__ == '__main__':
    main()
