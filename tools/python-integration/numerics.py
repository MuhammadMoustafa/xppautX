#!/usr/bin/env python3
"""tools/python-integration/numerics.py: the numerics of a run, as xppautX has them (W427, #489).

A run's settings are the model's options block over xppautX's defaults, and an XPPAUT set file
(`--setfile`) over both. The defaults are xppautX's own, read from core/ where they are written
(core/model_options.cpp's resets, core/load_eqn.h's NumericsSettings, core/solver.cpp's method
table), so the Python side has no default of its own.
"""

from pathlib import Path
import re
from types import SimpleNamespace

REPO_ROOT = Path(__file__).resolve().parent.parent.parent

# core/model_options.cpp writes an option's default in its row: `.reset = [](Session &s) { s.numerics.MEMBER = VALUE; }, ... .key = "KEY"`
RESET = re.compile(r'\.reset = \[\]\(Session &s\) \{ s\.numerics\.\w+ = ([-0-9.eE]+); \},[^{}]*?\.key = "(\w+)"')
# ... and the default method's, the name picked unless the model has integral equations
METHOD_RESET = re.compile(r'"Volterra" : "([^"]+)"')
# core/load_eqn.h: a NumericsSettings member that no option names
STOL = re.compile(r'double stol = ([-0-9.eE]+);')
# core/solver.cpp's registry rows: {method::ID,"name","set file label","key",...}
METHOD_ROW = re.compile(r'\{method::\w+,"([^"]+)","([^"]+)","(\w+)"')
# XPPAUT's set file labels, in its "Numerical stuff", of the options a run reads
SET_LABELS = {'nout': 'store_every', 'total': 'total_time', 'DeltaT': 'dt', 'T0': 'start_time', 'Transient': 'transient_time',
              'Bound': 'bound', 'DtMin': 'min_step', 'DtMax': 'max_step', 'Tolerance': 'tolerance',
              'Abs. Tolerance': 'abs_tolerance', 'Newton tolerance': 'singpt_jacobian_epsilon'}
WHOLE = ('store_every', 'eul_iter')


class Unreadable(Exception):
    """A file of xppautX's that the Python side reads is not as it expects: the message says which."""


def read_core(name):
    return (REPO_ROOT / 'core' / name).read_text(encoding='utf-8')


def method_rows():
    rows = METHOD_ROW.findall(read_core('solver.cpp'))
    if not rows:
        raise Unreadable('core/solver.cpp: no method rows found')
    return rows


def core_defaults():
    """The numerics xppautX starts every model with: {option key: value}, the method as its key."""
    options = read_core('model_options.cpp')
    defaults = {key: float(value) for value, key in RESET.findall(options)}
    default_method = METHOD_RESET.search(options)
    stol = STOL.search(read_core('load_eqn.h'))
    if not default_method or not stol:
        raise Unreadable('core/model_options.cpp or core/load_eqn.h: a default not found')
    defaults['method'] = next(key for name, _, key in method_rows() if name == default_method.group(1))
    defaults['stol'] = float(stol.group(1))
    return defaults


def numerics_of(odex):
    """The run's numerics: xppautX's defaults, then the model's options."""
    values = core_defaults()
    keys = {key for _, _, key in method_rows()}
    for key, text in odex.options.items():
        if key == 'method':
            if text not in keys:
                raise Unreadable('the option method: %s is not a method' % text)
            values[key] = text
        elif key in values:
            values[key] = float(text)
    return whole_numbers(SimpleNamespace(**values))


def whole_numbers(numerics):
    for key in WHOLE:
        setattr(numerics, key, int(getattr(numerics, key)))
    return numerics


def apply_xppaut_set(odex, numerics, path):
    """An XPPAUT set file over the model: its numerics, the initial values ("Old ICs") and the
    parameters, each line a value and then its label (core/lunch-new.cpp reads the same file)."""
    section, rows = None, {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        if line.startswith('##'):
            continue
        if line.startswith('#'):
            section = line.strip('# ').lower()
            rows[section] = []
        elif section and line.strip():
            value, _, label = line.strip().partition(' ')
            rows[section].append((value, label.strip()))
    methods = {label: key for _, label, key in method_rows()}
    for value, label in rows.get('numerical stuff', []):
        if label in methods:
            numerics.method = methods[label]
        elif label in SET_LABELS:
            setattr(numerics, SET_LABELS[label], float(value))
    numerics.total_time = abs(numerics.total_time)  # a negative Total is XPPAUT's, taken as its absolute value
    whole_numbers(numerics)
    names = {n.lower(): n for n in list(odex.states) + list(odex.pars)}
    if len(names) != len(odex.states) + len(odex.pars):
        raise Unreadable('%s: the model has names that differ only by case' % path)
    for section, target, kind in (('old ics', odex.inits, odex.states), ('parameters', odex.pars, odex.pars)):
        for value, label in rows.get(section, []):
            if names.get(label.lower()) not in kind:
                raise Unreadable('%s: %s is not in the model (%s)' % (path, label, section))
            target[names[label.lower()]] = float(value)
