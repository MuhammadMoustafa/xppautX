"""One-shot W531 migration: AUTO's settings take readable names (core/auto_settings.cpp).
Rewrites the repository's own files that hold a setting or a calculation kind:
  - every .odex's `continuation: { ... }` block (ntst is mesh_intervals, ...);
  - the autocompare descriptions' per-run `auto` settings (tests/auto/*.json);
  - the diagram baselines' calculation columns (tests/auto/*.diagram.csv,
    tools/models/lecar_diagram.csv): equilibrium_fold is fold, periodic_fold is
    cycle_fold, torus is neimark_sacker;
  - the recording's model, its snapshot's model and auto/settings.txt
    (examples/recordings/lecar_auto.recx), then its fingerprint.
Run from the repository root: uv run --locked python tools/migrate_w531_settings.py
This is a historical migration, never called by a reader.
"""
import base64
import io
import re
import subprocess
import zipfile
from pathlib import Path
from xppclient import read_recx

# an option of a .odex or a description (XPPAUT's spellings) to its name
OPTIONS = {'ntst': 'mesh_intervals', 'nmax': 'max_steps', 'npr': 'output_every', 'ncol': 'collocation_points',
           'ds': 'step_size', 'dsmin': 'min_step_size', 'dsmax': 'max_step_size', 'parmin': 'parameter_min',
           'parmax': 'parameter_max', 'normmin': 'norm_min', 'normmax': 'norm_max', 'epsl': 'parameter_tolerance',
           'epsu': 'state_tolerance', 'epss': 'special_point_tolerance',
           # the descriptions' settings that are not options of a model
           'mxbf': 'max_branch_switches', 'iid': 'output_detail', 'iad': 'mesh_adapt_every',
           'itmx': 'max_location_iterations', 'itnw': 'max_newton_iterations', 'nwtn': 'full_newton_after',
           'iads': 'step_adapt_every', 'suppbp': 'suppress_branch_points'}
# a session file's keys (settings.txt) to theirs
SESSION = {'ntst': 'mesh_intervals', 'nmx': 'max_steps', 'npr': 'output_every', 'ncol': 'collocation_points',
           'ds': 'step_size', 'dsmin': 'min_step_size', 'dsmax': 'max_step_size', 'rl0': 'parameter_min',
           'rl1': 'parameter_max', 'a0': 'norm_min', 'a1': 'norm_max', 'epsl': 'parameter_tolerance',
           'epsu': 'state_tolerance', 'epss': 'special_point_tolerance', 'iad': 'mesh_adapt_every',
           'itmx': 'max_location_iterations', 'itnw': 'max_newton_iterations', 'nwtn': 'full_newton_after',
           'iads': 'step_adapt_every', 'suppbp': 'suppress_branch_points'}
OUTPUT_DETAIL = ['none', 'little', 'normal', 'start_jacobian', 'extensive', 'debugging']
CALCULATIONS = {'equilibrium_fold': 'fold', 'periodic_fold': 'cycle_fold', 'torus': 'neimark_sacker'}
CALCULATION_NAMES = {'none', 'equilibrium_fold', 'periodic_fold', 'hopf', 'torus', 'branch_point', 'period_doubling',
                     'fixed_period', 'boundary_value', 'equilibrium', 'periodic', 'discrete', 'homoclinic',
                     'neutral_saddle'}


def rename_keys(block):
    """the keys of a `{ key: value, ... }` or `{"key": value}` text, renamed"""
    return re.sub(r'(?<![\w.])("?)([a-z0-9_]+)("?)(\s*:)', lambda m: m[1] + OPTIONS.get(m[2], m[2]) + m[3] + m[4], block)


LINE_WIDTH = 100  # core/odex_write.cpp's line_width


def braced(items, indent):
    """core/odex_write.cpp's braced(): one line, or rows of at most LINE_WIDTH"""
    line = '{ ' + ', '.join(items) + ' }'
    if indent + len(line) <= LINE_WIDTH:
        return line
    pad = ' ' * (indent + 2)
    out, row = '{\n', ''
    for item in items:
        if row and len(pad) + len(row) + 2 + len(item) > LINE_WIDTH:
            out += pad + row + ',\n'
            row = ''
        row += (', ' if row else '') + item
    return out + pad + row + ',\n' + ' ' * indent + '}'


def migrate_odex(text):
    def block(m):
        items = [rename_keys(i.strip()) for i in m[1].replace('\n', ' ').split(',') if i.strip()]
        return 'continuation: ' + braced(items, 2)
    return re.sub(r'continuation:\s*\{([^{}]*)\}', block, text)


def migrate_description(text):
    def block(m):
        out = rename_keys(m[0])
        # a negative max_branch_switches meant one direction only: its own setting now
        negative = re.search(r'"max_branch_switches":\s*(-\d+)', out)
        if negative:
            out = out.replace(negative[0], '"max_branch_switches": %d,\n"switch_one_direction": true' % -int(negative[1]))
        return out
    return re.sub(r'"auto":\s*\{[^{}]*\}', block, text)


def migrate_csv(text):
    rows = []
    for row in text.split('\n'):
        fields = row.split(',', 11)
        if len(fields) > 11 and fields[0] in CALCULATION_NAMES:
            fields[0] = CALCULATIONS.get(fields[0], fields[0])
            fields[10] = CALCULATIONS.get(fields[10], fields[10])
            row = ','.join(fields)
        rows.append(row)
    return '\n'.join(rows)


def migrate_settings(text):
    rows = []
    for row in text.split('\n'):
        key, _, value = row.partition(' ')
        if key == 'mxbf':
            rows.append('max_branch_switches %d' % abs(int(value)))
            rows.append('switch_one_direction %s' % ('true' if int(value) < 0 else 'false'))
        elif key == 'iid':
            rows.append('output_detail %s' % OUTPUT_DETAIL[int(value)])
        elif key in SESSION:
            rows.append('%s %s' % (SESSION[key], value))
        else:
            rows.append(row)
    return '\n'.join(rows)


def write_if_changed(path, text):
    if path.read_text(encoding='utf-8') != text:
        path.write_text(text, encoding='utf-8', newline='')
        print(path)


files = subprocess.run(['git', 'ls-files'], capture_output=True, text=True, check=True).stdout.split('\n')
for name in files:
    path = Path(name)
    if not name or not path.is_file():
        continue
    if name.endswith('.odex'):
        write_if_changed(path, migrate_odex(path.read_text(encoding='utf-8')))
    elif name.startswith('tests/auto/') and name.endswith('.json'):
        write_if_changed(path, migrate_description(path.read_text(encoding='utf-8')))
    elif name.endswith('.diagram.csv') or name == 'tools/models/lecar_diagram.csv':
        write_if_changed(path, migrate_csv(path.read_text(encoding='utf-8')))

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
            data = migrate_settings(data.decode()).encode()
        elif member.filename.endswith('.odex'):
            data = migrate_odex(data.decode()).encode()
        dest.writestr(member, data)
encoded = base64.b64encode(output.getvalue()).decode()
text = prefix + '@snapshot\n' + '\n'.join(encoded[k:k + 76] for k in range(0, len(encoded), 76)) + '\n@end' + migrate_odex(suffix)
text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
path.write_text(text, encoding='utf-8')
print(path)
