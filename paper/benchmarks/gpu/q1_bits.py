"""W268 question 1: does the GPU give xppautX's bits? CORE-MATH's exp, tanh and cosh compiled as device code (core_math_device.py) and
CUDA's own exp, tanh and cosh, each against the host copy xpp::math calls, on a few million arguments.

    uv run --locked --group gpu python paper/benchmarks/gpu/q1_bits.py [--n 1000000]

The host copies are built from third_party/core-math with the Makefile's flags (FPFLAGS -ffp-contract=off; the FMA copy with
-mfma, renamed cr_<name>_fma): the plain one runs on a CPU without FMA, the FMA one on this machine; both are compared."""
import argparse
import ctypes
from pathlib import Path
import subprocess
import tempfile

import numpy as np

from gpucommon import cp, dev, ulps

HOST_FLAGS = ('-O2', '-ffp-contract=off', '-w')  # the Makefile's OPT and FPFLAGS for third_party/core-math
KERNEL = r'''
extern "C" __global__ void run(const double *x, double *y, long long n, int which) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    double v = x[i];
    y[i] = which == 0 ? %(f0)s(v) : which == 1 ? %(f1)s(v) : %(f2)s(v);
}
'''


def build_host(folder):
    objects, wrapper = [], []
    for name in dev.FUNCTIONS:
        src = dev.CORE_MATH / name / (name + '.c')
        for suffix, extra in (('', []), ('_fma', ['-mfma', '-Dcr_%s=cr_%s_fma' % (name, name)])):
            obj = folder / ('%s%s.o' % (name, suffix))
            subprocess.run(['gcc', *HOST_FLAGS, *extra, '-c', str(src), '-o', str(obj)], check=True)
            objects.append(str(obj))
            wrapper.append('double cr_%s%s(double);\nvoid run_%s%s(const double *x, double *y, long n) { for (long i = 0; i < n; i++) y[i] = cr_%s%s(x[i]); }\n'
                           % (name, suffix, name, suffix, name, suffix))
    wsrc = folder / 'wrap.c'
    wsrc.write_text(''.join(wrapper))
    so = folder / 'libhost.so'
    subprocess.run(['gcc', '-O2', '-ffp-contract=off', '-shared', '-fPIC', str(wsrc), *objects, '-o', str(so), '-lm'], check=True)
    return ctypes.CDLL(str(so))


def host(lib, name, x, fma):
    y = np.empty_like(x)
    getattr(lib, 'run_%s%s' % (name, '_fma' if fma else '')).restype = None
    getattr(lib, 'run_%s%s' % (name, '_fma' if fma else ''))(x.ctypes.data_as(ctypes.c_void_p), y.ctypes.data_as(ctypes.c_void_p), ctypes.c_long(len(x)))
    return y


def run_device(source, options, x, which):
    mod = cp.RawModule(code=source, options=options)
    k = mod.get_function('run')
    xd = cp.asarray(x)
    yd = cp.empty_like(xd)
    k(((len(x) + 255) // 256,), (256,), (xd, yd, np.int64(len(x)), np.int32(which)))
    return yd.get()


def ulps_nan_equal(a, b):
    """gpucommon.ulps, with two NaNs counted equal (the payload of a NaN is not a result)."""
    return np.where(np.isnan(a) & np.isnan(b), 0, ulps(a, b))


def arguments(name, n, seed=268):
    """Uniform where the models use them, the whole exponent range, random bit patterns, the neighbours of CORE-MATH's hard cases, specials."""
    rng = np.random.default_rng(seed)
    parts = [rng.uniform(-10, 10, n), rng.uniform(-745, 710, n) if name != 'tanh' else rng.uniform(-30, 30, n),
             (rng.integers(0, 2**64, n, dtype=np.uint64)).view(np.float64),
             rng.uniform(-1, 1, n) * 2.0 ** rng.integers(-60, 5, n)]
    text = (dev.CORE_MATH / name / (name + '.c')).read_text()
    import re
    hard = np.array([float.fromhex(h) for h in re.findall(r'-?0x[0-9a-f]+(?:\.[0-9a-f]+)?p[-+]?\d+', text)])
    hard = hard[np.isfinite(hard)]
    base = hard.view(np.int64)
    near = np.concatenate([(base + d).view(np.float64) for d in range(-3, 4)])
    specials = np.array([0.0, -0.0, np.inf, -np.inf, np.nan, 5e-324, -5e-324, 2.2250738585072014e-308, 709.78, 709.79, -745.13, -745.14, 1.0, -1.0])
    x = np.concatenate(parts + [near, hard, -hard, specials])
    return np.ascontiguousarray(x)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--n', type=int, default=1000000, help='arguments per range (four ranges per function)')
    a = ap.parse_args()
    with tempfile.TemporaryDirectory(prefix='w268') as tmp:
        lib = build_host(Path(tmp))
    print('GPU: %s; CuPy %s, CUDA runtime %s' % (cp.cuda.runtime.getDeviceProperties(0)['name'].decode(), cp.__version__, cp.cuda.runtime.runtimeGetVersion()))
    core = dev.device_source()
    core_k = core + KERNEL % dict(f0='crd_exp', f1='crd_tanh', f2='crd_cosh')
    cuda_k = KERNEL % dict(f0='exp', f1='tanh', f2='cosh')
    variants = (('CORE-MATH device, -fmad=false', core_k, dev.NVRTC_OPTIONS),
                ('CORE-MATH device, fmad on', core_k, ('--std=c++20',)),
                ('CUDA native, -fmad=false', cuda_k, ('--std=c++17', '-fmad=false')),
                ('CUDA native, fmad on', cuda_k, ('--std=c++17',)))
    print('%-8s %-34s %10s %12s %10s' % ('function', 'device variant', 'arguments', 'differ', 'max ulp'))
    for which, name in enumerate(dev.FUNCTIONS):
        x = arguments(name, a.n)
        h_fma, h_plain = host(lib, name, x, True), host(lib, name, x, False)
        d = ulps_nan_equal(h_fma, h_plain)
        print('%-8s %-34s %10d %12d %10d' % (name, 'host plain copy vs host FMA copy', len(x), int((d != 0).sum()), int(d.max())))
        for label, src, options in variants:
            u = ulps_nan_equal(run_device(src, options, x, which), h_fma)
            print('%-8s %-34s %10d %12d %10d' % (name, label, len(x), int((u != 0).sum()), int(u.max())))


if __name__ == '__main__':
    main()
