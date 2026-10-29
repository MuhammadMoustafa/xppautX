"""One-shot W529 migration of the repository's own files to format version 4 (Discussion #415 decision 9).
Run at the repository root: uv run --locked python tools/migrate_w529_version.py
It gives every .odex its `version 4` statement (after the header comment --convert writes, else first), every
.anix its `version: 4` statement and brings a recording's embedded models and session snapshot along, with the
recording's fingerprint recomputed. Kept for the record: no reader calls it, and a file of another version is
refused, never upgraded.
"""
import base64
import io
import re
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from xppclient import read_recx  # noqa: E402

VERSION = 4
CONVERTED = re.compile(r'# [^\n]*: converted from [^\n]*\n')
SNAPSHOT_LINE = 76


def tracked(*patterns):
    out = subprocess.run(['git', 'ls-files', *patterns], check=True, capture_output=True, text=True).stdout
    return [Path(p) for p in out.splitlines()]


def with_odex_version(text):
    """text with `version 4` after the first line when --convert's header, else at the start; unchanged when it has one"""
    if re.match(r'(?:[ \t]*(?:#[^\n]*)?\r?\n)*[ \t]*version[ \t]+\d', text):
        return text
    newline = '\r\n' if '\r\n' in text else '\n'
    line = 'version %d%s' % (VERSION, newline)
    header = CONVERTED.match(text.replace('\r\n', '\n'))
    if header:
        end = text.index('\n') + 1
        return text[:end] + line + text[end:]
    return line + text


def with_anix_version(text):
    if re.match(r'[ \t]*version[ \t]*:', text):
        return text
    newline = '\r\n' if '\r\n' in text else '\n'
    return 'version: %d%s' % (VERSION, newline) + text


def rewrite(path, change):
    with path.open(encoding='utf-8', newline='') as source:
        original = source.read()
    text = change(original)
    if text != original:
        with path.open('w', encoding='utf-8', newline='') as target:
            target.write(text)
        return True
    return False


def migrate_snapshot(lines):
    """the session zip of a recording's @snapshot, its model members given their version"""
    archive = zipfile.ZipFile(io.BytesIO(base64.b64decode(''.join(lines))))
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as target:
        for info in archive.infolist():
            data = archive.read(info.filename)
            if info.filename.startswith('model/') and info.filename.endswith('.odex'):
                data = with_odex_version(data.decode('utf-8')).encode('utf-8')
            target.writestr(zipfile.ZipInfo(info.filename, info.date_time), data, zipfile.ZIP_DEFLATED)
    packed = base64.b64encode(out.getvalue()).decode()
    return [packed[i:i + SNAPSHOT_LINE] for i in range(0, len(packed), SNAPSHOT_LINE)]


def migrate_recording(text):
    lines = text.split('\n')
    result, i = [], 0
    while i < len(lines):
        line = lines[i]
        result.append(line)
        i += 1
        if line == '@snapshot' or (line.startswith('@file ') and line.endswith('.odex')):
            end = lines.index('@end', i)
            body = lines[i:end]
            if line == '@snapshot':
                body = migrate_snapshot(body)
            else:
                unescaped = '\n'.join(l[1:] if l.startswith('@@') else l for l in body)
                changed = with_odex_version(unescaped).split('\n')
                body = ['@' + l if l.startswith('@') else l for l in changed]
            result += body
            i = end
    migrated = '\n'.join(result)
    return re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(migrated)[4], migrated)


if __name__ == '__main__':
    changed = 0
    for path in tracked('*.odex'):
        changed += rewrite(path, with_odex_version)
    for path in tracked('*.anix'):
        changed += rewrite(path, with_anix_version)
    for path in tracked('*.recx'):
        changed += rewrite(path, migrate_recording)
    print('migrated', changed, 'files to format version', VERSION)
