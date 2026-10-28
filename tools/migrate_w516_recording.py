"""One-shot W516 migration: the recording's AUTO steps become the registry's commands (auto.open, auto.run, auto.grab).
Run from the repository root: uv run --locked python tools/migrate_w516_recording.py
This is a historical migration, never called by a reader.
"""
import re
from pathlib import Path
from xppclient import read_recx

path = Path('examples/recordings/lecar_auto.recx')
text = path.read_text(encoding='utf-8')
steps = {
    '{"step":"File","keys":["f"],"view":true}\n{"step":"File → Auto","keys":["a"],"view":true}':
        '{"step":"File → AUTO","cmd":{"cmd": "auto.open"},"view":true}',
    '{"step":"AUTO → Run → Steady state","win":"auto","keys":["r","s"]}':
        '{"step":"AUTO → Run → Equilibrium","cmd":{"cmd": "auto.run", "kind": "equilibrium", "replace_diagram": false}}',
    '{"step":"AUTO → Grab","win":"auto","keys":["g"],"answers":[{"key":"Tab"},{"key":"Return"}]}':
        '{"step":"AUTO → Grab","cmd":{"cmd": "auto.grab", "type": "hopf", "index": 1}}',
    '{"step":"AUTO → Run → Periodic","win":"auto","keys":["r","p"]}':
        '{"step":"AUTO → Run → Periodic","cmd":{"cmd": "auto.run", "kind": "periodic", "type": "hopf", "index": 1}}',
}
for old, new in steps.items():
    assert text.count(old) >= 1, old
    # the first File step of the pair (the second occurrence of the File step) is the one before AUTO
    text = text.replace(old, new) if old.count('File → Auto') == 0 else text[::-1].replace(old[::-1], new[::-1], 1)[::-1]
text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
path.write_text(text, encoding='utf-8')
