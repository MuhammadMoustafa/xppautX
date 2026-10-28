"""One-shot W519 migration: AUTO's point type codes become names (core/auto_codes.h).
Rewrites the repository's own files that hold a code: the autocompare descriptions
(tests/auto/*.json: every "type"), and the codimension-two switches in the recording's
auto/settings.txt (examples/recordings/lecar_auto.recx), then its fingerprint.
Run from the repository root: uv run --locked python tools/migrate_w519_point_names.py
This is a historical migration, never called by a reader.
"""
import base64
import io
import re
import zipfile
from pathlib import Path
from xppclient import read_recx

NAMES = {'EP': 'end', 'MX': 'no_convergence', 'BP': 'branch_point', 'LP': 'fold', 'HB': 'hopf',
         'PD': 'period_doubling', 'TR': 'neimark_sacker', 'UZ': 'user_point', 'BT': 'bogdanov_takens', 'CP': 'cusp',
         'ZH': 'zero_hopf', 'GH': 'generalized_hopf', 'R1': 'resonance_1_1', 'R2': 'resonance_1_2',
         'R3': 'resonance_1_3', 'R4': 'resonance_1_4', 'LPD': 'fold_flip', 'LTR': 'fold_neimark_sacker',
         'PTR': 'flip_neimark_sacker', 'TTR': 'double_neimark_sacker'}

for path in sorted(Path('tests/auto').glob('*.json')):
    text = path.read_text(encoding='utf-8')
    new = re.sub(r'"type": "([A-Z0-9]+)"', lambda m: '"type": "%s"' % NAMES[m[1]] if m[1] in NAMES else m[0], text)
    if new != text:
        path.write_text(new, encoding='utf-8')

path = Path('examples/recordings/lecar_auto.recx')
text = path.read_text(encoding='utf-8')
prefix, tail = text.split('@snapshot\n', 1)
encoded, suffix = tail.split('@end', 1)
source = zipfile.ZipFile(io.BytesIO(base64.b64decode(encoded)))
output = io.BytesIO()
with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as dest:
    for member in source.infolist():
        data = source.read(member)
        if member.filename == 'auto/settings.txt':
            data = re.sub(rb'^([A-Z0-9]+) (true|false)$',
                          lambda m: (NAMES[m[1].decode()].encode() if m[1].decode() in NAMES else m[1]) + b' ' + m[2],
                          data, flags=re.M)
        dest.writestr(member, data)
encoded = base64.b64encode(output.getvalue()).decode()
text = prefix + '@snapshot\n' + '\n'.join(encoded[k:k+76] for k in range(0, len(encoded), 76)) + '\n@end' + suffix
text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
path.write_text(text, encoding='utf-8')
