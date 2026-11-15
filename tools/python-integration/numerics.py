#!/usr/bin/env python3
"""tools/python-integration/numerics.py: the numerics of a run, as xppautX has them (W427, #489).

A run's settings are the model's options block over xppautX's defaults (an XPPAUT set file is converted
into the model's .odex once, `xppautX --convert model.ode --statefile FILE`). The defaults are xppautX's own, read from core/ where they are written
(core/model_options.cpp's resets, the NumericsSettings struct, the method table and XPPAUT's method keys, found wherever
core/ writes them), so the Python side has no default of its own.
"""

from pathlib import Path
import re
from types import SimpleNamespace

REPO_ROOT = Path(__file__).resolve().parent.parent.parent

# core/model_options.cpp writes an option's default in its row: `.reset = [](const SessionParts &s) { s.numerics->MEMBER = VALUE; }, ... .key = "KEY"`
RESET = re.compile(r'\.reset = \[\]\((?:const )?\w+ &s\) \{ s\.numerics(?:->|\.)[\w.]+ = ([-0-9.eE]+); \},[^{}]*?\.key = "(\w+)"')
# ... and the name a row has in a .odex (`step.min`), which its key (`min_step`) is not: the row's `.odex = "NAME"` and then its `.key = "KEY"`
ODEX_NAME = re.compile(r'\.odex = "([\w.]+)"(?:(?!\.odex = ).)*?\.key = "(\w+)"', re.S)
# ... and the default method's, the name picked unless the model has integral equations
METHOD_RESET = re.compile(r'"Volterra" : "([^"]+)"')
# the NumericsSettings struct: a member that no option names
STOL = re.compile(r'double stol = ([-0-9.eE]+);')
# the method table's rows: {method::ID,"name","set file label","key",...}
METHOD_ROW = re.compile(r'\{method::\w+,"([^"]+)","([^"]+)","(\w+)"')
LEGACY_METHOD_KEYS = re.compile(r'constexpr std::string_view keys = "([^"]+)"')
WHOLE = ('store_every', 'eul_iter')


class Unreadable(Exception):
    """A file of xppautX's that the Python side reads is not as it expects: the message says which."""


def read_core(name):
    return (REPO_ROOT / 'core' / name).read_text(encoding='utf-8')


def find_in_core(pattern, what):
    """The one file of core/ whose text `pattern` matches, and its matches: a definition is read where core/ writes it, so a
    move between files (W744 moved the method table, W757 changed the option rows) never leaves this reading nothing."""
    found = [(f.name, pattern.findall(f.read_text(encoding='utf-8'))) for f in sorted((REPO_ROOT / 'core').glob('*.[ch]*'))]
    found = [(name, matches) for name, matches in found if matches]
    if len(found) != 1:
        raise Unreadable('core/: %s found in %s' % (what, ', '.join(name for name, _ in found) or 'no file'))
    return found[0]


def method_rows():
    return find_in_core(METHOD_ROW, 'the method table')[1]


def legacy_method_key(key):
    """XPPAUT's one-character method key, from the same core registry reader."""
    keys = find_in_core(LEGACY_METHOD_KEYS, "XPPAUT's method keys")[1]
    rows = method_rows()
    if len(keys) != 1 or len(keys[0]) != len(rows):
        raise Unreadable("core/: XPPAUT's method keys do not match the method table")
    for i, (_, _, candidate) in enumerate(rows):
        if key == candidate:
            return keys[0][i]
    raise Unreadable('core/: no method key ' + key)


def core_defaults():
    """The numerics xppautX starts every model with: {option key: value}, the method as its key."""
    options = read_core('model_options.cpp')
    defaults = {key: float(value) for value, key in RESET.findall(options)}
    default_method = METHOD_RESET.search(options)
    if not defaults or not default_method:
        raise Unreadable("core/model_options.cpp: the option rows' defaults not found")
    stol = find_in_core(STOL, 'the stol default')[1]
    defaults['method'] = next(key for name, _, key in method_rows() if name == default_method.group(1))
    defaults['stol'] = float(stol[0])
    return defaults


def numerics_of(odex):
    """The run's numerics: xppautX's defaults, then the model's options."""
    values = core_defaults()
    keys = {key for _, _, key in method_rows()}
    keys_of = {name: key for name, key in ODEX_NAME.findall(read_core('model_options.cpp'))}
    for name, text in odex.options.items():
        key = keys_of.get(name, name)
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
