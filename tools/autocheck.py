#!/usr/bin/env python3
"""Checks of the protocol front end's behaviour under load: how AUTO's diagram
travels as data, how input is read, and how quickly a long computation stops.

usage: tools/autocheck.py [--server ./xppautX] [-v] [--report] [SECTION...]

Sections: diagram, input, abort, control, files, stability, sessions, session,
script, replay, names (default: all; tools/verify.sh runs them all). files compares
AUTO's saved diagram of lecar with a reference; stability checks that a point's
eigenvalues are its own (a run's first point is not computed unless it
restarts from a label of the same kind: auto_stability.h); sessions checks that concurrent servers
keep their AUTO files apart; session is the "cmd":"session" save/load of
docs/protocol.md (issue #11): one name for the .set and .auto pair a long
AUTO run is picked back up from; script plays
examples/scripts/lecar_auto.jsonl through --script (docs/protocol.md
"Scripts") and checks a broken script exits 1; names loads
tools/models/longnames.ode (20-40 character names) and checks it computes,
saves and continues exactly like shortnames.ode. replay interrupts an
integration and an AUTO run over --server and checks that a script made of
the same commands and the recorded {"cmd":"abort","at":...} stops them at
the same point: the same data file, the same saved diagram. --report prints the
measurements without failing on the latency limits, for comparing builds.
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xppclient import Server, is_idle, is_ask, is_state

ap = argparse.ArgumentParser()
ap.add_argument('--server', default='./xppautX')
ap.add_argument('-v', action='store_true')
ap.add_argument('--report', action='store_true', help='measure only; latency limits do not fail')
ap.add_argument('sections', nargs='*', default=['diagram', 'input', 'abort', 'control', 'files', 'stability',
                                                'sessions', 'session', 'script', 'replay', 'names'])
args = ap.parse_args()
here = os.path.dirname(os.path.abspath(__file__))
LECAR = 'examples/ode/lecar.ode'
HEAVY = os.path.join(here, 'models', 'heavy.ode')
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

def infos(evs):
    """the autoinfo events (docs/protocol.md "The AUTO diagram as data")"""
    return [e for e in evs if e.get('ev') == 'autoinfo']


def is_point(e):
    """a diagram event with new points: AUTO computed some"""
    return e.get('ev') == 'diagram' and e.get('op') == 'add'


def diagram_ops(evs):
    return [e['op'] for e in evs if e.get('ev') == 'diagram']


def same_axes(axes, st, keys=('xmin', 'xmax', 'ymin', 'ymax', 'x0', 'y0', 'wid', 'hgt')):
    """the axes of the diagram data are state.auto (printed with %g)"""
    return all(abs(axes[k] - st[k]) <= 1e-5 * max(1, abs(st[k])) for k in keys)


# ---- diagram: AUTO's diagram and info strip arrive as data, in few events ----

def section_diagram():
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_idle)
    s.send(cmd='data', events=['autoinfo'])
    s.collect(is_idle)
    open_auto(s)
    s.send(cmd='auto', op='redraw')
    evs, _ = s.collect(is_idle)
    dg = Diagram().apply(evs)
    evs = run_menu(s, 's')

    # the diagram's data (docs/protocol.md "diagram"): the steady branch,
    # its points numbered in order, finite, its labels marked
    dg.apply(evs)
    adds = [e for e in evs if is_point(e)]
    runs = [r for e in adds for r in e['runs']]
    print('INFO steady run: %d points, %d add events, %d runs, data %d B' % (
        len(dg.pts), len(adds), len(runs), sum(len(json.dumps(e)) for e in evs if e.get('ev') == 'diagram')))
    br = [p for p in dg.pts if abs(p['br']) == 1]
    check('the diagram data of a steady run: one branch, its points in order, finite',
          len(dg.pts) > 20 and len(br) == len(dg.pts)
          and all(b['pt'] == a['pt'] + 1 for a, b in zip(br, br[1:]) if not b['new'])
          and all(abs(p['x']) < 1e30 and abs(p['y']) < 1e30 for p in dg.pts),
          '%d points, %d on branch 1' % (len(dg.pts), len(br)))
    check('and its labels are marked', any(p['lab'] for p in dg.pts))
    check('a steady run arrives in a few diagram events, runs of points joined', len(adds) <= 10
          and max((len(r['x']) for r in runs), default=0) >= 10,
          '%d events, longest run %d' % (len(adds), max((len(r['x']) for r in runs), default=0)))
    st = [e for e in evs if is_state(e)][-1]['auto']
    check('the diagram axes are the AUTO ranges', same_axes(dg.axes, st), 'axes %s state %s' % (dg.axes, st))
    got = infos(evs)
    check('the run ends with its last point\'s stability circle (autoinfo)',
          bool(got) and got[-1]['stab'] is not None and got[-1]['info'] is None, str(got[-1:]))

    s.send(cmd='auto', op='redraw')
    evs, _ = s.collect(is_idle)
    print('INFO reDraw: %d events %s' % (len(evs), sorted({e.get('ev') for e in evs})))
    check('a reDraw arrives in a few events', len(evs) <= 10, '%d events' % len(evs))
    check('a reDraw of the same diagram sends its axes, not its points again', diagram_ops(evs) == ['axes'],
          str(diagram_ops(evs)))

    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='ArrowRight')
    evs, ask = s.collect(is_ask)
    got = infos(evs)
    check('a grab step shows the circle and the point at once (autoinfo before the next ask)',
          len(got) == 1 and got[0]['info'] is not None and got[0]['info']['point'] == 1
          and got[0]['stab'] is not None, str(got)[:200])
    check('a grab step does not clear the diagram', not diagram_ops(evs), str(diagram_ops(evs)))

    # Enter (FINE) used to redraw_diagram() the whole thing just to be rid of
    # the XOR cursor (auto_grab_end, core/auto_nox.c traverse_diagram): the
    # page keeps the cursor itself, so taking a point should be as cheap as
    # any other grab step, not a full repaint.
    s.send(cmd='answer', id=ask['id'], key='Return')
    take_evs, _ = s.collect(is_idle)
    check('taking a grab point (Enter) does not clear the diagram',
          'reset' not in diagram_ops(take_evs) and 'add' not in diagram_ops(take_evs), str(diagram_ops(take_evs)))
    check('taking a grab point (Enter) ends in a few events', len(take_evs) < 10, '%d events' % len(take_evs))

    # Esc ends a grab without taking anything
    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='ArrowRight')
    esc_evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='Escape')
    more, end = s.collect(is_idle)
    check('Esc from a grab ends it, the diagram untouched',
          end is not None and not diagram_ops(more) and not any(is_ask(e) for e in more), str(more)[:200])

    # Axes/hI-lo draws the diagram again by itself (T21: no reDraw needed):
    # the client ends up with every point in the new quantities (here the
    # same values, a steady branch's max and min being one, so only the
    # axes go out); a Fit after that only sends the new axes
    n = len(dg.pts)
    s.send(cmd='auto', op='axes')
    evs, _ = s.answer_asks(is_idle, {'menu': lambda e: {'key': 'i'},
                                     'form': lambda e: {'ok': 1, 'values': e['values']}})
    dg.apply(evs)
    check('Axes/hI-lo draws the diagram again at once: every point, the new plot type',
          'axes' in diagram_ops(evs) and len(dg.pts) == n and dg.axes['plot'] == 2,
          '%s, %d points of %d' % (diagram_ops(evs), len(dg.pts), n))
    s.send(cmd='auto', op='axes')
    evs, _ = s.answer_asks(is_idle, {'menu': lambda e: {'key': 'f'}})
    dg.apply(evs)
    st = [e for e in evs if is_state(e)][-1]['auto']
    check('Axes/Fit sends the fitted axes only', diagram_ops(evs) == ['axes'] and same_axes(dg.axes, st),
          '%s axes %s state %s' % (diagram_ops(evs), dg.axes, st))
    s.close()


# ---- input: end of input, pipelined commands, no polling -------------------

def section_input():
    # stdin from the null device (NUL on Windows, which GetFileType calls a
    # character device like a console: W18), a closed pipe and an empty file;
    # a few runs each, since a wrong console attach hung only some of them
    slow = []
    with tempfile.TemporaryFile() as empty:
        for how in ['null device'] * 3 + ['closed pipe'] * 2 + ['empty file'] * 2:
            if how == 'closed pipe':
                rfd, wfd = os.pipe()
                os.close(wfd)
                stdin = rfd
            else:
                stdin = subprocess.DEVNULL if how == 'null device' else empty
            t = time.monotonic()
            try:
                subprocess.run([os.path.abspath(args.server), '--server', os.path.abspath(LECAR)], stdin=stdin,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
                took = time.monotonic() - t
            except subprocess.TimeoutExpired:
                took = None
            if how == 'closed pipe':
                os.close(rfd)
            if took is None or took >= 3:
                slow.append('%s: %s' % (how, 'no exit in 10 s' if took is None else '%.1fs' % took))
    check('--server exits at end of input (null device, closed pipe, empty file)', not slow, '; '.join(slow))

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
    """user+system CPU seconds of pid: /proc on Linux, ps elsewhere (macOS)"""
    if os.path.exists('/proc/%d/stat' % pid):
        with open('/proc/%d/stat' % pid) as f:
            fields = f.read().rsplit(')', 1)[1].split()
        return (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')
    out = subprocess.run(['ps', '-o', 'time=', '-p', str(pid)], capture_output=True, text=True).stdout.strip()
    secs = 0.0
    for part in out.replace('-', ':').split(':'):  # [[dd-]hh:]mm:ss.ss
        secs = secs * 60 + float(part)
    return secs


# ---- abort: a long AUTO run stops at once, and can be continued ------------

def periodic_run(s):
    """from the Hopf point of heavy.ode, start a periodic run; returns once
    two points have been computed (diagram data), with the time between them"""
    s.collect(is_idle)
    s.send(cmd='data', events=['autoinfo'])
    s.collect(is_idle)
    open_auto(s)
    run_menu(s, 's')
    grab_hopf(s)
    s.send(cmd='auto', op='run')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='p')
    stamps = []
    while len(stamps) < 2:
        evs, e = s.collect(lambda e: is_point(e) or is_idle(e) or is_ask(e), timeout=60)
        if e is None or not is_point(e):
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
    check('the heavy periodic run is computing points', gap is not None)
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
    got = [e['info'] for e in infos(evs) if e['info']]
    last = got[-1] if got else {}
    print('INFO last point after abort: %s' % json.dumps(last)[:200])
    check('an aborted run ends on an EP point', last.get('sym') == 'EP', str(last)[:200])
    s.send(cmd='answer', id=ask['id'], key='Return')
    s.collect(is_idle)
    check('the server is still alive after an abort', s.alive())

    # a second run continues the branch from that point
    run_periodic(s)
    n = 0
    while n < 2:
        evs, e = s.collect(lambda e: is_point(e) or is_idle(e) or is_ask(e), timeout=60)
        if e is None or not is_point(e):
            break
        n += 1
    check('a run from the end point of the aborted run computes new points', n == 2, '%d points' % n)
    s.send(cmd='abort')
    s.collect(is_idle, timeout=120)
    check('the server is still alive after the second run', s.alive())

    # Close during a run: the AUTO window goes within a second
    run_periodic(s)
    s.collect(is_point, timeout=60)
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
    s.collect(is_point, timeout=60)
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


# ---- stability: a point's eigenvalues are its own (W15, auto_stability.h) --

def step(s, **c):
    s.send(**c)
    return s.collect(lambda e: is_idle(e) or is_ask(e), timeout=30)


def hopf_steady(s):
    """lecar's "hopf" parameter set, its fixed point as the IC (as
    examples/scripts/lecar_auto.jsonl does), AUTO; returns the events"""
    evs = []
    for c in ({'cmd': 'key', 'key': 'f'}, {'cmd': 'key', 'key': 'g'}, {'cmd': 'answer', 'key': 'd'},
              {'cmd': 'key', 'key': 's'}, {'cmd': 'answer', 'key': 'g'}, {'cmd': 'answer', 'key': 'n'},
              {'cmd': 'eqimport'}, {'cmd': 'key', 'key': 'f'}, {'cmd': 'key', 'key': 'a'}):
        evs += step(s, **c)[0]
    return evs


def run_any(s, key, timeout=120):
    """Auto/Run, answering its menu with key if it asks; the events to idle"""
    s.send(cmd='auto', op='run')
    evs, e = s.collect(lambda e: is_idle(e) or is_ask(e), timeout=timeout)
    if e is not None and is_ask(e):
        s.send(cmd='answer', id=e['id'], key=key)
        more, _ = s.collect(is_idle, timeout=timeout)
        evs += more
    return evs


def grab_point(s, i, take=False):
    """Grab, go to point i of the diagram data; take it (Return) or leave
    (Escape); returns the autoinfo there"""
    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)  # the grab starts on the first point: its autoinfo is here
    if take:
        s.send(cmd='answer', id=ask['id'], point=i, key='Return')
        more, _ = s.collect(is_idle)
    else:
        s.send(cmd='answer', id=ask['id'], point=i)
        more, ask = s.collect(is_ask)
        s.send(cmd='answer', id=ask['id'], key='Escape')
        s.collect(is_idle)
    got = [e for e in infos(evs + more) if e.get('info')]
    return got[-1] if got else None


def circle(info):
    return info['stab']['circle'] if info and info.get('stab') else None


def zeros(c):
    return c is not None and all(v == 0 for pair in c for v in pair)


def section_stability():
    home = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    s.collect(is_idle)
    s.send(cmd='data', events=['autoinfo'])
    s.collect(is_idle)
    dg = Diagram().apply(hopf_steady(s))
    syms = {}

    def run(key):
        """a run: its events into dg, its labels' symbols into syms; the index of its first point"""
        n = len(dg.pts)
        evs = run_any(s, key)
        dg.apply(evs)
        syms.update({lab: sym for e in evs if is_point(e) for r in e['runs'] for i, lab, sym in r.get('lab', [])})
        return n

    def label(sym, lo, hi):
        return next((i for i in range(lo, hi) if syms.get(dg.pts[i]['lab']) == sym), None)

    # 1: a steady state from initial data: the circle the page gets for a
    # stored point is not computed at the first point, the next one's own
    r1 = run('s')
    first, second = grab_point(s, r1), grab_point(s, r1 + 1)
    check('stability: a steady run\'s first point is not computed (autoinfo circle all zeros)',
          first is not None and first['info']['pt'] == 1 and zeros(circle(first)), str(first)[:300])
    check('stability: its second point has its own values',
          second is not None and second['info']['pt'] == 2 and circle(second) and not zeros(circle(second)),
          str(second)[:300])

    # 2: a periodic branch from the Hopf point: a change of kind
    hb = label('HB', r1, len(dg.pts))
    check('stability: the steady branch has a Hopf point', hb is not None, str(syms))
    if hb is None:
        s.close()
        return
    hb_info = grab_point(s, hb, take=True)
    r2 = run('p')
    end2 = len(dg.pts) - 1
    a, b = grab_point(s, r2), grab_point(s, r2 + 1)
    check('stability: a periodic run from a Hopf point starts not computed',
          a is not None and a['stab']['periodic'] == 1 and zeros(circle(a)), str(a)[:300])
    check('stability: its second point has its own multipliers',
          b is not None and b['stab']['periodic'] == 1 and circle(b) and not zeros(circle(b)), str(b)[:300])

    # 3: the steady branch extended from the Hopf label: the same kind, its
    # first point the label's solution, so the label's values
    grab_point(s, hb, take=True)
    r3 = run('e')
    a = grab_point(s, r3)
    check('stability: a steady restart from a label starts with the label\'s values',
          a is not None and r3 < len(dg.pts) and circle(a) and not zeros(circle(a))
          and circle(a) == circle(hb_info), '%s vs %s' % (circle(a), circle(hb_info)))

    # 4: the periodic branch extended from its end point: the same kind
    end_info = grab_point(s, end2, take=True)
    r4 = run('e')
    a = grab_point(s, r4)
    check('stability: a periodic restart from a label starts with the label\'s values',
          a is not None and end_info is not None and r4 < len(dg.pts) and a['stab']['periodic'] == 1
          and circle(a) and not zeros(circle(a)) and circle(a) == circle(end_info),
          '%s vs %s' % (circle(a), circle(end_info)))

    # 5: a two-parameter curve from the Hopf point: a change of kind (a
    # one-parameter diagram holds only its first point)
    grab_point(s, hb, take=True)
    r5 = run('t')
    a = grab_point(s, r5)
    check('stability: a two-parameter run from a Hopf point starts not computed',
          a is not None and r5 < len(dg.pts) and a['info'].get('f2') and zeros(circle(a)), str(a)[:300])
    s.close()
    shutil.rmtree(home, ignore_errors=True)


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
    check('session load sends the diagram', any(is_point(e) for e in evs))

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
    check('extending the loaded branch computes new points',
          any(is_point(e) for e in evs) and s2.alive(), str(evs)[:200])
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


# ---- replay: a recorded interruption stops the script's job at the same point

class Recording:
    """a --server session that keeps the commands it sends, as the browser's
    "Save session script" does: a script of them replays the session"""
    def __init__(self, ode):
        self.s = Server(args.server, ode, verbose=args.v)
        self.s.collect(is_idle)
        self.script = []

    def send(self, **cmd):
        self.script.append(dict(cmd))
        return self.s.send(**cmd)

    def interrupt(self, after):
        """Abort `after` seconds from now; returns the stopped event (None if
        the job ended first) and the events to idle, and records the event as
        the script's abort line"""
        time.sleep(after)
        self.s.send(cmd='abort')
        evs, e = self.s.collect(is_idle, timeout=120)
        stopped = next((x for x in evs if x.get('ev') == 'stopped'), None)
        if stopped:
            self.script.append({'cmd': 'abort', 'at': stopped['at']})
        return stopped, evs

    def write(self, path):
        with open(path, 'w') as f:
            f.write(''.join(json.dumps(c) + '\n' for c in self.script))


def script_file(lines):
    f = tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False, dir=here)
    f.write(''.join(l + '\n' for l in lines))
    f.close()
    return f.name


def stopped_events(out):
    return [json.loads(l)['at'] for l in out.splitlines() if l.startswith('{"ev":"stopped"')]


def last_rows(out):
    st = [json.loads(l) for l in out.splitlines() if l.startswith('{"ev":"state"')]
    return st[-1]['rows'] if st else None


def file_content(path, mode='r'):
    if not os.path.exists(path):
        return None
    with open(path, mode) as f:
        return f.read()


def section_replay():
    # (a) an integration of lecar, total 20000 (400001 rows, some seconds),
    # stopped after 0.3 s, then its data written by the browser's Write
    r = Recording(LECAR)
    s = r.s
    r.send(cmd='key', key='u')
    s.collect(is_idle)
    r.send(cmd='key', key='t')
    s.collect(is_ask)
    r.send(cmd='answer', ok=1, value='20000')
    s.collect(is_idle)
    r.send(cmd='key', key='Escape')
    s.collect(is_idle)
    r.send(cmd='key', key='i')
    s.collect(is_ask)
    r.send(cmd='answer', key='g')
    stopped, evs = r.interrupt(0.3)
    at = stopped['at'] if stopped else {}
    check('replay: an interrupted integration says where it stopped', at.get('what') == 'integrate' and
          0 < at.get('rows', 0) < 400001 and at['rows'] == rows(evs), '%s, rows %s' % (at, rows(evs)))
    print('INFO replay: integration stopped at %s' % at)
    r.send(cmd='browser', op='write')
    s.collect(is_ask)
    r.send(cmd='answer', file='run.dat')
    s.collect(is_idle, timeout=60)
    live_data = file_content(os.path.join(s.run, 'run.dat'), 'rb')
    path = os.path.join(here, 'replay_integrate.jsonl')
    r.write(path)
    s.close()
    code, out, run = run_script(path)
    replay_data = file_content(os.path.join(run, 'run.dat'), 'rb')
    check('replay: the script plays the interrupted integration', code == 0,
          'exit %d, %s' % (code, run_script.stderr[-300:]))
    check('replay: it stops at the recorded point, and says so', stopped_events(out) == [at] and
          last_rows(out) == at.get('rows'), '%s vs %s' % (stopped_events(out), at))
    check('replay: it writes the same data as the recorded session', live_data is not None and
          live_data == replay_data, '%s vs %s bytes' % (live_data and len(live_data), replay_data and len(replay_data)))
    os.unlink(path)
    shutil.rmtree(run, ignore_errors=True)

    # (b) lecar_auto.jsonl's periodic branch stopped after 0.3 s, then the
    # diagram saved (AUTO File/Save diagram)
    with open(SCRIPT) as f:
        lines = [json.loads(l) for l in f if l.strip() and not l.lstrip().startswith('#')]
    body, save = lines[:-3], lines[-3:]
    r = Recording(LECAR)
    for i, c in enumerate(body):
        r.send(**c)
        if i + 1 < len(body):
            r.s.collect(is_ask if body[i + 1]['cmd'] == 'answer' else is_idle, timeout=60)
    stopped, evs = r.interrupt(0.3)
    at = stopped['at'] if stopped else {}
    check('replay: an interrupted AUTO run says where it stopped',
          at.get('what') == 'auto' and at.get('branch', 0) > 0 and at.get('point', 0) > 1, str(at))
    print('INFO replay: AUTO stopped at %s' % at)
    for i, c in enumerate(save):
        r.send(**c)
        r.s.collect(is_ask if i < 2 else is_idle, timeout=30)
    live_diagram = file_content(os.path.join(r.s.run, 'lecar.auto'))
    path = os.path.join(here, 'replay_auto.jsonl')
    r.write(path)
    r.s.close()
    code, out, run = run_script(path)
    replay_diagram = file_content(os.path.join(run, 'lecar.auto'))
    check('replay: the script plays the interrupted AUTO run', code == 0,
          'exit %d, %s' % (code, run_script.stderr[-300:]))
    check('replay: AUTO stops at the recorded branch and point', stopped_events(out) == [at],
          '%s vs %s' % (stopped_events(out), at))
    check('replay: and saves the same diagram as the recorded session', live_diagram is not None and
          live_diagram == replay_diagram, '%s vs %s chars' % (live_diagram and len(live_diagram),
                                                              replay_diagram and len(replay_diagram)))
    os.unlink(path)
    shutil.rmtree(run, ignore_errors=True)

    # (c) an interruption the job never gets to: exit 1 at once, naming the line
    path = script_file(['{"cmd":"key","key":"i"}', '{"cmd":"answer","key":"g"}',
                        '# lecar stores 601 rows',
                        '{"cmd":"abort","at":{"what":"integrate","rows":100000,"t":5000}}',
                        '{"cmd":"key","key":"i"}', '{"cmd":"answer","key":"g"}'])
    code, out, run = run_script(path)
    check('replay: an interruption never reached exits 1, naming its line',
          code == 1 and 'script line 4: the recorded interruption at' in run_script.stderr and
          'was never reached' in run_script.stderr, 'exit %d, %s' % (code, run_script.stderr[-300:]))
    check('replay: and plays nothing after it', out.count('"ev":"idle"') == 1, '%d idles' % out.count('"ev":"idle"'))
    os.unlink(path)
    shutil.rmtree(run, ignore_errors=True)

    # (d) abort lines that interrupt nothing (bare, or of a job that cannot be
    # placed) are consumed: the script goes on
    path = script_file(['{"cmd":"abort"}', '{"cmd":"key","key":"i"}', '{"cmd":"answer","key":"g"}',
                        '{"cmd":"abort","at":{"what":"other"}}', '{"cmd":"abort"}'])
    code, out, run = run_script(path)
    check('replay: abort lines that interrupt nothing do not stall the script',
          code == 0 and last_rows(out) == 601 and not stopped_events(out),
          'exit %d, rows %s, %s' % (code, last_rows(out), run_script.stderr[-300:]))
    os.unlink(path)
    shutil.rmtree(run, ignore_errors=True)


for name in args.sections:
    globals()['section_' + name]()
print('auto checks: %s' % ('all passed' if failures == 0 else '%d failed' % failures))
sys.exit(1 if failures else 0)
