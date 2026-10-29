#!/usr/bin/env -S uv run --locked python
"""W246 (#302): xppautX's AUTO on models/PY_S1Bf_bench.ode over --server.

The file starts at the depolarised equilibrium (Iapp = 20000 pA) and carries
every AUTO constant, so the steps are XPPAUT's keys (hopf_xppaut.py): File/Auto,
Run/Steady state, grab the first HB, Run/Periodic. Prints each run's wall time,
xppautX's CPU time and AUTO's labelled points.

    uv run --locked python hopf_xppautx.py XPPAUTX [MODEL]
"""
import hashlib, json, sys, time
from pathlib import Path
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'tools'))
from xppclient import Server, is_done, open_auto  # noqa: E402
from explore_xppautx import step, labels, ensure_completed  # noqa: E402

RUN_LIMIT = 3600  # s: a run still going after this has hung


def cpu_seconds(proc):
    """the server process's CPU time (Linux /proc, or Windows GetProcessTimes through psutil-free ctypes)"""
    if sys.platform == 'win32':
        import ctypes
        from ctypes import wintypes
        k = ctypes.WinDLL('kernel32', use_last_error=True)
        k.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        t = [wintypes.FILETIME() for _ in range(4)]
        if not k.GetProcessTimes(wintypes.HANDLE(int(proc._handle)), *map(ctypes.byref, t)):
            raise ctypes.WinError(ctypes.get_last_error())
        return sum((f.dwHighDateTime << 32 | f.dwLowDateTime) for f in t[2:]) / 1e7
    import os
    f = open(f'/proc/{proc.pid}/stat').read().rsplit(')', 1)[1].split()
    return (int(f[11]) + int(f[12])) / os.sysconf('SC_CLK_TCK')


def points(evs):
    """every diagram point the events added: (branch, number, x, y, y2)"""
    return [(r['br'], r['pt'] + i, x, r['y'][i], r['y2'][i] if 'y2' in r else None) for e in evs
            if e.get('ev') == 'auto.diagram' and e.get('op') == 'add' for r in e['runs'] for i, x in enumerate(r['x'])]


def run(s, key, limit=RUN_LIMIT, scratch=None):
    """A named AUTO run (s equilibrium, p periodic): (wall s, CPU s, labelled points, all points); W248: the
    process's AUTO scratch folders seen while it runs go into the list `scratch`"""
    c0, t0 = cpu_seconds(s.proc), time.perf_counter()
    s.send(cmd='auto.run', kind={'s': 'equilibrium', 'p': 'periodic'}[key],
           **({'label': s.grabbed_label} if key == 'p' else {}))
    more, done = [], None
    while done is None:  # events up to done, looking at the scratch folders between them
        got, done = s.collect(is_done, timeout=0.2)
        more += got
        if scratch is not None:
            scratch += [d for d in s.auto_dirs() if d not in scratch]
        if time.perf_counter() - t0 > limit:
            raise RuntimeError(f'AUTO run did not end within {limit} s')
        if not s.alive() and done is None:
            raise RuntimeError(f'xppautX exited with status {s.proc.returncode} before the run ended')
    ensure_completed(more, done)
    return time.perf_counter() - t0, cpu_seconds(s.proc) - c0, labels(more), points(more)


def measure(binary, model=str(HERE / 'models' / 'PY_S1Bf_bench.ode'), threads=None, jacobian=None,
            extra_args=(), env=None, run_dir=None, limit=RUN_LIMIT, ready=None, go=None, out=None):
    """{run: {wall, cpu, labels: [(pt, type, Iapp)]}} for the steady-state and periodic runs.
    W269 (#326): threads (AUTO's thread count, W247/W267: a preference, so env should name a
    fresh XPP_CONFIG_DIR), jacobian ('difference' or 'exact', W260/W275: set before AUTO opens,
    which reads it), extra_args (--no-compile); the defaults are W246's run.
    W248: run_dir (the folder the model is run in, shared by concurrent runs), a start barrier
    (ready/go files), out (the caller's dict, kept when the run fails)."""
    out = {} if out is None else out
    s = Server(binary, model, env=env, extra_args=extra_args, run=run_dir)
    out['pid'] = s.proc.pid
    try:
        return _measure(s, threads, jacobian, limit, ready, go, out)
    except Exception as e:
        out['_error'] = repr(e)
        out['_log'] = ''.join(s.log[-40:])[-3000:]
        raise
    finally:
        s.close()


def _measure(s, threads, jacobian, limit, ready, go, out):
    step(s)
    s.send(cmd='events.subscribe', events=[], diagram=True)
    step(s)
    if threads is not None:
        step(s, cmd='auto.set_threads', **({'automatic': True} if threads == 0 else {'threads': threads}))
    if jacobian is not None:
        step(s, cmd='set', kind='num', name='jacobian', text=jacobian)
    open_auto(s)
    if ready:  # W248: all concurrent runs are set up; start together
        Path(ready).write_text('ready')
        while not Path(go).exists():
            time.sleep(0.05)  # poll the caller's start-barrier file
    for name, key in (('steady', 's'), ('periodic', 'p')):
        if key == 'p':
            step(s, cmd='auto.grab', type='hopf', index=1)
        scratch, t_wall0 = [], time.time()
        wall, cpu, labs, pts = run(s, key, limit, scratch)
        out[name] = {'wall': wall, 'cpu': cpu, 't0': t_wall0, 't1': time.time(), 'scratch': scratch, 'labels': [(pt, sym, x) for _, pt, _, sym, x, _ in labs if sym],
                     'digest': hashlib.sha256(json.dumps(pts).encode()).hexdigest(), 'points': pts}
    return out


def main():
    for name, r in measure(*sys.argv[1:3]).items():
        print(f"{name}: {r['wall']:.2f} s wall, {r['cpu']:.2f} s CPU")
        for lab in r['labels']:
            print('  ', lab)


if __name__ == '__main__':
    main()
