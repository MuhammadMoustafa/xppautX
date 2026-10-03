#!/usr/bin/env python3
"""W34 (#72): runs xppautX's CVODE and SUNDIALS' CVODE on the same models and
keeps what each wrote, how long it took and what CVODE counted, for
tools/sundials_report.py to turn into tables (an evaluation: docs/sundials-eval.md).

  python3 tools/sundials_eval.py --vendored ./xppautX --sundials build/sundials/xppautX \
      --out results [--repeats 3] [--only NAME,NAME]

Each run is `xppautX model.odex --silent --debug` in a folder of its own: its
output.dat, its log (the `cvode stats:` lines core/cv_vendored.cpp and
core/cv_sundials.cpp log at DEBUG, summed over the integrations a run
restarts: events, the Poincare map), and the wall time of the whole process
(the best of --repeats; also the CPU time of the process, which another run
on the machine does not stretch). The models are tools/models/sundials/*.odex and the
examples that use CVODE; a tolerance run appends `@ meth=cvode, tol=T, atol=T`
(xppautX's toler is CVODE's absolute and atoler its relative tolerance, so
both are set), the reference runs 1e-13 (SUNDIALS' CVODE, and dp83 where the
model is not stiff). Standard library only: runs in WSL beside the binaries.
"""
import argparse, json, os, re, resource, shutil, subprocess, sys, time

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
MODELS = os.path.join(here, 'models', 'sundials')
EXAMPLES = os.path.join(root, 'examples', 'ode')

# the tolerances of the sweep (toler = atoler), and the reference's
TOLS = ['1e-3', '1e-4', '1e-6', '1e-8', '1e-10']
REF_TOL = '1e-13'
# the tolerances of the heavy (timing) models, and their reference's
HEAVY_TOLS = ['1e-6', '1e-8']
HEAVY_REF_TOL = '1e-11'
# one run's limit, seconds (a model that runs away is a result, not a hang)
RUN_TIMEOUT = 900

# name, model folder, kind: 'sweep' (every tolerance), 'asis' (its own settings
# only; 'cvode' adds meth=cvode), 'stiff' (no dp83 reference), 'heavy' (a few
# tolerances: the models that take time), 'timing' (heavy, and no reference),
# 'tolswap' (toler and atoler apart)
CASES = [
    ('osc', MODELS, 'sweep'),
    ('osc1e6', MODELS, 'tolswap'),
    ('osclong', MODELS, 'timing'),
    ('vdp', MODELS, 'sweep'),
    ('pr', MODELS, 'sweep stiff'),
    ('robertson', MODELS, 'sweep stiff'),
    ('heat', MODELS, 'sweep stiff'),
    ('heatband', MODELS, 'sweep stiff'),
    ('brus', MODELS, 'heavy stiff'),
    ('vdpstiff', MODELS, 'heavy stiff'),
    ('delay', MODELS, 'sweep'),
    ('ball', MODELS, 'sweep'),
    ('poincare', MODELS, 'sweep'),
    ('blowup', MODELS, 'asis'),
    ('candelator', EXAMPLES, 'asis'),  # the example's f(t) ends mid-line: it does not load
    ('atcoaster', EXAMPLES, 'asis'),
    ('fieldnoy', EXAMPLES, 'asis'),
    ('itoy', EXAMPLES, 'asis'),
    ('toy_ok', EXAMPLES, 'asis'),
    ('waterwheel', EXAMPLES, 'asis'),
    ('kuramot100', EXAMPLES, 'asis cvode'),
]

STATS = re.compile(r'cvode stats: nst=(\d+) nfe=(\d+) rhs=(\d+) nje=(\d+) nsetups=(\d+) nni=(\d+) ncfn=(\d+) netf=(\d+)')
STAT_NAMES = ['nst', 'nfe', 'rhs', 'nje', 'nsetups', 'nni', 'ncfn', 'netf']


def model_text(name, folder):
    for ext in ('.odex', '.ode'):
        path = os.path.join(folder, name + ext)
        if os.path.exists(path):
            return ext, open(path, newline='').read().replace('\r\n', '\n')
    sys.exit('no model ' + name)


def run_once(exe, workdir, model):
    t0 = time.perf_counter()
    r0 = resource.getrusage(resource.RUSAGE_CHILDREN)
    try:
        p = subprocess.run([exe, model, '--silent', '--debug', '--auto'], cwd=workdir,
                           capture_output=True, text=True, timeout=RUN_TIMEOUT, errors='replace')
        out, code = p.stdout + p.stderr, p.returncode
    except subprocess.TimeoutExpired as e:
        out, code = (e.stdout or b'').decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or ''), 'timeout'
    r1 = resource.getrusage(resource.RUSAGE_CHILDREN)
    return time.perf_counter() - t0, (r1.ru_utime - r0.ru_utime) + (r1.ru_stime - r0.ru_stime), out, code


def run(exe, text, ext, opts, workdir, repeats):
    """one configuration: the model's text with the options line appended,
    run `repeats` times; the result of the last, the best time"""
    if os.path.isdir(workdir):
        shutil.rmtree(workdir)
    os.makedirs(workdir)
    with open(os.path.join(workdir, 'model' + ext), 'w', newline='\n') as f:
        f.write(text.rstrip('\n') + '\n' + (('@ ' + opts + '\n') if opts else ''))
    times = []
    cpus = []
    for _ in range(repeats):
        for stale in ('output.dat',):
            if os.path.exists(os.path.join(workdir, stale)):
                os.remove(os.path.join(workdir, stale))
        dt, cpu, out, code = run_once(exe, workdir, 'model' + ext)
        times.append(dt)
        cpus.append(cpu)
        if code != 0:
            break
    stats = dict.fromkeys(STAT_NAMES, 0)
    integrations = 0
    for m in STATS.finditer(out):
        integrations += 1
        for k, v in zip(STAT_NAMES, m.groups()):
            stats[k] += int(v)
    keep = [l for l in out.splitlines() if re.search(r'ERROR|WARN|error|failed|stopp', l) and 'cvode stats' not in l]
    with open(os.path.join(workdir, 'log.txt'), 'w') as f:
        f.write(out)
    rows = 0
    last_t = None
    path = os.path.join(workdir, 'output.dat')
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                if line.strip():
                    rows += 1
                    last_t = line.split()[0]
    return {'code': code, 'time': min(times), 'times': times, 'cpu': min(cpus), 'cpu': min(cpus), 'stats': stats, 'integrations': integrations,
            'rows': rows, 'last_t': last_t, 'messages': keep[:6], 'dir': workdir}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--vendored', required=True)
    ap.add_argument('--sundials', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--repeats', type=int, default=3)
    ap.add_argument('--only', default='')
    ap.add_argument('--no-refs', action='store_true', help='skip the tight reference runs (a timing pass)')
    args = ap.parse_args()
    exes = {'vendored': os.path.abspath(args.vendored), 'sundials': os.path.abspath(args.sundials)}
    only = set(filter(None, args.only.split(',')))
    results = {}
    os.makedirs(args.out, exist_ok=True)
    # the process's own start, a model that does nothing
    base = run(exes['vendored'], model_text('osc', MODELS)[1], '.odex', 'total=0.1, dt=0.1, meth=cvode', os.path.join(args.out, 'baseline'), args.repeats)
    results['baseline'] = base
    for name, folder, kind in CASES:
        if only and name not in only:
            continue
        ext, text = model_text(name, folder)
        case = {'kind': kind, 'runs': {}}
        results[name] = case
        cfgs = []  # (tag, backend, exe, opts)
        ref_tol = HEAVY_REF_TOL if 'heavy' in kind else REF_TOL
        if 'tolswap' in kind:
            for tag, tol, atol in (('toler1e-3_atoler1e-10', '1e-3', '1e-10'), ('toler1e-10_atoler1e-3', '1e-10', '1e-3')):
                for b in ('vendored', 'sundials'):
                    cfgs.append((tag, b, exes[b], 'meth=cvode, tol=%s, atol=%s' % (tol, atol)))
        elif 'sweep' in kind or 'heavy' in kind or 'timing' in kind:
            for tol in (TOLS if 'sweep' in kind else HEAVY_TOLS):
                for b in ('vendored', 'sundials'):
                    cfgs.append(('tol' + tol, b, exes[b], 'meth=cvode, tol=%s, atol=%s' % (tol, tol)))
        else:
            for b in ('vendored', 'sundials'):
                cfgs.append(('asis', b, exes[b], 'meth=cvode' if 'cvode' in kind else ''))
        if 'tolswap' not in kind and 'timing' not in kind and not args.no_refs:
            cfgs.append(('ref_sundials', 'sundials', exes['sundials'], 'meth=cvode, tol=%s, atol=%s' % (ref_tol, ref_tol)))
            cfgs.append(('ref_vendored', 'vendored', exes['vendored'], 'meth=cvode, tol=%s, atol=%s' % (ref_tol, ref_tol)))
        if 'stiff' not in kind and 'tolswap' not in kind and 'timing' not in kind and not args.no_refs:
            cfgs.append(('ref_dp83', 'vendored', exes['vendored'], 'meth=DoPri8(3), tol=%s, atol=%s' % (REF_TOL, REF_TOL)))
        for tag, b, exe, opts in cfgs:
            wd = os.path.join(args.out, name, tag + '_' + b)
            # a model's own `@ meth=CVode`: a later line wins, so an as-is run adds nothing
            case['runs'][tag + '_' + b] = r = run(exe, text, ext, opts, wd, args.repeats)
            print('%-12s %-22s code=%s rows=%s t=%.3fs cpu=%.3fs nst=%s rhs=%s' % (name, tag + '_' + b, r['code'], r['rows'], r['time'], r['cpu'],
                  r['stats']['nst'], r['stats']['rhs']), flush=True)
    with open(os.path.join(args.out, 'results.json'), 'w') as f:
        json.dump(results, f, indent=1)


if __name__ == '__main__':
    main()
