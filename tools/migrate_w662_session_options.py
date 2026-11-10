"""One-shot W662 (#735) migration of the repository's own recordings: the command arguments a session remembered
(transpose, coupling, array_plot, torus, the sweep's second variable and cycle, equilibrium_range, bvp_range) were groups of
the `session { }` statement; they are rows of the option table now, written in the session's `options { }` block.
Run at the repository root: uv run --locked python tools/migrate_w662_session_options.py
It rewrites the session snapshot of every .recx and recomputes its fingerprint. Kept for the record: no reader calls it,
and a session file with the old groups is refused, never upgraded.
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

SNAPSHOT_LINE = 76
OLD_GROUPS = ('transpose', 'coupling', 'array_plot', 'torus', 'sweep', 'equilibrium_range', 'bvp_range')
YES_NO = {'0': 'false', '1': 'true', 'false': 'false', 'true': 'true'}
SIDES = {'0': 'start', '1': 'end'}


def tracked(*patterns):
    out = subprocess.run(['git', 'ls-files', *patterns], check=True, capture_output=True, text=True).stdout
    return [Path(p) for p in out.splitlines()]


def old_groups(session):
    """{group: {key: value text}} of the old groups at the session statement's depth 1 (a list is the text between its brackets)"""
    found = {}
    for name in OLD_GROUPS:
        match = re.search(r'^  %s: \{\n(.*?)^  \},\n' % name, session, re.S | re.M)
        if match:
            body = match.group(1)
            values = {}
            for key, value in re.findall(r'^    (\w+): (\[.*?\]|[^\n]*?),?$', body, re.S | re.M):
                values[key] = value.strip()
            found[name] = (values, match.span())
    return found


def new_items(old, first_variable):
    """{options group: [items]} for the old groups' values"""
    items = {'integration': [], 'equilibria': [], 'sweep': [], 'plot': [], 'analysis': []}
    value = lambda group, key: old[group][0][key]  # noqa: E731
    if 'transpose' in old:
        items['analysis'] += ['transpose.first_column: ' + value('transpose', 'first_column').strip('"'),
                              'transpose.columns: ' + value('transpose', 'columns'),
                              'transpose.column_skip: ' + value('transpose', 'col_skip'),
                              'transpose.first_row: ' + value('transpose', 'row0'),
                              'transpose.rows: ' + value('transpose', 'rows'),
                              'transpose.row_skip: ' + value('transpose', 'row_skip')]
    if 'coupling' in old:
        items['analysis'] = ['coupling: "%s = %s"' % (k, v.strip('"')) for k, v in old['coupling'][0].items()] + items['analysis']
    if 'array_plot' in old:
        column = value('array_plot', 'first_column').strip('"') or first_variable
        items['plot'] += ['array_plot.first_column: ' + column,
                          'array_plot.columns: ' + value('array_plot', 'columns'),
                          'array_plot.first_row: ' + value('array_plot', 'first_row'),
                          'array_plot.rows: ' + value('array_plot', 'rows'),
                          'array_plot.row_skip: ' + value('array_plot', 'row_skip'),
                          'array_plot.z_min: ' + value('array_plot', 'zmin'),
                          'array_plot.z_max: ' + value('array_plot', 'zmax')]
    if 'torus' in old:
        items['integration'] += ['torus.period: ' + value('torus', 'period')]
        items['integration'] += ['torus.fold: ' + n.strip() for n in value('torus', 'fold').strip('[]').replace('\n', ' ').split(',') if n.strip()]
        items['integration'] += ['torus.on: ' + YES_NO[value('torus', 'on')]]
    if 'equilibrium_range' in old:
        e = lambda key: value('equilibrium_range', key)  # noqa: E731
        items['equilibria'] += ['range_parameter: ' + e('item'), 'range_steps: ' + e('steps'), 'range_low: ' + e('low'),
                                'range_high: ' + e('high'), 'range_shoot: ' + YES_NO[e('shoot')], 'range_stability_column: ' + e('stab_col')]
    if 'sweep' in old:
        s = lambda key: value('sweep', key)  # noqa: E731
        items['sweep'] += ['second_variable: ' + s('rangeover2'), 'second_low: ' + s('rangelow2'), 'second_high: ' + s('rangehigh2'),
                           'cycle_color: ' + YES_NO[s('rangecycle')]]
    if 'bvp_range' in old:
        b = lambda key: value('bvp_range', key)  # noqa: E731
        items['integration'] += ['boundary_value.range_parameter: ' + b('item'), 'boundary_value.range_steps: ' + b('steps'),
                                 'boundary_value.range_low: ' + b('low'), 'boundary_value.range_high: ' + b('high'),
                                 'boundary_value.range_side: ' + SIDES[b('side')], 'boundary_value.range_cycle_color: ' + YES_NO[b('cycle')]]
    return items


def add_to_options(text, items):
    """text's options block with the items put into their groups (a group the block lacks is added at its end)"""
    head, rest = text.split('\nsession {', 1)
    for group, new in items.items():
        if not new:
            continue
        one_line = re.search(r'^  %s: \{ (.*) \},$' % group, head, re.M)
        many = re.search(r'^  %s: \{\n.*?^  \},$' % group, head, re.S | re.M)
        if one_line and not many:
            inner = one_line.group(1)
            head = head.replace(one_line.group(0), '  %s: { %s },' % (group, inner + ', ' + ', '.join(new)))
        elif many:
            block = many.group(0)
            lines = ['    ' + ', '.join(new) + ',']
            head = head.replace(block, block[:-len('  },')] + '\n'.join(lines) + '\n  },')
        else:
            end = head.rindex('\n}')
            head = head[:end] + '\n  %s: { %s },' % (group, ', '.join(new)) + head[end:]
    return head + '\nsession {' + rest


def migrate_session(text):
    head, session = text.split('\nsession {', 1)
    session = '\nsession {' + session
    old = old_groups(session)
    if not old:
        return text
    first_variable = re.search(r'^    ending: \{\n\s+(\w+):', session, re.M)
    first_variable = first_variable.group(1) if first_variable else re.search(r'^init (\w+)', text, re.M).group(1)
    items = new_items(old, first_variable)
    for _, (_, (begin, end)) in sorted(old.items(), key=lambda kv: -kv[1][1][0]):
        session = session[:begin] + session[end:]
    return add_to_options(head + session, items)


def migrate_snapshot(lines):
    archive = zipfile.ZipFile(io.BytesIO(base64.b64decode(''.join(lines))))
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as target:
        for info in archive.infolist():
            data = archive.read(info.filename)
            if info.filename == 'session.odex':
                data = migrate_session(data.decode('utf-8')).encode('utf-8')
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
        if line == '@snapshot':
            end = lines.index('@end', i)
            result += migrate_snapshot(lines[i:end])
            i = end
    migrated = '\n'.join(result)
    return re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(migrated)[4], migrated)


if __name__ == '__main__':
    changed = 0
    for path in tracked('*.recx'):
        original = path.read_text(encoding='utf-8')
        migrated = migrate_recording(original)
        if migrated != original:
            path.write_text(migrated, encoding='utf-8', newline='')
            changed += 1
    print('migrated', changed, 'recordings')
