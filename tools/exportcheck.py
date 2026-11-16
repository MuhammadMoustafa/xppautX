#!/usr/bin/env python3
"""tools/exportcheck.py (W764, #843): a model written as a Python program (xppautX --convert MODEL --to python, the model.export
command's core function) runs by itself and gives the rows xppautX gives.

Each model on the default method (Runge-Kutta, the program's classic RK4) is exported, the script run with this interpreter (the
repo's uv environment's, since the check runs under `uv run --locked python`) and its CSV compared, row by row and column by column,
with the output.dat `xppautX MODEL --silent` writes. The tolerance: xppautX stores each value as a float (6e-8 relative at most) and
its xpp::math functions are correctly rounded where Python's math calls the C library (at most an ulp each); over the runs here
the two stay within a few 1e-7, so 1e-6 relative (and 1e-9 absolute, for values near zero) fails what is wrong and nothing else.
Then the rest of the card: a model on another method says so in its header, a model that uses a name Python reserves has it renamed
(and listed in the header), a model with a table, a delay or a shift is refused with its line and no file, and --to takes only
the words it names.

usage: tools/exportcheck.py [--bin ./xppautX]
"""
import argparse
import ast
import csv
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xppclient import Checker  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--bin', default='./xppautX')
args = ap.parse_args()
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
binary = os.path.abspath(args.bin)
ODE = os.path.join(root, 'examples', 'ode')
MODELS = os.path.join(root, 'tools', 'models')
RELATIVE, ABSOLUTE = 1e-6, 1e-9
# a safety bound for a broken child, never a speed assertion
LIMIT = 300

check = Checker()


def run(command, cwd):
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=LIMIT)


def stage(source, text_edit=None):
    """a scratch folder holding the model (its text changed by text_edit); its path"""
    folder = tempfile.mkdtemp(prefix='exportcheck-')
    name = os.path.basename(source)
    with open(source, encoding='utf-8') as f:
        text = f.read()
    with open(os.path.join(folder, name), 'w', encoding='utf-8', newline='') as f:
        f.write(text_edit(text) if text_edit else text)
    return folder, name


def read_rows(path):
    with open(path, newline='') as f:
        return list(csv.reader(f))


def compare(label, source, text_edit=None):
    """exports the model (its text changed by text_edit), runs the script and compares; the scratch folder is the caller's to remove
    (it holds the script), or None when a step failed"""
    folder, name = stage(source, text_edit)
    base = os.path.splitext(name)[0]
    silent = run([binary, name, '--silent'], folder)
    convert = run([binary, '--convert', name, '--to', 'python'], folder)
    script = os.path.join(folder, base + '.py')
    check('%s: --convert --to python writes %s.py' % (label, base), convert.returncode == 0 and os.path.exists(script), convert.stderr[-300:])
    check('%s: the model runs in xppautX' % label, silent.returncode == 0, silent.stderr[-300:])
    ran = run([sys.executable, script], folder) if os.path.exists(script) else None
    csv_file = os.path.join(folder, base + '.csv')
    check('%s: the script runs and writes the CSV beside itself' % label, ran is not None and ran.returncode == 0 and os.path.exists(csv_file),
          ran.stderr[-300:] if ran else 'no script')
    if silent.returncode != 0 or ran is None or ran.returncode != 0 or not os.path.exists(csv_file):
        cleanup(folder)
        return None
    ours = [line.split() for line in open(os.path.join(folder, 'output.dat'), encoding='utf-8') if line.strip()]
    rows = read_rows(csv_file)
    header, theirs = rows[0], rows[1:]
    check('%s: the same number of rows (%d) and columns (%d)' % (label, len(ours), len(header)),
          len(ours) == len(theirs) and all(len(a) == len(b) == len(header) for a, b in zip(ours, theirs)),
          '%d rows vs %d, header %s' % (len(ours), len(theirs), header))
    worst, where = 0.0, ''
    for i, (a, b) in enumerate(zip(ours, theirs)):
        for j, (x, y) in enumerate(zip(a, b)):
            x, y = float(x), float(y)
            if math.isnan(x) and math.isnan(y):
                continue
            excess = abs(x - y) - (RELATIVE * max(abs(x), abs(y)) + ABSOLUTE)
            if excess > worst or (excess > 0 and not where):
                worst, where = max(worst, excess), 'row %d column %s: xppautX %r, script %r' % (i, header[j], x, y)
    check('%s: every value is within %g relative of xppautX\'s' % (label, RELATIVE), not where, where)
    return folder


def cleanup(*folders):
    for folder in folders:
        if folder:
            shutil.rmtree(folder, ignore_errors=True)


def header_of(folder, base):
    with open(os.path.join(folder, base + '.py'), encoding='utf-8') as f:
        lines = []
        for line in f:
            if not line.startswith('#'):
                break
            lines.append(line)
    return ''.join(lines)


def refused(label, source, text_edit, what, line):
    folder, name = stage(source, text_edit)
    try:
        base = os.path.splitext(name)[0]
        result = run([binary, '--convert', name, '--to', 'python'], folder)
        check('%s: refused, naming %s' % (label, what), result.returncode != 0 and what in result.stderr, result.stderr[-300:])
        check('%s: ... and the model line (%s:%d)' % (label, name, line) if line else '%s: ... and a model line' % label,
              ('%s:%d:' % (name, line) in result.stderr) if line else (name + ':') in result.stderr, result.stderr[-300:])
        check('%s: ... and no file is written' % label, not os.path.exists(os.path.join(folder, base + '.py')), str(os.listdir(folder)))
    finally:
        cleanup(folder)


# every model that runs on Runge-Kutta: a user function, a fixed variable and aux columns (lecar), a plain flow (vanderpol,
# lorenz), a name Python reserves (lorenz2's `del`), user functions that call each other with conditionals, and a derived parameter
published = ['lecar.odex', 'vanderpol.odex', 'lorenz.odex', 'lorenz2.odex']
for model in published:
    cleanup(compare(model, os.path.join(ODE, model)))
cleanup(compare('compile_inline.odex', os.path.join(MODELS, 'compile_inline.odex'), lambda t: t + 'options { integration: { total_time: 1 } }\n'))
cleanup(compare('compile_derived.odex', os.path.join(MODELS, 'compile_derived.odex')))
# a sum over an index (a loop in the script) on Runge-Kutta, as the model's own n and steps
cleanup(compare('heavy.odex (a sum, n = 20, Runge-Kutta)', os.path.join(MODELS, 'heavy.odex'),
                lambda t: t.replace('n = 2000', 'n = 20').replace('method: stiff', 'method: runge_kutta').replace('total_time: 20, dt: 0.01', 'total_time: 0.2, dt: 0.0002')))

# the renames, listed in the header, the script still valid Python
folder, name = stage(os.path.join(ODE, 'lorenz2.odex'))
try:
    run([binary, '--convert', name, '--to', 'python'], folder)
    header = header_of(folder, 'lorenz2')
    check('lorenz2: the header lists the rename del -> del_', 'del -> del_' in header, header)
    check('lorenz2: the CSV header keeps the model\'s own names', 'del' not in open(os.path.join(folder, 'lorenz2.py'), encoding='utf-8').read().split('COLUMNS = ')[1].split('\n')[0].lower().split(','), '')
finally:
    cleanup(folder)
# a model whose names are ones the script itself uses (a function, a module, a name of its constants) has them renamed, and runs
renamed = compare('lecar with names the script uses', os.path.join(ODE, 'lecar.odex'),
                  lambda t: re.sub(r'\bgl\b', 'rhs', re.sub(r'\bphi\b', 'csv', re.sub(r'\biapp\b', 'DT', t))))
try:
    header = header_of(renamed, 'lecar') if renamed else ''
    check('lecar with names the script uses: the header lists rhs -> rhs_, csv -> csv_ and DT -> DT_',
          all(word in header for word in ('rhs -> rhs_', 'csv -> csv_', 'DT -> DT_')), header)
finally:
    cleanup(renamed)

# another method: the header says so, and the script runs
folder, name = stage(os.path.join(ODE, 'atcoaster.odex'))
try:
    run([binary, '--convert', name, '--to', 'python'], folder)
    check('atcoaster (CVode): the header names the model\'s own method and says the numbers differ',
          "The model's own method is CVode" in header_of(folder, 'atcoaster') and 'differ' in header_of(folder, 'atcoaster'), header_of(folder, 'atcoaster'))
finally:
    cleanup(folder)
folder, name = stage(os.path.join(ODE, 'lecar.odex'))
try:
    run([binary, '--convert', name, '--to', 'python'], folder)
    check('lecar (Runge-Kutta): the header has no line about another method', 'own method' not in header_of(folder, 'lecar'), header_of(folder, 'lecar'))
finally:
    cleanup(folder)

# what Python cannot say is refused, with the model line and no file
refused('delay.odex', os.path.join(ODE, 'delay.odex'), None, 'delay()', 10)
refused('a lookup table', os.path.join(ODE, 'lecar.odex'),
        lambda t: t.replace('aux rate = phi', 'aux rate = phi\ntable g sin(t), n=11, lo=0, hi=10\naux gv = g(v)'), 'a lookup table', 22)
refused('kuramot100.odex', os.path.join(ODE, 'kuramot100.odex'), None, 'shift()', 10)
refused('mackey.odex', os.path.join(root, 'examples', 'canonical', 'mackey.odex'), None, 'delay()', 6)
refused('a model with events', os.path.join(ODE, 'izh_2003_rs.odex'), None, 'events', 19)
refused('a map', os.path.join(ODE, 'izh_2004q_depolarizing_afterpotential.odex'), None, 'map', 0)

# --to: the default, the other word, and a word that is neither
folder, name = stage(os.path.join(ODE, 'lecar.odex'))
try:
    unknown = run([binary, '--convert', name, '--to', 'julia'], folder)
    check('--to julia is an error listing the values', unknown.returncode != 0 and 'one of odex, python' in unknown.stderr, unknown.stderr)
    check('--to without --convert is an error', run([binary, name, '--to', 'python'], folder).returncode != 0, '')
    both = run([binary, '--convert', name, '--to', 'python', '--statefile', 'x.set'], folder)
    check('--to python does not go with --statefile', both.returncode != 0 and 'statefile' in both.stderr, both.stderr)
    check('nothing was written by those', sorted(os.listdir(folder)) == [name], str(os.listdir(folder)))
finally:
    cleanup(folder)
# --to odex is today's conversion, and what --convert does with no --to: both write the same .odex
folder, name = stage(os.path.join(ODE, 'lecar.ode'))
try:
    run([binary, '--convert', '--yes', name], folder)
    plain = open(os.path.join(folder, 'lecar.odex'), encoding='utf-8').read() if os.path.exists(os.path.join(folder, 'lecar.odex')) else None
    os.remove(os.path.join(folder, 'lecar.odex')) if plain is not None else None
    run([binary, '--convert', '--yes', name, '--to', 'odex'], folder)
    named = open(os.path.join(folder, 'lecar.odex'), encoding='utf-8').read() if os.path.exists(os.path.join(folder, 'lecar.odex')) else None
    check('--convert and --convert --to odex write the same .odex', plain is not None and plain == named, '')
    run([binary, '--convert', '--yes', name, '--to', 'python'], folder)
    check('--convert MODEL.ode --to python writes the .py beside it', os.path.exists(os.path.join(folder, 'lecar.py')), str(os.listdir(folder)))
finally:
    cleanup(folder)

print('%d checks, %d failed' % (check.checks, check.failures))
sys.exit(1 if check.failures else 0)
