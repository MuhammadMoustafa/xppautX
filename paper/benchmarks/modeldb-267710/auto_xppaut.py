#!/usr/bin/env -S uv run --locked python
"""W269 (#326): ModelDB 267710's AUTO workflow in XPPAUT 8.0, through its X11 window on a
private Xvfb display with xdotool, the keys auto_xppautx.py sends xppautX: Initialconds/Go
and /Last (the model's buttons int and intl) on models/6cModel/rest.ode, File/Auto,
Run/Steady state, Grab (Tab to the rightmost HB, Return), Numerics: Ds = -PERIODIC_DS
(XPPAUT's form: Return moves to the next field, Tab accepts them all), Run/Periodic.

W246's helpers (../auto/hopf_xppaut.py) drive the window and read AUTO's point table on
XPPAUT's stdout. XPPAUT prints nothing when an integration ends: its end is taken as the
process's CPU time staying unchanged for SETTLE seconds (a harness's wait, never part of a
measured AUTO run).

    uv run --locked python auto_xppaut.py XPPAUT
"""
import os, shutil, subprocess, sys, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'auto'))
import hopf_xppaut as H  # noqa: E402
from auto_xppautx import MODEL_DIR, INTEGRATE, PERIODIC_DS, LIMIT  # noqa: E402  one source of the workflow

MODEL = 'rest.ode'
SETTLE = 2.0  # s of unchanged CPU time that say XPPAUT's integration has ended
TICK = 1 / os.sysconf('SC_CLK_TCK')


def wait_idle(proc, c0, t0, limit=LIMIT):
    """until XPPAUT's CPU time stops growing for SETTLE seconds: (CPU s used since c0, wall s since t0)"""
    last, since = H.cpu_seconds(proc.pid), time.perf_counter()
    while time.perf_counter() - t0 < limit:
        if proc.poll() is not None:
            raise RuntimeError(f'XPPAUT exited with {proc.returncode}')
        time.sleep(H.POLL)  # the poll interval of a wait for a condition (CPU time unchanged)
        c = H.cpu_seconds(proc.pid)
        if c > last + TICK / 2:
            last, since = c, time.perf_counter()
        elif last > c0 and time.perf_counter() - since >= SETTLE:
            return last - c0, since - t0
    raise RuntimeError('XPPAUT integration did not end')


def wait_end(proc, c0, log, rows_before, limit=LIMIT):
    """CPU seconds the run used and its table rows, once a closing EP or MX row follows the
    run's first row (a steady-state run's first point is itself labelled EP)"""
    end = time.monotonic() + limit
    while time.monotonic() < end:
        if proc.poll() is not None:
            raise RuntimeError(f'XPPAUT exited with {proc.returncode}')
        rows = H.table(log)[rows_before:]
        if any(r[2] in ('EP', 'MX') for r in rows[1:]):
            return H.cpu_seconds(proc.pid) - c0, rows
        time.sleep(H.POLL)  # the poll interval of a wait for a condition (the closing row)
    raise RuntimeError('AUTO run did not end')


def labelled(rows):
    return [(int(r[1]), r[2], float(r[4])) for r in rows if r[2] != 'RG']


def measure(binary, keep=None, limit=LIMIT):
    binary = os.path.abspath(binary)
    work = Path(tempfile.mkdtemp(prefix='w269-mdb-xppaut-'))
    home = work / 'home'
    home.mkdir()
    shutil.copytree(MODEL_DIR, work / 'model')
    xvfb = subprocess.Popen(['Xvfb', H.DISPLAY, '-screen', '0', '1600x1200x24', '-nolisten', 'tcp'],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    xpp, out = None, {}
    try:
        H.wait_display()
        env = {**os.environ, 'DISPLAY': H.DISPLAY, 'HOME': str(home), 'LC_ALL': 'C'}
        log = work / 'xppaut.log'
        # stdout line-buffered (stdbuf), so the table is read in whole lines as AUTO prints them
        xpp = subprocess.Popen(['stdbuf', '-oL', binary, MODEL], cwd=work / 'model', env=env, stdout=open(log, 'w'),
                               stderr=subprocess.STDOUT)
        main_win = H.window('XPP')
        cpu = wall = 0
        for key in INTEGRATE:
            c0, t0 = H.cpu_seconds(xpp.pid), time.perf_counter()
            H.keys(main_win, 'i', key)
            c, w = wait_idle(xpp, c0, t0, limit)
            cpu, wall = cpu + c, wall + w
        out['integrate'] = {'wall': wall, 'cpu': cpu}
        H.keys(main_win, 'f', 'a')
        auto_win = H.window('AUTO')
        for name, run_keys in (('steady', ('r', 's')), ('periodic', ('r', 'p'))):
            if name == 'periodic':
                labs = out['steady']['labels']
                hbs = [i for i, (_, sym, _) in enumerate(labs) if sym == 'HB']
                k = max(hbs, key=lambda i: labs[i][2])
                H.keys(auto_win, 'g', *['Tab'] * k, 'Return')  # from the first label, Tab k times
                out['grabbed'] = {'label_position': k, 'ampPar': labs[k][2]}
                H.keys(auto_win, 'n')
                form = H.window('AutoNum')
                ds = f'{-PERIODIC_DS:g}'
                H.keys(form, 'Return', 'Return', 'Return', *['BackSpace'] * 12,
                       *[{'-': 'minus', '.': 'period'}.get(ch, ch) for ch in ds], 'Tab')
            n, c0, t0 = len(H.table(log)), H.cpu_seconds(xpp.pid), time.perf_counter()
            H.keys(auto_win, *run_keys)
            cpu, rows = wait_end(xpp, c0, log, n, limit)
            out[name] = {'wall': time.perf_counter() - t0, 'cpu': cpu, 'labels': labelled(rows),
                         'rows': [' '.join(r) for r in rows]}
    finally:
        for p in (xpp, xvfb):
            if p is not None and p.poll() is None:
                p.kill()
                p.wait()
        if keep:
            shutil.copytree(work, keep, dirs_exist_ok=True)
        shutil.rmtree(work)
    return out


def main():
    keep = sys.argv[sys.argv.index('--keep') + 1] if '--keep' in sys.argv else None
    r = measure(sys.argv[1], keep)
    for name in ('integrate', 'steady', 'periodic'):
        print(f"{name}: {r[name]['wall']:.2f} s wall, {r[name]['cpu']:.2f} s CPU")
        for lab in r[name].get('labels', []):
            print('  ', lab)


if __name__ == '__main__':
    main()
