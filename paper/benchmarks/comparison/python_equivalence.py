#!/usr/bin/env -S uv run --locked python
"""Proves the Python integrators of today's tree give the very bytes W617's tree gave, and writes the record (W803, #884).

    uv run --locked python paper/benchmarks/comparison/python_equivalence.py [--units DIR] [--jobs N]

W617's Python rows are keyed by the hash of tools/python-integration/*.py as of its commit; that code has been
renamed and re-read from core/ since (W390, c14fd05b, W804), so no row of W617 matches today's hash though no number changed.
This script shows it: every distinct .odex the python study staged (DIR/python/UNIT/attempt-NNN/inputs/*/*.odex) and
Terman's dop853 reference (DIR/terman/episodic/attempt-NNN/models/episodic.odex) is run by both trees, W617's (git archive of
its commit, in a temporary folder) and this one, untimed, with the driver's argv (`integrate.py MODEL.odex` in the
model's folder), and the numerics each tree builds (every field) and the output bytes are compared. A run longer than
RUN_SECONDS goes on a prefix: the model's total_time (the reference's last time) cut by the first of PREFIXES that both
trees finish in time, named in the record. The record, python-equivalence.json, is what common.proven_python_program reads:
it licenses the key of the Python arm only (all_identical, and both hashes still the ones named).
Rerun this one command whenever tools/python-integration changes and the record is to stay valid.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from common import EQUIVALENCE, REPO, python_sha, tree_sha

W617_COMMIT = '9885a178ae3ab8f365c49ed62d2d723aad727ded'  # the commit W617's runs were made at (its provenance.json)
RUN_SECONDS = 120  # a run longer than this on either tree is cut to a prefix: the proof is of bytes, not of speed
PREFIXES = (1.0, 0.1, 0.01, 0.001)  # the fractions of the run tried in order
TERMAN_END, TERMAN_STEP = 1999, .4  # terman.py's reference: times np.arange(0, 1999, .4)
TOTAL = re.compile(r'(total_time:\s*)([-0-9.eE+]+)')
# W804 turned the fixed crossing fraction `stol` into the option global.min_step_fraction: the earlier tree's field under its
# new name (same 1e-10 default, same use in flags.py), the one rename between the trees' numerics namespaces
RENAMED_FIELDS = {'stol': 'global_min_step_fraction'}
# what each tree's own code is made to print: its numerics, and its dop853 reference rows
SNIPPET = '''
import json, sys
tree, odex, what, end = sys.argv[1:5]
sys.path[:0] = [tree + "/tools/python-integration", tree + "/tools"]
from driver import load
model, numerics = load(odex)
if what == "numerics":
    print(json.dumps(vars(numerics), sort_keys=True))
else:
    import numpy as np
    from driver import format_rows
    from scipy_integrate import run_dop853
    rows, _ = run_dop853(model, numerics, np.arange(0, float(end), %s))
    open("output.dat", "w", encoding="utf-8").write(format_rows(rows))
''' % TERMAN_STEP


def program_sha(tree):
    # W617's key of the Python arm: its files alone (today's python_sha adds the options schema's defaults, W804)
    return tree_sha((f.name, f.read_bytes()) for f in (Path(tree) / 'tools' / 'python-integration').glob('*.py'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(argv, folder):
    # (exit code, None) or (None, the timeout): output.dat is left in folder
    try:
        done = subprocess.run(argv, cwd=folder, capture_output=True, text=True, timeout=RUN_SECONDS)
    except subprocess.TimeoutExpired:
        return None, 'timeout'
    return done.returncode, done.stderr[-300:]


def numerics_of(tree, odex, folder):
    done = subprocess.run([sys.executable, '-c', SNIPPET, tree, str(odex), 'numerics', '0'], cwd=folder, capture_output=True, text=True)
    if done.returncode:
        return 'failed: ' + done.stderr[-300:]
    return {RENAMED_FIELDS.get(k, k): v for k, v in json.loads(done.stdout).items()}


def prefixed(text, fraction):
    # the same text with total_time cut to a fraction of itself, exactly as written for both trees
    return TOTAL.sub(lambda m: m.group(1) + repr(float(m.group(2)) * fraction), text, count=1) if fraction != 1.0 else text


def compare(trees, odex, terman, scratch):
    # {numerics equal, mode, fraction, rows, sha of each tree's output, identical} for one model
    text = Path(odex).read_text(encoding='utf-8')
    result = dict(odex_sha256=sha(odex))
    for fraction in PREFIXES:
        outputs = {}
        for name, tree in trees.items():
            folder = Path(tempfile.mkdtemp(dir=scratch))
            if terman:
                shutil.copy2(odex, folder / 'episodic.odex')
                argv = [sys.executable, '-c', SNIPPET, tree, 'episodic.odex', 'dop853', repr(TERMAN_END * fraction)]
            else:
                (folder / Path(odex).name).write_text(prefixed(text, fraction), encoding='utf-8')
                argv = [sys.executable, str(Path(tree) / 'tools' / 'python-integration' / 'integrate.py'), Path(odex).name]
            code, why = run(argv, folder)
            output = folder / 'output.dat'
            outputs[name] = (code, why, sha(output) if output.exists() else None, output)
            if code is None:
                shutil.rmtree(folder, ignore_errors=True)
        if any(o[0] is None for o in outputs.values()):
            continue  # too long: the next shorter prefix
        result.update(mode='full' if fraction == 1.0 else 'prefix', fraction=fraction,
                      exit_codes={n: o[0] for n, o in outputs.items()},
                      rows=sum(1 for _ in outputs['current'][3].open()) if outputs['current'][3].exists() else 0,
                      output_sha256={n: o[2] for n, o in outputs.items()},
                      identical=outputs['earlier'][2] is not None and outputs['earlier'][2] == outputs['current'][2]
                      and outputs['earlier'][0] == outputs['current'][0],
                      stderr_equal=outputs['earlier'][1] == outputs['current'][1])
        for o in outputs.values():
            shutil.rmtree(o[3].parent, ignore_errors=True)
        return result
    result.update(mode='none', identical=False)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('--units', type=Path, default=Path.home() / 'xppautx-comparison' / 'results', help="the comparison's results folder (W617's attempts)")
    p.add_argument('--jobs', type=int, default=3)
    p.add_argument('--out', type=Path, default=EQUIVALENCE)
    a = p.parse_args()
    with tempfile.TemporaryDirectory() as scratch:
        scratch = Path(scratch)
        earlier = scratch / 'earlier'
        earlier.mkdir()
        archive = subprocess.run(['git', 'archive', W617_COMMIT, 'core', 'tools'], cwd=REPO, capture_output=True, check=True).stdout
        subprocess.run(['tar', '-x', '-C', str(earlier)], input=archive, check=True)
        trees = {'earlier': str(earlier), 'current': str(REPO)}
        # the distinct models of the python study (the staged inputs of every unit and its ready/ladder variants) by content
        variants = {}
        for f in sorted(a.units.glob('python/*/attempt-*/inputs/*/*.odex')):
            variants.setdefault(sha(f), (f, []))[1].append(f.relative_to(a.units / 'python').parts[0] + '/' + f.parts[-2])
        terman_model = sorted(a.units.glob('terman/episodic/attempt-*/models/episodic.odex'))[-1]
        jobs = [(model, names, False) for model, names in variants.values()] + [(terman_model, ['terman dop853 reference'], True)]

        def one(job):
            model, names, terman = job
            folder = Path(tempfile.mkdtemp(dir=scratch))
            shutil.copy2(model, folder / model.name)
            found = compare(trees, model, terman, scratch)
            found['numerics_equal'] = numerics_of(trees['earlier'], folder / model.name, folder) == numerics_of(trees['current'], folder / model.name, folder)
            found['kind'] = 'terman dop853 reference' if terman else 'python study'
            found['units'] = sorted(set(names))
            print('%-28s %-7s %s' % (found['units'][0], found.get('mode'), 'identical' if found['identical'] and found['numerics_equal'] else 'DIFFERENT'), flush=True)
            return found

        with ThreadPoolExecutor(a.jobs) as pool:
            units = sorted(pool.map(one, jobs), key=lambda u: (u['kind'], u['units']))
        record = dict(earlier_commit=W617_COMMIT, earlier_program_sha256=program_sha(earlier), current_program_sha256=python_sha(),
                      units=units, all_identical=all(u['identical'] and u['numerics_equal'] for u in units))
    a.out.write_text(json.dumps(record, indent=1, sort_keys=True) + '\n', encoding='utf-8')
    print('all identical' if record['all_identical'] else 'DIFFERENCES: see ' + str(a.out))
    sys.exit(0 if record['all_identical'] else 1)


if __name__ == '__main__':
    main()
