"""One-shot W353 migration of the repository's recording snapshot to named windows.
Run from the repository root: uv run --locked python tools/migrate_w353_recording.py
This is a historical migration, never called by a reader.
"""
import base64
import io
import re
import zipfile
from pathlib import Path
from xppclient import read_recx

path = Path('examples/recordings/lecar_auto.recx')
text = path.read_text(encoding='utf-8')
prefix, tail = text.split('@snapshot\n', 1)
encoded, suffix = tail.split('@end', 1)
source = zipfile.ZipFile(io.BytesIO(base64.b64decode(encoded)))
output = io.BytesIO()
with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as dest:
    for member in source.infolist():
        data = source.read(member)
        if member.filename == 'windows.setx':
            data = re.sub(rb'number: (\d+),\n    open: true,', lambda m: b'name: "graph_' + str(int(m[1]) + 1).encode() + b'",', data)
            data = re.sub(rb'active: (\d+),', lambda m: b'active: "graph_' + str(int(m[1]) + 1).encode() + b'",', data)
        elif member.filename == 'marks.setx':
            data = re.sub(rb'window: (\d+),', lambda m: b'window: "graph_' + str(int(m[1]) + 1).encode() + b'",', data)
        elif member.filename == 'auto/views.txt':
            rows, count = [], 0
            for row in data.decode().splitlines():
                if row.startswith('view '):
                    count += 1
                    row = 'view "diagram_%d" %s' % (count, row[5:])
                elif row.startswith('active '):
                    row = 'active "diagram_%d"' % (int(row[7:]) + 1)
                rows.append(row)
            data = ('\n'.join(rows) + '\n').encode()
        dest.writestr(member, data)
encoded = base64.b64encode(output.getvalue()).decode()
text = prefix + '@snapshot\n' + '\n'.join(encoded[k:k+76] for k in range(0, len(encoded), 76)) + '\n@end' + suffix
text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
path.write_text(text, encoding='utf-8')
