#!/usr/bin/env -S uv run --locked python
"""W555: the runs the W354 series left without data, repeated untimed for the plots (overlays.py --extra).

    extra_runs.py integration --series S --out O --xppautx X --xppaut P   # Izhikevich 2004 (XPPAUT, short HOME) and the python study's diverging runs
    extra_runs.py lpc --series S --out O --xppaut P                      # XPPAUT on ode_mu26_blocked's full description

Each run goes through the series' own functions (integration.launch, auto_reference.reference), in a short scratch folder, and
with common.short_home() as its HOME: XPPAUT 8.0's AUTO file name is a 200-character array that a long HOME overflows (docs/xppaut-findings.md finding 75).
The result is copied to O/<study>/<workload>/<program>/ with command.json and environment.json.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import tempfile
import time
from types import SimpleNamespace

from common import done_attempts, save
import auto_reference as reference
from integration import launch

ENVIRONMENT_KEYS = ('HOME', 'XPP_CONFIG_DIR', 'LC_ALL', 'OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS')
IZHIKEVICH_2004 = ('izh_2004f_spike_frequency_adaptation', 'izh_2004j_subthreshold_oscillations', 'izh_2004o_threshold_variability',
                   'izh_2004q_depolarizing_afterpotential', 'izh_2004s_inhibition_induced_spiking', 'izh_2004t_inhibition_induced_bursting')
DIVERGING = ('episodic-euler', 'episodic-backward_euler', 'episodic-runge_kutta', 'kuramot100-qualrk')  # the series' diverged/*.json
PROGRAMS = ('xppautx', 'xppaut', 'python')
LPC_WORKLOAD = ('ode_mu26_blocked', 'adaptive')


def rerun(a, study, workload, programs, source, scratch):
    """Run each program on the inputs of the series run folder `source`, untimed, and keep what it wrote."""
    model = next(source.glob('*.ode'))
    for program in programs:
        folder = scratch / ('%s-%s' % (workload, program))
        result = launch(a, model, program, folder, measured=False)
        dest = a.out / study / workload / program
        shutil.copytree(folder, dest)
        env = {k: ('short_home(): a temp folder, removed after the run' if k == 'HOME' else os.environ.get(k, 'set by common.environment')) for k in ENVIRONMENT_KEYS}
        save(dest / 'environment.json', dict(env, note='common.environment(): HOME is common.short_home(); DISPLAY removed', original_folder=str(folder)))
        print(study, workload, program, 'exit', result['exit_code'], 'points', result.get('points'), flush=True)


def integration(a):
    scratch = Path(tempfile.mkdtemp(prefix='', dir=a.scratch))
    a.cores = [0]
    try:
        for name in IZHIKEVICH_2004:
            rerun(a, 'izhikevich', name, ('xppaut',), done_attempts(a.series, 'izhikevich')[name] / 'runs' / name / '0/xppaut/run', scratch)
        for name in DIVERGING:
            rerun(a, 'python', name, PROGRAMS, done_attempts(a.series, 'python')[name] / 'runs' / name / '0/xppautx/run', scratch)
    finally:
        shutil.rmtree(scratch)


def lpc(a):
    from continuation import start_values
    from odex_model import Odex
    mdir = done_attempts(a.series, 'continuation')[LPC_WORKLOAD[0]] / LPC_WORKLOAD[0] / LPC_WORKLOAD[1]
    d = json.loads((mdir / 'description.json').read_text())
    odex = Odex(Path(d['model']))
    eqs = reference.Equations(odex, d.get('parameters', {}))
    start = start_values((mdir / 'pilot-xppautx' / 'diagram.csv').read_bytes())
    dest = a.out / 'continuation' / LPC_WORKLOAD[0] / LPC_WORKLOAD[1] / 'xppaut'
    dest.mkdir(parents=True)
    save(dest / 'command.json', dict(binary=str(a.xppaut), driver='tools/auto_reference.py reference(...) -> execute_xppaut, on a private Xvfb',
                                     description=str(mdir / 'description.json'), runs=[r['name'] for r in d['runs']], untimed=True, run_limit_seconds=a.limit))
    args = SimpleNamespace(auto_dir=None, xppaut=str(a.xppaut), keep=dest / 'files')
    args.keep.mkdir()
    reference.RUN_LIMIT = a.limit  # the series' 600 s per run is what left lpc_soma and lpc_dend unavailable
    began = time.perf_counter()
    value = reference.reference('series', d, 'xppaut', args, (odex, eqs, start, False), None, process_env=dict(os.environ, LC_ALL='C'))
    save(dest / 'reference.json', value)
    save(dest / 'time.json', dict(wall_seconds=time.perf_counter() - began))
    save(dest / 'environment.json', dict(LC_ALL='C', note='HOME is execute_xppaut\'s own work folder; DISPLAY is its private Xvfb'))
    print('lpc', {r['name']: ('unavailable' if 'unavailable' in r else len(r.get('curve', []))) for r in value['runs']}, flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('study', choices=('integration', 'lpc'))
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--xppaut', type=Path, required=True)
    p.add_argument('--xppautx', type=Path)
    p.add_argument('--series', type=Path, help='the results folder (run.py: <study>/<unit>/)')
    p.add_argument('--limit', type=int, default=3 * 3600, help='lpc: seconds one XPPAUT run may take before it is given up')
    p.add_argument('--scratch', type=Path, default=Path('/tmp/w555'), help='a short folder for the runs')
    a = p.parse_args()
    a.out, a.xppaut = a.out.resolve(), a.xppaut.resolve()
    a.scratch.mkdir(parents=True, exist_ok=True)
    {'integration': integration, 'lpc': lpc}[a.study](a)


if __name__ == '__main__':
    main()
