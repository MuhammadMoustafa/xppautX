#!/usr/bin/env -S uv run --locked python
"""Integration studies, serial process protocol; reuse W238, W241 and W427."""
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys

from common import (BENCH, INPUT_SUFFIXES, HERE, LONG_RUN_REASON, LONG_RUN_SECONDS, REPO, TOOLS, before_series, progress, command, derived_key, earlier_context, environment,
                    initialize, inputs_sha, is_long, link_tree, mark_reused, measure_sha, module, parser, repetitions, required_repetitions,
                    reuse_key, rotation, save, selected, sha, summarize)

screen = module('screen', BENCH / 'five-model-screen/screen.py')
scaling = module('scaling', BENCH / 'size-scaling/scaling.py')
PYTHON = TOOLS / 'python-integration'
sys.path.insert(0, str(PYTHON))
from odex_model import Odex

from numerics import apply_xppaut_set, legacy_method_key, method_rows, numerics_of


def ode_option(text, line):
    # XPPAUT allows settings after included settings. Insert before the end,
    # preserving the source and the conversion log beside the staged input.
    end = re.search(r'(?im)^\s*(?:done|d)\s*$', text)
    if end is None:
        raise ValueError('staged .ode: no end marker')
    return text[:end.start()] + line + '\n' + text[end.start():]


def convert(a, model):
    p = subprocess.run([str(a.xppautx), '--convert', '--yes', str(model)], capture_output=True, text=True)
    model.with_suffix('.conversion.log').write_text(p.stdout + p.stderr)
    if p.returncode:
        raise RuntimeError('%s: conversion failed: %s' % (model, p.stdout + p.stderr))


def setfile_patch(path, values):
    lines = path.read_text().splitlines()
    found = set()
    for i, line in enumerate(lines):
        parts = line.split(maxsplit=1)
        if len(parts) == 2 and parts[1] in values:
            lines[i] = '%s  %s' % (values[parts[1]], parts[1])
            found.add(parts[1])
    if found != set(values):
        raise ValueError('%s: missing SET labels %s' % (path, set(values) - found))
    path.write_text('\n'.join(lines) + '\n')


def stage(a, study):
    dest = a.out / 'models'
    dest.mkdir()
    if study in ('screen', 'python'):
        bundle = a.out / 'screen-staging'
        shutil.copytree(BENCH / 'five-model-screen/sources', bundle / 'sources')
        cases = screen.CASES + (screen.EXTRA_CASES if study == 'screen' else [])
        if a.check:
            cases = [c for c in cases if selected(a, c['name'])] if a.only else [next(c for c in cases if c['name'] == 'kuramot100')]
        screen.prepare(bundle, a.xppautx, a.xppaut, cases)
        shutil.copytree(bundle / 'models', dest, dirs_exist_ok=True)
        if study == 'screen':
            return [dest / (c['name'] + '.ode') for c in cases]
    paths = []
    if study == 'modeldb':
        source = BENCH / 'modeldb-267710/models/6cModel'
        for f in source.glob('*.inc'):
            shutil.copy2(f, dest / f.name)
        paths = [source / (name + '.ode') for name in ('rest', 'fire')]
    elif study == 'research':
        paths = [REPO / 'examples/ode' / (name + '.ode') for name in ('mu26', 'ma23')]
    elif study == 'izhikevich':
        paths = sorted((BENCH / 'izhikevich').glob('izh_*.ode'))
        if len(paths) != 28:
            raise ValueError('expected all 28 Izhikevich sources')
    elif study == 'python':
        paths = [*sorted((REPO / 'examples/ode').glob('izh_*.ode')), REPO / 'examples/ode/mu26.ode', REPO / 'examples/ode/ma23.ode']
        paths = [p for p in paths if selected(a, p.stem)] if a.check and a.only else [] if a.check else paths
    for path in paths:
        shutil.copy2(path, dest / path.name)
        convert(a, dest / path.name)
    return sorted(dest.glob('*.ode'))


def variant(a, source, label, *, ready=False, method=None, total=None, tolerance=None, full=False, step=None):
    folder = a.out / 'inputs' / label
    folder.mkdir(parents=True)
    for f in source.parent.iterdir():
        if f.suffix == '.inc' or f.name.startswith(source.stem + '.'):
            shutil.copy2(f, folder / f.name)
    model = folder / source.name
    odex = Odex(model.with_suffix('.odex'))
    dt = float(odex.options.get('dt', '.05'))
    if method == 'discrete' or odex.discrete:
        dt = 1.0
    short = a.check and not full  # a check's runs are short, a pilot's are the study's own
    settings = []
    if ready or short or total is not None:
        total = dt if ready else (max(2 * dt, .1) if short and total is None else total)
        settings += ['total=%.17g' % total]
        if ready or short:
            settings += ['trans=0', 'nout=1']
    if method:
        settings += ['meth=' + legacy_method_key(method)]
    if step:
        settings += ['dt=%.17g' % step]
    if tolerance:
        settings += ['tol=%.17g' % tolerance, 'atol=%.17g' % tolerance]
    if settings:
        model.write_text(ode_option(model.read_text(), '@ ' + ','.join(settings)))
        setfile = folder / (source.stem + '.ode.set')
        if setfile.exists():
            updates = {'total': total} if total is not None else {}
            if step:
                updates['DeltaT'] = '%.17g' % step
            if tolerance:
                updates.update({'Tolerance': '%.17g' % tolerance, 'Abs. Tolerance': '%.17g' % tolerance})
            if method:
                # SET method numbers come from the core's method registry,
                # through W427's already parsed table, never another table.
                old_label = next(line.split(maxsplit=1)[1] for line in setfile.read_text().splitlines()
                                 if len(line.split(maxsplit=1)) == 2 and line.split(maxsplit=1)[1] in {label for _, label, _ in method_rows()})
                index, new_label = next((i, label) for i, (_, label, key) in enumerate(method_rows()) if key == method)
                lines = setfile.read_text().splitlines()
                lines = ['%d  %s' % (index, new_label) if line.split(maxsplit=1)[1:] == [old_label] else line for line in lines]
                setfile.write_text('\n'.join(lines) + '\n')
            if updates:
                setfile_patch(setfile, updates)
        model.with_suffix('.odex').unlink()
        convert(a, model)
    save(folder / 'hashes.json', {f.name: sha(f) for f in folder.iterdir() if f.is_file()})
    return model


def engine_argv(a, model, engine, setfile):
    # The command line of one program's run of `model` (its files copied beside it), without the pinning.
    if engine == 'python':
        argv = [sys.executable, str(PYTHON / 'integrate.py'), model.with_suffix('.odex').name]
    elif engine == 'xppautx':
        argv = [str(a.xppautx), model.with_suffix('.odex').name, '--silent', '--internset', '0']
    elif engine == 'xppaut':
        argv = [str(a.xppaut), model.name, '-silent', '-internset', '0']
    elif engine == 'reference':
        argv = [sys.executable, str(PYTHON / 'scipy_integrate.py'), model.with_suffix('.odex').name]
    else:
        raise ValueError('unknown engine ' + engine)
    if setfile.exists():
        argv += ['-setfile' if engine == 'xppaut' else '--setfile', setfile.name]
    return argv


def launch(a, model, engine, folder, measured):
    folder.mkdir(parents=True)
    for f in model.parent.iterdir():
        if f.suffix in INPUT_SUFFIXES:
            shutil.copy2(f, folder / f.name)
    argv = engine_argv(a, model, engine, folder / (model.stem + '.ode.set'))
    result = command(argv, folder, measured=measured, cpus=a.cores[:1], env=environment(folder / 'home'))
    output = folder / 'output.dat'
    if output.exists():
        rows = [[float(v) for v in line.split()] for line in output.read_text().splitlines() if line.strip()]
        result.update(points=len(rows) - 1, output_sha256=sha(output))
        result['valid'] &= (len(rows) >= 2 and all(len(r) == len(rows[0]) and all(math.isfinite(v) for v in r) for r in rows)
                            and all(rows[i][0] > rows[i-1][0] for i in range(1, len(rows))))
        if measured and result['points']:
            result.update(wall_per_point=result['wall_seconds'] / result['points'], cpu_per_point=result['cpu_seconds'] / result['points'])
        with output.open('rb') as src, gzip.open(folder / 'output.dat.gz', 'wb') as dst:
            shutil.copyfileobj(src, dst)
    else:
        result['valid'] = False
    save(folder / 'result.json', result)
    return result


def python_verdict(a, model, left, right):
    # W427's judge on Python's own saved run (its output.dat is what run_model gave, as xppautX prints it), not an integration
    # again. The verdict depends only on the two outputs, the model and the judge's code, so it is kept beside the gates under
    # that key: later repetitions, and an attempt reusing this one, take it from there (a perturbed-derivative probe is
    # SENSITIVITY_PROBES more Python runs, made only when the two runs differ).
    import numpy as np
    from driver import load, Run
    setfile = model.parent / (model.stem + '.ode.set')
    code = hashlib.sha256(b''.join(f.read_bytes() for f in sorted(PYTHON.glob('*.py')))).hexdigest()
    key = dict(xppautx=sha(left / 'output.dat'), python=sha(right / 'output.dat'), model=sha(model.with_suffix('.odex')),
               setfile=sha(setfile) if setfile.exists() else None, code=code)
    name = model.parent.name + '.json'
    for folder in [a.out] + ([a.reuse] if a.reuse else []):
        cached = folder / 'gates' / 'python-judge' / name
        if cached.exists() and json.loads(cached.read_text())['key'] == key:
            return json.loads(cached.read_text())['verdict']
    check = module('python_agreement', PYTHON / 'check.py')
    m, n = load(model.with_suffix('.odex'), setfile if setfile.exists() else None)
    rows = np.loadtxt(right / 'output.dat', ndmin=2).astype(np.float32)
    run = Run(rows, ['t'] + m.states + m.aux_names, rows[:, 0].tolist(), None)
    verdict = list(check.judge(m, n, n.method, run, np.loadtxt(left / 'output.dat', ndmin=2)))
    save(a.out / 'gates' / 'python-judge' / name, dict(key=key, verdict=verdict))
    return verdict


def gate(a, model, outputs, engines, known):
    # known: comparisons an earlier study already made of these very outputs (both programs' rows imported): not made again.
    comparisons = []
    for engine in engines:
        if engine == 'xppautx':
            continue
        if engine in known:
            comparisons.append(known[engine])
            continue
        left, right = outputs['xppautx'], outputs[engine]
        if not (left / 'output.dat.gz').exists() or not (right / 'output.dat.gz').exists():
            comparisons.append(dict(engine=engine, passed=False, reason='missing output'))
            continue
        result = screen.compare(left / 'output.dat.gz', right / 'output.dat.gz')
        if engine == 'python':
            error, detail = python_verdict(a, model, left, right)
            result.update(passed=error is None, w427_detail=detail, w427_error=error)
        else:
            # Equal work: the screen tolerance, or the same trajectory sampled a hair apart (screen.compare's timing check).
            result['passed'] = result['within_screen_tolerance'] or result['within_timing_tolerance']
            result['passed_by'] = 'screen tolerance' if result['within_screen_tolerance'] else 'timing tolerance' if result['passed'] else None
        comparisons.append(dict(engine=engine, **result))
    return dict(passed=all(c['passed'] for c in comparisons), comparisons=comparisons)


MIN_HORIZON_SHARE = 0.1  # of the full run: a shorter agreeing prefix measures start-up, not the integration
# The methods whose programs are timed at equal accuracy rather than equal tolerance: XPPAUT 8.0 restarts Dormand-Prince at
# every output point, so no step exceeds the output step and its accuracy comes from dt, not tol (docs/xppaut-findings.md
# finding 59), while xppautX and Python keep their step across output points (W444, #507).
LADDER_METHODS = ('dopri8',)
# Tighter tolerances tried after the study's own, until a program reaches the reference; 1e-12 is the reference's own tolerance
# (scipy_integrate.REFERENCE_TOLERANCE), so no finer rung can be closer to it.
LADDER_TOLERANCES = (1e-6, 1e-9, 1e-12)
HORIZON_FRACTION = 0.9  # of the first divergence time: the prefix keeps a margin before the outputs leave tolerance


def refusal_reasons(agreement, current):
    reasons = ['%s exit %s' % (r['engine'], r.get('exit_code')) for r in current if not r['valid']]
    for c in agreement['comparisons']:
        if not c['passed']:
            error = c.get('max_abs_error')
            reasons.append('%s: %s' % (c['engine'], c.get('reason') or 'outputs differ (max error %s at row %s, column %s)'
                                       % ('not finite' if error is None else '%.3g' % error, c.get('worst_abs_row'), c.get('worst_abs_column'))))
    return reasons or ['gate failed']


def diverged_horizon(agreement):
    # None when a run is missing or invalid, or the grids differ: only a real divergence earns a horizon.
    failed = [c for c in agreement['comparisons'] if not c['passed']]
    if not failed or any('first_outside_time' not in c for c in failed):
        return None
    first = min(failed, key=lambda c: c['first_outside_time'])
    if first['first_outside_time'] < MIN_HORIZON_SHARE * first['final_time']:
        return None
    return dict(horizon=HORIZON_FRACTION * first['first_outside_time'], horizon_fraction=HORIZON_FRACTION,
                first_outside_time=first['first_outside_time'], first_outside_row=first['first_outside_row'],
                first_outside_column=first['first_outside_column'], engine=first['engine'],
                max_abs_error_full_run=first['max_abs_error'], final_time=first['final_time'], error_curve=first['error_curve'])


def current_key(a, engine, model):
    return reuse_key(a, engine, model.parent, measure_sha(engine_argv), a.cores[:1])


def earlier_rows(a):
    # The earlier study's headline rows by (workload, engine, repetition, row).
    context = earlier_context(a)
    if 'rows' not in context:
        context['rows'] = {(r['workload'], r['engine'], r['repetition'], r['row']): r
                           for r in context['records'] if r.get('row') in ('run', 'ready') and not r.get('sweep')}
    return context['rows']


def earlier_folder(a, rec):
    return a.reuse / 'runs' / rec['workload'] / str(rec['repetition']) / rec['engine'] / rec['row']


def argv_unchanged(a, engine, folder, source_name):
    # An earlier row's command.json against the command line the driver would build now (the program's own path and the
    # interpreter aside, which the program and interpreter hashes cover; the pinned CPU stays in).
    old = json.loads((folder / 'command.json').read_text())
    model = folder / source_name
    new = ['taskset', '-c', str(a.cores[0]), *engine_argv(a, model, engine, folder / (model.stem + '.ode.set'))]
    return old[:3] + old[4:] == new[:3] + new[4:] and len(old) > 3


def earlier_key(a, rec, key, source_name):
    # The earlier row's reuse key as recorded, or, for a row of a series before W452, derived from what is on disk
    # (common.derived_key). None when its command line is not the one built now.
    folder = earlier_folder(a, rec)
    if not folder.is_dir():
        return None
    if rec.get('reuse_key'):
        return rec['reuse_key']
    if not argv_unchanged(a, rec['engine'], folder, source_name):
        return None
    cpus = [int(c) for c in json.loads((folder / 'command.json').read_text())[2].split(',')]
    return derived_key(a, rec, folder, key['measure'], cpus)


def same_key(have, key, *, inputs=True):
    skip = ('derived',) if inputs else ('derived', 'inputs')
    return have is not None and {k: v for k, v in have.items() if k not in skip} == {k: v for k, v in key.items() if k not in skip}


def importable(a, name, engine, rep, models):
    # An engine's rows of one repetition from the earlier study: all of them (run, ready) valid, and each row's key equal to
    # what this run would record. Returns ({row: (record, key)}, None) or (None, why).
    if not a.reuse:
        return None, 'no --reuse'
    rows = earlier_rows(a)
    found = {}
    for row, model in models.items():
        rec = rows.get((name, engine, rep, row))
        if rec is None:
            return None, 'no earlier %s row of repetition %d' % (row, rep)
        if not rec.get('valid'):
            return None, 'the earlier %s row of repetition %d was not valid' % (row, rep)
        key = current_key(a, engine, model)
        have = earlier_key(a, rec, key, model.name)
        if not same_key(have, key):
            changed = [k for k in key if have is None or have.get(k) != key[k]]
            return None, 'earlier %s row differs in %s' % (row, ', '.join(changed) or 'its command line')
        found[row] = (rec, dict(have, **key) if have.get('derived') else have)
    return found, None


def import_rows(a, found, name, engine, rep):
    # The earlier rows and their run folders, marked reused_from; the folders are linked, not copied.
    imported = []
    for row, (rec, key) in found.items():
        dest = a.out / 'runs' / name / str(rep) / engine / row
        dest.parent.mkdir(parents=True, exist_ok=True)
        link_tree(earlier_folder(a, rec), dest)
        imported.append(dict(mark_reused(a, rec), reuse_key=key))
    return imported


def reuse_whole(a, name, model, ready, records):
    # A workload an earlier attempt finished (done/) with every program unchanged: its records, runs, ladder, work-precision
    # and dt-sweep rows and gates are imported as they were. Programs unchanged: each engine's program hash and measurement
    # hash equal, the model and ready inputs equal (the ladder's and sweeps' inputs derive from them), the command line equal.
    marker = a.reuse / 'done' / (name + '.json')
    if not marker.exists():
        return False
    marker = json.loads(marker.read_text())
    for label, built in ((name, model), (name + '-ready', ready)):
        earlier = a.reuse / 'inputs' / label
        if not earlier.is_dir() or inputs_sha(earlier) != inputs_sha(built.parent):
            progress(a, '%s: not reused whole: the input %s differs' % (name, label))
            return False
    sample = {}
    for r in marker['records']:
        sample.setdefault(r['engine'], r)
    for engine, rec in sample.items():
        key = current_key(a, engine, model)
        have = earlier_key(a, rec, key, model.name)
        if not same_key(have, key, inputs=False):
            progress(a, '%s: not reused whole: %s differs in %s' % (name, engine, ', '.join(k for k in key if k != 'inputs' and (have is None or have.get(k) != key[k])) or 'its command line'))
            return False
    workloads = {r['workload'] for r in marker['records']}
    imported = []
    for r in marker['records']:
        folder = earlier_folder(a, r)
        have = r.get('reuse_key') or derived_key(a, r, folder, current_key(a, r['engine'], model)['measure'],
                                                  [int(c) for c in json.loads((folder / 'command.json').read_text())[2].split(',')])
        imported.append(dict(mark_reused(a, r), reuse_key=have))
        dest = a.out / 'runs' / r['workload'] / str(r['repetition']) / r['engine'] / r['row']
        dest.parent.mkdir(parents=True, exist_ok=True)
        link_tree(folder, dest)
    for w in workloads:
        for kind in ('reference', 'ladder', 'work-precision', 'dt-sweep', 'horizons', 'diverged'):
            for src in (a.reuse / kind).glob(w + '*'):
                if src.name in (w, w + '.json'):
                    dest = a.out / kind / src.name
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    link_tree(src, dest) if src.is_dir() else shutil.copy2(src, dest)
        for src in (a.reuse / 'gates').glob(w + '-[0-9]*.json'):
            (a.out / 'gates').mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, a.out / 'gates' / src.name)
    records.extend(imported)
    save(a.out / 'done' / (name + '.json'), dict(records=imported))
    save(a.out / 'runs.json', records)
    progress(a, 'REUSED %s (every program: %s) from %s' % (name, ', '.join(sorted(sample)), a.reuse))
    return True


def series(a, source, name, records, *args, **kwargs):
    start = len(records)
    allowed = _series(a, source, name, records, *args, **kwargs)
    if allowed and not a.check and len(records) > start and not (a.out / 'done' / (name + '.json')).exists():
        save(a.out / 'done' / (name + '.json'), dict(records=records[start:]))
    return allowed


def stop_here(a, records, driver_failures, name, why):
    progress(a, 'STOPPED at %s: %s' % (name, why))
    summarize(records, a.out)
    save(a.out / 'completion.json', dict(passed=False, check=a.check, rows=len(records), driver_failures=driver_failures, stopped_at=name, why=why))
    sys.exit('stopped at %s (--stop-on-failure): %s' % (name, why))


def effective_numerics(model):
    # The numerics the programs read: the model's options, then its set file (as numerics.py reads both).
    odex = Odex(model.with_suffix('.odex'))
    numerics = numerics_of(odex)
    setfile = model.parent / (model.stem + '.ode.set')
    if setfile.exists():
        apply_xppaut_set(odex, numerics, setfile)
    return numerics


def reaches_reference(c):
    # The screen tolerance on every value. Not screen.compare's time grid check: the reference prints its times as doubles and
    # the programs as single precision floats (0.05 and 0.050000001), the same times written differently.
    return c['same_shape'] and not c['non_finite_error'] and c['outside_tolerance'] == 0


def ladder(a, name, build, engines):
    # Untimed pilots: each program's loosest tolerance, the study's own then LADDER_TOLERANCES, whose full run reaches the
    # reference within the screen tolerance (the judge of the other comparisons). A program stops climbing at its first pass.
    # Returns {engine: tolerance, or None when no rung reached}, saving every rung's error beside it.
    folder = a.out / 'ladder' / name
    own = build(name + '-pilot-own', None, full=True)
    reference = launch(a, own, 'reference', folder / 'reference', False)
    if not reference['valid']:
        raise RuntimeError('%s: the reference run failed, so no program can be placed on the ladder' % name)
    own_tolerance = effective_numerics(own).relative_tolerance
    rungs = [(own_tolerance, own)] + [(t, build('%s-pilot-%g' % (name, t), t, full=True)) for t in LADDER_TOLERANCES if t < own_tolerance]
    chosen, tried = {}, []
    for tolerance, model in rungs:
        for engine in [e for e in engines if e not in chosen]:
            run = folder / ('%g' % tolerance) / engine
            result = launch(a, model, engine, run, measured=False)
            row = dict(engine=engine, tolerance=tolerance, valid=result['valid'], reached=False)
            if result['valid']:
                c = screen.compare(run / 'output.dat.gz', folder / 'reference/output.dat.gz')
                row.update(reached=reaches_reference(c), max_abs_error=c['max_abs_error'], first_outside_time=c.get('first_outside_time'))
            tried.append(row)
            if row['reached']:
                chosen[engine] = tolerance
        if len(chosen) == len(engines):
            break
    result = {e: chosen.get(e) for e in engines}
    save(a.out / 'ladder' / (name + '.json'), dict(workload=name, chosen=result, rungs=tried, never=[e for e in engines if e not in chosen],
         note='never: no rung up to %g reached the reference within the screen tolerance' % LADDER_TOLERANCES[-1]))
    for e in engines:
        progress(a, '%s: %s timed at tolerance %s' % (name, e, '%g' % result[e] if result[e] else 'none (never within the screen tolerance up to %g)' % LADDER_TOLERANCES[-1]))
    return result, rungs


# Output spacings of the sweep, as multiples of the model's dt (the step XPPAUT 8.0 restarts at, so its step cap and its cost).
# 1 is the study's own; coarser spacings write fewer rows, so 64 is cheap here (it is the finer ones that cost row writing).
SPACING_FACTORS = (1, 4, 16, 64)


def timed_points(a, name, records, kind, points):
    # Times each point with the study's protocol (warm-up, repetitions, rotating order) and puts its error against its
    # reference beside every row. A point: key, engine, model, tolerance, dt, reference (a folder). Rows carry `sweep`=kind.
    errors, long_points = {}, set()
    for rep in repetitions(a):
        progress(a, '%s: %s repetition %d of %d%s' % (name, kind, rep, a.repeats, ' (warm-up, not counted)' if rep == 0 else ''))
        for p in rotation([p for p in points if rep < 2 or (p['engine'], p['key']) not in long_points], rep):
            row = '%s-%s' % (kind, p['key'])
            folder = a.out / 'runs' / name / str(rep) / p['engine'] / row
            result = launch(a, p['model'], p['engine'], folder, measured=not a.check)
            if not result['valid']:
                raise RuntimeError('%s: %s %s of %s failed (exit %s)' % (name, kind, p['key'], p['engine'], result.get('exit_code')))
            if rep == 0:
                c = screen.compare(folder / 'output.dat.gz', p['reference'] / 'output.dat.gz')
                errors[(p['engine'], row)] = dict(reference_max_abs_error=c['max_abs_error'], reference_first_outside_time=c.get('first_outside_time'),
                                                  output_spacing=c['final_time'] / result['points'])
            rec = dict(workload=name, engine=p['engine'], row=row, repetition=rep, **result, equal_work=True, sweep=kind,
                       tolerance=p['tolerance'], dt=p['dt'], timing_scope='whole silent process', **errors[(p['engine'], row)])
            if (p['engine'], p['key']) in long_points or is_long(rec):
                long_points.add((p['engine'], p['key']))
                rec['repetitions_reason'] = LONG_RUN_REASON
            records.append(rec)
        save(a.out / 'runs.json', records)
        summarize(records, a.out)


def equal_accuracy_rows(a, records, name, engine):
    # The headline rows, already timed: counted repetitions only (a check has the one untimed run).
    return [r for r in records if r['workload'] == name and r['engine'] == engine and r.get('row') == 'run' and (a.check or r['repetition'] > 0)]


def write_curve(a, records, name, kind, folder, points, reused, note):
    # One entry per (program, point): the timed repetitions (or the headline rows a point reuses) and the rows each run wrote.
    entries = []
    for p, recs in [(p, [r for r in records if r['workload'] == name and r['engine'] == p['engine'] and r.get('row') == '%s-%s' % (kind, p['key'])
                         and (a.check or r['repetition'] > 0)]) for p in points] + reused:
        wall, cpu = [r.get('wall_seconds') for r in recs], [r.get('cpu_seconds') for r in recs]
        timed = None not in wall + cpu and len(recs) == required_repetitions(recs)
        entries.append(dict(engine=p['engine'], tolerance=p['tolerance'], dt=p['dt'], output_spacing=recs[0]['output_spacing'], output_points=recs[0]['points'],
                            rows_written=recs[0]['points'] + 1, max_abs_error=recs[0]['reference_max_abs_error'],
                            first_outside_time=recs[0]['reference_first_outside_time'],
                            source='equal-accuracy run' if p in [q for q, _ in reused] else kind,
                            counted_repetitions=len(recs), repetitions_reason=recs[0].get('repetitions_reason'),
                            wall_seconds=wall, cpu_seconds=cpu, wall_mean=statistics.mean(wall) if timed else None,
                            wall_sample_sd=statistics.stdev(wall) if timed and len(wall) > 1 else None, cpu_mean=statistics.mean(cpu) if timed else None))
    save(a.out / folder / (name + '.json'), dict(workload=name, kind=kind, note=note, points=entries))


def point_of(model, engine, key, tolerance, reference):
    numerics = effective_numerics(model)
    return dict(key=key, engine=engine, model=model, tolerance=tolerance, dt=numerics.dt, reference=reference)


def work_precision(a, name, records, rungs, chosen, engines):
    # Every ladder rung timed for every program, to show what the tolerance buys each. A program's equal-accuracy point is
    # the headline series, timed already, so it is read from there and not timed again (a check times everything, untimed).
    reference = a.out / 'ladder' / name / 'reference'
    points, reused = [], []
    for tolerance, model in rungs:
        for e in engines:
            p = point_of(model, e, '%g' % tolerance, tolerance, reference)
            if not a.check and chosen[e] == tolerance:
                reused.append((p, equal_accuracy_rows(a, records, name, e)))
            else:
                points.append(p)
    progress(a, '%s: work-precision, %d points timed, %d read from the equal-accuracy rows' % (name, len(points), len(reused)))
    timed_points(a, name, records, 'wp', points)
    write_curve(a, records, name, 'wp', 'work-precision', points, reused, 'time against max abs error versus the reference, per program, at every ladder tolerance')


def dt_sweep(a, name, records, build, rungs, chosen, engines):
    # One fixed tolerance, the study's own (the loosest rung): there xppautX and Python take steps far larger than dt across
    # output points while XPPAUT's step is capped by dt, so the spacing sets XPPAUT's accuracy and cost (coarser: larger error, less work) while
    # xppautX's stay put. A tolerance every program already meets (the ladder's) would put all at the error floor.
    tolerance = effective_numerics(rungs[0][1]).relative_tolerance
    base = effective_numerics(rungs[0][1]).dt
    points, reused = [], []
    for factor in SPACING_FACTORS:
        model = build('%s-dt%g' % (name, factor), tolerance, full=True, step=base * factor)
        reference = a.out / 'dt-sweep' / name / ('reference-x%g' % factor)
        if not launch(a, model, 'reference', reference, False)['valid']:
            raise RuntimeError('%s: the reference run at dt/%g failed' % (name, factor))
        for e in engines:
            p = point_of(model, e, 'x%g' % factor, tolerance, reference)
            if factor == 1 and not a.check and chosen[e] == tolerance:
                reused.append((p, equal_accuracy_rows(a, records, name, e)))
            else:
                points.append(p)
    progress(a, '%s: dt sweep at tolerance %g, dt multiplied by %s, %d points timed' % (name, tolerance, ', '.join('%g' % f for f in SPACING_FACTORS), len(points)))
    timed_points(a, name, records, 'dt', points)
    write_curve(a, records, name, 'dt', 'dt-sweep', points, reused,
                'fixed tolerance %g (the study\'s own), dt = %g * factor; rows_written is what each run wrote' % (tolerance, base))


def _series(a, source, name, records, method=None, native_only=False, horizon=None):
    terman_acceptance = None
    if source.stem == 'episodic' and not a.check:
        criteria = HERE / 'terman-criteria.json'
        if not criteria.exists():
            raise RuntimeError('refusing to time Terman without predeclared terman-criteria.json')
        parent_status = json.loads((a.out.parents[1] / 'status.json').read_text())
        terman_out = Path(parent_status['studies']['terman']['out'])
        terman_acceptance = json.loads((terman_out / 'acceptance.json').read_text())
        if not terman_acceptance['passed'] or terman_acceptance.get('check_only'):
            raise RuntimeError('refusing to time Terman: convergence has not passed')
    def build(label, tolerance, ready=False, full=False, step=None):
        path = variant(a, source, label, ready=ready, method=method, total=None if ready else horizon, tolerance=tolerance, full=full, step=step)
        if terman_acceptance and tolerance is None:
            setfile_patch(path.parent / 'episodic.ode.set', {'Tolerance': terman_acceptance['chosen_tolerance']})
        return path

    model, ready = build(name, None), build(name + '-ready', None, ready=True)
    if a.reuse and reuse_whole(a, name, model, ready, records):
        return True
    engines = ['xppautx'] if native_only else ['xppautx', 'xppaut'] + (['python'] if a.study == 'python' else [])
    # By default one input for every program; a ladder method gives each program the tolerance at which it reaches the reference.
    inputs = {e: (model, ready) for e in engines}
    by_ladder = a.study == 'python' and method in LADDER_METHODS
    if by_ladder:
        chosen, rungs = ladder(a, name, build, engines)
        every_engine = engines
        engines = [e for e in engines if chosen[e]]
        if not engines:
            return True
        own = effective_numerics(model).relative_tolerance
        by_tolerance = {own: (model, ready)}
        for e in engines:
            t = chosen[e]
            if t not in by_tolerance:
                by_tolerance[t] = (build('%s-tol%g' % (name, t), t), build('%s-tol%g-ready' % (name, t), t, ready=True))
            inputs[e] = by_tolerance[t]
    before_series(a)
    allowed, baseline = True, None
    accuracy = {e: dict(tolerance=effective_numerics(inputs[e][0]).relative_tolerance) for e in engines} if a.study == 'python' else {}
    long_engines, noted, measure = set(), set(), measure_sha(engine_argv)
    for rep in repetitions(a):
        # A program whose warm-up was long is counted once: it takes no part in the repetitions after the first counted one.
        active = [e for e in engines if rep < 2 or e not in long_engines]
        progress(a, '%s: repetition %d of %d%s' % (name, rep, a.repeats, ' (warm-up, not counted)' if rep == 0 else ''))
        outputs = {}
        current = []
        imported = set()
        for engine in rotation(active, rep):
            rows = dict(zip(('run', 'ready'), inputs[engine]))
            found, why = importable(a, name, engine, rep, rows)
            if found:
                imported.add(engine)
                if engine not in noted:
                    noted.add(engine)
                    progress(a, '%s: %s REUSED from %s: program, measurement code and inputs unchanged (each repetition earlier finished is imported)' % (name, engine, a.reuse))
                for rec in import_rows(a, found, name, engine, rep):
                    if engine in long_engines:
                        rec['repetitions_reason'] = LONG_RUN_REASON
                    current.append(rec)
                    records.append(rec)
                    if rec['row'] == 'run':
                        outputs[engine] = a.out / 'runs' / name / str(rep) / engine / 'run'
                save(a.out / 'runs.json', records)
                continue
            if a.reuse and engine not in noted:
                noted.add(engine)
                progress(a, '%s: %s runs fresh from repetition %d: %s' % (name, engine, rep, why))
            for row, input_model in rows.items():
                folder = a.out / 'runs' / name / str(rep) / engine / row
                result = launch(a, input_model, engine, folder, measured=not a.check)
                rec = dict(workload=name, engine=engine, row=row, repetition=rep, **result,
                           timing_scope='whole silent process' if row == 'run' else 'process start through one step', **accuracy.get(engine, {}))
                rec['reuse_key'] = current_key(a, engine, input_model)
                if engine in long_engines:
                    rec['repetitions_reason'] = LONG_RUN_REASON
                current.append(rec)
                if row == 'run':
                    outputs[engine] = folder
                records.append(rec)
                save(a.out / 'runs.json', records)
        if rep == 0 and not a.check:
            for e in active:
                if any(is_long(r) for r in current if r['engine'] == e and r['row'] == 'run'):
                    long_engines.add(e)
                    progress(a, '%s: %s warm-up over %d s: one counted repetition' % (name, e, LONG_RUN_SECONDS))
                    for r in current:
                        if r['engine'] == e:
                            r['repetitions_reason'] = LONG_RUN_REASON
        known = {}
        if 'xppautx' in imported:
            earlier_gate = a.reuse / 'gates' / (name + '-%d.json' % rep)
            if earlier_gate.exists():
                known = {c['engine']: c for c in json.loads(earlier_gate.read_text())['comparisons'] if c['engine'] in imported}
        logs = {e: (outputs[e] / 'run.log').read_text(errors='replace') for e in active}
        last = {e: next((l.strip() for l in reversed(t.splitlines()) if 'not completed' not in l and l.strip() and 'Run complete' not in l), '')
                for e, t in logs.items()}
        # An engine aborted: xppautX says why and exits 1 (W480), XPPAUT prints "Integration not completed" and exits 0, both
        # writing what they have; Python's integrator stops without output.
        run_failed = {e: any(r['exit_code'] != 0 for r in current if r['engine'] == e and r['row'] == 'run') for e in active}
        stopped = {e: run_failed[e] if e == 'xppautx' else ('Integration not completed' in logs[e]) if e == 'xppaut' else
                   run_failed[e] and not (outputs[e] / 'output.dat').exists()
                   for e in active}
        same_row = len({sha(outputs[e] / 'output.dat') for e in ('xppautx', 'xppaut') if e in outputs and (outputs[e] / 'output.dat').exists()}) == 1
        if rep == 0 and all(stopped.values()):
            # Every engine stops: the method fails on this model (it leaves the bounds or Newton does not converge), so there is no
            # work to compare or time. A result of its own, not a failure of the harness; where xppautX and XPPAUT stop at different
            # rows, each one's stop is kept.
            save(a.out / 'diverged' / (name + '.json'), dict(workload=name, method=method, last_log_line=last, same_row=same_row,
                 note='every engine aborts, xppautX and XPPAUT %s: the method fails on this model; nothing was timed'
                      % ('at the same row' if same_row else 'at different rows')))
            progress(a, 'ALL ENGINES ABORT (%s): the method fails on this model; nothing timed: %s' % ('; '.join('%s: %s' % kv for kv in last.items()), name))
            for rec in current:
                rec['equal_work'] = False
            save(a.out / 'runs.json', records)
            return True
        agreement = (dict(passed=True, comparisons=[], note='equal accuracy by the ladder, not equal tolerance') if by_ladder
                     else gate(a, model, outputs, active, known))
        if terman_acceptance and not by_ladder:
            import numpy as np
            # Import here: terman shares the staging and launch owner above.
            from terman import array, compare as compare_readouts, readouts
            criteria = json.loads((HERE / 'terman-criteria.json').read_text())
            names = Odex(model.with_suffix('.odex')).states
            ref = json.loads((terman_out / 'dop853-reference.json').read_text())
            agreement['terman'] = {e: compare_readouts(readouts(array(outputs[e] / 'output.dat'), names, criteria),
                                      ref, criteria, terman_acceptance['statistics_only']) for e in active}
            agreement['passed'] = all(r['passed'] for r in agreement['terman'].values())
        save(a.out / 'gates' / (name + '-%d.json' % rep), agreement)
        allowed &= agreement['passed'] and all(r['valid'] for r in current)
        for rec in current:
            rec['equal_work'] = allowed
        save(a.out / 'runs.json', records)
        if rep == 0:
            baseline = {e: sha(outputs[e] / 'output.dat') for e in engines if (outputs[e] / 'output.dat').exists()}
            if a.study == 'python':
                # W427's independent solver is outside the timed programs.
                ref = launch(a, model, 'reference', a.out / 'reference' / name, False)
                comparisons = {e: screen.compare(outputs[e] / 'output.dat.gz', a.out / 'reference' / name / 'output.dat.gz')
                               for e in engines if (outputs[e] / 'output.dat.gz').exists()} if ref['valid'] else {}
                save(a.out / 'reference' / name / 'comparison.json', dict(process=ref, comparisons=comparisons))
                # Every timed row carries its error against the reference beside its time.
                for e, c in comparisons.items():
                    accuracy[e].update(reference_max_abs_error=c['max_abs_error'], reference_first_outside_time=c.get('first_outside_time'),
                                       output_spacing=c['final_time'] / next(r['points'] for r in current if r['engine'] == e and r['row'] == 'run'))
                for rec in current:
                    rec.update(accuracy.get(rec['engine'], {}))
                save(a.out / 'runs.json', records)
            if not allowed:
                reasons = refusal_reasons(agreement, current)
                divergence = diverged_horizon(agreement) if horizon is None and all(r['valid'] for r in current) else None
                if divergence:
                    # Chaotic or event-driven: the full runs part, but the prefix agrees. Time that prefix
                    # instead, and record where and by how much the outputs left each other.
                    save(a.out / 'horizons' / (name + '.json'), dict(workload=name, reasons=reasons, **divergence))
                    print('NOT EQUAL WORK over the full run (%s); timing the agreeing prefix to t=%.6g (first divergence t=%.6g, column %d)'
                          % ('; '.join(reasons), divergence['horizon'], divergence['first_outside_time'], divergence['first_outside_column']), flush=True)
                    del records[len(records) - len(current):]
                    return series(a, source, name + '-horizon', records, method, native_only, horizon=divergence['horizon'])
                print('NOT EQUAL WORK; no measured series', name, '-', '; '.join(reasons), flush=True)
                break
        else:
            for rec in current:
                if rec['row'] == 'run' and rec.get('output_sha256') != baseline.get(rec['engine']):
                    rec['equal_work'] = False
        summarize(records, a.out)
    if by_ladder and allowed:
        work_precision(a, name, records, rungs, chosen, every_engine)
        dt_sweep(a, name, records, build, rungs, chosen, every_engine)
        summarize(records, a.out)
    return allowed


def large_stiff(a):
    kit = BENCH / 'large-stiff'
    if a.check:
        argv = [sys.executable, str(kit / 'study.py'), 'check', '--worker', str(a.worker), '--sizes', '50', '--allow-no-klu', '--out', str(a.out / 'check')]
        subprocess.run(argv, cwd=REPO, check=True)
        return
    before_series(a)
    subprocess.run([sys.executable, str(kit / 'study.py'), 'check', '--worker', str(a.worker), '--sizes', '50', '200', '--out', str(a.out / 'check')], cwd=REPO, check=True)
    subprocess.run([sys.executable, str(kit / 'study.py'), 'check', '--worker', str(a.worker), '--sizes', '1000', '2000', '3000', '5000', '--out', str(a.out / 'pilot')], cwd=REPO, check=True)
    for mode in ('measure', 'cache', 'summarize'):
        argv = [sys.executable, str(kit / 'study.py'), mode, '--worker', str(a.worker), '--cpu', str(a.cores[0]), '--out', str(a.out / 'results'), '--repeats', str(a.repeats)]
        if mode == 'measure':
            argv += ['--pilot', str(a.out / 'pilot'), '--machine-record', str(a.out / 'machine.json')]
        subprocess.run(argv, cwd=REPO, check=True)


def main():
    p = parser(__doc__)
    p.add_argument('--study', choices=('screen', 'modeldb', 'research', 'izhikevich', 'python', 'scaling', 'large-stiff'), required=True)
    p.add_argument('--worker', type=Path)
    a = p.parse_args()
    initialize(a)
    if a.study == 'large-stiff':
        large_stiff(a)
        return
    records, passed, driver_failures = [], True, []
    if a.study == 'scaling':
        cases = json.loads((BENCH / 'size-scaling/cases-ubuntu-20261020.json').read_text())
        if a.check:
            cases = [cases[0]]
        for c in cases:
            if not selected(a, c['name']):
                continue
            for duration, steps in c['steps'].items():
                folder = a.out / 'models' / (c['name'] + '-' + duration)
                folder.mkdir(parents=True)
                path = folder / (c['name'] + '-' + duration + '.ode')
                scaling.write_model(path, c['family'], c['size'], min(steps, 2) if a.check else steps, c['rows'])
                convert(a, path)
                # XPPAUT cannot load models above its 5000-state ceiling.
                # Those rows retain xppautX data and an explicit unavailable row.
                if not c.get('legacy', True):
                    save(folder / 'unavailable.json', dict(xppaut='XPPAUT 8.0 exceeds its 5000-state model limit'))
                ok = series(a, path, path.stem, records, native_only=not c.get('legacy', True))
                passed &= ok
                if a.stop_on_failure and not ok:
                    stop_here(a, records, driver_failures, path.stem, 'not equal work, see the log above')
        save(a.out / 'cost-per-time.json', scaling_costs(records, cases))
    else:
        for path in stage(a, a.study):
            if not selected(a, path.stem):
                continue
            methods = [None]
            if a.study == 'python':
                from driver import METHODS
                odex = Odex(path.with_suffix('.odex'))
                methods = ['discrete'] if odex.discrete else [m for m in METHODS if m != 'discrete']
                if a.check:
                    methods = ['runge_kutta', *LADDER_METHODS]  # the ladder runs once, untimed
            for method in methods:
                name = path.stem + ('-' + method if method else '')
                try:
                    progress(a, 'WORKLOAD %s' % name)
                    ok = series(a, path, name, records, method)
                    passed &= ok
                    if a.stop_on_failure and not ok:
                        stop_here(a, records, driver_failures, name, 'not equal work, see the log above')
                except Exception as e:
                    progress(a, 'FAILED %s: %s' % (name, e))
                    save(a.out / 'failures' / (name + '.json'), dict(error=str(e), type=type(e).__name__))
                    driver_failures.append(name)
                    passed = False
                    if a.stop_on_failure:
                        stop_here(a, records, driver_failures, name, '%s: %s' % (type(e).__name__, e))
    summarize(records, a.out)
    save(a.out / 'completion.json', dict(passed=passed, check=a.check, rows=len(records), driver_failures=driver_failures))
    if driver_failures or a.check and not passed:
        sys.exit('agreement/driver failure; retained results name every failed workload')


def scaling_costs(records, cases):
    from statistics import mean
    result = []
    for case in cases:
        for engine in ('xppautx', 'xppaut'):
            rows = {d: [r for r in records if r['workload'] == case['name'] + '-' + d and r['engine'] == engine
                        and r.get('equal_work') and r['repetition'] > 0 and r['row'] == 'run'] for d in ('T', '2T')}
            if all(rs and len(rs) == required_repetitions(rs) for rs in rows.values()):
                means = {d: mean(r['wall_seconds'] for r in rs) for d, rs in rows.items()}
                result.append(dict(workload=case['name'], engine=engine, wall_per_model_time=(means['2T'] - means['T']) / (case['steps']['T'] * scaling.DT)))
    return result


if __name__ == '__main__':
    main()
