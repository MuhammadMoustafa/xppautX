"""W268 question 5: autoX's collocation against a GPU's launch latency.

    uv run --locked --group gpu python paper/benchmarks/gpu/q5_launch.py

Prints (1) the work of one mesh interval of a typical run (tests/autox/ode_lecar.json: Morris-Lecar's cycles, 15 intervals, 4 collocation
points; ndim 2), counted from core/setubv2.cpp and core/conpar2.cpp's loops, and (2) what a GPU costs to do anything at all: an empty
kernel's launch-to-completion time, back-to-back launches, a host-device round trip of the mesh's data, and one launch that computes
a mesh's worth of right-hand sides (420 cells of q3's kernel with CORE-MATH's functions: its stage 1)."""
import json
import statistics
import time

import numpy as np

from gpucommon import REPO, build, cp, gpu_seconds
import q3_network

DESCRIPTION = REPO / 'tests' / 'autox' / 'ode_lecar.json'
NDIM = 2        # Morris-Lecar's v and w
PARAMETERS = 2  # free parameters of a periodic run: the continued one and the period (AUTO's NFPR for orbits)
EMPTY = 'extern "C" __global__ void empty() {}'


def interval_work(ndim, ncol, ncb, nint=1):
    """One mesh interval's work in setubv (the Jacobian pass of setubv_make_fa_aa_bb) and conpar (the elimination), as the loops are written.

    rhs evaluations per collocation point: funi differences the right-hand side by central differences in each of the ndim states
    (2 ndim evaluations) and forward in each of the ncb parameters, on top of the value (1 + 2 ndim + ncb). The residual pass calls funi again
    (docs/xppaut-findings.md 39: its Jacobian is discarded), so a second set is counted for that, and the figure is a range."""
    per_point = 1 + 2 * ndim + ncb
    rhs = ncol * per_point
    nra, nca = ndim * ncol, ndim * (ncol + 1)
    nov = ndim
    assemble = ncol * ((ncol + 1) * ndim * ndim + ndim * ncb) * 2 + ncol * ndim * (ncol + 1) * 2  # AA/BB fill and the residual's weighted sums
    # conpar_process: for each pivot column, each lower row subtracts the pivot row over (left + rest of the columns + ncb), a multiply-add each
    steps = nca - nov - nov
    elimination = 0
    for col in range(nov, nov + steps):
        pivot_row = col - nov
        elimination += (nra - pivot_row - 1) * (nov + (nca - col - 1) + ncb) * 2
    return dict(rhs_evaluations=rhs, rhs_evaluations_with_residual_pass=2 * rhs, assembly_flops=assemble, elimination_flops=elimination, rows=nra, columns=nca)


def main():
    d = json.loads(DESCRIPTION.read_text())
    cycles = next(r for r in d['runs'] if r['name'] == 'cycles')['autox']
    ntst, ncol = cycles['mesh.intervals'], cycles.get('mesh.collocation_points', 4)  # 4: the setting's default
    w = interval_work(NDIM, ncol, PARAMETERS)
    print('== work of one mesh interval (%s run "cycles": NTST %d, NCOL %d, ndim %d, %d free parameters)' % (DESCRIPTION.name, ntst, ncol, NDIM, PARAMETERS))
    for k, v in w.items():
        print('   %-36s %d' % (k, v))
    print('   one pass over the mesh: %d rhs evaluations (%d with the residual pass), %d flops of assembly and %d of elimination' %
          (ntst * w['rhs_evaluations'], ntst * w['rhs_evaluations_with_residual_pass'], ntst * w['assembly_flops'], ntst * w['elimination_flops']))
    print('   the mesh data one pass would carry each way: AA %d, BB %d doubles' % (ntst * w['rows'] * w['columns'], ntst * w['rows'] * PARAMETERS))

    print('== GPU latencies (%s), untimed (machine not quiet)' % cp.cuda.runtime.getDeviceProperties(0)['name'].decode())
    empty = build(EMPTY, ('--std=c++17',), 'empty')
    s = gpu_seconds(lambda: empty((1,), (32,), ()), 2000)
    print('   empty kernel, launch and synchronise: median %.1f us (min %.1f, max %.1f, %d repeats)' % (statistics.median(s) * 1e6, min(s) * 1e6, max(s) * 1e6, len(s)))

    def many():
        for _ in range(1000):
            empty((1,), (32,), ())
    s = gpu_seconds(many, 20)
    print('   1000 empty launches then one synchronise: median %.2f us per launch (min %.2f, max %.2f, %d repeats)' % (statistics.median(s) * 1e3, min(s) * 1e3, max(s) * 1e3, len(s)))

    host = np.zeros(ntst * w['rows'] * w['columns'], dtype=np.float64)
    dev_buf = cp.empty_like(cp.asarray(host))

    def roundtrip():
        dev_buf.set(host)
        dev_buf.get()
    s = gpu_seconds(roundtrip, 500)
    print('   copy %d doubles (%.0f KB) to the GPU and back: median %.1f us (min %.1f, max %.1f)' % (len(host), host.nbytes / 1024, statistics.median(s) * 1e6, min(s) * 1e6, max(s) * 1e6))

    cells = ntst * ncol * (1 + 2 * NDIM + PARAMETERS)  # one right-hand side per thread: the mesh's evaluations in one launch
    for variant in q3_network.VARIANTS:
        g = q3_network.GpuRun(variant, cells, 1, 0.05)
        g.y[:] = g.y0
        stage = lambda: g.k(g.grid, (q3_network.BLOCK,), (np.int32(1), np.int32(g.n), np.float64(g.dt), g.y, g.acc, g.y, g.a))
        s = gpu_seconds(stage, 1000)
        print('   one launch of %d right-hand sides (%s): median %.1f us (min %.1f, max %.1f)' % (cells, variant[0], statistics.median(s) * 1e6, min(s) * 1e6, max(s) * 1e6))


if __name__ == '__main__':
    main()
