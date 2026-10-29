#!/usr/bin/env -S uv run --locked python
"""W269 (#326): ModelDB 267710's AUTO workflow (its README's steps) in xppautX over --server:
integrate 600 ms at ampPar = 0 twice (Initialconds/Go, then /Last: the model's buttons int
and intl, on models/6cModel/rest.odex), open AUTO,
Run/Steady state in ampPar with the model's own opt.inc settings, grab the rightmost of the
branch's Hopf points, set Ds negative (-PERIODIC_DS) and Run/Periodic.

    uv run --locked python auto_xppautx.py XPPAUTX [THREADS JACOBIAN [--no-compile]]

measure() returns {integrate, steady, periodic: {wall, cpu, labels, digest, points}}.
"""
import hashlib, json, shutil, sys, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'auto'))
sys.path.insert(0, str(HERE.parents[2] / 'tools'))
from xppclient import Server, open_auto  # noqa: E402
from explore_xppautx import step, numerics  # noqa: E402
from hopf_xppautx import run, cpu_seconds  # noqa: E402  W246's AUTO run and its CPU time

MODEL_DIR = HERE / 'models' / '6cModel'
MODEL = 'rest.ode'
# The model's own buttons int (Initialconds/Go) and intl (Initialconds/Last): AUTO starts
# from the last integration's initial values (XPPAUT's last_ic), so Go alone leaves it at
# the file's values (-70 mV, not an equilibrium: MX at once) and Last starts from Go's end.
INTEGRATE = ('run.initial', 'run.last')
PERIODIC_DS = 0.02  # XPPAUT's default Ds (the model sets none), negative for the periodic run (README)
LIMIT = 3600  # s: a run still going after this has hung (never a speed criterion)


def rightmost_hb(labels):
    """(index among the HB labels in branch order, 1-based, as `auto grab` counts; its ampPar)"""
    hbs = [x for _, sym, x in labels if sym == 'hopf']
    if len(hbs) < 2:
        raise RuntimeError(f'expected two Hopf points on the steady-state branch, got {labels}')
    k = max(range(len(hbs)), key=lambda i: hbs[i])
    return k + 1, hbs[k]


def measure(binary, threads=None, jacobian=None, extra_args=(), env=None, limit=LIMIT):
    with tempfile.TemporaryDirectory(prefix='w269-mdb-') as tmp:
        work = Path(tmp) / 'model'
        shutil.copytree(MODEL_DIR, work)
        s = Server(binary, str(work / MODEL), run=str(work), env=env, extra_args=extra_args)
        try:
            return _measure(s, threads, jacobian, limit)
        finally:
            s.close()


def _measure(s, threads, jacobian, limit):
    step(s, timeout=limit)
    s.send(cmd='events.subscribe', events=[], diagram=True)
    step(s)
    if threads is not None:
        _ok(step(s, cmd='auto.set_threads', **({'automatic': True} if threads == 0 else {'threads': threads}))[0], 'threads')
    if jacobian is not None:
        _ok(step(s, cmd='set', kind='num', name='jacobian', text=jacobian)[0], 'jacobian')
    out = {}
    c0, t0 = cpu_seconds(s.proc), time.perf_counter()
    for command in INTEGRATE:
        step(s, cmd=command, timeout=limit)
    out['integrate'] = {'wall': time.perf_counter() - t0, 'cpu': cpu_seconds(s.proc) - c0}
    open_auto(s)
    for name, key in (('steady', 's'), ('periodic', 'p')):
        if key == 'p':
            k, x = rightmost_hb(out['steady']['labels'])
            _ok(step(s, cmd='auto.grab', type='hopf', index=k)[0], 'grab')
            numerics(s, step_size=-PERIODIC_DS)
            out['grabbed'] = {'index': k, 'ampPar': x}
        wall, cpu, labs, pts = run(s, key, limit)
        out[name] = {'wall': wall, 'cpu': cpu, 'labels': [(pt, sym, x) for _, pt, _, sym, x, _ in labs if sym],
                     'digest': hashlib.sha256(json.dumps(pts).encode()).hexdigest(), 'points': pts}
    return out


def _ok(evs, what):
    err = [e for e in evs if e.get('ev') in ('session.error', 'session.message') and e.get('kind', 'error') == 'error']
    if err:
        raise RuntimeError(f'{what}: {err}')


def main():
    binary = sys.argv[1]
    threads = int(sys.argv[2]) if len(sys.argv) > 2 else None
    jacobian = sys.argv[3] if len(sys.argv) > 3 else None
    r = measure(binary, threads, jacobian, sys.argv[4:])
    for name in ('integrate', 'steady', 'periodic'):
        print(f"{name}: {r[name]['wall']:.2f} s wall, {r[name]['cpu']:.2f} s CPU")
        for lab in r[name].get('labels', []):
            print('  ', lab)
        if 'points' in r[name]:
            print('   points', len(r[name]['points']))


if __name__ == '__main__':
    main()
