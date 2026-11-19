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
import traceback

from common import (BENCH, INPUT_SUFFIXES, HERE, LONG_RUN_REASON, LONG_RUN_SECONDS, REPO, TOOLS, before_series, progress, command, derived_key, earlier_context, earlier_program, environment,
                    initialize, is_long, link_tree, mark_reused, measure_sha, module, parser, program_inputs, repetitions, required_repetitions,
                    reuse_key, rotation, save, selected, sha, short_home, summarize, wanted, write_plan)

screen = module('screen', BENCH / 'five-model-screen/screen.py')
scaling = module('scaling', BENCH / 'size-scaling/scaling.py')
PYTHON = TOOLS / 'python-integration'
sys.path.insert(0, str(PYTHON))
from odex_model import Odex

from numerics import legacy_method_key, method_rows, numerics_of


def ode_option(text, line):
    # XPPAUT allows settings after included settings. Insert before the end,
    # preserving the source and the conversion log beside the staged input.
    end = re.search(r'(?im)^\s*(?:done|d)\s*$', text)
    if end is None:
        raise ValueError('staged .ode: no end marker')
    return text[:end.start()] + line + '\n' + text[end.start():]


def convert(a, model):
    # An XPPAUT set file beside the model (model.ode.set) is converted into its .odex once, so xppautX's run needs no switch.
    setfile = model.parent / (model.stem + '.ode.set')
    state = ['--statefile', str(setfile)] if model.suffix == '.ode' and setfile.exists() else []
    p = subprocess.run([str(a.xppautx), '--convert', '--yes', str(model), *state], capture_output=True, text=True)
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
        if a.only:
            cases = [c for c in cases if wanted(a, c['name'])]  # a unit stages its own model only (W699)
        elif a.check:
            cases = [next(c for c in cases if c['name'] == 'kuramot100')]
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
    if a.only:
        paths = [p for p in paths if wanted(a, p.stem)]
    elif a.check and study == 'python':
        paths = []
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
        argv = [str(a.xppautx), model.with_suffix('.odex').name, '--silent']
    elif engine == 'xppaut':
        argv = [str(a.xppaut), model.name, '-silent', '-internset', '0']
    elif engine == 'reference':
        argv = [sys.executable, str(PYTHON / 'scipy_integrate.py'), model.with_suffix('.odex').name]
    else:
        raise ValueError('unknown engine ' + engine)
    if engine == 'xppaut' and setfile.exists():
        argv += ['-setfile', setfile.name]  # XPPAUT keeps its own switch; the other engines' .odex holds the set
    return argv


def launch(a, model, engine, folder, measured):
    folder.mkdir(parents=True)
    for f in model.parent.iterdir():
        if f.suffix in INPUT_SUFFIXES:
            shutil.copy2(f, folder / f.name)
    argv = engine_argv(a, model, engine, folder / (model.stem + '.ode.set'))
    with short_home() as home:
        result = command(argv, folder, measured=measured, cpus=a.cores[:1], env=environment(home))
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
    from numerics import defaults as python_defaults  # the numerics the Python side starts from, read from the options schema (W804)
    code = hashlib.sha256(b''.join(f.read_bytes() for f in sorted(PYTHON.glob('*.py'))) + json.dumps(python_defaults(), sort_keys=True).encode()).hexdigest()
    key = dict(xppautx=sha(left / 'output.dat'), python=sha(right / 'output.dat'), model=sha(model.with_suffix('.odex')),
               code=code)
    name = model.parent.name + '.json'
    for folder in [a.out] + ([a.reuse] if a.reuse else []):
        cached = folder / 'gates' / 'python-judge' / name
        if cached.exists() and json.loads(cached.read_text())['key'] == key:
            return json.loads(cached.read_text())['verdict']
    check = module('python_agreement', PYTHON / 'check.py')
    m, n = load(model.with_suffix('.odex'))
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
    # The row key a run of `model` (its .ode) records and an import checks: the same function for the plan and the run (W827).
    return reuse_key(a, engine, measure_sha(engine_argv), a.cores[:1], program_inputs(engine, model))


def earlier_rows(a):
    # The earlier study's headline rows by (workload, engine, repetition, row).
    context = earlier_context(a)
    if 'rows' not in context:
        context['rows'] = {(r['workload'], r['engine'], r['repetition'], r['row']): r
                           for r in context['records'] if r.get('row') in ('run', 'ready') and not r.get('sweep')}
    return context['rows']


def earlier_diverged(a, name):
    # Whether the earlier study found every engine aborting on this workload (its diverged/ marker): it was then run in its warm-up only.
    return bool(a.reuse) and (a.reuse / 'diverged' / (name + '.json')).exists()


def earlier_folder(a, rec):
    return a.reuse / 'runs' / rec['workload'] / str(rec['repetition']) / rec['engine'] / rec['row']


def argv_unchanged(a, engine, folder, source_name):
    # An earlier row's command.json against the command line the driver would build now. The program's own paths are left out:
    # the binary of an XPPAUT or xppautX run (one item), the interpreter and the script of a Python run (two items, the script
    # being another worktree's file, not the program's name); the program and interpreter hashes cover them. The pinned CPU stays in.
    old = json.loads((folder / 'command.json').read_text())
    model = folder / source_name
    new = ['taskset', '-c', str(a.cores[0]), *engine_argv(a, model, engine, folder / (model.stem + '.ode.set'))]
    own = 2 if engine in ('python', 'reference') else 1
    return old[:3] + old[3 + own:] == new[:3] + new[3 + own:] and len(old) > 3


def recorded_key(rec):
    # a row's recorded key, its program as today's where W803's record proves the Python arm unchanged (common.earlier_program)
    key = rec['reuse_key']
    return dict(key, program=earlier_program(rec['engine'], key['program'])) if 'program' in key else key


def earlier_key(a, rec, key, source_name, folder=None):
    # The earlier row's reuse key: its recorded program, measurement code, CPUs and interpreter (recorded_key), its inputs as its
    # program reads them now from the row's own copy of the model (program_inputs, W827: the files it kept, not the ones it
    # recorded), or, for a row of a series before W452, all of it derived from what is on disk (common.derived_key). None when the
    # folder is gone or its command line is not the one built now. `folder`: the row's run folder when it is not a headline row's
    # (a ladder rung's, W803: W617's rungs kept no key).
    folder = folder or earlier_folder(a, rec)
    if not folder.is_dir() or not argv_unchanged(a, rec['engine'], folder, source_name):
        return None
    model = folder / source_name
    if rec.get('reuse_key'):
        return dict(recorded_key(rec), inputs=program_inputs(rec['engine'], model))
    cpus = [int(c) for c in json.loads((folder / 'command.json').read_text())[2].split(',')]
    return derived_key(a, rec, model, key['measure'], cpus)


def same_key(have, key, *, inputs=True):
    skip = ('derived',) if inputs else ('derived', 'inputs')
    return have is not None and {k: v for k, v in have.items() if k not in skip} == {k: v for k, v in key.items() if k not in skip}


def importable(a, name, engine, rep, models, key_of=current_key):
    # An engine's rows of one repetition from the earlier study: all of them (run, ready) valid, and each row's key equal to
    # what this run would record (key_of(a, engine, model): the server study's own, W295). Returns ({row: (record, key)}, None) or (None, why).
    if not a.reuse:
        return None, 'no --reuse'
    rows = earlier_rows(a)
    aborted = earlier_diverged(a, name)
    found = {}
    for row, model in models.items():
        rec = rows.get((name, engine, rep, row))
        if rec is None:
            return None, 'no earlier %s row of repetition %d' % (row, rep)
        if not rec.get('valid') and not aborted:  # where every engine aborted, each one's abort is the result
            return None, 'the earlier %s row of repetition %d was not valid' % (row, rep)
        key = key_of(a, engine, model)
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


def whole_reusable(a, name, model, ready):
    # (marker, None) when an earlier attempt finished this workload (done/) and every row it kept is equal work now: each row's key
    # (its program, measurement code, what that program reads of the model or the ready input, the command line) is the same as
    # this run's key for it (current_key, the same function an import checks), else (None, why). No folder-wide signature (W827).
    marker = a.reuse / 'done' / (name + '.json')
    if not marker.exists():
        return None, 'no earlier finished run'
    marker = json.loads(marker.read_text())
    for r in marker['records']:
        built = ready if r['row'] == 'ready' else model
        key = current_key(a, r['engine'], built)
        have = earlier_key(a, r, key, built.name)
        if not same_key(have, key):
            changed = [k for k in key if have is None or have.get(k) != key[k]]
            return None, '%s %s differs in %s' % (r['engine'], r['row'], ', '.join(changed) or 'its command line')
    return marker, None


def reuse_whole(a, name, model, ready, records):
    # A workload an earlier attempt finished: its records, runs, ladder, work-precision and dt-sweep rows and gates are imported
    # as they were.
    marker, why = whole_reusable(a, name, model, ready)
    if not marker:
        if (a.reuse / 'done' / (name + '.json')).exists():
            progress(a, '%s: not reused whole: %s' % (name, why))
        return False
    sample = {r['engine'] for r in marker['records']}
    workloads = {r['workload'] for r in marker['records']}
    imported = []
    for r in marker['records']:
        built = ready if r['row'] == 'ready' else model
        folder = earlier_folder(a, r)
        imported.append(dict(mark_reused(a, r), reuse_key=earlier_key(a, r, current_key(a, r['engine'], built), built.name)))
        dest = a.out / 'runs' / r['workload'] / str(r['repetition']) / r['engine'] / r['row']
        dest.parent.mkdir(parents=True, exist_ok=True)
        link_tree(folder, dest)
    for w in workloads:
        for kind in ('reference', 'ladder', 'work-precision', 'dt-sweep', 'horizons', 'diverged'):
            for src in (a.reuse / kind).glob(w + '*'):
                if src.name in (w, w + '.json'):
                    dest = a.out / kind / src.name
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    link_tree(src, dest)
        for src in (a.reuse / 'gates').glob(w + '-[0-9]*.json'):
            (a.out / 'gates').mkdir(parents=True, exist_ok=True)
            link_tree(src, a.out / 'gates' / src.name)
    records.extend(imported)
    save(a.out / 'done' / (name + '.json'), dict(records=imported))
    save(a.out / 'runs.json', records)
    progress(a, 'REUSED %s (every program: %s) from %s' % (name, ', '.join(sorted(sample)), a.reuse))
    return True


def series(a, source, name, records, *args, **kwargs):
    start = len(records)
    allowed = _series(a, source, name, records, *args, **kwargs)
    # A workload whose runs were valid but whose programs did not agree is a finished result too (a later run with the same keys
    # would only repeat it); one that crashed is not.
    finished = allowed or all(r['valid'] for r in records[start:])
    if finished and a.timed and len(records) > start and not (a.out / 'done' / (name + '.json')).exists():
        save(a.out / 'done' / (name + '.json'), dict(records=records[start:]))
    return allowed


def stop_here(a, records, driver_failures, name, why):
    progress(a, 'STOPPED at %s: %s' % (name, why))
    summarize(records, a.out)
    save(a.out / 'completion.json', dict(passed=False, check=a.check, rows=len(records), driver_failures=driver_failures, stopped_at=name, why=why))
    sys.exit('stopped at %s (--stop-on-failure): %s' % (name, why))


def effective_numerics(model):
    # The numerics the programs read: the model's options (its set file is converted into them).
    return numerics_of(Odex(model.with_suffix('.odex')))


def reaches_reference(c):
    # The screen tolerance on every value. Not screen.compare's time grid check: the reference prints its times as doubles and
    # the programs as single precision floats (0.05 and 0.050000001), the same times written differently.
    return c['same_shape'] and not c['non_finite_error'] and c['outside_tolerance'] == 0


REFERENCE_KEY = 'reference-key.json'  # beside a reference run: the key its program, inputs and interpreter had (W805)


def reference_key(a, model):
    # The reference is scipy_integrate.py, a file of the Python tree (its driver and numerics with it), run by this interpreter:
    # its key is Python's, so no compared program's key (xppautX's included) is part of it (W805).
    return current_key(a, 'python', model)


def reference_reused(a, folder, model):
    # Whether an earlier attempt's reference run of this folder had the same Python tree, inputs and interpreter (W805). An
    # attempt from before REFERENCE_KEY kept its key as derived_key derives a row's: from its provenance, command line and inputs.
    if not a.reuse:
        return False
    earlier = a.reuse / folder.relative_to(a.out)
    if (earlier / REFERENCE_KEY).exists():
        stored = dict(json.loads((earlier / REFERENCE_KEY).read_text()), inputs=program_inputs('python', earlier / model.name))
        return recorded_key(dict(engine='python', reuse_key=stored)) == reference_key(a, model)
    if not (earlier / 'result.json').exists() or not json.loads((earlier / 'result.json').read_text())['valid']:
        return False
    if not argv_unchanged(a, 'reference', earlier, model.name):
        return False
    cpus = [int(c) for c in json.loads((earlier / 'command.json').read_text())[2].split(',')]
    have = derived_key(a, dict(engine='python'), earlier / model.name, measure_sha(engine_argv), cpus)
    return same_key(have, reference_key(a, model))


def reference_run(a, model, folder):
    # The untimed reference run of `model` into `folder`, or the earlier attempt's run of it when reference_reused (W805).
    # Returns (result, reused).
    if reference_reused(a, folder, model):
        progress(a, 'reference %s REUSED from %s: Python tree and inputs unchanged' % (folder.relative_to(a.out), a.reuse))
        link_tree(a.reuse / folder.relative_to(a.out), folder)
        return json.loads((folder / 'result.json').read_text()), True
    result = launch(a, model, 'reference', folder, False)
    if result['valid']:
        save(folder / REFERENCE_KEY, reference_key(a, model))
    return result, False


def ladder_rungs(a, name, build):
    # The ladder's inputs, built without a run: the study's own model (its pilot, the reference's input) and one model per rung
    # below its tolerance, the loosest first.
    own = build(name + '-pilot-own', None, full=True)
    own_tolerance = effective_numerics(own).relative_tolerance
    return own, [(own_tolerance, own)] + [(t, build('%s-pilot-%g' % (name, t), t, full=True)) for t in LADDER_TOLERANCES if t < own_tolerance]


def ladder_import(a, name, engine, rungs, reference_unchanged):
    # One program's climb in an earlier ladder of this workload (W805): (its rung rows, the tolerance it reached or None), when its
    # program, measurement and inputs are unchanged at every rung it tried and the reference is unchanged; else (None, why). A
    # program's climb depends on no other program's, so the rows and where it stopped are its own.
    if not a.reuse:
        return None, 'no --reuse'
    earlier = a.reuse / 'ladder' / (name + '.json')
    if not earlier.exists():
        return None, 'no earlier ladder'
    if not reference_unchanged:
        return None, 'the reference differs'
    rows = [r for r in json.loads(earlier.read_text())['rungs'] if r['engine'] == engine]
    if not rows:
        return None, 'no earlier rung'
    models = dict(rungs)
    for row in rows:
        tolerance = row['tolerance']
        if tolerance not in models:
            return None, 'rung %g is not one of this ladder\'s' % tolerance
        key = current_key(a, engine, models[tolerance])
        have = earlier_key(a, row, key, models[tolerance].name, a.reuse / 'ladder' / name / ('%g' % tolerance) / engine)
        if not same_key(have, key):
            changed = [k for k in key if have is None or have.get(k) != key[k]]
            return None, 'rung %g differs in %s' % (tolerance, ', '.join(changed) or 'its command line')
        if not (a.reuse / 'ladder' / name / ('%g' % tolerance) / engine).is_dir():
            return None, 'rung %g has no earlier run folder' % tolerance
    reached = next((r['tolerance'] for r in rows if r['reached']), None)
    if reached is None and len(rows) != len(rungs):
        return None, 'the earlier climb stopped before the last rung'
    return rows, reached


def ladder(a, name, build, engines):
    # Untimed pilots: each program's loosest tolerance, the study's own then LADDER_TOLERANCES, whose full run reaches the
    # reference within the screen tolerance (the judge of the other comparisons). A program stops climbing at its first pass.
    # Returns {engine: tolerance, or None when no rung reached}, saving every rung's error beside it. A program whose ladder an
    # earlier attempt ran with the same program, measurement and inputs, and the same reference, takes that climb (W805); the
    # others climb here, rung by rung, on their own.
    folder = a.out / 'ladder' / name
    own, rungs = ladder_rungs(a, name, build)
    reference, reference_same = reference_run(a, own, folder / 'reference')
    if not reference['valid']:
        raise RuntimeError('%s: the reference run failed, so no program can be placed on the ladder' % name)
    chosen, tried, decided = {}, [], set()
    for engine in engines:
        rows, reached = ladder_import(a, name, engine, rungs, reference_same)
        if rows is None:
            if a.reuse:
                progress(a, '%s: %s climbs its ladder: %s' % (name, engine, reached))
            continue
        for row in rows:
            link_tree(a.reuse / 'ladder' / name / ('%g' % row['tolerance']) / engine, folder / ('%g' % row['tolerance']) / engine)
            tried.append(mark_reused(a, row))
        decided.add(engine)
        if reached is not None:
            chosen[engine] = reached
        progress(a, '%s: %s REUSED its ladder from %s (%d rungs)' % (name, engine, a.reuse, len(rows)))
    for tolerance, model in rungs:
        if len(decided) == len(engines):
            break
        for engine in [e for e in engines if e not in decided]:
            run = folder / ('%g' % tolerance) / engine
            result = launch(a, model, engine, run, measured=False)
            row = dict(engine=engine, tolerance=tolerance, valid=result['valid'], reached=False, reuse_key=current_key(a, engine, model))
            if result['valid']:
                c = screen.compare(run / 'output.dat.gz', folder / 'reference/output.dat.gz')
                row.update(reached=reaches_reference(c), max_abs_error=c['max_abs_error'], first_outside_time=c.get('first_outside_time'))
            tried.append(row)
            if row['reached']:
                chosen[engine] = tolerance
                decided.add(engine)
    result = {e: chosen.get(e) for e in engines}
    save(a.out / 'ladder' / (name + '.json'), dict(workload=name, chosen=result, rungs=tried, never=[e for e in engines if e not in chosen],
         note='never: no rung up to %g reached the reference within the screen tolerance' % LADDER_TOLERANCES[-1]))
    for e in engines:
        progress(a, '%s: %s timed at tolerance %s' % (name, e, '%g' % result[e] if result[e] else 'none (never within the screen tolerance up to %g)' % LADDER_TOLERANCES[-1]))
    return result, rungs


# Output spacings of the sweep, as multiples of the model's dt (the step XPPAUT 8.0 restarts at, so its step cap and its cost).
# 1 is the study's own; coarser spacings write fewer rows, so 64 is cheap here (it is the finer ones that cost row writing).
SPACING_FACTORS = (1, 4, 16, 64)


def earlier_sweeps(a):
    # The earlier study's valid sweep rows by (workload, engine, repetition, row) (W805).
    context = earlier_context(a)
    if 'sweeps' not in context:
        context['sweeps'] = {(r['workload'], r['engine'], r['repetition'], r['row']): r
                             for r in context['records'] if r.get('sweep') and r.get('valid')}
    return context['sweeps']


def imported_sweep(a, name, p, kind):
    # A sweep point's timed rows of an earlier attempt, one per repetition it counted (W805): (their records, None) when the point's
    # program, measurement and inputs are unchanged, else (None, why). A long point was counted once (its repetitions_reason).
    if not a.reuse:
        return None, 'no --reuse'
    row = '%s-%s' % (kind, p['key'])
    reps = list(repetitions(a))
    earlier = earlier_sweeps(a)
    found = [earlier.get((name, p['engine'], rep, row)) for rep in reps]
    present = [r for r in found if r is not None]
    missing = [rep for rep, r in zip(reps, found) if r is None]
    if not present:
        return None, 'no earlier %s row' % row
    if missing and not (min(missing) > 1 and any(r.get('repetitions_reason') for r in present)):
        return None, 'the earlier %s row has no repetition %d' % (row, min(missing))
    key = current_key(a, p['engine'], p['model'])
    for r in present:
        have = earlier_key(a, r, key, p['model'].name)
        if not same_key(have, key):
            changed = [k for k in key if have is None or have.get(k) != key[k]]
            return None, 'the earlier %s row differs in %s' % (row, ', '.join(changed) or 'its command line')
    return [dict(mark_reused(a, r), reuse_key=key) for r in present], None


def timed_points(a, name, records, kind, points):
    # Times each point with the study's protocol (warm-up, repetitions, rotating order) and puts its error against its
    # reference beside every row. A point: key, engine, model, tolerance, dt, reference (a folder). Rows carry `sweep`=kind.
    # A point an earlier attempt timed with the same program, measurement and inputs is imported, not timed again (W805).
    todo = []
    for p in points:
        found, _ = imported_sweep(a, name, p, kind)
        if found is None:
            todo.append(p)
            continue
        for rec in found:
            link_tree(earlier_folder(a, rec), a.out / 'runs' / name / str(rec['repetition']) / rec['engine'] / rec['row'])
        records.extend(found)
    errors, long_points = {}, set()
    for rep in repetitions(a):
        progress(a, '%s: %s repetition %d of %d%s' % (name, kind, rep, a.repeats, ' (warm-up, not counted)' if rep == 0 else ''))
        for p in rotation([p for p in todo if rep < 2 or (p['engine'], p['key']) not in long_points], rep):
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
                       tolerance=p['tolerance'], dt=p['dt'], timing_scope='whole silent process', reuse_key=current_key(a, p['engine'], p['model']),
                       **errors[(p['engine'], row)])
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


def work_points(a, name, records, rungs, chosen, engines):
    # The points work-precision times: every ladder rung for every program. A program's equal-accuracy point is the headline
    # series, timed already, so it is read from there (reused) and not timed again (a check times everything, untimed).
    reference = a.out / 'ladder' / name / 'reference'
    points, reused = [], []
    for tolerance, model in rungs:
        for e in engines:
            p = point_of(model, e, '%g' % tolerance, tolerance, reference)
            if not a.check and chosen[e] == tolerance:
                reused.append((p, equal_accuracy_rows(a, records, name, e)))
            else:
                points.append(p)
    return points, reused


def work_precision(a, name, records, rungs, chosen, engines):
    points, reused = work_points(a, name, records, rungs, chosen, engines)
    progress(a, '%s: work-precision, %d points timed, %d read from the equal-accuracy rows' % (name, len(points), len(reused)))
    timed_points(a, name, records, 'wp', points)
    write_curve(a, records, name, 'wp', 'work-precision', points, reused, 'time against max abs error versus the reference, per program, at every ladder tolerance')


def dt_points(a, name, build, records, rungs, chosen, engines):
    # The dt sweep's points at one fixed tolerance, the study's own (the loosest rung), and the (model, reference folder) pairs whose
    # reference runs they need. Returns (points, reused, references).
    tolerance = effective_numerics(rungs[0][1]).relative_tolerance
    base = effective_numerics(rungs[0][1]).dt
    points, reused, references = [], [], []
    for factor in SPACING_FACTORS:
        model = build('%s-dt%g' % (name, factor), tolerance, full=True, step=base * factor)
        reference = a.out / 'dt-sweep' / name / ('reference-x%g' % factor)
        references.append((model, reference))
        for e in engines:
            p = point_of(model, e, 'x%g' % factor, tolerance, reference)
            if factor == 1 and not a.check and chosen[e] == tolerance:
                reused.append((p, equal_accuracy_rows(a, records, name, e)))
            else:
                points.append(p)
    return points, reused, references


def dt_sweep(a, name, records, build, rungs, chosen, engines):
    # One fixed tolerance, the study's own (the loosest rung): there xppautX and Python take steps far larger than dt across
    # output points while XPPAUT's step is capped by dt, so the spacing sets XPPAUT's accuracy and cost (coarser: larger error, less work) while
    # xppautX's stay put. A tolerance every program already meets (the ladder's) would put all at the error floor.
    tolerance = effective_numerics(rungs[0][1]).relative_tolerance
    base = effective_numerics(rungs[0][1]).dt
    points, reused, references = dt_points(a, name, build, records, rungs, chosen, engines)
    for model, reference in references:
        if not reference_run(a, model, reference)[0]['valid']:
            raise RuntimeError('%s: the reference run at %s failed' % (name, reference.name))
    progress(a, '%s: dt sweep at tolerance %g, dt multiplied by %s, %d points timed' % (name, tolerance, ', '.join('%g' % f for f in SPACING_FACTORS), len(points)))
    timed_points(a, name, records, 'dt', points)
    write_curve(a, records, name, 'dt', 'dt-sweep', points, reused,
                'fixed tolerance %g (the study\'s own), dt = %g * factor; rows_written is what each run wrote' % (tolerance, base))


def builder(a, source, method, horizon):
    # (build, terman_acceptance, terman_out): one workload's input maker. The Terman network is timed only at the tolerance its
    # convergence study chose (--terman: that unit's attempt).
    terman_acceptance, terman_out = None, None
    if source.stem == 'episodic' and not a.check:
        criteria = HERE / 'terman-criteria.json'
        if not criteria.exists():
            raise RuntimeError('refusing to time Terman without predeclared terman-criteria.json')
        if not a.terman:
            raise RuntimeError('the Terman network is timed after its convergence study: pass --terman')
        terman_out = a.terman
        terman_acceptance = json.loads((terman_out / 'acceptance.json').read_text())
        if not terman_acceptance['passed'] or terman_acceptance.get('check_only'):
            raise RuntimeError('refusing to time Terman: convergence has not passed')

    def build(label, tolerance, ready=False, full=False, step=None):
        path = variant(a, source, label, ready=ready, method=method, total=None if ready else horizon, tolerance=tolerance, full=full, step=step)
        if terman_acceptance and tolerance is None:
            setfile_patch(path.parent / 'episodic.ode.set', {'Tolerance': terman_acceptance['chosen_tolerance']})
            convert(a, path)  # xppautX's .odex takes the patched SET
        return path
    return build, terman_acceptance, terman_out


def study_engines(a, native_only):
    return ['xppautx'] if native_only else ['xppautx', 'xppaut'] + (['python'] if a.study == 'python' else [])


def tolerance_inputs(build, name, model, ready, chosen, engines):
    # Each program's input pair (model, ready): the study's own at its tolerance, or the pair at the tolerance its ladder chose.
    by_tolerance = {effective_numerics(model).relative_tolerance: (model, ready)}
    inputs = {}
    for e in engines:
        t = chosen[e]
        if t not in by_tolerance:
            by_tolerance[t] = (build('%s-tol%g' % (name, t), t), build('%s-tol%g-ready' % (name, t), t, ready=True))
        inputs[e] = by_tolerance[t]
    return inputs


def plan_series(a, source, name, method, native_only):
    # --plan: what an earlier attempt of this workload supplies and what would run, by the keys the run uses (nothing is run).
    if not a.reuse:
        return write_plan(a, name, ['no earlier attempt'], 0)
    build, _, _ = builder(a, source, method, None)
    model, ready = build(name, None), build(name + '-ready', None, ready=True)
    marker, why = whole_reusable(a, name, model, ready)
    if marker:
        return write_plan(a, name, [], len(marker['records']))
    if a.study == 'python' and method in LADDER_METHODS:
        return plan_ladder(a, name, build, model, ready, study_engines(a, native_only))
    imported, would_run = 0, []
    rows = dict(zip(('run', 'ready'), (model, ready)))
    for engine in study_engines(a, native_only):
        imported += plan_timed(a, name, engine, rows, would_run)
    write_plan(a, name, would_run, imported)


def plan_timed(a, name, engine, rows, would_run):
    # One program's timed rows, repetition by repetition: imported when importable, else named in would_run. Returns how many it imports.
    imported = 0
    for rep in repetitions(a):
        if rep > 0 and earlier_diverged(a, name):
            break  # every engine aborted in the warm-up: no repetition follows
        found, why = importable(a, name, engine, rep, rows)
        if found:
            imported += len(found)
        elif not (rep > 1 and any(r.get('repetitions_reason') for r in earlier_rows(a).values() if r['workload'] == name and r['engine'] == engine)):
            would_run.append('%s repetition %d: %s' % (engine, rep, why))  # a long run has only one counted repetition (W452)
    return imported


def plan_ladder(a, name, build, model, ready, engines):
    # --plan of a ladder workload, by the checks the run makes (W805): each program's climb, then its timed rows at the tolerance
    # it reached and its sweep points are imported or named. A program whose climb would run is named by its climb alone: the
    # rows that follow it are the run's, not the plan's.
    own, rungs = ladder_rungs(a, name, build)
    reference_same = reference_reused(a, a.out / 'ladder' / name / 'reference', own)
    would_run, imported, chosen = [], 0, {}
    for engine in engines:
        rows, reached = ladder_import(a, name, engine, rungs, reference_same)
        if rows is None:
            would_run.append('%s ladder: %s' % (engine, reached))
            continue
        imported += len(rows)
        chosen[engine] = reached
    climbed = [e for e in engines if e in chosen]
    timed = [e for e in climbed if chosen[e]]
    inputs = tolerance_inputs(build, name, model, ready, chosen, timed)
    for engine in timed:
        imported += plan_timed(a, name, engine, dict(zip(('run', 'ready'), inputs[engine])), would_run)
    if timed and len(climbed) == len(engines):
        # The sweeps are the run's when some program reached the reference and every climb is imported: a run whose climbs all
        # stop short of it returns before any sweep (_series), and a climb that would run decides the sweeps itself (W827).
        points = {'wp': work_points(a, name, [], rungs, chosen, climbed)[0],
                  'dt': dt_points(a, name, build, [], rungs, chosen, climbed)[0]}
        for kind, kind_points in points.items():
            for p in kind_points:
                found, why = imported_sweep(a, name, p, kind)
                if found:
                    imported += len(found)
                else:
                    would_run.append('%s %s %s: %s' % (p['engine'], kind, p['key'], why))
    write_plan(a, name, would_run, imported)


def _series(a, source, name, records, method=None, native_only=False, horizon=None):
    build, terman_acceptance, terman_out = builder(a, source, method, horizon)
    model, ready = build(name, None), build(name + '-ready', None, ready=True)
    if a.reuse and reuse_whole(a, name, model, ready, records):
        return True
    engines = study_engines(a, native_only)
    # By default one input for every program; a ladder method gives each program the tolerance at which it reaches the reference.
    by_ladder = a.study == 'python' and method in LADDER_METHODS
    if by_ladder:
        chosen, rungs = ladder(a, name, build, engines)
        every_engine = engines
        engines = [e for e in engines if chosen[e]]
        if not engines:
            return True
        inputs = tolerance_inputs(build, name, model, ready, chosen, engines)
    else:
        inputs = {e: (model, ready) for e in engines}
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
        # An engine aborted: xppautX and Python's integrator say why and exit 1 (W480; W555: Python writes what it has too),
        # XPPAUT prints "Integration not completed" and exits 0, each writing what it has.
        run_failed = {e: any(r['exit_code'] != 0 for r in current if r['engine'] == e and r['row'] == 'run') for e in active}
        stopped = {e: ('Integration not completed' in logs[e]) if e == 'xppaut' else run_failed[e] for e in active}
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
                ref, _ = reference_run(a, model, a.out / 'reference' / name)
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


SCALING_CASES = BENCH / 'size-scaling/cases-ubuntu-20261020.json'


def scaling_workloads(a):
    # (case, duration, steps, name) of the size-scaling study's selected workloads: a case at one duration is one (W699).
    cases = json.loads(SCALING_CASES.read_text())
    if a.check:
        cases = [cases[0]]
    return [(c, duration, steps, c['name'] + '-' + duration) for c in cases for duration, steps in c['steps'].items()
            if selected(a, c['name'] + '-' + duration, c['name'])]


def stage_scaling(a, c, duration, steps, name):
    folder = a.out / 'models' / name
    folder.mkdir(parents=True)
    path = folder / (name + '.ode')
    scaling.write_model(path, c['family'], c['size'], min(steps, 2) if a.check else steps, c['rows'])
    convert(a, path)
    # XPPAUT cannot load models above its 5000-state ceiling.
    # Those rows retain xppautX data and an explicit unavailable row.
    if not c.get('legacy', True):
        save(folder / 'unavailable.json', dict(xppaut='XPPAUT 8.0 exceeds its 5000-state model limit'))
    return path


def study_workloads(a, paths):
    # (source, method, name) of the other studies' selected workloads: the python study runs each model at every method.
    for path in paths:
        methods = [None]
        if a.study == 'python':
            from driver import METHODS
            odex = Odex(path.with_suffix('.odex'))
            methods = ['discrete'] if odex.discrete else [m for m in METHODS if m != 'discrete']
            if a.check:
                methods = ['runge_kutta', *LADDER_METHODS]  # the ladder runs once, untimed
        for method in methods:
            name = path.stem + ('-' + method if method else '')
            if selected(a, name, path.stem):
                yield path, method, name


def main():
    p = parser(__doc__)
    p.add_argument('--study', choices=('screen', 'modeldb', 'research', 'izhikevich', 'python', 'scaling'), required=True)
    p.add_argument('--terman', type=Path, help='the Terman convergence unit\'s attempt folder: the network is timed at the tolerance it chose')
    a = p.parse_args()
    initialize(a)
    records, passed, driver_failures = [], True, []
    if a.study == 'scaling':
        todo = [(name, c, duration, steps) for c, duration, steps, name in scaling_workloads(a)]
    else:
        todo = [(name, path, method) for path, method, name in study_workloads(a, stage(a, a.study))]
    if a.list:
        print(json.dumps([t[0] for t in todo]))
        return
    for name, *what in todo:
        try:
            progress(a, 'WORKLOAD %s' % name)
            if a.study == 'scaling':
                c, duration, steps = what
                source, method, native_only = stage_scaling(a, c, duration, steps, name), None, not c.get('legacy', True)
            else:
                (source, method), native_only = what, False
            if a.plan:
                plan_series(a, source, name, method, native_only)
                continue
            ok = series(a, source, name, records, method, native_only)
            passed &= ok
            if a.stop_on_failure and not ok:
                stop_here(a, records, driver_failures, name, 'not equal work, see the log above')
        except Exception as e:
            progress(a, 'FAILED %s: %s' % (name, e))
            save(a.out / 'failures' / (name + '.json'), dict(error=str(e), type=type(e).__name__, traceback=traceback.format_exc()))
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
