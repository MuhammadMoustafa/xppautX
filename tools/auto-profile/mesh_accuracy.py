"""W412 (#474): independent off-collocation ODE defect of saved BVP polynomials.

    python3 tools/auto-profile/mesh_accuracy.py OUT.json TRACE_DIR... --at PARAMETER...
For each profile.py --tool newton run, the stored point at each PARAMETER (a mark
the run located). The model is ModelDB's AUTO-07p C translation, built as a
library (system libm), so no continuation, assembly or Newton code of either
program is reused. At four non-collocation locations in each interval, compare
the derivative of the degree-NCOL interpolant with the model RHS. Also
integrate each whole interval independently with RK4 and halve its step: the
difference estimates the reference integration's own uncertainty. Units are
the model's state/ms; relative defects use 1 + the largest sampled absolute
RHS for each state.
"""
import argparse, ctypes, json, math, subprocess, tempfile
from pathlib import Path

PERIOD_INDEX = 10  # ModelDB's AUTO translation uses PAR(1) and PAR(11).
MAX_RK4_STEP = 0.00025  # ms: well inside stability for the observed ~-2220/ms fast mode.
MARK_TOLERANCE = 1e-9  # a located mark at EPSS=1e-8 is within 2.2e-13 of its value
MODEL = '/mnt/c/gitRepos/xppautX/paper/benchmarks/modeldb-267710/auto07p/6c.c'
AUTO_INCLUDE = '/mnt/c/gitRepos/xppautX/reference/auto-07p/include'  # auto_f2c.h


def weights(n, x):
    nodes = [i / n for i in range(n + 1)]
    values, derivatives = [], []
    for k in range(n + 1):
        others = [i for i in range(n + 1) if i != k]
        denominator = math.prod(nodes[k] - nodes[i] for i in others)
        values.append(math.prod(x - nodes[i] for i in others) / denominator)
        derivatives.append(sum(math.prod(x - nodes[i] for i in others if i != m)
                               for m in others) / denominator)
    return values, derivatives


def rk4(rhs, state, h, steps):
    state = list(state)
    h /= steps
    for _ in range(steps):
        a = rhs(state)
        b = rhs([u + h * v / 2 for u, v in zip(state, a)])
        c = rhs([u + h * v / 2 for u, v in zip(state, b)])
        d = rhs([u + h * v for u, v in zip(state, c)])
        state = [u + h * (a + 2*b + 2*c + d) / 6
                 for u, a, b, c, d in zip(state, a, b, c, d)]
    return state


def assess(row, function):
    n, c, ld = row['ndim'], row['ncol'], row['ld']
    vector = ctypes.c_double * n
    par = (ctypes.c_double * (PERIOD_INDEX + 1))(row['parameter'], *([0.0] * (PERIOD_INDEX - 1)), row['period'])
    icp = (ctypes.c_int * 2)(0, PERIOD_INDEX)
    output = vector()

    def rhs(state):
        function(n, vector(*state), icp, par, 0, output, None, None)
        if not all(math.isfinite(v) for v in output):
            raise ArithmeticError('independent model evaluation is not finite')
        return list(output)

    matrices = [weights(c, (k + 0.5) / c) for k in range(c)]
    defect, magnitude, closure, reference = ([0.0] * n for _ in range(4))
    extrema = [math.inf, -math.inf]
    for j in range(row['ntst']):
        nodes = [[row['ups'][ld * (i + n*k) + j] for i in range(n)] for k in range(c)]
        nodes.append([row['ups'][ld*i + j + 1] for i in range(n)])
        h = row['period'] * (row['tm'][j + 1] - row['tm'][j])
        for w, dw in matrices:
            state = [sum(w[k] * nodes[k][i] for k in range(c+1)) for i in range(n)]
            deriv = [sum(dw[k] * nodes[k][i] for k in range(c+1)) / h for i in range(n)]
            f = rhs(state)
            for i in range(n):
                defect[i] = max(defect[i], abs(deriv[i] - f[i]))
                magnitude[i] = max(magnitude[i], abs(f[i]))
            extrema = [min(extrema[0], state[0]), max(extrema[1], state[0])]
        steps = max(1, math.ceil(abs(h) / MAX_RK4_STEP))
        a, b = rk4(rhs, nodes[0], h, steps), rk4(rhs, nodes[0], h, 2 * steps)
        for i in range(n):
            closure[i] = max(closure[i], abs(b[i] - nodes[-1][i]))
            reference[i] = max(reference[i], abs(a[i] - b[i]))
    return {'point': row['ntot'], 'parameter': row['parameter'], 'period': row['period'],
            'voltage_range': extrema, 'rhs_max': magnitude, 'defect': defect,
            'scaled_defect': max(d / (1 + f) for d, f in zip(defect, magnitude)),
            'periodicity': max(abs(row['ups'][ld*i] - row['ups'][ld*i + row['ntst']]) for i in range(n)),
            'interval_closure': closure, 'rk4_step_halving_difference': reference}


def model(library):
    """the C translation's func, through AUTO's exported user table (auto_f2c.h makes func static)"""
    pointer = ctypes.POINTER(ctypes.c_double)
    address = ctypes.c_void_p.in_dll(ctypes.CDLL(library), 'user').value
    return ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, pointer, ctypes.POINTER(ctypes.c_int), pointer,
                            ctypes.c_int, pointer, pointer, pointer)(address)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('out', type=Path)
    ap.add_argument('--at', type=float, nargs='+', required=True)
    ap.add_argument('folders', type=Path, nargs='+')
    args = ap.parse_args()
    with tempfile.TemporaryDirectory(prefix='w412-model-') as scratch:
        library = f'{scratch}/model.so'
        subprocess.run(['gcc', '-std=gnu17', '-O2', '-ffp-contract=off', '-shared', '-fPIC', '-I', AUTO_INCLUDE,
                        MODEL, '-o', library, '-lm'], check=True)
        function = model(library)
        results = {}
        for folder in args.folders:
            rows = [row for line in (folder / 'modeldb.newton.jsonl').read_text().splitlines()
                    if (row := json.loads(line))['event'] == 'stplbv']
            results[folder.name] = []
            for parameter in args.at:
                row = min(rows, key=lambda r: abs(r['parameter'] - parameter))
                if abs(row['parameter'] - parameter) > MARK_TOLERANCE:
                    raise ValueError(f'{folder}: no stored point at {parameter}')
                results[folder.name].append(assess(row, function))
                print(folder.name, parameter, 'scaled defect', results[folder.name][-1]['scaled_defect'])
    args.out.write_text(json.dumps(results, indent=1) + '\n')


if __name__ == '__main__':
    main()
