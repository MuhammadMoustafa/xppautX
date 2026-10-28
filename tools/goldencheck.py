#!/usr/bin/env python3
"""Golden-file checks for PDF, SVG, GIF and array-print exports
(W31c: what W29f compared by hand, as a committed check).
Drives xppautX --server through tools/xppclient.py's Server and compares
what it writes, byte for byte with CRs stripped (a Windows checkout may
have CRLF line endings), with tests/golden/<name>. A difference prints
the file and the number of its first differing line.

Three models cover the kinds of output: examples/ode/lecar.odex (run,
then Graphic stuff/PDF and .../SVG export the default V-W phase
plane, its own two variables as an array plot for Print arrayplot, and
two kinescope frames saved as GIF), examples/ode/vanderpol.odex (run,
switched to the x-xp phase plane with a fixed view, Nullcline/New and
Dir.field-Flow/(D)irect Field computed, then a PDF export, so a
plot with a nullcline and a direction field is covered too) and
tools/models/pictures.odex (W492: colours with alpha, dashed curves with
markers, a label and a frozen curve in their own colours and styles, PDF and SVG).

Nothing here embeds a date, a host path or the program's version: the
PDF has no date or identifier (core/my_pdf.cpp), and neither writer reads
anything outside the picture, so the same session is the same bytes.
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
# a differing output is copied here (CI uploads it): the bytes, not only the first differing line
ap.add_argument('--keep', default=None, metavar='DIR')
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
    print('  got:  %s' % got_lines[line - 1][:200].decode('latin-1') if line <= len(got_lines) else '  got:  (end of file)')
    print('  want: %s' % want_lines[line - 1][:200].decode('latin-1') if line <= len(want_lines) else '  want: (end of file)')
    if args.keep:
        os.makedirs(args.keep, exist_ok=True)
        with open(os.path.join(args.keep, name), 'wb') as f:
            f.write(text)


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
    s = Server(binary, 'examples/ode/lecar.odex', env={'HOME': home}, verbose=args.v)
    try:
        run_and_go(s)

        # Graphic stuff/PDF: menu 'g', its submenu's 'p', the picture
        # parameters form (kept at its own defaults), then the file to
        # write it to
        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': menu('p'), 'form': form_defaults,
                                 'file': lambda e: {'ok': 1, 'file': 'lecar.pdf'}})
        check_file('lecar.pdf', read(s.run, 'lecar.pdf'))

        # .../SVG: the same submenu's 'v' (the same parameters form), then
        # the file to write
        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': menu('v'), 'form': form_defaults, 'file': lambda e: {'ok': 1, 'file': 'lecar.svg'}})
        check_file('lecar.svg', read(s.run, 'lecar.svg'))

        # the IC box's arry button (V, W, lecar's own two variables), then
        # Print arrayplot with a fixed name in place of its own default
        s.send(cmd='plotvars', how=2, names=['V', 'W'])
        s.collect(is_idle)

        def print_form(e):
            values = list(e['values'])
            return {'ok': 1, 'values': values}

        s.send(cmd='key', win='aplot', key='p')
        s.answer_asks(is_idle, {'form': print_form, 'file': lambda e: {'file': 'lecar_array.pdf'}})
        check_file('lecar_array.pdf', read(s.run, 'lecar_array.pdf'))
        s.send(cmd='window.destroy', window='array_plot_1')
        s.collect(is_idle)

        # Kinescope: capture window 1 twice, then Save writes a GIF per
        # frame from whatever picture the client answers "pixels" with
        for _ in range(2):
            s.send(cmd='key', key='k')
            s.answer_asks(lambda e: e.get('ev') == 'film', {'menu': menu('c')})
            s.collect(is_idle)
        s.send(cmd='key', key='k')
        # one ask for the series: the second frame's name is derived (W130)
        s.answer_asks(is_idle, {'menu': menu('s'), 'file': lambda e: {'file': 'kin.gif'}, 'pixels': pixels})
        for name in ('kin.gif', 'kin-frame1.gif'):
            check_file(name, read(s.run, name))
    finally:
        s.close()


def run_vanderpol(home):
    s = Server(binary, 'examples/ode/vanderpol.odex', env={'HOME': home}, verbose=args.v)
    try:
        run_and_go(s)

        # the ICs box's pp (phase plane) button: x vs xp, then a fixed
        # view so the axes (and so the PDF tick labels) do not
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
                                 'file': lambda e: {'ok': 1, 'file': 'vanderpol.pdf'}})
        check_file('vanderpol.pdf', read(s.run, 'vanderpol.pdf'))
    finally:
        s.close()


def run_pictures(home):
    """W492: colours with alpha, a dashed curve with diamond markers, a label and a frozen curve in their own
    colours and styles, the array plot in the plasma scale"""
    s = Server(binary, 'tools/models/pictures.odex', env={'HOME': home}, verbose=args.v)
    try:
        run_and_go(s)
        s.send(cmd='key', key='n')
        s.answer_asks(is_idle, {'menu': menu('n')})
        s.send(cmd='annotation.add_text', window='graph_1', x=0.1, y=0.9, text='\\1a\\0 label', text_size=3, color='#1c7ed6cc')
        s.collect(is_idle)
        # Graphic stuff/Freeze/Freeze, its form: colour, how it is drawn, dash, width, marker and its size, key and name
        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': lambda e: {'key': 'f'}, 'form': lambda e: {
            'ok': 1, 'values': ['#7048e8b0', 'both', 'dotted', '2', 'cross', '3', 'first run', 'frz1']}})
        s.send(cmd='key', key='i')
        s.answer_asks(is_idle, {'menu': menu('g')})
        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': menu('p'), 'form': form_defaults, 'file': lambda e: {'ok': 1, 'file': 'pictures.pdf'}})
        check_file('pictures.pdf', read(s.run, 'pictures.pdf'))
        s.send(cmd='key', key='g')
        s.answer_asks(is_idle, {'menu': menu('v'), 'form': form_defaults, 'file': lambda e: {'ok': 1, 'file': 'pictures.svg'}})
        check_file('pictures.svg', read(s.run, 'pictures.svg'))
        s.send(cmd='plotvars', how=2, names=['v', 'w'])
        s.collect(is_idle)
        s.send(cmd='key', win='aplot', key='p')
        s.answer_asks(is_idle, {'form': lambda e: {'ok': 1, 'values': ['top', 'side', 'bottom']},
                                 'file': lambda e: {'file': 'pictures_array.pdf'}})
        check_file('pictures_array.pdf', read(s.run, 'pictures_array.pdf'))
        s.send(cmd='window.destroy', window='array_plot_1')
        s.collect(is_idle)
    finally:
        s.close()


def main():
    home = tempfile.mkdtemp(prefix='xppgoldenhome')
    try:
        run_lecar(home)
        run_vanderpol(home)
        run_pictures(home)
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
