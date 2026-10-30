#!/usr/bin/env -S uv run --locked python
"""Settled continuation workloads, using the repository's drivers and gate."""
import copy
import functools
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import sys
import time
import traceback
from types import SimpleNamespace

from common import (BENCH, REPO, TOOLS, before_series, derived_program_sha, earlier_context, environment, executable_sha,
                    initialize, link_tree, mark_reused, measure_sha, progress, module, parser, reachable_sha, repetitions, rotation,
                    save, selected, sha, summarize, tracked_text, tree_sha)
import autocompare as comparison
import auto_reference as reference
from comparison_measure import RunClock, process_cpu
from odex_model import Odex
from xppclient import Server, open_auto
from integration import convert, same_key

# This is the original digest algorithm, not a second interpretation of W269.
sys.path.insert(0, str(BENCH / 'auto'))
old_events = module('w269_events', BENCH / 'auto/hopf_xppautx.py')
exact_source = module('w459_exact_source', BENCH / 'auto/auto07p/exact_source.py')
FIXED_POINT_BUDGET = 100000  # room for PY_S1Bf to reach its parameter bound with unit fixed steps
MIN_WINDOW = 1.0  # E1/T1 repetitions and T2 pilot sizing; never an acceptance test.
SMOKE_POINTS = 5  # start, intermediate samples and a completed endpoint for untimed harness checks
HAND_WRITTEN = BENCH / 'auto/auto07p/py_s1bf.c'  # PY_S1Bf's exact derivatives for AUTO-07p (W246, W269): by hand, jac_check.c
EXACT_AUTO07P = ('P1', 'E1', 'P2', 'E2', 'T1')  # the workloads AUTO-07p also runs with exact derivatives (W459: the others generated)
# W457 (#520): an exact-Jacobian row is judged against a run of xppautX's own exact Jacobian that is converged: the
# convergence tolerances divided by TIGHTENING, the continuation steps by REFINEMENT (and as many steps allowed).
TIGHTENING = 1e3
T2_SIZED_POINTS = 400  # T2's sized pilot: a fixed size, as AUTO-07p's spurious GH (finding 2) follow its step; sized from
# xppautX's pilot time it grew to 598 points once W465/W466 made it faster, and AUTO-07p labelled two, not four (series 22i)
T2_SIZED_GH = 4  # AUTO-07p's spurious GH labels at T2_SIZED_POINTS (W456's five at the description's steps)
REFINEMENT = 10
CONVERGED = '.converged'  # that run's name where compare_references names a reference
comparison.REFERENCES.setdefault(CONVERGED, 'the converged exact run')


def description(name):
    return copy.deepcopy(comparison.read_description(comparison.BASE / (name + '.json')))


def closure(d, targets):
    wanted = set(targets)
    for r in reversed(d['runs']):
        if r['name'] in wanted and 'run' in r['start']:
            wanted.add(r['start']['run'])
    d = copy.deepcopy(d)
    d['runs'] = [r for r in d['runs'] if r['name'] in wanted]
    d['differences'] = [v for v in d.get('differences', []) if v['run'] in wanted]
    return d


def workloads(a):
    models = a.out / 'models'
    models.mkdir()
    py = models / 'PY_S1Bf.ode'
    shutil.copy2(BENCH / 'auto/models/PY_S1Bf_bench.ode', py)
    convert(a, py)
    eq = dict(step_size=-1, min_step_size=1e-4, max_step_size=50, max_steps=5000, output_every=5000, parameter_min=200, parameter_max=20001,
              mesh_intervals=100, collocation_points=4, parameter_tolerance=1e-4, state_tolerance=1e-4, special_point_tolerance=1e-4, max_branch_switches=5)
    d = comparison.codes_of_types(dict(model=str(py.with_suffix('.odex')), reason='P1/E1: W269 PY_S1Bf, freshly converted by final tip', runs=[
        dict(name='steady', kind='equilibrium', start=dict(state='current'), parameter='Iapp', auto=eq),
        dict(name='periodic', kind='periodic', start=dict(run='steady', type='hopf', index=1), parameter='Iapp', auto=copy.deepcopy(eq))]))
    # W551 (#616): the third point of the cycles from the Hopf point, with DS < 0 (finding 86, AUTO-07p finding 8; docs/auto-references.md W551)
    p1_cause = (
        "XPPAUT 8.0 (point 3, Iapp 16394.43) and AUTO-07p (point 3, 16394.432) label a BP at the third point of the cycles from the Hopf point (period 2.1616, 15 pA below the Hopf point at "
        "16409.41), where the cycles have no branch point: an independent computation (docs/studies/floquet_check.py: PY_S1Bf's hand-written exact "
        "Jacobian, the cycle by Newton's method on the return map, the monodromy matrix from the variational equations) gives the second Floquet "
        "multiplier 1.00007 (a pair 1 +- 1.4e-4 i) at 16409, then 0.99679 (16400), 0.99490 (16394.43), 0.99439 (16392.93), 0.99003 (16380), "
        "0.98286 (16358.65) and 0.97692 (16300), the next two 0.9773 and 0.8976. At the Hopf point (16409.41) it is exactly 1 (e^(i w T) = 1, the amplitude "
        "direction), so cycles born there start with it at 1 and the 1.00007 is that double root split by the integration error; beyond it "
        "(1 - m)/(16409.41 - Iapp) is constant, 3.41e-4 (16400), 3.40e-4 (16394.43), 3.40e-4 (16392.93), 3.39e-4 (16380): it leaves 1 linearly "
        "from the Hopf point, as the normal form says, and returns to it nowhere. "
        "A branch point of cycles needs it at 1; this near-double multiplier just after the Hopf point is also what makes the branch-point test ill-conditioned at the first points. A central-difference monodromy gives the same (0.99489938 at 16394.43), so the Jacobian's differencing "
        "does not explain the label. It is the kind of start artefact W458 removed from xppautX (the label sits at the third point of a run with "
        "DS < 0, where stpnps orients the start's phase-condition derivative by the eigen-cycle whatever the sign of DS and the branch-point test's "
        "determinant changes sign: docs/xppaut-findings.md finding 86, docs/auto07p-findings.md finding 8); a DS > 0 run, which would isolate the orientation, was not made.")
    d['differences'] = [dict(reference=suffix, run='periodic', side='reference', type='BP', index=1, cause=p1_cause) for suffix in ('.xppaut', '.auto07p')]
    result = [('P1', d, ['periodic'], True), ('E1', closure(d, ['steady']), ['steady'], True)]
    mdb = description('ode_motoneuron6c')
    p2 = closure(mdb, ['periodic'])
    # A common parameter-bound window on the ascending cycles, before the
    # legacy spurious BP/failure near .6211; it is not an NMX comparison.
    p2['runs'][-1]['auto'].update(parameter_max=.62, max_steps=100000)
    result += [('P2', p2, ['periodic'], True), ('E2', closure(mdb, ['steady']), ['steady'], True)]
    for n in (50, 100, 200, 350):
        model = models / ('E3-%d.ode' % n)
        model.write_text("par p=0\nx[1..%d]'=-x[j]+p\ndone\n" % n)
        convert(a, model)
        e3 = description('tstauto_bigaut')
        e3['model'] = str(model.with_suffix('.odex'))
        result.append(('E3-%d' % n, e3, ['steady'], True))
    blocked = description('ode_mu26_blocked')
    result.append(('T1', closure(blocked, ['hb_soma']), ['hb_soma'], True))
    t2 = closure(mdb, ['hb_up', 'hb_down'])
    for r in t2['runs'][1:]:
        r['parameters'] = ['CaDen_Max', 'ampPar']
        # DS is arclength of the extended system, not a density increment.
        # Preserve W431's +/-0.1 and DSMAX .5; bound the density in both directions.
        r['auto'].update(parameter_min=0, parameter_max=.000336)
    # The former traces belong to another parameter order/window. Fresh
    # discrepancies are reported by the pilot, never silently transferred;
    # W456 traced the one this window's pilot reported.
    t2['differences'] = [dict(reference='.auto07p', run='hb_down', side='reference', type='GH', index=i, cause=(
        "AUTO-07p labels five GH on the arm that runs to ampPar 0.82 (its hb_up, our hb_down: opposite sign convention), at ampPar 0.739 to 0.795 "
        "(CaDen_Max 9.7e-5 to 3.8e-5). An independent first Lyapunov coefficient (the model's Fortran right-hand side, quadruple precision, "
        "Kuznetsov's formula) is +8.5e-4 to +1.9e-3 at all 89 points of the arm and +9.1e-4 to +1.25e-3 at the five labels, and agrees with ours "
        "to 1.5e-4: no sign change. The cause is finding 2's (docs/auto07p-findings.md): with EPSL = EPSU = EPSS 1e-7 and 1e-9 AUTO-07p labels none "
        "(W456, docs/auto-references.md).")) for i in range(1, 6)]
    result.append(('T2', t2, ['hb_up', 'hb_down'], True))
    for name in ('ode_mu26_control', 'ode_mu26_blocked', 'ode_ma23'):
        d = description(name)
        result.append((name, d, [r['name'] for r in d['runs']], True))
    for name in ('tstauto_lecar', 'tstauto_simplefold', 'tstauto_tsthom', 'izh_class1_integrator', 'izh_class2', 'izh_resonator'):
        d = description(name)
        result.append(('V1-' + name if name.startswith('tstauto') else name, d, [r['name'] for r in d['runs']], False))
    if a.check:
        restart = copy.deepcopy(next(d for name, d, _, _ in result if name == 'E3-50'))
        restart['runs'][0]['auto']['max_steps'] = SMOKE_POINTS
        restart = repeats_description(restart, ['steady'], 2)
        result.append(('V1-E1-restart', restart, [r['name'] for r in restart['runs']], False))
        fixed = description('tstauto_simplefold')
        for r in fixed['runs']:
            r['auto'].update(min_step_size=abs(r['auto']['step_size']), max_step_size=abs(r['auto']['step_size']), step_adapt_every=0)
        result.append(('V1-fixed', fixed, ['steady'], False))
    for name, d, targets, timed in result:
        if not selected(a, name):
            continue
        source = REPO / d['model']
        # Native models are restaged too; the same model serves all programs.
        if not source.is_absolute():
            source = source.resolve()
        ode = source.with_suffix('.ode')
        if ode.exists() and source.parent != models:
            folder = models / name
            folder.mkdir()
            target = folder / ode.name
            shutil.copy2(ode, target)
            convert(a, target)
            d['model'] = str(target.with_suffix('.odex'))
        if a.check and not a.plan and not a.pilots:
            # All front ends, generator, saved rows and gate, at the smallest
            # meaningful workload. Explicit --only selects additional short rows.
            if not a.only and name not in ('V1-tstauto_simplefold', 'V1-E1-restart', 'V1-fixed'):
                continue
            for r in d['runs']:
                if r['start'].get('state') == 'current':
                    r['start'].pop('settle', None)
                    r['start'].pop('step', None)
            if name.startswith(('V1-', 'E3-')):
                for r in d['runs']:
                    r['auto']['max_steps'] = SMOKE_POINTS
            d.pop('differences', None)
        if name in ('P1', 'E1'):
            o = Odex(REPO / d['model'])
            d['parameters'] = dict(o.pars)
            d['initial_conditions'] = dict(o.inits)
        yield name, d, targets, timed


def native(a, d, folder, threads, events, jacobian, measured):
    folder.mkdir(parents=True)
    records, digests = [], {}
    ready = {}

    class Observer:
        def created(self, server):
            self.clock = RunClock(server) if measured else None

        def ready(self, server):
            if measured:
                ready.update(wall_seconds=time.monotonic()-server.started_at, cpu_seconds=process_cpu(server.proc.pid))

        def finished(self, server, run, emitted, ranges):
            if not events and any(e.get('ev') == 'auto.diagram' for e in emitted):
                raise RuntimeError('no-diagram row still received diagram events')
            metric = self.clock.rows[-1] if measured else {}
            lo, hi = ranges[run['name']]
            records.append(dict(run=run['name'], points=hi-lo, **metric))
            if events:
                digests[run['name']] = hashlib.sha256(json.dumps(old_events.points(emitted)).encode()).hexdigest()

    data, _, threads_auto, ranges = comparison.compute(str(a.xppautx), d, threads,
        env=environment(folder / 'home'), run=str(folder), diagram_data=events, jacobian=jacobian, observer=Observer())
    (folder / 'diagram.csv').write_bytes(data)
    save(folder / 'run-ranges.json', ranges)
    save(folder / 'w269-event-digests.json', digests)
    return dict(data=data, ranges=ranges, ready=ready, runs=records, digests=digests, threads_auto=threads_auto)


def foreign(a, d, program, inputs, folder, measured, exact=None):
    folder.mkdir(parents=True)
    args = SimpleNamespace(auto_dir=str(a.auto_dir), xppaut=str(a.xppaut), keep=folder / 'files')
    args.keep.mkdir()
    measurement = {} if measured else None
    env = environment(folder / 'home')
    env['OMP_NUM_THREADS'] = os.environ.get('OMP_NUM_THREADS', '1')
    value = reference.reference('series', d, program, args, inputs, measurement,
            exact, process_env=env)
    save(folder / 'reference.json', value)
    save(folder / 'measurement.json', measurement)
    return comparison.checked_reference(value, reference.SUFFIX[program], d), measurement


def gate(name, d, native_result, refs):
    notes = []
    diffs, measures = comparison.compare_references(name, d, native_result['data'], native_result['ranges'], refs, notes)
    unavailable = [{"program": suffix, "run": r['name'], "reason": r['unavailable']} for suffix, ref in refs.items() for r in ref['runs'] if 'unavailable' in r]
    if name in ('P1', 'P2'):
        rows = comparison.stored_rows(native_result['data'], native_result['ranges'])
        for r in d['runs']:
            if r['kind'] != 'periodic':
                continue
            a = r['auto']
            endpoints = [('xppautX', float(rows[r['name']][-1]['continued_parameter']))]
            endpoints += [(suffix, run['curve'][-1][0]) for suffix, ref in refs.items() for run in ref['runs']
                          if run['name'] == r['name'] and 'unavailable' not in run and run.get('curve')]
            for program, value in endpoints:
                if a['parameter_min'] < value < a['parameter_max']:
                    diffs.append('%s %s/%s ended inside the parameter bounds at %s; not the requested bounded workload' % (name, program, r['name'], value))
    unexpected = [u for u in unavailable if not (u['program'] == '.xppaut' and u['run'] in ('lpc_soma', 'lpc_dend'))]
    return dict(passed=not diffs and not unexpected, differences=diffs, notes=notes, measures=measures, unavailable=unavailable,
                traced_differences=d.get('differences', []), scope='special points plus both directions of every whole branch')


def pilot_gate(a, name, mode, d, initial, refs, out, tag):
    """gate() of the pilots, kept in the attempt's folder under a key of everything it reads (the description, xppautX's
    pilot and the references); an attempt reusing the pilots takes the earlier result for the same key (#525)."""
    key = hashlib.sha256(json.dumps([name, d, hashlib.sha256(initial['data']).hexdigest(), initial['ranges'], refs], sort_keys=True).encode()).hexdigest()
    for folder in [out] + ([a.reuse / name / mode] if a.reuse else []):
        kept = folder / ('pilot-gate-%s.json' % tag)
        if kept.exists() and json.loads(kept.read_text())['key'] == key:
            return json.loads(kept.read_text())['result']
    result = gate(name, d, initial, refs)
    save(out / ('pilot-gate-%s.json' % tag), dict(key=key, result=result))
    return result


# W457 (#520): the exact rows' gate. XPPAUT and AUTO-07p difference the right-hand side with a step of 1e-7*(1+max|u|), too large
# for states of 1e-5 to 1e-22 (docs/auto-references.md W455), so a row with the exact Jacobian is not judged against them. It is
# equal work when its special points are xppautX's finite-difference pilot's, in order, and its values are within the gate's
# tolerances of a converged run of the same description. Both follow from one comparison with that run (compare_references,
# the rows as `ours` or as the reference) and one check of the run itself against the pilot (`signature`).

def start_artefact(r, i):
    # W458 (#521): a test function that changes sign between the first two points after a Hopf start labels a point that is
    # nothing (xppautX's BP and XPPAUT's, AUTO-07p's LP on P2's cycles, none of them at tighter EPS and DS): the stored
    # points 2 and 3 of a branch of cycles started at HB are not compared.
    return r['kind'] == 'periodic' and comparison.code_of(r['start'].get('type', '')) == 'HB' and i in (1, 2)


def tightened(d):
    out = copy.deepcopy(d)
    out.pop('differences', None)
    for r in out['runs']:
        a = r['auto']
        for k in ('parameter_tolerance', 'state_tolerance', 'special_point_tolerance'):
            a[k] = a.get(k, reference.DEFAULTS[reference.AUTO_NAMES[k]]) / TIGHTENING
        for k in ('step_size', 'min_step_size', 'max_step_size'):
            a[k] /= REFINEMENT
        a['max_steps'] *= REFINEMENT
    return out


def signature(d, data, ranges):
    rows = comparison.stored_rows(data, ranges)
    return {r['name']: [s for s, i in comparison.labelled(rows[r['name']], r['kind']) if not start_artefact(r, i)] for r in d['runs'] if r['name'] in rows}


def reference_of(d, data, ranges):
    """A diagram as the runs of a reference file (tools/autocompare.py read_reference), what compare_references reads of
    another program: the special points (spectrum values as the diagram stores them: multipliers), and every point's
    [parameter, second parameter, norm]."""
    rows = comparison.stored_rows(data, ranges)
    runs = []
    for r in d['runs']:
        if r['name'] not in rows:
            continue
        curve = r['kind'] in ('two_parameter', *comparison.HOMCONT)
        points = [dict(type=symbol, point=i + 1, par=comparison.quantity(rows[r['name']][i], 'par'), norm=comparison.quantity(rows[r['name']][i], 'norm'),
                       period=comparison.quantity(rows[r['name']][i], 'period'), spectrum='multiplier',
                       values=[list(v) for v in comparison.spectrum(rows[r['name']][i])] if i else [],
                       **({'par2': comparison.quantity(rows[r['name']][i], 'par2')} if curve else {}))
                  for symbol, i in comparison.labelled(rows[r['name']], r['kind'])]
        samples = [[comparison.quantity(row, 'par'), comparison.quantity(row, 'par2') if 'continued_parameter2' in row else 0.0, comparison.quantity(row, 'norm')]
                   for row in rows[r['name']] if not curve or 'continued_parameter2' in row]
        runs.append(dict(name=r['name'], points=points, curve=samples))
    return dict(runs=runs)


def converged_pilot(a, d, out, initial):
    """The converged run of the description (untimed, one more pilot), what it is judged by, and whether its special points are
    xppautX's finite-difference pilot's."""
    run = native(a, tightened(d), out / 'pilot-xppautx-converged', 1, True, 'exact', measured=False)
    run['reference'] = reference_of(d, run['data'], run['ranges'])
    want, got = signature(d, initial['data'], initial['ranges']), signature(d, run['data'], run['ranges'])
    run['mismatch'] = None if want == got else 'special points of the converged exact run %s, of the finite-difference pilot %s' % (got, want)
    return run


def converged_compare(name, d, data, ranges, suffix, ref):
    """(differences, measures) of `ref` against the converged run, the stored points that W458 leaves out dropped from both:
    `data` is the side that is `ours` for compare_references and `ref` the other (a row of xppautX against the converged run's
    reference form, or another program's row against the converged run's data)."""
    rows = comparison.stored_rows(data, ranges)
    by_name = {r['name']: r for r in d['runs']}
    seen = {}
    known = []
    for r in d['runs']:
        for symbol, i in comparison.labelled(rows[r['name']], r['kind']) if r['name'] in rows else ():
            seen[r['name'], symbol] = seen.get((r['name'], symbol), 0) + 1
            if start_artefact(r, i):
                known.append(dict(reference=suffix, run=r['name'], side='ours', type=symbol, index=seen[r['name'], symbol], cause='W458'))
    ref = dict(ref, runs=[dict(r, points=[p for p in r['points'] if not start_artefact(by_name[r['name']], abs(p['point']) - 1)]) for r in ref['runs']])
    return comparison.compare_references(name, dict(d, differences=known), data, ranges, {suffix: ref})


def converged_sides(d, converged, program, folder):
    """The arguments of converged_compare for a row's folder (its saved output): a row of xppautX is `ours` against the converged
    run in reference form; another program's row is the reference to the converged run's data."""
    if program == 'xppautx':
        return ((folder / 'diagram.csv').read_bytes(), json.loads((folder / 'run-ranges.json').read_text()), CONVERGED,
                repeat_references(d, {CONVERGED: converged['reference']})[CONVERGED])
    return (converged['data'], by_original(d, converged['ranges']), reference.SUFFIX[program],
            saved_reference(folder, program, given(d, program)))


def given(d, program):
    """The description a program is given: XPPAUT is not given the limit-point-of-cycles runs, past 600 s each (W436/W437)."""
    return dict(d, runs=[r for r in d['runs'] if not r['name'].startswith('lpc_')]) if program == 'xppaut' else d


def exact_gate(name, d, converged, program, folder):
    """The gate of a row with the exact Jacobian, from its folder (a fresh row's, an imported row's, a pilot's)."""
    sides = converged_sides(d, converged, program, folder)
    diffs, measures = converged_compare(name, d, *sides)
    unavailable = [{"program": sides[2], "run": r['name'], "reason": r['unavailable']} for r in sides[3]['runs'] if 'unavailable' in r]
    return dict(passed=not diffs and not unavailable and converged['mismatch'] is None, differences=([converged['mismatch']] if converged['mismatch'] else []) + diffs,
                notes=[], measures=measures, unavailable=unavailable, traced_differences=[],
                scope='exact Jacobian: special points of the finite-difference pilot, values within the gate\'s tolerances of the converged exact run')


def by_original(d, ranges):
    return {r['name']: ranges[original_run(r['name'])] for r in d['runs'] if 'skip' not in r}


def exact_equations(a, name, d, eqs, start, rhs, out):
    """The equation file AUTO-07p's exact rows run: PY_S1Bf's by hand, or the model's generated (W459) and checked against
    central differences and the Fortran right-hand side; None where AUTO-07p has no exact row."""
    if name not in EXACT_AUTO07P:
        return None
    if name in ('P1', 'E1'):
        return HAND_WRITTEN
    check = exact_source.check(exact_source.source(eqs, start, d), eqs, d, out / 'exact', a.auto_dir, rhs)
    if not check['passed']:
        raise RuntimeError('the generated derivatives of %s fail their check: %s' % (name, out / 'exact/exact-check.json'))
    return out / 'exact/exact.c'


NO_ERROR = dict(by_quantity={}, ratio=0.0)


def worse(a, b):
    return dict(by_quantity={q: max(a['by_quantity'].get(q, 0.0), b['by_quantity'].get(q, 0.0)) for q in a['by_quantity'].keys() | b['by_quantity'].keys()},
                ratio=max(a['ratio'], b['ratio']))


def converged_errors(name, d, converged, program, folder):
    """{run: {'by_quantity': the worst difference of each quantity, 'ratio': the worst difference over its tolerance}} of a row
    (its folder's output) against the converged run, whatever the row's gate: its error beside its time (W445). A run's
    includes those of the runs it started from."""
    _, measures = converged_compare(name, d, *converged_sides(d, converged, program, folder))
    own = {}
    for _, run, quantity, _, diff, tolerance in measures:
        own[original_run(run)] = worse(own.get(original_run(run), NO_ERROR), dict(by_quantity={quantity: diff}, ratio=diff / tolerance))
    # a run starts from a point of the run before it (a Hopf point's period moves its cycles): its error includes theirs. Only
    # the original runs: a repeat restarts its own branch (E1, T1), which would make the chain a cycle
    starts = {r['name']: r['start'].get('run') for r in d['runs'] if r['name'] == original_run(r['name'])}
    errors = {}
    for run in own:
        errors[run], source = NO_ERROR, run
        while source:
            errors[run], source = worse(errors[run], own.get(source, NO_ERROR)), starts.get(source)
    return errors


def reused_errors(a, name, mode, d, converged, recs):
    """The errors an imported row's records already carry (converged_error, converged_error_ratio: with_error), in the shape
    converged_errors gives; None unless the earlier attempt measured against the very converged run of this description
    (its converged.json: the same tightened description and mismatch, the runs being the same program's), so recomputing
    would give the same numbers."""
    earlier = earlier_mode_file(a, name, mode, 'converged.json')
    if not earlier or 'errors' not in earlier or earlier['description'] != tightened(d) or earlier['mismatch'] != converged['mismatch']:
        return None
    return {r['workload'].rsplit('/', 1)[1]: dict(by_quantity=r['converged_error'], ratio=r['converged_error_ratio'])
            for r in recs if r['row'] == 'run' and 'converged_error' in r}


def with_error(record, errors):
    if record['row'] != 'run' or record['workload'].rsplit('/', 1)[1] not in errors:
        return record
    e = errors[record['workload'].rsplit('/', 1)[1]]
    return dict(record, converged_error=e['by_quantity'], converged_error_ratio=e['ratio'])


def repeats_description(d, targets, count):
    out = copy.deepcopy(d)
    for i in range(1, count):
        for r in d['runs']:
            if r['name'] in targets:
                extra = copy.deepcopy(r)
                extra['name'] += '-repeat-%d' % i
                if r['start'].get('state') == 'current':
                    extra['start'] = dict(run=r['name'], type='EP', index=1)
                out['runs'].append(extra)
                out.setdefault('differences', []).extend(dict(k, run=extra['name']) for k in d.get('differences', []) if k['run'] == r['name'])
    return out


def original_run(name):
    return name.split('-repeat-', 1)[0]


def repeat_references(d, refs):
    result = {}
    for suffix, ref in refs.items():
        by_name = {r['name']: r for r in ref['runs']}
        result[suffix] = dict(ref, runs=[dict(by_name[original_run(r['name'])], name=r['name']) for r in d['runs'] if 'skip' not in r])
    return result


def aggregate(result, targets):
    out = {}
    for name in targets:
        runs = [r for r in result['runs'] if r['run'] == name or r['run'].startswith(name + '-repeat-')]
        if not runs:
            continue
        points = sum(r.get('points', 0) for r in runs)
        row = dict(points=points, branches=len(runs))
        if all('wall_seconds' in r for r in runs):
            row.update(wall_seconds=sum(r['wall_seconds'] for r in runs), cpu_seconds=sum(r['cpu_seconds'] for r in runs))
            if points:
                row.update(wall_per_point=row['wall_seconds']/points, cpu_per_point=row['cpu_seconds']/points)
        out[name] = row
    return out


def variants(name, timed, check):
    if check or not timed:
        return [('xppautx-fd-1-diagram', 'xppautx', 1, True, 'difference'), ('xppaut-fd', 'xppaut', 1, False, 'difference'), ('auto07p-fd-1', 'auto07p', 1, False, 'difference')]
    rows = []
    for jac, label in (('difference', 'fd'), ('exact', 'exact')):
        for threads in (1, 2, 4, 'auto'):
            for events in (True, False):
                rows.append(('xppautx-%s-%s-%s' % (label, threads, 'diagram' if events else 'none'), 'xppautx', threads, events, jac))
    rows += [('xppaut-fd', 'xppaut', 1, False, 'difference')]
    rows += [('auto07p-fd-%d' % n, 'auto07p', n, False, 'difference') for n in (1, 2, 4)]
    if name in EXACT_AUTO07P:
        rows += [('auto07p-exact-1', 'auto07p', 1, False, 'exact')]
    return rows


def digest_history(name, result, folder):
    history = []
    for root in (BENCH / 'auto/results', BENCH / 'modeldb-267710/results'):
        for path in root.glob('*w269*/runs.json'):
            raw = json.loads(path.read_text())
            for row in raw if isinstance(raw, list) else raw.get('runs', []):
                for run, v in row.items():
                    if isinstance(v, dict) and 'digest' in v:
                        history.append(dict(source=str(path.relative_to(BENCH)), run=run, digest=v['digest']))
    save(folder / 'w269-digest-comparison.json', dict(workload=name, current=result['digests'], historical=history,
         comparisons={r: dict(matches=[h for h in history if h['run'] == r and h['digest'] == digest],
                              changed_or_different_window=not any(h['run'] == r and h['digest'] == digest for h in history),
                              status='reported; inspect docs/auto-references.md W302, W421, W425, W436-W438 before attributing a changed digest')
                      for r, digest in result['digests'].items()},
         note='W269 used SMT siblings for thread rows. Its old tip is history and is never timed again.'))


# W454 (#517): a program whose code, inputs and options did not change is not run again. A row's key names what its result
# depended on; an earlier attempt's row is imported (its records, its run folder linked) only when every part is equal.
# xppautX's rows depend on tools/autocompare.py's driver and xppclient's Server; XPPAUT's and AUTO-07p's on
# tools/auto_reference.py's own program driver, so a change to the xppautX driver is never a change to theirs.
TRACKED = {'xppautx': (('tools/autocompare.py', ('compute',)), ('tools/xppclient.py', ('Server',)), ('tools/comparison_measure.py', None)),
           'xppaut': (('tools/auto_reference.py', ('reference', 'execute_xppaut')), ('tools/comparison_measure.py', None)),
           'auto07p': (('tools/auto_reference.py', ('reference', 'execute_auto07p')), ('tools/comparison_measure.py', None))}


@functools.cache
def measure_key(engine, commit=None):
    # The measurement code: the tracked files' reachable parts (as of `commit`, or now) and this driver's own function.
    tracked = [(path, (hashlib.sha256(tracked_text(path, commit).encode()).hexdigest() if roots is None
                       else reachable_sha(tracked_text(path, commit), roots)).encode()) for path, roots in TRACKED[engine]]
    return tree_sha([('tracked', tree_sha(tracked).encode()), ('local', measure_sha(native if engine == 'xppautx' else foreign).encode())])


def description_sha(d, engine):
    # What a program is asked to do: the description with the model's contents in place of where this attempt put it.
    d = given(copy.deepcopy(d), engine)
    d['model'] = sha(d['model'])
    return hashlib.sha256(json.dumps(d, sort_keys=True).encode()).hexdigest()


def start_values(data):
    first = comparison.diagram_rows(data)[0]
    return [float(first[k]) for k in first if k.startswith('u0.')]


def saved_reference(folder, program, d):
    """A row's saved reference, checked against the description. Its model's path names the attempt that wrote it, and a row
    reused from another attempt has the same model only by its key (the model's contents): the path checked is this one's."""
    ref = json.loads((folder / 'reference.json').read_text())
    return comparison.checked_reference(dict(ref, model=d['model']), reference.SUFFIX[program], d)


def reference_digest(ref):
    # A foreign row's result without the model's path, which names the attempt's folder: a row reused from another attempt
    # was otherwise never equal to a fresh one (series 22h, 2026-10-23)
    return hashlib.sha256(json.dumps({k: v for k, v in ref.items() if k != 'model'}, sort_keys=True).encode()).hexdigest()


def row_key(a, engine, d, start, row, cpus):
    key = dict(program=executable_sha(a, engine), measure=measure_key(engine), description=description_sha(d, engine), row=row, cpus=list(cpus))
    if engine != 'xppautx':
        key['start'] = start
    if engine == 'auto07p' and row.get('jacobian') == 'exact' and Path(d['model']).stem != 'PY_S1Bf':
        key['exact_source'] = sha(exact_source.__file__)  # W459: the generator makes the equation file (PY_S1Bf's is by hand, in no key before)
    return key


def derived_row_key(a, engine, earlier_d, earlier_start, row, cpus):
    # The key of an earlier row that recorded none (a series before W454), from what the earlier attempt kept: its
    # provenance (binaries, commit), its description, the start its pilot found; the tracked code as of its commit and this
    # driver's own functions as they are (git-ignored: taken as unchanged, as W452 took common.py).
    commit = earlier_context(a)['provenance']['commit']
    key = dict(program=derived_program_sha(a, engine), measure=measure_key(engine, commit), description=description_sha(earlier_d, engine),
               row=row, cpus=list(cpus), derived=True)
    if engine != 'xppautx':
        key['start'] = earlier_start
    return key


def earlier_mode_file(a, name, mode, file):
    path = a.reuse / name / mode / file
    return json.loads(path.read_text()) if path.exists() else None


def earlier_start(a, name, mode):
    path = a.reuse / name / mode / 'pilot-xppautx' / 'diagram.csv'
    return start_values(path.read_bytes()) if path.exists() else None


def earlier_key(a, name, mode, engine, recorded, description_file, row, cpus):
    # The recorded key, else the one derived from the earlier attempt's files; None when there is nothing to derive it from.
    if recorded:
        return recorded
    window = earlier_mode_file(a, name, mode, description_file)
    start = earlier_start(a, name, mode)
    if window is None or start is None:
        return None
    return derived_row_key(a, engine, window['description'] if description_file == 'window.json' else window, start, row, cpus)


def key_differences(have, key):
    return [k for k in key if have is None or have.get(k) != key[k]] if have is not None else ['everything: no earlier key']


def reusable_row(a, name, mode, rep, label, engine, key, row, cpus):
    # (records, folder) of this row of an earlier attempt when it ran valid equal work and its key equals this run's; else
    # (None, why). Imported means the records are copied (marked reused_from) and the run folder linked.
    prefix = name + '/' + mode
    recs = [r for r in earlier_context(a)['records'] if r['engine'] == label and r['repetition'] == rep
            and (r['workload'] == prefix or r['workload'].startswith(prefix + '/'))]
    folder = a.reuse / name / mode / 'runs' / str(rep) / label
    runs = [r for r in recs if r['row'] == 'run']
    if not runs or not (folder / 'gate.json').exists():
        return None, 'no earlier row'
    if not all(r.get('valid') and r.get('equal_work') for r in runs):
        return None, 'the earlier row was not valid equal work'
    have = earlier_key(a, name, mode, engine, next((r['reuse_key'] for r in runs if r.get('reuse_key')), None), 'window.json', row, runs[0]['pinned_cpus'])
    if not same_key(have, key):
        return None, 'differs in ' + ', '.join(key_differences(have, key))
    return recs, folder


def reusable_pilot(a, name, mode, program, key):
    folder = a.reuse / name / mode / ('pilot-' + program)
    if not (folder / 'reference.json').exists():
        return None, 'no earlier pilot'
    recorded = folder / 'reuse-key.json'
    have = earlier_key(a, name, mode, program, json.loads(recorded.read_text()) if recorded.exists() else None, 'description.json', key['row'], key['cpus'])
    return (folder, None) if same_key(have, key) else (None, 'differs in ' + ', '.join(key_differences(have, key)))


def plan_reuse(a, name, mode, d, targets, timed, initial, start, phase):
    # --plan (a dry run): what an earlier attempt would supply of this workload and what would run, decided by the same
    # keys and checks the series uses. Nothing but xppautX's pilot has run.
    for program in ('xppaut', 'auto07p'):
        rd = copy.deepcopy(d)
        key = row_key(a, program, rd, start, dict(label='pilot-' + program, measured=True), a.cores[:1])
        earlier, why = reusable_pilot(a, name, mode, program, key)
        phase('plan: %s pilot %s' % (program, 'imported' if earlier else 'runs: ' + why))
    sized = earlier_mode_file(a, name, mode, 'window.json')
    count = sized['branch_repetitions'] if name in ('E1', 'T1') and sized else 1
    run_d = repeats_description(d, targets, count)
    for label, program, threads, events, jac in variants(name, timed, False):
        cpus = a.cores[:initial['threads_auto'] if threads == 'auto' else threads]
        row = dict(label=label, threads=threads, jacobian=jac, events=events, measured=True)
        key = row_key(a, program, run_d, start, row, cpus)
        found = [reusable_row(a, name, mode, rep, label, program, key, row, cpus) for rep in range(a.repeats + 1)]
        reasons = sorted({why for recs, why in found if not recs})
        phase('plan: row %s: %d of %d repetitions imported%s' % (label, sum(1 for recs, _ in found if recs), a.repeats + 1, '; runs: ' + '; '.join(reasons) if reasons else ''))


def workload(a, name, d, targets, timed, records, mode='adaptive', branch_repetitions=1):
    out = a.out / name / mode
    out.mkdir(parents=True)
    save(out / 'description.json', d)
    original_affinity = os.sched_getaffinity(0)
    os.sched_setaffinity(0, a.cores[:1])
    try:
        began = time.monotonic()

        def phase(text):
            progress(a, '%s/%s: %s (%.0f s)' % (name, mode, text, time.monotonic() - began))

        def stop(g):
            save(out / 'gate.json', g)
            progress(a, 'NOT EQUAL WORK; measurements omitted: %s %s (%d differences, details in %s)' % (name, mode, len(g['differences']), out / 'gate.json'))
            for line in g['differences'][:3]:
                progress(a, '    ' + line[:240])
            return False

        phase('our pilot starts')
        initial = native(a, d, out / 'pilot-xppautx', 1, True, 'difference', measured=not a.check and timed)
        phase('our pilot finished')
        odex = Odex(REPO / d['model'])
        eqs = reference.Equations(odex, d.get('parameters', {}))
        start = start_values(initial['data'])
        inputs = odex, eqs, start, False
        if a.plan:
            plan_reuse(a, name, mode, d, targets, timed, initial, start, phase)
            return True
        rhs = reference.check_rhs(a.xppautx, eqs, start, out / 'rhs')
        phase('RHS check %s' % ('passed' if rhs['passed'] else 'FAILED'))
        digest_history(name, initial, out)
        if not rhs['passed']:
            return stop(dict(passed=False, differences=['%s: the right-hand side check failed, see %s' % (name, out / 'rhs')], notes=[], measures=[],
                             unavailable=[], traced_differences=[], rhs=rhs, scope='stopped after the right-hand side check'))
        refs, pilots = {}, {}
        for program in ('xppaut', 'auto07p'):
            rd = given(copy.deepcopy(d), program)
            measured = not a.check and timed
            pilot_key = row_key(a, program, rd, start, dict(label='pilot-' + program, measured=measured), a.cores[:1])
            earlier, why = reusable_pilot(a, name, mode, program, pilot_key) if a.reuse else (None, 'no --reuse')
            if earlier:
                link_tree(earlier, out / ('pilot-' + program))
                ref, metric = saved_reference(earlier, program, rd), json.loads((earlier / 'measurement.json').read_text())
                phase('%s pilot REUSED from %s' % (program, a.reuse))
            else:
                phase('%s pilot starts%s' % (program, ' (not reused: %s)' % why if a.reuse else ''))
                ref, metric = foreign(a, rd, program, inputs, out / ('pilot-' + program), measured)
                save(out / ('pilot-' + program) / 'reuse-key.json', pilot_key)
                phase('%s pilot finished' % program)
            if program == 'xppaut':
                for r in d['runs']:
                    if r['name'].startswith('lpc_'):
                        ref['runs'].append(dict(name=r['name'], parameters=r['parameters'], points=[], unavailable='past 600 s (W436/W437); deliberately not repeated in this series'))
            refs[reference.SUFFIX[program]] = ref
            pilots[program] = metric
            if program == 'xppaut':
                # a reference's differences do not depend on the other: stop before the next pilot when this one already fails
                phase('gate against %s starts' % program)
                g = pilot_gate(a, name, mode, d, initial, refs, out, 'xppaut')
                g['rhs'] = rhs
                if not g['passed']:
                    return stop(g)
        phase('gate starts')
        g = pilot_gate(a, name, mode, d, initial, refs, out, 'both')
        g['rhs'] = rhs
        g['passed'] &= rhs['passed']
        phase('gate finished')
        if not g['passed']:
            return stop(g)
        converged, exact_c = None, None
        if timed:
            # W457/W459: the converged exact run that judges the exact rows and measures every row's error (not for the fixed
            # step, whose step is the workload), and the exact rows' own pilots, untimed, against it
            exact_c = exact_equations(a, name, d, eqs, start, rhs, out)
            if mode != 'fixed':
                phase('converged exact pilot starts')
                try:
                    converged = converged_pilot(a, d, out, initial)
                    phase('converged exact pilot finished%s' % (': ' + converged['mismatch'] if converged['mismatch'] else ''))
                except RuntimeError as e:
                    # tight tolerances and short steps can fail where the description's do not (cycles near a homoclinic orbit):
                    # no converged run, so the exact rows keep the gate of the finite-difference programs
                    save(out / 'converged.json', dict(unavailable=str(e)))
                    phase('converged exact pilot UNAVAILABLE: %s' % str(e)[:300])
            if converged:
                native(a, d, out / 'pilot-xppautx-exact', 1, True, 'exact', measured=False)
                gates = {'xppautx-exact': exact_gate(name, d, converged, 'xppautx', out / 'pilot-xppautx-exact')}
                if exact_c:
                    foreign(a, d, 'auto07p', inputs, out / 'pilot-auto07p-exact', False, exact=exact_c)
                    gates['auto07p-exact'] = exact_gate(name, d, converged, 'auto07p', out / 'pilot-auto07p-exact')
                errors = {'xppautx-fd': converged_errors(name, d, converged, 'xppautx', out / 'pilot-xppautx'),
                          'xppaut-fd': converged_errors(name, d, converged, 'xppaut', out / 'pilot-xppaut'),
                          'auto07p-fd': converged_errors(name, d, converged, 'auto07p', out / 'pilot-auto07p'),
                          **{label: converged_errors(name, d, converged, label.split('-')[0], out / ('pilot-' + label)) for label in gates}}
                save(out / 'converged.json', dict(mismatch=converged['mismatch'], description=tightened(d), exact_pilots=gates, errors=errors))
                for label, g_exact in gates.items():
                    phase('%s pilot against the converged run: %s' % (label, 'equal work' if g_exact['passed'] else 'NOT EQUAL WORK: ' + '; '.join(g_exact['differences'])[:300]))
        if a.check or not timed:
            save(out / 'completion.json', dict(passed=True, timed=False))
            return True
        # E1 and T1 repeat the branch inside each fresh session. T2 grows its
        # point count by untimed pilots; no pilot contributes a measured row.
        count = branch_repetitions
        sized = earlier_mode_file(a, name, mode, 'window.json') if a.reuse else None
        if name in ('E1', 'T1') and sized:
            # the earlier attempt's window, so programs not run again compare equal work with those run now
            count = sized['branch_repetitions']
        elif name in ('E1', 'T1'):
            fastest = min(r['wall_seconds'] for r in initial['runs'] if r['run'] in targets)
            count = max(count, math.ceil(MIN_WINDOW / fastest))
        if name == 'T2':
            fastest = min(r['wall_seconds'] for r in initial['runs'] if r['run'] in targets)
            if fastest < MIN_WINDOW and d['runs'][-1]['auto']['max_steps'] < T2_SIZED_POINTS:
                larger = copy.deepcopy(d)
                old = larger['runs'][-1]['auto']['max_steps']
                new = T2_SIZED_POINTS
                larger['runs'][-1]['auto']['max_steps'] = new
                # Keep both physical parameter windows fixed while taking more steps.
                for r in larger['runs']:
                    if r['name'] in targets:
                        r['auto']['max_steps'] = new
                        for key in ('step_size', 'max_step_size', 'min_step_size'):
                            r['auto'][key] *= old / new
                # AUTO-07p's spurious GH (finding 2) come from the Newton residual, so their count follows the step: at the doubled
                # points and halved steps it labels four, not five (series 22e, 2026-10-22); the traced entries follow it
                larger['differences'] = [k for k in larger.get('differences', []) if not (k['type'] == 'GH' and k['index'] > T2_SIZED_GH)]
                save(out / 'pilot-sizing.json', dict(old_points=old, chosen_points=new, pilot_wall=fastest))
                return workload(a, name, larger, targets, timed, records, mode + '-sized')
        run_d = repeats_description(d, targets, count)
        save(out / 'window.json', dict(branch_repetitions=count, nominal_min_seconds=MIN_WINDOW, description=run_d))
        engines = variants(name, timed, a.check)
        required = 4
        if len(a.cores) < required:
            raise ValueError('settled thread rows require four distinct physical cores in the machine record')
        before_series(a)
        baselines, eligibility, row_errors = {}, {}, {}
        shortest = None
        for rep in repetitions(a):
            for label, program, threads, events, jac in rotation(engines, rep):
                if rep > 0 and not eligibility[label]:
                    continue
                cpus = a.cores[:initial['threads_auto'] if threads == 'auto' else threads]
                os.sched_setaffinity(0, cpus)
                folder = out / 'runs' / str(rep) / label
                progress(a, '%s/%s: repetition %d of %d, row %s' % (name, mode, rep, a.repeats, label))
                os.environ['OMP_NUM_THREADS'] = str(threads)
                os.environ['OPENBLAS_NUM_THREADS'] = '1'
                exact = jac == 'exact'
                fd = given(copy.deepcopy(run_d), program)
                row = dict(label=label, threads=threads, jacobian=jac, events=events, measured=True)
                key = row_key(a, program, run_d, start, row, cpus)
                recs, source = reusable_row(a, name, mode, rep, label, program, key, row, cpus) if a.reuse else (None, 'no --reuse')
                if recs:
                    link_tree(source, folder)
                    agreement = json.loads((folder / 'gate.json').read_text())
                    digest = (hashlib.sha256((folder / 'diagram.csv').read_bytes()).hexdigest() if program == 'xppautx' else
                              reference_digest(json.loads((folder / 'reference.json').read_text())))
                    progress(a, '%s/%s: row %s REUSED from %s: program, measurement code, inputs and options unchanged' % (name, mode, label, a.reuse))
                    if rep > 0:
                        # The same check as a fresh repetition: against this attempt's repetition 0, which may be fresh or from another attempt
                        agreement = dict(passed=digest == baselines[label])
                    made = [(r['workload'].rsplit('/', 1)[1], r) for r in recs if r['row'] == 'run']
                    new_records = [dict(mark_reused(a, r), reuse_key=key, equal_work=agreement['passed']) for r in recs]
                else:
                    if a.reuse:
                        progress(a, '%s/%s: row %s runs fresh: %s' % (name, mode, label, source))
                    if program == 'xppautx':
                        result = native(a, run_d, folder, threads, events, jac, True)
                        agreement = gate(name, run_d, result, repeat_references(run_d, refs)) if rep == 0 else dict(passed=hashlib.sha256(result['data']).hexdigest() == baselines[label])
                        digest = hashlib.sha256(result['data']).hexdigest()
                    else:
                        ref, result = foreign(a, fd, program, inputs, folder, True, exact=exact_c if exact else None)
                        digest = reference_digest(ref)
                        shared = dict(initial, ranges=by_original(run_d, initial['ranges']))
                        agreement = gate(name, run_d, shared, {reference.SUFFIX[program]: ref}) if rep == 0 else dict(passed=digest == baselines[label])
                        for r in result['runs']:
                            branch = next(v for v in ref['runs'] if v['name'] == r['run'])
                            r['points'] = len(branch.get('curve', []))
                    save(folder / 'gate.json', agreement)
                    made = list(aggregate(result, targets).items())
                    new_records = [dict(workload=name + '/' + mode, engine=label, row='ready', repetition=rep,
                                        valid=result.get('ready') is not None, equal_work=agreement['passed'], reuse_key=key, **(result.get('ready') or {}))]
                    new_records += [dict(workload=name + '/' + mode + '/' + run, engine=label, row='run', repetition=rep,
                                         valid=True, equal_work=agreement['passed'], pinned_cpus=cpus, jacobian=jac, reuse_key=key,
                                         wrapper_wall_seconds=result.get('wrapper_wall_seconds'), **metrics) for run, metrics in made]
                if rep == 0 and exact and converged:
                    # W457: a row with the exact Jacobian is equal work against the converged run, whichever gate its folder was under
                    agreement = exact_gate(name, run_d, converged, program, folder)
                    save(folder / 'gate.json', agreement)
                    new_records = [dict(r, equal_work=agreement['passed']) for r in new_records]
                if rep == 0:
                    baselines[label] = digest
                    eligibility[label] = agreement['passed']
                    if converged:
                        row_errors[label] = (reused_errors(a, name, mode, d, converged, recs) if recs else None) or converged_errors(name, run_d, converged, program, folder)
                if not agreement['passed']:
                    progress(a, 'ROW NOT EQUAL WORK: %s/%s %s repetition %d: %s' % (name, mode, label, rep, '; '.join(agreement.get('differences', ['digest changed from repetition 0']))[:300]))
                for run, metrics in made:
                    if rep == 0 and agreement['passed'] and name in ('E1', 'T1'):
                        shortest = metrics['wall_seconds'] if shortest is None else min(shortest, metrics['wall_seconds'])
                records.extend(with_error(r, row_errors.get(label, {})) for r in new_records)
                save(a.out / 'runs.json', records)
                summarize(records, a.out)
            if rep == 0 and shortest is not None and shortest < MIN_WINDOW:
                # Size all rows from their warm-ups, including exact/parallel
                # rows. Restart excluded warm-ups with the longer common window.
                longer = max(count + 1, math.ceil(count * MIN_WINDOW / shortest))
                save(out / 'repeat-sizing.json', dict(shortest_warmup=shortest, old=count, chosen=longer))
                return workload(a, name, d, targets, timed, records, mode + '-repeat-%d' % longer, longer)
        save(out / 'completion.json', dict(passed=True, timed=True))
        return True
    finally:
        os.sched_setaffinity(0, original_affinity)


def stop_here(a, records, failed, driver_failures, name, why):
    progress(a, 'STOPPED at %s: %s' % (name, why))
    save(a.out / 'completion.json', dict(check=a.check, failed_gates=failed, driver_failures=driver_failures, measured_rows=len(records), stopped_at=name, why=why))
    sys.exit('stopped at %s (--stop-on-failure): %s' % (name, why))


def main():
    p = parser(__doc__)
    p.add_argument('--plan', action='store_true', help='with --reuse and --only: print which rows an earlier attempt supplies and which would run (xppautX\'s pilot only); the full descriptions, no timing')
    p.add_argument('--pilots', action='store_true', help='with --only: the full descriptions\' pilots and gates, the converged and exact pilots included, untimed, then stop (W457)')
    a = p.parse_args()
    if a.plan or a.pilots:
        a.check = True
    initialize(a)
    records, failed, driver_failures = [], [], []
    for name, d, targets, timed in workloads(a):
        try:
            progress(a, 'WORKLOAD %s' % name)
            ok = workload(a, name, d, targets, timed, records)
            if not ok:
                failed.append(name)
                if a.stop_on_failure:
                    stop_here(a, records, failed, driver_failures, name, 'not equal work, see the differences above')
            if name == 'P1' and (a.plan or not a.check):
                fixed = copy.deepcopy(d)
                for r in fixed['runs']:
                    r['auto'].update(min_step_size=abs(r['auto']['step_size']), max_step_size=abs(r['auto']['step_size']), step_adapt_every=0, max_steps=FIXED_POINT_BUDGET)
                progress(a, 'WORKLOAD %s fixed step' % name)
                if not workload(a, name, fixed, targets, timed, records, 'fixed'):
                    failed.append(name + '-fixed')
                    if a.stop_on_failure:
                        stop_here(a, records, failed, driver_failures, name + '-fixed', 'not equal work, see the differences above')
        except Exception as e:
            failed.append(name)
            driver_failures.append(name)
            save(a.out / name / 'failure.json', dict(error=str(e), type=type(e).__name__, traceback=traceback.format_exc()))
            progress(a, 'FAILED %s: %s' % (name, e))
            if a.stop_on_failure:
                stop_here(a, records, failed, driver_failures, name, '%s: %s' % (type(e).__name__, e))
    save(a.out / 'completion.json', dict(check=a.check, failed_gates=failed, driver_failures=driver_failures, measured_rows=len(records)))
    # A refused workload means missing measurements: the study is not complete (it was recorded as complete before).
    if failed:
        sys.exit('refused or failed workloads: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
