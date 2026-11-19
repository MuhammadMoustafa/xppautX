#!/usr/bin/env -S uv run --locked python
"""One screen for a results folder of run.py (W699): each study's units done and failed, the unit that is running and its last
progress line, and every failure's first lines. Read-only; `watch.py ROOT` once, or `watch -n 20 uv run --locked python watch.py ROOT`."""
from collections import Counter
import json
from pathlib import Path
import sys
import time

from common import DONE, PASS1, STOPPED, STUDIES


def newest(unit):
    # The name of the unit's newest attempt (attempt-NNN), '' when none began.
    return max((p.name.split('.')[0] for p in unit.glob('attempt-*')), default='')


def main():
    for root in [Path(p) for p in sys.argv[1:]] or sorted(Path('paper/benchmarks/results').glob('w354-*'))[-1:]:
        print('==', root.name, time.strftime('%H:%M:%S'))
        if (root / STOPPED).exists():  # W818: the series ended at a slower unit; the fix starts it again from its first unit
            print('  STOPPED at a slower unit:', ', '.join(json.loads((root / STOPPED).read_text())['slower']))
        for study in STUDIES:
            units = sorted(p for p in (root / study).glob('*') if p.is_dir())
            done = [u for u in units if (u / DONE).exists()]
            failed = [u for u in units if u not in done and (u / (newest(u) + '.failure.json')).exists()]
            running = [u for u in units if u not in done and u not in failed and newest(u)]
            print('  %-12s %d units: %d done, %d failed%s' % (study, len(units), len(done), len(failed), ', running ' + running[-1].name if running else ''))
            # W818: each finished unit's verdict against its reference (run.log has the ratios), and pass 1's where pass 2 is not done
            records = {u.name: json.loads((u / DONE).read_text()) for u in done}
            if records:
                print('     verdicts: ' + ', '.join('%d %s' % (n, v) for v, n in sorted(Counter(r.get('verdict', 'not judged') for r in records.values()).items())))
            for name, r in sorted(records.items()):
                if r.get('verdict') == 'slower':
                    print('     SLOWER %s (run.log has its ratios)' % name)
                if r.get('behind'):  # W830: reported, not a stop
                    print('     BEHIND another program %s: %d row(s) (run.log has the ratios)' % (name, r['behind']))
            for u in units:
                if u not in done and (u / PASS1).exists():
                    print('     pass 1 %s: %s' % (u.name, json.loads((u / PASS1).read_text())['verdict']))
            for u in running[-1:]:
                log = sorted(u.glob('attempt-[0-9][0-9][0-9]/progress.log'))[-1:]
                if log:
                    print('     now:', log[0].read_text(errors='replace').splitlines()[-1][:140], '| last line %ds ago' % (time.time() - log[0].stat().st_mtime))
            for u in failed:
                record = json.loads((u / (newest(u) + '.failure.json')).read_text())
                print('     FAILED %s: %s' % (u.name, record.get('error') or '; '.join(record.get('log_tail', [])[-2:])[:200]))


if __name__ == '__main__':
    main()
