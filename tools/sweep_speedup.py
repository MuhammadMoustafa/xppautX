#!/usr/bin/env -S uv run --locked python
"""W434: how much faster a sweep's runs are on threads. Run it alone on the machine:

    tools/heavy.sh --alone uv run --locked python tools/sweep_speedup.py --server ./xppautX.exe

For each model and each thread count it starts a --server, sets the thread count (in a configuration folder of its
own, never the user's preferences), runs one `run.range` of the model's own range settings with a stated number of
steps and prints the wall time from the command to its `done`, the best of REPEATS, and the speed-up over one thread.
The runs are not written to files (the main thread writes them one at a time, which would be what is timed); that the
runs are the same on every thread count is tools/servercheck.py's check_sweep_threads. The time is the number printed, never a pass or a fail (AGENTS.md "Checks that can be trusted").
"""
import argparse
import os
import shutil
import tempfile

from xppclient import Server, is_done

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPEATS = 3
# the model, the range's variable and steps, and the run's own settings: Morris-Lecar as the file has it (a run of 2000
# steps, short against what the main thread does for each run it takes in) and with a run 20 times longer
RANGE = dict(variable='i', start=-.2, end=.5, steps=399)
MODELS = [
    ('ml-range.odex', RANGE, {}),
    ('ml-range.odex', RANGE, dict(total_time=2000, store_every=20)),
]


def timed(binary, model, command, configure, threads, folder, config):
    s = Server(binary, model, run=folder, env={'XPP_CONFIG_DIR': config})
    try:
        s.collect(is_done)
        s.send(cmd='session.set_threads', threads=threads)
        s.collect(is_done)
        if configure:
            s.send(cmd='run.configure', **configure)
            s.collect(is_done)
        started = s.send(cmd='run.range', reset_storage=True, use_old_ic=True, cycle_color=False, movie=False, **command)
        _, done = s.collect(is_done, timeout=3600)
        ended = done['_t'] if done else None
        if not done or done.get('status') != 'ok':
            raise SystemExit('%s on %d threads: %s' % (model, threads, done))
        return ended - started
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--server', default='./xppautX')
    ap.add_argument('--threads', default='1,2,4', help='the thread counts to time, the first the one the others are compared with')
    args = ap.parse_args()
    counts = [int(word) for word in args.threads.split(',')]
    root = tempfile.mkdtemp(prefix='sweepspeed')
    try:
        for name, command, configure in MODELS:
            model = os.path.join(ROOT, 'examples', 'ode', name)
            first = None
            for threads in counts:
                best = None
                for _ in range(REPEATS):
                    folder = tempfile.mkdtemp(prefix='run', dir=root)
                    seconds = timed(os.path.abspath(args.server), model, command, configure, threads, folder, os.path.join(root, 'config'))
                    best = seconds if best is None else min(best, seconds)
                first = first or best
                print('speedup: %s %s, %d runs, %d threads: %.2f s, %.2fx' % (name, configure or 'as the file has it', command['steps'] + 1, threads, best, first / best))
    finally:
        shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    main()
