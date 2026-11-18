"""One worker process of the server study (W295, #353): its share of one model's parameter grid, run by one arm, one run after another.

    server_worker.py JOB.json

The coordinator (server.py) starts n of these at once, each pinned to its CPU, and times the whole sweep from outside; this file times
what only the worker sees: its start-up and each run. The arms:

  xppautx_server       one `xppautX --server` process for the worker's runs, driven by python/xppautx's Client: value.set_parameters,
                       run.initial, then the rows through data.page (the only way the data leaves the process, so it is in the run)
  xppautx_silent       one `xppautX --silent` process per run, on a copy of the model whose parameter line holds the value (the
                       command line has no parameter), the rows its output.dat
  export_python, export_python_numpy, export_python_numba
                       the script `xppautX --convert --to python | python_numpy | python_numba` wrote, imported as a module in this
                       process, its module-level parameter set, run() writing its CSV
  scipy                solve_ivp, RK45, at each value the tolerance the pilot matched to RK4's error there, over the python script's own rhs

A run's time is from the value to the rows being delivered (a file written, or the rows in the client). What reads the rows back (the
last row, or the whole table for the agreement pass) comes after the clock stops. Not a check: this file decides nothing; it reports.
"""
import csv
import importlib.util
import inspect
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

START = time.perf_counter()
REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'python'))  # the Client (W763)
sys.path.insert(0, str(REPO / 'tools'))  # exportcompare: the one comparison of rows

from exportcompare import first_difference  # noqa: E402

# the most rows one data.page request returns (docs/protocol.md: data.page's count, at most 2000)
PAGE_ROWS = 2000
# the most columns one data.page request returns (at most 500: a model with more is a shape the agreement pass refuses, never a cut table)
PAGE_COLUMNS = 500
# DOP853 at this tolerance is the accuracy reference of the pilot (tools/python-integration/scipy_integrate.py's, many orders below
# the errors compared with it)
REFERENCE_TOLERANCE = 1e-12
# the scipy RK45 tolerances the pilot tries, loosest first: the first whose error against the reference is no larger than RK4's is used
RTOL_LADDER = [10.0 ** -k for k in range(3, 13)]
# the agreement pass's slack on the triangle bound |scipy - xppautX| <= scipy's error + RK4's error (both against the reference): the
# rounding of the differences themselves, exportcompare's absolute
SLACK = 1e-12


def parameter_text(text, name, value):
    """The model's text with the parameter's `par` value replaced by `value` (written so it reads back exactly)."""
    pattern = re.compile(r'(?<![\w.])' + re.escape(name) + r'(\s*=\s*)[^,\s]+')
    lines, changed = text.split('\n'), 0
    for i, line in enumerate(lines):
        if re.match(r'\s*par\b', line):
            lines[i], n = pattern.subn(lambda m: name + m.group(1) + repr(float(value)), line)
            changed += n
    if changed != 1:
        raise ValueError('the model has %d "par" values named %s, not one' % (changed, name))
    return '\n'.join(lines)


def read_table(path):
    """output.dat's rows as numbers"""
    return [[float(v) for v in line.split()] for line in Path(path).read_text().splitlines() if line.strip()]


def read_csv(path):
    """a script's CSV: (the columns' names, its rows as numbers)"""
    with open(path, newline='') as f:
        rows = list(csv.reader(f))
    return rows[0], [[float(v) for v in r] for r in rows[1:]]


def import_script(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def times_of(module):
    """the stored times of the script's run: its steps' accumulated t, as the script and xppautX make them"""
    if module.STORE_EVERY != 1 or module.START_TIME != 0.0 or module.STORE_FROM != 0.0:
        raise ValueError('the study stores every step from time 0')
    steps = int((module.TOTAL_TIME + 0.1 * abs(module.DT)) / abs(module.DT))
    times, t = [module.START_TIME], module.START_TIME
    for _ in range(steps):
        t = t + module.DT
        times.append(t)
    return times


def with_values(function, values):
    """the script's function with the run's values as its `_p` argument, when it takes one (a function takes `_p` only when it reads a
    parameter or a derived value: W770)"""
    if '_p' not in inspect.signature(function).parameters:
        return function
    return lambda *args: function(*args, values)


def scipy_rows(module, times, rtol, method='RK45'):
    """the script's rhs under solve_ivp at tolerance rtol (relative and absolute): rows as the script stores them (t, states, outputs)"""
    from scipy.integrate import solve_ivp
    values = module.parameters()
    rhs, outputs = with_values(module.rhs, values), with_values(module.outputs, values)
    solution = solve_ivp(rhs, (times[0], times[-1]), list(module.INITIAL), method=method, t_eval=times, rtol=rtol, atol=rtol)
    if not solution.success:
        raise RuntimeError('solve_ivp %s: %s' % (method, solution.message))
    return [[t] + list(y) + list(outputs(t, y)) for t, y in zip(solution.t, solution.y.T)]

def write_csv(path, header, rows):
    with open(path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)


class Arm:
    """What one arm needs: set up once (`start`), one run at a value (`run`: the time-critical part; it leaves its rows' place in
    `self.rows`), and `rows_after` reading them back (outside the clock), `stop`."""

    def __init__(self, job, folder):
        self.job, self.folder = job, folder
        self.inputs = Path(job['inputs'])
        self.model = job['model']  # the .odex's file name

    def start(self):
        pass

    def stop(self):
        pass


class Server(Arm):
    def start(self):
        from xppautx import Client
        self.client = Client(str(self.inputs / self.model), binary=self.job['binary'], cwd=str(self.folder), scratch=True)
        self.client.start_events()

    def run(self, i, value):
        self.client.command('value.set_parameters', values={self.job['parameter']: value})
        self.client.command('run.initial')
        started = time.perf_counter()
        self.rows, first = [], 0
        while True:
            page = self.client.command('data.page', from_row=first, count=PAGE_ROWS, columns=PAGE_COLUMNS)
            self.rows += page['rows']
            first += len(page['rows'])
            if first >= page['total_rows']:
                break
        self.fetch_seconds = time.perf_counter() - started

    def rows_after(self, i):
        return self.rows

    def stop(self):
        self.client.close()


class Silent(Arm):
    def run(self, i, value):
        self.run_folder = self.folder / ('run%d' % i)
        self.run_folder.mkdir()
        (self.run_folder / self.model).write_text(parameter_text((self.inputs / self.model).read_text(), self.job['parameter'], value))
        p = subprocess.run([self.job['binary'], self.model, '--silent'], cwd=self.run_folder, capture_output=True, text=True)
        if p.returncode:
            raise RuntimeError('xppautX --silent exit %d: %s' % (p.returncode, p.stderr[-500:]))

    def rows_after(self, i):
        return read_table(self.run_folder / 'output.dat')

    def forget(self, i):
        if not self.job['keep_rows']:  # the agreement pass leaves its rows as the others' reference
            shutil.rmtree(self.run_folder)


class Script(Arm):
    """An exported script: the python one's module is also the scipy arm's"""
    language = 'python'

    def start(self):
        self.script = self.inputs / self.language / (Path(self.model).stem + '.py')
        self.module = import_script(self.script, 'exported_' + self.language)
        self.output = self.folder / 'out.csv'
        self.module.OUTPUT = str(self.output)

    def set_value(self, value):
        name = self.job['parameter']
        if not hasattr(self.module, name):
            raise ValueError('%s has no module-level parameter %s' % (self.script, name))
        setattr(self.module, name, value)

    def run(self, i, value):
        self.set_value(value)
        self.module.run()

    def rows_after(self, i):
        return read_csv(self.output)[1]


class Numba(Script):
    """The script's numba compile is in its first run's time, as the start-up of the other arms is in theirs: the parameter is read
    when run() starts, so the later runs need no compile of their own."""
    language = 'python_numba'


class Numpy(Script):
    language = 'python_numpy'


class Scipy(Script):
    def start(self):
        super().start()
        import scipy.integrate  # noqa: F401  (the import is the start-up's, not the first run's)
        self.times = times_of(self.module)
        self.header = list(self.module.COLUMNS)

    def run(self, i, value):
        self.set_value(value)
        write_csv(self.output, self.header, scipy_rows(self.module, self.times, self.job['rtols'][i]))


ARMS = dict(xppautx_server=Server, xppautx_silent=Silent, export_python=Script, export_python_numpy=Numpy, export_python_numba=Numba, scipy=Scipy)


def compare(job, i, rows):
    """The agreement pass: this run's rows against xppautX's (the silent run's output.dat of the same grid index): (where, largest)"""
    reference = read_table(Path(job['reference']) / ('run%d' % i) / 'output.dat')
    if len(reference) != len(rows) or any(len(a) != len(b) for a, b in zip(reference, rows)):
        return 'the rows are %d x %d, xppautX\'s %d x %d' % (len(rows), len(rows[0]) if rows else 0, len(reference), len(reference[0])), None
    if job['arm'] == 'scipy':  # its rows are the pilot's: within the triangle bound of the two errors the pilot measured at this value
        return first_difference(reference, rows, relative=0.0, absolute=job['bounds'][i] + SLACK, other='scipy')
    return first_difference(reference, rows, other=job['arm'])


def pilot(job):
    """The scipy tolerance that matches RK4's error at every value of the grid (the arm scipy's input; not timed), so the two are
    timed at equal accuracy everywhere (W814, #895: one tolerance from the middle value made scipy do more work than RK4 where RK4's
    error is larger, lecdiff200's near its threshold 47 times, and less where it is smaller): at each value the loosest RTOL_LADDER
    entry whose RK45 rows are no further from DOP853's reference than xppautX's RK4 rows are (a tolerance whose solve fails does not
    count). The rows' columns are compared whole, every state and output, by their largest absolute difference."""
    module = import_script(Path(job['inputs']) / 'python' / (Path(job['model']).stem + '.py'), 'exported_pilot')
    module.OUTPUT = os.devnull
    times = times_of(module)
    return dict(points=[matched(job, module, times, i, value) for i, value in zip(job['indices'], job['values'])],
                reference_tolerance=REFERENCE_TOLERANCE)


def matched(job, module, times, i, value):
    """pilot's tolerance at the grid's value i"""
    setattr(module, job['parameter'], value)
    reference = scipy_rows(module, times, REFERENCE_TOLERANCE, method='DOP853')
    rk4 = read_table(Path(job['reference']) / ('run%d' % i) / 'output.dat')

    def error(rows):
        return max(abs(x - y) for a, b in zip(rows, reference) for x, y in zip(a, b))
    rk4_error = error(rk4)
    ladder = []
    for rtol in RTOL_LADDER:
        try:
            e = error(scipy_rows(module, times, rtol))
        except (ArithmeticError, RuntimeError) as failure:
            e = math.inf  # a trial step of a loose tolerance can leave the range of cosh: that rung did not reach RK4's error
            ladder.append(dict(rtol=rtol, error=str(failure)))
            continue
        ladder.append(dict(rtol=rtol, error=e))
        if e <= rk4_error:
            return dict(index=i, value=value, rtol=rtol, rk4_error=rk4_error, scipy_error=e, ladder=ladder)
    raise RuntimeError('value %d (%g): RK45 never reaches RK4\'s error %g (ladder %s)' % (i, value, rk4_error, ladder))


def main():
    job_path = Path(sys.argv[1])
    job = json.loads(job_path.read_text())
    folder = job_path.parent
    if job['arm'] == 'pilot':
        (folder / 'result.json').write_text(json.dumps(pilot(job)))
        return
    arm = ARMS[job['arm']](job, folder)
    arm.start()
    startup = time.perf_counter() - START  # the imports, the process's load or the script's import: everything before the first value
    runs = []
    try:
        for i, value in zip(job['indices'], job['values']):
            arm.fetch_seconds = None
            started = time.perf_counter()
            arm.run(i, value)
            seconds = time.perf_counter() - started
            rows = arm.rows_after(i)
            record = dict(index=i, value=value, seconds=seconds, fetch_seconds=arm.fetch_seconds, rows=len(rows), last_row=rows[-1],
                          finite=all(math.isfinite(v) for r in rows for v in r))
            if job['keep_rows']:
                record['where'], record['largest'] = compare(job, i, rows) if job.get('reference') else ('', None)
            runs.append(record)
            if hasattr(arm, 'forget'):
                arm.forget(i)
    finally:
        arm.stop()
    (folder / 'result.json').write_text(json.dumps(dict(arm=job['arm'], startup_seconds=startup, runs=runs), allow_nan=False))


if __name__ == '__main__':
    main()
