#!/usr/bin/env python3
"""W299/#357: one AUTO numerical baseline for every platform.

Descriptions use W306's run vocabulary, not recorded keys. Only execute_run
translates it to today's menus. Each model runs serially at 1 and auto threads.
No elapsed-time assertion: completion is idle; TIMEOUT is only a hang guard.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import sys
import tempfile
import time
import zipfile

from xppclient import Checker, SLOW, Server, is_ask, is_idle, open_auto, placed, run_any

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / 'tests/auto'
# Plenty of room for slow CI/sanitizers, never a numerical acceptance criterion.
TIMEOUT = 120 * SLOW
DIAGRAM_MEMBER = 'auto/diagram.csv'  # the session file's diagram (core/xpp_session_auto.h)
KINDS = {'equilibrium', 'periodic', 'two_parameter', 'fixed_period', 'bvp',
         'homoclinic', 'heteroclinic'}
NUMERICS = {'nmax': 'nmx', 'parmin': 'rl0', 'parmax': 'rl1',
            **{k: k for k in ('ds', 'dsmin', 'dsmax', 'npr', 'ntst', 'ncol',
                              'epsl', 'epsu', 'epss', 'mxbf', 'iid')}}


class Diagram:
    """The stored points' labels and types, independently of event batching:
    what a run's start by type and index needs."""
    def __init__(self):
        self.points = []

    def apply(self, events):
        for e in events:
            if e.get('ev') != 'diagram' or e.get('view') != 0:
                continue
            if e['op'] == 'reset':
                del self.points[e['keep']:]
            elif e['op'] == 'add':
                if not 0 <= e['from'] <= len(self.points):
                    raise ValueError('noncontiguous diagram: ' + str(e['from']))
                del self.points[e['from']:]
                for r in e['runs']:
                    labs = {i: (lab, sym) for i, lab, sym in r.get('lab', [])}
                    self.points += [dict(zip(('lab', 'sym'), labs.get(i, (0, ''))))
                                    for i in range(len(r['x']))]
        return self


def members(value, allowed, required=()):
    if not isinstance(value, dict) or set(value) - set(allowed) or set(required) - set(value):
        raise ValueError('expected fields %s (required %s), got %s' % (allowed, required, value))


def finite(value, name, integer=False, positive=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError('%s must be finite: %s' % (name, value))
    if (integer and int(value) != value) or (positive and value <= 0):
        raise ValueError('%s has an invalid value: %s' % (name, value))


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate field: ' + key)
        result[key] = value
    return result


def read_description(path):
    d = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=no_duplicates)
    members(d, ('model', 'reason', 'parameters', 'initial_conditions', 'runs', 'skip'), ('model', 'reason'))
    model = ROOT / d['model']
    if model.resolve().parent not in (ROOT / 'examples/tstauto', ROOT / 'examples/ode') or not model.is_file():
        raise ValueError('model must be a file in examples/tstauto or examples/ode')
    if not isinstance(d['reason'], str) or not d['reason'].strip():
        raise ValueError('reason must explain the runs')
    for kind in ('parameters', 'initial_conditions'):
        for name, value in d.get(kind, {}).items():
            finite(value, name)
    if 'skip' in d:
        if not isinstance(d['skip'], str) or not d['skip'].strip() or 'runs' in d:
            raise ValueError('skip needs a reason and cannot also have runs')
        return d
    if not isinstance(d.get('runs'), list) or not d['runs']:
        raise ValueError('runs must be a nonempty list')
    names = set()
    for r in d['runs']:
        members(r, ('name', 'kind', 'start', 'parameter', 'parameters', 'auto', 'switch', 'skip'),
                ('name', 'kind', 'start', 'auto'))
        if not isinstance(r['name'], str) or not r['name'] or r['name'] in names or r['kind'] not in KINDS:
            raise ValueError('invalid run name or kind: ' + str(r))
        st = r['start']
        if not isinstance(st, dict):
            raise ValueError('start must be an object')
        if st.get('state') == 'current':
            members(st, ('state', 'settle'), ('state',))
            if 'settle' in st:
                finite(st['settle'], 'settle', positive=True)
        else:
            members(st, ('run', 'type', 'index'), ('run', 'type', 'index'))
            if st['run'] not in names or st['type'] not in ('EP', 'LP', 'HB', 'BP', 'PD', 'TR', 'UZ', 'MX'):
                raise ValueError('start must name an earlier run and point type')
            finite(st['index'], 'index', integer=True, positive=True)
        names.add(r['name'])
        if r['kind'] in ('two_parameter', 'fixed_period'):
            if not isinstance(r.get('parameters'), list) or len(r['parameters']) != 2 or len(set(r['parameters'])) != 2:
                raise ValueError('two-parameter run needs two distinct parameters')
        elif not isinstance(r.get('parameter'), str) or not r['parameter']:
            raise ValueError('run needs its parameter')
        members(r['auto'], NUMERICS, ('ds', 'nmax', 'npr', 'parmin', 'parmax', 'ntst', 'dsmin', 'dsmax'))
        for k, v in r['auto'].items():
            finite(v, k, integer=k in ('nmax', 'npr', 'ntst', 'ncol', 'mxbf', 'iid'))
        a = r['auto']
        if not 0 < a['dsmin'] <= abs(a['ds']) <= a['dsmax'] or a['parmin'] >= a['parmax']:
            raise ValueError('invalid continuation step or parameter range')
        if any(a[k] < 1 for k in ('nmax', 'npr', 'ntst')):
            raise ValueError('nmax, npr and ntst must be positive')
        if ('ncol' in a and not 2 <= a['ncol'] <= 7
                or 'iid' in a and not 0 <= a['iid'] <= 5
                or 'mxbf' in a and a['mxbf'] < 0
                or any(a[k] <= 0 for k in ('epsl', 'epsu', 'epss') if k in a)):
            raise ValueError('invalid AUTO setting (docs/protocol.md AUTO settings): ' + str(a))
        if 'skip' in r and (not isinstance(r['skip'], str) or not r['skip'].strip()):
            raise ValueError('run skip needs its reason')
        if 'switch' in r and not isinstance(r['switch'], bool):
            raise ValueError('switch must be boolean')
    if all('skip' in r for r in d['runs']):
        raise ValueError('no runnable continuations; use a model skip with its reason')
    return d


def completed(s, command=None):
    if command:
        s.send(**command)
    events, end = s.collect(lambda e: is_idle(e) or is_ask(e), timeout=TIMEOUT)
    ensure_completed(events, end)
    return events


def ensure_completed(events, end):
    errors = [placed(e) or str(e) for e in events if e.get('error') or e.get('ev') in ('bad', 'eof', 'ask')]
    if end is None or errors:
        raise RuntimeError('; '.join(errors) or 'command did not complete (safety timeout)')


def execute_run(s, r, diagram, ranges, model_pars, slots):
    """The replaceable adapter to pre-W306 AUTO keys and asks."""
    start = r['start']
    if start.get('state') == 'current':
        if 'settle' in start:
            completed(s, dict(cmd='set', kind='num', name='total_time', value=start['settle']))
            s.send(cmd='key', key='i')
            events, end = s.answer_asks(is_idle, {'menu': lambda e: {'key': 'g'}}, timeout=TIMEOUT)
            ensure_completed([e for e in events if e.get('ev') != 'ask'], end)
            state = next(e for e in reversed(events) if e.get('ev') == 'state')
            completed(s, dict(cmd='set', values=[dict(kind='ic', name=p[0], value=float(v))
                       for p, v in zip(state['ics'], state['now'])]))
    else:
        lo, hi = ranges[start['run']]
        points = [p for p in diagram.points[lo:hi] if p['sym'] == start['type']]
        if len(points) < start['index']:
            raise ValueError('start point not found: ' + str(start))
        completed(s, dict(cmd='auto', op='grab', label=points[start['index'] - 1]['lab']))
    pars = r.get('parameters', [r.get('parameter')])
    # Replace the whole slot list: a partial overwrite can put the same
    # parameter in two slots; AUTO's later slot then overwrites its value.
    auto_pars = (pars + [p for p in model_pars if p not in pars])[:slots]
    numerics = {NUMERICS[k]: v for k, v in r['auto'].items()}
    diagram.apply(completed(s, dict(cmd='auto', op='set', numerics=numerics,
                                   pars=auto_pars, marks=[], axes={'par1': pars[0],
                                   **({'par2': pars[1], 'plot': 4} if len(pars) == 2 else {})})))
    keys = {'equilibrium': 's' if start.get('state') == 'current' else 'e',
            'periodic': 'p', 'two_parameter': 't', 'fixed_period': 'f',
            'bvp': 'b', 'homoclinic': 'h', 'heteroclinic': 'e'}
    key = 's' if r.get('switch') else keys[r['kind']]
    lo = len(diagram.points)
    events = run_any(s, key, timeout=TIMEOUT)
    ensure_completed([e for e in events if e.get('ev') != 'ask'],
                     events[-1] if events and is_idle(events[-1]) else None)
    diagram.apply(events)
    if len(diagram.points) <= lo:
        raise RuntimeError('AUTO stored no points for ' + r['name'])
    stops = [e['stop'] for e in events if e.get('ev') == 'autoinfo' and e.get('stop')]
    if not stops or stops[-1]['why'].startswith('noconv'):
        raise RuntimeError('AUTO failed to converge: ' + str(stops[-1:] or events[-3:]))
    ranges[r['name']] = lo, len(diagram.points)


def session_diagram(s):
    """The diagram as the session file keeps it (auto/diagram.csv): every
    point's parameters, states, extremes and eigenvalues, each number
    written to round-trip exactly, where the diagram events carry only the
    plotted coordinates to seven digits."""
    with tempfile.TemporaryDirectory() as folder:
        path = os.path.join(folder, 'baseline.snapx')
        completed(s, dict(cmd='session', op='save', name=path, data=False))
        with zipfile.ZipFile(path) as z:
            return z.read(DIAGRAM_MEMBER).replace(b'\r\n', b'\n')


def compute(binary, d, threads):
    s = Server(binary, ROOT / d['model'])
    try:
        startup = completed(s)
        model_pars = [p[0] for e in startup if e.get('ev') == 'state' for p in e['pars']]
        completed(s, dict(cmd='data', events=['autoinfo', 'autosettings']))
        events = completed(s, dict(cmd='auto', op='set', threads=threads))
        setting = next(e for e in reversed(events) if e.get('ev') == 'autosettings')
        if setting['threads'] != threads:
            raise RuntimeError('thread setting was not applied')
        values = [dict(kind=kind, name=k, value=v) for group, kind in
                  (('parameters', 'par'), ('initial_conditions', 'ic')) for k, v in d.get(group, {}).items()]
        if values:
            completed(s, dict(cmd='set', values=values))
        diagram = Diagram().apply(open_auto(s))
        ranges = {}
        for r in d['runs']:
            if 'skip' not in r:
                execute_run(s, r, diagram, ranges, model_pars, len(setting['pars']))
        return session_diagram(s), len(diagram.points), setting['threads_auto']
    finally:
        s.close()
        if s.proc.returncode and sys.exc_info()[0] is None:
            raise RuntimeError('server exited %s: %s' % (s.proc.returncode, ''.join(s.log)[-1500:]))


def difference(name, want, got):
    a, b = want.splitlines(), got.splitlines()
    i = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
    return '%s: first difference at %s\n  want: %s\n  got:  %s' % (
        name, 'the header' if i == 0 else 'point %d' % i, a[i].decode() if i < len(a) else '(end of diagram)',
        b[i].decode() if i < len(b) else '(end of diagram)')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--server', '--bin', default=None)
    ap.add_argument('--only', help='comma-separated description names (without .json)')
    ap.add_argument('--update', action='store_true', help='rewrite only for an intended numerical change')
    ap.add_argument('--keep', type=Path, help='save differing diagrams here (CI artifact)')
    args = ap.parse_args()
    os.chdir(ROOT)
    binary = args.server or ('./xppautX.exe' if Path('xppautX.exe').exists() else './xppautX')
    check = Checker()
    descriptions = {}
    try:
        for path in sorted(BASE.glob('*.json')):
            try:
                descriptions[path.stem] = read_description(path)
            except (ValueError, TypeError, KeyError, AttributeError) as e:
                raise ValueError('%s: %s' % (path.relative_to(ROOT), e)) from e
        expected = {f.as_posix() for f in Path('examples/tstauto').iterdir() if f.suffix in ('.ode', '.odex')}
        expected.update(f.as_posix() for f in Path('examples/ode').glob('*.odex')
                        if re.search(r'\bauto\s*:', re.sub(r'#[^\n]*', '', f.read_text(encoding='utf-8'))))
        models = [d['model'] for d in descriptions.values()]
        if set(models) != expected or len(models) != len(set(models)):
            raise ValueError('description coverage: missing %s, extra %s, or duplicate models' %
                             (sorted(expected - set(models)), sorted(set(models) - expected)))
        selected = args.only.split(',') if args.only else list(descriptions)
        if not selected or set(selected) - descriptions.keys():
            raise ValueError('unknown --only models: ' + str(set(selected) - descriptions.keys()))
    except (ValueError, OSError) as e:
        print('FAIL descriptions: ' + str(e))
        return 1
    durations = {1: 0.0, 'auto': 0.0}
    skipped = points = 0
    for name in selected:
        d = descriptions[name]
        if 'skip' in d:
            skipped += 1
            print('SKIP %s: %s' % (name, d['skip']))
            continue
        for r in d['runs']:
            if 'skip' in r:
                print('SKIP %s/%s: %s' % (name, r['name'], r['skip']))
        path = BASE / (name + '.diagram.csv')
        computed = {}
        for threads in durations:
            began = time.monotonic()
            try:
                data, count, auto_count = compute(binary, d, threads)
                computed[threads] = data
                points += count
                if args.update:
                    if threads == 'auto':
                        same = computed.get(1) == data
                        check(name + ': 1 and auto threads agree', same,
                              difference(name, computed.get(1, b''), data) if not same else '')
                        if same:
                            old = path.read_bytes().replace(b'\r', b'') if path.exists() else b''
                            if old != data:
                                path.write_bytes(data)
                                print('UPDATE %s: %d points' % (name, count))
                else:
                    want = path.read_bytes().replace(b'\r', b'') if path.exists() else b''
                    same = want == data
                    check('%s threads=%s (%d points, auto=%d)' % (name, threads, count, auto_count),
                          same, difference(name, want, data) if not same else '')
                    if not same and args.keep:
                        args.keep.mkdir(parents=True, exist_ok=True)
                        (args.keep / ('%s.threads-%s.diagram.csv' % (name, threads))).write_bytes(data)
                        (args.keep / (name + '.expected.diagram.csv')).write_bytes(want)
            except (OSError, ValueError, RuntimeError, StopIteration, KeyError) as e:
                check('%s threads=%s' % (name, threads), False, str(e))
            finally:
                durations[threads] += time.monotonic() - began
    print('perf: AUTO 1 thread %.3fs; auto threads %.3fs' % (durations[1], durations['auto']))
    print('AUTO %s: %d models, %d skipped, %d points, %d checks, %d failures' %
          ('FAILED' if check.failures else 'ok', len(selected), skipped, points, check.checks, check.failures))
    return int(bool(check.failures))


if __name__ == '__main__':
    sys.exit(main())
