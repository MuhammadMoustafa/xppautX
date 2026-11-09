#!/usr/bin/env python3
"""tools/interpreted_copy.py [--convert BIN] SOURCE DEST: write DEST as a copy of the model SOURCE
whose equations run in the interpreter: its options set `compile: false` (W661, #734).

The option is the model's own (an `integration` key of its top-level `options` block), so the copy
is a model file like any other: no switch, no environment variable, no edit of the model in the
repository. A model with no `options` block gets one at its end; one whose block has no `integration`
group gets it as the block's first item; one that has it gets `compile: false` first in that group.
Comments (`#` to the end of a line) and quoted text are skipped when the braces are read.

A .ode SOURCE is first converted by BIN (`BIN --convert --yes`) in a folder of its own, with the
files beside it copied there. The sibling files of SOURCE are copied beside DEST, so DEST loads the
same includes and animations. The helper exits with an error, writing nothing, when the braces do not
balance, when the file has more than one top-level options statement, or when the placed option cannot
be found again in the copy: a wrong copy is never written silently.

Run it with the repo's environment: `uv run --locked python tools/interpreted_copy.py ...`.
A run of the copy logs no "Compiled N of M programs" line at --verbose (expr_native.cpp).
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

OPTION = 'compile: false'


class CopyError(Exception):
    pass


def code_chars(text):
    """(index, char) for each character that is code: not in a comment or a quoted text."""
    quoted = comment = False
    for i, c in enumerate(text):
        if comment:
            comment = c != '\n'
        elif quoted:
            quoted = c != '"'
        elif c == '#':
            comment = True
        elif c == '"':
            quoted = True
        else:
            yield i, c


def options_blocks(text):
    """(index of its `{`, index of its `}`) for each top-level options statement."""
    blocks = []
    depth = 0
    start = None
    open_at = None
    for i, c in code_chars(text):
        if depth == 0 and start is None and text.startswith('options', i) and (i == 0 or text[i - 1] == '\n'):
            start = i
        if c == '{':
            depth += 1
            if depth == 1 and start is not None and open_at is None:
                open_at = i
        elif c == '}':
            depth -= 1
            if depth < 0:
                raise CopyError('unbalanced `}` in the model')
            if depth == 0 and open_at is not None:
                blocks.append((open_at, i))
                start = open_at = None
    if depth != 0:
        raise CopyError('unbalanced `{` in the model: its options block cannot be found')
    return blocks


def integration_group(text, open_at, close_at):
    """index of the `{` of the block's own `integration:` group, or None."""
    depth = 0
    for i, c in code_chars(text):
        if i <= open_at:
            continue
        if i >= close_at:
            break
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        elif depth == 0 and text.startswith('integration', i) and (i == 0 or not text[i - 1].isalnum()):
            j = i + len('integration')
            while j < len(text) and text[j] in ' \t':
                j += 1
            if j < len(text) and text[j] == ':':
                j += 1
                while j < len(text) and text[j] in ' \t\n':
                    j += 1
                if j < len(text) and text[j] == '{':
                    return j
    return None


def interpreted(text):
    blocks = options_blocks(text)
    if len(blocks) > 1:
        raise CopyError('the model has %d options statements; options is written once' % len(blocks))
    if not blocks:
        out = text.rstrip('\n') + '\noptions { integration: { %s } }\n' % OPTION
        placed = 'options { integration: { %s } }' % OPTION
    else:
        open_at, close_at = blocks[0]
        group = integration_group(text, open_at, close_at)
        if group is not None:
            out = text[:group + 1] + ' %s,' % OPTION + text[group + 1:]
            placed = 'integration: { %s,' % OPTION
        else:
            out = text[:open_at + 1] + '\n  integration: { %s },' % OPTION + text[open_at + 1:]
            placed = 'integration: { %s },' % OPTION
    if out.count(OPTION) != text.count(OPTION) + 1 or placed not in out:
        raise CopyError('could not place `%s` in the options' % OPTION)
    # the copy must still read back with the option in it, at the top level of the options block
    again = options_blocks(out)
    if len(again) != 1 or integration_group(out, again[0][0], again[0][1]) is None:
        raise CopyError('the copy has no integration group holding `%s`' % OPTION)
    if OPTION not in out[again[0][0]:again[0][1]]:
        raise CopyError('the copy does not hold `%s` in its options' % OPTION)
    return out


def convert(binary, source, folder):
    """the .odex the binary writes for the .ode SOURCE, in a folder of its own (siblings copied there)."""
    source = Path(source)
    for p in source.parent.iterdir():
        if p.is_file():
            shutil.copy2(p, folder / p.name)
    try:
        done = subprocess.run([binary, '--convert', '--yes', source.name], cwd=folder,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError as error:
        raise CopyError('cannot run %s: %s' % (binary, error))
    odex = folder / (source.stem + '.odex')
    if not odex.exists():
        raise CopyError('%s did not convert (exit %d)' % (source, done.returncode))
    return odex


def main(argv):
    args = argv[1:]
    binary = None
    if args[:1] == ['--convert']:
        if len(args) < 2:
            raise CopyError('usage: interpreted_copy.py [--convert BIN] SOURCE DEST')
        binary, args = os.path.abspath(args[1]), args[2:]
    if len(args) != 2:
        raise CopyError('usage: interpreted_copy.py [--convert BIN] SOURCE DEST')
    source, dest = Path(args[0]), Path(args[1])
    if source.suffix == '.ode':
        if binary is None:
            raise CopyError('%s is a .ode: give the binary with --convert BIN' % source)
        with tempfile.TemporaryDirectory(prefix='interp-') as folder:
            text = convert(binary, source, Path(folder)).read_text(encoding='utf-8')
            source_text = interpreted(text)
    else:
        source_text = interpreted(source.read_text(encoding='utf-8'))
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.parent != source.parent:
        for p in source.parent.iterdir():
            if p.is_file() and p.name != source.name and not (dest.parent / p.name).exists():
                shutil.copy2(p, dest.parent / p.name)
    dest.write_text(source_text, encoding='utf-8', newline='')


if __name__ == '__main__':
    try:
        main(sys.argv)
    except CopyError as error:
        sys.exit('interpreted_copy: ' + str(error))
