#!/usr/bin/env python3
"""tools/exportcheck.py (W764, #843; W766, #846): a model written as a Python program (xppautX --convert MODEL --to python, the model.export
command's core function) runs by itself and gives the rows xppautX gives, in each of the three targets: python (the standard library),
python_numpy (the state a numpy array, an array line of the model one slice) and python_numba (python's scalar statements compiled by numba's
@njit).

Each model on the default method (Runge-Kutta, the program's classic RK4) is exported, the script run with this interpreter (the
repo's uv environment's, since the check runs under `uv run --locked python`, which holds numpy and numba) and its CSV compared, row by row and column by column,
with the output.dat `xppautX MODEL --silent` writes, xppautX keeping doubles (storage_precision: double, so the comparison
sees the integration, not the float a data table keeps by default). A model of arithmetic alone gives xppautX's numbers bit for
bit, since the script steps RK4 in xppautX's own order and Python, like xppautX, fuses no multiply-add: it is compared exactly.
A model that calls a function (exp, tanh, ...) may differ by xpp::math's correctly rounded result against the C library's, an
ulp each, which a run grows to a few 1e-11 (lecar): 1e-9 relative (and 1e-12 absolute, for values near zero) fails what is wrong
and nothing else.
The published array models of examples/ode that the export does not refuse (lecdiff200, wave, lamprey, lecsimp2, itoy, toy_ok) run in all three targets
on Runge-Kutta, and python_numpy must have written the array lines of the first three as slices (their runs are named in its header).
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
RELATIVE, ABSOLUTE = 1e-9, 1e-12
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


def with_double_storage(text):
    """the model's text with storage_precision: double in its options' integration group"""
    i = text.find('options {')
    if i < 0:
        return text + 'options { integration: { storage_precision: double } }\n'
    group = re.compile(r'integration:\s*\{').search(text, i)
    at = group.end() if group else i + len('options {')
    return text[:at] + (' storage_precision: double,' if group else ' integration: { storage_precision: double },') + text[at:]


LANGUAGES = ('python', 'python_numpy', 'python_numba')


def on_runge_kutta(total_time=None):
    """a text edit: the model on the method the script integrates with, run for total_time when given (a chaotic model's last rows
    would grow the ulp that the C library's functions differ by to the tolerance)"""
    def edit(text):
        if re.search(r'\bmethod:\s*\w+', text):
            text = re.sub(r'\bmethod:\s*\w+', 'method: runge_kutta', text, count=1)
        else:
            text = text.replace('integration: {', 'integration: { method: runge_kutta,', 1)
        return re.sub(r'\btotal_time:\s*[\d.]+', 'total_time: %g' % total_time, text, count=1) if total_time else text
    return edit


def compare(label, source, text_edit=None, exact=False, language='python'):
    """exports the model (its text changed by text_edit) in the language, runs the script and compares; the scratch folder is the caller's
    to remove (it holds the script), or None when a step failed"""
    label = label if language == 'python' else '%s [%s]' % (label, language)
    folder, name = stage(source, lambda text: with_double_storage(text_edit(text) if text_edit else text))
    base = os.path.splitext(name)[0]
    silent = run([binary, name, '--silent'], folder)
    convert = run([binary, '--convert', name, '--to', language], folder)
    script = os.path.join(folder, base + '.py')
    check('%s: --convert --to %s writes %s.py' % (label, language, base), convert.returncode == 0 and os.path.exists(script), convert.stderr[-300:])
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
    worst, where, largest = 0.0, '', 0.0
    for i, (a, b) in enumerate(zip(ours, theirs)):
        for j, (x, y) in enumerate(zip(a, b)):
            x, y = float(x), float(y)
            if math.isnan(x) and math.isnan(y):
                continue
            largest = max(largest, abs(x - y) / max(abs(x), abs(y), 1e-300))
            excess = (x != y) if exact else abs(x - y) - (RELATIVE * max(abs(x), abs(y)) + ABSOLUTE)
            if excess > worst or (excess > 0 and not where):
                worst, where = max(worst, excess), 'row %d column %s: xppautX %r, script %r' % (i, header[j], x, y)
    check('%s: every value %s xppautX\'s' % (label, 'equals' if exact else 'is within %g relative of' % RELATIVE), not where, where)
    print('info: %s: largest relative difference %.3g' % (label, largest))
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
# each with whether it is arithmetic alone (compared exactly)
published = {'lecar.odex': False, 'vanderpol.odex': True, 'lorenz.odex': True, 'lorenz2.odex': True}
for language in LANGUAGES:
    for model, exact in published.items():
        cleanup(compare(model, os.path.join(ODE, model), exact=exact, language=language))
    cleanup(compare('compile_inline.odex', os.path.join(MODELS, 'compile_inline.odex'), lambda t: t + 'options { integration: { total_time: 1 } }\n', language=language))
    cleanup(compare('compile_derived.odex', os.path.join(MODELS, 'compile_derived.odex'), language=language))
    # a sum over an index (a loop in the script) on Runge-Kutta, as the model's own n and steps
    cleanup(compare('heavy.odex (a sum, n = 20, Runge-Kutta)', os.path.join(MODELS, 'heavy.odex'),
                    lambda t: t.replace('n = 2000', 'n = 20').replace('method: stiff', 'method: runge_kutta').replace('total_time: 20, dt: 0.01', 'total_time: 0.2, dt: 0.0002'),
                    language=language))

# the published array models, on Runge-Kutta: the array lines are slices in python_numpy, and the numbers are the same in all three
SLICED = {'lecdiff200.odex': ('V2..V199', 'W1..W200'), 'wave.odex': (), 'lamprey.odex': ()}
for language in LANGUAGES:
    for model, total_time in (('lecdiff200.odex', None), ('wave.odex', None), ('lamprey.odex', None), ('lecsimp2.odex', None), ('itoy.odex', 10), ('toy_ok.odex', 30)):
        folder = compare(model, os.path.join(ODE, model), on_runge_kutta(total_time), language=language)
        try:
            if folder and language == 'python_numpy' and model in SLICED:
                header = header_of(folder, os.path.splitext(model)[0])
                run_names = SLICED[model]
                check('%s [python_numpy]: its array lines are written as slices (the header names the runs%s)' % (model, ': ' + ', '.join(run_names) if run_names else ''),
                      'Equations written as slices' in header and all(name in header for name in run_names), header)
                script = open(os.path.join(folder, os.path.splitext(model)[0] + '.py'), encoding='utf-8').read()
                check('%s [python_numpy]: ... and its right-hand side holds fewer than 20 derivatives, not one for each copy' % model,
                      len(re.findall(r'^    d\d+ = ', script, re.M)) < 20, str(len(re.findall(r'^    d\d+ = ', script, re.M))))
        finally:
            cleanup(folder)

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

# the numpy and numba scripts import np and njit: a model's names that are those get renamed there, and in python's script they stay
for language, listed in (('python', False), ('python_numpy', True), ('python_numba', True)):
    renamed = compare('lecar with np and njit as names', os.path.join(ODE, 'lecar.odex'),
                      lambda t: re.sub(r'\bgl\b', 'np', re.sub(r'\bphi\b', 'njit', t)), language=language)
    try:
        header = header_of(renamed, 'lecar') if renamed else ''
        check('lecar with np and njit as names [%s]: the header %s np -> np_ and njit -> njit_' % (language, 'lists' if listed else 'has no'),
              ('np -> np_' in header and 'njit -> njit_' in header) == listed, header)
    finally:
        cleanup(renamed)
# what each target imports and compiles
for language, imports, compiled in (('python', False, False), ('python_numpy', True, False), ('python_numba', True, True)):
    folder, name = stage(os.path.join(ODE, 'lecar.odex'))
    try:
        run([binary, '--convert', name, '--to', language], folder)
        script = open(os.path.join(folder, 'lecar.py'), encoding='utf-8').read()
        check('lecar [%s]: %s numpy' % (language, 'imports' if imports else 'does not import'), ('import numpy as np' in script) == imports, script[:600])
        check('lecar [%s]: %s' % (language, 'rhs, outputs, step and the helpers are @njit, with no fastmath' if compiled else 'nothing is compiled'),
              (script.count('@njit\ndef ') >= 3 and 'fastmath' not in script.split('\ndef ', 1)[1] and 'from numba import njit' in script) if compiled else ('numba' not in script), script[:600])
    finally:
        cleanup(folder)

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
    check('--to julia is an error listing the values', unknown.returncode != 0 and 'one of odex, python, python_numpy, python_numba' in unknown.stderr, unknown.stderr)
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
