"""W268 question 2: a parameter sweep of Morris-Lecar (examples/ode/lecar.odex, fixed-step RK4 at its dt), one run per GPU thread,
against xppautX's own rows and runs per second.

    uv run --locked --group gpu python paper/benchmarks/gpu/q2_sweep.py [--n 1000 10000 100000] [--bin PATH]

Checks (data, never pixels): the GPU's rows against xppautX's, bit for bit, in doubles (the model is run with `storage_precision: double`
so the data table holds the integrator's doubles; the .odex is copied with that one option added, never edited). Throughput (a probe on a
machine that is not quiet; run.py is the timed one): GPU runs per second for the three exp-family variants, xppautX's in-process
`run.range` in one process and in 16."""
import argparse
import concurrent.futures
from pathlib import Path
import statistics
import tempfile
import time

import numpy as np

from gpucommon import REPO, HERE, binary, c_double, build, cp, dev, gpu_seconds, odex_numbers, serve, spread, ulps

MODEL = REPO / 'examples' / 'ode' / 'lecar.odex'
SWEEP_PARAMETER, SWEEP_START, SWEEP_END = 'iapp', 0.0, 0.2  # the applied current: from rest through the oscillating range of the file
SAMPLE = 1000        # sweep values checked against xppautX one run at a time, by their final row
TRAJECTORIES = 12    # of those, runs whose all rows are compared
WORKERS = 16         # the machine's hardware threads: one xppautX process each
BLOCK = 128
VARIANTS = (
    ('CORE-MATH tanh/cosh, -fmad=false (xppautX bits)', 'crd_tanh', 'crd_cosh', True, dev.NVRTC_OPTIONS),
    ('CUDA tanh/cosh, -fmad=false', 'tanh', 'cosh', False, ('--std=c++17', '-fmad=false')),
    ('CUDA tanh/cosh, fmad on', 'tanh', 'cosh', False, ('--std=c++17',)),
)


def sweep_values(n):
    """What run.range gives run i: start + i * ((end - start) / steps), steps = n - 1."""
    return SWEEP_START + np.arange(n) * ((SWEEP_END - SWEEP_START) / (n - 1))


def kernel_source(tanh, cosh, with_core_math):
    pars, inits, (total, dt) = odex_numbers(MODEL)
    consts = ''.join('constexpr double %s = %s;\n' % (k, c_double(v)) for k, v in pars.items() if k != SWEEP_PARAMETER)
    consts += 'constexpr double V0 = %s, W0 = %s;\n' % (c_double(inits['v']), c_double(inits['w']))
    body = (HERE / 'kernels' / 'ml_rk4.cu').read_text().replace('@PARAMS@', consts).replace('@TANH@', tanh).replace('@COSH@', cosh)
    return (dev.device_source(('tanh', 'cosh')) if with_core_math else '') + body, total, dt


def launcher(variant, n, values, ntraj=0):
    label, tanh, cosh, core, options = variant
    source, total, dt = kernel_source(tanh, cosh, core)
    nsteps = round(total / dt)
    k = build(source, options, 'ml_rk4')
    xs = cp.asarray(values)
    out = cp.empty(3 * n, dtype=cp.float64)
    traj = cp.empty(ntraj * (nsteps + 1) * 3, dtype=cp.float64) if ntraj else None

    def launch():
        k(((n + BLOCK - 1) // BLOCK,), (BLOCK,), (xs, np.int64(n), np.int32(nsteps), np.float64(dt), out, traj if traj is not None else np.uint64(0), np.int32(ntraj)))
    return launch, out, traj, nsteps


def double_model(folder):
    """lecar.odex with storage_precision double in its integration group, the only change."""
    text = MODEL.read_text()
    old = 'integration: { total_time: 30, dt: .05 }'
    assert old in text
    path = Path(folder) / 'lecar_double.odex'
    path.write_text(text.replace(old, old[:-2] + ', storage_precision: double }'))
    return path


def cpu_rows(client, value, parameter=SWEEP_PARAMETER):
    client.command('value.set_parameters', values={parameter: float(value)})
    client.command('run.initial')
    return np.array(client.command('data.page', from_row=0, count=2000, columns=2)['rows'], dtype=np.float64)


def check(bin_path, ns):
    rng = np.random.default_rng(268)
    print('== bit-for-bit check: GPU rows against xppautX (%s)' % bin_path)
    with tempfile.TemporaryDirectory(prefix='w268') as tmp:
        model = double_model(tmp)
        with serve(model, bin_path) as xpp:
            for n in ns:
                values = sweep_values(n)
                sample = np.unique(np.concatenate([[0, n - 1], rng.choice(n, min(SAMPLE, n), replace=False)]))
                cpu_final = np.array([cpu_rows(xpp, values[i])[-1] for i in sample])  # t, v, w of each run's last row
                for variant in VARIANTS:
                    launch, out, _, nsteps = launcher(variant, n, values)
                    launch()
                    gpu_final = out.get().reshape(-1, 3)[sample]
                    d = ulps(gpu_final, cpu_final)
                    # whole trajectories of the first runs of the sample
                    sub = values[sample[:TRAJECTORIES]]
                    l2, _, traj, _ = launcher(variant, len(sub), sub, ntraj=len(sub))
                    l2()
                    t = traj.get().reshape(len(sub), nsteps + 1, 3)
                    rows_differ = sum(int((ulps(t[j], cpu_rows(xpp, sub[j])) != 0).any(axis=1).sum()) for j in range(len(sub)))
                    worst = float(np.max(np.abs(gpu_final[:, 1:] - cpu_final[:, 1:])))
                    print('N=%-6d %-50s final rows of %d runs: %d differ (max %d ulp, max |dv,dw| %.3g); %d trajectory rows of %d runs differ' %
                          (n, variant[0], len(sample), int((d != 0).any(axis=1).sum()), int(d.max()), worst, rows_differ, len(sub)))
            # the last run of an in-process sweep is the last value: run.range's own value arithmetic
            for n in ns:
                r = xpp.command('run.range', variable=SWEEP_PARAMETER, start=SWEEP_START, end=SWEEP_END, steps=n - 1, reset_storage=True)
                last = np.array(xpp.command('data.page', from_row=600, count=1, columns=2)['rows'][0])
                launch, out, _, _ = launcher(VARIANTS[0], n, sweep_values(n))
                launch()
                print('N=%-6d run.range (%d runs, %d steps): its last run equals the GPU\'s run %d: %s' % (n, r['runs_completed'], r['total_steps'], n - 1, bool((out.get().reshape(-1, 3)[-1] == last).all())))


def cpu_range(bin_path, model, n, workers):
    """Wall seconds for n runs of run.range split into `workers` xppautX processes (started and loaded before the clock)."""
    dp = (SWEEP_END - SWEEP_START) / (n - 1)
    per = [n // workers + (1 if k < n % workers else 0) for k in range(workers)]
    first = np.concatenate([[0], np.cumsum(per)[:-1]])
    clients = [serve(model, bin_path) for _ in range(workers)]
    try:
        def job(k):
            c = clients[k]
            if per[k] == 0:
                return 0
            c.command('run.range', variable=SWEEP_PARAMETER, start=SWEEP_START + first[k] * dp, end=SWEEP_START + (first[k] + per[k] - 1) * dp,
                      steps=max(per[k] - 1, 1) if per[k] > 1 else 1, reset_storage=True)
            return per[k]
        t0 = time.perf_counter()
        with concurrent.futures.ThreadPoolExecutor(workers) as pool:
            list(pool.map(job, range(workers)))
        return time.perf_counter() - t0
    finally:
        for c in clients:
            c.close()


def throughput(bin_path, ns, gpu_repeats, cpu_repeats):
    print('== throughput: runs per second, untimed (machine not quiet)')
    with tempfile.TemporaryDirectory(prefix='w268') as tmp:
        model = double_model(tmp)
        for n in ns:
            values = sweep_values(n)
            for variant in VARIANTS:
                launch, out, _, _ = launcher(variant, n, values)
                t = gpu_seconds(launch, gpu_repeats)
                tt = gpu_seconds(lambda: (launch(), out.get()), gpu_repeats)
                print('N=%-6d GPU %-50s kernel %s -> %.4g runs/s; with the copy back median %.4g s' % (n, variant[0], spread(t), n / statistics.median(t), statistics.median(tt)))
            for workers in (1, WORKERS):
                t = [cpu_range(bin_path, model, n, workers) for _ in range(cpu_repeats)]
                print('N=%-6d xppautX run.range in %2d process%s: %s -> %.4g runs/s' % (n, workers, ' ' if workers == 1 else 'es', spread(t), n / statistics.median(t)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--n', type=int, nargs='+', default=[1000, 10000, 100000])
    ap.add_argument('--bin')
    ap.add_argument('--gpu-repeats', type=int, default=5)
    ap.add_argument('--cpu-repeats', type=int, default=3)
    ap.add_argument('--only', choices=('check', 'throughput'))
    a = ap.parse_args()
    b = binary(a.bin)
    print('GPU %s; CuPy %s' % (cp.cuda.runtime.getDeviceProperties(0)['name'].decode(), cp.__version__))
    if a.only != 'throughput':
        check(b, a.n)
    if a.only != 'check':
        throughput(b, a.n, a.gpu_repeats, a.cpu_repeats)


if __name__ == '__main__':
    main()
