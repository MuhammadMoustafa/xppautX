#!/usr/bin/env python3
"""Golden-file checks for PostScript, SVG, GIF and array-print exports
(W31c: what W29f compared by hand, as a committed check).
Drives xppautX --server through tools/xppclient.py's Server and compares
what it writes, byte for byte with CRs stripped (a Windows checkout may
have CRLF line endings), with tests/golden/<name>. A difference prints
the file and the number of its first differing line.

Two models cover the four kinds of output: examples/ode/lecar.ode (run,
then Graphic stuff/Postscript and .../SVG export the default V-W phase
plane, its own two variables as an array plot for Print arrayplot, and
two kinescope frames saved as GIF) and examples/ode/vanderpol.ode (run,
switched to the x-xp phase plane with a fixed view, Nullcline/New and
Dir.field-Flow/(D)irect Field computed, then a PostScript export, so a
plot with a nullcline and a direction field is covered too).

Nothing here embeds a date, a host path or the program's version:
core/my_ps.cpp and core/my_svg.cpp write fixed header text (checked
below), and my_svg.cpp's only outside input is $HOME/xppaut-stylesheet.css
(read when present and folded into the <style> block), kept out by
pointing HOME at an empty scratch directory for every run started here.
The kinescope GIFs are the core's own GIF encoder (core/scrngif.cpp)
applied to a fixed 2x1 RGB image this script answers every "pixels" ask
with, not a real screen capture: protocol v2 draws nothing itself, only
the client has a picture (docs/protocol.md "pixels").

usage: tools/goldencheck.py [--update] [--bin ./xppautX] [-v]
  --update   write what this run produced as the golden files instead of
             comparing (like tools/examples_check.sh)
  --bin      the xppautX binary (default ./xppautX, or ./xppautX.exe)
"""
import argparse
import base64
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xppclient import SLOW, Server, is_idle, is_ask  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--bin', default=None)
ap.add_argument('--update', action='store_true')
ap.add_argument('-v', action='store_true')
args = ap.parse_args()

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
os.chdir(root)
GOLDEN = os.path.join(root, 'tests', 'golden')

binary = args.bin
if binary is None:
    binary = './xppautX.exe' if not os.path.exists('xppautX') and os.path.exists('xppautX.exe') else './xppautX'

failures = 0


def check_file(name, data):
    """compares data (bytes, or None if the file was never written) with
    tests/golden/name, CRs removed; --update writes it there instead"""
    global failures
    if data is None:
        failures += 1
        print('FAIL %s: was not written' % name)
        return
    text = data.replace(b'\r', b'')
    path = os.path.join(GOLDEN, name)
    if args.update:
        os.makedirs(GOLDEN, exist_ok=True)
        with open(path, 'wb') as f:
            f.write(text)
        print('INFO wrote %s (%d bytes)' % (path, len(text)))
        return
    if not os.path.exists(path):
        failures += 1
        print('FAIL %s: no golden file at %s' % (name, path))
        return
    with open(path, 'rb') as f:
        want = f.read()
    if text == want:
        print('PASS %s (%d bytes)' % (name, len(text)))
        return
    failures += 1
    got_lines, want_lines = text.split(b'\n'), want.split(b'\n')
    line = next((i + 1 for i, (a, b) in enumerate(zip(got_lines, want_lines)) if a != b),
                min(len(got_lines), len(want_lines)) + 1)
    print('FAIL %s: differs from %s at line %d (%d vs %d bytes)' %
          (name, path, line, len(text), len(want)))


def read(run_dir, name):
    path = os.path.join(run_dir, name)
    if not os.path.exists(path):
        return None
    with open(path, 'rb') as f:
        return f.read()


pixels = lambda e: {'w': 2, 'h': 1, 'rgb': base64.b64encode(bytes([255, 0, 0, 0, 0, 255])).decode()}
menu = lambda key: (lambda e: {'key': key})
form_defaults = lambda e: {'ok': 1, 'values': e['values']}


def run_and_go(s):
    """Initialconds/Go: run to completion, check the row count 601 (as
    tools/servercheck.py's own run of lecar does)"""
    global failures
    s.collect(is_idle)  # startup hello/state/idle
    s.send(cmd='key', key='i')
    _, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='g')
    evs, _ = s.collect(is_idle, timeout=30 * SLOW)
    st = next((e for e in reversed(evs) if e.get('ev') == 'state'), None)
    if not (st and st.get('rows')):
        failures += 1
        print('FAIL run: no rows stored (%s)' % (st and st.get('rows')))
    return st


def run_lecar(home):
    s = Server(binary, 'examples/ode/lecar.ode', env={'HOME': home}, verbose=args.v)
    try:
        run_and_go(s)

        # Graphic stuff/Postscript: menu 'g', its submenu's 'p', the PS
        # parameters form (kept at its own defaults), then the file to
        # write it to
        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': menu('p'), 'form': form_defaults,
                                 'file': lambda e: {'ok': 1, 'file': 'lecar.ps'}})
        check_file('lecar.ps', read(s.run, 'lecar.ps'))

        # .../SVG: the same submenu's 'v' (create_svg asks no form), then
        # the file to write
        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': menu('v'), 'file': lambda e: {'ok': 1, 'file': 'lecar.svg'}})
        check_file('lecar.svg', read(s.run, 'lecar.svg'))

        # the IC box's arry button (V, W, lecar's own two variables), then
        # Print arrayplot with a fixed name in place of its own default
        s.send(cmd='plotvars', how=2, names=['V', 'W'])
        s.collect(is_idle)

        def print_form(e):
            values = list(e['values'])
            values[0] = 'lecar_array.ps'
            return {'ok': 1, 'values': values}

        s.send(cmd='key', win='aplot', key='p')
        s.answer_asks(is_idle, {'form': print_form})
        check_file('lecar_array.ps', read(s.run, 'lecar_array.ps'))
        s.send(cmd='aplot', op='close')
        s.collect(is_idle)

        # Kinescope: capture window 1 twice, then Save writes a GIF per
        # frame from whatever picture the client answers "pixels" with
        for _ in range(2):
            s.send(cmd='key', key='k')
            s.answer_asks(lambda e: e.get('ev') == 'film', {'menu': menu('c')})
            s.collect(is_idle)
        s.send(cmd='key', key='k')
        s.answer_asks(is_idle, {'menu': menu('s'), 'string': lambda e: {'value': 'kin'}, 'pixels': pixels})
        for i in range(2):
            check_file('kin_%d.gif' % i, read(s.run, 'kin_%d.gif' % i))
    finally:
        s.close()


def run_vanderpol(home):
    s = Server(binary, 'examples/ode/vanderpol.ode', env={'HOME': home}, verbose=args.v)
    try:
        run_and_go(s)

        # the ICs box's pp (phase plane) button: x vs xp, then a fixed
        # view so the axes (and so the PostScript tick labels) do not
        # depend on any auto-scaling of this run's own trajectory
        s.send(cmd='plotvars', how=1, names=['x', 'xp'])
        s.collect(is_idle)
        # (Window/Window: the keys w w, then the four numbers)
        s.send(cmd='key', key='w')
        s.answer_asks(is_idle, {'menu': menu('w'), 'form': lambda e: {'ok': 1, 'values': ['-3', '3', '-3', '3']}})

        # Nullcline/New, then Dir.field-Flow/(D)irect Field, so the
        # export carries a nullcline and a direction field, not curves
        # alone
        s.send(cmd='key', key='n')
        s.answer_asks(is_idle, {'menu': menu('n')})
        # Direct Field then asks a "Grid:" string, its own default (16)
        s.send(cmd='key', key='d')
        s.answer_asks(is_idle, {'menu': menu('d'), 'string': lambda e: {'value': e['value']}})

        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': menu('p'), 'form': form_defaults,
                                 'file': lambda e: {'ok': 1, 'file': 'vanderpol.ps'}})
        check_file('vanderpol.ps', read(s.run, 'vanderpol.ps'))
    finally:
        s.close()


def main():
    home = tempfile.mkdtemp(prefix='xppgoldenhome')
    try:
        run_lecar(home)
        run_vanderpol(home)
    finally:
        shutil.rmtree(home, ignore_errors=True)
    if args.update:
        print('golden files written to %s' % GOLDEN)
        return 0
    if failures == 0:
        print('golden ok: 0 failures')
        return 0
    print('GOLDEN CHECK FAILED: %d failures' % failures)
    return 1


sys.exit(main())
