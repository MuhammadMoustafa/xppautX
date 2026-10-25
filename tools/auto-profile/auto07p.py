"""W412 (#474): ModelDB 267710's AUTO-07p translation run by AUTO-07p itself.

    AUTO_DIR=~/auto-07p-0.9.4 uv run --locked python tools/auto-profile/auto07p.py OUT [--marks P ...] [--constants JSON]
The steady run (c.6c.eq), then 20 periodic points from its rightmost HB (c.6c.po),
one thread, IID=3 (every Newton iterate in the diagnostics), NPR=1. --marks
stops at the last of these ampPar values (UZR); --constants overrides c.6c.po's
constants (EPSL, DS, ...). Writes OUT/summary.json: the stored points and, from
the periodic run's diagnostics (OUT/d.periodic), its Newton corrections, the
Jacobian builds (AUTO-07p's STEPBV builds one in each of a step's first NWTN
iterations), the special-point searches, the tests that started them and their
corrections, the fold-function values and the step sizes.
"""
import argparse, collections, json, os, re, shutil, subprocess, sys
from pathlib import Path

NWTN = 3  # c.6c.po's
POINTS = 20  # profile.py's ModelDB cut
MODEL = Path('/mnt/c/gitRepos/xppautX/paper/benchmarks/modeldb-267710/auto07p')
TESTS = {'User': 'uz', 'Fold': 'fold', 'BP': 'bp'}  # the diagnostics' test lines, as newton.py names them
CORRECTION = re.compile(r'\s*(-?\d+)\s+(-?\d+)\s+(\d+)\s+(\S+)\s+(\S+)\s*')  # BR PT IT PAR L2-NORM


def diagnostics(text):
    corrections, folds, steps, searching, test = [], [], [], False, None
    started = collections.Counter()
    for line in text.splitlines():
        fields = line.split()
        if 'Location of special point' in line:  # each trial's line, then Convergence
            trial = re.search(r'Iteration\s+(\d+)', line)
            if trial and trial[1] == '0':
                started[test] += 1
            searching = bool(trial)
        elif 'Possible special point' in line:
            searching = False
        elif (match := CORRECTION.fullmatch(line)) and int(match[3]) > 0:
            corrections.append({'point': int(match[2]), 'iteration': int(match[3]),
                                'parameter': float(match[4]), 'search': searching})
        elif len(fields) > 3 and fields[2] in TESTS and fields[3] in ('Func.', 'Function'):
            test = TESTS[fields[2]]
            if test == 'fold':
                folds.append({'point': int(fields[1]), 'value': float(fields[4])})
        elif fields[2:5] == ['Next', 'Step', ':']:
            steps.append({'point': int(fields[1]), 'step': float(fields[5])})
    if not corrections:
        raise ValueError('no Newton corrections in the diagnostics')
    return {'newton': len(corrections), 'jacobians': sum(c['iteration'] <= NWTN for c in corrections),
            'searches': started.total(), 'started_by': dict(started),
            'search_newton': sum(c['search'] for c in corrections),
            'fold_tests': len(folds), 'corrections': corrections, 'folds': folds, 'steps': steps}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('out', type=Path)
    ap.add_argument('--marks', type=float, nargs='+', default=[])
    ap.add_argument('--constants', type=json.loads, default={})
    args = ap.parse_args()
    auto = Path(os.environ['AUTO_DIR']).resolve()
    sys.path.insert(0, str(auto / 'python'))
    from auto import run, save
    out = args.out.resolve()
    out.mkdir(parents=True)
    for path in MODEL.iterdir():
        if path.is_file():
            shutil.copyfile(path, out / path.name)
    subprocess.run(['gcc', '-std=gnu17', '-g', '-O2', '-ffp-contract=off', '-I', str(auto / 'include'),
                    '-c', '6c.c', '-o', '6c.o'], cwd=out, check=True)
    os.environ['OMP_NUM_THREADS'] = '1'
    os.chdir(out)
    steady = run(e='6c', c='6c.eq')
    hb = max(steady('HB'), key=lambda solution: solution['PAR(1)'])
    save(steady, 'steady')
    marks = {'UZR': {1: args.marks}, 'STOP': [f'UZ{len(args.marks)}']} if args.marks else {}
    periodic = run(hb, c='6c.po', NMX=POINTS, IID=3, NPR=1, **marks, **args.constants)
    save(periodic, 'periodic')
    result = {'hb': hb['PAR(1)'], 'constants': args.constants, 'marks': args.marks,
              'points': [{'point': s['PT'], 'type': s['TY name'], 'parameter': s['PAR(1)'],
                          'period': s['PAR(11)']} for s in periodic()],
              **diagnostics((out / 'd.periodic').read_text())}
    (out / 'summary.json').write_text(json.dumps(result, indent=1) + '\n')
    print(out.name, {key: value for key, value in result.items() if isinstance(value, (int, float))})


if __name__ == '__main__':
    main()
