"""One-shot W663 (#736) migration of the repository's own models: a button was `buttons: "name:keys"` (XPPAUT's letters), it is a
label and a registry command now. Run at the repository root: uv run --locked python tools/migrate_w663_buttons.py
It rewrites every tracked .odex's `interface: { buttons: "..." , ... }` line and moves the sliders of every recording's session snapshot into its options
block. Kept for the record: no reader calls it, and the old
spelling is refused, never read.
"""
import re
import subprocess
from pathlib import Path

COMMANDS = {'ig': 'run.initial', 'il': 'run.last', 'wf': 'window.fit', 'fa': 'autox.open'}
LINE = re.compile(r'^(\s*)interface: \{ (buttons: "[^"]*"(?:, buttons: "[^"]*")*) \},$', re.M)


def convert(match):
    buttons = re.findall(r'buttons: "([^":]*):([^"]*)"', match.group(2))
    items = ', '.join('{ label: "%s", command: %s }' % (name, COMMANDS[keys]) for name, keys in buttons)
    return '%sinterface: { buttons: [ %s ] },' % (match.group(1), items)


def migrate_models():
    changed = 0
    for path in subprocess.run(['git', 'ls-files', '*.odex'], check=True, capture_output=True, text=True).stdout.split():
        text = Path(path).read_text(encoding='utf-8', newline='')
        migrated = LINE.sub(convert, text)
        if migrated != text:
            Path(path).write_text(migrated, encoding='utf-8', newline='')
            changed += 1
    print('migrated', changed, 'models')


# ---- the session snapshots of the recordings: the sliders leave the session statement for the options block (interface) ----
import base64
import io
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from xppclient import read_recx  # noqa: E402

SNAPSHOT_LINE = 76


def slider_items(text):
    """the items of a session statement's old `sliders: [ ... ]`, each as `{ name: x, low: a, high: b, step: c }`"""
    return [' '.join(item.split()).rstrip(',') for item in re.findall(r'\{[^{}]*\}', text)]


def migrate_session(text, model):
    """text, a session file, with its `sliders` group moved into the options block as `interface: { sliders: [...], buttons: [...] }`;
    the buttons are the model's own (a session holds every saved row)"""
    match = re.search(r'^  sliders: \[(.*?)\],\n', text, re.S | re.M)
    if not match:
        return text
    items = slider_items(match.group(1))
    sliders = '[ ' + ', '.join(items) + ' ]' if items else '[]'
    found = re.search(r'buttons: (\[[^\]]*\])', model)
    interface = '  interface: { sliders: %s, buttons: %s },\n' % (sliders, found.group(1) if found else '[]')
    text = text[:match.start()] + text[match.end():]
    head, rest = text.split('\nsession {', 1)
    end = head.rindex('\n}')
    return head[:end] + '\n' + interface.rstrip('\n') + head[end:] + '\nsession {' + rest


def migrate_snapshot(lines):
    archive = zipfile.ZipFile(io.BytesIO(base64.b64decode(''.join(lines))))
    names = archive.namelist()
    model = next((archive.read(n).decode('utf-8') for n in names if n.startswith('model/') and n.endswith('.odex')), '')
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as target:
        for info in archive.infolist():
            data = archive.read(info.filename)
            if info.filename == 'session.odex':
                data = migrate_session(data.decode('utf-8'), model).encode('utf-8')
            target.writestr(zipfile.ZipInfo(info.filename, info.date_time), data, zipfile.ZIP_DEFLATED)
    packed = base64.b64encode(out.getvalue()).decode()
    return [packed[i:i + SNAPSHOT_LINE] for i in range(0, len(packed), SNAPSHOT_LINE)]


def migrate_recording(text):
    lines = text.split('\n')
    result, i = [], 0
    while i < len(lines):
        result.append(lines[i])
        i += 1
        if result[-1] == '@snapshot':
            end = lines.index('@end', i)
            result += migrate_snapshot(lines[i:end])
            i = end
    migrated = '\n'.join(result)
    return re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(migrated)[4], migrated)


if __name__ == '__main__':
    migrate_models()
    recordings = 0
    for path in subprocess.run(['git', 'ls-files', '*.recx'], check=True, capture_output=True, text=True).stdout.split():
        original = Path(path).read_text(encoding='utf-8')
        migrated = migrate_recording(original)
        if migrated != original:
            Path(path).write_text(migrated, encoding='utf-8', newline='')
            recordings += 1
    print('migrated', recordings, 'recordings')
