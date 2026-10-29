"""W439: shared foreground execution, provenance and summary protocol."""
import argparse
import ast
import csv
import hashlib
import importlib.util
import inspect
import json
import os
from pathlib import Path
import platform
import resource
import shutil
import statistics
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
BENCH = HERE.parent
REPO = BENCH.parent.parent
TOOLS = REPO / 'tools'
sys.path.insert(0, str(TOOLS))
from comparison_measure import run_foreground
REPEATS = 5  # Settled fresh measured processes, after one excluded warm-up.
TIMEOUT = 3600  # Hang guard, never a timing acceptance criterion.
# Python's own implicit methods on large models take over an hour a run: five counted repetitions add nothing at a
# hundred-fold gap, so a program whose warm-up run took longer than this is counted once (W452, #515).
LONG_RUN_SECONDS = 600
LONG_RUN_REASON = 'warm-up run longer than %d s: one counted repetition' % LONG_RUN_SECONDS
PYTHON_DIR = TOOLS / 'python-integration'
INPUT_SUFFIXES = ('.ode', '.odex', '.inc', '.set')  # the files a launch copies beside the program's run


def save(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + '.tmp')
    temp.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    temp.replace(path)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def parser(description):
    p = argparse.ArgumentParser(description=description)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--xppautx', type=Path, required=True)
    p.add_argument('--xppaut', type=Path, required=True)
    p.add_argument('--auto-dir', type=Path, default=Path(os.environ.get('AUTO_DIR', str(Path.home() / 'auto-07p-0.9.4'))))
    p.add_argument('--machine-record', type=Path, required=True)
    p.add_argument('--check', action='store_true', help='one short untimed process per row; never a timing series')
    p.add_argument('--only', help='comma-separated workload names')
    p.add_argument('--repeats', type=int, default=REPEATS, help='counted repetitions after the warm-up (W460: one first, the rest later with --reuse)')
    p.add_argument('--stop-on-failure', action='store_true', help='stop at the first workload that fails or is refused, so it can be looked at')
    p.add_argument('--reuse', type=Path, help='an earlier attempt of this study: the rows of a program whose executable, measurement code and input files are unchanged are imported, not run again')
    return p


def initialize(a):
    global REPEATS
    REPEATS = a.repeats  # read at call time by repetitions, required_repetitions and summarize
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=False)
    a.xppautx, a.xppaut, a.auto_dir = a.xppautx.resolve(), a.xppaut.resolve(), a.auto_dir.resolve()
    record = json.loads(a.machine_record.read_text())
    save(a.out / 'machine.json', record)
    a.cores = physical_cores(record)
    save(a.out / 'provenance.json', dict(check=a.check, argv=sys.argv, python=sys.version,
         commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
         binaries={str(b): sha(b) for b in (a.xppautx, a.xppaut)}, cores=a.cores,
         protocol='one warm-up, five fresh processes (one when the warm-up exceeds LONG_RUN_SECONDS), rotating order, sample SD; checks untimed',
         adapter='registry command adapter; no protocol change during a series'))
    if not a.check and subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=REPO, text=True).strip():
        raise RuntimeError('comparison requires a clean tracked checkout')
    if not a.check and ('microsoft' in platform.release().lower() or sys.platform != 'linux'):
        raise RuntimeError('Timing runs only on the Ubuntu comparison machine, never on the laptop')


def physical_cores(record):
    lines = record['lscpu_extended'].splitlines()
    header = lines[0].split()
    seen, cpus = set(), []
    allowed = os.sched_getaffinity(0)
    for line in lines[1:]:
        r = dict(zip(header, line.split()))
        cpu, core = int(r['CPU']), (r.get('SOCKET', '0'), r['CORE'])
        if cpu in allowed and r.get('ONLINE', 'yes').lower() in ('yes', 'y', '1') and core not in seen:
            cpus.append(cpu)
            seen.add(core)
    if not cpus:
        raise ValueError('machine record lscpu -e has no available physical core')
    return cpus


READY_POLL_SECONDS = 10  # the poll interval of the wait for an idle machine, not a delay before a measurement
READY_LIMIT_SECONDS = 1800  # safety limit only: a machine busy for half an hour is a refusal, never a measurement


def before_series(a):
    # A desktop app's brief burst above the CPU limit must not end a study of many hours: wait for the machine to be idle,
    # printing who was busy, and refuse only when it stays busy. Every measurement still starts on a machine that passed the check.
    if a.check:
        return
    waited = 0
    while True:
        p = subprocess.run(['bash', str(TOOLS / 'comparison-setup.sh'), '--ready'], cwd=REPO, text=True, capture_output=True)
        print((p.stdout + p.stderr).strip(), flush=True)
        if p.returncode == 0:
            return
        if waited >= READY_LIMIT_SECONDS:
            raise RuntimeError('machine not idle after %d s: refusing to measure' % waited)
        time.sleep(READY_POLL_SECONDS)  # poll interval inside a wait for a condition
        waited += READY_POLL_SECONDS


def environment(home):
    home.mkdir(parents=True)
    env = dict(os.environ, HOME=str(home), XPP_CONFIG_DIR=str(home / 'config'), LC_ALL='C',
               OMP_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1', MKL_NUM_THREADS='1')
    env.pop('DISPLAY', None)
    return env


def command(argv, folder, *, measured=False, env=None, cpus=None, timeout=TIMEOUT):
    folder.mkdir(parents=True, exist_ok=True)
    argv = [str(v) for v in argv]
    if cpus:
        argv = ['taskset', '-c', ','.join(map(str, cpus)), *argv]
    save(folder / 'command.json', argv)
    if measured:
        start, before = time.perf_counter(), resource.getrusage(resource.RUSAGE_CHILDREN)
    with (folder / 'run.log').open('w') as log:
        try:
            p = run_foreground(argv, cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
            result = dict(exit_code=p.returncode, valid=p.returncode == 0)
        except subprocess.TimeoutExpired:
            result = dict(exit_code='timeout', valid=False)
    if measured:
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        result.update(wall_seconds=time.perf_counter() - start,
                      cpu_seconds=after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime)
    save(folder / 'process.json', result)
    return result


def progress(a, text):
    # One line per step, with the time, to the study's log and to progress.log: `tail -f` shows where a long run is.
    line = time.strftime('%H:%M:%S ') + text
    print(line, flush=True)
    with (a.out / 'progress.log').open('a') as f:
        f.write(line + '\n')


def repetitions(a):
    return range(1) if a.check else range(REPEATS + 1)


def rotation(rows, rep):
    k = rep % len(rows)
    return rows[k:] + rows[:k]


def required_repetitions(runs):
    # Five counted repetitions, or the one a long run is limited to (W452): rows say so themselves.
    return 1 if runs and all(r.get('repetitions_reason') for r in runs) else REPEATS


def summarize(records, folder):
    groups = {}
    for r in records:
        if r.get('repetition', 0) > 0:
            groups.setdefault((r['workload'], r['engine'], r.get('row', 'run')), []).append(r)
    summary = []
    for (workload, engine, row), runs in groups.items():
        required = required_repetitions(runs)
        valid = len(runs) == required and all(r.get('equal_work', False) and r.get('valid', False) for r in runs)
        out = dict(workload=workload, engine=engine, row=row, attempted=len(runs), counted_repetitions=len(runs),
                   repetitions_reason=runs[0].get('repetitions_reason'), equal_work=valid)
        # The python study's accuracy beside the time: the tolerance used and the error against the reference (the continuation's:
        # the worst difference of each quantity from the converged exact run, and the worst over its tolerance); sweep marks a
        # work-precision or output-spacing row (none for the equal-accuracy rows), with the output step dt and the rows written.
        for key in ('sweep', 'tolerance', 'dt', 'points', 'reference_max_abs_error', 'reference_first_outside_time', 'converged_error', 'converged_error_ratio'):
            out[key] = runs[0].get(key)
        for metric in ('wall_seconds', 'cpu_seconds', 'wall_per_point', 'cpu_per_point', 'wrapper_wall_seconds'):
            numbers = [r[metric] for r in runs if r.get(metric) is not None]
            out[metric + '_mean'] = statistics.mean(numbers) if valid and len(numbers) == required else None
            out[metric + '_sample_sd'] = statistics.stdev(numbers) if valid and len(numbers) == required and required > 1 else None
        summary.append(out)
    save(folder / 'summary.json', summary)
    if summary:
        with (folder / 'summary.csv').open('w', newline='') as f:
            writer = csv.DictWriter(f, list(summary[0]))
            writer.writeheader()
            writer.writerows(summary)


def is_long(rec):
    # The warm-up (repetition 0) of this program on this workload took longer than LONG_RUN_SECONDS.
    return rec.get('repetition') == 0 and (rec.get('wall_seconds') or 0) > LONG_RUN_SECONDS


# W452 (#515): a program whose code and options did not change is not rerun. A row's reuse key names what its timing
# depended on: the program (executable hash; Python's own files for python), the measurement code, the input files and
# the CPU it was pinned to. An earlier row is imported only when every part is equal.

def tree_sha(items):
    # items: (name, bytes) pairs; one digest of their names and contents, in name order.
    h = hashlib.sha256()
    for name, data in sorted(items):
        h.update(name.encode() + b'\0' + hashlib.sha256(data).digest())
    return h.hexdigest()


def python_sha():
    return tree_sha((f.name, f.read_bytes()) for f in PYTHON_DIR.glob('*.py'))


def measure_sha(*functions):
    # The measurement code: tools/comparison_measure.py (the clock and the process wait), common.py's command() and
    # environment() (pinning, environment, accounting) and the driver's own argv builder, which the caller passes. Not the
    # driver files whole: integration.py changes with every study and most of it (staging, gates) is not what is timed.
    return tree_sha([('comparison_measure.py', (TOOLS / 'comparison_measure.py').read_bytes())]
                    + [(f.__name__, inspect.getsource(f).encode()) for f in (command, environment, *functions)])


def inputs_sha(folder):
    return tree_sha((f.name, f.read_bytes()) for f in Path(folder).iterdir() if f.suffix in INPUT_SUFFIXES)


AUTO_DIRS = ('bin', 'lib', 'python', 'src', 'cmds')  # the parts of an AUTO-07p build that run


def auto_files(auto_dir):
    return [f for d in AUTO_DIRS for f in sorted((Path(auto_dir) / d).rglob('*')) if f.is_file() and f.suffix != '.pyc' and '__pycache__' not in f.parts]


def auto_sha(auto_dir):
    # AUTO-07p is a directory, not an executable: its running parts' names and contents (W454).
    root = Path(auto_dir)
    return tree_sha((str(f.relative_to(root)), f.read_bytes()) for f in auto_files(root))


def executable_sha(a, engine):
    cache = a.__dict__.setdefault('_executable_sha', {})
    if engine not in cache:
        cache[engine] = python_sha() if engine == 'python' else auto_sha(a.auto_dir) if engine == 'auto07p' else sha(getattr(a, engine))
    return cache[engine]


def reuse_key(a, engine, folder, measure, cpus):
    return dict(program=executable_sha(a, engine), measure=measure, inputs=inputs_sha(folder), cpus=list(cpus),
                interpreter=sys.version if engine == 'python' else None)


def git_text(commit, path):
    return subprocess.run(['git', 'show', '%s:%s' % (commit, path)], cwd=REPO, capture_output=True, check=True).stdout


def earlier_context(a):
    # What the earlier study recorded about itself: its provenance.json (executables' hashes, interpreter, commit).
    if 'earlier' not in a.__dict__:
        provenance = json.loads((a.reuse / 'provenance.json').read_text())
        a.earlier = dict(provenance=provenance, records=json.loads((a.reuse / 'runs.json').read_text()) if (a.reuse / 'runs.json').exists() else [])
    return a.earlier


def derived_program_sha(a, engine):
    # The program hash of an earlier row that recorded none, from the earlier study's provenance.json: its binaries' hashes
    # (the --xppautx and --xppaut arguments, in that order, as initialize() wrote them), Python's files as of its commit.
    provenance = earlier_context(a)['provenance']
    if engine == 'auto07p':
        # No hash was recorded (W454): the current build counts as the earlier one when the earlier study used the same folder
        # and no running part of it is newer than the study's provenance.json (the build's files are older than the study).
        argv = provenance['argv']
        same_folder = '--auto-dir' in argv and Path(argv[argv.index('--auto-dir') + 1]).resolve() == a.auto_dir
        started = (a.reuse / 'provenance.json').stat().st_mtime
        return executable_sha(a, engine) if same_folder and all(f.stat().st_mtime <= started for f in auto_files(a.auto_dir)) else None
    if engine == 'python':
        names = subprocess.check_output(['git', 'ls-tree', '--name-only', provenance['commit'], 'tools/python-integration/'], cwd=REPO, text=True).split()
        return tree_sha((Path(n).name, git_text(provenance['commit'], n)) for n in names if n.endswith('.py'))
    return list(provenance['binaries'].values())[('xppautx', 'xppaut').index(engine)]


def derived_key(a, rec, folder, measure, cpus):
    # An earlier row with no reuse_key (a series before W452): its key from what is on disk, never guessed.
    #  program: provenance.json's recorded hashes (derived_program_sha);
    #  inputs: the row's own folder, which holds the files the launch copied;
    #  cpus and argv: the row's command.json, which must equal what the current argv builder would give (checked by the caller
    #   through `argv_equal`, since only it knows the builder);
    #  measure: the current measurement code's hash when tools/comparison_measure.py at the earlier commit is byte-identical
    #   to the current file. common.py's command()/environment() are git-ignored and were not recorded, so they are taken as
    #   unchanged: W452 only adds to common.py and did not edit them.
    provenance = earlier_context(a)['provenance']
    try:
        same_clock = git_text(provenance['commit'], 'tools/comparison_measure.py') == (TOOLS / 'comparison_measure.py').read_bytes()
    except subprocess.CalledProcessError:
        same_clock = False
    return dict(program=derived_program_sha(a, rec['engine']), measure=measure if same_clock else None, inputs=inputs_sha(folder),
                cpus=list(cpus), interpreter=provenance['python'] if rec['engine'] == 'python' else None, derived=True)


def link_tree(src, dest):
    # Hard links where the filesystem allows, so an imported run costs no space; never a second program run.
    def link(s, d):
        try:
            os.link(s, d)
        except OSError:
            shutil.copy2(s, d)
    shutil.copytree(src, dest, copy_function=link)


def mark_reused(a, rec):
    rec = dict(rec)
    if 'reused_from' in rec:
        rec['earlier_reused_from'] = rec['reused_from']
    rec['reused_from'] = str(a.reuse)
    return rec


def selected(a, name):
    return not a.only or name in a.only.split(',')


# W454 (#517): the tracked code a row's measurement depends on, as of the current tree or of an earlier commit.

def tracked_text(path, commit=None):
    # A tracked file's text in the working tree, or as of `commit` ('' when it did not exist there).
    if commit is None:
        return (REPO / path).read_text(encoding='utf-8')
    try:
        return git_text(commit, path).decode('utf-8')
    except subprocess.CalledProcessError:
        return ''


def reachable_sha(text, roots):
    # One digest of the top-level functions, classes and constants of a module that `roots` reach through names used in
    # their bodies (what a change to any of them could change in a root's behaviour); a root missing from the text
    # counts as nothing, so a file before the root existed differs.
    tree = ast.parse(text)
    top = {}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef)):
            top[node.name] = node
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    top[target.id] = node
    seen, stack = [], list(roots)
    while stack:
        name = stack.pop()
        if name in seen or name not in top:
            continue
        seen.append(name)
        stack += [n.id for n in ast.walk(top[name]) if isinstance(n, ast.Name) and n.id in top]
    return tree_sha((name, ast.get_source_segment(text, top[name]).encode()) for name in seen)
