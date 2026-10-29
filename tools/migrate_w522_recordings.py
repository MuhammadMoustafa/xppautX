"""One-shot W522 migration of repository recordings to analysis registry commands.
Run at the repository root: uv run --locked python tools/migrate_w522_recordings.py
Readers accept only the new commands; this historical migration is never called by a reader.
"""
import json
import re
from pathlib import Path
from xppclient import read_recx

for path in Path('examples/recordings').glob('*.recx'):
    with path.open(encoding='utf-8', newline='') as source:
        original = source.read()
    rows = []
    for row in original.splitlines(keepends=True):
        if row.startswith('{"step":'):
            step = json.loads(row)
            command = None
            if step.get('keys') == ['s', 'g', 'n'] and 'win' not in step:
                command = {'cmd': 'equilibrium.find'}
            elif step.get('win') == 'equilibrium' and step.get('keys') == ['i']:
                command = {'cmd': 'equilibrium.import_as_ic'}
            if command:
                step = {'step': step['step'], 'cmd': command}
                row = json.dumps(step, separators=(',', ':')) + ('\r\n' if row.endswith('\r\n') else '\n')
        rows.append(row)
    text = ''.join(rows)
    if text != original:
        text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
        with path.open('w', encoding='utf-8', newline='') as target:
            target.write(text)
