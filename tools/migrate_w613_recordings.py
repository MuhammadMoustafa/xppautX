"""One-shot W613 migration: a session's numerics hold `storage_precision` (core/model_options.cpp), and a .setx loads
only what it holds in full, so the repository's recordings' @snapshot model.setx gain it, `single` (XPPAUT's float, the
default), after store_from; then each fingerprint.
Run from the repository root: uv run --locked python tools/migrate_w613_recordings.py
This is a historical migration, never called by a reader.
"""
import base64
import io
import re
import zipfile
from pathlib import Path
from xppclient import read_recx

for path in sorted(Path('examples/recordings').glob('*.recx')):
    text = path.read_text(encoding='utf-8', newline='')
    prefix, tail = text.split('@snapshot\n', 1)
    encoded, suffix = tail.split('@end', 1)
    source = zipfile.ZipFile(io.BytesIO(base64.b64decode(encoded)))
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as dest:
        for member in source.infolist():
            data = source.read(member)
            if member.filename == 'model.setx' and b'storage_precision' not in data:
                data = re.sub(rb'(\n  store_from: [^\n]*\n)', rb'\1  storage_precision: single,\n', data, count=1)
            dest.writestr(member, data)
    encoded = base64.b64encode(output.getvalue()).decode()
    text = prefix + '@snapshot\n' + '\n'.join(encoded[k:k + 76] for k in range(0, len(encoded), 76)) + '\n@end' + suffix
    text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
    path.write_text(text, encoding='utf-8', newline='')
    print(path)
