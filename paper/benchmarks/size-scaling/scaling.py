#!/usr/bin/env -S uv run --locked python
"""W241 (#297): does xppautX's advantage over XPPAUT 8.0 grow with model size?

Each scalable model runs at three durations: one step (start-up, load and
output set-up), T and 2T. The difference of the 2T and T means, over T, is the
integration cost per unit of model time with start-up removed; the one-step
run is the fixed cost. Both programs get the same .ode (XPPAUT) or its
conversion (.odex, xppautX), the same settings, a fresh HOME, no DISPLAY and
one pinned CPU.

  calibrate  pick T per case from XPPAUT probes, write models/ and cases.json
  run        warm-up + five repeats per case/duration/engine -> results/<name>/
  summarize  results/<name>/runs.json -> summary.csv, summary.json
  restage    W269: cases.json and models/*.ode into --cases-file/--models-dir, converted again
  extend     W269: sizes past MAXODE (--sizes FAMILY:N,...), sized on --probe-engine

Linux (WSL) and Windows (W242, #298); Windows runs the models WSL calibrated.
W269 (#326) adds --engines legacy,modern,interp: 'interp' is xppautX with --no-compile.
"""
import argparse, csv, ctypes, gzip, hashlib, itertools, json, os, platform, shutil, statistics
import subprocess, sys, tempfile, threading, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'five-model-screen'))
from screen import compare, inspect_output, ENGINE_ARGS, convert_args  # noqa: E402  one owner of the output checks and engines

DT = 0.05       # the bundled models' own step, kept for every family
ROWS = 200      # output rows of a T and a 2T run alike, so writing cancels in the slope
TARGET = 3.0    # seconds of XPPAUT integration in a 2T run: start-up noise stays small beside it
PROBE_MIN = 0.5  # seconds a calibration probe must integrate before its rate is trusted
REPEATS = 5     # measured fresh-process runs per case, duration and engine (after one warm-up)
TIMEOUT = 900   # safety limit for a hang only, never a pass/fail threshold
CPU = 2         # the logical CPU every run is pinned to (W238's choice)


def kuramoto(n):
    """XPPAUT's kuramot100.ode at n oscillators: an all-to-all sum, n*n terms per evaluation."""
    return (f"# kuramoto with sums, {n} oscillators (XPPAUT's kuramot100.ode resized)\n"
            "h(phi)=sin(phi+a)-sin(a)+b*sin(2*phi)\npar a=.5,b=.2\npar amp=1,k=1\n"
            f"table w % {n} 0 {n-1} amp*t/{n}\n"
            f"x[0..{n-1}]'=w([j])+k*sum(0,{n-1})of(h(shift(x0,i')-x[j]))/{n}\n")


def lecdiff(n):
    """XPPAUT's lecdiff200.ode at n cells: a diffusive Morris-Lecar chain, 2n states."""
    return (f"# Morris-Lecar chain, {n} cells (XPPAUT's lecdiff200.ode resized)\n"
            "par dif=5\nparams iapp=0.0,phi=.333\nparam va=-.01,vb=0.15,vc=0.1,vd=0.145,gca=1\n"
            "params vk=-.7,vl=-.5,gk=2.0,gl=.5,om=1\n"
            "minf(v)=.5*(1+tanh((v-va)/vb))\nninf(v)=.5*(1+tanh((v-vc)/vd))\n"
            "lamn(v)= phi*cosh((v-vc)/(2*vd))\nica(v)=gca*minf(v)*(v-1)\n"
            "f(v,w)=(iapp+gl*(vl-v)+gk*w*(vk-v)-ica(v))\ng(v,w)=(lamn(v)*(ninf(v)-w))\n"
            f"v0=v1\nv{n+1}=v{n}\n"
            f"v[1..{n}]'=f(v[j],w[j])+dif*(v[j+1]-2*v[j]+v[j-1])\nw[1..{n}]'=g(v[j],w[j])\n"
            f"v[20..{n}](0)=-.49\nv[1..19](0)=0\n")


def amari(n):
    """XPPAUT's amari.ode on n points: a neural field through a convolution special."""
    return (f"# amari bump, {n} points (XPPAUT's amari.ode resized)\n"
            "mh(x)=exp(-abs(x))-c*b*exp(-b*abs(x))\npar c=1.05,b=.5\n"
            "table w % 101 -50 50 mh(dx*t)*dx\npar dx=.1\n"
            f"fu[0..{n-1}]=heav(u[j])\nspecial z=conv(even,{n},50,w,fu0)\n"
            f"u[0..{n-1}]'=-u[j]-h+z([j])\npar h=.15\n"
            f"init u[{n//2-10}..{n//2+10}]=.8\n")


# family: (generator, method, states per size unit, sizes). Every size stays
# under MAXODE (5000 variables, both programs' xpplim.h).
FAMILIES = {
    'kuramoto': (kuramoto, 'Runge-Kutta', 1, [10, 25, 50, 100, 200, 400]),
    'lecdiff': (lecdiff, 'Runge-Kutta', 2, [25, 50, 100, 200, 400, 800, 1600, 2400]),
    'amari': (amari, 'euler', 1, [100, 200, 400, 800, 1600, 2400]),
}
DURATIONS = ['step', 'T', '2T']


def settings(steps, method, rows=ROWS):
    """The @ line: XPPAUT's own option names, the same ones W238's screen used.
    W447 (#510): a case's T and 2T write the same `rows` (min(ROWS, T steps)) by storing every
    steps//rows-th step, so the 2T - T slope is integration cost alone even when T < ROWS steps
    (before, T wrote T rows and 2T wrote 2T rows there). The one-step run stores every step."""
    nout = max(1, steps // rows)
    return (f"@ total={steps*DT:.10g},dt={DT},meth={method},t0=0,trans=0,nout={nout},"
            f"maxstor={steps//nout+10},bound=100000,toler=.001,atoler=.001,dtmin=1e-12,dtmax=1\n")


def write_model(path, family, n, steps, rows=ROWS):
    gen, method, _, _ = FAMILIES[family]
    path.write_text(gen(n) + settings(steps, method, rows) + 'done\n')


def sha_lf(path):
    """Hash with CRs removed, so a Windows and a Linux output of the same numbers match."""
    return hashlib.sha256(path.read_bytes().replace(b'\r', b'')).hexdigest()


def kernel32():
    """The Windows calls this runner makes, typed: handles and masks are 64-bit."""
    from ctypes import wintypes
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.GetCurrentProcess.restype = wintypes.HANDLE
    k.SetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.c_size_t]
    k.SetProcessAffinityMask.restype = wintypes.BOOL
    k.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(ctypes.c_ulonglong)] * 4
    k.GetProcessTimes.restype = wintypes.BOOL
    return k


def pin_self(cpu):
    """Children inherit the affinity: taskset's job on Linux, set here on Windows."""
    if os.name == 'nt':
        k = kernel32()
        if not k.SetProcessAffinityMask(k.GetCurrentProcess(), 1 << cpu):
            raise OSError('SetProcessAffinityMask failed')
    else:
        os.sched_setaffinity(0, {cpu})


def child_cpu_seconds(proc):
    """The finished child's user+system time, read before its handle is closed."""
    if os.name == 'nt':
        ft = [ctypes.c_ulonglong() for _ in range(4)]
        if not kernel32().GetProcessTimes(int(proc._handle), *[ctypes.byref(f) for f in ft]):
            return None
        return (ft[2].value + ft[3].value) / 1e7
    return None  # Linux: taken from RUSAGE_CHILDREN around the run


def engine_args(engine, binary, model):
    if engine == 'legacy':
        return [str(binary), model + '.ode', '-silent', '-internset', '0']
    return [str(binary), model + '.odex', '--silent', '--internset', '0'] + ENGINE_ARGS[engine]


def engine_binaries(args):
    """W269: the engines a run times, each with its program ('interp' is --modern with --no-compile)"""
    engines = args.engines.split(',')
    if any(e not in ENGINE_ARGS for e in engines) or len(set(engines)) != len(engines):
        raise ValueError('--engines: distinct names of ' + ','.join(ENGINE_ARGS))
    return {e: (args.legacy if e == 'legacy' else args.modern).resolve() for e in engines}


def run_once(case, duration, engine, rep, binary, models, work, timeout):
    model = f"{case['name']}-{duration}"
    run = work / f"{model}-{engine}-{rep}"
    run.mkdir()
    shutil.copy2(models / (model + ('.ode' if engine == 'legacy' else '.odex')), run)
    home = run / 'home'
    home.mkdir()
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / 'config'), LC_ALL='C',
               OMP_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1')
    env.pop('DISPLAY', None)
    if os.name != 'nt':
        import resource
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.perf_counter()
    with (run / 'run.log').open('wb') as log:
        proc = subprocess.Popen(engine_args(engine, binary, model), cwd=run, env=env,
                                stdout=log, stderr=subprocess.STDOUT)
        timed_out = []

        def expire():
            if proc.poll() is None:
                timed_out.append(True)
                proc.kill()  # this runner's own child, by its handle
        timer = threading.Timer(timeout, expire)
        timer.start()
        try:
            code = proc.wait()
        finally:
            timer.cancel()
            timer.join()
        wall = time.perf_counter() - start
        cpu = child_cpu_seconds(proc)
    if os.name != 'nt':
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        cpu = (after.ru_utime + after.ru_stime) - (before.ru_utime + before.ru_stime)
    steps = case['steps'][duration]
    check = dict(name=model, total=steps * DT, dt=DT, states=case['states'])
    out = run / 'output.dat'
    record = dict(case=case['name'], family=case['family'], size=case['size'], states=case['states'],
                  duration=duration, model_time=steps * DT, engine=engine, repetition=rep,
                  wall_seconds=wall, cpu_seconds=cpu, exit_code='timeout' if timed_out else code,
                  output=inspect_output(out, check))
    if out.exists():
        record['output']['sha256_lf'] = sha_lf(out)
    record['valid'] = record['exit_code'] == 0 and record['output']['valid']
    if not record['valid']:
        record['log_tail'] = (run / 'run.log').read_text(errors='replace')[-2000:]
    return record, run


def convert(paths, modern):
    """xppautX's own conversion of each .ode next to it (the .odex every xppautX run reads)"""
    for path in sorted(paths):
        p = subprocess.run([str(modern), *convert_args(), str(path)], capture_output=True, text=True)
        if p.returncode:
            raise RuntimeError(f'{path.name}: conversion failed: {p.stdout}{p.stderr}')


def size_cases(sizes, engine, binary, modern, models, timeout):
    """Each (family, n) sized so its 2T run integrates about TARGET seconds in the probe engine
    (XPPAUT for W241; W269's sizes past MAXODE, which XPPAUT cannot load, probe xppautX's
    interpreter): (cases, probes), the models written into models."""
    cases, probes = [], []
    with tempfile.TemporaryDirectory(prefix='xpp-scaling-cal-') as tmp:
        work = Path(tmp)
        for family, n in sizes:
            per = FAMILIES[family][2]
            case = dict(name=f'{family}{n}', family=family, size=n, states=per * n, steps={}, rows=ROWS)

            def probe(steps):
                case['steps']['probe'] = steps
                write_model(work / f"{case['name']}-probe.ode", family, n, steps)
                if engine != 'legacy':
                    convert([work / f"{case['name']}-probe.ode"], modern)
                rec, run = run_once(case, 'probe', engine, len(probes), binary, work, work, timeout)
                probes.append(rec)
                shutil.rmtree(run)
                if not rec['valid']:
                    raise RuntimeError(f"{case['name']}: {engine} probe failed: {rec.get('log_tail', '')}")
                return rec['wall_seconds']
            fixed = probe(1)
            steps = ROWS * 2
            while True:
                spent = probe(steps) - fixed
                if spent >= PROBE_MIN:
                    break
                steps *= 4
            rate = spent / (steps * DT)  # seconds per unit of model time
            t_steps = TARGET / 2 / rate / DT
            t_steps = max(ROWS, round(t_steps / ROWS) * ROWS) if t_steps >= ROWS else max(1, round(t_steps))
            case['steps'] = dict(step=1, T=t_steps, **{'2T': 2 * t_steps})
            case['rows'] = min(ROWS, t_steps)
            case['calibration'] = dict(probe_steps=steps, probe_seconds=spent, fixed_seconds=fixed, probe_engine=engine)
            for d in DURATIONS:
                write_model(models / f"{case['name']}-{d}.ode", family, n, case['steps'][d], case['rows'])
            print(f"{case['name']}: T={t_steps*DT:.10g} ({t_steps} steps)", flush=True)
            cases.append(case)
    return cases, probes


def calibrate(args):
    """Size each case so its 2T run integrates about TARGET seconds in XPPAUT."""
    models, cases_file = HERE / args.models_dir, HERE / args.cases_file
    if models.exists() and any(models.iterdir()) or cases_file.exists():
        raise FileExistsError(f'{models} or {cases_file}: already calibrated; choose new names (--models-dir, --cases-file)')
    models.mkdir(exist_ok=True)
    pin_self(args.cpu)
    sizes = [(family, n) for family, (_, _, _, ns) in FAMILIES.items() for n in ns]
    cases, probes = size_cases(sizes, 'legacy', args.legacy, args.modern, models, args.timeout)
    convert(models.glob('*.ode'), args.modern)
    cases_file.write_text(json.dumps(cases, indent=2) + '\n')
    (models / 'calibration.json').write_text(json.dumps(dict(
        machine=platform.platform(), cpu=platform.processor(), load_average=os.getloadavg(),
        time_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), probes=probes), indent=2) + '\n')


def restage(args):
    """W269: the calibrated cases and .ode models again (cases.json, models/), into a new cases
    file and models folder, each .ode converted by this --modern (the .odex of an older xppautX
    is not read by a newer one)."""
    cases_file, models = HERE / args.cases_file, HERE / args.models_dir
    if cases_file.exists() or models.exists():
        raise FileExistsError(f'{cases_file} or {models} exists; choose new names')
    models.mkdir()
    for path in sorted((HERE / 'models').glob('*.ode')):
        shutil.copy2(path, models)
    convert(models.glob('*.ode'), args.modern)
    shutil.copy2(HERE / 'cases.json', cases_file)


def extend(args):
    """W269: further sizes (W243 lifted xppautX's MAXODE), calibrated on --probe-engine and
    appended to --cases-file; XPPAUT is tried on each one-step model and the case marked
    legacy=false, with XPPAUT's words, when it cannot run it."""
    cases_file, models = HERE / args.cases_file, HERE / args.models_dir
    cases = json.loads(cases_file.read_text())
    sizes = [(family, int(n)) for item in args.sizes for family, ns in [item.split(':')] for n in ns.split(',')]
    for family, n in sizes:
        if family not in FAMILIES or any(c['name'] == f'{family}{n}' for c in cases):
            raise ValueError(f'{family}{n}: unknown family or already a case')
    pin_self(args.cpu)
    probe_binary = args.legacy if args.probe_engine == 'legacy' else args.modern
    new, probes = size_cases(sizes, args.probe_engine, probe_binary, args.modern, models, args.timeout)
    convert([models / f"{c['name']}-{d}.ode" for c in new for d in DURATIONS], args.modern)
    with tempfile.TemporaryDirectory(prefix='xpp-scaling-legacy-') as tmp:
        for case in new:
            rec, run = run_once(case, 'step', 'legacy', 0, args.legacy, models, Path(tmp), args.timeout)
            case['legacy'] = rec['valid']
            if not rec['valid']:
                case['legacy_refusal'] = rec.get('log_tail', '')[-600:]
            shutil.rmtree(run)
            print(f"{case['name']}: XPPAUT {'runs it' if rec['valid'] else 'cannot run it'}", flush=True)
    cases_file.write_text(json.dumps(cases + new, indent=2) + '\n')
    (models / f'calibration-{time.strftime("%Y%m%d-%H%M%S")}.json').write_text(
        json.dumps(dict(machine=platform.platform(), load_average=os.getloadavg(), probes=probes), indent=2) + '\n')


def version(binary, engine):
    flag = '-version' if engine == 'legacy' else '--version'
    p = subprocess.run([str(binary), flag], capture_output=True, text=True, timeout=60)
    return (p.stdout + p.stderr).strip()


def gz(src, dst):
    with src.open('rb') as a, gzip.open(dst, 'wb') as b:
        shutil.copyfileobj(a, b)


def run(args):
    cases = json.loads((HERE / args.cases_file).read_text())
    models = HERE / args.models_dir
    if args.cases:
        cases = [c for c in cases if c['name'] in args.cases]
    target = HERE / 'results' / args.result_name
    if target.exists():
        raise FileExistsError(f'{target}: refusing to overwrite; choose another --result-name')
    target.mkdir(parents=True)
    pin_self(args.cpu)
    binary = engine_binaries(args)
    engines = list(binary)
    meta = dict(card='W241 #297 / W242 #298' + (' / W269 #326' if engines != ['legacy', 'modern'] else ''),
                platform=platform.platform(), machine=platform.machine(),
                python=sys.version, cpu_affinity=args.cpu, repetitions=REPEATS, engines=engines,
                engine_args={e: ENGINE_ARGS[e] for e in engines},
                cases_file=args.cases_file, models_dir=args.models_dir,
                binaries={e: dict(path=str(b), sha256=hashlib.sha256(b.read_bytes()).hexdigest(),
                                  version=version(b, e)) for e, b in binary.items()},
                timing='whole process per run; per-time cost = (mean 2T - mean T) / T',
                environment='fresh HOME, no DISPLAY, LC_ALL=C, OMP/OPENBLAS threads 1, pinned CPU',
                start_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
    records, comparisons = [], []
    with tempfile.TemporaryDirectory(prefix='xpp-scaling-') as tmp:
        work = Path(tmp)
        for ci, case in enumerate(cases):
            # a size past XPPAUT's MAXODE (extend: legacy=false) runs xppautX's engines only
            here = [e for e in engines if e != 'legacy' or case.get('legacy', True)]
            for duration in DURATIONS:
                kept = {}
                for rep in range(REPEATS + 1):  # 0 is the excluded warm-up
                    if len(here) == 2:
                        order = here if (ci + rep) % 2 else here[::-1]  # W241's alternation
                    else:
                        k = (ci + rep) % len(here)
                        order = here[k:] + here[:k]
                    for engine in order:
                        rec, rdir = run_once(case, duration, engine, rep, binary[engine],
                                             models, work, args.timeout)
                        records.append(rec)
                        print(f"{case['name']}-{duration}-{engine}-{rep}: {rec['wall_seconds']:.4f}s"
                              f" valid={rec['valid']}", flush=True)
                        if rep == 1 and rec['valid']:
                            kept[engine] = work / f"{case['name']}-{duration}-{engine}.dat.gz"
                            gz(rdir / 'output.dat', kept[engine])
                        shutil.rmtree(rdir)
                for left, right in itertools.combinations(here, 2):
                    if left in kept and right in kept:
                        pair = {} if engines == ['legacy', 'modern'] else dict(pair=[left, right])
                        comparisons.append(dict(case=case['name'], duration=duration, **pair,
                                                **compare(kept[left], kept[right])))
                for p in kept.values():
                    p.unlink()
            (target / 'runs.json').write_text(json.dumps(dict(metadata=meta, runs=records), indent=2) + '\n')
            (target / 'comparison.json').write_text(json.dumps(comparisons, indent=2) + '\n')
    meta['end_utc'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
    (target / 'runs.json').write_text(json.dumps(dict(metadata=meta, runs=records), indent=2) + '\n')
    summarize_dir(target, args.cases_file)


def mean_sd(values):
    return statistics.mean(values), statistics.stdev(values)


# W269: the ratios a summary gives, numerator / denominator time (> 1: the second is faster);
# legacy/modern keeps W241's field names, the others are suffixed with the pair
RATIOS = [('legacy', 'modern', ''), ('legacy', 'interp', '_legacy_interp'), ('interp', 'modern', '_interp_modern')]


def summarize_dir(target, cases_file='cases.json'):
    data = json.loads((target / 'runs.json').read_text())
    comparisons = {(c['case'], c['duration'], tuple(c.get('pair', ('legacy', 'modern')))): c
                   for c in json.loads((target / 'comparison.json').read_text())}
    cases = json.loads((HERE / data['metadata'].get('cases_file', cases_file)).read_text())
    engines = data['metadata'].get('engines', ['legacy', 'modern'])
    rows = []
    for case in cases:
        rr = [r for r in data['runs'] if r['case'] == case['name'] and r['repetition'] > 0]
        if not rr:
            continue
        row = dict(family=case['family'], size=case['size'], states=case['states'],
                   T=case['steps']['T'] * DT)
        done = []
        for engine in engines:
            if not any(r['engine'] == engine for r in rr):
                continue  # not run on this case (past XPPAUT's MAXODE)
            m = {}
            for d in DURATIONS:
                v = [r['wall_seconds'] for r in rr if r['engine'] == engine and r['duration'] == d and r['valid']]
                if len(v) != REPEATS:
                    break
                m[d] = mean_sd(v)
                hashes = {r['output']['sha256_lf'] for r in rr if r['engine'] == engine and r['duration'] == d}
                row[f'{engine}_{d}_deterministic'] = len(hashes) == 1
            if len(m) != len(DURATIONS):
                row[f'{engine}_complete'] = False
                continue
            slope = (m['2T'][0] - m['T'][0]) / row['T']
            slope_se = (m['2T'][1] ** 2 / REPEATS + m['T'][1] ** 2 / REPEATS) ** .5 / row['T']
            row.update({f'{engine}_fixed_s': m['step'][0], f'{engine}_fixed_sd': m['step'][1],
                        f'{engine}_wall2T_s': m['2T'][0], f'{engine}_wall2T_sd': m['2T'][1],
                        f'{engine}_per_time_s': slope, f'{engine}_per_time_se': slope_se,
                        f'{engine}_linearity': (m['T'][0] - m['step'][0]) / (m['2T'][0] - m['T'][0])})
            done.append(engine)
        for x, y, suffix in RATIOS:
            if x not in done or y not in done:
                continue
            a, sa, b, sb = row[f'{x}_per_time_s'], row[f'{x}_per_time_se'], row[f'{y}_per_time_s'], row[f'{y}_per_time_se']
            row['integration_ratio' + suffix] = a / b  # > 1: y integrates faster
            row['integration_ratio_se' + suffix] = a / b * ((sa / a) ** 2 + (sb / b) ** 2) ** .5
            row['whole_run_ratio_2T' + suffix] = row[f'{x}_wall2T_s'] / row[f'{y}_wall2T_s']
            row['fixed_difference_s' + suffix] = row[f'{y}_fixed_s'] - row[f'{x}_fixed_s']
            for d in ['T', '2T']:
                c = comparisons.get((case['name'], d, (x, y))) or comparisons.get((case['name'], d, (y, x)), {})
                row[f'outputs_{d}' + suffix] = ('exact' if c.get('exact_values') else
                                                'within screen tolerance' if c.get('within_screen_tolerance') else
                                                f"differ (max {c.get('max_abs_error')})" if c else 'not compared')
        row['complete'] = len(done) == sum(1 for e in engines if e != 'legacy' or case.get('legacy', True))
        rows.append(row)
    (target / 'summary.json').write_text(json.dumps(rows, indent=2) + '\n')
    fields = list(dict.fromkeys(k for r in rows for k in r))
    with (target / 'summary.csv').open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)
    for r in rows:
        parts = [f"{x}/{y}={r['integration_ratio' + s]:.3f}±{r['integration_ratio_se' + s]:.3f}"
                 for x, y, s in RATIOS if 'integration_ratio' + s in r]
        fixed = [f"{e} {r[e + '_fixed_s']*1000:.0f} ms" for e in engines if e + '_fixed_s' in r]
        outs = [f"{x}-{y} {r['outputs_2T' + s]}" for x, y, s in RATIOS if 'outputs_2T' + s in r]
        print(f"{r['family']:9} {r['size']:5} states={r['states']:5}  integration {' '.join(parts)}"
              f"  fixed {', '.join(fixed)}  outputs {'; '.join(outs)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('mode', choices=['calibrate', 'restage', 'extend', 'run', 'summarize'])
    ap.add_argument('--legacy', type=Path)
    ap.add_argument('--modern', type=Path)
    ap.add_argument('--result-name')
    ap.add_argument('--cases', nargs='+')
    ap.add_argument('--cpu', type=int, default=CPU)
    ap.add_argument('--timeout', type=float, default=TIMEOUT)
    # W269 (#326): three engines, the restaged models and the sizes past MAXODE
    ap.add_argument('--engines', default='legacy,modern', help="comma-separated: legacy, modern (xppautX), interp (xppautX --no-compile)")
    ap.add_argument('--cases-file', default='cases.json', help='the cases a run times (restage/extend write it)')
    ap.add_argument('--models-dir', default='models', help='the models a run reads (restage/extend write it)')
    ap.add_argument('--sizes', nargs='+', default=[], help='extend: FAMILY:N[,N...]')
    ap.add_argument('--probe-engine', default='interp', help='extend: the engine whose speed sizes T')
    args = ap.parse_args()
    if args.mode == 'summarize':
        summarize_dir(HERE / 'results' / args.result_name, args.cases_file)
    elif args.mode == 'calibrate':
        calibrate(args)
    elif args.mode == 'restage':
        restage(args)
    elif args.mode == 'extend':
        extend(args)
    else:
        run(args)


if __name__ == '__main__':
    main()
