#!/usr/bin/env -S uv run --locked python
"""W354 stage 2, in settled order; each study is independently resumable."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from common import BENCH, HERE, REPO, TOOLS, save, sha

READY_WAIT_SECONDS = 900  # how long a timed study waits for an idle machine before it gives up
READY_POLL_SECONDS = 5

STUDIES = ('terman', 'screen', 'modeldb', 'research', 'izhikevich', 'python', 'continuation', 'scaling', 'large-stiff')


def record(root, check):
    if check:
        # Untimed WSL checks cannot satisfy the Ubuntu governor/idle policy.
        value = dict(check_only=True, lscpu_extended=subprocess.check_output(['lscpu', '-e'], text=True))
    else:
        # W448: --ready (the governor, and no other process on the CPU) before each timed study. The session supervising
        # the series (its desktop app) is busy only while it acts, so wait for idle: a poll for a condition, bounded.
        deadline = time.monotonic() + READY_WAIT_SECONDS
        while subprocess.run(['bash', str(TOOLS / 'comparison-setup.sh'), '--ready'], cwd=REPO).returncode:
            if time.monotonic() > deadline:
                sys.exit('comparison-setup.sh --ready refused for %d s: the timed series needs the machine to itself' % READY_WAIT_SECONDS)
            time.sleep(READY_POLL_SECONDS)  # the poll interval of a wait for an idle machine
        p = subprocess.run(['bash', str(TOOLS / 'comparison-setup.sh'), '--record'], cwd=REPO, check=True, capture_output=True, text=True)
        path = next(l.removeprefix('record: ') for l in p.stdout.splitlines() if l.startswith('record: '))
        value = json.loads(Path(path).read_text())
    path = root / 'machine.json'
    save(path, value)
    return path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--xppautx', type=Path, required=True)
    p.add_argument('--xppaut', type=Path, required=True)
    p.add_argument('--auto-dir', type=Path, required=True)
    p.add_argument('--worker', type=Path, default=Path('build/w378/worker'))
    p.add_argument('--study', choices=STUDIES, action='append', help='resume only these studies, in settled order')
    p.add_argument('--stop-on-failure', action='store_true', help='stop at the first failed workload (and so the first failed study)')
    p.add_argument('--reuse', type=Path, help='an earlier results folder (it may be unfinished): rows of a program whose executable, measurement code and inputs are unchanged are imported, not run again')
    p.add_argument('--check', action='store_true')
    p.add_argument('--repeats', type=int, help='counted repetitions per row (W460); the drivers default to five')
    p.add_argument('--only', help='short checks: select workload names within a study')
    a = p.parse_args()
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=True)
    status_path = a.out / 'status.json'
    status = json.loads(status_path.read_text()) if status_path.exists() else {}
    signature = dict(commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
                     binaries={str(b.resolve()): sha(b) for b in (a.xppautx, a.xppaut)},
                     drivers={str(f.relative_to(BENCH)): sha(f) for f in HERE.glob('*.py')}, check=a.check, only=a.only)
    if status and status['signature'] != signature:
        raise ValueError('results belong to another tip, binary, driver or selection: choose a new --out')
    status.setdefault('signature', signature)
    status.setdefault('studies', {})
    failed = []
    for study in STUDIES:
        if a.study and study not in a.study:
            continue
        previous = status['studies'].get(study, {})
        if previous.get('state') == 'complete':
            print('RESUME completed', study, flush=True)
            continue
        attempt = previous.get('attempt', 0) + 1
        out = a.out / study / ('attempt-%03d' % attempt)
        machine = record(out.parent, a.check)
        script = HERE / ('terman.py' if study == 'terman' else 'continuation.py' if study == 'continuation' else 'integration.py')
        argv = [sys.executable, '-X', 'faulthandler', str(script), '--out', str(out), '--xppautx', str(a.xppautx.resolve()),
                '--xppaut', str(a.xppaut.resolve()), '--auto-dir', str(a.auto_dir.resolve()), '--machine-record', str(machine)]
        if study not in ('terman', 'continuation'):
            argv += ['--study', study]
        if study == 'large-stiff':
            argv += ['--worker', str(a.worker.resolve())]
        if a.stop_on_failure:
            argv += ['--stop-on-failure']
        if a.reuse:
            earlier = json.loads((a.reuse / 'status.json').read_text())['studies'].get(study, {})
            # Any earlier attempt, finished or stopped: the driver imports each program's rows that are still valid (W452).
            if earlier.get('out') and (Path(earlier['out']) / 'runs.json').exists():
                argv += ['--reuse', earlier['out']]
        if a.check:
            argv += ['--check']
        if a.only:
            argv += ['--only', a.only]
        if a.repeats is not None and study != 'terman':
            argv += ['--repeats', str(a.repeats)]
        status['studies'][study] = dict(state='running', attempt=attempt, out=str(out), command=argv)
        save(status_path, status)
        # Foreground, serial, with logs retained even if the study raises.
        with (out.parent / ('attempt-%03d.log' % attempt)).open('w') as log:
            result = subprocess.run(argv, cwd=REPO, stdout=log, stderr=subprocess.STDOUT)
        entry = status['studies'][study]
        entry.update(state='complete' if result.returncode == 0 else 'failed', exit_code=result.returncode)
        save(status_path, status)
        print(study, entry['state'], out, flush=True)
        if result.returncode:
            failed.append(study)
            if a.stop_on_failure:
                break
    if failed:
        sys.exit('Failed (results retained; resume with --study): ' + ', '.join(failed))


if __name__ == '__main__':
    main()
