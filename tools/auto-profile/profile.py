#!/usr/bin/env python3
"""W358 (#420): instruction counts of xppautX's AUTO per phase, from callgrind.

Runs a workload through tools/xppclient.py's Server with tools/auto-profile/callgrind_wrap.sh in
place of the program, so valgrind counts the whole process; the phases are cut with
callgrind_control (-d dumps what was counted, -z zeroes it), the program carries no measuring code.
Writes OUT/<workload>.<phase>.cg (callgrind's own format, read by tables.py) and
OUT/<workload>.<phase>.json (the phase's stored points, 'points'). With --tool newton (W412), gdb
breakpoints write ModelDB's BVP solves, steps, fold-function values and stored meshes to
OUT/<workload>.newton.jsonl (tools/auto-profile/newton.py) instead.

    uv run --locked python tools/auto-profile/profile.py BINARY OUT WORKLOAD [--nmax N] [--paper DIR]

WORKLOAD: py_s1bf (W246/W269's periodic run: phases setup, steady, periodic), modeldb (W269's ModelDB
267710 as examples/ode/motoneuron6c.odex, W429: the same phases), or a tests/auto description name (tstauto_lecar, ...: phase
'all', the description's runs at 1 thread). --paper is paper/benchmarks (git-ignored, so only
the maintainer's checkout has it; default: the main checkout's). Run it through tools/heavy.sh in WSL.
"""
import argparse, glob, json, os, shutil, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
# valgrind runs 50-100 times slower: only the safety limits of tools/xppclient.py grow (never a criterion)
os.environ.setdefault('XPP_CHECK_SLOW', '400')
from xppclient import Server, is_done  # noqa: E402


class Profile:
    """one valgrind process; cut() ends a phase"""
    def __init__(self, out, workload, tool):
        self.out, self.workload, self.tool = Path(out), workload, tool

    def cut(self, s, phase, points=None, zero=True):
        """dump what has been counted since the last cut as OUT/workload.phase.cg, then zero
        (under --tool gdb only the phase's points are kept)"""
        pid = s.proc.pid
        for flag in () if self.tool != 'callgrind' else ('-d', '-z') if zero else ('-d',):
            # callgrind acts when the process next runs, so wake it with a command that ends in done
            p = subprocess.Popen(['callgrind_control', flag, str(pid)], stdout=subprocess.PIPE, text=True)
            s.send(cmd='events.subscribe', events=['diagram'])
            s.collect(is_done)
            p.communicate(timeout=600)
            if flag == '-d':
                files = sorted(glob.glob(f'{os.environ["CALLGRIND_OUT"]}/cg.{pid}.out*'), key=os.path.getmtime)
                shutil.copy(files[-1], self.out / f'{self.workload}.{phase}.cg')
        if points is not None:
            (self.out / f'{self.workload}.{phase}.json').write_text(json.dumps({'points': points}))


def program(args):
    os.environ['XPP_PROFILED'] = os.path.abspath(args.binary)
    os.environ['CALLGRIND_OUT'] = tempfile.mkdtemp(prefix='w358-cg-')
    out = Path(args.out).resolve() / args.workload
    if args.tool == 'gdb':  # the matrices the eigenvalue routines are given (dump_matrices.py)
        os.environ['GDB_SCRIPT'] = str(HERE / 'dump_matrices.py')
        os.environ['MATRIX_OUT'] = f'{out}.matrices'
        Path(os.environ['MATRIX_OUT']).unlink(missing_ok=True)
    if args.tool == 'newton':  # W412: the BVP solves and steps (newton.py)
        os.environ['GDB_SCRIPT'] = str(HERE / 'newton.py')
        os.environ['NEWTON_OUT'] = f'{out}.newton.jsonl'
    return str(HERE / ('callgrind_wrap.sh' if args.tool == 'callgrind' else 'gdb_wrap.sh'))


def converted(args, src, ode, dest, edits=()):
    """a copy of the model folder src with ode converted to .odex by the program itself (the
    .odex files in paper/benchmarks are older than the format); edits: (file, old, new) text
    replacements first, for what the program has refused since the model was staged"""
    shutil.copytree(src, dest)
    for name, old, new in edits:
        f = Path(dest) / name
        f.write_text(f.read_text().replace(old, new))
    for old in Path(dest).glob('*.odex'):
        old.unlink()
    subprocess.run([os.path.abspath(args.binary), ode, '--convert', '--yes'], cwd=dest, check=True,
                   stdout=subprocess.DEVNULL)
    return Path(dest) / (Path(ode).stem + '.odex')


def periodic(args, prof, wrapper):
    sys.path[:0] = [str(Path(args.paper) / 'auto'), str(Path(args.paper) / 'modeldb-267710')]
    import explore_xppautx, hopf_xppautx, auto_xppautx
    real = hopf_xppautx.run

    def run(s, key, *a, **k):
        if args.periodic_settings and key == 'p':  # W412: AUTO's numerics or marks for the periodic run
            auto_xppautx._ok(explore_xppautx.step(s, cmd='auto', op='set', **args.periodic_settings)[0], 'settings')
        if args.nmax and key == 'p':  # a section of the periodic branch (the steady run must reach its Hopf point): valgrind is 50-100 times slower
            explore_xppautx.numerics(s, nmx=args.nmax)
        if key == 's':  # everything before AUTO's first run (start-up, ModelDB's integrations)
            prof.cut(s, 'setup')
        wall, cpu, labs, pts = real(s, key, *a, **k)
        prof.cut(s, {'s': 'steady', 'p': 'periodic'}[key], pts)
        return wall, cpu, labs, pts
    hopf_xppautx.run = auto_xppautx.run = run
    scratch = tempfile.mkdtemp(prefix='w358-model-')
    if args.workload == 'py_s1bf':
        only = Path(scratch) / 'src'
        only.mkdir()
        shutil.copy(hopf_xppautx.HERE / 'models' / 'PY_S1Bf_bench.ode', only)
        model = converted(args, only, 'PY_S1Bf_bench.ode', Path(scratch) / 'model')
        hopf_xppautx.measure(wrapper, model=str(model), threads=1)
    else:
        # W429: the model is the repository's (ModelDB 267710 as examples/ode/motoneuron6c.odex, W431), staged
        # with the integration's output file next to it
        model = Path(scratch) / 'model'
        model.mkdir()
        shutil.copy(ROOT / 'examples/ode/motoneuron6c.odex', model)
        auto_xppautx.MODEL_DIR, auto_xppautx.MODEL = model, 'motoneuron6c.odex'
        auto_xppautx.measure(wrapper, threads=1)
    shutil.rmtree(scratch, ignore_errors=True)


def description(args, prof, wrapper):
    import autocompare
    d = autocompare.read_description(autocompare.BASE / (args.workload + '.json'))
    data, count, _, _ = autocompare.compute(wrapper, d, 1)
    (prof.out / f'{args.workload}.all.json').write_text(json.dumps({'points': count}))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('binary')
    ap.add_argument('out')
    ap.add_argument('workload')
    ap.add_argument('--nmax', type=int, help='stop the periodic run of py_s1bf/modeldb after this many points (Nmax)')
    ap.add_argument('--tool', choices=('callgrind', 'gdb', 'newton'), default='callgrind',
                    help='callgrind: instruction counts; gdb: the matrices EISPACK is given (OUT/WORKLOAD.matrices); '
                         'newton: the BVP solves, steps and meshes (OUT/WORKLOAD.newton.jsonl)')
    ap.add_argument('--periodic-settings', type=json.loads, help="an `auto set` command's fields (JSON) for the periodic run")
    ap.add_argument('--paper', default='/mnt/c/gitRepos/xppautX/paper/benchmarks')
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    wrapper = program(args)
    prof = Profile(out, args.workload, args.tool)
    if args.workload in ('py_s1bf', 'modeldb'):
        periodic(args, prof, wrapper)
    else:
        description(args, prof, wrapper)
        files = sorted(glob.glob(os.environ['CALLGRIND_OUT'] + '/cg.*.out'), key=os.path.getmtime) if args.tool == 'callgrind' else []
        if files:
            shutil.copy(files[-1], out / f'{args.workload}.all.cg')
    shutil.rmtree(os.environ['CALLGRIND_OUT'], ignore_errors=True)


if __name__ == '__main__':
    main()
