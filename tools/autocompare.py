#!/usr/bin/env python3
"""W299/#357: one AUTO numerical baseline for every platform.

Descriptions use W306's run vocabulary, not recorded keys. Only execute_run
translates it to today's menus. Each model runs serially at 1 and auto threads.
No elapsed-time assertion: completion is idle; TIMEOUT is only a hang guard.
"""
import argparse
import cmath
import csv
import io
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
    what a run's start by type and index needs. `runs` keeps each finished run's own points as that run
    left them: a later run on other axes (a two-parameter curve) redraws the diagram and its events label
    only what it plots, but a start grabs a point by its label from the run that found it."""
    def __init__(self):
        self.points = []
        self.runs = {}

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
    members(d, ('model', 'reason', 'parameters', 'initial_conditions', 'runs', 'skip', 'differences', *REFERENCE_SKIPS.values()), ('model', 'reason'))
    model = ROOT / d['model']
    if model.resolve().parent not in (ROOT / 'examples/tstauto', ROOT / 'examples/ode', ROOT / 'tools/models') or not model.is_file():
        raise ValueError('model must be a file in examples/tstauto, examples/ode or tools/models')
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
        members(r, ('name', 'kind', 'start', 'parameter', 'parameters', 'auto', 'switch', 'skip', 'expected_error', 'connection'),
                ('name', 'kind', 'start', 'auto'))
        if not isinstance(r['name'], str) or not r['name'] or r['name'] in names or r['kind'] not in KINDS:
            raise ValueError('invalid run name or kind: ' + str(r))
        if 'expected_error' in r and (not isinstance(r['expected_error'], str) or not r['expected_error'].strip()):
            raise ValueError('expected_error must name the error')
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
        if r['kind'] in ('homoclinic', 'heteroclinic'):
            connection = r.get('connection')
            members(connection, ('left', 'right', 'nunstab', 'nstab'), ('left', 'right', 'nunstab', 'nstab'))
            for side in ('left', 'right'):
                if not isinstance(connection[side], list) or not connection[side]:
                    raise ValueError('connection equilibria must be nonempty lists')
                for v in connection[side]:
                    finite(v, side)
            if len(connection['left']) != len(connection['right']):
                raise ValueError('connection equilibria must have equal dimensions')
            for key in ('nunstab', 'nstab'):
                finite(connection[key], key, integer=True, positive=True)
            if connection['nunstab'] + connection['nstab'] != len(connection['left']):
                raise ValueError('connection dimensions must span the phase space')
        elif 'connection' in r:
            raise ValueError('connection settings require HomCont')
        if r['kind'] in ('two_parameter', 'fixed_period', 'homoclinic', 'heteroclinic'):
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
    for k in d.get('differences', ()):
        members(k, ('reference', 'run', 'side', 'type', 'index', 'cause'), ('reference', 'run', 'side', 'type', 'index', 'cause'))
        if (k['reference'] not in REFERENCES or k['run'] not in names or k['side'] not in ('ours', 'reference')
                or k['type'] not in ('EP', *BIFURCATIONS.values(), *CODIM_TWO) or not isinstance(k['cause'], str) or not k['cause'].strip()):
            raise ValueError('invalid traced difference: ' + str(k))
        finite(k['index'], 'index', integer=True, positive=True)
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


def execute_run(s, r, diagram, ranges):
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
        points = [p for p in diagram.runs[start['run']] if p['sym'] == start['type']]
        if len(points) < start['index']:
            raise ValueError('start point not found: ' + str(start))
        completed(s, dict(cmd='auto', op='grab', label=points[start['index'] - 1]['lab']))
    pars = r.get('parameters', [r.get('parameter')])
    numerics = {NUMERICS[k]: v for k, v in r['auto'].items()}
    diagram.apply(completed(s, dict(cmd='auto', op='set', numerics=numerics,
                                   marks=[], axes={'par1': pars[0],
                                   **({'par2': pars[1], 'plot': 4} if len(pars) == 2 else {})})))
    keys = {'equilibrium': 's' if start.get('state') == 'current' else 'e',
            'periodic': 'p', 'two_parameter': 't', 'fixed_period': 'f',
            'bvp': 'b', 'homoclinic': 'h', 'heteroclinic': 'e'}
    key = 's' if r.get('switch') else keys[r['kind']]
    lo = len(diagram.points)
    if 'connection' in r:
        c = r['connection']
        values = [c['nunstab'], *c['left'], c['nstab'], *c['right']]
        def connection_form(e):
            if e.get('title') != 'Homoclinic info' or len(e['values']) != len(values):
                raise ValueError('unexpected HomCont form: ' + str(e))
            return dict(ok=1, values=[str(v) for v in values])
        s.send(cmd='key', win='auto', key='r')
        events, end = s.answer_asks(is_idle, {'menu': lambda e: dict(key=key), 'form': connection_form}, timeout=TIMEOUT)
        if end is None:
            raise RuntimeError('HomCont did not complete (safety timeout)')
    else:
        events = run_any(s, key, timeout=TIMEOUT)
    result = [e for e in events if e.get('ev') != 'ask']
    if 'expected_error' in r:
        errors = [e for e in result if e.get('error')]
        if len(errors) != 1 or r['expected_error'] != errors[0]['error']:
            raise RuntimeError('expected error %r, got %s' % (r['expected_error'], errors))
        if not errors[0].get('file') or not errors[0].get('line'):
            raise RuntimeError('expected error has no source location')
        result.remove(errors[0])
    ensure_completed(result, events[-1] if events and is_idle(events[-1]) else None)
    diagram.apply(events)
    if len(diagram.points) <= lo:
        raise RuntimeError('AUTO stored no points for ' + r['name'])
    stops = [e['stop'] for e in events if e.get('ev') == 'autoinfo' and e.get('stop')]
    if 'expected_error' not in r and (not stops or stops[-1]['why'].startswith('noconv')):
        raise RuntimeError('AUTO failed to converge: ' + str(stops[-1:] or events[-3:]))
    ranges[r['name']] = lo, len(diagram.points)
    diagram.runs[r['name']] = [dict(p) for p in diagram.points[lo:]]


def session_diagram(s):
    """The diagram as the session file keeps it (auto/diagram.csv): every
    point's parameters, states, extremes and eigenvalues, each number
    written to round-trip exactly, where the diagram events carry only the
    plotted coordinates to seven digits."""
    with tempfile.TemporaryDirectory() as folder:
        path = os.path.join(folder, 'baseline.snapx')
        completed(s, dict(cmd='session', op='save', name=path, data=False))
        with zipfile.ZipFile(path) as z:
            data = z.read(DIAGRAM_MEMBER).replace(b'\r\n', b'\n')
            for point, row in enumerate(diagram_rows(data)):
                for key,value in row.items():
                    for item in value.split(';') if key in ('continued','values') else [value]:
                        finite(float(item), 'diagram point %d %s' % (point,key))
            return data


def compute(binary, d, threads):
    s = Server(binary, ROOT / d['model'])
    try:
        completed(s)
        completed(s, dict(cmd='auto', op='set', threads=threads))
        # Subscription returns the current settings even when a repeated
        # threads=1 assignment changed nothing and emitted no change event.
        events = completed(s, dict(cmd='data', events=['autoinfo', 'autosettings']))
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
                execute_run(s, r, diagram, ranges)
        return session_diagram(s), len(diagram.points), setting['threads_auto'], ranges
    finally:
        s.close()
        if s.proc.returncode and sys.exc_info()[0] is None:
            raise RuntimeError('server exited %s: %s' % (s.proc.returncode, ''.join(s.log)[-1500:]))


# --- W300/#358: the special points compared with other programs' -----------------
# A reference file beside a diagram holds another program's special points for the
# same problem, written by tools/auto_reference.py on a machine that has the programs:
# `.auto07p` is AUTO-07p 0.9.4's, `.xppaut` XPPAUT 8.0's (its AUTO window driven with
# xdotool on Xvfb). They are compared here, from the committed files alone, matched by type and
# order. A missing or extra special point is a difference.
REFERENCES = {'.auto07p': 'AUTO-07p', '.xppaut': 'XPPAUT 8.0'}
# a description with no reference file says why in this field (an explicit reason, never silence)
REFERENCE_SKIPS = {'.auto07p': 'auto07p_skip', '.xppaut': 'xppaut_skip'}
REFERENCE_FORMAT = 1
# the types a reference point can have: the problem's own special points, then the codim-2
# points of a two-parameter curve (AUTO-07p's TY names: Bogdanov-Takens, cusp, zero-Hopf,
# generalised Hopf; the cycles' strong resonances R1-R4, fold-flip, fold-torus, ...)
SPECIAL_TYPES = ('EP', 'MX', 'BP', 'LP', 'HB', 'PD', 'TR')
CODIM_TWO = ('BT', 'CP', 'ZH', 'GH', 'R1', 'R2', 'R3', 'R4', 'LPD', 'LTR', 'PTR', 'TTR')
# the type of a stored point (itp, core/auto_nox.h) that both programs label a special point
BIFURCATIONS = {1: 'BP', 2: 'LP', 3: 'HB', 5: 'LP', 6: 'BP', 7: 'PD', 8: 'TR',
                -21: 'BT', -22: 'CP', -23: 'ZH', -31: 'BT', -32: 'GH', -33: 'ZH',
                **{-10 * start - 5 - i: kind for start in (5, 7, 8)
                   for i, kind in enumerate(('R1', 'R2', 'R3', 'R4'))}}
POINT_FIELDS = ('type', 'branch', 'label', 'point', 'par', 'norm', 'period', 'spectrum', 'values')
# Newton's convergence test of both codes (autlib1.c solvae, ae.f90 stepae) stops on a
# relative change below EPSL for the parameter and EPSU for the state, so a computed
# point is that close to the solution of its discretised equations
NEWTON_TOLERANCE = 2
# AUTO's Jacobian is a forward difference of the right-hand side with step HMACH*(1+|u|)
# (HMACH = 1e-7): the eigenvalues of that Jacobian carry its truncation error relative to
# the Jacobian's own size
FINITE_DIFFERENCE = 1e-7
# AUTO-07p's diagnostics file prints an eigenvalue or multiplier with six significant digits
PRINTED = 5e-6


def read_reference(path, d):
    """A reference file, strictly: its format, the description's model and runs. It holds every run, a skipped
    one too (kept for the card its reason names), and gives back the runs that are compared."""
    ref = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=no_duplicates)
    members(ref, ('format', 'source', 'model', 'runs'), ('format', 'source', 'model', 'runs'))
    runs = d['runs']
    if ref['format'] != REFERENCE_FORMAT or ref['model'] != d['model'] or not isinstance(ref['source'], str):
        raise ValueError('format, source or model does not match the description')
    if [r.get('name') for r in ref['runs']] != [r['name'] for r in runs]:
        raise ValueError('runs %s, the description has %s' % ([r.get('name') for r in ref['runs']], [r['name'] for r in runs]))
    for r, dr in zip(ref['runs'], runs):
        curve = dr['kind'] in ('two_parameter', 'homoclinic', 'heteroclinic')  # a curve's file holds its parameters, its samples and its second value
        axes = 'parameters' if curve else 'parameter'
        members(r, ('name', axes, 'unavailable', 'points', *(('curve',) if curve else ())), ('name', axes, 'points'))
        if r[axes] != dr.get(axes) or not isinstance(r['points'], list):
            raise ValueError('run %s: %s or points' % (r['name'], axes))
        if 'unavailable' in r and not (isinstance(r['unavailable'], str) and r['unavailable'].strip()):
            raise ValueError('run %s: unavailable needs its reason' % r['name'])
        if curve and 'unavailable' not in r:
            if not isinstance(r.get('curve'), list) or len(r['curve']) < 2:
                raise ValueError('run %s: a curve of at least two samples' % r['name'])
            for sample in r['curve']:
                if not isinstance(sample, list) or len(sample) != 3:
                    raise ValueError('run %s: curve sample %s is not [parameter, second parameter, norm]' % (r['name'], sample))
                for v in sample:
                    finite(v, 'curve sample')
        for p in r['points']:
            members(p, POINT_FIELDS + (('par2',) if curve else ()), POINT_FIELDS + (('par2',) if curve else ()))
            if p['type'] not in SPECIAL_TYPES + CODIM_TWO or p['spectrum'] not in ('', 'eigenvalue', 'multiplier'):
                raise ValueError('run %s: point %s' % (r['name'], p))
            finite(p['par'], 'par')
            if curve:
                finite(p['par2'], 'par2')
            finite(p['norm'], 'norm')
            if p['period'] is not None:
                finite(p['period'], 'period')
            if not isinstance(p['values'], list) or not all(isinstance(v, list) and len(v) == 2 for v in p['values']):
                raise ValueError('run %s: spectrum of label %s' % (r['name'], p['label']))
    return {**ref, 'runs': [r for r, dr in zip(ref['runs'], runs) if 'skip' not in dr]}


def read_references(descriptions):
    """{description: {suffix: reference}}: every reference file read and checked against its
    description; a description that is not skipped has each program's file or its reason for
    having none; a file without a description is refused."""
    found = {suffix: {p.name[:-len(suffix)] for p in BASE.glob('*' + suffix)} for suffix in REFERENCES}
    refs = {name: {} for name in descriptions}
    for suffix, names in found.items():
        for name in sorted(names):
            d = descriptions.get(name)
            path = BASE / (name + suffix)
            if d is None or 'skip' in d:
                raise ValueError('%s: no runnable description %s.json' % (path.relative_to(ROOT), name))
            try:
                refs[name][suffix] = read_reference(path, d)
            except (ValueError, TypeError, KeyError, AttributeError) as e:
                raise ValueError('%s: %s' % (path.relative_to(ROOT), e)) from e
    for name, d in descriptions.items():
        for suffix, field in REFERENCE_SKIPS.items():
            if 'skip' in d:
                continue
            has_file, reason = name in found[suffix], d.get(field)
            if has_file == (reason is not None) or (reason is not None and not (isinstance(reason, str) and reason.strip())):
                raise ValueError('%s: needs either %s%s or a %s reason, not both or neither' % (name, name, suffix, field))
    return refs


def reference_spectrum(p):
    """A reference point's spectrum as the diagram stores it, sorted, and the size of its
    eigenvalues: XPPAUT's diagram keeps the multiplier e^lambda of an equilibrium's eigenvalue
    lambda (so the stability circle is the unit circle; autevd.c send_eigen), a periodic orbit's
    Floquet multipliers as they are."""
    values = [complex(*v) for v in p['values']]
    size = max(abs(v) for v in values)
    if p['spectrum'] == 'eigenvalue':
        values = [cmath.exp(v) for v in values]
    return by_value((v.real, v.imag) for v in values), size


def diagram_rows(data):
    """Resolve branch constants and sparse continued values from the current diagram layout."""
    lines=data.decode('utf-8').splitlines()
    branches={}
    while lines and lines[0].startswith('branch,'):
        record=next(csv.reader([lines.pop(0)]))
        branches[record[1]]=record[2:]
    rows = list(csv.DictReader(lines))
    for row in rows:
        parameters=list(branches[row['branch']])
        for index,value in zip(row['continued'].split(';'),row['values'].split(';')):
            if int(index)<len(parameters): parameters[int(index)]=value
        for index,value in enumerate(parameters): row['par%d'%(index+1)]=value
        row['continued_parameter']=row['values'].split(';')[0]
        if ';' in row['values']:  # a two-parameter run continues two: the second, in the order of the run's parameters
            row['continued_parameter2']=row['values'].split(';')[1]
    return rows


def stored_rows(data, ranges):
    rows=diagram_rows(data)
    return {name: rows[lo:hi] for name, (lo, hi) in ranges.items()}


def by_value(values):
    """A spectrum in order of value. A conjugate pair's real parts agree only to rounding, so they are
    ordered by the real part rounded, then the imaginary part: the same order for both programs' pair."""
    return sorted(values, key=lambda v: (round(v[0], 9), v[1]))


def spectrum(row):
    n = sum(1 for k in row if k.startswith('evr'))
    return by_value((float(row['evr%d' % i]), float(row['evi%d' % i])) for i in range(1, n + 1))


def quantity(row, name):
    if name == 'spectrum':
        return spectrum(row)
    return float(row[{'par': 'continued_parameter', 'par2': 'continued_parameter2', 'norm': 'norm', 'period': 'per'}[name]])


def spread(a, b):
    """The distance between two values of a quantity: of two spectra, the largest between
    eigenvalues paired in order."""
    if isinstance(a, list):
        return max(abs(complex(*x) - complex(*y)) for x, y in zip(a, b))
    return abs(a - b)


def slope(rows, i, name, cycle=False):
    """The largest rate of change of a quantity per unit of the (parameter, norm) path length
    between the stored point i and its neighbours: a position error along the branch moves a
    quantity by at most this much per unit."""
    best = 0.0
    for j in (i - 1, i + 1):
        if 0 <= j < len(rows):
            coordinates = ('par', 'par2', 'norm') if cycle and 'continued_parameter2' in rows[i] and 'continued_parameter2' in rows[j] else ('par', 'norm')
            ds = math.hypot(*(quantity(rows[i], q) - quantity(rows[j], q) for q in coordinates))
            if ds > 0:
                best = max(best, spread(quantity(rows[i], name), quantity(rows[j], name)) / ds)
    return best


def compare_run(rows, auto, kind, ref_points, who, known=()):
    """Differences (strings) and measurements (quantity, point, difference, tolerance) of one run
    against a reference's special points. The tolerance of a quantity q at a special point:

      2 * EPSS*(1+DSMAX) * g  +  2 * EPS * (1+|q|)  [+ 2 * FD * (1+|q|) for an eigenvalue]

    The first term is the position error along the branch: both programs locate a special point
    by the secant method until the arclength step is below EPSS*(1+sqrt|DS*DSMAX|) (autlib1.c
    lcspae, ae.f90 lcspae), at most EPSS*(1+DSMAX); the two may differ by twice that, and g is
    the rate of change of q per unit of the (parameter, norm) path length measured from our own
    stored points around it (so the formula never allows more than the branch itself moves q).
    The second is the Newton convergence of each point (EPSL for the parameter, EPSU for the
    rest), twice. The third is the finite-difference Jacobian's accuracy. The tolerance is never
    widened to make a point pass: a difference beyond it is traced (docs/auto-references.md)."""
    epss, epsl, epsu = auto.get('epss', 1e-4), auto.get('epsl', 1e-4), auto.get('epsu', 1e-4)
    loc = epss * (1 + auto['dsmax'])
    ours = []
    for i, r in enumerate(rows):
        itp = int(r['itp'])
        symbol = BIFURCATIONS.get(itp)
        if symbol is None and itp > 9:
            symbol = BIFURCATIONS.get(itp % 10)  # the ordinary kind on a two-parameter curve
        if symbol:
            ours.append((symbol, i))
        elif i == 0 and itp == 9 and kind in ('equilibrium', 'bvp'):
            ours.append(('EP', i))
    detected = (*BIFURCATIONS.values(), *(CODIM_TWO if kind == 'two_parameter' else ()))
    theirs = [p for p in ref_points if p['type'] in detected or (p['type'] == 'EP' and p['point'] == 1)]
    diffs = []
    dropped = {'ours': set(), 'reference': set()}
    for k in known:  # a traced difference: one point the other side has no counterpart of (index: among its type, as listed)
        kinds = [t for t, _ in ours] if k['side'] == 'ours' else [p['type'] for p in theirs]
        at = [j for j, t in enumerate(kinds) if t == k['type']]
        if len(at) < k['index']:
            diffs.append('%s: traced difference not present any more, remove it: %s' % (who, k))
        else:
            dropped[k['side']].add(at[k['index'] - 1])
    ours = [x for j, x in enumerate(ours) if j not in dropped['ours']]
    theirs = [x for j, x in enumerate(theirs) if j not in dropped['reference']]
    want, got = [p['type'] for p in theirs], [t for t, _ in ours]
    if want != got:
        return diffs + ['%s: special points %s, ours %s' % (who, want, got)], []
    measures = []
    for (t, i), p in zip(ours, theirs):
        # the first stored point of a run holds no spectrum (zeros: not computed, core/auto_stability.cpp)
        names = ['par', 'norm'] + (['par2'] if kind == 'two_parameter' and 'par2' in p else []) + (['period'] if (t == 'HB' or kind == 'periodic') and p['period'] is not None else []) \
            + (['spectrum'] if p['values'] and i else [])
        for q in names:
            a = quantity(rows[i], q)
            b, size = reference_spectrum(p) if q == 'spectrum' else (p[q], 0)
            if q == 'spectrum' and len(a) != len(b):
                diffs.append('%s %s spectrum: %d values, ours %d' % (who, t, len(b), len(a)))
                continue
            scale = max(abs(complex(*x)) for x in a) if q == 'spectrum' else abs(a)
            # a spectrum's error: its printed digits and the Jacobian's finite differences, relative to
            # the size of the eigenvalues, carried through e^lambda when that is what is stored
            tol = (2 * loc * slope(rows, i, q, kind == 'two_parameter' and p.get('spectrum') == 'multiplier') + NEWTON_TOLERANCE * (epsl if q in ('par', 'par2') else epsu) * (1 + scale)
                   + (NEWTON_TOLERANCE * (FINITE_DIFFERENCE + PRINTED) * (1 + size) * scale if q == 'spectrum' else 0))
            diff = spread(a, b)
            measures.append((q, '%s %d' % (t, i + 1), diff, tol))
            if not diff <= tol:
                diffs.append('%s %s (stored point %d) %s: ours %s, reference %s, difference %.3g > tolerance %.3g'
                             % (who, t, i + 1, q, a, b, diff, tol))
    return diffs, measures


def project(point, polyline, tolerances):
    """Match a sample to the polyline's coordinate-wise uncertainty tubes.
    Each segment has Newton plus chord-error bounds. A geometrically nearest
    vertex can be a worse match than a coarser chord on another lap of a closed
    curve. Rank by the largest fraction of a bound consumed, without changing
    any bound. The feasible interval preserves all three coordinates together."""
    best = None
    for i, bounds in enumerate(tolerances):
        a, b = polyline[i], polyline[i + 1]
        along = [(b[k] - a[k]) / bounds[k] for k in range(3)]
        length = sum(x * x for x in along)
        t = 0.0 if length == 0 else min(1.0, max(0.0, sum((point[k] - a[k]) / bounds[k] * along[k] for k in range(3)) / length))
        lo, hi = 0.0, 1.0
        for k in range(3):
            delta = b[k] - a[k]
            if delta:
                ends = [(point[k] - a[k] + sign * bounds[k]) / delta for sign in (-1, 1)]
                lo, hi = max(lo, min(ends)), min(hi, max(ends))
            elif abs(point[k] - a[k]) > bounds[k]:
                lo, hi = 1.0, 0.0
        if lo <= hi:
            t = min(hi, max(lo, t))
        residual = [point[k] - (a[k] + t * (b[k] - a[k])) for k in range(3)]
        distance = max(abs(residual[k]) / bounds[k] for k in range(3))
        if best is None or distance < best[0]:
            best = distance, i, residual
    return best[1], best[2]


def chord_error(polyline, i):
    """What replacing the curve between vertices i and i+1 by the straight line costs in each coordinate: a
    quarter of the largest second difference at either end. The chord of a smooth curve through points a
    step h apart misses it by at most h^2|q''|/8, which the second difference estimates as h^2|q''|; the
    steps vary by at most a factor two (their adaptation), hence a quarter and not an eighth."""
    worst = [0.0, 0.0, 0.0]
    for j in (i, i + 1):
        if 0 < j < len(polyline) - 1:
            for k in range(3):
                worst[k] = max(worst[k], abs(polyline[j - 1][k] - 2 * polyline[j][k] + polyline[j + 1][k]) / 4)
    return worst


CURVE_QUANTITIES = ('curve par', 'curve par2', 'curve norm')


def compare_curve(rows, auto, ref_curve, who):
    """Differences (strings) and measurements of a two-parameter run's curve against a reference's
    samples. The programs' steps differ, so points are matched by where they lie: every sample of one
    curve against the other as a polyline. Match all three coordinates against each segment's
    uncertainty tube: parameter, second parameter and norm must agree within

      2 * EPS * (1+|q|)  +  the chord error of the other curve's polyline (chord_error)

    EPS is Newton's convergence tolerance of every continued point (EPSL for a parameter, EPSU for the norm),
    twice for two programs; no location term, a curve's samples being converged points and not located
    ones. Both directions are checked, so a curve that stops short or runs on is a difference too.
    Only samples inside the run's bounds on its first parameter (RL0, RL1) are compared: a run ends at
    the first point beyond a bound, which depends on the step before it, and two programs' step
    sequences differ; samples inside the bounds have the other curve's polyline around them."""
    epsl, epsu = auto.get('epsl', 1e-4), auto.get('epsu', 1e-4)
    # a curve of cycles first stores the starting data of the curve (the run xppautX does before it) as a branch of
    # one continued parameter: those rows are not on the curve
    ours = [(float(r['continued_parameter']), float(r['continued_parameter2']), float(r['norm']))
            for r in rows if 'continued_parameter2' in r]
    ref = [tuple(v) for v in ref_curve]
    diffs, worst = [], {}
    for source, target, side in ((ref, ours, 'reference'), (ours, ref, 'ours')):
        chords = [chord_error(target, i) for i in range(len(target) - 1)]
        failed = {}
        for n, point in enumerate(source):
            if not auto['parmin'] <= point[0] <= auto['parmax']:
                continue
            tolerances = [[NEWTON_TOLERANCE * (epsu if k == 2 else epsl) * (1 + abs(point[k])) + chord[k]
                           for k in range(3)] for chord in chords]
            i, residual = project(point, target, tolerances)
            for k, q in enumerate(CURVE_QUANTITIES):
                tol = tolerances[i][k]
                diff = abs(residual[k])
                if q not in worst or diff / tol > worst[q][0] / worst[q][1]:
                    worst[q] = diff, tol, '%s sample %d' % (side, n + 1)
                if diff > tol:
                    failed.setdefault(q, []).append((n + 1, point, diff, tol))
        for q, bad in failed.items():
            n, point, diff, tol = bad[0]
            diffs.append('%s %s: %d of %d %s samples are off the other curve; the first, sample %d %s, is %.3g away in the %s > tolerance %.3g'
                         % (who, q, len(bad), len(source), side, n, list(point), diff, q[6:], tol))
    return diffs, [(q, w[2], w[0], w[1]) for q, w in worst.items()]


def agreement(name, suffix, measures, diffs, traced=0):
    """One line of docs/auto-references.md's table: a reference's points compared and the largest
    absolute difference per quantity."""
    worst = {}
    for sx, _, q, _, diff, tol in measures:
        if sx == suffix:
            worst[q] = max(worst.get(q, 0.0), diff)
    cells = ', '.join('%s %.2g' % (q, worst[q]) for q in ('par', 'par2', 'norm', 'period', 'spectrum', *CURVE_QUANTITIES) if q in worst)
    return '%s | %s | %d | %s | %s' % (name, REFERENCES[suffix], sum(1 for m in measures if m[0] == suffix and m[2] == 'par'),
                                    cells or '-', 'differs: %d' % len(diffs) if diffs else
                                    'agrees, %d traced' % traced if traced else 'agrees')


def compare_references(name, d, data, ranges, refs):
    rows = stored_rows(data, ranges)
    diffs, measures = [], []
    for suffix, ref in refs.items():
        for r in ref['runs']:
            if 'unavailable' in r:
                continue
            dr = next(x for x in d['runs'] if x['name'] == r['name'])
            known = [k for k in d.get('differences', ()) if k['reference'] == suffix and k['run'] == r['name']]
            a, m = compare_run(rows[r['name']], dr['auto'], dr['kind'], r['points'],
                               '%s %s/%s' % (name, REFERENCES[suffix], r['name']), known)
            if 'curve' in r:
                c, cm = compare_curve(rows[r['name']], dr['auto'], r['curve'], '%s %s/%s' % (name, REFERENCES[suffix], r['name']))
                a, m = a + c, m + cm
            diffs += a
            measures += [(suffix, r['name']) + x for x in m]
    return diffs, measures


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
    ap.add_argument('--measures', action='store_true', help='print every compared quantity: difference and tolerance')
    ap.add_argument('--table', action='store_true', help='print the lines of docs/auto-references.md table')
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
        # a model's name is its folder and f.name: MSYS2's Python gives as_posix() a backslash (W344)
        expected = {'examples/tstauto/' + f.name for f in Path('examples/tstauto').iterdir() if f.suffix in ('.ode', '.odex')}
        expected.update('examples/ode/' + f.name for f in Path('examples/ode').glob('*.odex')
                        if re.search(r'\b(continuation|diagram)\s*:', re.sub(r'#[^\n]*', '', f.read_text(encoding='utf-8'))))
        models = [d['model'] for d in descriptions.values()]
        # tools/models also holds non-AUTO checks; only described models join this set.
        expected.update(model for model in models if (ROOT / model).resolve().parent == ROOT / 'tools/models')
        if set(models) != expected:
            raise ValueError('description coverage: missing %s, extra %s' %
                             (sorted(expected - set(models)), sorted(set(models) - expected)))
        references = read_references(descriptions)
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
        for suffix, field in REFERENCE_SKIPS.items():
            if field in d:
                status = 'PENDING' if 'pending' in d[field].lower() else 'SKIP'
                print('%s %s %s: %s' % (status, name, REFERENCES[suffix], d[field]))
        for r in d['runs']:
            if 'skip' in r:
                print('SKIP %s/%s: %s' % (name, r['name'], r['skip']))
        path = BASE / (name + '.diagram.csv')
        computed = {}
        for threads in durations:
            began = time.monotonic()
            try:
                data, count, auto_count, ranges = compute(binary, d, threads)
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
                    if threads == 1 and references[name]:
                        diffs, measures = compare_references(name, d, data, ranges, references[name])
                        check('%s against %s (%d quantities)' % (name, ' and '.join(
                              REFERENCES[x] for x in references[name]), len(measures)), not diffs, '\n'.join(diffs))
                        for m in measures if args.measures else ():
                            print('MEASURE %s %s %s %s %s difference %.3g tolerance %.3g' % ((name,) + m))
                        for suffix in references[name] if args.table else ():
                            print('TABLE | ' + agreement(name, suffix, measures, [x for x in diffs if REFERENCES[suffix] in x],
                                                  sum(1 for k in d.get('differences', ()) if k['reference'] == suffix)) + ' |')
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
