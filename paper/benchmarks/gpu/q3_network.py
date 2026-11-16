"""W268 question 3: the Morris-Lecar cable of examples/ode/lecdiff200.odex grown to 200, 2000 and 20000 cells (the file's text with the
array length changed), one integration, the right-hand side's cells computed in parallel per step on the GPU, against xppautX on the CPU.

    uv run --locked --group gpu python paper/benchmarks/gpu/q3_network.py [--n 200 2000 20000] [--bin PATH]

Growing the model: every `200` of the file (the array bound of the equations, the last cell of `v<n+1> = v<n>`'s boundary and of the
`init` ranges) becomes n and the boundary's 201 becomes n+1; the initial values (-0.49 from cell 20 on, 0 before; w = 0) and the
numerics (total 20, dt .05, Runge-Kutta: xppautX's defaults, as the file states none) are the file's. The model is given
`storage_precision: double`, so xppautX's rows are the integrator's doubles, which the GPU's are compared with, bit for bit."""
import argparse
from pathlib import Path
import re
import statistics
import tempfile
import time

import numpy as np

from gpucommon import REPO, HERE, binary, c_double, build, cp, dev, gpu_seconds, odex_numbers, serve, spread, ulps

MODEL = REPO / 'examples' / 'ode' / 'lecdiff200.odex'
FILE_CELLS = 200
BLOCK = 128
DATA_COLUMNS = 500  # the most columns one data.page request returns
VARIANTS = (
    ('CORE-MATH tanh/cosh, -fmad=false (xppautX bits)', 'crd_tanh', 'crd_cosh', True, dev.NVRTC_OPTIONS),
    ('CUDA tanh/cosh, fmad on', 'tanh', 'cosh', False, ('--std=c++17',)),
)


def grown_model(folder, cells):
    text = MODEL.read_text()
    text = text.replace('v%d = v%d' % (FILE_CELLS + 1, FILE_CELLS), 'v%d = v%d' % (cells + 1, cells))
    text = re.sub(r'\b%d\b' % FILE_CELLS, str(cells), text)
    text = text.replace('version 4', 'version 4\noptions { integration: { storage_precision: double } }', 1)
    path = Path(folder) / ('lecdiff%d.odex' % cells)
    path.write_text(text)
    return path


def initial_state(cells):
    """The file's init lines: v = -0.49 for cells 20..n, 0 for cells 1..19; w = 0 (not set)."""
    y = np.zeros(2 * cells)
    y[19:cells] = -0.49
    return y


def kernel(variant):
    label, tanh, cosh, core, options = variant
    pars, _, _ = odex_numbers(MODEL)
    consts = ''.join('constexpr double %s = %s;\n' % (k, c_double(v)) for k, v in pars.items())
    body = (HERE / 'kernels' / 'network_rk4.cu').read_text().replace('@PARAMS@', consts).replace('@TANH@', tanh).replace('@COSH@', cosh)
    return build((dev.device_source(('tanh', 'cosh')) if core else '') + body, options, 'stage')


class GpuRun:
    def __init__(self, variant, cells, nsteps, dt):
        self.k, self.n, self.nsteps, self.dt = kernel(variant), cells, nsteps, dt
        self.y0 = cp.asarray(initial_state(cells))
        self.y, self.acc = cp.empty_like(self.y0), cp.empty_like(self.y0)
        self.a, self.b = cp.empty_like(self.y0), cp.empty_like(self.y0)
        self.grid = ((cells + BLOCK - 1) // BLOCK,)
        self.graph = None

    def step(self):
        n, dt, g = np.int32(self.n), np.float64(self.dt), self.grid
        self.k(g, (BLOCK,), (np.int32(1), n, dt, self.y, self.acc, self.y, self.a))
        self.k(g, (BLOCK,), (np.int32(2), n, dt, self.y, self.acc, self.a, self.b))
        self.k(g, (BLOCK,), (np.int32(3), n, dt, self.y, self.acc, self.b, self.a))
        self.k(g, (BLOCK,), (np.int32(4), n, dt, self.y, self.acc, self.a, self.b))

    def run(self):
        """All the steps by launching four kernels each, from the initial state; the state is in self.y."""
        self.y[:] = self.y0
        for _ in range(self.nsteps):
            self.step()

    def run_graph(self):
        """The same launches recorded once as a CUDA graph (one step) and replayed nsteps times: the launch overhead the graph removes."""
        if self.graph is None:
            s = cp.cuda.Stream(non_blocking=True)
            with s:
                s.begin_capture()
                self.step()
                self.graph = s.end_capture()
            self.stream = s
        self.y[:] = self.y0
        cp.cuda.Stream.null.synchronize()
        for _ in range(self.nsteps):
            self.graph.launch(self.stream)
        self.stream.synchronize()


def cpu_final_row(xpp, cells, row):
    """The state of a stored row: v then w of every cell. data.page's rows are T then the block's columns, 500 at most at a time."""
    out = []
    for first in range(1, 2 * cells + 1, DATA_COLUMNS):
        count = min(DATA_COLUMNS, 2 * cells - first + 1)
        out += xpp.command('data.page', from_row=row, count=1, first_column=first, columns=count)['rows'][0][1:]
    return np.array(out, dtype=np.float64)


def measure(bin_path, ns, gpu_repeats, cpu_repeats, check_only=False):
    print('GPU %s; CuPy %s' % (cp.cuda.runtime.getDeviceProperties(0)['name'].decode(), cp.__version__))
    with tempfile.TemporaryDirectory(prefix='w268') as tmp:
        for n in ns:
            with serve(grown_model(tmp, n), bin_path) as xpp:
                times = []
                for _ in range(cpu_repeats):
                    t0 = time.perf_counter()
                    r = xpp.command('run.initial')
                    times.append(time.perf_counter() - t0)
                steps, total = r['steps'] - 1, r['time']
                dt = 0.05  # xppautX's default, the file states none; steps = total/dt
                cpu_state = cpu_final_row(xpp, n, steps)
            print('N=%-6d xppautX run.initial (%d steps, t=%.4g): %s' % (n, steps, total, spread(times)))
            for variant in VARIANTS:
                g = GpuRun(variant, n, steps, dt)
                g.run()
                cp.cuda.Stream.null.synchronize()
                d = ulps(g.y.get(), cpu_state)
                print('N=%-6d GPU %-50s final state: %d of %d values differ (max %d ulp)' % (n, variant[0], int((d != 0).sum()), len(d), int(d.max())))
                if check_only:
                    continue
                t_eager = gpu_seconds(g.run, gpu_repeats)
                t_graph = gpu_seconds(g.run_graph, gpu_repeats)
                print('N=%-6d GPU %-50s eager launches %s; CUDA graph %s' % (n, variant[0], spread(t_eager), spread(t_graph)))
                print('N=%-6d speed of the GPU over xppautX: eager %.3gx, graph %.3gx' % (n, statistics.median(times) / statistics.median(t_eager), statistics.median(times) / statistics.median(t_graph)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--n', type=int, nargs='+', default=[200, 2000, 20000])
    ap.add_argument('--bin')
    ap.add_argument('--gpu-repeats', type=int, default=3)
    ap.add_argument('--cpu-repeats', type=int, default=3)
    ap.add_argument('--check-only', action='store_true')
    a = ap.parse_args()
    measure(binary(a.bin), a.n, a.gpu_repeats, a.cpu_repeats, a.check_only)


if __name__ == '__main__':
    main()
