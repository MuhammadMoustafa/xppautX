"""One-shot W515 migration of the repository's recording snapshot: every window of windows.setx says
whether it is hidden (`hidden: false,`), so does the data browser (`browser_hidden: false,`), and every
view of auto/views.txt says whether it is shown or hidden (`view "NAME" shown PLOT ...`). Run from the repository root:
uv run --locked python tools/migrate_w515_recording.py
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
            data = re.sub(rb'^(    name: "[^"]*",\n)', rb'\1    hidden: false,\n', data, flags=re.MULTILINE)
            data = re.sub(rb'^(active: "[^"]*",\n)', rb'\1browser_hidden: false,\n', data, flags=re.MULTILINE)
        elif member.filename == 'auto/views.txt':
            data = re.sub(rb'^(view "[^"]*") ', rb'\1 shown ', data, flags=re.MULTILINE)
        dest.writestr(member, data)
encoded = base64.b64encode(output.getvalue()).decode()
text = prefix + '@snapshot\n' + '\n'.join(encoded[k:k+76] for k in range(0, len(encoded), 76)) + '\n@end' + suffix
text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
path.write_text(text, encoding='utf-8')
