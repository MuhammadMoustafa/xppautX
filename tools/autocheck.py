#!/usr/bin/env python3
"""Checks of the protocol front end's behaviour under load: how AUTO's drawing
travels, how input is read, and how quickly a long computation stops.

usage: tools/autocheck.py [--server ./xppautX] [-v] [--report] [SECTION...]

Sections: draw, input, abort, files, sessions (default: draw input abort
files). files compares AUTO's saved diagram of lecar with a reference;
sessions checks that concurrent servers keep their AUTO files apart. --report prints the measurements
without failing on the latency limits, for comparing builds.
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xppclient import Server, is_idle, is_ask, draws, draw_ops, last_picture

ap = argparse.ArgumentParser()
ap.add_argument('--server', default='./xppautX')
ap.add_argument('-v', action='store_true')
ap.add_argument('--report', action='store_true', help='measure only; latency limits do not fail')
ap.add_argument('sections', nargs='*', default=['draw', 'input', 'abort', 'files'])
args = ap.parse_args()
here = os.path.dirname(os.path.abspath(__file__))
LECAR = 'examples/ode/lecar.ode'
HEAVY = os.path.join(here, 'models', 'heavy.ode')
PICTURES = os.path.join(here, 'models', 'lecar_auto_pictures.json')
DIAGRAM = os.path.join(here, 'models', 'lecar_diagram.auto')
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
    s.collect(is_idle)
    evs = run_menu(s, 's')
    pts = sum(1 for o in draw_ops(evs, 101) if o[0] in ('line', 'poly'))
    got = {'run': {w: last_picture(evs, w) for w in (102, 103)}}

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


def section_abort():
    s = Server(args.server, HEAVY, verbose=args.v)
    gap = periodic_run(s)
    check('the heavy periodic run is drawing points', gap is not None)
    if gap is None:
        s.close()
        return
    t = s.send(cmd='abort')
    evs, e = s.collect(is_idle, timeout=120)
    took = (e['_t'] if e else time.monotonic()) - t
    check('Abort stops a periodic run within 0.5 s', e is not None and took < 0.5,
          'abort->idle %.2f s (a point takes %.2f s)' % (took, gap), limit=True)
    print('INFO abort->idle %.2f s, one point %.2f s' % (took, gap))

    # the run can be continued from where it stopped
    s.send(cmd='auto', op='grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='End')
    evs, ask = s.collect(is_ask)
    info = [o[3] for o in draw_ops(evs, 103) if o[0] == 'rtext']
    s.send(cmd='answer', id=ask['id'], key='Escape')
    s.collect(is_idle)
    print('INFO last point after abort: %s' % (info[-1:] or '?'))
    check('the server is still alive after an abort', s.alive())

    # Close during a run: the AUTO window goes within a second
    s.send(cmd='auto', op='run')
    evs, ask = s.collect(is_ask)
    if ask:
        s.send(cmd='answer', id=ask['id'], key='p')
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
    s.send(cmd='auto', op='run')
    evs, ask = s.collect(is_ask)
    if ask:
        s.send(cmd='answer', id=ask['id'], key='p')
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
    diff = next((i for i, (a, b) in enumerate(zip(lines, want)) if a != b), None)
    check('the saved diagram of lecar is unchanged', lines == want,
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
    a.close()
    b.close()
    left = os.listdir(home)
    check('nothing is written to HOME', left == [], str(left))
    shutil.rmtree(home, ignore_errors=True)


for name in args.sections:
    globals()['section_' + name]()
print('auto checks: %s' % ('all passed' if failures == 0 else '%d failed' % failures))
sys.exit(1 if failures else 0)
