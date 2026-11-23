"""W439: shared foreground execution, provenance and summary protocol."""
import argparse
import ast
from contextlib import contextmanager
import csv
import functools
import hashlib
import importlib.util
import inspect
import json
import os
from pathlib import Path
import platform
import re
import resource
import shutil
import statistics
import subprocess
import sys
import tempfile
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
XPPAUT_READS = ('.ode', '.inc', '.set')  # what XPPAUT reads of its input: the model, its included files and its SET (-setfile)


def save(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + '.tmp')
    temp.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    temp.replace(path)


SHA_CACHE = 'XPP_COMPARISON_SHA_CACHE'  # W802: a JSON file shared by the drivers of one run.py (it makes it); unset, nothing is kept
_digests = {}


def sha(path):
    # A file's digest, kept by its path, size and modification time: every driver of a run hashes the same binaries and models again
    # (a 184-unit plan spent a quarter of its time on it), so the digest is remembered in the process and, through SHA_CACHE, by
    # the next driver.
    path = Path(path).resolve()
    status = path.stat()
    key = '%s|%d|%d' % (path, status.st_size, status.st_mtime_ns)
    file = os.environ.get(SHA_CACHE)
    if file and not _digests and Path(file).exists():
        _digests.update(json.loads(Path(file).read_text()))
    if key not in _digests:
        _digests[key] = hashlib.sha256(path.read_bytes()).hexdigest()
        if file:
            save(file, _digests)
    return _digests[key]


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
    p.add_argument('--reuse', type=Path, help='an earlier attempt of this workload: the rows of a program whose executable, measurement code and input files are unchanged are imported, not run again')
    p.add_argument('--list', action='store_true', help='print the names of the study\'s workloads (its units, W699) as JSON and stop; runs nothing')
    p.add_argument('--plan', action='store_true', help='with --reuse: write plan.json, which rows the earlier attempt supplies and which would run, decided by the same keys the run uses; no timing')
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
    a.timed = not (a.check or a.plan or a.list)  # a listing and a plan time nothing
    if a.timed and subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=REPO, text=True).strip():
        raise RuntimeError('comparison requires a clean tracked checkout')
    if a.timed and ('microsoft' in platform.release().lower() or sys.platform != 'linux'):
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


READY_POLL_SECONDS = 60  # a busy machine is checked again a minute later (maintainer, 2026-11-17): the poll interval of the wait for idle, not a delay before a measurement
READY_LIMIT_SECONDS = 1800  # safety limit only: a machine busy for half an hour is a refusal, never a measurement


def wait_until_ready():
    # A desktop app's brief burst above the CPU limit must not end a study of many hours: wait for the machine to be idle,
    # printing who was busy, and refuse only when it stays busy. Every measurement still starts on a machine that passed the check.
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


# W829 (#910): the CPU other processes used while a timed unit ran. This is the one measure of the comparison: --ready's per-process
# check and each timed unit's record both read /proc. The unit's record is the machine's busy CPU over the unit (/proc/stat's first
# line) minus the series' own CPU (run.py, the children it reaped and the live processes below it), so a process that starts and
# exits inside the unit is in the total. No polling thread: a unit's record is its readings at its start and end.
CLOCK_TICKS = os.sysconf('SC_CLK_TCK')  # /proc's CPU ticks per second
DISTURBED_CPU_FRACTION = 0.5  # CPUs of other processes over a whole unit: a quiet desktop (Chrome, the desktop app, gnome-shell, the idle Claude sessions) uses about 0.15 CPU, a build several
STALL_SECONDS = 0.5  # W845: a one-CPU xppautX row whose wall time passes its own CPU time by more than this stalled (waited off the CPU): the 1483 level one-CPU xppautX rows of results/ and w718-20261120f gap at most 0.19 s (wall/cpu 1.004 on 43 s rows); the stalled E1 row gapped 0.94 s
# W849 (#930): a pinned CPU's core or package was thermally throttled during a unit when its Linux thermal_throttle counter (ms) grew: the CPU clock fell to 800 MHz, the rows of that moment ran 1.6-5.5x their time.
# W851 (#932): any growth counts, with no seconds bound: throttling only lengthens times, so run.py judges a throttled unit as any other and only a slower verdict makes it disturbed.
THROTTLE_DIR = '/sys/devices/system/cpu/cpu%d/thermal_throttle/%s_throttle_total_time_ms'  # a machine without these files (macOS, a VM) reads no counters: its units are judged by the competing CPU and the stalled rows alone
COMPETING_TOP = 3  # the processes a record and a run.log line name, the largest CPU users first (best effort: the ones the process table shows)


def busy_ticks():
    # The machine's busy CPU ticks over every CPU: /proc/stat's first line, user, nice, system, irq, softirq and steal (idle, iowait and
    # guest are left out: guest is already in user).
    user, nice, system, _idle, _iowait, irq, softirq, steal = (int(v) for v in Path('/proc/stat').read_text().split('\n', 1)[0].split()[1:9])
    return user + nice + system + irq + softirq + steal


def process_table():
    # {pid: (parent pid, start time, own CPU ticks, reaped CPU ticks, command name)} of every process /proc lists now. Own ticks are
    # utime + stime; reaped ticks are cutime + cstime, what the process's waited-for children used. A process that exits while it is
    # read is left out: it cannot compete with the series any more.
    table = {}
    for path in Path('/proc').glob('[0-9]*/stat'):
        try:
            text = path.read_text()
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            continue
        # The command name may hold spaces and parentheses: the fields after its last ')' start at field 3 (the state), so field N is index N-3.
        fields = text.rsplit(')', 1)[1].split()
        name = text.split('(', 1)[1].rsplit(')', 1)[0]
        table[int(path.parent.name)] = (int(fields[1]), int(fields[19]), int(fields[11]) + int(fields[12]), int(fields[13]) + int(fields[14]), name)
    return table


def throttle_ms(cpus):
    # W849: {label: total ms the core of each CPU in cpus, and its package, were thermally throttled}; a counter the machine lacks is left out.
    readings = {}
    for cpu in cpus:
        for kind in ('core', 'package'):
            try:
                readings['%s of cpu%d' % (kind, cpu)] = int(Path(THROTTLE_DIR % (cpu, kind)).read_text())
            except (OSError, ValueError):
                pass
    return readings


def machine_snapshot(cpus=()):
    # One reading for a timed unit's record (W829): the busy CPU ticks, the process table and (W849) the throttle time of the CPUs the series pins.
    return dict(busy=busy_ticks(), table=process_table(), throttle=throttle_ms(cpus))


def process_tree(table, root):
    # The pids of root and of every process below it in table (its children, their children, ...), root included when it is there.
    children = {}
    for pid, (parent, _, _, _, _) in table.items():
        children.setdefault(parent, []).append(pid)
    tree, todo = set(), [root]
    while todo:
        pid = todo.pop()
        if pid not in tree:
            tree.add(pid)
            todo += children.get(pid, [])
    return tree


def series_ticks(table):
    # The series' own CPU ticks in a reading: run.py and every live process below it, each with what it reaped. A child reaped by a
    # process of the series moves into that process's reaped ticks, so the total does not drop when a child is waited for.
    return sum(table[pid][2] + table[pid][3] for pid in process_tree(table, os.getpid()) if pid in table)


def competing_cpu(before, after, exclude):
    # {pid: (own CPU seconds, name)} of the processes of `after` that are not in `exclude`: the own CPU each used between the two
    # readings. A process `before` did not hold (or a reused pid with another start time) started since, so all its CPU is the interval's.
    used = {}
    for pid, (_, start, own, _, name) in after.items():
        if pid in exclude:
            continue
        earlier = before.get(pid)
        base = earlier[2] if earlier and earlier[1] == start else 0
        used[pid] = ((own - base) / CLOCK_TICKS, name)
    return used


def competing_record(before, after, wall):
    # A timed unit's record of the CPU other processes used while it ran (W829): the machine's busy CPU over the interval minus the
    # series' own. The two reads are not the same instant, so a tick may come out negative: it is clamped to zero.
    busy = after['busy'] - before['busy']
    own = series_ticks(after['table']) - series_ticks(before['table'])
    competing = max(0, busy - own) / CLOCK_TICKS
    fraction = competing / wall
    used = competing_cpu(before['table'], after['table'], process_tree(after['table'], os.getpid()))  # best effort: the names
    # W849: the growth of each throttle counter both readings hold, the largest one decides (W851: any growth is throttling; the verdict is about other processes only)
    throttled = {k: (after.get('throttle', {})[k] - v) / 1000 for k, v in before.get('throttle', {}).items() if k in after.get('throttle', {})}
    throttle_seconds = max(throttled.values(), default=0.0)
    largest = sorted(used.items(), key=lambda item: item[1][0], reverse=True)[:COMPETING_TOP]
    return dict(wall_seconds=wall, competing_cpu_seconds=competing, competing_fraction=fraction, bound=DISTURBED_CPU_FRACTION,
                verdict='disturbed' if fraction > DISTURBED_CPU_FRACTION else 'quiet',
                throttle_seconds=throttle_seconds, throttled={k: v for k, v in throttled.items() if v > 0},
                busy_cpu_seconds=busy / CLOCK_TICKS, series_cpu_seconds=own / CLOCK_TICKS,
                processes=[dict(pid=pid, name=name, cpu_seconds=s) for pid, (s, name) in largest if s > 0])


def stalled_rows(rows):
    # W845: the rows of a runs.json that stalled: xppautX on one CPU (pinned_cpus of length 1) with wall_seconds - cpu_seconds above
    # STALL_SECONDS. A competing process of a few ms of CPU a time does not move W829's unit-wide fraction, yet delays one row. Only
    # xppautX's rows are judged: its cpu_seconds is the whole program's, while XPPAUT's and AUTO-07p's wall time includes work their CPU
    # time leaves out (X driving, a compile step: a gap of 4 s and 1.4 s in level rows); a row on several threads waits on its own
    # threads (an unequal split of the work), so it is not judged either.
    return [dict(workload=r.get('workload'), engine=r['engine'], repetition=r.get('repetition'), gap_seconds=r['wall_seconds'] - r['cpu_seconds']) for r in rows
            if str(r.get('engine')).startswith('xppautx') and len(r.get('pinned_cpus') or ()) == 1
            and isinstance(r.get('wall_seconds'), (int, float)) and isinstance(r.get('cpu_seconds'), (int, float))
            and r['wall_seconds'] - r['cpu_seconds'] > STALL_SECONDS]


def before_series(a):
    if a.timed:
        wait_until_ready()


@contextmanager
def short_home():
    """A HOME for one run, short: XPPAUT 8.0 writes $HOME/<model file name> into a 200-byte array unchecked (docs/xppaut-findings.md
    finding 75), and a run folder inside the series is longer than that with the model's name (W562). A folder under the system
    temp dir, removed when the run that uses it is over."""
    home = Path(tempfile.mkdtemp(prefix='xpphome'))
    try:
        yield home
    finally:
        shutil.rmtree(home)


def environment(home):
    home.mkdir(parents=True, exist_ok=True)
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
        for metric in ('wall_seconds', 'cpu_seconds', 'wall_per_point', 'cpu_per_point', 'wrapper_wall_seconds', 'per_run_seconds', 'startup_seconds'):
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
    # the Python arm's program: its files, and the numerics defaults it starts from, which it reads from the options schema (W804)
    sys.path.insert(0, str(PYTHON_DIR))
    from numerics import defaults
    return tree_sha([(f.name, f.read_bytes()) for f in PYTHON_DIR.glob('*.py')] + [('numerics-defaults.json', json.dumps(defaults(), sort_keys=True).encode())])


# W803 (#884): python_equivalence.py proves an earlier hash of the Python integrators gave the very bytes of today's and writes
# python-equivalence.json. It stands for the Python arm only: an earlier row's program hash is read as today's when the
# record says every unit was identical and both hashes are still the ones it names; no other program's hash goes through it.
EQUIVALENCE = HERE / 'python-equivalence.json'


def proven_python_program(program):
    # `program`, an earlier row's hash of the Python integrators, as today's hash when the record proves them equal, else itself.
    if EQUIVALENCE.exists():
        record = json.loads(EQUIVALENCE.read_text())
        if record['all_identical'] and program == record['earlier_program_sha256'] and python_sha() == record['current_program_sha256']:
            return python_sha()
    return program


def earlier_program(engine, program):
    return proven_python_program(program) if engine == 'python' else program


def measure_sha(*functions):
    # The measurement code: tools/comparison_measure.py (the clock and the process wait), common.py's command() and
    # environment() (pinning, environment, accounting) and the driver's own argv builder, which the caller passes. Not the
    # driver files whole: integration.py changes with every study and most of it (staging, gates) is not what is timed.
    return tree_sha([('comparison_measure.py', (TOOLS / 'comparison_measure.py').read_bytes())]
                    + [(f.__name__, inspect.getsource(f).encode()) for f in (command, environment, *functions)])


# W809 (#890) named the equilibrium and continuation options by their sections and put the table's rows in the sections' order:
# {group: {name before: name now}}, in the order before it (an unchanged name maps to itself). The same settings: an earlier
# row's key, a description or a .odex written before it, is read under these (continuation's description_sha, odex_before_w809).
W809_NAMES = dict(
    equilibria={'max_iterations': 'newton.max_iterations', 'newton_tolerance': 'newton.tolerance', 'jacobian_step': 'newton.jacobian_step',
                'search_limit': 'newton.search_limit', 'print_eigenvalues': 'print_eigenvalues', 'draw_invariant_sets': 'draw_invariant_sets',
                'draw_strong_sets': 'draw_strong_sets', 'range_parameter': 'range.parameter', 'range_steps': 'range.steps', 'range_low': 'range.low',
                'range_high': 'range.high', 'range_shoot': 'range.shoot', 'range_stability_column': 'range.stability_column'},
    continuation={'mesh_intervals': 'mesh.intervals', 'max_steps': 'limits.steps', 'output_every': 'output.every',
                  'collocation_points': 'mesh.collocation_points', 'min_step_size': 'step.min', 'max_step_size': 'step.max',
                  'step_size': 'step.initial', 'parameter_min': 'limits.parameter_min', 'parameter_max': 'limits.parameter_max',
                  'norm_min': 'limits.norm_min', 'norm_max': 'limits.norm_max', 'parameter_tolerance': 'newton.parameter_tolerance',
                  'state_tolerance': 'newton.state_tolerance', 'special_point_tolerance': 'newton.special_point_tolerance',
                  'mesh_adapt_every': 'mesh.adapt_every', 'max_branch_switches': 'max_branch_switches',
                  'switch_one_direction': 'one_way_branch_switch', 'output_detail': 'output.detail',
                  'max_location_iterations': 'newton.max_location_iterations', 'max_newton_iterations': 'newton.max_iterations',
                  'full_newton_after': 'newton.full_after_steps', 'step_adapt_every': 'step.adapt_every',
                  'suppress_branch_points': 'suppress_branch_points', 'follow_all_branches': 'follow_all_branches',
                  'special_points': 'special_points'})
W809_GROUP = re.compile(r'^( *)(%s): \{([^{}]*)\}' % '|'.join(W809_NAMES), re.M)
ODEX_ITEM = re.compile(r'\s*([a-z_][a-z_.]*): ("[^"]*"|[^,"]*?)\s*(?:,|$)')  # a group's item: a name and one value, a quoted one with its commas
ODEX_LINE_WIDTH = 100  # core/odex_write.cpp's line_width: a group's items wrap at it


def odex_braced(items, indent):
    # core/odex_write.cpp's braced(): a group's items on one line, or wrapped at ODEX_LINE_WIDTH, the way the converter writes them
    line = '{ ' + ', '.join(items) + ' }'
    if indent + len(line) <= ODEX_LINE_WIDTH:
        return line
    pad, out, row = ' ' * (indent + 2), '{\n', ''
    for item in items:
        if row and len(pad) + len(row) + 2 + len(item) > ODEX_LINE_WIDTH:
            out, row = out + pad + row + ',\n', ''
        row += (', ' if row else '') + item
    return out + pad + row + ',\n' + ' ' * indent + '}'


def table_order(group, names):
    # whether these options of the group are in the option table's order today (the schema lists each group's in it): a writer
    # that writes the table's rows, a session's .odex, put them so; a converted .ode keeps its own statements' order
    order = list(json.loads((REPO / 'docs/odex-options.schema.json').read_text())['properties'][group]['properties'])
    return [order.index(n) for n in names] == sorted(order.index(n) for n in names)


def odex_before_w809(text):
    # A .odex's text with its equilibria and continuation options named, ordered (where written in the table's order) and
    # wrapped as before W809: the rows recorded before it hashed those bytes, and a .odex written now differs from them in these only. A group this cannot read whole
    # (an item it does not know) is left as written.
    def group(m):
        names = W809_NAMES[m.group(2)]
        back = {now: before for before, now in names.items()}
        body = m.group(3).strip()
        found = list(ODEX_ITEM.finditer(body))
        items = [(back.get(i.group(1)), i.group(2)) for i in found]
        if not body or ''.join(i.group(0) for i in found) != body or any(name is None for name, _ in items):
            return m.group(0)
        if table_order(m.group(2), [names[name] for name, _ in items]):  # a whole table written (a session's), not a converted .ode's
            order = list(names)
            items = sorted(items, key=lambda item: order.index(item[0]))
        return '%s%s: %s' % (m.group(1), m.group(2), odex_braced(['%s: %s' % item for item in items], len(m.group(1))))
    return W809_GROUP.sub(group, text)


def input_bytes(f):
    # what a run's input file is keyed by: its bytes, a .odex's under the names before W809 (odex_before_w809)
    return odex_before_w809(f.read_text(encoding='utf-8')).encode('utf-8') if f.suffix == '.odex' else f.read_bytes()


def inputs_sha(folder, suffixes=INPUT_SUFFIXES):
    return tree_sha((f.name, input_bytes(f)) for f in Path(folder).iterdir() if f.suffix in suffixes)


# .odex options that change no number, only the picture the array plot draws: each is the text of one of its titles
# (core/commands_array_plot.cpp). A Python program's model key leaves exactly these out, each one named with its reason (W806).
DISPLAY_OPTIONS = {
    'array_plot.top_label': 'the text above the picture (its x title)',
    'array_plot.side_label': 'the text beside it, up its side (its y title)',
    'array_plot.bottom_label': 'the text under it (its bottom title)',
}


@functools.cache
def numeric_defaults():
    # The numeric options the schema gives a default (core/model_options.cpp, W804), {.odex name: default}: an option written at
    # its default and an option not written are one model to every program, so a converter that starts writing one (W804's
    # global.min_step_fraction) changes no key.
    sys.path.insert(0, str(PYTHON_DIR))
    from numerics import option_rows
    return {name: float(prop['default']) for name, _, prop in option_rows()
            if prop.get('type') in ('number', 'integer') and 'default' in prop}


def model_key(path):
    # The .odex's digest without its DISPLAY_OPTIONS items and without any numeric option written at its default (numeric_defaults).
    # The generated .odex writes a comma after each item of a group, so an item is taken out with the comma before it: a
    # label-only change keeps the key, a number's change does not. An item first in its group is not taken out, so its change
    # changes the key (the safe side).
    path = Path(path)
    text = odex_before_w809(path.read_text(encoding='utf-8'))
    for key in DISPLAY_OPTIONS:
        text = re.sub(r',\s*%s\s*:\s*(?:"[^"]*"|[^,}\n]*)' % re.escape(key), '', text)
    for key, default in numeric_defaults().items():
        text = re.sub(r',\s*%s\s*:\s*([-+.\deE]+)(?=\s*[,}\n])' % re.escape(key),
                      lambda m: '' if float(m.group(1)) == default else m.group(0), text)
    text = re.sub(r',\s+', ', ', text)  # the converter wraps an options group's items at its width: an item taken out reflows the lines
    return tree_sha([(path.name, text.encode('utf-8'))])


# W827 (#908): a row is keyed by what its program reads of its input, never by every file a launch copies beside it. XPPAUT reads
# the model, its included files and its SET (XPPAUT_READS); xppautX reads the .odex it is given; the Python programs read the
# .odex's numbers (model_key). The .odex is xppautX's own conversion: a converter change that writes a new option reruns
# neither XPPAUT's rows nor any Python row whose numbers it does not change. The plan and the run both key rows here.
PYTHON_ENGINES = ('python', 'reference')


def program_inputs(engine, model):
    # The digest of what `engine` reads of the input `model` (its .ode path; the .odex and the rest of its folder beside it).
    model = Path(model)
    if engine == 'xppaut':
        return inputs_sha(model.parent, XPPAUT_READS)
    if engine in PYTHON_ENGINES:
        return model_key(model.with_suffix('.odex'))
    return inputs_sha(model.parent, ('.odex',))


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
        # a study whose engines are not one program of --xppautx/--xppaut (W295: Python scripts driving it) names each one's hash
        cache[engine] = a.program_sha[engine]() if engine in getattr(a, 'program_sha', {}) else python_sha() if engine == 'python' else auto_sha(a.auto_dir) if engine == 'auto07p' else sha(getattr(a, engine))
    return cache[engine]


def reuse_key(a, engine, measure, cpus, inputs):
    # A row's key: its program, measurement code, input digest (program_inputs, or a study's own), CPUs and interpreter.
    return dict(program=executable_sha(a, engine), measure=measure, inputs=inputs, cpus=list(cpus),
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
        return proven_python_program(tree_sha((Path(n).name, git_text(provenance['commit'], n)) for n in names if n.endswith('.py')))
    return list(provenance['binaries'].values())[('xppautx', 'xppaut').index(engine)]


def derived_key(a, rec, model, measure, cpus):
    # An earlier row with no reuse_key (a series before W452): its key from what is on disk, never guessed. `model`: the row's own
    # copy of the input model (its folder holds the files the launch copied).
    #  program: provenance.json's recorded hashes (derived_program_sha);
    #  inputs: program_inputs of that copy (what the row's program reads, W827);
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
    return dict(program=derived_program_sha(a, rec['engine']), measure=measure if same_clock else None, inputs=program_inputs(rec['engine'], model),
                cpus=list(cpus), interpreter=provenance['python'] if rec['engine'] == 'python' else None, derived=True)


def link_tree(src, dest):
    # An imported run is referenced, never copied (W802): hard links, so it costs no space and survives the earlier attempt's removal;
    # a different file system is the OSError, not a silent copy. A file's own link is replaced by `save`, never written through.
    src = Path(src)
    if src.is_dir():
        shutil.copytree(src, dest, copy_function=os.link)
    else:
        Path(dest).parent.mkdir(parents=True, exist_ok=True)
        os.link(src, dest)


def mark_reused(a, rec):
    rec = dict(rec)
    if 'reused_from' in rec:
        rec['earlier_reused_from'] = rec['reused_from']
    rec['reused_from'] = str(a.reuse)
    return rec


def selected(a, name, stem=None):
    # --only names workloads; a workload's stem (its model, before the method or duration the name adds) selects all of its workloads.
    return not a.only or name in a.only.split(',') or stem in a.only.split(',')


def wanted(a, stem):
    # Whether any workload of this model is selected, so a unit's run stages only its own model (W699).
    return not a.only or any(n == stem or n.startswith(stem + '-') for n in a.only.split(','))


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


# W699 (#774): a unit is one workload (a study's own name for it: a model, a model and method, a size and duration, a
# continuation description). run.py gives each its own folder <results>/<study>/<unit>/ holding its attempts
# (attempt-NNN: the driver's output, written for that one workload) and, once an attempt finished, done.json. A driver's
# `--plan` says what an earlier attempt of the unit supplies; the keys are the rows' own (W452, W575), so a unit's done record
# matches while its plan runs nothing.
STUDIES = ('terman', 'screen', 'modeldb', 'research', 'izhikevich', 'python', 'continuation', 'scaling', 'large-stiff', 'server')  # settled order
DONE = 'done.json'
PLAN = 'plan.json'
PASS1 = 'pass1.json'  # W818: a unit's pass-1 verdict, kept with the binaries it was judged for (its pass1/ folder is never counted)
STOPPED = 'stopped.json'  # W818: written when a series ends with a slower unit; the next run runs the slower unit again, and every unit whose binaries changed (W848)


def done_attempts(root, study):
    # {unit: the attempt folder its done record names} of every finished unit of a study, in name order.
    found = {}
    for record in sorted((Path(root) / study).glob('*/' + DONE)):
        found[record.parent.name] = record.parent / json.loads(record.read_text())['attempt']
    return found


def pooled(folders, name):
    # A study's rows: its units' own files, one after another (a unit has its own folder, W699).
    return [row for f in folders if (f / name).exists() for row in json.loads((f / name).read_text(encoding='utf-8'))]


# W818 (#899): a unit's verdict against the same unit of the reference run (run.py's --reuse). The time ratio is the new mean over
# the reference's (above 1: slower). The threshold is the reference's own spread: three of its sample SDs relative to its mean,
# and never less than VERDICT_MARGIN: one reference repetition has no SD, and a pass-1 run is a single repetition, so a stop on
# a smaller difference would stop the series on noise (W718 against W617 drifted up to 15% in medians); a quarter is the bar that
# found W817's 85 slower workloads.
VERDICT_MARGIN = 0.25
VERDICT_SD_FACTOR = 3


def program_of(engine):
    # The program an engine runs: its name's part before the first dash ('xppautx', 'xppaut', 'auto07p', 'python').
    return engine.split('-')[0]


def counted_rows(folder, xppautx):
    # {(workload, engine, row): summary row} of an attempt's counted rows: xppautX's when xppautx is True, the other programs' when
    # False. None when the folder is None; a sweep and a row with no mean are not counted. large-stiff's summary (W378's kit) has its
    # own layout with no engine: its rows are not comparison rows, so they are not counted.
    if folder is None:
        return {}
    return {(r['workload'], r['engine'], r['row']): r for r in pooled([Path(folder)], 'summary.json')
            if 'engine' in r and (program_of(r['engine']) == 'xppautx') == xppautx and not r.get('sweep')
            and r.get('wall_seconds_mean') is not None}


def timed_rows(folder):
    # {(workload, engine, row): summary row} of an attempt's counted xppautX rows; none when the folder is None or has no summary.
    return counted_rows(folder, True)


def fastest_by_program(rows):
    # {(workload, row, program): summary row} of counted rows: each program's fastest engine's row for each workload and row (W830).
    best = {}
    for (workload, engine, row), r in rows.items():
        key = (workload, row, program_of(engine))
        if key not in best or r['wall_seconds_mean'] < best[key]['wall_seconds_mean']:
            best[key] = r
    return best


def compare(new, ref, names):
    # The ratio of two rows' means and the spread it must pass: three SDs of the reference relative to its mean, never less than
    # VERDICT_MARGIN. names: the words for a ratio above the spread, below it, and within it.
    ratio = new['wall_seconds_mean'] / ref['wall_seconds_mean']
    spread = max(VERDICT_MARGIN, VERDICT_SD_FACTOR * (ref['wall_seconds_sample_sd'] or 0) / ref['wall_seconds_mean'])
    return ratio, spread, names[0] if ratio > 1 + spread else names[1] if ratio < 1 - spread else names[2]


def judge_row(new, ref):
    ratio, spread, verdict = compare(new, ref, ('slower', 'faster', 'same'))
    return dict(workload=new['workload'], engine=new['engine'], row=new['row'], repetitions=new['counted_repetitions'],
                mean=new['wall_seconds_mean'], reference_mean=ref['wall_seconds_mean'], reference_sd=ref['wall_seconds_sample_sd'],
                ratio=ratio, spread=spread, verdict=verdict)


def judge_against(folder):
    # W830: each workload's and row's xppautX row (its fastest engine) against the same row of each other program's fastest engine,
    # both from this attempt (the other programs' rows are W617's own, never rerun). ratio above 1 is xppautX slower: 'behind' when
    # past the spread, 'ahead' below it, else 'level'. Reported, never a stop: a gap that grew since W617 is a slower verdict.
    mine = fastest_by_program(counted_rows(folder, True))
    out = []
    for (workload, row, program), theirs in sorted(fastest_by_program(counted_rows(folder, False)).items()):
        ours = mine.get((workload, row, 'xppautx'))
        if ours is None:
            continue
        ratio, spread, verdict = compare(ours, theirs, ('behind', 'ahead', 'level'))
        out.append(dict(workload=workload, row=row, engine=ours['engine'], mean=ours['wall_seconds_mean'], program=program,
                        other_engine=theirs['engine'], other_mean=theirs['wall_seconds_mean'], other_sd=theirs['wall_seconds_sample_sd'],
                        ratio=ratio, spread=spread, verdict=verdict))
    return out


def judge(folder, reference):
    # The unit's verdict: slower when any row is, else faster when any is, else same; 'no reference' when the reference
    # attempt (None when there is none) has no counted xppautX row this attempt also has. `against`: judge_against's rows;
    # `behind`: how many of them are behind another program (the verdict does not change for them).
    new, ref = timed_rows(folder), timed_rows(reference)
    rows = [judge_row(new[key], ref[key]) for key in sorted(new) if key in ref]
    against = judge_against(folder)
    behind = sum(1 for r in against if r['verdict'] == 'behind')
    if not rows:
        return dict(verdict='no reference', rows=[], against=against, behind=behind)
    kinds = {r['verdict'] for r in rows}
    return dict(verdict='slower' if 'slower' in kinds else 'faster' if 'faster' in kinds else 'same', rows=rows, against=against,
                behind=behind)


# W802: no row of XPPAUT, AUTO-07p or Python is run again (their programs ran in W617; the maintainer, 2026-11-17). A plan's entry names
# its program first (a row's label, or "row " and the label): this picks the entries that are not xppautX's own.
FOREIGN_ENTRY = re.compile(r'(row )?(xppaut|auto07p|python)\b')
FROM_SCRATCH = ('large-stiff', 'server')  # the studies with no earlier attempt: W378's kit and W295's own series


def write_plan(a, name, would_run, imported):
    # would_run: one reason per row (or step) the earlier attempt does not supply; imported: how many it does.
    for why in would_run:
        progress(a, 'plan: %s: runs: %s' % (name, why))
    progress(a, 'plan: %s: %d imported, %d would run' % (name, imported, len(would_run)))
    # a workload planned in two modes (continuation's T2: its pilots, then its sized run) is one plan
    earlier = json.loads((a.out / PLAN).read_text()) if (a.out / PLAN).exists() else dict(would_run=[], imported=0)
    save(a.out / PLAN, dict(workload=name, would_run=earlier['would_run'] + would_run, imported=earlier['imported'] + imported))
