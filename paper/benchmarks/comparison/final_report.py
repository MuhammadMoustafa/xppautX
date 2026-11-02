#!/usr/bin/env -S uv run --locked python
"""W589: offline W583 comparison report and paper figures, computed once.

uv run --locked python paper/benchmarks/comparison/final_report.py --final ~/xppautx-comparison/final --out DIR

Readers consolidate report-20261031/scripts/{load,tables,labels}.py and
report-20261101/scripts/w557_load.py. No historical report is imported.
Only reads the frozen series; never runs a benchmark. Times are whole-process
wall seconds (continuation: one branch run), excluding repetition zero.
"""
import argparse
from collections import Counter, defaultdict
import csv
import json
import math
from pathlib import Path
import re
import shutil
import sys

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import continuation
from common import save
from integration import effective_numerics, screen

# Okabe-Ito colours; identities stay fixed across every renderer and panel.
COLORS = {'xppautx': '#0072B2', 'xppaut': '#D55E00', 'auto07p': '#CC79A7', 'python': '#009E73'}
NAMES = {'xppautx': 'xppautX', 'xppaut': 'XPPAUT 8.0', 'auto07p': 'AUTO-07p', 'python': 'Python'}
INTEGRATION = ('screen', 'modeldb', 'research', 'izhikevich', 'python')
# W590: what a reader of the paper's figures needs to know of each study and method, from the studies' own sources
# (paper/benchmarks/five-model-screen/sources, modeldb-267710, examples/ode/mu26.ode and ma23.ode, izhikevich/README.md,
# size-scaling/scaling.py, tools/python-integration/integrate.py).
STUDY_TITLES = {
    'screen': 'Model screen: Amari bump, weight clustering, Terman network, Kuramoto (100), Morris-Lecar chain (200), PY_S1Bf',
    'modeldb': 'ModelDB 267710 motoneuron: at rest and firing',
    'research': 'Published neurons: Ma et al. 2023 PV interneuron, Mu 2026 two-compartment PVN neuron',
    'izhikevich': 'Izhikevich 2003 and 2004 firing behaviours (28 models)',
    'python': 'Python re-implementation of xppautX\'s methods, on the screen models',
    'scaling': 'Size scaling: Amari, Kuramoto and Morris-Lecar chains, N = 10 to 6400 variables',
    'continuation': 'Continuation (AUTO): equilibria, cycles, two-parameter curves',
}
PANEL_TITLES = {
    'single': 'Published and demo models,\neach at its own method', 'izhikevich': 'Izhikevich\n(28 models)',
    'python': 'Python\n(screen models)', 'continuation': 'Continuation\n(AUTO)',
    'scaling': 'Model size\n(N = 10-6400)',
}
MODEL_TITLES = {
    'amari': 'Amari bump', 'clustor': 'Weight clustering', 'episodic': 'Terman network', 'kuramot100': 'Kuramoto (100)',
    'lecdiff200': 'Morris-Lecar chain (200)', 'PY_S1Bf': 'PY_S1Bf', 'fire': 'Motoneuron, firing', 'rest': 'Motoneuron, at rest',
    'ma23': 'PV interneuron (Ma 2023)', 'mu26': 'PVN neuron (Mu 2026)',
}
# A single model's row names its method in brackets, short: the figure's caption spells them out.
METHOD_SHORT = {'euler': 'Euler', 'runge_kutta': 'RK4', 'backward_euler': 'backward Euler', 'qualrk': 'QualRK', 'stiff': 'stiff',
                'dopri8': 'Dormand-Prince 8', 'discrete': 'map'}
SINGLE_STUDIES = ('screen', 'modeldb', 'research')  # one model per row: each model runs at its own method
METHOD_TITLES = {
    'euler': 'Euler', 'runge_kutta': 'Runge-Kutta 4', 'backward_euler': 'Backward Euler', 'qualrk': 'Adaptive Runge-Kutta (QualRK)',
    'stiff': 'Stiff (XPPAUT\'s stiff solver)', 'dopri8': 'Dormand-Prince 8(5,3)', 'discrete': 'Discrete map',
    'step': 'First step (start-up)', 'T': 'Average step (run to T)', '2T': 'Run to 2T',
    'fd': 'Finite-difference Jacobian', 'exact': 'Exact Jacobian',
}
# Sampling a shared error grid is only for the visual median and quartiles.
HEATMAP_ROWS = 12  # Two text lines per cell keep ratio ranges legible on paper.
CONTINUATION_ROWS = 18  # Long branch names need more space beside ratio bars.
UNEQUAL_GROUPS = 10  # The frozen report's historical section-5 grouping.
GRID_POINTS = 120
MIN_MEDIAN_CURVES = 3  # A population band needs at least three observed curves.
STUDY_PROGRAMS = {s: ('xppaut',) for s in (*INTEGRATION, 'scaling')}
STUDY_PROGRAMS.update(python=('xppaut', 'python'), continuation=('xppaut', 'auto07p'))
PNG_DPI = 300  # a journal's minimum for line art at print size
AGREEMENT_COLORS = {'found': '#b8ddb0', 'different': '#e66101', 'traced': '#b2abd2', 'unavailable': '#d9d9d9'}
RATIO_LABEL = 'Other / xppautX wall time (above 1: xppautX faster)'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def interpolate(points, target):
    """Log-log piecewise interpolation in tolerance order; no extrapolation.

    Nonmonotone curves can cross the target more than once: retain every
    adjacent segment and take the fastest crossing. Exact rung hits compete
    with those crossings. Zero errors cannot be placed on a log axis.
    """
    if not target or target <= 0:
        return None
    candidates = [p['time'] for p in points if p['error'] == target and p['time'] > 0]
    for left, right in zip(points, points[1:]):
        x, y = left['error'], right['error']
        if min(x, y) <= 0 or x == y or not min(x, y) < target < max(x, y):
            continue
        if min(left['time'], right['time']) <= 0:
            continue
        weight = math.log(target / x) / math.log(y / x)
        candidates.append(math.exp((1 - weight) * math.log(left['time']) + weight * math.log(right['time'])))
    return min(candidates) if candidates else None


def profile(ratios, taus):
    """Failures (None) stay in the denominator; empty groups have no curve."""
    return [sum(r is not None and r <= t for r in ratios) / len(ratios) for t in taus] if ratios else []


def point(p):
    times = p['wall_seconds']
    return dict(error=p['max_abs_error'], time=p['wall_mean'], low=min(times), high=max(times),
                tolerance=p['tolerance'], repetitions=len(times), samples=times)


def method_of(study, workload, engine=''):
    if study == 'continuation':
        return 'exact' if '-exact-' in engine else 'fd'
    if study == 'python':
        return workload.split('-', 1)[1]
    if study == 'scaling':
        return workload.rsplit('-', 1)[1]
    raise ValueError('no method encoded by study/workload: ' + study + '/' + workload)


def summary_rows(folder, study):
    """Headline rows only, as the historical load_csv; sweeps have their own JSON."""
    runs = read(folder / 'runs.json')
    samples = defaultdict(list)
    for r in runs:
        if r['repetition'] > 0 and r.get('equal_work') and r.get('wall_seconds') is not None:
            samples[(r['workload'], r['engine'], r['row'])].append(r['wall_seconds'])
    out = []
    methods = {}
    for r in read(folder / 'summary.json'):
        if r.get('sweep') or r['row'] != 'run':
            continue
        times = samples[(r['workload'], r['engine'], r['row'])]
        if study in INTEGRATION and study != 'python':
            if r['workload'] not in methods:
                methods[r['workload']] = effective_numerics(folder / 'models' / (r['workload'] + '.odex')).method
            method = methods[r['workload']]
        else:
            method = method_of(study, r['workload'], r['engine'])
        out.append(dict(study=study, workload=r['workload'], method=method,
                        program=r['engine'].split('-')[0], engine=r['engine'], equal_work=r['equal_work'],
                        time=r['wall_seconds_mean'], sd=r['wall_seconds_sample_sd'],
                        low=min(times) if times else None, high=max(times) if times else None,
                        repetitions=r['counted_repetitions'], samples=times,
                        error=r.get('reference_max_abs_error'), converged_error_ratio=r.get('converged_error_ratio')))
    return out


def exceptions(folders):
    """The ten historical groups, derived from records (not a list of reasons)."""
    result = []

    def add(study, workloads, programs, reasons, sources):
        result.append(dict(study=study, workloads=workloads, programs=programs,
                           reason='; '.join(dict.fromkeys(reasons)), sources=sources))

    p = folders['python']
    for path in sorted((p / 'diverged').glob('*.json')):
        d = read(path)
        add('python', [d['workload']], list(d['last_log_line']),
            [d['note'], *[k + ': ' + v for k, v in d['last_log_line'].items()]], [str(path.relative_to(p))])
    for path in sorted((p / 'ladder').glob('*.json')):
        d = read(path)
        if d['never']:
            add('python', [d['workload']], d['never'], [d['note']], [str(path.relative_to(p))])
    paths = sorted((folders['scaling'] / 'models').glob('*/unavailable.json'))
    add('scaling', [p.parent.name for p in paths], ['xppaut'],
        [read(p)['xppaut'] for p in paths], [str(p.relative_to(folders['scaling'])) for p in paths])
    c = folders['continuation']
    paths = sorted(c.glob('*/*/pilot-xppaut-not-gated.json'))
    add('continuation', [str(p.parent.relative_to(c)) + '/' + r['name'] for p in paths
                         for r in read(p.parent / 'description.json')['runs']], ['xppaut'],
        [read(p)['reason'] for p in paths], [str(p.relative_to(c)) for p in paths])
    paths = sorted(c.glob('*/*/pilot-xppaut-unsupported.json'))
    add('continuation', [str(p.parent.relative_to(c)) + '/' + r['name'] for p in paths
                         for r in read(p.parent / 'description.json')['runs'] if r['kind'] == 'homoclinic'],
        ['xppaut'], [read(p)['reason'] for p in paths], [str(p.relative_to(c)) for p in paths])
    # Missing cycle-fold runs are visible in the saved reference; the tracked
    # driver owns and documents the refusal reason (the historical report did
    # the same, rather than pretending an omitted pilot had a failure message).
    paths = sorted(c.glob('ode_mu26_blocked/*/pilot-xppaut/reference.json'))
    omitted = []
    for path in paths:
        d = read(path.parent.parent / 'description.json')
        present = {r['name'] for r in read(path)['runs']}
        omitted += [str(path.parent.parent.relative_to(c)) + '/' + r['name'] for r in d['runs']
                    if r['name'].startswith('lpc_') and r['name'] not in present]
    reason = next(line.strip().strip('"') for line in (HERE / 'continuation.py').read_text().splitlines() if 'past 600 s each' in line)
    add('continuation', omitted, ['xppaut'], [reason],
        [str(p.relative_to(c)) for p in paths] + ['comparison/continuation.py: description_for'])
    skips = [(p, r) for p in sorted(c.glob('*/*/description.json')) for r in read(p)['runs'] if r.get('skip')]
    add('continuation', [str(p.parent.relative_to(c)) + '/' + r['name'] for p, r in skips],
        ['xppautx', 'xppaut'], [r['skip'] for _, r in skips], [str(p.relative_to(c)) for p, _ in skips])
    if len(result) != UNEQUAL_GROUPS or any(not e['workloads'] or not e['reason'] for e in result):
        raise ValueError('frozen series no longer matches the ten historical unequal-work groups')
    return result


def gate_cell(workload, method, program, gate, source, reason=''):
    """Project the comparison's saved judgment onto one run/reference.

    This reads gate output, not pilot labels. Distinct measured labels count
    matched special points; repeated quantities at a point do not add points.
    """
    run = workload.rsplit('/', 1)[1]
    reference = continuation.reference.SUFFIX[program]
    cell = dict(workload=workload, method=method, program=program, reference=reference,
                gate_source=source, gate_passed=gate['passed'] if gate else None,
                found=0, measures=[], differences=[], traced=[], reason=reason, status='unavailable')
    if reason or gate is None:
        return cell
    unavailable = [u for u in gate['unavailable'] if u['program'] == reference and u['run'] == run]
    if unavailable:
        cell['reason'] = '; '.join(u['reason'] for u in unavailable)
        return cell
    cell['measures'] = [m for m in gate['measures'] if m[0] == reference and m[1] == run]
    cell['found'] = len({m[3] for m in cell['measures'] if not m[2].startswith('curve ')})
    # Exact-pilot nodes judge one program against the converged run; their
    # traced entries deliberately use .converged, even for AUTO-07p's node.
    cell['traced'] = [t for t in gate['traced_differences'] if t['run'] == run
                      and (method == 'exact' or t['reference'] == reference)]
    prefixes = [workload.split('/')[0] + ' ' + label + '/' + run
                for label in (continuation.comparison.REFERENCES[reference], reference)]
    cell['differences'] = [text for text in gate['differences']
                           if any(re.search(re.escape(prefix) + r'(?=[:\s]|$)', text) for prefix in prefixes)]
    if cell['differences']:
        cell['status'] = 'different'
    elif not gate['passed']:
        # A failed gate cannot certify an unaffected cell. Preserve the whole
        # gate's own words rather than inventing a per-label failure.
        cell['reason'] = 'Gate failed outside this run/reference: ' + '; '.join(gate['differences'])
    else:
        cell['status'] = 'traced' if cell['traced'] else 'found'
    return cell


def agreement(folder):
    """Saved gates are the authority; absent FD gates use the owner's pipeline.

    Replaying continuation.gate calls autocompare.compare_references with its
    clipping, ignored types and traced differences. It starts no program and
    writes nothing into the frozen data.
    """
    cells, gates = [], {}
    for path in sorted(folder.glob('*/*/description.json')):
        d, base = read(path), path.parent
        name = base.parent.name
        prefix = str(base.relative_to(folder))
        native = base / 'pilot-xppautx'
        if not (native / 'diagram.csv').exists():
            continue
        fd_sources = {}
        for tag in ('both', 'xppaut'):
            p = base / f'pilot-gate-{tag}.json'
            if p.exists():
                source = str(p.relative_to(folder)) + '#result'
                gates[source] = read(p)['result']
                fd_sources[tag] = source
        if 'both' not in fd_sources:
            refs = {}
            for program in ('xppaut', 'auto07p'):
                p = base / ('pilot-' + program) / 'reference.json'
                if p.exists() and program + '_skip' not in d and not (base / f'pilot-{program}-unsupported.json').exists():
                    refs['.' + program] = continuation.saved_reference(p.parent, program, continuation.given(d, program))
            source = prefix + '/autocompare-replay'
            gates[source] = continuation.gate(name, d, dict(data=(native / 'diagram.csv').read_bytes(),
                                                            ranges=read(native / 'run-ranges.json')), refs)
            fd_sources['both'] = source
        converged_path = base / 'converged.json'
        exact = read(converged_path).get('exact_pilots', {}) if converged_path.exists() else {}
        for program in ('xppaut', 'auto07p'):
            for method in ('fd', 'exact'):
                own = base / ('pilot-xppautx-exact' if method == 'exact' else 'pilot-xppautx')
                if not (own / 'diagram.csv').exists():
                    continue
                source = fd_sources.get('xppaut', fd_sources['both']) if program == 'xppaut' else fd_sources['both']
                gate = gates[source]
                reference_path = base / ('pilot-' + program + ('-exact' if method == 'exact' else '')) / 'reference.json'
                available = {r['name']: r for r in read(reference_path)['runs']} if reference_path.exists() else {}
                reason = ''
                availability_source = ''
                if method == 'exact':
                    source = str(converged_path.relative_to(folder)) + '#exact_pilots/' + program + '-exact'
                    gate = exact.get(program + '-exact')
                    if gate is not None:
                        gates[source] = gate
                    else:
                        source = None
                        reason = 'No exact pilot gate for this program' if program != 'xppaut' else 'XPPAUT has no exact-Jacobian option'
                for suffix in ('not-gated', 'unsupported'):
                    p = base / f'pilot-{program}-{suffix}.json'
                    if p.exists():
                        reason = read(p)['reason']
                        availability_source = str(p.relative_to(folder))
                if program + '_skip' in d:
                    reason = d[program + '_skip']
                    availability_source = str(path.relative_to(folder)) + '#' + program + '_skip'
                for run in d['runs']:
                    why = reason or run.get('skip', '')
                    if not why and gate is not None:
                        missing = next((u['reason'] for u in gate['unavailable'] if u['program'] == '.' + program and u['run'] == run['name']), '')
                        why = missing or ('No saved reference pilot for this run' if run['name'] not in available else available[run['name']].get('unavailable', ''))
                    cell = gate_cell(prefix + '/' + run['name'], method, program, gate, source, why)
                    if why:
                        cell['availability_source'] = availability_source or str(path.relative_to(folder)) + '#runs/' + run['name']
                    cells.append(cell)
    return cells, gates


def compute(final):
    status = read(final / 'status.json')
    folders = {s: Path(v['out']) for s, v in status['studies'].items()}
    unequal = exceptions(folders)
    rows = [r for s, p in folders.items() if s not in ('terman', 'large-stiff') for r in summary_rows(p, s)]
    groups = defaultdict(list)
    for r in rows:
        groups[(r['study'], r['workload'], r['method'])].append(r)
    # Failed ladders have no headline summary; include every excluded workload.
    for e in unequal:
        for w in e['workloads']:
            methods = ('fd',) if e['study'] == 'continuation' else (method_of(e['study'], w),)
            for m in methods:
                groups.setdefault((e['study'], w, m), [])
    labels, gates = agreement(folders['continuation'])
    for cell in labels:
        groups.setdefault(('continuation', cell['workload'], cell['method']), [])
    label_keys = {(c['workload'], c['method'], c['program']) for c in labels}
    for s, w, m in groups:
        if s == 'continuation':
            for p in ('xppaut', 'auto07p'):
                if (w, m, p) not in label_keys:
                    labels.append(dict(workload=w, method=m, program=p, found=0, traced=[], differences=[], measures=[],
                                       gate_source=None, gate_passed=None, status='unavailable', reason='No saved pilot gate for this Jacobian group'))
    overview = []
    for (s, w, m), candidates in groups.items():
        cells = {}
        for prog in NAMES:
            possible = [r for r in candidates if r['program'] == prog and r['equal_work'] and r['time'] is not None]
            selected = min(possible, key=lambda r: r['time']) if possible else None
            reasons = [e['reason'] for e in unequal if e['study'] == s and w in e['workloads'] and prog in e['programs']]
            cells[prog] = dict(row=selected, reason='; '.join(reasons) or ('not measured in this study/group' if selected is None else ''))
            if not candidates and s == 'continuation' and not reasons:
                cells[prog]['reason'] = 'pilot-only setup or inherited description; no headline timing row'
        base = cells['xppautx']['row']
        for cell in cells.values():
            r = cell['row']
            valid = base and r and not cell['reason']
            cell.update(ratio=r['time'] / base['time'] if valid else None,
                        low=r['low'] / base['high'] if valid and r['low'] is not None else None,
                        high=r['high'] / base['low'] if valid and r['high'] is not None else None)
        overview.append(dict(study=s, workload=w, model=(w.split('/')[0] if s == 'continuation' else re.match(r'[a-z]+', w)[0] if s == 'scaling' else w.split('-')[0]), method=m, cells=cells))
    matched = equal_work_matches(overview)
    curves = []
    for study in INTEGRATION:
        for path in sorted((folders[study] / 'work-precision').glob('*.json')):
            d = read(path)
            baseline = next(r for r in overview if r['study'] == study and r['workload'] == d['workload'])['cells']['xppautx']['row']
            target, base = baseline['error'], baseline['time']
            for program in ('xppautx', *STUDY_PROGRAMS[study]):
                pts = sorted([point(p) for p in d['points'] if p['engine'] == program and p['wall_mean'] is not None], key=lambda p: -p['tolerance'])
                if pts:
                    curves.append(dict(study=study, workload=d['workload'], model=d['workload'].split('-')[0],
                                       method=method_of(study, d['workload']), program=program, points=pts,
                                       target_error=target, baseline_time=base,
                                       normalized=[dict(error=p['error'] / target, time=p['time'] / base,
                                                        low=p['low'] / baseline['high'], high=p['high'] / baseline['low']) for p in pts]
                                       if target and target > 0 else []))
    large = []
    with (folders['large-stiff'] / 'results/summary.csv').open(newline='') as stream:
        for r in csv.DictReader(stream):
            large.append({k: (v if k in ('family', 'solver') else float(v)) for k, v in r.items()})
    # Audit includes ready rows and excluded/untimed samples, without rendering
    # pilot/warm-up wall times as performance measurements.
    audit = {s: read(p / 'summary.json') for s, p in folders.items() if (p / 'summary.json').exists()}
    terman = {name: read(folders['terman'] / (name + '.json')) for name in ('acceptance', 'convergence', 'saved-readouts')}
    inventory = [dict(study=r['study'], workload=r['workload'], model=r['model'], method=r['method']) for r in overview]
    inventory += [dict(study='large-stiff', workload=r['family'] + str(int(r['n'])), model=r['family'], method=r['solver']) for r in large]
    inventory.append(dict(study='terman', workload='episodic', model='episodic', method='acceptance'))
    return dict(screen_tolerance=dict(atol=screen.ATOL, rtol=screen.RTOL, timing_fraction=screen.TIMING_FRACTION),
                status=status, inventory=inventory, overview=overview, rows=rows, matched=matched, curves=curves, unequal=unequal,
                agreement=labels, large_stiff=large, audit=audit, terman=terman,
                gates=gates, scaling_cost=read(folders['scaling'] / 'cost-per-time.json'))


def equal_work_matches(overview):
    """The equal-work gate defines matched accuracy; timed means are authoritative.

    Include missing and rejected rows in the study's program denominator.
    A timed sweep is diagnostic data, never a different profile time.
    """
    result = []
    for r in overview:
        if r['study'] not in (*INTEGRATION, 'scaling'):
            continue
        baseline = r['cells']['xppautx']['row']
        for program in STUDY_PROGRAMS[r['study']]:
            cell = r['cells'][program]
            other = cell['row']
            reaches = bool(baseline and other and baseline['equal_work'] and other['equal_work']
                           and baseline['time'] is not None and other['time'] is not None and not cell['reason'])
            result.append(dict(study=r['study'], workload=r['workload'], model=r['model'], method=r['method'],
                               program=program, baseline_time=baseline['time'] if baseline else None,
                               time=other['time'] if reaches else None,
                               ratio=other['time'] / baseline['time'] if reaches else None,
                               status='matched by equal-work gate' if reaches else 'does not reach',
                               reason='' if reaches else cell['reason'] or 'xppautX did not reach the reference'))
    return result


def coverage(data):
    matched = data['matched']
    by_study = {s: {p: dict(total=sum(r['study'] == s and r['program'] == p for r in matched),
                            matched=sum(r['study'] == s and r['program'] == p and r['ratio'] is not None for r in matched))
                    for p in STUDY_PROGRAMS[s]} for s in (*INTEGRATION, 'scaling')}
    return dict(inventory=dict(Counter(r['study'] for r in data['inventory'])),
                overview=dict(Counter(r['study'] for r in data['overview'])),
                matched_workloads=len({(r['study'], r['workload']) for r in matched}),
                profile_exclusions=[dict(study='terman', workload='episodic', method='acceptance', reason='untimed acceptance/convergence evidence; no timed rows')],
                profile_by_study=by_study,
                matched_by_program={p: dict(total=sum(r['program'] == p for r in matched),
                                           matched=sum(r['program'] == p and r['ratio'] is not None for r in matched))
                                    for p in ('xppaut', 'python')},
                sweep_workloads=len({c['workload'] for c in data['curves']}),
                resolved_sweep_workloads=len({c['workload'] for c in data['curves'] if len({p['error'] for p in c['points']}) > 1}),
                continuation_cells=len(data['agreement']), overlays=len(data['overlays']['figures']),
                unequal_groups=len(data['unequal']), large_stiff_rows=len(data['large_stiff']))


def line(name, x, y, program, low=None, high=None, **kwargs):
    return dict(name=name, x=list(x), y=list(y), program=program,
                low=list(low) if low is not None else None, high=list(high) if high is not None else None, **kwargs)


def ratio_finding(records):
    program = 'xppaut' if any(r['program'] == 'xppaut' for r in records) else next((r['program'] for r in records), 'python')
    ratios = [r['ratio'] for r in records if r['program'] == program and r['ratio'] is not None]
    if not ratios:
        return 'No completed equal-work pairs in this selection'
    return f'{NAMES[program]} is slower on {100 * sum(v > 1 for v in ratios) / len(ratios):.0f}% of completed pairs'


def profile_spec(records, key):
    values = [r['ratio'] for r in records if r['ratio'] is not None]
    taus = sorted(set([min([1.] + values) / 2, 1., max([1.] + values) * 2] + values))
    traces = []
    for p in ('xppaut', 'python'):
        rows = [r for r in records if r['program'] == p]
        if rows:
            n = sum(r['ratio'] is not None for r in rows)
            traces.append(line(f'{NAMES[p]} ({n} of {len(rows)} reach)', taus,
                               profile([r['ratio'] for r in rows], taus), p, step=True))
    scope = ''
    if key in STUDY_PROGRAMS:
        scope = key
    elif key.startswith(('profile-study-', 'profile-method-')):
        scope = key[len('profile-'):].replace('-', ' ')
    return dict(key=key, title=(scope + ': ' if scope else '') + ratio_finding(records), traces=traces, xlabel='Other / xppautX wall time',
                ylabel='Fraction of workloads', xlog=True, ylog=False, reference=True)


def normalized_view(curves, program):
    traces = []
    for c in curves:
        pts = c['normalized']
        traces.append(line(c['workload'], [p['error'] for p in pts], [p['time'] for p in pts], program,
                           [p['low'] for p in pts], [p['high'] for p in pts], faint=True))
    positive = [p['error'] for c in curves for p in c['normalized'] if p['error'] > 0]
    grid = sorted(set(np.geomspace(min(positive), max(positive), GRID_POINTS).tolist() + positive)) if positive else []
    population = [[interpolate(c['normalized'], x) for x in grid] for c in curves]
    median, low, high, counts = [], [], [], []
    for column in zip(*population):
        available = [v for v in column if v is not None]
        counts.append(len(available))
        q = np.quantile(available, [.25, .5, .75]).tolist() if len(available) >= MIN_MEDIAN_CURVES else [None] * 3
        low.append(q[0]); median.append(q[1]); high.append(q[2])
    # Split support into segments once. Neither renderer can join a gap or
    # extend a quantile beyond the grid points supported by three curves.
    segments = []
    for i, value in enumerate(median):
        if value is not None:
            if not segments or segments[-1][-1] != i - 1:
                segments.append([])
            segments[-1].append(i)
    for index, segment in enumerate(segments):
        traces.append(line('Median and 25–75% (at least three curves)', [grid[i] for i in segment],
                           [median[i] for i in segment], program, [low[i] for i in segment], [high[i] for i in segment],
                           population=True, legend=index == 0))
    resolved = sum(len({p['error'] for p in c['points']}) > 1 for c in curves)
    return dict(key='normalized-' + program, title=f'{NAMES[program]}: only {resolved} of {len(curves)} sweeps resolve changing error',
                traces=traces, xlabel='Error / xppautX matched error', ylabel='Time / xppautX matched time', xlog=True, ylog=True,
                population=dict(grid=grid, median=median, low=low, high=high, contributors=counts))


def agreement_text(cell):
    if cell['status'] == 'different':
        return f"{len(cell['differences'])} gate differences"
    if cell['status'] == 'traced':
        return f"{len(cell['traced'])} traced"
    return ''


def overview_forest(overview):
    """The paper's overview (W590): a panel of single models, each at its own method, then a panel per study of groups by
    method; only comparable workloads count, so a group's number is how many it has. Size scaling shows its first step
    (start-up) and its run to T; the run to 2T, only there for the 2T - T integration cost, is the scaling figure's."""
    single = [dict(label=f"{MODEL_TITLES[r['workload']]} ({METHOD_SHORT[r['method']]})",
                   cells={'xppaut': dict(ratio=r['cells']['xppaut']['ratio'])})
              for s in SINGLE_STUDIES for r in overview if r['study'] == s and r['cells']['xppaut']['ratio'] is not None]
    panels = [dict(key='single', programs=['xppaut'], rows=single)]
    for study in ('izhikevich', 'python', 'continuation', 'scaling'):
        summary = heat_summary([r for r in overview if r['study'] == study], study)
        rows = [dict(label=METHOD_TITLES[r['method']], cells={q: c for q, c in r['cells'].items() if c['count']})
                for r in summary['rows'] if any(c['count'] for c in r['cells'].values())
                and (study != 'scaling' or r['method'] != '2T')]
        panels.append(dict(key=study, programs=list(STUDY_PROGRAMS[study]), rows=rows))
    return panels


LARGE_STIFF_SOLVERS = {'legacy-dense': 'Dense (CVODE)', 'legacy-band': 'Band (CVODE)', 'klu': 'Sparse (KLU)',
                       'gmres': 'GMRES(30), band-preconditioned'}


def large_stiff_text(rows):
    """W378's finding for the paper's text (W590: no figure): each solver's time and memory at the largest size."""
    n = max(r['n'] for r in rows)
    at = {(r['family'], r['solver']): r for r in rows if r['n'] == n}
    table = ['| Solver | Cable time (s) | Cable memory (MiB) | Ring time (s) | Ring memory (MiB) |', '|---|---|---|---|---|']
    for solver, title in LARGE_STIFF_SOLVERS.items():
        cells = [f"{at[(f, solver)]['wall_mean']:.2f} | {at[(f, solver)]['peak_rss_kib_mean'] / 1024:.0f}" for f in ('cable', 'ring')]
        table.append(f'| {title} | ' + ' | '.join(cells) + ' |')
    return ('## Large stiff networks (in the text, no figure)\n\nW378 asked whether xppautX needs a sparse or Krylov linear '
            f'solver for large stiff networks: a diffusion cable and a ring of Morris-Lecar cells, N = {min(r["n"] for r in rows):.0f} '
            f'to {n:.0f} equations, all four solvers on the same right-hand side. At N = {n:.0f} the band solver xppautX '
            'already has matches KLU and GMRES; only the dense solver falls behind, in time and memory:\n\n' + '\n'.join(table) + '\n')


SCALING_FAMILIES = {'kuramoto': 'Kuramoto', 'lecdiff': 'Morris-Lecar chain', 'amari': 'Amari'}


def scaling_sentence(scaling_cost):
    """How the integration ratio, (2T - T) / T for XPPAUT over xppautX's, moves with N in each model, and how xppautX's
    own cost grows past XPPAUT's limit."""
    cost = defaultdict(dict)
    for c in scaling_cost:
        cost[c['workload']][c['engine']] = c['wall_per_model_time']
    parts, beyond = [], []
    for family, title in SCALING_FAMILIES.items():
        sizes = sorted((int(re.search(r'\d+', w)[0]), c) for w, c in cost.items() if w.startswith(family))
        both = [(n, c['xppaut'] / c['xppautx']) for n, c in sizes if 'xppaut' in c]
        parts.append(f'{title} {both[0][1]:.2f} at N = {both[0][0]} and {both[-1][1]:.2f} at N = {both[-1][0]} '
                     f'(all sizes {min(r for _, r in both):.2f}-{max(r for _, r in both):.2f})')
        alone = [(n, c['xppautx']) for n, c in sizes if 'xppaut' not in c]
        if len(alone) > 1:
            beyond.append(f'{alone[-1][1] / alone[0][1]:.1f} times ({title}, N = {alone[0][0]} to {alone[-1][0]})')
    return ('Integration ratio per model ((2T - T) / T, XPPAUT\'s over xppautX\'s), smallest and largest size both ran: '
            + '; '.join(parts) + '. Past XPPAUT\'s limit, xppautX\'s cost per unit of model time grows in step with N: '
            + ' and '.join(beyond) + '.')


def overview_caption(panels, matched, scaling_cost):
    single, groups = panels[0], panels[1:]
    # Workloads both programs finished, each once (model size by its run to T), and the sizes XPPAUT cannot run at all.
    xppaut = [r for r in matched if r['program'] == 'xppaut' and r['baseline_time'] is not None
              and (r['study'] != 'scaling' or r['method'] == 'T')]
    both = [r for r in xppaut if r['ratio'] is not None]
    lost = [r for r in both if r['ratio'] <= 1]
    studies = sorted({STUDY_TITLES[r['study']].split(' ')[0] for r in lost})
    unfinished = len(xppaut) - len(both)
    count = {p['key']: sum(c['count'] for r in p['rows'] for c in r['cells'].values()) for p in groups}
    return (f'Each program\'s time divided by xppautX\'s for the same work at matched accuracy, on a log scale: above 1, '
            f'xppautX is faster. The left panel holds the {len(single["rows"])} published and demo models, one workload each '
            'at its own method (in brackets; QualRK is the adaptive Runge-Kutta method). Each other panel is a study, grouped '
            'by method: the dot is the median ratio, the bar the middle half of the workloads (25th to 75th percentile) and '
            'the number how many workloads the group compares. Only workloads both programs completed at matched accuracy '
            'are shown; data/coverage.json lists the rest and why. Izhikevich: the 28 firing behaviours of Izhikevich 2003 '
            'and 2004. Python: a Python re-implementation of xppautX\'s methods on the screen models. Continuation: AUTO '
            'branch runs, grouped by how the Jacobian is formed. Model size: Amari, Kuramoto and Morris-Lecar chains at '
            f'N = 10 to 6400 variables ({count["scaling"] // 2} sizes), step 0.05. The first step is a run of one step, '
            'start-up alone (loading, parsing, setting up, writing output); the average step is a whole run to T divided '
            'by its steps, start-up included. T is chosen per model and size so XPPAUT\'s run to T integrates for about '
            '1.5 s, since the cost of a time unit grows steeply with N; both programs run the same T, so it cancels in the '
            f'ratio. Of the {len(both)} workloads both XPPAUT 8.0 and xppautX finished, XPPAUT was at least as fast on '
            f'{len(lost)}' + (f', all {studies[0]} models' if len(studies) == 1 else '') + f'; it could not run '
            f'{unfinished} model sizes, past its limit of 5000 equations. ' + scaling_sentence(scaling_cost))


def heat_summary(rows, study):
    methods = list(dict.fromkeys(r['method'] for r in rows))
    summary = []
    for method in methods:
        cells = {}
        for program in STUDY_PROGRAMS[study]:
            group = [r for r in rows if r['method'] == method]
            values = [r['cells'][program]['ratio'] for r in group if r['cells'][program]['ratio'] is not None]
            cells[program] = dict(ratio=float(np.median(values)) if values else None, count=len(values), total=len(group),
                                  minimum=min(values) if values else None, maximum=max(values) if values else None,
                                  q1=float(np.percentile(values, 25)) if values else None, q3=float(np.percentile(values, 75)) if values else None)
        summary.append(dict(workload=method, method=method, cells=cells))
    values = [c['ratio'] for r in summary for c in r['cells'].values() if c['ratio'] is not None]
    title = f'{study}: medians span {min(values):.2g}–{max(values):.2g}×' if values else f'{study}: no comparable timings'
    return dict(key='summary-' + study, kind='heatmap', title=title, study=study,
                rows=summary, programs=list(STUDY_PROGRAMS[study]), summary=True)


def scaling_title(rows, family, kind):
    pairs = defaultdict(dict)
    for r in rows:
        pairs[r['workload']][r['program']] = r
    ratios = [v['xppaut']['time'] / v['xppautx']['time'] for v in pairs.values() if 'xppaut' in v and 'xppautx' in v]
    sizes = [int(re.search(r'\d+', w)[0]) for w, v in pairs.items() if 'xppaut' in v and 'xppautx' in v]
    lo, hi = min(ratios), max(ratios)
    result = f'xppautX stays {lo:.1f}–{hi:.1f}× faster' if lo > 1 else f'XPPAUT/xppautX spans {lo:.1f}–{hi:.1f}×'
    return f'{family}, {kind}: {result}\nat shared sizes through N = {max(sizes)}'


def continuation_finding(records):
    findings = []
    for program in ('xppaut', 'auto07p'):
        ratios = [r['ratio'] for r in records if r['program'] == program and r['ratio'] is not None]
        if ratios:
            n = sum(r > 1 for r in ratios)
            findings.append(f"{NAMES[program]} on every paired run ({len(ratios)})" if n == len(ratios)
                            else f"{NAMES[program]} on {n} of {len(ratios)} paired runs")
    return 'xppautX is faster than ' + ' and than '.join(findings)


def continuation_summaries(data):
    # Aggregate gate judgments by model and Jacobian/program, preserving raw branch
    # counts in the detailed appendix. Times include headline branches only.
    columns = [('xppaut', 'fd'), ('auto07p', 'fd'), ('auto07p', 'exact')]
    models = sorted({r['workload'].split('/')[0] for r in data['agreement']})
    labels, timings = [], []
    for model in models:
        for p, m in columns:
            cc = [c for c in data['agreement'] if c['workload'].split('/')[0] == model and c['program'] == p and c['method'] == m]
            available = [c for c in cc if c['status'] != 'unavailable']
            status = ('unavailable' if not available else 'different' if any(c['status'] == 'different' for c in available)
                      else 'traced' if any(c['status'] == 'traced' for c in available) else 'found')
            labels.append(dict(workload=model, program=p, method=m, found=sum(c['found'] for c in available),
                               differences=[x for c in available for x in c['differences']],
                               traced=[x for c in available for x in c['traced']],
                               status=status, compared=len(available), total=len(cc)))
            candidates = [r for r in data['overview'] if r['study'] == 'continuation' and r['method'] == m and r['model'] == model
                          and any(x['row'] is not None for x in r['cells'].values())]
            cells = [r['cells'][p] for r in candidates if r['cells'][p]['ratio'] is not None]
            timings.append(dict(model=model, program=p, method=m, total=len(candidates), count=len(cells),
                                ratio=float(np.median([c['ratio'] for c in cells])) if cells else None,
                                low=min(c['low'] for c in cells) if cells else None,
                                high=max(c['high'] for c in cells) if cells else None))
    differences = sum(c['status'] == 'different' for c in labels)
    total = sum(c['status'] != 'unavailable' for c in labels)
    matrix = dict(key='continuation-agreement', kind='agreement', cells=labels, columns=columns,
                  title=f'{total - differences} of {total} available model/program groups pass their gates', aggregated=True)
    pairs = [dict(program=p, ratio=r['cells'][p]['ratio']) for r in data['overview'] if r['study'] == 'continuation'
             for p in ('xppaut', 'auto07p') if r['cells'][p]['ratio'] is not None]
    time = dict(key='continuation-time', kind='ratio_summary', records=timings,
                title=continuation_finding(pairs), xlabel='Other / xppautX wall time', ylabel='Model (branches aggregated)')
    return matrix, time


def views(data):
    """All scientific values are computed here, shared by HTML and matplotlib."""
    charts = []
    for study in dict.fromkeys(r['study'] for r in data['overview']):
        rows = [r for r in data['overview'] if r['study'] == study]
        for page, start in enumerate(range(0, len(rows), HEATMAP_ROWS)):
            charts.append(dict(key=f'overview-{study}-{page + 1}', kind='heatmap', study=study,
                               title=heat_summary(rows, study)['title'], rows=rows[start:start + HEATMAP_ROWS],
                               programs=list(STUDY_PROGRAMS[study])))
    # Cache every selectable study/model/method intersection by its workload
    # signature: filtering selects precomputed profiles, never recomputes CDFs.
    matched = data['matched']
    partitions = [('all', matched)]
    partitions += [('study-' + s, [r for r in matched if r['study'] == s]) for s in (*INTEGRATION, 'scaling')]
    partitions += [('method-' + m, [r for r in matched if r['method'] == m]) for m in sorted({r['method'] for r in matched})]
    data['filter_profiles'] = {}
    from itertools import product
    for study, model, method in product([None, *sorted({r['study'] for r in matched})],
                                        [None, *sorted({r['model'] for r in matched})],
                                        [None, *sorted({r['method'] for r in matched})]):
        subset = [r for r in matched if (study is None or r['study'] == study) and (model is None or r['model'] == model)
                  and (method is None or r['method'] == method)]
        signature = '|'.join(sorted({r['study'] + '/' + r['workload'] for r in subset}))
        if subset and signature not in data['filter_profiles']:
            data['filter_profiles'][signature] = profile_spec(subset, 'filtered-profile')
    for key, subset in partitions:
        if subset:
            charts.append(profile_spec(subset, 'profile-' + key))
    for p in ('xppautx', 'xppaut', 'python'):
        curves = [c for c in data['curves'] if c['program'] == p]
        if curves:
            charts.append(normalized_view(curves, p))
    for family in ('kuramoto', 'lecdiff', 'amari'):
        for kind in ('step', 'T', '2T'):
            rows = [r for r in data['rows'] if r['study'] == 'scaling' and r['workload'].startswith(family)
                    and r['method'] == kind and r['time'] is not None and r['equal_work']]
            traces = []
            for p in ('xppautx', 'xppaut'):
                rr = sorted([r for r in rows if r['program'] == p], key=lambda r: int(re.search(r'\d+', r['workload'])[0]))
                traces.append(line(NAMES[p] + ' (1 thread)', [int(re.search(r'\d+', r['workload'])[0]) for r in rr],
                                   [r['time'] for r in rr], p, [r['low'] for r in rr], [r['high'] for r in rr]))
            charts.append(dict(key=f'scaling-{family}-{kind}', title=scaling_title(rows, family, kind), traces=traces,
                               xlabel='N (model size)', ylabel='Wall time (s)', xlog=True, ylog=True))
    for family in ('cable', 'ring'):
        for metric, unit in [('wall', 'Wall time (s)'), ('peak_rss_kib', 'Peak memory (KiB)')]:
            traces = []
            for style, solver in enumerate(sorted({r['solver'] for r in data['large_stiff']})):
                rr = sorted([r for r in data['large_stiff'] if r['family'] == family and r['solver'] == solver], key=lambda r: r['n'])
                y = [r[metric + '_mean'] for r in rr]
                sd = [r[metric + '_sample_sd'] for r in rr]
                traces.append(line(solver, [r['n'] for r in rr], y, 'xppautx',
                                   [max(a - b, np.finfo(float).tiny) for a, b in zip(y, sd)], [a + b for a, b in zip(y, sd)], style=style))
            dense = next(t for t in traces if t['name'] == 'legacy-dense')
            charts.append(dict(key=f'large-stiff-{family}-{metric}', title=f'{family}: dense reaches {dense["y"][-1] if metric == "wall" else dense["y"][-1] / 1024:.3g} {"s" if metric == "wall" else "MiB"}',
                               traces=traces, xlabel='N (compartments)', ylabel=unit, xlog=True, ylog=True))
    for method in ('fd', 'exact'):
        rows = [r for r in data['overview'] if r['study'] == 'continuation' and r['method'] == method]
        for page, start in enumerate(range(0, len(rows), CONTINUATION_ROWS)):
            subset = rows[start:start + CONTINUATION_ROWS]
            traces = []
            for p in ('xppaut', 'auto07p'):
                if p == 'xppaut' and method == 'exact':
                    continue
                cells = [r['cells'][p] for r in subset]
                traces.append(line(NAMES[p], [r['workload'] for r in subset], [c['ratio'] for c in cells], p,
                                   [c['low'] for c in cells], [c['high'] for c in cells], bars=True))
            pairs = [dict(program=p, ratio=r['cells'][p]['ratio']) for r in subset for p in ('xppaut', 'auto07p') if r['cells'][p]['ratio'] is not None]
            charts.append(dict(key=f'continuation-time-{method}-{page + 1}', title=continuation_finding(pairs) if pairs else 'Pilot evidence has no headline timing',
                               traces=traces, xlabel='Workload', ylabel=RATIO_LABEL, xlog=False, ylog=True))
            cells = [c for c in data['agreement'] if c['method'] == method and c['workload'] in {r['workload'] for r in subset} and c['program'] != 'xppautx'
                     and not (method == 'exact' and c['program'] == 'xppaut')]
            n = sum(c['status'] in ('found', 'traced') for c in cells); total = sum(c['status'] != 'unavailable' for c in cells)
            charts.append(dict(key=f'continuation-labels-{method}-{page + 1}', kind='agreement', cells=cells,
                               title=f'{n} of {total} available branch/program gates pass',
                               columns=[(p, method) for p in ('xppaut', 'auto07p') if method == 'fd' or p != 'xppaut']))
    for c in charts:
        c['set'] = 'appendix'
    def panel(key, title, children, ncols, height):
        return dict(key=key, kind='panels', title=title, panels=children, ncols=ncols, width=7., height=height, set='paper')
    studies = list(dict.fromkeys(r['study'] for r in data['overview']))
    overview_panels = [heat_summary([r for r in data['overview'] if r['study'] == s], s) for s in studies]
    medians = [c['ratio'] for panel in overview_panels for r in panel['rows'] for c in r['cells'].values() if c['ratio'] is not None]
    forest = overview_forest(data['overview'])
    charts.append(dict(key='overview', kind='forest', set='paper', width=7., panels=forest, caption=overview_caption(forest, matched, data['scaling_cost']),
                       title=f'xppautX is faster in {sum(v > 1 for v in medians)} of {len(medians)} study and method groups'))
    matrix, time = continuation_summaries(data)
    charts += [matrix | dict(set='paper', width=7., height=9.5), time | dict(set='paper', width=7., height=8.)]
    data['ratio_limit'] = max(abs(math.log10(c['ratio'])) for r in data['overview'] for p, c in r['cells'].items() if p != 'xppautx' and c['ratio'])
    data['agreement_colors'] = AGREEMENT_COLORS
    data['findings'] = {s: heat_summary([r for r in data['overview'] if r['study'] == s], s)['title'] for s in studies}
    for view in charts:
        if view.get('kind') == 'agreement':
            for cell in view['cells']:
                cell.update(state=cell['status'], text=agreement_text(cell))
    return charts


def static(charts, out, ratio_limit):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle, Patch
    from matplotlib.colors import TwoSlopeNorm, LinearSegmentedColormap
    from matplotlib.cm import ScalarMappable
    from matplotlib.ticker import LogLocator, FuncFormatter, NullFormatter, MaxNLocator
    from matplotlib.lines import Line2D
    from matplotlib.transforms import blended_transform_factory
    from textwrap import fill
    plt.rcParams.update({'font.size': 7, 'axes.titlesize': 8, 'axes.labelsize': 7, 'xtick.labelsize': 7, 'ytick.labelsize': 7,
                         'legend.fontsize': 7, 'axes.spines.top': False, 'axes.spines.right': False, 'pdf.fonttype': 42, 'svg.fonttype': 'none'})
    cmap = LinearSegmentedColormap.from_list('ratio', ['#b2182b', '#f7f7f7', '#2166ac'])
    styles = ('-', '--', ':', '-.')
    def log_ticks(axis):
        limits = axis.axes.get_xlim() if axis is axis.axes.xaxis else axis.axes.get_ylim()
        lo, hi = sorted(limits)
        candidates = [factor * 10. ** exponent for exponent in range(math.floor(math.log10(lo)), math.ceil(math.log10(hi)) + 1)
                      for factor in (1, 2, 5) if lo <= factor * 10. ** exponent <= hi]
        if len(candidates) < 3:
            candidates = [x for x in MaxNLocator(nbins=4).tick_values(lo, hi) if lo <= x <= hi]
        if len(candidates) > 6:
            candidates = [candidates[i] for i in np.linspace(0, len(candidates) - 1, 6).round().astype(int)]
        axis.set_ticks(candidates)
        axis.set_major_formatter(FuncFormatter(lambda x, _: f'{x:g}'))
        axis.set_minor_locator(LogLocator(base=10, subs=(1, 2, 3, 4, 5, 6, 7, 8, 9), numticks=60))
        axis.set_minor_formatter(NullFormatter())
    def draw(ax, view):
        kind = view.get('kind')
        if kind == 'heatmap':
            rows, programs = view['rows'], view['programs']
            z = np.array([[math.log10(r['cells'][p]['ratio']) if r['cells'][p]['ratio'] else np.nan for p in programs] for r in rows])
            ax.imshow(z, aspect='auto', cmap=cmap, norm=TwoSlopeNorm(0, -ratio_limit, ratio_limit))
            for i, r in enumerate(rows):
                for j, p in enumerate(programs):
                    c = r['cells'][p]
                    if c['ratio'] is None:
                        ax.add_patch(Rectangle((j - .5, i - .5), 1, 1, facecolor='#ddd', hatch='///', edgecolor='#888', linewidth=0))
                    elif view.get('summary'):
                        ax.text(j, i, f'{c["ratio"]:.2g}×; n={c["count"]}/{c["total"]}\n[{c["minimum"]:.2g}, {c["maximum"]:.2g}]', ha='center', va='center', fontsize=7, color='white' if abs(math.log10(c['ratio'])) > .7 * ratio_limit else '#222')
                    else:
                        ax.text(j, i, f'{c["ratio"]:.2g} [{c["low"]:.2g}, {c["high"]:.2g}]', ha='center', va='center', fontsize=7, color='white' if abs(math.log10(c['ratio'])) > .7 * ratio_limit else '#222')
            ax.set_xticks(range(len(programs)), [NAMES[p] for p in programs])
            ax.set_yticks(range(len(rows)), [r['method'] if view.get('summary') else r['workload'] + ' / ' + r['method'] for r in rows])
        elif kind == 'agreement':
            rows = list(dict.fromkeys(c['workload'] for c in view['cells']))
            columns = view['columns']
            for i, w in enumerate(rows):
                for j, (p, m) in enumerate(columns):
                    c = next((c for c in view['cells'] if c['workload'] == w and c['program'] == p and c['method'] == m), None)
                    state = c['status'] if c else 'unavailable'
                    ax.add_patch(Rectangle((j - .5, i - .5), 1, 1, facecolor=AGREEMENT_COLORS[state], edgecolor='white'))
                    text = agreement_text(c) if c else ''
                    if text:
                        ax.text(j, i, text.replace(', ', '\n'), ha='center', va='center', fontsize=7)
            ax.set(xlim=(-.5, len(columns) - .5), ylim=(len(rows) - .5, -.5))
            ax.set_xticks(range(len(columns)), [NAMES[p] + ' ' + m for p, m in columns])
            ax.set_yticks(range(len(rows)), rows)
            ax.legend(handles=[Patch(facecolor=c, label={'found': 'Agree (gate passed)', 'different': 'Different (gate failed)', 'traced': 'Traced (gate passed)', 'unavailable': 'Unavailable / designed skip'}[s]) for s, c in AGREEMENT_COLORS.items()],
                      loc='upper center', bbox_to_anchor=(.5, -.07), ncol=2, frameon=False)
        elif kind == 'ratio_summary':
            records = view['records']; models = list(dict.fromkeys(r['model'] for r in records)); groups = [('xppaut', 'fd'), ('auto07p', 'fd'), ('auto07p', 'exact')]
            for index, (p, m) in enumerate(groups):
                rr = [r for r in records if r['program'] == p and r['method'] == m]
                yy = [models.index(r['model']) + (index - 1) * .2 for r in rr]
                good = [(r, y) for r, y in zip(rr, yy) if r['ratio'] is not None]
                ax.errorbar([r['ratio'] for r, _ in good], [y for _, y in good],
                            xerr=[[r['ratio'] - r['low'] for r, _ in good], [r['high'] - r['ratio'] for r, _ in good]],
                            fmt=['o', 's', '^'][index], color=COLORS[p], markersize=3, linewidth=.7, label=NAMES[p] + ' ' + m)
                ax.scatter([1. for r, _ in zip(rr, yy) if r['ratio'] is None], [y for r, y in zip(rr, yy) if r['ratio'] is None], marker='x', color='#888', s=12)
            ax.axvline(1, color=COLORS['xppautx'], linestyle=':', linewidth=1)
            ax.set_xscale('log'); log_ticks(ax.xaxis)
            ax.set_yticks(range(len(models)), models); ax.invert_yaxis()
            ax.set_xlabel('Other / xppautX; point = branch median, bars = full branch/repetition range')
            handles, _ = ax.get_legend_handles_labels()
            handles.append(Line2D([0], [0], marker='x', color='#888', linestyle='none', label='No paired timing (including pilot-only)'))
            ax.legend(handles=handles, frameon=False, loc='upper center', bbox_to_anchor=(.5, -.10), ncol=2)
        else:
            for index, t in enumerate(view['traces']):
                # Preserve None as a gap: removing it would connect disjoint support.
                y = np.array([v if v is not None else np.nan for v in t['y']], dtype=float)
                x = t['x']; color = COLORS[t['program']]
                label = None if t.get('faint') or t.get('legend') is False else t['name']
                if t.get('bars'):
                    xx = np.arange(len(x)) + (index - .5) * .25
                    valid = np.isfinite(y)
                    ax.bar(xx[valid], y[valid] - 1, bottom=1, width=.23, color=color, label=label)
                    if t['low'] is not None:
                        lo, hi = np.array(t['low'], dtype=float), np.array(t['high'], dtype=float)
                        ax.errorbar(xx[valid], y[valid], yerr=[y[valid] - lo[valid], hi[valid] - y[valid]], fmt='none', color='black', linewidth=.5)
                    ax.set_xticks(range(len(x)), x, rotation=75, ha='right')
                else:
                    ax.plot(x, y, color=color, alpha=.18 if t.get('faint') else 1, linewidth=.7 if t.get('faint') else 1.3,
                            linestyle=styles[t.get('style', 0)], drawstyle='steps-post' if t.get('step') else 'default',
                            marker=None if t.get('step') else '.', markersize=3, label=label)
                    if t['low'] is not None:
                        ax.fill_between(x, np.array(t['low'], dtype=float), np.array(t['high'], dtype=float),
                                        color=color, alpha=.04 if t.get('faint') else .16)
            if view.get('reference'):
                ax.axvline(1, color=COLORS['xppautx'], linestyle=':', linewidth=1, label='xppautX reference (=1)')
            if view['xlog']:
                ax.set_xscale('log'); log_ticks(ax.xaxis)
            if view['ylog']:
                ax.set_yscale('log')
                if not any(y is not None and y > 0 for t in view['traces'] for y in t['y']):
                    ax.set_ylim(.5, 2); ax.text(.5, .5, 'No headline timing: pilot evidence only', transform=ax.transAxes, ha='center', fontsize=7)
                log_ticks(ax.yaxis)
            else:
                ax.set_ylim(0, 1.03)
            ax.set_xlabel(fill(view['xlabel'], 48)); ax.set_ylabel(fill(view['ylabel'], 38))
            ax.legend(frameon=False, loc='best')
        title_width = max(24, int(ax.get_position().width * ax.figure.get_figwidth() * 72 / (8 * .55)))
        ax.set_title(fill(view['title'], title_width), fontsize=8)
    def forest(view):
        # One row of panels on one shared log axis of the time ratio: a dot per single model; for a group, its median (dot),
        # the middle half of its workloads (bar) and how many it has (the number above the bar). The caption says what the
        # marks mean.
        order = ('single', 'izhikevich', 'python', 'continuation', 'scaling')
        panels = [next(p for p in view['panels'] if p['key'] == k) for k in order]
        # A panel is as wide as its rows or its title's longest line (about four characters to a row), whichever is wider.
        widths = [max(len(p['rows']), max(map(len, PANEL_TITLES[p['key']].split('\n'))) / 4) + .6 for p in panels]
        fig, axes = plt.subplots(1, len(panels), figsize=(view['width'], 5.2), sharey=True, width_ratios=widths, layout='constrained')
        for ax, panel in zip(axes, panels):
            programs = panel['programs']
            offsets = np.linspace(-.25, .25, len(programs)) if len(programs) > 1 else [0.]
            for x, r in enumerate(panel['rows']):
                for dx, q in zip(offsets, programs):
                    c = r['cells'].get(q)
                    if not c:
                        continue
                    if 'q1' in c:
                        ax.plot([x + dx] * 2, [c['q1'], c['q3']], color=COLORS[q], linewidth=2.4, solid_capstyle='butt', alpha=.45)
                        ax.text(x + dx, max(c['q3'], c['ratio']) * 1.15, str(c['count']), ha='center', va='bottom', fontsize=6.5, color=COLORS[q])
                    ax.plot(x + dx, c['ratio'], 'o', color=COLORS[q], markersize=3.5)
            ax.axhline(1, color='#555', linewidth=.8)
            ax.set_xticks(range(len(panel['rows'])), [r['label'] for r in panel['rows']], rotation=90, ha='center')
            ax.tick_params(axis='x', length=0)
            ax.set_xlim(-.6, len(panel['rows']) - .4)
            ax.spines['bottom'].set_visible(False)
            ax.set_title(PANEL_TITLES[panel['key']], fontsize=7.5, fontweight='bold')
        ax = axes[0]
        ax.set_yscale('log')
        ax.set_ylim(.3, 50)
        ax.set_yticks([.5, 1, 2, 5, 10, 20, 50], ['0.5', '1', '2', '5', '10', '20', '50'])
        ax.yaxis.set_minor_formatter(NullFormatter())
        ax.set_ylabel('Other program\'s time ÷ xppautX\'s')
        programs = list(dict.fromkeys(q for p in panels for q in p['programs']))
        fig.legend(handles=[Line2D([0], [0], color=COLORS[q], marker='o', linewidth=2.4, label=NAMES[q]) for q in programs],
                   loc='outside upper center', ncol=len(programs), frameon=False)
        return fig
    for view in charts:
        if view.get('kind') == 'forest':
            fig = forest(view)
        elif view.get('kind') == 'panels':
            ncols = view['ncols']; nrows = math.ceil(len(view['panels']) / ncols)
            fig, axes = plt.subplots(nrows, ncols, figsize=(view['width'], view['height']), squeeze=False, layout='constrained')
            for ax, panel in zip(axes.flat, view['panels']):
                draw(ax, panel)
            for ax in list(axes.flat)[len(view['panels']):]:
                ax.set_visible(False)
            fig.suptitle(fill(view['title'], 100), fontsize=8)
        else:
            rows = len(view.get('rows', [])) or len({c['workload'] for c in view.get('cells', [])})
            default_height = max(3.7, rows * .23 + 1.6) if view.get('kind') in ('heatmap', 'agreement') else 4.6 if any(t.get('bars') for t in view.get('traces', [])) else 3.4
            fig, ax = plt.subplots(figsize=(view.get('width', 7.), view.get('height', default_height)), layout='constrained')
            draw(ax, view)
        if view.get('kind') == 'heatmap':
            bar = fig.colorbar(ScalarMappable(norm=TwoSlopeNorm(0, -ratio_limit, ratio_limit), cmap=cmap),
                               ax=[ax for ax in fig.axes if ax.get_visible()], orientation='horizontal', fraction=.025, pad=.04)
            bar.set_ticks([-ratio_limit, 0, ratio_limit], labels=[f'{10 ** -ratio_limit:.2g}', '1', f'{10 ** ratio_limit:.2g}'])
            bar.set_label('Other / xppautX ratio (log colour): red = slower xppautX; blue = faster')
        directory = out / view['set']; directory.mkdir(parents=True, exist_ok=True)
        for suffix in ('png', 'svg'):
            fig.savefig(directory / (view['key'] + '.' + suffix), dpi=PNG_DPI, bbox_inches='tight', pad_inches=.05)  # nothing clipped at the edge
        plt.close(fig)


def issue_history():
    history = read(HERE / 'final_report_history.json')
    roadmap = {}
    for text in (ROOT / 'docs/roadmap.md').read_text().splitlines():
        cells = [v.strip() for v in text.strip('|').split('|')]
        if re.fullmatch(r'W\d+[a-z]?', cells[0]):
            roadmap[cells[0]] = cells[1:]
    return dict(cards=[dict(card=c, roadmap=roadmap[c]) for c in dict.fromkeys(history['series_cards'] + history['issue_cards'])],
                candidates=history['candidates'])


def page(data, charts, out):
    # Escape script delimiters even when a frozen model or log contains HTML.
    blob = json.dumps(dict(data=data, views=charts, colors=COLORS, names=NAMES, programs=STUDY_PROGRAMS), allow_nan=False).replace('<', '\\u003c')
    plotly = (HERE / 'plotly-3.3.1.min.js').read_text().replace('</script', '<\\/script')
    license_notice = '/*\n' + (HERE / 'plotly-LICENSE.txt').read_text() + '*/\n'
    template = r'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>W583 final comparison — W589</title><style>
:root{color-scheme:light dark;--bg:#fafafa;--fg:#222;--card:#fff;--edge:#bbb}@media(prefers-color-scheme:dark){:root{--bg:#181b20;--fg:#eee;--card:#242930;--edge:#666}}
body{font:15px/1.5 system-ui,sans-serif;background:var(--bg);color:var(--fg);max-width:1300px;margin:auto;padding:24px}h1,h2{line-height:1.2}a{color:#56a4e4}select,button{font:inherit;padding:6px;background:var(--card);color:var(--fg);border:1px solid var(--edge)}nav{position:sticky;top:0;background:var(--bg);padding:12px;z-index:10;border-bottom:1px solid var(--edge)}nav a{margin-right:18px}.chart{background:var(--card);min-height:350px;margin:18px 0}table{border-collapse:collapse;width:100%;font-size:13px}td,th{padding:7px;border:1px solid var(--edge);text-align:left}td{overflow-wrap:anywhere}details{margin:10px 0}pre{white-space:pre-wrap;overflow-wrap:anywhere;max-height:500px;overflow:auto}img{max-width:100%;height:auto}.scroll{max-height:480px;overflow:auto}.note{border-left:4px solid #999;padding-left:14px}dialog{background:var(--bg);color:var(--fg);width:min(1100px,90vw);max-height:90vh;overflow:auto;border:1px solid var(--edge)}.close{float:right}section{scroll-margin-top:100px}.muted{opacity:.8}
</style><h1>The frozen W583 comparison</h1><p>Rebuilt from the frozen results, without running a benchmark. All ratios are <b>other / xppautX</b>; above 1 means xppautX is faster. Blue favours xppautX, red favours the other program. Program colours are fixed throughout.</p>
<nav><a href="#overview">Overview</a><a href="#profiles">Matched accuracy</a><a href="#normalized">Normalised curves</a><a href="#scaling">Scaling</a><a href="#continuation">Continuation</a><a href="#exceptions">Unequal work</a><a href="#issues">Issues</a><br>
<label>Study <select id="study"></select></label> <label>Model <select id="model"></select></label> <label>Method <select id="method"></select></label> <button id="reset">Reset</button></nav>
<p class="note">Time bands show the range of counted repetitions, excluding warm-up; ratios use conservative bounds (other minimum / xppautX maximum, other maximum / xppautX minimum), not confidence intervals. Large-stiff has mean ± sample SD because its summary stores SD. One-counted-run cases have no measured spread. Continuation selects the fastest equal-work variant separately within finite-difference and exact Jacobian groups; its selected engine and repetitions are retained in each cell. Integration time includes start-up; continuation time is the branch run. No timings of aborts are presented as completed work.</p>
<section id="overview"><h2>Overview: where xppautX saves time</h2><p>Rows are grouped by study and method. Grey hatched cells have no comparable timing: hover for the reason. Click a cell or workload to inspect its data and overlays.</p><div id="heat" class="chart"></div><div id="inventory" class="scroll"></div></section>
<section id="profiles"><h2>Matched accuracy means passing the equal-work gate</h2><p>Each completed pair uses its timed equal-work mean, other / xppautX. The screen tolerance is <b>__ATOL__ + __RTOL__ × |reference value|</b> at each value. The ladder selects each program's loosest tolerance that passes this gate; other integration studies use their saved equal-work gate (including the declared time-grid allowance, __TIMING__ of an output step, where applicable). Work-precision sweeps diagnose the same timed rows; no curve interpolation replaces a passed timing. Aborts, a ladder's “never”, and unavailable XPPAUT sizes remain in the denominator as “does not reach”, with their reasons below. Only programs run in a study enter its denominator. Terman's untimed acceptance evidence has no timing ratio. xppautX is the vertical reference at 1; curves show mean ratios, without shaded bounds.</p><p id="profile-count"></p><div id="profile" class="chart"></div><div id="profile-multiples"></div><details><summary>Study without timed profile rows</summary><div id="excluded"></div></details><div id="matched-table"></div></section>
<section id="normalized"><h2>Relative curves retain each workload's shape</h2><p>One panel per program: faint curves and repetition-range bands, bold pointwise median and shaded 25–75% workload band. Each curve stops at its own sampled limits. The median and 25–75% band exist only where at least three curves have data, with gaps retained in both renderers and contributor counts exported. There are six timed tolerance sweeps, but only four workloads resolve changing error (the others are at an error floor); this is fewer than five informative sweeps, so this view is kept in the HTML/appendix and omitted from the curated paper set. Constant-error curves appear as vertical segments and contribute only at their exact sampled error, never across an invented width. Zero errors have no position on a log axis and stay in the data.</p><div id="normal-plots"></div></section>
<section id="scaling"><h2>Scaling separates model size from solver cost</h2><p>Scaling's step, T and 2T runs are separated. These data contain one thread only, drawn solid; there is no measured thread-count scaling to invent. Large-stiff compares xppautX solvers only: line style distinguishes solvers while the program colour stays fixed. XPPAUT's unavailable large sizes remain in the overview and unequal-work table.</p><div id="scale-plots"></div></section>
<section id="continuation"><h2>Continuation: comparison gate judgments and time</h2><p>Cells show the comparison gate's own <b>agree, traced, different, or unavailable</b> judgment. Finite-difference cells use the saved pilot gates; exact cells use converged.json's exact_pilots. Where a pilot gate was not saved, the existing continuation gate calls autocompare on saved outputs, without running a program or writing the frozen data. Matched-point counts come from distinct special-point measurements; traced cells retain the gate's causes, and different cells retain its own words. No second label matcher is used. This is the gate's judgment, not an independent claim that a label is real or spurious. Unavailable pilots and designed skips stay grey. Pilot-only setup/inherited descriptions without headline timing rows are retained and identified in the overview. Click cells for gate evidence, traced causes, skip reasons and overlays.</p><div id="continuation-plots"></div></section>
<section id="exceptions"><h2>The ten groups that are not equal work</h2><p>Derived from diverged records, never-reaching ladders, unavailable models, unsupported/not-gated pilots, omitted reference runs and description skips. Grouping matches the historical report's section 5; each group expands to its individual workloads.</p><div id="unequal"></div></section>
<section id="terman"><h2>Terman acceptance is untimed evidence</h2><p>This study has acceptance and convergence readouts, not a runs.json timing series. It is not in the time heatmap or matched profile.</p><details><summary>Acceptance, convergence and saved readouts</summary><pre id="terman-data"></pre></details></section>
<details><summary>Every overlay and skipped overlay</summary><div id="all-overlays"></div></details>
<section id="issues"><h2>Issues found and fixes made</h2><p>Cards from the earlier series and the final report are carried forward together. The roadmap supplies the recorded titles, issue links and status; candidate descriptions preserve the 2026-10-31 report's historical assessment.</p><div id="issue-list"></div></section>
<section><h2>Reproducible numbers and paper figures</h2><p><a href="data/report.json">Complete report data</a> · <a href="data/views.json">Every chart's values</a> · <a href="data/coverage.json">Coverage and exclusions</a>. The figures/paper/ folder contains three curated PNG/SVG figures, with their captions in its README.md; figures/appendix/ holds the per-workload heatmaps, full branch panels and diagnostic views. All fonts are at least 7 pt at the saved 7 in width. Raw summary rows (including start-up) and selected engine identities are in report.json; the frozen input is never modified.</p><div id="paper-files"></div><details id="paper-section"><summary>Inspect the three paper views</summary><div id="paper-plots"></div></details><details><summary>Coverage</summary><pre id="coverage"></pre></details></section>
<dialog id="detail"><button class="close" id="close">Close</button><h2 id="detail-title"></h2><div id="detail-chart" class="chart"></div><div id="detail-overlays"></div><pre id="detail-data"></pre></dialog>
<script>__PLOTLY__</script><script id="report-data" type="application/json">__DATA__</script><script>
'use strict';const B=JSON.parse(document.getElementById('report-data').textContent),D=B.data,V=B.views,C=B.colors,N=B.names;
const $=id=>document.getElementById(id), esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let dark=matchMedia('(prefers-color-scheme: dark)').matches;
function layout(title){return {title:{text:title,font:{size:16}},paper_bgcolor:'transparent',plot_bgcolor:'transparent',font:{color:dark?'#eee':'#222',size:12},margin:{l:90,r:40,t:70,b:85},legend:{orientation:'h'},hovermode:'closest'};}
function table(target,headers,rows){$(target).innerHTML='<table><thead><tr>'+headers.map(h=>'<th>'+esc(h)+'</th>').join('')+'</tr></thead><tbody>'+rows.map(r=>'<tr>'+r.map(c=>'<td>'+esc(c)+'</td>').join('')+'</tr>').join('')+'</tbody></table>';}
function options(id,values){$(id).innerHTML=['all',...new Set(values)].map(s=>'<option value="'+esc(s)+'">'+esc(s)+'</option>').join('');}
options('study',D.inventory.map(r=>r.study));options('model',D.inventory.map(r=>r.model));options('method',D.inventory.map(r=>r.method));
function chosen(r){return ($('study').value==='all'||r.study===$('study').value)&&($('model').value==='all'||(r.model??r.workload.split(/[/-]/)[0])===$('model').value)&&($('method').value==='all'||r.method===$('method').value);}
const ratioLimit=D.ratio_limit;
function drawHeat(node,v){
 const rows=v.rows,p=v.programs,labels=rows.map(r=>v.summary?r.method:r.workload+' / '+r.method);
 let l=layout(v.title+' (baseline: xppautX)');l.height=Math.max(350,rows.length*28+150);l.margin.l=v.summary?170:310;l.yaxis={autorange:'reversed'};
 const shapes=[],annotations=[];
 rows.forEach((r,i)=>p.forEach((prog,j)=>{const c=r.cells[prog];if(c.ratio===null){shapes.push({type:'rect',xref:'x',yref:'y',x0:j-.5,x1:j+.5,y0:i-.5,y1:i+.5,fillcolor:'#888',opacity:.3,line:{width:0}});annotations.push({x:j,y:i,text:'///',showarrow:false});}else if(v.summary){annotations.push({x:j,y:i,text:c.ratio.toPrecision(2)+'×; n='+c.count+'/'+c.total+'<br>['+c.minimum.toPrecision(2)+', '+c.maximum.toPrecision(2)+']',showarrow:false,font:{color:Math.abs(Math.log10(c.ratio))>.7*ratioLimit?'white':'#222'}});}}));l.shapes=shapes;l.annotations=annotations;
 const hover=rows.map(r=>p.map(p=>JSON.stringify(r.cells[p])));
 const traces=[{type:'heatmap',x:p.map(p=>N[p]),y:labels,z:rows.map(r=>p.map(p=>r.cells[p].ratio?Math.log10(r.cells[p].ratio):null)),zmin:-ratioLimit,zmax:ratioLimit,zmid:0,colorscale:[[0,'#b2182b'],[.5,'#f7f7f7'],[1,'#2166ac']],hoverongaps:false,text:hover,hovertemplate:'%{y}<br>%{x}<br>%{text}<extra></extra>',colorbar:{title:{text:'log10 ratio'}}},
 {type:'scatter',mode:'markers',x:rows.flatMap(()=>p.map(p=>N[p])),y:rows.flatMap((r,i)=>p.map(()=>labels[i])),marker:{opacity:0,size:30},text:hover.flat(),hovertemplate:'%{y}<br>%{x}<br>%{text}<extra></extra>',showlegend:false}];
 Plotly.react(node,traces,l,{responsive:true});if(!v.summary)node.on('plotly_click',e=>{const i=labels.indexOf(e.points[0].y);if(i>=0)detail(rows[i]);});
}
function heat(rows){$('heat').replaceChildren();for(const study of new Set(rows.map(r=>r.study))){const div=document.createElement('div');div.className='chart';$('heat').append(div);drawHeat(div,{rows:rows.filter(r=>r.study===study),programs:B.programs[study],title:D.findings[study]+' (all-study medians)'});}}
function draw(node,v){let l=layout(v.title),tr=[];l.xaxis={title:{text:v.xlabel},type:v.xlog?'log':'category'};l.yaxis={title:{text:v.ylabel},type:v.ylog?'log':'linear'};
if(!v.ylog)l.yaxis.range=[0,1.03];
if(v.reference){l.shapes=[{type:'line',xref:'x',yref:'paper',x0:1,x1:1,y0:0,y1:1,line:{color:C.xppautx,dash:'dot',width:2}}];}
if(v.xlog)l.xaxis.dtick='D2';if(v.ylog)l.yaxis.dtick='D2';
for(const t of v.traces){const color=C[t.program],style=['solid','dash','dot','dashdot'][t.style??0];
if(t.low){tr.push({x:t.x,y:t.low,type:'scatter',mode:'lines',line:{width:0,color},showlegend:false,hoverinfo:'skip'});tr.push({x:t.x,y:t.high,type:'scatter',mode:'lines',line:{width:0,color},fill:'tonexty',fillcolor:color+(t.faint?'0a':'26'),showlegend:false,hoverinfo:'skip'});}
tr.push({x:t.x,y:t.bars?t.y.map(y=>y===null?null:y-1):t.y,name:t.name,type:t.bars?'bar':'scatter',base:t.bars?1:undefined,mode:'lines+markers',opacity:t.faint?.22:1,line:{color,width:t.faint?1:2,dash:style,shape:t.step?'hv':'linear'},marker:{color,size:t.faint?3:5},error_y:t.bars?{type:'data',symmetric:false,array:t.y.map((y,i)=>y===null?null:t.high[i]-y),arrayminus:t.y.map((y,i)=>y===null?null:y-t.low[i])}:undefined,connectgaps:false,showlegend:!t.faint&&t.legend!==false});}
l.barmode='group';Plotly.react(node,tr,l,{responsive:true});}
function panels(id,views){$(id).replaceChildren();for(const v of views){const div=document.createElement('div');div.className='chart';$(id).append(div);draw(div,v);}}
function drawAgreement(node,v){
 const ws=[...new Set(v.cells.map(c=>c.workload))],cols=v.columns,labels=cols.map(([p,m])=>N[p]+' '+m),states=Object.keys(D.agreement_colors),names=['Agree (gate passed)','Different (gate failed)','Traced (gate passed)','Unavailable / designed skip'];
 let l=layout(v.title);l.height=Math.max(350,ws.length*28+180);l.margin.l=310;l.yaxis={autorange:'reversed'};
 const cells=ws.map(w=>cols.map(([p,m])=>v.cells.find(c=>c.workload===w&&c.program===p&&c.method===m)));
 const scale=states.flatMap((s,i)=>[[i/states.length,D.agreement_colors[s]],[(i+1)/states.length,D.agreement_colors[s]]]);
 const traces=[{type:'heatmap',x:labels,y:ws,z:cells.map(row=>row.map(c=>states.indexOf(c?.state??'unavailable'))),zmin:-.5,zmax:states.length-.5,colorscale:scale,showscale:false,text:cells.map(row=>row.map(c=>c?.text??'')),texttemplate:'%{text}',textfont:{color:'#222',size:12},customdata:cells,hovertemplate:'%{y}<br>%{x}<br>%{text}<extra></extra>'},...states.map((s,i)=>({type:'scatter',mode:'markers',x:[null],y:[null],name:names[i],marker:{symbol:'square',size:12,color:D.agreement_colors[s]}}))];
 Plotly.react(node,traces,l,{responsive:true});node.on('plotly_click',e=>{const c=e.points[0].customdata;if(c)detail({study:'continuation',workload:c.workload,method:c.method,aggregated:!!v.aggregated});});
}
function drawRatioSummary(node,v){
 const models=[...new Set(v.records.map(r=>r.model))],l=layout(v.title),groups=[['xppaut','fd'],['auto07p','fd'],['auto07p','exact']];l.height=models.length*28+180;l.margin.l=220;l.xaxis={type:'log',title:{text:v.xlabel},dtick:'D2'};l.yaxis={autorange:'reversed'};l.shapes=[{type:'line',xref:'x',yref:'paper',x0:1,x1:1,y0:0,y1:1,line:{color:C.xppautx,dash:'dot'}}];
 const traces=groups.map(([p,m],i)=>{const rr=v.records.filter(r=>r.program===p&&r.method===m);return {type:'scatter',mode:'markers',name:N[p]+' '+m,x:rr.map(r=>r.ratio),y:rr.map(r=>r.model),marker:{color:C[p],symbol:['circle','square','triangle-up'][i],size:7},error_x:{type:'data',symmetric:false,array:rr.map(r=>r.ratio===null?null:r.high-r.ratio),arrayminus:rr.map(r=>r.ratio===null?null:r.ratio-r.low)},text:rr.map(r=>JSON.stringify(r)),hovertemplate:'%{text}<extra></extra>'};});const missing=v.records.filter(r=>r.ratio===null);traces.push({type:'scatter',mode:'markers',name:'No paired timing (including pilot-only)',x:missing.map(()=>1),y:missing.map(r=>r.model),marker:{symbol:'x',color:'#888',size:6},text:missing.map(r=>JSON.stringify(r)),hovertemplate:'%{text}<extra></extra>'});Plotly.react(node,traces,l,{responsive:true});
}
function renderSpec(node,v){if(v.kind==='panels'){node.style.display='grid';node.style.gridTemplateColumns='repeat(2,minmax(0,1fr))';for(const panel of v.panels){const div=document.createElement('div');div.className='chart';node.append(div);renderSpec(div,panel);}}else if(v.kind==='heatmap')drawHeat(node,v);else if(v.kind==='agreement')drawAgreement(node,v);else if(v.kind==='ratio_summary')drawRatioSummary(node,v);else draw(node,v);}
$('paper-section').ontoggle=()=>{if($('paper-section').open&&!$('paper-plots').children.length){for(const v of V.filter(v=>v.set==='paper')){const title=document.createElement('h3');title.textContent=v.key;$('paper-plots').append(title);const div=document.createElement('div');$('paper-plots').append(div);const img=document.createElement('img');img.src='figures/paper/'+v.key+'.png';img.alt=v.title;img.style.maxWidth='100%';div.append(img);}}};
function detail(r){$('detail-title').textContent=r.study+' / '+r.workload+' / '+r.method;const curves=D.curves.filter(c=>c.study===r.study&&c.workload===r.workload);
draw('detail-chart',{title:curves.length?'Work-precision: every timed rung':'No timed tolerance ladder for this workload',xlabel:'Maximum absolute error versus reference (model units)',ylabel:'Wall time (s)',xlog:true,ylog:true,traces:curves.map(c=>({name:N[c.program],program:c.program,x:c.points.map(p=>p.error),y:c.points.map(p=>p.time),low:c.points.map(p=>p.low),high:c.points.map(p=>p.high)}))});
$('detail-overlays').replaceChildren();let figs=D.overlays.figures.filter(f=>f.study===r.study&&((r.aggregated&&f.workload.split('/')[0]===r.workload)||f.workload===r.study+'/'+r.workload||f.workload===r.workload||(f.workload===r.workload.split('/').slice(0,2).join('/')&&f.run===r.workload.split('/')[2])));
for(const f of figs){let a=document.createElement('a');a.href='overlays/'+f.figure;let img=document.createElement('img');img.src=a.href;img.alt=[f.workload,f.program,f.quantity].join(' · ');img.loading='lazy';a.append(img);$('detail-overlays').append(a);}
$('detail-data').textContent=JSON.stringify({large_stiff:r.study==='large-stiff'?D.large_stiff.filter(x=>x.family===r.model&&x.solver===r.method):undefined,terman:r.study==='terman'?D.terman:undefined,overview:D.overview.filter(x=>x.study===r.study&&(x.workload===r.workload||(r.aggregated&&x.workload.startsWith(r.workload+'/')))),matched:D.matched.filter(x=>x.study===r.study&&(x.workload===r.workload||(r.aggregated&&x.workload.startsWith(r.workload+'/')))),agreement:D.agreement.filter(x=>(x.workload===r.workload||(r.aggregated&&x.workload.startsWith(r.workload+'/')))&&x.method===r.method),gates:Object.fromEntries(Object.entries(D.gates).filter(([k])=>k.startsWith(r.workload.split('/').slice(0,2).join('/')+'/'))),excluded:D.unequal.filter(x=>x.study===r.study&&x.workloads.includes(r.workload)),curves},null,2);$('detail').showModal();}
matchMedia('(prefers-color-scheme: dark)').addEventListener('change',e=>{dark=e.matches;redraw();});
$('close').onclick=()=>$('detail').close();
function profileView(mm,title){
 const signature=[...new Set(mm.map(r=>r.study+'/'+r.workload))].sort().join('|');
 const v=D.filter_profiles[signature];
 return v?{...v,title:(title?title+' · ':'')+v.title}:{title:'No equal-work timing rows in this selection',xlabel:'Other / xppautX wall time',ylabel:'Fraction of workloads',xlog:true,ylog:false,traces:[]};
}
function redraw(){
 const rows=D.overview.filter(chosen),inventory=D.inventory.filter(chosen);heat(rows);
 $('inventory').innerHTML='<table><tr><th>Workload (click to inspect)</th><th>Study</th><th>Method</th></tr>'+inventory.map((r,i)=>'<tr><td><button data-row="'+i+'">'+esc(r.workload)+'</button></td><td>'+esc(r.study)+'</td><td>'+esc(r.method)+'</td></tr>').join('')+'</table>';
 $('inventory').querySelectorAll('button').forEach(b=>b.onclick=()=>detail(inventory[Number(b.dataset.row)]));
 const mm=D.matched.filter(chosen);
 draw('profile',profileView(mm,''));
 $('profile-count').textContent=new Set(mm.map(r=>r.study+'/'+r.workload)).size+' selected workloads; matched means the equal-work gate. '+mm.filter(r=>r.ratio===null).length+' program/workload cases do not reach or are unavailable; all stay in the denominator.';
 let multiples=[];
 for(const study of new Set(mm.map(r=>r.study)))multiples.push(profileView(mm.filter(r=>r.study===study),'Study: '+study));
 for(const method of new Set(mm.map(r=>r.method)))multiples.push(profileView(mm.filter(r=>r.method===method),'Method: '+method));
 panels('profile-multiples',multiples);
 let normal=[];
 for(const v of V.filter(v=>v.set==='appendix'&&v.key.startsWith('normalized-'))){const p=v.key.replace('normalized-',''),cc=D.curves.filter(c=>c.program===p&&chosen(c)),all=D.curves.filter(c=>c.program===p);if(!cc.length)continue;
 const names=new Set(cc.map(c=>c.workload));let traces=v.traces.filter(t=>names.has(t.name));
 if(cc.length===all.length)traces.push(...v.traces.filter(t=>t.population));
 normal.push({...v,title:N[p]+': '+cc.length+' selected curves; median needs 3 contributors',traces});}
 panels('normal-plots',normal);
 let scales=[];
 for(const v of V.filter(v=>v.set==='appendix'&&v.key.startsWith('scaling-'))){const [,family,kind]=v.key.split('-'),traces=v.traces.map(t=>{const indices=t.x.map((n,i)=>rows.some(r=>r.study==='scaling'&&r.workload===family+n+'-'+kind)?i:-1).filter(i=>i>=0);return {...t,x:indices.map(i=>t.x[i]),y:indices.map(i=>t.y[i]),low:indices.map(i=>t.low[i]),high:indices.map(i=>t.high[i])};}).filter(t=>t.x.length);if(traces.length)scales.push({...v,traces});}
 for(const v of V.filter(v=>v.set==='appendix'&&v.key.startsWith('large-stiff-'))){const family=v.key.split('-')[2],traces=v.traces.filter(t=>inventory.some(r=>r.study==='large-stiff'&&r.model===family&&r.method===t.name));if(traces.length)scales.push({...v,traces});}
 panels('scale-plots',scales);
 $('continuation-plots').replaceChildren();
 for(const original of V.filter(v=>v.set==='appendix'&&v.key.startsWith('continuation-'))){const method=original.key.split('-')[2],ws=new Set(rows.filter(r=>r.study==='continuation'&&r.method===method).map(r=>r.workload));let v;
 if(original.kind==='agreement'){const cells=original.cells.filter(c=>ws.has(c.workload));if(!cells.length)continue;v={...original,cells};}
 else{const traces=original.traces.map(t=>{const ix=t.x.map((w,i)=>ws.has(w)?i:-1).filter(i=>i>=0);return {...t,x:ix.map(i=>t.x[i]),y:ix.map(i=>t.y[i]),low:ix.map(i=>t.low[i]),high:ix.map(i=>t.high[i])};});if(!traces.some(t=>t.x.length))continue;v={...original,traces};}
 const div=document.createElement('div');div.className='chart';$('continuation-plots').append(div);if(v.kind==='agreement')drawAgreement(div,v);else draw(div,v);}
 table('matched-table',['Study','Workload','Program','Equal-work mean ratio','Coverage','Reason'],mm.map(r=>[r.study,r.workload,N[r.program],r.ratio,r.status,r.reason]));
}
for(const id of ['study','model','method'])$(id).onchange=redraw;$('reset').onclick=()=>{for(const id of ['study','model','method'])$(id).value='all';redraw();};
table('excluded',['Study','Workload','Why excluded'],D.coverage.profile_exclusions.map(r=>[r.study,r.workload,r.reason]));
table('unequal',['Study','Workloads','Programs','Reason','Evidence'],D.unequal.map(r=>[r.study,r.workloads.join(', '),r.programs.map(p=>N[p]).join(', '),r.reason,r.sources.join(', ')]));
$('issue-list').innerHTML='<ul>'+D.history.cards.map(c=>'<li><b>'+esc(c.card)+'</b> '+c.roadmap.map(s=>esc(s).replace(/\[([^\]]+)\]\((https:\/\/github\.com\/[^)]+)\)/g,'<a href="$2">$1</a>')).join(' · ')+'</li>').join('')+'</ul><h3>Historical candidate assessment</h3><ul>'+D.history.candidates.map(c=>'<li><b>'+esc(c[0])+'</b> '+esc(c[1])+'</li>').join('')+'</ul>';
$('all-overlays').innerHTML='<ul>'+D.overlays.figures.map(f=>'<li><a href="overlays/'+esc(f.figure)+'">'+esc([f.study,f.workload,f.run,f.program,f.quantity].filter(Boolean).join(' · '))+'</a></li>').join('')+'</ul><pre>'+esc(JSON.stringify(D.overlays.skipped,null,2))+'</pre>';
$('paper-files').innerHTML='<ul>'+D.paper_files.map(f=>'<li><a href="'+esc(f)+'">'+esc(f)+'</a> · <a href="'+esc(f.replace('.png','.svg'))+'">SVG</a></li>').join('')+'</ul>';
$('coverage').textContent=JSON.stringify(D.coverage,null,2);$('terman-data').textContent=JSON.stringify(D.terman,null,2);redraw();
</script></html>'''
    (out / 'report.html').write_text(template.replace('__PLOTLY__', license_notice + plotly).replace('__DATA__', blob).replace('__ATOL__', str(data['screen_tolerance']['atol'])).replace('__RTOL__', str(data['screen_tolerance']['rtol'])).replace('__TIMING__', str(data['screen_tolerance']['timing_fraction'])), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--final', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    a = parser.parse_args()
    final, out = a.final.expanduser().resolve(), a.out.expanduser().resolve()
    if out == final or final in out.parents:
        parser.error('--out must be outside the read-only frozen series')
    data = compute(final)
    data['overlays'] = read(final / 'overlays/manifest.json')
    data['history'] = issue_history()
    data['coverage'] = coverage(data)
    charts = views(data)
    data['paper_files'] = [f'figures/paper/{v["key"]}.png' for v in charts if v['set'] == 'paper']
    for directory in ('data', 'figures', 'overlays'):
        (out / directory).mkdir(parents=True, exist_ok=True)
    for f in data['overlays']['figures']:
        relative = Path(f['figure'])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('overlay path escapes overlays/: ' + str(relative))
        target = out / 'overlays' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(final / 'overlays' / relative, target)
    save(out / 'data/report.json', data)
    save(out / 'data/views.json', charts)
    save(out / 'data/coverage.json', data['coverage'])
    static(charts, out / 'figures', data['ratio_limit'])
    # A figure not yet reviewed for the paper (W590) has its finding as a provisional caption.
    (out / 'figures/paper/README.md').write_text('# Paper figures\n\nEach figure is a PNG (300 dpi) with an SVG copy.\n\n' + ''.join(
        f'## {v["key"]}.png\n\n{v.get("caption") or v["title"] + " (provisional caption)"}\n\n' for v in charts if v['set'] == 'paper')
        + large_stiff_text(data['large_stiff']))
    page(data, charts, out)
    print(json.dumps(data['coverage'], indent=2))
    print(f'{len(data["paper_files"])} paper views, {len(charts) - len(data["paper_files"])} appendix views; report: {out / "report.html"}')


if __name__ == '__main__':
    main()
