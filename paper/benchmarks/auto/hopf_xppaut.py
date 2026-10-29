#!/usr/bin/env -S uv run --locked python
"""W246 (#302): XPPAUT 8.0's AUTO on PY_S1Bf_bench.ode, driven through its X11
window on a private Xvfb display with xdotool (XPPAUT has no batch mode for AUTO).

The model file carries every AUTO constant as an option, so the keys are the
ones a user presses: File/Auto, Run/Steady state, Grab (Tab to the HB, Return),
Run/Periodic. XPPAUT prints AUTO's point table on stdout: a run is over when its
closing EP or MX line appears; the CPU time XPPAUT used meanwhile is the measure.

    uv run --locked python hopf_xppaut.py XPPAUT [--keep DIR]
"""
import os, shutil, subprocess, sys, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODEL = HERE / 'models' / 'PY_S1Bf_bench.ode'
DISPLAY = os.environ.get('W248_DISPLAY', ':77')  # W248: one display per concurrent run
TICK = os.sysconf('SC_CLK_TCK')
POLL = 0.25  # s between reads of XPPAUT's output while a run goes on
RUN_LIMIT = 3600  # s: a run still going after this has hung


def cpu_seconds(pid):
    f = open(f'/proc/{pid}/stat').read().rsplit(')', 1)[1].split()
    return (int(f[11]) + int(f[12])) / TICK  # utime, stime


def xdo(*args):
    return subprocess.run(['xdotool', *args], env={**os.environ, 'DISPLAY': DISPLAY},
                          capture_output=True, text=True).stdout.strip()


def window(name, timeout=30):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        ids = xdo('search', '--onlyvisible', '--name', name).split()
        if ids:
            return ids[-1]
        time.sleep(POLL)
    raise RuntimeError(f'no window named {name!r}; windows: {all_windows()}')


def wait_display(timeout=30):
    """until the Xvfb server answers"""
    end = time.monotonic() + timeout
    while subprocess.run(['xdpyinfo', '-display', DISPLAY], capture_output=True).returncode != 0:
        if time.monotonic() > end:
            raise RuntimeError(f'Xvfb on {DISPLAY} did not start')
        time.sleep(POLL)


def all_windows():
    return [(w, xdo('getwindowname', w)) for w in xdo('search', '--onlyvisible', '--name', '.').split()]


def keys(win, *ks):
    """XPPAUT reads the key under the pointer (no window manager): move there, then type"""
    xdo('mousemove', '--window', win, '20', '20')
    for k in ks:
        xdo('key', '--window', win, k)
        time.sleep(0.3)  # XPPAUT opens a pop-up menu per key; it reads the next key from it


def table(log):
    return [l.split() for l in log.read_text().splitlines() if l[:4].strip().isdigit() and len(l.split()) > 4]


def wait_done(proc, start_cpu, log, rows_before, limit=RUN_LIMIT):
    """CPU seconds the run used, once AUTO's table has a new closing (EP/MX) line"""
    end = time.monotonic() + limit
    while time.monotonic() < end:
        rows = table(log)[rows_before:]
        if any(r[2] in ('EP', 'MX') for r in rows):
            return cpu_seconds(proc.pid) - start_cpu, rows
        if proc.poll() is not None:  # W248: XPPAUT itself ended (a crash)
            raise RuntimeError(f'XPPAUT exited with status {proc.returncode} before the run ended')
        time.sleep(POLL)
    raise RuntimeError(f'AUTO run did not end within {limit} s')


def measure(binary, keep=None, steady_only=False, model=MODEL, home=None, limit=RUN_LIMIT, ready=None, go=None, out=None, tabs=1):
    """{run: {wall, cpu, labels: [(pt, type, Iapp)]}} for the steady-state and periodic runs"""
    binary = os.path.abspath(binary)
    work = Path(tempfile.mkdtemp(prefix='w246-xppaut-'))
    if home is None:  # W248: a caller may give the HOME several runs share
        home = work / 'home'
        home.mkdir()
    home = Path(home)
    model = Path(model)
    shutil.copy(model, work)
    xvfb = subprocess.Popen(['Xvfb', DISPLAY, '-screen', '0', '1600x1200x24', '-nolisten', 'tcp'],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    xpp = None
    out = {} if out is None else out
    try:
        wait_display()
        env = {**os.environ, 'DISPLAY': DISPLAY, 'HOME': str(home), 'LC_ALL': 'C'}
        log = work / 'xppaut.log'
        xpp = subprocess.Popen([binary, model.name], cwd=work, env=env, stdout=open(log, 'w'),
                               stderr=subprocess.STDOUT)
        main_win = window('XPP')
        keys(main_win, 'f', 'a')
        auto_win = window('AUTO')
        if ready:  # W248: all concurrent runs are set up; start together
            Path(ready).write_text('ready')
            while not Path(go).exists():
                time.sleep(POLL)
        for name, run_keys in (('steady', ('r', 's')), ('periodic', ('r', 'p'))):
            if name == 'periodic':
                if steady_only:
                    break
                keys(auto_win, 'g', *['Tab'] * tabs, 'Return')  # grab: from point 1 (EP) Tab goes to the next label, the HB
            n, c0, t0, t_wall0 = len(table(log)), cpu_seconds(xpp.pid), time.perf_counter(), time.time()
            keys(auto_win, *run_keys)
            cpu, rows = wait_done(xpp, c0, log, n, limit)
            out[name] = {'wall': time.perf_counter() - t0, 'cpu': cpu,
                         'labels': [(int(r[1]), r[2], float(r[4])) for r in rows if r[2] != 'RG'],
                         'rows': [' '.join(r) for r in rows], 't0': t_wall0, 't1': time.time()}
    except Exception as e:  # W248: what a failed run left behind is the evidence
        out['_error'] = repr(e)
        out['_log'] = (work / 'xppaut.log').read_text(errors='replace')[-3000:]
        raise
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
    for name, r in measure(sys.argv[1], keep, '--steady-only' in sys.argv).items():
        print(f"{name}: {r['wall']:.2f} s wall, {r['cpu']:.2f} s CPU")
        for lab in r['labels']:
            print('  ', lab)


if __name__ == '__main__':
    main()
