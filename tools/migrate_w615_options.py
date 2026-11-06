"""One-shot W615 migration: the option table's keys are readable and sectioned (core/model_options.cpp, W614).
Rewrites the options blocks (the file's own and each set's `options:`) of the .odex files and text given:
each key to its new key, an entry moved to its new group when the group changed, and the values that were
numbers standing for a choice (dfdraw, postprocess) or a yes/no (colorize) to their words.
Run from the repository root: uv run --locked python tools/migrate_w615_options.py FILE...
This is a historical migration, never called by a reader.
"""
import os
import re
import sys

# (old group, old key, numbered) -> (new group, new key)
MAP = {
    ('analysis', 'histbins', False): ('analysis', 'histogram.bins'),
    ('analysis', 'histbins2', False): ('analysis', 'histogram2.bins'),
    ('analysis', 'histcol', False): ('analysis', 'histogram.column'),
    ('analysis', 'histcol2', False): ('analysis', 'histogram2.column'),
    ('analysis', 'histhi', False): ('analysis', 'histogram.high'),
    ('analysis', 'histhi2', False): ('analysis', 'histogram2.high'),
    ('analysis', 'histlo', False): ('analysis', 'histogram.low'),
    ('analysis', 'histlo2', False): ('analysis', 'histogram2.low'),
    ('analysis', 'postprocess', False): ('output', 'after_run'),
    ('analysis', 'speccol', False): ('analysis', 'spectrum.column'),
    ('analysis', 'speccol2', False): ('analysis', 'spectrum.column2'),
    ('analysis', 'specwidth', False): ('analysis', 'spectrum.width'),
    ('analysis', 'specwin', False): ('analysis', 'spectrum.window'),
    ('continuation', 'collocation_points', False): ('continuation', 'collocation_points'),
    ('continuation', 'max_step_size', False): ('continuation', 'max_step_size'),
    ('continuation', 'max_steps', False): ('continuation', 'max_steps'),
    ('continuation', 'mesh_intervals', False): ('continuation', 'mesh_intervals'),
    ('continuation', 'min_step_size', False): ('continuation', 'min_step_size'),
    ('continuation', 'norm_max', False): ('continuation', 'norm_max'),
    ('continuation', 'norm_min', False): ('continuation', 'norm_min'),
    ('continuation', 'output_every', False): ('continuation', 'output_every'),
    ('continuation', 'parameter_max', False): ('continuation', 'parameter_max'),
    ('continuation', 'parameter_min', False): ('continuation', 'parameter_min'),
    ('continuation', 'parameter_tolerance', False): ('continuation', 'parameter_tolerance'),
    ('continuation', 'special_point_tolerance', False): ('continuation', 'special_point_tolerance'),
    ('continuation', 'state_tolerance', False): ('continuation', 'state_tolerance'),
    ('continuation', 'step_size', False): ('continuation', 'step_size'),
    ('diagram', 'autoeval', False): ('integration', 'tables.auto_evaluate'),
    ('diagram', 'autovar', False): ('diagram', 'variable'),
    ('diagram', 'autoxmax', False): ('diagram', 'x_max'),
    ('diagram', 'autoxmin', False): ('diagram', 'x_min'),
    ('diagram', 'autoymax', False): ('diagram', 'y_max'),
    ('diagram', 'autoymin', False): ('diagram', 'y_min'),
    ('diagram', 'sec', False): ('diagram', 'colors.stable_equilibrium'),
    ('diagram', 'spc', False): ('diagram', 'colors.stable_periodic'),
    ('diagram', 'uec', False): ('diagram', 'colors.unstable_equilibrium'),
    ('diagram', 'upc', False): ('diagram', 'colors.unstable_periodic'),
    ('equilibria', 'draw_invariant_sets', False): ('equilibria', 'draw_invariant_sets'),
    ('equilibria', 'draw_strong_sets', False): ('equilibria', 'draw_strong_sets'),
    ('equilibria', 'jacobian_step', False): ('equilibria', 'jacobian_step'),
    ('equilibria', 'max_iterations', False): ('equilibria', 'max_iterations'),
    ('equilibria', 'neartol', False): ('equilibria', 'near_tolerance'),
    ('equilibria', 'newton_tolerance', False): ('equilibria', 'newton_tolerance'),
    ('equilibria', 'print_eigenvalues', False): ('equilibria', 'print_eigenvalues'),
    ('equilibria', 'search_limit', False): ('equilibria', 'search_limit'),
    ('export', 'picture_color', False): ('export', 'color'),
    ('export', 'picture_font_size', False): ('export', 'font_size'),
    ('export', 'plotfmt', False): ('export', 'format'),
    ('integration', 'absolute_tolerance', False): ('integration', 'step.absolute_tolerance'),
    ('integration', 'bandlo', False): ('integration', 'jacobian.band_lower'),
    ('integration', 'bandup', False): ('integration', 'jacobian.band_upper'),
    ('integration', 'delay', False): ('integration', 'delay.max_delay'),
    ('integration', 'dt', False): ('integration', 'dt'),
    ('integration', 'fold', False): ('integration', 'torus.fold'),
    ('integration', 'jacobian', False): ('integration', 'jacobian.kind'),
    ('integration', 'max_step', False): ('integration', 'step.max'),
    ('integration', 'method', False): ('integration', 'method'),
    ('integration', 'min_step', False): ('integration', 'step.min'),
    ('integration', 'relative_tolerance', False): ('integration', 'step.relative_tolerance'),
    ('integration', 'seed', False): ('integration', 'seed'),
    ('integration', 'start_time', False): ('integration', 'start_time'),
    ('integration', 'state_limit', False): ('integration', 'state_limit'),
    ('integration', 'stoch', False): ('integration', 'stochastic'),
    ('integration', 'stop_at_limit', False): ('integration', 'stop_at_limit'),
    ('integration', 'storage_rows', False): ('integration', 'storage_rows'),
    ('integration', 'store_every', False): ('integration', 'store_every'),
    ('integration', 'store_from', False): ('integration', 'store_from'),
    ('integration', 'subsystem_split', False): ('integration', 'subsystem_split'),
    ('integration', 'torus_period', False): ('integration', 'torus.period'),
    ('integration', 'total_time', False): ('integration', 'total_time'),
    ('integration', 'vmaxpts', False): ('integration', 'delay.history_points'),
    ('interface', 'animation', False): ('interface', 'animation'),
    ('interface', 'but', False): ('interface', 'buttons'),
    ('interface', 'sliders', False): ('interface', 'sliders'),
    ('output', 'logfile', False): ('output', 'log_file'),
    ('output', 'output', False): ('output', 'data_file'),
    ('output', 'quiet', False): ('output', 'quiet'),
    ('plot', 'axes', False): ('plot', 'axes.kind'),
    ('plot', 'colorhi', False): ('plot', 'color_by.high'),
    ('plot', 'colorize', False): ('plot', 'color_by.enabled'),
    ('plot', 'colorlo', False): ('plot', 'color_by.low'),
    ('plot', 'colormap', False): ('plot', 'colormap'),
    ('plot', 'colorvia', False): ('plot', 'color_by.via'),
    ('plot', 'dfdraw', False): ('plot', 'direction_field.draw'),
    ('plot', 'dfgrid', False): ('plot', 'direction_field.grid'),
    ('plot', 'line.dash', False): ('plot', 'line.dash'),
    ('plot', 'line.draw', False): ('plot', 'line.draw'),
    ('plot', 'line.marker', False): ('plot', 'line.marker'),
    ('plot', 'line.marker_size', False): ('plot', 'line.marker_size'),
    ('plot', 'line.width', False): ('plot', 'line.width'),
    ('plot', 'multiwin', False): ('plot', 'windows.multiple'),
    ('plot', 'nplot', False): ('plot', 'curves.count'),
    ('plot', 'nullcline_mesh', False): ('plot', 'nullclines.mesh'),
    ('plot', 'nullclines.draw', False): ('plot', 'nullclines.draw'),
    ('plot', 'nullclines.file', False): ('output', 'nullclines_file'),
    ('plot', 'phi', False): ('plot', 'axes.phi'),
    ('plot', 'simplot', False): ('plot', 'windows.simultaneous'),
    ('plot', 'smc', False): ('plot', 'manifolds.stable_color'),
    ('plot', 'theta', False): ('plot', 'axes.theta'),
    ('plot', 'umc', False): ('plot', 'manifolds.unstable_color'),
    ('plot', 'xhi', False): ('plot', 'axes.x_high'),
    ('plot', 'xhi', True): ('plot', 'curves.x_high'),
    ('plot', 'xlo', False): ('plot', 'axes.x_low'),
    ('plot', 'xlo', True): ('plot', 'curves.x_low'),
    ('plot', 'xmax', False): ('plot', 'axes.x_max'),
    ('plot', 'xmin', False): ('plot', 'axes.x_min'),
    ('plot', 'xnc', False): ('plot', 'nullclines.x_color'),
    ('plot', 'xp', False): ('plot', 'curves.x'),
    ('plot', 'xp', True): ('plot', 'curves.x'),
    ('plot', 'yhi', False): ('plot', 'axes.y_high'),
    ('plot', 'yhi', True): ('plot', 'curves.y_high'),
    ('plot', 'ylo', False): ('plot', 'axes.y_low'),
    ('plot', 'ylo', True): ('plot', 'curves.y_low'),
    ('plot', 'ymax', False): ('plot', 'axes.y_max'),
    ('plot', 'ymin', False): ('plot', 'axes.y_min'),
    ('plot', 'ync', False): ('plot', 'nullclines.y_color'),
    ('plot', 'yp', False): ('plot', 'curves.y'),
    ('plot', 'yp', True): ('plot', 'curves.y'),
    ('plot', 'zmax', False): ('plot', 'axes.z_max'),
    ('plot', 'zmin', False): ('plot', 'axes.z_min'),
    ('plot', 'zp', False): ('plot', 'curves.z'),
    ('plot', 'zp', True): ('plot', 'curves.z'),
    ('poincare', 'poincare_map', False): ('poincare', 'map'),
    ('poincare', 'poincare_plane', False): ('poincare', 'plane'),
    ('poincare', 'poincare_sign', False): ('poincare', 'sign'),
    ('poincare', 'poincare_stop', False): ('poincare', 'stop'),
    ('poincare', 'poincare_variable', False): ('poincare', 'variable'),
    ('sweep', 'rangefiles', False): ('sweep', 'write_files'),
    ('sweep', 'rangehigh', False): ('sweep', 'high'),
    ('sweep', 'rangelow', False): ('sweep', 'low'),
    ('sweep', 'rangeoldic', False): ('sweep', 'old_ic'),
    ('sweep', 'rangeover', False): ('sweep', 'variable'),
    ('sweep', 'rangereset', False): ('sweep', 'reset'),
    ('sweep', 'rangestep', False): ('sweep', 'steps'),
}
DFDRAW = ['none', 'arrows', 'scaled_arrows', 'flow', 'arrows_and_save', 'scaled_arrows_and_save']
POSTPROCESS = ['none', 'histogram', 'fourier_series', 'power_spectrum', 'psd_x', 'psd_xy', 'coherence_xy', 'histogram_2d']
NUMBERED = re.compile(r'^(xp|yp|zp|xlo|xhi|ylo|yhi)([2-8])$')


def mask(text):
    """text with the strings' and comments' characters blanked (same length), so braces and commas in them count for nothing"""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            for k in range(i + 1, min(j, n)):
                out[k] = ' '
            i = j + 1
        elif c == '#':
            j = text.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        else:
            i += 1
    return ''.join(out)


def match(masked, open_at):
    depth = 0
    for i in range(open_at, len(masked)):
        if masked[i] == '{':
            depth += 1
        elif masked[i] == '}':
            depth -= 1
            if depth == 0:
                return i
    raise SystemExit('unbalanced braces')


def split_top(masked, text, lo, hi):
    """the comma-separated parts of text[lo:hi] (original text), cut at depth-0 commas of the masked copy"""
    parts, depth, start = [], 0, lo
    for i in range(lo, hi):
        c = masked[i]
        if c == '{' or c == '[':
            depth += 1
        elif c == '}' or c == ']':
            depth -= 1
        elif c == ',' and depth == 0:
            parts.append((start, i))
            start = i + 1
    parts.append((start, hi))
    return [(masked[a:b], text[a:b]) for a, b in parts]


def key_of(m):
    k = re.search(r'[A-Za-z_][\w.]*', m)
    return k


def value_text(masked_entry, entry):
    colon = masked_entry.find(':')
    return colon, entry[colon + 1:]


def convert_value(old_key, value):
    word = value.strip()
    if old_key == 'dfdraw' and word.isdigit() and int(word) < len(DFDRAW):
        return DFDRAW[int(word)]
    if old_key == 'postprocess' and word.isdigit() and int(word) < len(POSTPROCESS):
        return POSTPROCESS[int(word)]
    if old_key == 'colorize' and re.fullmatch(r'-?\d+', word):
        return 'false' if int(word) == 0 else 'true'
    return None


def rewrite_options(text, masked, open_at):
    """the options block whose { is at open_at: its new text"""
    close_at = match(masked, open_at)
    groups = split_top(masked, text, open_at + 1, close_at)
    out = []  # [group name or None, entries (str), tail text]
    moves = []  # (new group, entry text)
    for m, entry in groups:
        k = key_of(m)
        if not k:
            out.append([None, [entry], ''])
            continue
        name = k.group(0)
        colon, rest = value_text(m, entry)
        if colon < 0 or not rest.strip().startswith('{'):
            out.append([None, [entry], ''])  # a top-level entry (animation)
            continue
        inner_open = entry.index('{', colon)
        inner_close = len(entry.rstrip()) - 1
        assert entry[inner_close] == '}', entry
        items = split_top(m, entry, inner_open + 1, inner_close)
        kept = []
        for im, ientry in items:
            ik = key_of(im)
            if not ik:
                kept.append(ientry)
                continue
            old = ik.group(0).lower()
            numbered = False
            n = NUMBERED.match(old) if name == 'plot' else None
            base = old
            if n:
                base, numbered = n.group(1), True
            new = MAP.get((name, base, numbered))
            if not new:
                kept.append(ientry)
                continue
            new_group, new_key = new
            if numbered:
                new_key += n.group(2)
            colon_i, ivalue = value_text(im, ientry)
            fixed = convert_value(base, ivalue) if colon_i >= 0 else None
            text_out = ientry[:ik.start()] + new_key + ientry[ik.end():]
            if fixed is not None:
                c2 = text_out.index(':', ik.start() + len(new_key))
                text_out = text_out[:c2 + 1] + ' ' + fixed + ivalue[len(ivalue.rstrip()):]
            if new_group == name:
                kept.append(text_out)
            else:
                moves.append((new_group, text_out.strip()))
        out.append([name, kept, entry[:inner_open + 1], entry[inner_close:]])
    # place the moved entries
    for group, entry in moves:
        for g in out:
            if g[0] == group:
                g[1].append(' ' + entry)
                break
        else:
            at = len(out) - 1 if out and out[-1][0] is None and not out[-1][1][0].strip() else len(out)
            out.insert(at, [group, [' ' + entry], '\n  ' + group + ': {', ' }'])
    # rebuild; a group the moves emptied goes
    pieces = []
    for g in out:
        if g[0] is None:
            pieces.append(g[1][0])
        elif any(e.strip() for e in g[1]):
            pieces.append(g[2] + ','.join(g[1]) + g[3])
    new = ','.join(pieces)
    return text[:open_at + 1] + new + text[close_at:]


def migrate(text):
    changed = True
    pos = 0
    out = text
    while True:
        masked = mask(out)
        found = None
        for m in re.finditer(r'(?<![\w.])options\s*:?\s*\{', masked):
            if m.start() >= pos:
                found = m
                break
        if not found:
            return out
        open_at = found.end() - 1
        new_block = rewrite_options(out, masked, open_at)
        close_old = match(masked, open_at)
        close_new = close_old + len(new_block) - len(out)
        out = new_block
        pos = close_new + 1


GROUPS = sorted({g for g, _, _ in MAP}, key=len, reverse=True)
SEGMENT = re.compile(r'\b(' + '|'.join(GROUPS) + r')(\\?"?\s*:\s*\{)([^{}]*)\}')


def migrate_inline(text):
    """options written inside a program's string (a test's or a script's model text): `group: { key: value, ... }` on one
    line, keys renamed and values converted; an entry whose group changed is reported, never moved"""
    def segment(m):
        group, head, body = m.group(1), m.group(2), m.group(3)
        def entry(e):
            k = re.match(r'(\s*)([A-Za-z_][\w.]*)(\s*:\s*)([^,]*?)(\s*)$', e)
            if not k:
                return e
            lead, key, colon, value, trail = k.groups()
            low = key.lower()
            n = NUMBERED.match(low) if group == 'plot' else None
            base, numbered = (n.group(1), True) if n else (low, False)
            new = MAP.get((group, base, numbered))
            if not new:
                return e
            if new[0] != group:
                print('  moves to', new[0] + ':', key, file=sys.stderr)
            fixed = convert_value(base, value)
            return lead + new[1] + (n.group(2) if n else '') + colon + (fixed if fixed is not None else value) + trail
        return group + head + ','.join(entry(e) for e in body.split(',')) + '}'
    return SEGMENT.sub(segment, text)


def migrate_recx(path):
    """a recording: the model in its snapshot and the model files it carries after it, then its fingerprint"""
    import base64
    import io
    import zipfile
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from xppclient import read_recx
    with open(path, encoding='utf-8', newline='') as f:
        text = f.read()
    prefix, tail = text.split('@snapshot\n', 1)
    encoded, suffix = tail.split('@end', 1)
    source = zipfile.ZipFile(io.BytesIO(base64.b64decode(encoded)))
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as dest:
        for member in source.infolist():
            data = source.read(member)
            if member.filename.endswith('.odex'):
                data = migrate(data.decode()).encode()
            dest.writestr(member, data)
    encoded = base64.b64encode(output.getvalue()).decode()
    text = prefix + '@snapshot\n' + '\n'.join(encoded[k:k + 76] for k in range(0, len(encoded), 76)) + '\n@end' + migrate(suffix)
    text = re.sub(r'fingerprint: [0-9a-f]+', 'fingerprint: ' + read_recx(text)[4], text)
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(text)
    print('migrated', path)


if __name__ == '__main__':
    if sys.argv[1] == '--recx':
        for path in sys.argv[2:]:
            migrate_recx(path)
        sys.exit(0)
    inline = sys.argv[1] == '--inline'
    for path in sys.argv[2 if inline else 1:]:
        with open(path, encoding='utf-8', newline='') as f:
            old = f.read()
        new = migrate_inline(old) if inline else migrate(old)
        if new != old:
            with open(path, 'w', encoding='utf-8', newline='') as f:
                f.write(new)
            print('migrated', path)
