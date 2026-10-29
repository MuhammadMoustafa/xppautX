#!/usr/bin/env -S uv run --locked python
"""W249 (#305): the ceiling of a compiled right-hand side. Times stock xppautX (the
interpreter) against the test build's hand-written PY_S1Bf right-hand side
(hand_rhs.inc; XPP_HAND_RHS=1 xpp::math, =2 libm) on

  integrate  --silent on the spiking model (Iapp = 2000 pA, the model's own
             stiff settings, 2000 ms, 200001 rows): wall and CPU time, output md5
  auto       W246's periodic run (../auto/hopf_xppautx.py on PY_S1Bf_bench.odex):
             CPU time, labelled points

One excluded warm-up, then REPEATS measured runs per variant, order rotating; pinned CPU.
Writes results/<name>/runs.json and summary.json.

    uv run --locked python ceiling.py --stock XPPAUTX --hand XPPAUTX_TEST --name NAME [--variants a,b,...]

W250 (#306): the variant tcc (XPP_HAND_RHS=3) needs a test build patched with --tcc.
"""
import argparse, hashlib, json, os, statistics, subprocess, sys, tempfile, time
from pathlib import Path

WIN = sys.platform == 'win32'
if not WIN:
    import resource

HERE = Path(__file__).resolve().parent
AUTO = HERE.parent / 'auto'
sys.path.insert(0, str(AUTO))
import hopf_xppautx  # noqa: E402

CPU = 2
REPEATS = 5
SPIKE_IAPP = '2000'  # pA: between the rest-branch fold (158) and the depolarised HB (16409): the cell spikes


def spike_model(folder):
    src = (AUTO / 'models' / 'PY_S1Bf_bench.odex').read_text()
    assert src.count('par Iapp = 20000\n') == 1
    p = Path(folder) / 'spike.odex'
    p.write_text(src.replace('par Iapp = 20000\n', f'par Iapp = {SPIKE_IAPP}\n'))
    return p


def pin_self():
    """Windows (W250): this process on CPU, which the programs it starts inherit (no taskset there)"""
    import ctypes
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.GetCurrentProcess.restype = ctypes.c_void_p
    k.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    if not k.SetProcessAffinityMask(k.GetCurrentProcess(), 1 << CPU):
        raise ctypes.WinError(ctypes.get_last_error())


def integrate(binary, mode, model):
    out = model.parent / 'output.dat'
    out.unlink(missing_ok=True)
    env = {**os.environ, 'XPP_HAND_RHS': mode}
    t0 = time.perf_counter()
    if WIN:
        p = subprocess.Popen([binary, model.name, '--silent'], cwd=model.parent, env=env,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert p.wait() == 0
        wall, cpu = time.perf_counter() - t0, hopf_xppautx.cpu_seconds(p)
    else:
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        subprocess.run(['taskset', '-c', str(CPU), binary, model.name, '--silent'], cwd=model.parent, check=True,
                       env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        wall = time.perf_counter() - t0
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        cpu = (after.ru_utime + after.ru_stime) - (before.ru_utime + before.ru_stime)
    data = out.read_bytes().replace(b'\r', b'')
    return {'wall': wall, 'cpu': cpu, 'rows': data.count(b'\n'), 'md5': hashlib.md5(data).hexdigest()}


def auto(binary, mode, folder):
    if WIN:
        os.environ['XPP_HAND_RHS'] = mode
        return hopf_xppautx.measure(binary)
    w = Path(folder) / f'pinned-{mode}'
    w.write_text(f'#!/bin/sh\nXPP_HAND_RHS={mode} exec taskset -c {CPU} {binary} "$@"\n')
    w.chmod(0o755)
    return hopf_xppautx.measure(str(w))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--stock', required=True)
    ap.add_argument('--hand', required=True)
    ap.add_argument('--name', required=True)
    ap.add_argument('--repeats', type=int, default=REPEATS)
    ap.add_argument('--variants', default='interpreter,hand-xppmath,hand-libm')
    a = ap.parse_args()
    out = HERE / 'results' / a.name
    out.mkdir(parents=True, exist_ok=False)
    variants = {'interpreter': (os.path.abspath(a.stock), '0'), 'hand-xppmath': (os.path.abspath(a.hand), '1'),
                'hand-libm': (os.path.abspath(a.hand), '2'), 'tcc': (os.path.abspath(a.hand), '3')}
    if WIN:
        pin_self()
    names = a.variants.split(',')
    assert names[0] == 'interpreter' and set(names) <= set(variants)
    runs, log = [], open(out / 'run.log', 'w')
    with tempfile.TemporaryDirectory(prefix='w249-') as tmp:
        model = spike_model(tmp)
        for rep in range(a.repeats + 1):
            for v in names[rep % len(names):] + names[:rep % len(names)]:
                b, mode = variants[v]
                i = integrate(b, mode, model)
                au = auto(b, mode, tmp)
                runs.append({'variant': v, 'rep': rep, 'warmup': rep == 0, 'integrate': i, 'auto': au})
                msg = (f"rep {rep} {v}: integrate {i['cpu']:.2f} s CPU md5 {i['md5'][:12]}; "
                       f"AUTO periodic {au['periodic']['cpu']:.2f} s CPU, {au['periodic']['labels'][-1]}")
                print(msg, flush=True)
                log.write(msg + '\n')
                (out / 'runs.json').write_text(json.dumps(runs, indent=1))
    summary = {}
    for v in names:
        rs = [r for r in runs if r['variant'] == v and not r['warmup']]
        s = summary[v] = {}
        for k, get in (('integrate_cpu', lambda r: r['integrate']['cpu']), ('integrate_wall', lambda r: r['integrate']['wall']),
                       ('auto_periodic_cpu', lambda r: r['auto']['periodic']['cpu']),
                       ('auto_steady_cpu', lambda r: r['auto']['steady']['cpu'])):
            x = [get(r) for r in rs]
            s[k + '_mean'], s[k + '_se'] = statistics.mean(x), statistics.stdev(x) / len(x) ** .5
        s['integrate_md5'] = sorted({r['integrate']['md5'] for r in rs})
        s['auto_labels'] = sorted({json.dumps(r['auto']['periodic']['labels']) for r in rs})
    (out / 'summary.json').write_text(json.dumps(summary, indent=1))
    base = summary['interpreter']
    for v, s in summary.items():
        print(f"{v:13s} integrate {s['integrate_cpu_mean']:6.2f} +/- {s['integrate_cpu_se']:.2f} s "
              f"(x{base['integrate_cpu_mean'] / s['integrate_cpu_mean']:.2f}), md5 {[m[:12] for m in s['integrate_md5']]}; "
              f"AUTO {s['auto_periodic_cpu_mean']:6.2f} +/- {s['auto_periodic_cpu_se']:.2f} s "
              f"(x{base['auto_periodic_cpu_mean'] / s['auto_periodic_cpu_mean']:.2f}), {len(s['auto_labels'])} label set(s)")


if __name__ == '__main__':
    main()
