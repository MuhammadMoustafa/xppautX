#!/usr/bin/env python3
"""tools/python-integration/numerics.py: the numerics of a run, as xppautX has them (W427, #489).

A run's options are the model's options block over xppautX's defaults (an XPPAUT set file is converted
into the model's .odex once, `xppautX --convert model.ode --statefile FILE`). The defaults are read from
docs/odex-options.schema.json, which `xppautX --options-schema` writes from the option table (W804): each
numeric option's `default` and its `x-key` (the key of the numerics event), the method's table.
"""

from pathlib import Path
import json
from types import SimpleNamespace

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCHEMA = REPO_ROOT / 'docs' / 'odex-options.schema.json'
WHOLE = ('store_every', 'eul_iter')


class Unreadable(Exception):
    """A file of xppautX's that the Python side reads is not as it expects: the message says which."""


def schema():
    try:
        return json.loads(SCHEMA.read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        raise Unreadable('%s: %s' % (SCHEMA.name, e))


def option_rows():
    """Each option of the schema that has a numerics key: (its .odex name, its key, its schema property)."""
    return [(name, prop['x-key'], prop)
            for group in schema()['properties'].values()
            for name, prop in (group.get('properties') or {}).items() if 'x-key' in prop]


def numeric_defaults():
    """{key: default} of the numeric options and the method, as the schema gives them (the method's default is its key)."""
    out = {}
    for _, key, prop in option_rows():
        if prop.get('type') not in ('number', 'integer') and key != 'method':
            continue
        if 'default' not in prop:
            raise Unreadable('%s: the option %s has no default' % (SCHEMA.name, key))
        out[key] = prop['default']
    return out


def defaults():
    """The values a run starts from before the model's options: {field: value}, the method as its key."""
    values = {key: value if key == 'method' else float(value) for key, value in numeric_defaults().items()}
    return values


def method_table():
    """The method table as the schema lists it, in the set file's order: [{name, set_label, key, xppaut}]."""
    for _, key, prop in option_rows():
        if key == 'method':
            return prop['x-methods']
    raise Unreadable('%s: no method option' % SCHEMA.name)


def method_rows():
    """(name, set-file label, key) of each method, in the set file's order."""
    return [(m['name'], m['set_label'], m['key']) for m in method_table()]


def legacy_method_key(key):
    """XPPAUT's one-character @ meth key of a method key."""
    for m in method_table():
        if m['key'] == key:
            return m['xppaut']
    raise Unreadable('no method key ' + key)


def numerics_of(odex):
    """The run's numerics: xppautX's defaults, then the model's options."""
    values = defaults()
    keys_of = {name: key for name, key, _ in option_rows()}
    for name, text in odex.options.items():
        key = keys_of.get(name, name)
        if key == 'method':
            if text not in {m['key'] for m in method_table()}:
                raise Unreadable('the option method: %s is not a method' % text)
            values[key] = text
        elif key in values:
            values[key] = float(text)
    return whole_numbers(SimpleNamespace(**values))


def whole_numbers(numerics):
    for key in WHOLE:
        setattr(numerics, key, int(getattr(numerics, key)))
    return numerics
