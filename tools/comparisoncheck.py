#!/usr/bin/env -S uv run --locked python
"""W543: untimed checks of the tracked comparison drivers, on scratch inputs.

Run from the checkout: uv run --locked python tools/comparisoncheck.py
No warm-ups, repetitions, readiness sampling or timing summaries are run.
"""
import argparse
import ast
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace

from autocompare import completed
from xppclient import Checker, Server, open_auto, whole_series

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT / 'paper/benchmarks'
sys.path.insert(0, str(BENCH / 'comparison'))
from common import module
import integration
import continuation
import terman

LIMIT = 60  # safety limit for a hung smoke subprocess; never a speed assertion
AUTO_POINTS = 5  # exercise streaming and completion without a continuation series


def integration_case(a, study, name, check):
    """Use the study's own staging, short-check variant and untimed launcher."""
    out = a.out / (study + '-' + name)
    out.mkdir()
    args = SimpleNamespace(**(vars(a) | dict(out=out, only=name, check=True)))
    sources = integration.stage(args, study)
    source = next(p for p in sources if p.stem == name)
    model = integration.variant(args, source, name)
    for engine in ('xppautx', 'xppaut'):
        if engine == 'xppaut' and not a.xppaut.is_file():
            print('SKIP %s/XPPAUT: executable missing: %s' % (study, a.xppaut))
            continue
        result = integration.launch(args, model, engine, out / engine, measured=False)
        check(study + '/' + engine, result['valid'], result)
    if study == 'python':
        result = integration.launch(args, model, 'python', out / 'python', measured=False)
        check('python/python', result['valid'], result)
    if study == 'screen':
        counts = module('compile_counts', BENCH / 'five-model-screen/compile_counts.py')
        count = counts.count(str(a.xppautx), model.with_suffix('.odex'))
        check('screen/compile_counts', count['compiled'] == count['programs'] and not count['not_compiled'], count)
    return args, model


def terman_case(a, check):
    args, model = integration_case(a, 'screen', 'episodic', check)
    odex = integration.Odex(model.with_suffix('.odex'))
    criteria = json.loads(terman.CRITERIA.read_text())
    rows = terman.array(args.out / 'xppautx/output.dat')
    readouts = terman.readouts(rows, odex.states, criteria)
    check('terman/readouts', readouts['final_time'] > 0 and len(readouts['counts']) == 20, readouts)


def scaling_case(a, check):
    folder = a.out / 'scaling'
    folder.mkdir()
    case = json.loads((BENCH / 'size-scaling/cases-ubuntu-20261020.json').read_text())[0]
    model = folder / (case['name'] + '-step.ode')
    integration.scaling.write_model(model, case['family'], case['size'], case['steps']['step'], case['rows'])
    integration.convert(a, model)
    for engine in ('xppautx', 'xppaut'):
        if engine == 'xppaut' and not a.xppaut.is_file():
            print('SKIP scaling/XPPAUT: executable missing: ' + str(a.xppaut))
            continue
        result = integration.launch(a, model, engine, folder / engine, measured=False)
        check('scaling/' + engine, result['valid'], result)


def continuation_case(a, check):
    args = SimpleNamespace(**(vars(a) | dict(out=a.out / 'continuation', only='V1-tstauto_simplefold',
                                          check=True, plan=False, pilots=False)))
    args.out.mkdir()
    name, description, _, _ = next(continuation.workloads(args))
    for events in (True, False):
        result = initial = continuation.native(args, description, args.out / str(events), 1, events, 'difference', measured=False)
        check('continuation/diagram=' + str(events), bool(result['data']) and all(hi > lo for lo, hi in result['ranges'].values()))
    odex = continuation.Odex(continuation.REPO / description['model'])
    eqs = continuation.reference.Equations(odex, description.get('parameters', {}))
    inputs = odex, eqs, continuation.start_values(initial['data']), False
    for program in ('xppaut', 'auto07p'):
        dependency = a.xppaut if program == 'xppaut' else a.auto_dir / 'lib/libauto_c.a'
        if not dependency.is_file():
            print('SKIP continuation/%s: build missing: %s' % (program, dependency))
            continue
        result, _ = continuation.foreign(args, description, program, inputs, args.out / program, measured=False)
        check('continuation/' + program, bool(result['runs']) and all(r.get('curve') and 'unavailable' not in r for r in result['runs']), result)


def auto_case(a, check):
    """The older AUTO driver, on its original steady-state input and settings."""
    sys.path.insert(0, str(BENCH / 'auto'))
    import hopf_xppautx
    from explore_xppautx import step, numerics
    s = Server(str(a.xppautx), str(BENCH / 'auto/models/PY_S1Bf_bench.ode'))
    try:
        step(s)
        step(s, cmd='events.subscribe', events=[], diagram=True)
        step(s, cmd='auto.set_threads', threads=1)
        open_auto(s)
        numerics(s, max_steps=AUTO_POINTS)
        _, _, _, points = hopf_xppautx.run(s, 's', limit=LIMIT)
        check('auto/steady driver', len(points) == AUTO_POINTS, points)
    finally:
        s.close()


def modeldb_auto_case(a, check):
    driver = module('modeldb_auto', BENCH / 'modeldb-267710/auto_xppautx.py')
    folder = a.out / 'modeldb-auto'
    shutil.copytree(driver.MODEL_DIR, folder)
    s = Server(str(a.xppautx), str(folder / driver.MODEL), run=str(folder))
    try:
        completed(s)
        completed(s, dict(cmd='events.subscribe', events=[], diagram=True))
        completed(s, dict(cmd='run.configure', total_time=.1))
        for command in driver.INTEGRATE:
            events = completed(s, dict(cmd=command))
            check('modeldb-auto/' + command, any(e.get('ev') == 'job.computing' for e in events))
        open_auto(s)
        driver.numerics(s, max_steps=AUTO_POINTS)
        _, _, _, points = driver.run(s, 's', limit=LIMIT)
        check('modeldb-auto/steady driver', len(points) == AUTO_POINTS, points)
    finally:
        s.close()
    print('SKIP old AUTO full periodic workflows/foreign variants: Hopf searches and periodic branches use the full workloads; no under-minute bound. Shared auto.run adapter exercised above; continuation covers the small foreign case.')


def vectorization_case(a, check):
    # Its smallest existing server model; keep its configured method and run length.
    model = BENCH / 'vectorization-audit/models/lecar_nullc_dfield.odex'
    env = dict(os.environ, BIN=str(a.xppautx))
    result = subprocess.run(['uv', 'run', '--locked', 'python', str(BENCH / 'vectorization-audit/server_cg.py'), str(model)],
                            cwd=ROOT, env=env, capture_output=True, text=True, timeout=LIMIT)
    check('vectorization/server driver', result.returncode == 0, result.stdout + result.stderr)
    result = subprocess.run(['uv', 'run', '--locked', 'python', str(BENCH / 'vectorization-audit/autocg.py'), 'tstauto_simplefold'],
                            cwd=ROOT, env=env, capture_output=True, text=True, timeout=LIMIT)
    check('vectorization/AUTO driver', result.returncode == 0, result.stdout + result.stderr)


def large_stiff_case(a, check):
    generator = module('stiff_generate', BENCH / 'large-stiff/generate.py')
    for family in ('cable', 'ring'):
        model = generator.generate(family, 6, a.out / 'large-stiff')
        s = Server(str(a.xppautx), str(model))
        try:
            completed(s)
            completed(s, dict(cmd='events.subscribe', events=['plot.series']))
            events = completed(s, dict(cmd='run.initial'))
            series = whole_series(events)
            check('large-stiff/' + family, bool(series) and series[-1]['rows'] == 101)
        finally:
            s.close()
    print('SKIP large-stiff external worker: requires its own worktree-linked libstudy.so and pinned SUNDIALS build; generated inputs exercised against this worktree binary.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, default=ROOT / 'xppautX')
    parser.add_argument('--comparison-build', type=Path, default=Path.home() / 'xppautx-comparison/build')
    options = parser.parse_args()
    check = Checker()
    # Parse without importing: several historical scripts execute their series at import.
    sources = list(BENCH.rglob('*.py'))
    for path in sources:
        ast.parse(path.read_text(), filename=str(path))
    check('tracked Python syntax (%d files)' % len(sources), True)
    with tempfile.TemporaryDirectory(prefix='w543-smoke-') as scratch:
        args = SimpleNamespace(out=Path(scratch), xppautx=options.server.resolve(),
                               xppaut=options.comparison_build / 'xppaut-8.0/xppaut',
                               auto_dir=options.comparison_build / 'auto-07p',
                               cores=sorted(os.sched_getaffinity(0))[:1])
        studies = [('terman', lambda: terman_case(args, check)),
                   *[(study, lambda study=study, name=name: integration_case(args, study, name, check))
                     for study, name in [('screen', 'kuramot100'), ('modeldb', 'rest'), ('research', 'ma23'),
                                         ('izhikevich', 'izh_2004a_tonic_spiking'), ('python', 'kuramot100')]],
                   ('continuation', lambda: continuation_case(args, check)),
                   ('scaling', lambda: scaling_case(args, check)),
                   ('auto', lambda: auto_case(args, check)),
                   ('modeldb-auto', lambda: modeldb_auto_case(args, check)),
                   ('vectorization', lambda: vectorization_case(args, check)),
                   ('large-stiff', lambda: large_stiff_case(args, check))]
        for name, run in studies:
            try:
                run()
            except Exception as error:
                check(name, False, str(error))
    print('%d checks, %d failures' % (check.checks, check.failures))
    return int(bool(check.failures))


if __name__ == '__main__':
    sys.exit(main())
