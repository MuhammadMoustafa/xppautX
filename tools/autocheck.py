#!/usr/bin/env python3
"""Checks of the protocol front end's behaviour under load: how AUTO's drawing
travels, how input is read, and how quickly a long computation stops.

usage: tools/autocheck.py [--server ./xppautX] [-v] [--report] [SECTION...]

Sections: draw, input, abort, control, files, sessions, session, script,
names (default: all; tools/verify.sh runs them all). files compares AUTO's
saved diagram of lecar with a reference; sessions checks that concurrent servers
keep their AUTO files apart; session is the "cmd":"session" save/load of
docs/protocol.md (issue #11): one name for the .set and .auto pair a long
AUTO run is picked back up from; script plays
examples/scripts/lecar_auto.jsonl through --script (docs/protocol.md
"Scripts") and checks a broken script exits 1; names loads
tools/models/longnames.ode (20-40 character names) and checks it computes,
saves and continues exactly like shortnames.ode. --report prints the
measurements without failing on the latency limits, for comparing builds.
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xppclient import Server, is_idle, is_ask, is_state, draws, draw_ops, last_picture

ap = argparse.ArgumentParser()
ap.add_argument('--server', default='./xppautX')
ap.add_argument('-v', action='store_true')
ap.add_argument('--report', action='store_true', help='measure only; latency limits do not fail')
ap.add_argument('sections', nargs='*', default=['draw', 'input', 'abort', 'control', 'files', 'sessions', 'session',
                                                'script', 'names'])
args = ap.parse_args()
here = os.path.dirname(os.path.abspath(__file__))
LECAR = 'examples/ode/lecar.ode'
HEAVY = os.path.join(here, 'models', 'heavy.ode')
PICTURES = os.path.join(here, 'models', 'lecar_auto_pictures.json')
DIAGRAM = os.path.join(here, 'models', 'lecar_diagram.auto')
SCRIPT = 'examples/scripts/lecar_auto.jsonl'
failures = 0


def check(name, ok, detail='', limit=False):
    """limit: a timing limit, reported but not failed under --report"""
    global failures
    if limit and args.report:
        print('INFO ' + name + ('  ' + detail if detail else ''))
        return
    print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else '  ' + detail))
    failures += 0 if ok else 1


def open_auto(s):
    s.send(cmd='key', key='f')
    s.send(cmd='key', key='a')
    s.collect(lambda e: e.get('ev') == 'window' and e.get('win') == 101)
    s.collect(is_idle)


def run_menu(s, key, timeout=60):
    """Auto/Run, answer the start menu with key; returns the events to idle"""
    s.send(cmd='auto', op='run')
    evs, ask = s.collect(is_ask)
    if ask is None:
        return evs
    s.send(cmd='answer', id=ask['id'], key=key)
    more, _ = s.collect(is_idle, timeout=timeout)
    return evs + more


def grab_hopf(s):
    """Grab, Tab to the first label, take it"""
    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='Tab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='Return')
    s.collect(is_idle)


class Diagram:
    """the AUTO diagram as the client keeps it from "diagram" events"""

    def __init__(self):
        self.axes, self.pts = None, []

    def apply(self, evs):
        for e in evs:
            if e.get('ev') != 'diagram':
                continue
            if e['op'] in ('axes', 'reset'):
                if e['op'] == 'reset':
                    del self.pts[e['keep']:]
                self.axes = e
            elif e['op'] == 'add':
                assert e['from'] <= len(self.pts), (e['from'], len(self.pts))
                del self.pts[e['from']:]
                for r in e['runs']:
                    labs = {i: lab for i, lab, sym in r.get('lab', [])}
                    for i, x in enumerate(r['x']):
                        self.pts.append({'br': r['br'], 'pt': r['pt'] + i, 'd': r['d'], 'c': r['c'], 'x': x,
                                         'y': r['y'][i], 'y2': (r.get('y2') or r['y'])[i], 'lab': labs.get(i, 0),
                                         'new': i == 0 and r.get('new', 0)})
        return self

    def visible(self, a, b):
        """whether auto_line draws some of the segment a-b (clipped to the axes)"""
        ax, ay, bx, by = a['x'], a['y'], b['x'], b['y']
        t0, t1 = 0.0, 1.0
        for p, q in ((ax - bx, ax - self.axes['xmin']), (bx - ax, self.axes['xmax'] - ax),
                     (ay - by, ay - self.axes['ymin']), (by - ay, self.axes['ymax'] - ay)):
            if p == 0:
                if q < 0:
                    return False
            elif p < 0:
                t0 = max(t0, q / p)
            else:
                t1 = min(t1, q / p)
        return t0 <= t1

    def inside(self, x, y):
        """chk_auto_bnds"""
        i, j = self.pixel(x, y)
        a = self.axes
        return a['x0'] <= i < a['x0'] + a['wid'] and a['y0'] <= j < a['y0'] + a['hgt']

    def pixel(self, x, y):
        """auto_nox.c IXVal, IYVal"""
        a = self.axes
        return (int(a['wid'] * (x - a['xmin']) / (a['xmax'] - a['xmin'])) + a['x0'],
                a['hgt'] - int(a['hgt'] * (y - a['ymin']) / (a['ymax'] - a['ymin'])) + a['y0'])


def segments_by_color(ops):
    """the line segments of window 101's ops by colour, and their ends"""
    n, ends, col = {}, set(), 0
    for o in ops:
        if o[0] == 'color':
            col = o[1]
        elif o[0] in ('poly', 'line'):
            v = list(zip(o[1::2], o[2::2]))
            n[col] = n.get(col, 0) + len(v) - 1
            ends.update(v)
    return n, ends


def diagram_ops(evs):
    return [e['op'] for e in evs if e.get('ev') == 'diagram']


def same_axes(axes, st, keys=('xmin', 'xmax', 'ymin', 'ymax', 'x0', 'y0', 'wid', 'hgt')):
    """the axes of the diagram data are state.auto (printed with %g)"""
    return all(abs(axes[k] - st[k]) <= 1e-5 * max(1, abs(st[k])) for k in keys)


def ops_by_kind(ops):
    n = {}
    for o in ops:
        n[o[0]] = n.get(o[0], 0) + 1
    return n


# ---- draw: AUTO's drawing arrives in a few events, polylines join ----------

def section_draw():
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_idle)
    open_auto(s)
    s.send(cmd='size', win=101, w=500, h=300)
    evs, _ = s.collect(is_idle)
    dg = Diagram().apply(evs)
    evs = run_menu(s, 's')
    pts = sum(1 for o in draw_ops(evs, 101) if o[0] in ('line', 'poly'))
    got = {'run': {w: last_picture(evs, w) for w in (102, 103)}}

    # the diagram's data (docs/protocol.md "diagram") is what was drawn: a
    # segment back to the point before for every line point, in the colour
    # of its stability, and a cross of two segments at a label's y and y2,
    # all where the axes show them
    dg.apply(evs)
    lines, branches = {}, {}
    for k, p in enumerate(dg.pts):
        branches[p['br']] = branches.get(p['br'], 0) + 1
        if p['d'] == 1 and dg.visible(p, p if p['new'] or k == 0 else dg.pts[k - 1]):
            lines[p['c']] = lines.get(p['c'], 0) + 1
        if p['lab']:
            lines[0] = lines.get(0, 0) + 2 * (dg.inside(p['x'], p['y']) + dg.inside(p['x'], p['y2']))
    drawn, ends = segments_by_color(draw_ops(evs, 101))
    print('INFO steady run: points by branch %s, data %d B, drawing %d B' % (
        branches, sum(len(json.dumps(e)) for e in evs if e.get('ev') == 'diagram'),
        sum(len(json.dumps(e)) for e in draws(evs, 101))))
    check('the diagram data of a steady run has every point drawn (segments by colour)',
          len(dg.pts) > 20 and lines == drawn, 'data %s drawing %s' % (lines, drawn))
    off = [p for p in dg.pts if p['d'] == 1 and dg.inside(p['x'], p['y']) and not any(
        abs(dg.pixel(p['x'], p['y'])[0] - x) <= 1 and abs(dg.pixel(p['x'], p['y'])[1] - y) <= 1 for x, y in ends)]
    check('and every point is where the drawing has it', not off, str(off[:3]))
    st = [e for e in evs if is_state(e)][-1]['auto']
    check('the diagram axes are the AUTO ranges', same_axes(dg.axes, st), 'axes %s state %s' % (dg.axes, st))

    s.send(cmd='auto', op='redraw')
    evs, _ = s.collect(is_idle)
    n101 = len(draws(evs, 101))
    ops = draw_ops(evs, 101)
    polys = [o for o in ops if o[0] == 'poly']
    longest = max(((len(o) - 1) // 2 for o in polys), default=0)
    print('INFO reDraw: %d draw events (101: %d, 102: %d), 101 ops %s, longest poly %d points'
          % (len([e for e in evs if e.get('ev') == 'draw']), n101, len(draws(evs, 102)), ops_by_kind(ops), longest))
    check('a reDraw arrives in a few draw events', len([e for e in evs if e.get('ev') == 'draw']) <= 10,
          '%d events' % len([e for e in evs if e.get('ev') == 'draw']))
    check('a reDraw of the same diagram sends its axes, not its points again', diagram_ops(evs) == ['axes'],
          str(diagram_ops(evs)))
    check('a reDraw joins the branch into polylines', longest >= 10, 'longest poly %d points' % longest)

    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='ArrowRight')
    evs, ask = s.collect(is_ask)
    got['grab'] = {w: last_picture(evs, w) for w in (102, 103)}
    check('a grab step shows the circle and the point at once',
          got['grab'][102][:1] == [['clear']] and got['grab'][103][:1] == [['clear']],
          str(got['grab'])[:200])
    check('a grab step does not clear the diagram', ['clear'] not in draw_ops(evs, 101), str(draw_ops(evs, 101))[:200])

    # Enter (FINE) used to redraw_diagram() the whole thing just to be rid of
    # the XOR cursor (auto_grab_end, core/auto_nox.c traverse_diagram): the
    # browser now hides a cursor overlay instead, so taking a point should be
    # as cheap as any other grab step, not a full repaint.
    s.send(cmd='answer', id=ask['id'], key='Return')
    take_evs, _ = s.collect(is_idle)
    n_draw = len(draws(take_evs, 101))
    check('taking a grab point (Enter) does not clear the diagram',
          ['clear'] not in draw_ops(take_evs, 101), str(draw_ops(take_evs, 101))[:200])
    check('taking a grab point (Enter) redraws in a few events', n_draw < 10, '%d draw events' % n_draw)

    # Esc used to leave the XOR cursor on screen in the browser (it erases
    # nothing on the way out, relying on X11's later full redraw to clean up
    # a cursor that was drawn into the diagram - the browser's overlay has no
    # such redraw to rely on, so auto_grab_end must hide it itself).
    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='ArrowRight')
    esc_evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='Escape')
    more, _ = s.collect(is_idle)
    esc_evs = esc_evs + more
    cursor_ops = [o for o in draw_ops(esc_evs, 101) if o[0] == 'cursor']
    check('Esc from a grab leaves the cursor hidden',
          bool(cursor_ops) and cursor_ops[-1] == ['cursor'], str(cursor_ops))

    # Axes/hI-lo plots another quantity: it clears the diagram (a reset that
    # keeps nothing), and its reDraw sends every point again; a Fit after
    # that only sends the new axes
    n = len(dg.pts)
    s.send(cmd='auto', op='axes')
    evs, _ = s.answer_asks(is_idle, {'menu': lambda e: {'key': 'i'},
                                     'form': lambda e: {'ok': 1, 'values': e['values']}})
    s.send(cmd='auto', op='redraw')
    more, _ = s.collect(is_idle)
    evs += more
    dg.apply(evs)
    resets = [e for e in evs if e.get('ev') == 'diagram' and e['op'] == 'reset']
    check('Axes/hI-lo then reDraw: a reset, then every point again',
          diagram_ops(evs)[:1] == ['reset'] and resets[0]['keep'] == 0 and 'add' in diagram_ops(evs) and
          len(dg.pts) == n and dg.axes['plot'] == 2, '%s, %d points of %d' % (diagram_ops(evs), len(dg.pts), n))
    s.send(cmd='auto', op='axes')
    evs, _ = s.answer_asks(is_idle, {'menu': lambda e: {'key': 'f'}})
    dg.apply(evs)
    st = [e for e in evs if is_state(e)][-1]['auto']
    check('Axes/Fit sends the fitted axes only', diagram_ops(evs) == ['axes'] and same_axes(dg.axes, st),
          '%s axes %s state %s' % (diagram_ops(evs), dg.axes, st))
    s.close()

    got = json.loads(json.dumps(got))  # window keys as strings, like the file
    if not os.path.exists(PICTURES):
        with open(PICTURES, 'w') as f:
            json.dump(got, f, indent=0)
        print('INFO wrote %s (the reference pictures of windows 102 and 103)' % PICTURES)
        return
    with open(PICTURES) as f:
        want = json.load(f)
    for phase in ('run', 'grab'):
        for w in ('102', '103'):
            check('the %s ends with the same picture in window %s' % (phase, w), got[phase][w] == want[phase][w],
                  'got %s want %s' % (str(got[phase][w])[:120], str(want[phase][w])[:120]))


# ---- input: end of input, pipelined commands, no polling -------------------

def section_input():
    t = time.monotonic()
    r = subprocess.run([os.path.abspath(args.server), '--server', os.path.abspath(LECAR)], stdin=subprocess.DEVNULL,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20)
    check('--server exits at end of input', time.monotonic() - t < 3, '%.1fs' % (time.monotonic() - t))

    s = Server(args.server, LECAR, verbose=False)
    s.collect(is_idle)
    n = 1000
    s.proc.stdin.write(''.join(json.dumps({'cmd': 'set', 'kind': 'par', 'name': 'iapp', 'value': i / 10000.0}) + '\n'
                               for i in range(n)))
    s.proc.stdin.flush()
    idles = 0
    while idles < n:
        evs, e = s.collect(is_idle, timeout=20)
        if e is None:
            break
        idles += 1
    s.send(cmd='state')
    evs, st = s.collect(lambda e: e.get('ev') == 'state')
    s.collect(is_idle)
    iapp = st and dict(st['pars']).get('iapp')
    check('%d pipelined commands each end in idle, in order' % n, idles == n and iapp == (n - 1) / 10000.0,
          'idles %d, iapp %s' % (idles, iapp))

    if sys.platform != 'win32':
        pid = s.proc.pid
        t0 = cpu_seconds(pid)
        time.sleep(2)
        used = cpu_seconds(pid) - t0
        check('an idle server uses no CPU', used < 0.1, '%.2f s of CPU in 2 s' % used)
    s.close()


def cpu_seconds(pid):
    with open('/proc/%d/stat' % pid) as f:
        fields = f.read().rsplit(')', 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')


# ---- abort: a long AUTO run stops at once, and can be continued ------------

def periodic_run(s):
    """from the Hopf point of heavy.ode, start a periodic run; returns once
    two points have been drawn, with the time between them"""
    s.collect(is_idle)
    open_auto(s)
    run_menu(s, 's')
    grab_hopf(s)
    s.send(cmd='auto', op='run')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='p')
    stamps = []
    while len(stamps) < 2:
        evs, e = s.collect(lambda e: (e.get('ev') == 'draw' and e.get('win') == 102) or is_idle(e) or is_ask(e),
                           timeout=60)
        if e is None or e.get('ev') != 'draw':
            return None
        stamps.append(e['_t'])
    return stamps[1] - stamps[0]


def run_periodic(s):
    """Auto/Run from the grabbed point, continuing its periodic branch: the
    Hopf point's menu starts it (p), a periodic label's extends it (e)"""
    s.send(cmd='auto', op='run')
    evs, ask = s.collect(is_ask)
    if ask:
        s.send(cmd='answer', id=ask['id'], key='p' if 'p' in ask.get('keys', '') else 'e')


def section_abort():
    s = Server(args.server, HEAVY, verbose=args.v)
    gap = periodic_run(s)
    check('the heavy periodic run is drawing points', gap is not None)
    if gap is None:
        s.close()
        return
    # a point takes a few Newton steps: land the Abort inside one of them
    time.sleep(0.25)
    t = s.send(cmd='abort')
    evs, e = s.collect(is_idle, timeout=120)
    took = (e['_t'] if e else time.monotonic()) - t
    check('Abort stops a periodic run within 0.5 s', e is not None and took < 0.5,
          'abort->idle %.2f s (a point takes %.2f s)' % (took, gap), limit=True)
    print('INFO abort->idle %.2f s, one point %.2f s' % (took, gap))

    # the run ends on an end point (EP, not MX: no convergence), which is
    # where it can be continued from
    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='End')
    evs, ask = s.collect(is_ask)
    info = [o[3] for o in draw_ops(evs, 103) if o[0] == 'rtext']
    last = info[-1] if info else '?'
    print('INFO last point after abort: %s' % last)
    check('an aborted run ends on an EP point', last.split()[2:3] == ['EP'], last)
    s.send(cmd='answer', id=ask['id'], key='Return')
    s.collect(is_idle)
    check('the server is still alive after an abort', s.alive())

    # a second run continues the branch from that point
    run_periodic(s)
    n = 0
    while n < 2:
        evs, e = s.collect(lambda e: (e.get('ev') == 'draw' and e.get('win') == 102) or is_idle(e) or is_ask(e),
                           timeout=60)
        if e is None or e.get('ev') != 'draw':
            break
        n += 1
    check('a run from the end point of the aborted run draws new points', n == 2, '%d points' % n)
    s.send(cmd='abort')
    s.collect(is_idle, timeout=120)
    check('the server is still alive after the second run', s.alive())

    # Close during a run: the AUTO window goes within a second
    run_periodic(s)
    s.collect(lambda e: e.get('ev') == 'draw' and e.get('win') == 102, timeout=60)
    t = s.send(cmd='abort')
    s.send(cmd='auto', op='close')
    evs, e = s.collect(lambda e: e.get('ev') == 'window' and e.get('win') == 101 and e.get('op') == 'destroy',
                       timeout=120)
    took = (e['_t'] if e else time.monotonic()) - t
    check('Abort then Close during a run closes the AUTO window within 1 s', e is not None and took < 1,
          '%.2f s' % took, limit=True)
    print('INFO abort+close -> destroy %.2f s' % took)
    s.collect(is_idle)

    # Quit during a run: the process ends within a second
    open_auto(s)
    run_periodic(s)
    s.collect(lambda e: e.get('ev') == 'draw' and e.get('win') == 102, timeout=60)
    t = s.send(cmd='quit')
    try:
        s.proc.wait(timeout=60)
    except subprocess.TimeoutExpired:
        s.proc.kill()
    took = time.monotonic() - t
    check('Quit during a run exits within 1 s', took < 1, '%.2f s' % took, limit=True)
    print('INFO quit -> exit %.2f s' % took)
    s.close()


# ---- files: the saved diagram of lecar is what it always was --------------

def lecar_diagram(s):
    """steady state, grab the first label, a periodic branch, File/Save
    diagram; returns the file's text (None if not written)"""
    s.collect(is_idle)
    open_auto(s)
    run_menu(s, 's')
    grab_hopf(s)
    run_menu(s, 'p', timeout=120)
    s.send(cmd='auto', op='file')
    evs, e = s.answer_asks(is_idle, {'menu': lambda e: {'key': 's'},
                                     'file': lambda e: {'ok': 1, 'file': 'diagram.auto'},
                                     'string': lambda e: {'ok': 1, 'value': 'diagram.auto'}})
    path = os.path.join(s.run, 'diagram.auto')
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return f.read()


def lines_close(a, b, rtol=1e-6, atol=1e-9):
    """the same tokens, numbers equal to 1e-6: glibc and mingw-w64 libm
    differ by a few ULPs, which a continuation grows into the last digits"""
    ta, tb = a.split(), b.split()
    if len(ta) != len(tb):
        return False
    for x, y in zip(ta, tb):
        if x == y:
            continue
        try:
            fx, fy = float(x), float(y)
        except ValueError:
            return False
        if abs(fx - fy) > atol + rtol * max(abs(fx), abs(fy)):
            return False
    return True


def section_files():
    home = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    got = lecar_diagram(s)
    s.close()
    shutil.rmtree(home, ignore_errors=True)
    lines = got.splitlines() if got else []
    check('File/Save diagram writes the diagram', len(lines) > 50, '%d lines' % len(lines))
    if got is None:
        return
    if not os.path.exists(DIAGRAM):
        with open(DIAGRAM, 'w', newline='') as f:
            f.write(got)
        print('INFO wrote %s (the reference diagram)' % DIAGRAM)
        return
    with open(DIAGRAM, newline='') as f:
        want = f.read().splitlines()
    diff = next((i for i, (a, b) in enumerate(zip(lines, want)) if not lines_close(a, b)), None)
    check('the saved diagram of lecar is unchanged',
          len(lines) == len(want) and diff is None,
          'first difference at line %s; %d vs %d lines' % (diff, len(lines), len(want)))


# ---- sessions: two servers on one model keep their AUTO files apart --------

def section_sessions():
    home = tempfile.mkdtemp(prefix='xpphome')
    a = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    b = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    da = lecar_diagram(a)
    db = lecar_diagram(b)
    check('two sessions on one model both save their diagram', da is not None and db is not None)
    check('and the two diagrams agree', da == db)
    # a grabs the label its own run wrote, after b has run too
    a.send(cmd='auto', op='grab')
    evs, ask = a.collect(is_ask)
    a.send(cmd='answer', id=ask['id'], key='Tab')
    evs, ask = a.collect(is_ask)
    a.send(cmd='answer', id=ask['id'], key='Return')
    a.collect(is_idle)
    evs = run_menu(a, 'e', timeout=120)
    msgs = ' '.join(str(e.get('text', '')) for e in evs if e.get('ev') == 'message')
    check('a session still extends from its own orbit', 'nan' not in msgs.lower() and a.alive(), msgs[:200])
    runs = [a.run, b.run]
    # each session has an AUTO scratch directory of its own, gone at exit
    auto_dirs = a.auto_dirs() + b.auto_dirs()
    check('each session has its own AUTO scratch directory', len(auto_dirs) == 2, str(auto_dirs))
    a.close()
    b.close()
    check('the scratch directories are gone once the sessions exit',
          not any(os.path.isdir(d) for d in auto_dirs), str(auto_dirs))
    left = os.listdir(home)
    check('nothing is written to HOME', left == [], str(left))
    shutil.rmtree(home, ignore_errors=True)


# ---- session: "cmd":"session" save/load picks a long AUTO run back up -----

def section_session():
    home1 = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LECAR, env={'HOME': home1}, verbose=args.v)
    s.collect(is_idle)
    open_auto(s)
    run_menu(s, 's')
    grab_hopf(s)
    run_menu(s, 'p', timeout=120)

    s.send(cmd='session', op='save', name='s1')
    evs, e = s.collect(is_idle, timeout=20)
    st = [x for x in evs if x.get('ev') == 'state']
    saved = st[-1] if st else None
    sess = saved.get('session') if saved else None
    check('session save reports the .set and .auto files',
          sess == {'set': 's1.set', 'auto': 's1.auto'}, str(sess))
    set_path = os.path.join(s.run, 's1.set')
    auto_path = os.path.join(s.run, 's1.auto')
    check('session save writes the .set file', os.path.exists(set_path))
    check('session save writes the .auto file', os.path.exists(auto_path))
    pars1 = dict(saved['pars']) if saved else {}

    # a NEW server, in a new directory, with only the two saved files copied in
    home2 = tempfile.mkdtemp(prefix='xpphome')
    s2 = Server(args.server, LECAR, env={'HOME': home2}, verbose=args.v)
    s2.collect(is_idle)
    if os.path.exists(set_path): shutil.copy(set_path, s2.run)
    if os.path.exists(auto_path): shutil.copy(auto_path, s2.run)
    s.close()
    shutil.rmtree(home1, ignore_errors=True)

    s2.send(cmd='session', op='load', name='s1')
    evs, e = s2.collect(is_idle, timeout=20)
    st = [x for x in evs if x.get('ev') == 'state']
    loaded = st[-1] if st else None
    sess2 = loaded.get('session') if loaded else None
    check('session load reports the same files', sess2 == sess, str(sess2))
    pars2 = dict(loaded['pars']) if loaded else {}
    check('session load restores the first session\'s parameters',
          bool(pars1) and pars1 == pars2, 'saved %s loaded %s' % (pars1, pars2))
    check('session load opens the AUTO window',
          any(x.get('ev') == 'window' and x.get('win') == 101 for x in evs))
    check('session load draws the diagram', bool(draw_ops(evs, 101)))

    # grab the first label and Run extending the branch, as section_sessions
    # does for a session's own orbit: new points, and no NaN
    s2.send(cmd='auto', op='grab')
    evs, ask = s2.collect(is_ask)
    s2.send(cmd='answer', id=ask['id'], key='Tab')
    evs, ask = s2.collect(is_ask)
    s2.send(cmd='answer', id=ask['id'], key='Return')
    s2.collect(is_idle)
    evs = run_menu(s2, 'e', timeout=60)
    msgs = ' '.join(str(e.get('text', '')) for e in evs if e.get('ev') == 'message')
    check('extending the loaded branch draws new points',
          any(e.get('ev') == 'draw' for e in evs) and s2.alive(), str(evs)[:200])
    check('extending the loaded branch draws no NaN message', 'nan' not in msgs.lower(), msgs[:200])
    s2.close()
    shutil.rmtree(home2, ignore_errors=True)


# ---- control: Abort, Quit and queued commands during a long integration ----

def set_total(s, total):
    """nUmerics/Total"""
    s.send(cmd='key', key='u')
    s.collect(is_idle)
    s.send(cmd='key', key='t')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], ok=1, value=str(total))
    s.collect(is_idle)
    s.send(cmd='key', key='Escape')
    s.collect(is_idle)


def rows(evs):
    st = [e for e in evs if e.get('ev') == 'state']
    return st[-1]['rows'] if st else None


# a slider move integrates again at once, with no prompt on the way
RERUN = {'cmd': 'slide', 'name': 'mu', 'value': -1, 'rerun': 1}


def section_control():
    s = Server(args.server, HEAVY, verbose=args.v)
    s.collect(is_idle)
    set_total(s, 400)  # 40001 rows, some 10 s

    # an Abort sent right behind the command, before the engine has taken it
    s.proc.stdin.write(json.dumps(RERUN) + '\n' + json.dumps({'cmd': 'abort'}) + '\n')
    s.proc.stdin.flush()
    t = time.monotonic()
    evs, e = s.collect(is_idle, timeout=60)
    took = (e['_t'] if e else time.monotonic()) - t
    n = rows(evs)
    check('an Abort sent with the command still stops it', e is not None and n is not None and n < 40001,
          'rows %s' % n)
    check('Abort right after the command: idle within 0.5 s', e is not None and took < 0.5, '%.2f s' % took,
          limit=True)
    print('INFO abort with the command -> idle %.2f s, %s rows' % (took, n))
    s.send(cmd='state')
    evs, e = s.collect(is_idle, timeout=10)
    check('an Abort has no idle of its own', len([x for x in evs if is_idle(x)]) == 1 and
          any(x.get('ev') == 'state' for x in evs), str([x.get('ev') for x in evs]))

    # a command sent after an Abort is not cancelled by it
    set_total(s, 20)
    s.send(**RERUN)
    evs, e = s.collect(is_idle, timeout=60)
    check('a command sent after an Abort runs in full', rows(evs) == 2001, 'rows %s' % rows(evs))

    # a normal command sent during a job waits for it and is not lost
    set_total(s, 400)
    open_auto(s)
    s.send(**RERUN)
    s.collect(lambda e: e.get('ev') == 'progress', timeout=30)
    s.send(cmd='auto', op='close')
    t = s.send(cmd='abort')
    evs, e = s.collect(is_idle, timeout=60)
    took = (e['_t'] if e else time.monotonic()) - t
    closed = [x for x in evs if x.get('ev') == 'window' and x.get('win') == 101 and x.get('op') == 'destroy']
    check('Abort stops an integration', e is not None and (rows(evs) or 0) < 40001, 'rows %s' % rows(evs))
    check('Abort stops an integration within 0.5 s', e is not None and took < 0.5, '%.2f s' % took, limit=True)
    print('INFO abort during an integration -> idle %.2f s' % took)
    check('a command sent during a job waits for its idle', not closed)
    evs, e = s.collect(lambda e: e.get('ev') == 'window' and e.get('win') == 101 and e.get('op') == 'destroy',
                       timeout=10)
    check('a command sent during a job runs after it (Close)', e is not None)
    s.collect(is_idle)

    # Quit during an integration
    s.send(**RERUN)
    s.collect(lambda e: e.get('ev') == 'progress', timeout=30)
    t = s.send(cmd='quit')
    try:
        s.proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        s.proc.kill()
    took = time.monotonic() - t
    check('Quit during an integration exits within 1 s', took < 1, '%.2f s' % took, limit=True)
    print('INFO quit during an integration -> exit %.2f s' % took)
    s.close()


# ---- names: a model with long names works like the same model with short --

LONG = os.path.join(here, 'models', 'longnames.ode')
SHORT = os.path.join(here, 'models', 'shortnames.ode')
LONG_PAR = 'applied_stimulus_current_amplitude'
LONG_B = 'recovery_slope_parameter_b_with_forty_ch'


def silent_output(ode):
    """xppautX ODE -silent in a scratch dir: output.dat's text (None if none)"""
    run = tempfile.mkdtemp(prefix='xppnames')
    shutil.copy(ode, run)
    subprocess.run([os.path.abspath(args.server), os.path.basename(ode), '-silent'], cwd=run,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
    path = os.path.join(run, 'output.dat')
    text = open(path).read() if os.path.exists(path) else None
    shutil.rmtree(run, ignore_errors=True)
    return text


def last_state(evs):
    st = [e for e in evs if e.get('ev') == 'state']
    return st[-1] if st else None


def names_diagram(ode):
    """load ODE, integrate, start from the end (the rest state), AUTO steady
    state over the first parameter; returns the Diagram and the symbols of
    its labelled points"""
    s = Server(args.server, ode, verbose=args.v)
    s.collect(is_idle)
    for key in 'gl':  # Initialconds/Go, then Initialconds/Last
        s.send(cmd='key', key='i')
        evs, ask = s.collect(is_ask)
        s.send(cmd='answer', id=ask['id'], key=key)
        s.collect(is_idle, timeout=30)
    s.send(cmd='key', key='f')
    s.send(cmd='key', key='a')
    evs, _ = s.collect(lambda e: e.get('ev') == 'window' and e.get('win') == 101)
    more, _ = s.collect(is_idle)
    evs += more + run_menu(s, 's', timeout=60)
    syms = [sym for e in evs if e.get('ev') == 'diagram' and e['op'] == 'add'
            for r in e['runs'] for i, lab, sym in r.get('lab', [])]
    s.close()
    return Diagram().apply(evs), syms


def section_names():
    short, long_ = silent_output(SHORT), silent_output(LONG)
    check('names: -silent integrates the long-named model',
          long_ is not None and len(long_.splitlines()) == 2001, str(long_ and len(long_.splitlines())))
    check('names: and its output.dat is the short-named model\'s', long_ is not None and long_ == short)

    home = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LONG, env={'HOME': home}, verbose=args.v)
    evs, _ = s.collect(is_idle)
    hello = next((e for e in evs if e.get('ev') == 'hello'), None)
    st = last_state(evs)
    msgs = [e for e in evs if e.get('ev') == 'message']
    check('names: the model loads with no message', st is not None and not msgs, str(msgs)[:200])
    pars = dict(st['pars']) if st else {}
    check('names: state carries the parameters\' full names', LONG_B in pars and LONG_PAR in pars, str(pars))
    check('names: and the variables\'', st is not None and [n for n, v in st['ics']] ==
          ['MEMBRANE_POTENTIAL_FAST_VARIABLE', 'SLOW_RECOVERY_VARIABLE_W'], str(st and st['ics']))
    check('names: hello lists the auxiliary by name', hello is not None and
          'TOTAL_MEMBRANE_DRIVE_CURRENT' in hello['lists'][0], str(hello and hello['lists'][0]))

    s.send(cmd='set', kind='par', name=LONG_B, value=0.9)
    s.collect(is_idle)
    s.send(cmd='set', kind='ic', name='slow_recovery_variable_w', value=-0.25)
    evs, _ = s.collect(is_idle)
    st = last_state(evs)
    check('names: set a parameter by its long name', st is not None and dict(st['pars']).get(LONG_B) == 0.9,
          str(st and st['pars']))
    check('names: set an initial condition by its long name, in any case',
          st is not None and dict(st['ics']).get('SLOW_RECOVERY_VARIABLE_W') == -0.25, str(st and st['ics']))
    # a name longer than any matches nothing, not the name it starts with
    s.send(cmd='set', kind='par', name=LONG_B + 'x' * 40, value=5)
    evs, _ = s.collect(is_idle)
    st = last_state(evs)
    check('names: a longer name sets nothing', st is not None and dict(st['pars']).get(LONG_B) == 0.9,
          str(st and st['pars']))

    s.send(cmd='key', key='i')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='g')
    s.collect(is_idle, timeout=30)
    s.send(cmd='browser', **{'from': 0, 'count': 1, 'col': 1, 'ncol': 3})
    evs, br = s.collect(lambda e: e.get('ev') == 'browser')
    s.collect(is_idle)
    check('names: the data browser heads its columns with the long names', br is not None and br['cols'] ==
          ['T', 'MEMBRANE_POTENTIAL_FAST_VARIABLE', 'SLOW_RECOVERY_VARIABLE_W', 'TOTAL_MEMBRANE_DRIVE_CURRENT'],
          str(br and br['cols']))

    # the .set file of a session keeps the values under the long names
    s.send(cmd='session', op='save', name='ln')
    s.collect(is_idle, timeout=20)
    set_path = os.path.join(s.run, 'ln.set')
    text = open(set_path).read() if os.path.exists(set_path) else ''
    check('names: the .set file names the long parameter', LONG_B in text)
    s2 = Server(args.server, LONG, env={'HOME': home}, verbose=args.v)
    s2.collect(is_idle)
    for f in ('ln.set', 'ln.auto'):
        if os.path.exists(os.path.join(s.run, f)):
            shutil.copy(os.path.join(s.run, f), s2.run)
    s.close()
    s2.send(cmd='session', op='load', name='ln')
    evs, _ = s2.collect(is_idle, timeout=20)
    st = last_state(evs)
    check('names: a .set round trip keeps the long-named values',
          st is not None and dict(st['pars']).get(LONG_B) == 0.9 and
          dict(st['ics']).get('SLOW_RECOVERY_VARIABLE_W') == -0.25, str(st and (st['pars'], st['ics'])))
    s2.close()
    shutil.rmtree(home, ignore_errors=True)

    dl, syml = names_diagram(LONG)
    ds, syms = names_diagram(SHORT)
    check('names: AUTO continues the steady state over a long-named parameter', len(dl.pts) > 20,
          '%d points' % len(dl.pts))
    check('names: the diagram\'s axis is labelled with the parameter\'s full name',
          dl.axes is not None and dl.axes.get('xlabel') == LONG_PAR, str(dl.axes and dl.axes.get('xlabel')))
    key = lambda d: [(p['br'], p['pt'], p['lab'], p['x'], p['y']) for p in d.pts]
    check('names: and the diagram is the short-named model\'s', key(dl) == key(ds),
          '%d vs %d points' % (len(dl.pts), len(ds.pts)))
    check('names: the Hopf bifurcation is labelled in both', 'HB' in syml and syml == syms, '%s %s' % (syml, syms))


# ---- script: xppautX --script plays a file of protocol commands -----------

def run_script(script_path, ode=LECAR):
    """xppautX --script SCRIPT_PATH ODE, in a scratch dir with a copy of
    ODE; returns (exit code, stdout text, the scratch dir)"""
    run = tempfile.mkdtemp(prefix='xppscript')
    shutil.copy(ode, run)
    r = subprocess.run([os.path.abspath(args.server), '--script', os.path.abspath(script_path),
                        os.path.basename(ode)], cwd=run, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       text=True, timeout=60)
    run_script.stderr = r.stderr
    return r.returncode, r.stdout, run


def section_script():
    code, out, run = run_script(SCRIPT)
    idles = out.count('"ev":"idle"')
    check('xppautX --script plays lecar_auto.jsonl to the end', code == 0, 'exit %d, %s' % (code, out[-300:]))
    check('it reaches idle after each of its commands', idles >= 8, '%d idles' % idles)
    check('it saves the AUTO diagram (File/Save diagram, key s)',
          os.path.exists(os.path.join(run, 'lecar.auto')))
    shutil.rmtree(run, ignore_errors=True)

    # a bad formula in a "set" sends a "message" "error" event
    bad = tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False, dir=here)
    bad.write('{"cmd":"set","kind":"par","name":"iapp","text":"%not a valid formula("}\n')
    bad.close()
    code, out, run = run_script(bad.name)
    check('a "set" with a bad formula exits 1', code == 1, 'exit %d' % code)
    os.unlink(bad.name)
    shutil.rmtree(run, ignore_errors=True)

    # an answer sent with no ask pending for it cannot be matched
    bad = tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False, dir=here)
    bad.write('{"cmd":"answer","key":"s"}\n')
    bad.close()
    code, out, run = run_script(bad.name)
    check('an answer to nothing exits 1', code == 1, 'exit %d' % code)
    os.unlink(bad.name)
    shutil.rmtree(run, ignore_errors=True)

    # a command where an answer was due stops at once, naming the line
    # (it used to wait for an answer forever)
    bad = tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False, dir=here)
    bad.write('# the Initialconds menu opens and wants an answer\n'
              '{"cmd":"key","key":"i"}\n'
              '{"cmd":"key","key":"f"}\n')
    bad.close()
    t = time.monotonic()
    code, out, run = run_script(bad.name)
    check('a command where an answer was due exits 1 at once, naming the line',
          code == 1 and time.monotonic() - t < 10 and 'script line 3 does not answer' in run_script.stderr,
          'exit %d, %s' % (code, run_script.stderr[-300:]))
    os.unlink(bad.name)
    shutil.rmtree(run, ignore_errors=True)


for name in args.sections:
    globals()['section_' + name]()
print('auto checks: %s' % ('all passed' if failures == 0 else '%d failed' % failures))
sys.exit(1 if failures else 0)
