#!/usr/bin/env python3
"""W476 (#539): callgrind profiles of the core's workloads, and their function tables.

    uv run --locked python tools/vectorization-audit/profile.py BINARY OUT [WORKLOAD...] [--summary]

Each workload is one `BINARY MODEL --silent --noout [options]` (no output.dat: writing the rows is not the workload) run under valgrind --tool=callgrind (an integration, a
post-processing step, a model load) or one tests/auto description through tools/auto-profile/profile.py
(the AUTO runs). OUT/NAME.cg is callgrind's file; OUT/NAME.txt its self-cost table (callgrind_annotate,
files:functions, instructions). --summary prints, per workload, the functions above 1% of its instructions
(a model load or a startup's share excluded when it is the whole run: see the study).
The integration models are examples/ode's, their `integration:` line replaced by the workload's own, so one
model runs under every integrator and each run is seconds under valgrind.
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
os.environ.setdefault('XPP_CHECK_SLOW', '400')  # valgrind is 50 times slower: only the client's safety limits grow
from xppclient import Server, is_ask, is_idle  # noqa: E402
EX = ROOT / 'examples/ode'
TOOLS = ROOT / 'tools/models'
LECAR = 'lecar.odex'
# a model whose right-hand side costs almost nothing, so a post-processing step is the run
HARMONIC = ('harmonic.odex', 'x\' = y\ny\' = -x\ninit x = 1\naux e = x*x+y*y\n')

# name -> (model file, the integration: block, extra options block members, command-line arguments)
RUNS = {
    # one model, every integrator (W476: lecar, 2 states)
    'int_rk4':        (EX / LECAR, 'method: runge_kutta, dt: 0.01, total_time: 300', '', []),
    'int_euler':      (EX / LECAR, 'method: euler, dt: 0.001, total_time: 150', '', []),
    'int_modeuler':   (EX / LECAR, 'method: modified_euler, dt: 0.002, total_time: 150', '', []),
    'int_qualrk':     (EX / LECAR, 'method: qualrk, dt: 0.01, total_time: 1500', '', []),
    'int_dopri5':     (EX / LECAR, 'method: dopri5, dt: 0.01, total_time: 3000', '', []),
    'int_dopri8':     (EX / LECAR, 'method: dopri8, dt: 0.01, total_time: 3000', '', []),
    'int_cvode':      (EX / LECAR, 'method: cvode, dt: 0.01, total_time: 3000', '', []),
    'int_adams':      (EX / LECAR, 'method: adams, dt: 0.01, total_time: 300', '', []),
    'int_stiff':      (EX / LECAR, 'method: stiff, dt: 0.01, total_time: 1500', '', []),
    'int_gear':       (EX / LECAR, 'method: gear, dt: 0.01, total_time: 1500', '', []),
    'int_rosenbrock': (EX / LECAR, 'method: rosenbrock, dt: 0.01, total_time: 1500', '', []),
    'int_backeuler':  (EX / LECAR, 'method: backward_euler, dt: 0.01, total_time: 300', '', []),
    # larger systems
    'int_kuramoto100_rk4': (EX / 'kuramot100.odex', 'total_time: 1.5, state_limit: 100000', '', []),
    'int_lecdiff200_cvode': (EX / 'lecdiff200.odex', 'method: cvode, dt: 0.05, total_time: 60', '', []),
    'int_lecdiff200_stiff': (EX / 'lecdiff200.odex', 'method: stiff, dt: 0.05, total_time: 2', '', []),
    'int_lecdiff200_gear': (EX / 'lecdiff200.odex', 'method: gear, dt: 0.05, total_time: 30', '', []),
    # delay, map, Volterra, DAE
    'int_dde':        (EX / 'dde.odex', 'delay: 10, total_time: 1500', '', []),
    'int_map':        (EX / 'kohonen.odex', 'method: discrete, total_time: 3000, store_every: 8', '', []),
    'int_volterra':   (EX / 'tstvol2.odex', 'total_time: 5, dt: 0.0005', '', []),
    'int_dae':        (EX / 'dae_ex1.odex', 'total_time: 10000, dt: 0.01', '', []),
    # non-solver work
    'nullclines_dfield': (EX / LECAR, 'method: runge_kutta, dt: 0.05, total_time: 3',
                          'plot: { nullcline_mesh: 400, dfgrid: 100, dfdraw: 1, nullclines.draw: true, xlo: -.6, xhi: 1.2, ylo: -.25, yhi: 1.2, xp: v, yp: w }',
                          ['--dfdraw', '1', '--ncdraw', '1']),
    'fourier':        (HARMONIC, 'method: euler, dt: 0.001, total_time: 130, storage_rows: 140000',
                       'analysis: { postprocess: 2 }', []),
    'histogram':      (HARMONIC, 'method: euler, dt: 0.001, total_time: 130, storage_rows: 140000',
                       'analysis: { postprocess: 1, histcol: x, histbins: 64, histlo: -1.2, histhi: 1.2 }', []),
    'load_kuramoto100': (EX / 'kuramot100.odex', 'total_time: 0.01, state_limit: 100000', '', []),
    'load_longnames': (TOOLS / 'longnames.ode', None, '', []),
    'load_lecdiff200': (EX / 'lecdiff200.odex', 'total_time: 0.01', '', []),
}
# tests/auto descriptions (tools/auto-profile/profile.py): equilibrium, periodic, two-parameter, BVP, homoclinic
AUTO = ['tstauto_ab', 'tstauto_pp2', 'tstauto_brper', 'ode_motoneuron6c', 'tstauto_ab_hopf', 'tstauto_ab_fold',
        'forced_pp_ns', 'tstauto_dumbvp', 'tstauto_exp', 'ode_tstheti', 'lorenz_homoclinic', 'tstauto_tsthom']


def callgrind(binary, out, name, argv, cwd):
    cg = out / f'{name}.cg'
    subprocess.run(['valgrind', '--tool=callgrind', f'--callgrind-out-file={cg}', '--error-exitcode=0', '-q',
                    str(binary), *argv], cwd=cwd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return cg


def table(cg, name, out):
    text = subprocess.run(['callgrind_annotate', '--inclusive=no', '--tree=none', str(cg)], check=True,
                          capture_output=True, text=True).stdout
    (out / f'{name}.txt').write_text(text)


def drop_group(text, group):
    """text without the options group `group: { ... }` (braces matched; the model's own, replaced by the workload's)"""
    m = re.search(r'^\s*%s:\s*\{' % group, text, re.M)
    if not m:
        return text
    depth, i = 1, m.end()
    while depth:
        depth += {'{': 1, '}': -1}.get(text[i], 0)
        i += 1
    return text[:m.start()] + text[i:].removeprefix(',')


def run_model(binary, out, name):
    model, integration, options, args = RUNS[name]
    with tempfile.TemporaryDirectory(prefix='w476-') as d:
        if isinstance(model, tuple):  # an inline model: (file name, text)
            model, text = Path(model[0]), model[1]
        else:
            text = model.read_text()
        if integration is not None:
            for group in re.findall(r'(\w+):\s*\{', options):
                text = drop_group(text, group)
            text = drop_group(text, 'integration')
            block = 'integration: { %s },\n  %s' % (integration, options + (',' if options else ''))
            if re.search(r'options\s*\{', text):
                text = re.sub(r'options\s*\{', 'options {\n  ' + block, text, count=1)
            else:
                text += 'options {\n  ' + block + '\n}\n'
        elif options:
            raise SystemExit('options need an integration block')
        path = Path(d) / model.name
        path.write_text(text)
        table(callgrind(binary, out, name, [path.name, '--silent', '--noout', *args], d), name, out)


def run_server(binary, out, name):
    """the page's data work: one run of lecar with every data event on (series, plots, nullclines, dfield, marks), as
    the browser asks for them; the whole process under callgrind"""
    cg_dir = tempfile.mkdtemp(prefix='w476-cg-')
    os.environ.update(XPP_PROFILED=str(binary), CALLGRIND_OUT=cg_dir)
    s = Server(ROOT / 'tools/auto-profile/callgrind_wrap.sh', EX / LECAR)
    try:
        s.collect(is_idle)
        s.send(cmd='data', events=['series', 'plots', 'nullclines', 'dfield', 'marks'])
        s.collect(is_idle)
        for field, value in (('total', 100), ('dt', 0.005)):  # Numerics: 20000 rows
            s.send(cmd='set', kind='num', name=field, value=value)
            s.collect(is_idle)
        s.send(cmd='key', key='i')
        while True:
            evs, e = s.collect(lambda e: is_ask(e) or is_idle(e))
            if e is None or is_idle(e):
                break
            s.send(cmd='answer', id=e['id'], key='g')
    finally:
        s.close()
    cg = sorted(Path(cg_dir).glob('cg.*.out*'), key=os.path.getmtime)[-1]
    shutil.copy(cg, out / f'{name}.cg')
    shutil.rmtree(cg_dir, ignore_errors=True)
    table(out / f'{name}.cg', name, out)


def parse(path):
    """(total, [(share %, instructions, file:function)]) of a callgrind_annotate table"""
    total, rows = None, []
    for line in path.read_text().split('-- Auto-annotated source')[0].splitlines():
        m = re.match(r'\s*([\d,]+) \(\s*([\d.]+)%\)\s+(.*)', line)
        if not m:
            continue
        ir = int(m.group(1).replace(',', ''))
        if m.group(3).startswith('PROGRAM TOTALS'):
            total = ir
        else:
            fn = re.sub(r' \[[^\]]*\]$', '', m.group(3))
            fn = fn.replace(str(ROOT) + '/', '')
            rows.append((float(m.group(2)), ir, fn[:150]))
    return total, rows


def lines(path, source, minimum):
    """the source lines of `source` (a path ending) that cost at least `minimum` % of the workload, as callgrind_annotate
    annotated them (its 'Auto-annotated source' sections, per line, self cost)"""
    out, current = [], None
    for line in path.read_text().splitlines():
        if line.startswith('-- Auto-annotated source:'):
            current = line.endswith(source) or source in line
            continue
        m = re.match(r'\s*([\d,]+) \(\s*([\d.]+)%\)\s+(.*)', line)
        if current and m and '=>' not in m.group(3) and float(m.group(2)) >= minimum:
            out.append(f'{float(m.group(2)):6.2f}%  {int(m.group(1).replace(",", "")) / 1e6:9.2f} M  {m.group(3).strip()[:140]}')
    return out


def compare(first, second):
    """the total instructions of every workload in two output folders (two builds), and their ratio"""
    print(f'{"workload":24} {"M instr A":>12} {"M instr B":>12} {"B/A":>7}')
    for t in sorted(first.glob('*.txt')):
        if (second / t.name).exists():
            a, b = parse(t)[0], parse(second / t.name)[0]
            print(f'{t.stem:24} {a / 1e6:12.1f} {b / 1e6:12.1f} {b / a:7.3f}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('binary')
    ap.add_argument('out')
    ap.add_argument('workloads', nargs='*')
    ap.add_argument('--summary', action='store_true')
    ap.add_argument('--compare', metavar='OUT2', help='print the totals of OUT and OUT2 (another build) side by side')
    ap.add_argument('--lines', metavar='FILE', help='print the source lines of FILE (a path ending) above --min percent')
    ap.add_argument('--min', type=float, default=0.3)
    a = ap.parse_args()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    names = a.workloads or list(RUNS) + ['plot_data'] + AUTO
    if a.compare:
        compare(out, Path(a.compare).resolve())
        return
    if a.lines:
        for name in names:
            print(f'== {name}: {a.lines}')
            print('\n'.join(lines(out / f'{name}.txt', a.lines, a.min)))
        return
    for name in names:
        if a.summary:
            continue
        if name == 'plot_data':
            run_server(Path(a.binary).resolve(), out, name)
        elif name in RUNS:
            run_model(Path(a.binary).resolve(), out, name)
        else:  # an AUTO description: its callgrind file is OUT/NAME.all.cg
            subprocess.run(['uv', 'run', '--locked', 'python',
                            str(ROOT / 'tools/auto-profile/profile.py'), str(Path(a.binary).resolve()), str(out), name],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
            table(out / f'{name}.all.cg', name, out)
        print('profiled', name, flush=True)
    for name in names:
        t = out / f'{name}.txt'
        if not t.exists():
            continue
        total, rows = parse(t)
        print(f'\n== {name}: {total/1e6:.1f} M instructions')
        for share, ir, fn in rows:
            if share >= 1.0:
                print(f'{share:6.2f}%  {ir/1e6:9.2f} M  {fn}')


if __name__ == '__main__':
    main()
