#!/usr/bin/env -S uv run --locked python
"""One screen for a W354 results folder: each study's state, the workload it is on, how long it has run, and the
first thing that went wrong. Read-only; `watch.py ROOT` once, or `watch -n 20 uv run --locked python watch.py ROOT`."""
import json
from pathlib import Path
import sys
import time

FLAGS = ('STOPPED', 'FAILED', 'NOT EQUAL WORK', 'ALL ENGINES ABORT', 'REUSED')


def main():
    roots = [Path(p) for p in sys.argv[1:]] or sorted(Path('paper/benchmarks/results').glob('w354-*'))[-1:]
    for root in roots:
        status = json.loads((root / 'status.json').read_text()) if (root / 'status.json').exists() else {}
        print('==', root.name, time.strftime('%H:%M:%S'))
        for study, entry in status.get('studies', {}).items():
            out = Path(entry['out'])
            log = out / 'progress.log'
            lines = log.read_text(errors='replace').splitlines() if log.exists() else []
            work = [l for l in lines if 'WORKLOAD' in l]
            flagged = [l for l in lines if any(f in l for f in FLAGS) and 'REUSED' not in l]
            started = out.stat().st_mtime if out.exists() else None
            age = '' if not lines else 'last line %ds ago' % (time.time() - log.stat().st_mtime)
            print('  %-12s %-9s attempt %s exit %s  %s' % (study, entry.get('state'), entry.get('attempt'), entry.get('exit_code'), age))
            if entry.get('state') == 'running' and lines:
                print('     now:', work[-1][9:] if work else lines[-1][:100], '|', lines[-1][9:110])
            if flagged:
                print('     first problem:', flagged[0][:200])
            if len(flagged) > 1:
                print('     (%d more flagged lines in %s)' % (len(flagged) - 1, log))
            done = out / 'done'
            if done.is_dir():
                print('     finished workloads:', len(list(done.glob('*.json'))), '| diverged:', len(list((out / 'diverged').glob('*.json'))) if (out / 'diverged').is_dir() else 0)
            failures = out / 'failures'
            if failures.is_dir() and any(failures.iterdir()):
                print('     driver failures:', ', '.join(sorted(p.stem for p in failures.glob('*.json'))))


if __name__ == '__main__':
    main()
