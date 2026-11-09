#!/usr/bin/env python3
"""Checks of the protocol front end's behaviour under load: how autoX's diagram
travels as data, how input is read, and how quickly a long computation stops.

usage: tools/autoxcheck.py [--server ./xppautX] [-v] [--shard INDEX/COUNT] [SECTION...]

Sections: diagram, grab, input, abort, control, files, csv, stability, sessions,
session, sessiondata, recording, replay, names, scratch, errors, memory (default: all;
tools/verify.sh runs them all).
memory (W117) shows the adjoint, its H function and a histogram again
after a run grew the store and after a load (they borrow the stored
columns), and runs CVODE twice into a step error and into the Poincare
map's "Cannot zero RHS"; asancheck's build sees a freed column read or
CVODE's memory leaked.
errors checks that invalid Numerics values (Ncol 8, Ntst 0) are refused
at input, leave settings intact and allow the next run to compute the
same points. Unit tests cover the internal numerical error paths.
scratch checks that a start removes an xppautoX-<pid>-N folder (autox_nox.cpp's
autoX scratch directory) left by a dead pid, and leaves one alone whose pid is
still running (issue #32). grab checks "auto" "grab" by label and by type+index
(issue #112): a periodic run from a point grabbed that way gives the same
diagram points as grabbing it interactively, and an unknown label or
type/index is refused. files compares
autoX's saved diagram of lecar with a reference; csv checks File/eXport CSV
(core/csv_export.cpp, W26, issue #42): the diagram and eigenvalues CSVs have
a header of names, the right row counts, and the eigenvalues file's keys
join the diagram file's; stability checks that a point's
eigenvalues are its own (a run's first point is not computed unless it
restarts from a label of the same kind: autox_stability.h); sessions checks that concurrent servers
keep their autoX files apart; session is the "cmd":"session" save/load of
docs/protocol.md (issue #11, W57): one session file, name.snapx, a long
autoX run is picked back up from, carrying its model (W103): opened where
no .ode is, its saved model loads, a file table with it, and a file
without its model is refused; sessiondiagram checks autoX's File/Save
and Load through .snapx, the exact diagram, settings, views and restart
orbits, the saved model, and refusal of damaged members before replacing
any state. recording plays
examples/recordings/lecar_autox.recx with --silent and checks a broken recording exits 1; names loads
tools/models/longnames.odex (200-character names: no length limit, W76)
and checks it computes, saves and continues exactly like shortnames.odex. replay interrupts an
integration and an autoX run over --server and checks that a .recx made of
the same commands and a recorded abort stops them at
the same point: the same data file, the same saved diagram.
nagumo (W345) checks tstheti's planar HomCont branch against the exact
front speed in both directions, then refines the mesh and interval.
first_steps (W572) checks the zeros in a branch's first steps, which
AUTO-07p's and XPPAUT's tests never see (pp2, Lorenz, lamom2, brper and the
forced predator-prey), each against the model's own equations.
"""
import argparse, base64, copy, csv, io, json, math, os, re, shutil, subprocess, sys, tempfile, time, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import autoxcompare
from autoxcompare import BASE, completed, compute, diagram_rows, execute_run, read_description, session_diagram
from xppclient import versioned, versioned_anix, Checker, file_bytes, make_recording, recording_text, replay_recording, run_commands, wait_until, macos_thread_states, SLOW, Server, is_done, is_ask, is_state, placed, whole_series, save_permission, open_autox, run_any

ap = argparse.ArgumentParser()
ap.add_argument('--server', '--bin', dest='server', default='./xppautX')
ap.add_argument('-v', action='store_true')
ap.add_argument('--list', action='store_true', help='print the selected sections and exit')
ap.add_argument('--shard', help='run one round-robin partition of the sections, INDEX/COUNT (one-based)')
ap.add_argument('sections', nargs='*', default=['diagram', 'grab', 'input', 'abort', 'control', 'files', 'csv', 'stability',
                                                'sessions', 'session', 'sessiondata', 'sessiondiagram', 'recording', 'replay', 'play', 'names', 'scratch', 'errors', 'subscriptions', 'jacobian',
                                                'memory', 'threads', 'limits', 'nagumo', 'hopf_reduced', 'codim', 'switch', 'first_steps', 'orbit_start'])
args = ap.parse_args()
if args.shard:
    try:
        index, count = map(int, args.shard.split('/'))
    except ValueError:
        ap.error('--shard must be INDEX/COUNT')
    if not 1 <= index <= count <= len(args.sections):
        ap.error('--shard requires 1 <= INDEX <= COUNT <= number of sections')
    args.sections = args.sections[index - 1::count]
if args.list:
    print(' '.join(args.sections))
    sys.exit(0)
here = os.path.dirname(os.path.abspath(__file__))
LECAR = 'examples/ode/lecar.odex'
HEAVY = os.path.join(here, 'models', 'heavy.odex')
DIAGRAM = os.path.join(here, 'models', 'lecar_diagram.csv')  # the .snapx's diagram.csv of section_files
OLD_AUTOX = os.path.join(here, 'models', 'lecar_diagram.auto')  # XPPAUT's .auto of the same diagram
RECORDING = 'examples/recordings/lecar_autox.recx'
check = Checker()


def perf(name, detail=''):
    """A measurement, never pass/fail (W58: performance is for CI, not the
    program; the pass/fail stays on the result -- a reply arrived, a window
    closed, the process exited -- checked separately)."""
    print('perf: ' + name + ('  ' + detail if detail else ''))


def run_menu(s, key, timeout=60 * SLOW):
    """Run the registry kind corresponding to the fixture's historical autoX letter."""
    return run_any(s, key, timeout)


def grab_hopf(s):
    """Grab, Tab to the first label, take it; the events"""
    s.send(cmd='autox.grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='Tab')
    more, ask = s.collect(is_ask)
    evs += more
    s.send(cmd='answer', id=ask['id'], key='Return')
    more, _ = s.collect(is_done)
    return evs + more


class Diagram:
    """a view of the autoX diagram (view 0 unless named) as the client keeps it from "diagram" events"""

    def __init__(self, view='diagram_1'):
        self.axes, self.pts, self.view = None, [], view

    def apply(self, evs):
        for e in evs:
            if e.get('ev') != 'autox.diagram' or e.get('view') != self.view:
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
    """the autoxinfo events (docs/protocol.md "The autoX diagram as data")"""
    return [e for e in evs if e.get('ev') == 'autox.info']


def is_point(e):
    """a diagram event with new points: autoX computed some"""
    return e.get('ev') == 'autox.diagram' and e.get('op') == 'add'


def diagram_ops(evs):
    return [e['op'] for e in evs if e.get('ev') == 'autox.diagram']


def same_axes(axes, st, keys=('xmin', 'xmax', 'ymin', 'ymax', 'x0', 'y0', 'wid', 'hgt')):
    """the axes of the diagram data are state.auto (printed with %g)"""
    return all(abs(axes[k] - st[k]) <= 1e-5 * max(1, abs(st[k])) for k in keys)


# ---- diagram: autoX's diagram and info strip arrive as data, in few events ----

def section_diagram():
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_done)
    s.send(cmd='events.subscribe', events=['autox.info'])
    s.collect(is_done)
    open_autox(s)
    s.send(cmd='autox.redraw')
    evs, _ = s.collect(is_done)
    dg = Diagram().apply(evs)
    evs = run_menu(s, 's')

    # the diagram's data (docs/protocol.md "diagram"): the steady branch,
    # its points numbered in order, finite, its labels marked
    dg.apply(evs)
    adds = [e for e in evs if is_point(e)]
    runs = [r for e in adds for r in e['runs']]
    print('INFO steady run: %d points, %d add events, %d runs, data %d B' % (
        len(dg.pts), len(adds), len(runs), sum(len(json.dumps(e)) for e in evs if e.get('ev') == 'autox.diagram')))
    br = [p for p in dg.pts if abs(p['br']) == 1]
    check('the diagram data of a steady run: one branch, its points in order, finite',
          len(dg.pts) > 20 and len(br) == len(dg.pts)
          and all(b['pt'] == a['pt'] + 1 for a, b in zip(br, br[1:]) if not b['new'])
          and all(abs(p['x']) < 1e30 and abs(p['y']) < 1e30 for p in dg.pts),
          '%d points, %d on branch 1' % (len(dg.pts), len(br)))
    check('and its labels are marked', any(p['lab'] for p in dg.pts))
    check('a steady run arrives its points joined in runs', max((len(r['x']) for r in runs), default=0) >= 10,
          '%d events, longest run %d' % (len(adds), max((len(r['x']) for r in runs), default=0)))
    perf('steady run diagram events', '%d' % len(adds))
    st = [e for e in evs if is_state(e)][-1]['autox']
    check('the diagram axes are the autoX ranges', same_axes(dg.axes, st), 'axes %s state %s' % (dg.axes, st))
    got = infos(evs)
    check('the run ends with its last point\'s stability circle (autoxinfo)',
          bool(got) and got[-1]['stab'] is not None and got[-1]['info'] is None, str(got[-1:]))

    s.send(cmd='autox.redraw')
    evs, _ = s.collect(is_done)
    print('INFO reDraw: %d events %s' % (len(evs), sorted({e.get('ev') for e in evs})))
    check('a reDraw arrives in a few events', len(evs) <= 10, '%d events' % len(evs))
    check('a reDraw of the same diagram sends its axes, not its points again', diagram_ops(evs) == ['axes'],
          str(diagram_ops(evs)))

    s.send(cmd='autox.grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='ArrowRight')
    evs, ask = s.collect(is_ask)
    got = infos(evs)
    check('a grab step shows the circle and the point at once (autoxinfo before the next ask)',
          len(got) == 1 and got[0]['info'] is not None and got[0]['info']['point'] == 1
          and got[0]['stab'] is not None, str(got)[:200])
    check('a grab step does not clear the diagram', not diagram_ops(evs), str(diagram_ops(evs)))

    # Enter (FINE) used to redraw_diagram() the whole thing just to be rid of
    # the XOR cursor (autox_grab_end, core/autox_nox.c traverse_diagram): the
    # page keeps the cursor itself, so taking a point should be as cheap as
    # any other grab step, not a full repaint.
    s.send(cmd='answer', id=ask['id'], key='Return')
    take_evs, _ = s.collect(is_done)
    check('taking a grab point (Enter) does not clear the diagram',
          'reset' not in diagram_ops(take_evs) and 'add' not in diagram_ops(take_evs), str(diagram_ops(take_evs)))
    check('taking a grab point (Enter) ends in a few events', len(take_evs) < 10, '%d events' % len(take_evs))

    # Esc ends a grab without taking anything
    s.send(cmd='autox.grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='ArrowRight')
    esc_evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='Escape')
    more, end = s.collect(is_done)
    check('Esc from a grab ends it, the diagram untouched',
          end is not None and not diagram_ops(more) and not any(is_ask(e) for e in more), str(more)[:200])

    # Axes/hI-lo draws the diagram again by itself (T21: no reDraw needed):
    # the client ends up with every point in the new quantities (here the
    # same values, a steady branch's max and min being one, so only the
    # axes go out); a Fit after that only sends the new axes
    n = len(dg.pts)
    s.send(cmd='autox.configure_axes', plot_type='maximum_and_minimum')
    evs,_ = s.collect(is_done)
    dg.apply(evs)
    check('Axes/hI-lo draws the diagram again at once: every point, the new plot type',
          'axes' in diagram_ops(evs) and len(dg.pts) == n and dg.axes['plot'] == 'maximum_and_minimum',
          '%s, %d points of %d' % (diagram_ops(evs), len(dg.pts), n))
    s.send(cmd='autox.fit')
    evs,_ = s.collect(is_done)
    dg.apply(evs)
    st = [e for e in evs if is_state(e)][-1]['autox']
    check('Axes/Fit sends the fitted axes only', diagram_ops(evs) == ['axes'] and same_axes(dg.axes, st),
          '%s axes %s state %s' % (diagram_ops(evs), dg.axes, st))
    s.close()


# ---- grab: auto grab by label or type+index (W64, issue #112) -------------

def section_grab():
    """A periodic run from the diagram's Hopf point, grabbed by label with
    no ask, gives the same diagram points as grabbing it interactively (by
    its index, as clicking it does); so does grabbing it by type and index
    ("the 1st HB"). An unknown label, or a type/index with no such point,
    is refused and changes nothing."""

    def hopf_of(s):
        """lecar's "hopf" parameter set, a steady run from it; the Hopf
        point's diagram index and label (None, None if it has none)"""
        dg = Diagram().apply(hopf_steady(s))
        r1 = len(dg.pts)
        evs = run_any(s, 's')
        dg.apply(evs)
        syms = {lab: sym for e in evs if is_point(e) for r in e['runs'] for i, lab, sym in r.get('lab', [])}
        hb = next((i for i in range(r1, len(dg.pts)) if syms.get(dg.pts[i]['lab']) == 'hopf'), None)
        return dg, (hb, dg.pts[hb]['lab']) if hb is not None else (None, None)

    # interactive path: the same setup, grabbed by its diagram index (as a
    # diagram click takes the nearest point: docs/protocol.md "Grab by
    # point"), then its periodic branch
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_done)
    s.send(cmd='events.subscribe', events=['autox.info'])
    s.collect(is_done)
    dg, (hb, label) = hopf_of(s)
    check('the steady branch has a Hopf point to grab', hb is not None, '')
    if hb is None:
        s.close()
        return
    grab_point(s, hb, take=True)
    n = len(dg.pts)
    dg.apply(run_any(s, 'p'))
    interactive = list(dg.pts[n:])
    check('the periodic run from the interactive grab computes points', len(interactive) > 2,
          '%d points' % len(interactive))
    s.close()

    # scripted path: the same setup, grabbed by label with no ask
    s2 = Server(args.server, LECAR, verbose=args.v)
    s2.collect(is_done)
    dg2, (hb2, label2) = hopf_of(s2)
    check('the second server\'s run finds the same label', label2 == label, '%s vs %s' % (label2, label))
    s2.send(cmd='autox.grab', label=label)
    evs, _ = s2.collect(is_done)
    check('grab by label asks nothing', not any(is_ask(e) for e in evs), str([e.get('ev') for e in evs]))
    n = len(dg2.pts)
    dg2.apply(run_any(s2, 'p'))
    scripted = list(dg2.pts[n:])
    check('a periodic run from "auto grab label" gives the same diagram points as grabbing it interactively',
          interactive == scripted, 'interactive %d points, scripted %d points' % (len(interactive), len(scripted)))

    # an unknown label is refused; nothing changes (no diagram event, no ask)
    s2.send(cmd='autox.grab', label=999999)
    evs, _ = s2.collect(is_done)
    errs = [e.get('error') for e in evs if e.get('ev') in ('session.message', 'done') and 'error' in e]
    check('an unknown label is refused (a message error)', bool(errs), str(errs))
    check('an unknown label changes nothing (no diagram event, no ask)',
          not diagram_ops(evs) and not any(is_ask(e) for e in evs), str([e.get('ev') for e in evs]))
    s2.close()

    # by type and index: "the 1st HB" is the same point as the label grabbed
    s3 = Server(args.server, LECAR, verbose=args.v)
    s3.collect(is_done)
    dg3, _ = hopf_of(s3)
    s3.send(cmd='autox.grab', type='hopf', index=1)
    evs, _ = s3.collect(is_done)
    check('grab by type and index asks nothing', not any(is_ask(e) for e in evs), str([e.get('ev') for e in evs]))
    n = len(dg3.pts)
    dg3.apply(run_any(s3, 'p'))
    by_type = list(dg3.pts[n:])
    check('grab by type and index (the 1st HB) gives the same diagram points as grab by label',
          by_type == scripted, '%d points vs %d' % (len(by_type), len(scripted)))

    # an out-of-range index is refused too
    s3.send(cmd='autox.grab', type='hopf', index=99)
    evs, _ = s3.collect(is_done)
    errs = [e.get('error') for e in evs if e.get('ev') in ('session.message', 'done') and 'error' in e]
    check('an out-of-range type/index is refused too', bool(errs), str(errs))
    s3.close()


# ---- input: end of input, pipelined commands, no polling -------------------

def section_input():
    # stdin from the null device (NUL on Windows, which GetFileType calls a
    # character device like a console: W18), a closed pipe and an empty file;
    # a few runs each, since a wrong console attach hung only some of them
    hung = []
    times = []
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
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10 * SLOW)
                times.append('%s %.2f s' % (how, time.monotonic() - t))
            except subprocess.TimeoutExpired:
                hung.append(how)
            if how == 'closed pipe':
                os.close(rfd)
    perf('--server exits at end of input', ', '.join(times))
    check('--server exits at end of input (null device, closed pipe, empty file)', not hung,
          'no exit in %g s: %s' % (10 * SLOW, ', '.join(hung)))

    s = Server(args.server, LECAR, verbose=False)
    s.collect(is_done)
    n = 1000
    s.proc.stdin.write(''.join(json.dumps({'cmd': 'value.set_parameters', 'values': {'iapp': i / 10000.0}}) + '\n'
                               for i in range(n)))
    s.proc.stdin.flush()
    dones = 0
    while dones < n:
        evs, e = s.collect(is_done, timeout=20 * SLOW)
        if e is None:
            break
        dones += 1
    s.send(cmd='session.get_state')
    evs, st = s.collect(lambda e: e.get('ev') == 'session.state')
    s.collect(is_done)
    iapp = st and dict(st['pars']).get('iapp')
    check('%d pipelined commands each end in done, in order' % n, dones == n and iapp == (n - 1) / 10000.0,
          'dones %d, iapp %s' % (dones, iapp))

    if sys.platform != 'win32':
        seen = {}
        check('an idle server blocks every thread waiting for input', wait_until(lambda: input_blocked(s.proc.pid, seen)),
              'last thread states read:\n' + seen.get('states', ''))
    s.close()


def input_blocked(pid, seen):
    """The OS's sleeping state proves the idle reader blocks, without a CPU/time budget.
    seen['states'] keeps what was last read, for the check's failure."""
    tasks = '/proc/%d/task' % pid
    if os.path.isdir(tasks):
        states = []
        for task in os.listdir(tasks):
            with open(os.path.join(tasks, task, 'stat')) as f:
                states.append(f.read().rsplit(')', 1)[1].split()[0])
        seen['states'] = ' '.join(states)
        return bool(states) and all(state == 'S' for state in states)
    out = subprocess.run(['ps', '-M', '-p', str(pid)], capture_output=True, text=True, check=True)
    states = macos_thread_states(out.stdout)
    # Darwin also calls a sleeping thread I (idle); both prove it blocks.
    seen['states'] = out.stdout
    return bool(states) and all(state.startswith(('S', 'I')) for state in states)


# ---- abort: a long autoX run stops at once, and can be continued ------------

def periodic_run(s):
    """from the Hopf point of heavy.odex, start a periodic run; returns once
    two points have been computed (diagram data), with the time between them"""
    s.collect(is_done)
    s.send(cmd='events.subscribe', events=['autox.info'])
    s.collect(is_done)
    open_autox(s)
    run_menu(s, 's')
    grab_hopf(s)
    run_periodic(s)
    stamps = []
    while len(stamps) < 2:
        evs, e = s.collect(lambda e: is_point(e) or is_done(e) or is_ask(e), timeout=60 * SLOW)
        if e is None or not is_point(e):
            return None
        stamps.append(e['_t'])
    return stamps[1] - stamps[0]


def run_periodic(s):
    """Run periodic orbits from the grabbed point, without waiting for the run to end"""
    label = s.grabbed_label
    s.send(cmd='autox.run', kind='periodic', **({'label': label} if label else {}))


def section_abort():
    s = Server(args.server, HEAVY, verbose=args.v)
    gap = periodic_run(s)
    check('the heavy periodic run is computing points', gap is not None)
    if gap is None:
        s.close()
        return
    # W106: autoX's settings sent during its run (a setting) are kept for
    # after it, each a command of its own then; the data subscription (a
    # view) runs after them
    s.send(cmd='autox.configure_numerics', **{'max_steps': 1500})
    s.send(cmd='events.subscribe', events=['autox.info', 'autox.settings'])
    # periodic_run already observed two diagram points: the run is computing.
    # Abort now, without guessing the duration of a Newton step.
    t = s.send(cmd='run.stop')
    evs, e = s.collect(is_done, timeout=120 * SLOW)
    took = (e['_t'] if e else time.monotonic()) - t
    check('Abort stops a periodic run (reaches done)', e is not None,
          'abort->done %.2f s (a point takes %.2f s)' % (took, gap))
    perf('abort stops a periodic run -> done', '%.2f s (a point takes %.2f s)' % (took, gap))
    set_evs, _ = s.collect(is_done, timeout=10 * SLOW)
    data_evs, _ = s.collect(is_done, timeout=10 * SLOW)
    errs = [x.get('error') for x in evs + set_evs if x.get('ev') == 'session.message' and 'error' in x]
    st = [x for x in data_evs if x.get('ev') == 'autox.settings']
    check('W106: autoX settings sent during its run: no error, applied after it with their own done',
          not errs and st and st[-1]['numerics']['max_steps'] == 1500, '%s %s' % (errs, st and st[-1]['numerics']))

    # the run ends on an end point (EP, not MX: no convergence), which is
    # where it can be continued from
    s.send(cmd='autox.grab')
    evs, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], key='End')
    evs, ask = s.collect(is_ask)
    got = [e['info'] for e in infos(evs) if e['info']]
    last = got[-1] if got else {}
    print('INFO last point after abort: %s' % json.dumps(last)[:200])
    check('an aborted run ends on an EP point', last.get('sym') == 'end', str(last)[:200])
    s.send(cmd='answer', id=ask['id'], key='Return')
    taken, _ = s.collect(is_done)
    check('the server is still alive after an abort', s.alive())

    # a second run continues the branch from that point
    run_periodic(s)
    n = 0
    while n < 2:
        evs, e = s.collect(lambda e: is_point(e) or is_done(e) or is_ask(e), timeout=60 * SLOW)
        if e is None or not is_point(e):
            break
        n += 1
    check('a run from the end point of the aborted run computes new points', n == 2, '%d points' % n)
    s.send(cmd='run.stop')
    s.collect(is_done, timeout=120 * SLOW)
    check('the server is still alive after the second run', s.alive())

    # Close during a run: the autoX window goes within a second
    run_periodic(s)
    s.collect(is_point, timeout=60 * SLOW)
    t = s.send(cmd='run.stop')
    s.send(cmd='window.close', window='diagram_1')
    evs, e = s.collect(lambda e: e.get('ev') == 'window.destroyed' and e.get('win') == 'diagram_1',
                       timeout=120 * SLOW)
    took = (e['_t'] if e else time.monotonic()) - t
    check('Abort then Close during a run closes the autoX window', e is not None, '%.2f s' % took)
    perf('abort+close -> destroy', '%.2f s' % took)
    s.collect(is_done)

    # Quit during a run: the process ends on its own, not killed
    open_autox(s)
    run_periodic(s)
    s.collect(is_point, timeout=60 * SLOW)
    t = s.send(cmd='session.quit', save=False)
    try:
        s.proc.wait(timeout=60 * SLOW)
        exited = True
    except subprocess.TimeoutExpired:
        s.proc.kill()
        exited = False
    took = time.monotonic() - t
    check('Quit during a run exits the process on its own', exited, '%.2f s' % took)
    perf('quit during a run -> exit', '%.2f s' % took)
    s.close()


# ---- files: the saved diagram of lecar is what it always was --------------

def save_diagram(s, name, written=None):
    """File > Save session as of name; the members of the .snapx it
    wrote, written (name when not given), by name ({} if not written)"""
    import zipfile
    s.send(cmd='session.save_as', file=name, confirm=True)
    s.answer_asks(is_done, {'choice':lambda e:{'key':'y'}})
    path = os.path.join(s.run, written or name)
    if not os.path.exists(path) or not zipfile.is_zipfile(path):
        return {}
    z = zipfile.ZipFile(path)
    return {n: z.read(n) if n.endswith('.npz') else z.read(n).decode() for n in z.namelist()}


def in_model_folder(s, path):
    """a file argument is a base name in the model's folder: path, copied there when it is elsewhere"""
    if os.path.dirname(path) not in ('', s.run):
        shutil.copy(path, s.run)
    return os.path.basename(path)


def load_diagram(s, path):
    """File > Open session of path (answering a reset's question yes, a model
    switch's "save first?" don't): the events"""
    s.send(cmd='session.open', file=in_model_folder(s, path))
    return s.answer_asks(is_done, {'choice':lambda e:{'key':'d' if e.get('keys') == 'sd' else 'y'}})[0]


def import_diagram(s, path):
    """Import a foreign XPPAUT diagram through its explicit menu item."""
    s.send(cmd='autox.import_diagram', file=in_model_folder(s, path))
    return s.answer_asks(is_done, {'choice':lambda e:{'key':'d' if e.get('keys') == 'sd' else 'y'}})[0]


def open_file(s, path):
    """{"cmd": "session.open_model"} of path, a model switch's "save first?" answered don't: the events"""
    s.send(cmd='session.open_model', file=path)
    evs, e = s.answer_asks(is_done, {'choice': lambda e: {'key': 'd'}})
    return evs


def hello_title(evs):
    h = [e for e in evs if e.get('ev') == 'hello']
    return h[-1].get('title', '') if h else None


def lecar_diagram(s, events=None):
    """steady state, grab the first label, a periodic branch, File/Save
    diagram; returns the .snapx's diagram.csv (None if not written); with
    a list as events, the autoxinfo and diagram events are appended to it"""
    s.collect(is_done)
    if events is not None:
        s.send(cmd='events.subscribe', events=['autox.info'])
        s.collect(is_done)
    open_autox(s)
    evs = run_menu(s, 's')
    evs += grab_hopf(s)
    evs += run_menu(s, 'p', timeout=120 * SLOW)
    if events is not None:
        events += evs
    return save_diagram(s, 'diagram.snapx').get('autox/diagram.csv')


def lines_close(a, b, rtol=1e-6, atol=1e-9):
    """Keep the historical default lecar fixture's numerical acceptance."""
    ta, tb = a.replace(';',' ').split(), b.replace(';',' ').split()
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
    # Convert the historical fixture's storage only; keep its original tolerance.
    lines = [l.replace(',', ' ') for l in got.splitlines()] if got else []
    check('File/Save diagram writes the diagram', len(lines) > 50, '%d lines' % len(lines))
    if got is None:
        return
    if not os.path.exists(DIAGRAM):
        with open(DIAGRAM, 'w', newline='') as f:
            f.write(got)
        print('INFO wrote %s (the reference diagram)' % DIAGRAM)
        return
    with open(DIAGRAM, newline='') as f:
        want = [l.replace(',', ' ') for l in f.read().splitlines()]
    diff = next((i for i, (a, b) in enumerate(zip(lines, want)) if not lines_close(a,b,atol=1e-6)), None)
    check('the saved diagram of lecar is unchanged',
          len(lines) == len(want) and diff is None,
          'first difference at line %s; %d vs %d lines' % (diff, len(lines), len(want)))


# ---- jacobian: autoX with the exact Jacobian (W260) -------------------------

def labelled_points(csv_text):
    """a saved diagram.csv's labelled points: (branch, type, label, the
    continuation parameter, the period), in order"""
    points = []
    for row in diagram_rows(csv_text.encode('utf-8')):
        if int(row['lab']) != 0:
            points.append((int(row['ibr']), row['itp'], int(row['lab']), float(row['continued_parameter']), float(row['per'])))
    return points


def section_jacobian():
    """lecar's steady states and the periodic branch from its Hopf point
    (lecar_diagram), with XPPAUT's differences and with @ jacobian=exact
    (autoX's JAC=1: func gives dfdu and dfdp): the same labelled points, of
    the same types, the parameter and period alike to 1e-5; the numbers
    are not the same bits (differences are not exact)"""
    model_dir = tempfile.mkdtemp(prefix='xppjacobian')
    try:
        exact = os.path.join(model_dir, 'lecar.odex')
        with open(LECAR) as f:
            text = f.read()
        exact_text = text.replace('integration: {', 'integration: { jacobian.kind: exact,', 1)
        assert exact_text != text, 'lecar.odex has no integration group to give jacobian.kind: exact'
        with open(exact, 'w') as f:
            f.write(exact_text)
        diagrams, events = {}, {}
        for name, ode in (('difference', LECAR), ('exact', exact)):
            s = Server(args.server, ode, verbose=args.v)
            start = time.monotonic()
            events[name] = []
            diagrams[name] = lecar_diagram(s, events[name])
            perf('jacobian %s: lecar steady and periodic' % name, '%.2f s' % (time.monotonic() - start))
            s.close()
        check('jacobian: both runs save their diagrams', all(diagrams.values()))
        if not all(diagrams.values()):
            return
        d, e = labelled_points(diagrams['difference']), labelled_points(diagrams['exact'])
        same = len(d) == len(e) and all(a[:3] == b[:3] and all(abs(x - y) <= 1e-5 * max(1, abs(x)) for x, y in zip(a[3:], b[3:]))
                                        for a, b in zip(d, e))
        check('jacobian: the exact Jacobian finds the same labelled points (branch, type, label; parameter and period to 1e-5)',
              same and len(d) > 3, '%s\n%s' % (d, e))
        check('jacobian: and is its own computation (the diagrams are not the same bits)',
              diagrams['difference'] != diagrams['exact'])
        # W278: every run of a diagram says how its Jacobian was formed: in the diagram data, the info
        # strip of the grabbed point, and the saved diagram.csv's jacobian column (0 difference, 1 exact)
        for name, code in (('difference', '0'), ('exact', '1')):
            runs = {r['jac'] for e in events[name] if is_point(e) for r in e['runs']}
            shown = {i['info']['jac'] for i in infos(events[name]) if i.get('info')}
            rows = diagram_rows(diagrams[name].encode('utf-8'))
            column = {r['jacobian'] for r in rows}
            check('jacobian: a %s diagram says so in its runs, the grabbed info and the saved diagram.csv' % name,
                  runs == {name} and shown == {name} and column == {code}, '%s %s %s' % (runs, shown, column))
    finally:
        shutil.rmtree(model_dir, ignore_errors=True)


def section_threads():
    """W267: autoX's thread count (a program preference, set by `auto` `set` `threads`, kept in a
    config folder of its own) never changes a result: lecar's steady and periodic branches, run with 1
    thread and with 2 (as many as the computer has, if fewer), save the same diagram.csv to the bit"""
    config = tempfile.mkdtemp(prefix='xppthreads')
    try:
        diagrams = {}
        for want in (1, 2):
            s = Server(args.server, LECAR, env={'XPP_CONFIG_DIR': os.path.join(config, str(want))}, verbose=args.v)
            s.collect(is_done)
            s.send(cmd='events.subscribe', events=['autox.settings'])
            evs, _ = s.collect(is_done)
            most = [e for e in evs if e.get('ev') == 'autox.settings'][-1]['threads_max']
            count = min(want, most)
            s.send(cmd='preferences.configure', threads=count)
            evs, _ = s.collect(is_done)
            errors = [e.get('error') for e in evs if e.get('ev') == 'session.message' and e.get('error')]
            shown = [e for e in evs if e.get('ev') == 'autox.settings'][-1]['threads']
            check('threads: %d thread(s) set and shown' % count, not errors and shown == count, '%s %s' % (errors, shown))
            diagrams[want] = lecar_diagram(s)
            s.close()
        check('threads: both runs save their diagrams', all(diagrams.values()))
        check('threads: 1 thread and 2 give the same diagram, to the bit', diagrams[1] == diagrams[2])
    finally:
        shutil.rmtree(config, ignore_errors=True)


# ---- csv: File/eXport CSV (W26, issue #42, core/csv_export.cpp) ----------

def lecar_csv(s):
    """steady state, grab the first label, a periodic branch, File/eXport
    CSV; returns (diagram csv text, eigenvalues csv text, the diagram's
    data as the page gets it), the texts None if not written"""
    s.collect(is_done)
    s.send(cmd='events.subscribe', events=['autox.info'])
    s.collect(is_done)
    open_autox(s)
    evs = run_menu(s, 's')
    grab_hopf(s)
    evs += run_menu(s, 'p', timeout=120 * SLOW)
    dg = Diagram().apply(evs)
    s.send(cmd='autox.export_csv', file='lecar.csv')
    s.collect(is_done)
    dpath = os.path.join(s.run, 'lecar.csv')
    epath = os.path.join(s.run, 'lecar_eig.csv')
    if not os.path.exists(dpath) or not os.path.exists(epath):
        return None, None, dg
    with open(dpath, newline='') as f:
        d = f.read()
    with open(epath, newline='') as f:
        e = f.read()
    return d, e, dg


def section_csv():
    import csv, io
    from collections import Counter
    home = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    d, e, dg = lecar_csv(s)
    s.close()
    shutil.rmtree(home, ignore_errors=True)
    check('File/eXport CSV writes the diagram and eigenvalues files', d is not None and e is not None)
    if d is None or e is None:
        return
    check('autoX diagram and eigenvalues CSV use LF on every platform',
          chr(13) not in d and chr(13) not in e and d.endswith(chr(10)) and e.endswith(chr(10)))
    drows = list(csv.DictReader(io.StringIO(d)))
    erows = list(csv.DictReader(io.StringIO(e)))
    check('the diagram CSV has a header of names',
          bool(drows) and {'branch', 'point', 'type', 'label', 'stability', 'period'} <= set(drows[0].keys()),
          str(list(drows[0].keys()) if drows else []))
    check('W339: diagram CSV exports every model parameter from its branch',
          bool(drows) and 'parameter.iapp' in drows[0] and 'parameter.phi' in drows[0]
          and all(r['parameter.iapp']==r['param1'] for r in drows))
    # the steady branch against the diagram data the page gets (the periodic
    # run's points may repeat, which the page's data draws once): the same
    # points, each row's param1 (lecar's autoX x axis) its point's x
    csv1 = [(int(r['branch']), int(r['point']), float(r['param1'])) for r in drows if r['branch'] == '1']
    dg1 = [(p['br'], p['pt'], p['x']) for p in dg.pts if abs(p['br']) == 1]
    close = lambda a, b: a[1] == b[1] and abs(a[2] - b[2]) <= 1e-6 * max(1, abs(b[2]))
    check('the diagram CSV has the steady branch as the diagram data has it',
          len(csv1) == len(dg1) > 5 and all(close(a, b) for a, b in zip(csv1, dg1)),
          '%d rows, %d points; first difference %s' % (
              len(csv1), len(dg1), next(((a, b) for a, b in zip(csv1, dg1) if not close(a, b)), None)))
    check('branch and point numbers are positive (the stability column holds what autoX signs them by)',
          all(int(r['branch']) > 0 and int(r['point']) > 0 for r in drows))
    check('the eigenvalues CSV has a header of names',
          bool(erows) and set(erows[0].keys()) == {'branch', 'point', 'index', 're', 'im', 'kind'},
          str(list(erows[0].keys()) if erows else []))
    dkeys = {(r['branch'], r['point']) for r in drows}
    ekeys = {(r['branch'], r['point']) for r in erows}
    check('the eigenvalues CSV is keyed by branch and point, joining the diagram CSV', ekeys and ekeys <= dkeys,
          '%d of %d point keys unmatched' % (len(ekeys - dkeys), len(ekeys)))
    counts = Counter((r['branch'], r['point']) for r in erows)
    node_n = next(iter(counts.values()), 0)
    check('every diagram point has the same number of eigenvalues/multipliers (NODE)',
          node_n > 0 and all(v == node_n for v in counts.values()), 'node=%d' % node_n)
    kinds = {r['kind'] for r in erows}
    check('kind marks eigenvalue vs multiplier', kinds <= {'eigenvalue', 'multiplier'} and kinds, str(kinds))


# ---- stability: a point's eigenvalues are its own (W15, autox_stability.h) --

def step(s, **c):
    s.send(**c)
    return s.collect(lambda e: is_done(e) or is_ask(e), timeout=30 * SLOW)


def hopf_steady(s):
    """lecar's "hopf" parameter set, its fixed point as the IC (as
    examples/recordings/lecar_autox.recx does), autoX; returns the events"""
    evs = []
    for c in ({'cmd': 'set.apply', 'name': 'hopf', 'scope': 'all'}, {'cmd': 'equilibrium.find'}, {'cmd': 'initial_conditions.set_from_equilibrium'},
              {'cmd': 'autox.open'}):
        evs += step(s, **c)[0]
    return evs


def grab_point(s, i, take=False):
    """Grab, go to point i of the diagram data; take it (Return) or leave
    (Escape); returns the autoxinfo there"""
    s.send(cmd='autox.grab')
    evs, ask = s.collect(is_ask)  # the grab starts on the first point: its autoxinfo is here
    if take:
        s.send(cmd='answer', id=ask['id'], point=i, key='Return')
        more, _ = s.collect(is_done)
    else:
        s.send(cmd='answer', id=ask['id'], point=i)
        more, ask = s.collect(is_ask)
        s.send(cmd='answer', id=ask['id'], key='Escape')
        s.collect(is_done)
    got = [e for e in infos(evs + more) if e.get('info')]
    return got[-1] if got else None


def circle(info):
    return info['stab']['circle'] if info and info.get('stab') else None


def zeros(c):
    return c is not None and all(v == 0 for pair in c for v in pair)


def section_stability():
    home = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    s.collect(is_done)
    s.send(cmd='events.subscribe', events=['autox.info'])
    s.collect(is_done)
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
    check('stability: a steady run\'s first point is not computed (autoxinfo circle all zeros)',
          first is not None and first['info']['pt'] == 1 and zeros(circle(first)), str(first)[:300])
    check('stability: its second point has its own values',
          second is not None and second['info']['pt'] == 2 and circle(second) and not zeros(circle(second)),
          str(second)[:300])

    # 2: a periodic branch from the Hopf point: a change of kind
    hb = label('hopf', r1, len(dg.pts))
    check('stability: the steady branch has a Hopf point', hb is not None, str(syms))
    if hb is None:
        s.close()
        return
    hb_info = grab_point(s, hb, take=True)
    # Test inheritance from a computed endpoint. A long near-homoclinic run
    # can disable stability detection (inaccurate unit multiplier), leaving
    # its endpoint legitimately uncomputed; W326's restore changes that path.
    evs, _ = step(s, cmd='autox.configure_numerics', **{'max_steps': 10, 'output_every': 5, 'step_size': 0.01, 'max_step_size': 0.01})
    check('stability: the short periodic restart fixture is configured', not errors_of(evs), str(errors_of(evs)))
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


# ---- sessions: two servers on one model keep their autoX files apart --------

def section_sessions():
    home = tempfile.mkdtemp(prefix='xpphome')
    a = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    b = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    da = lecar_diagram(a)
    db = lecar_diagram(b)
    check('two sessions on one model both save their diagram', da is not None and db is not None)
    check('and the two diagrams agree', da == db)
    # a grabs the label its own run wrote, after b has run too
    a.send(cmd='autox.grab')
    evs, ask = a.collect(is_ask)
    a.send(cmd='answer', id=ask['id'], key='Tab')
    evs, ask = a.collect(is_ask)
    a.send(cmd='answer', id=ask['id'], key='Return')
    a.collect(is_done)
    evs = run_menu(a, 'e', timeout=120 * SLOW)
    msgs = ' '.join(str(e.get('text', '')) for e in evs if e.get('ev') == 'session.message')
    check('a session still extends from its own orbit', 'nan' not in msgs.lower() and a.alive(), msgs[:200])
    runs = [a.run, b.run]
    # each session has an autoX scratch directory of its own, gone at exit
    autox_dirs = a.autox_dirs() + b.autox_dirs()
    check('each session has its own autoX scratch directory, and no run left its own folder (W595)', len(autox_dirs) == 2, str(autox_dirs))
    a.close()
    b.close()
    check('the scratch directories are gone once the sessions exit',
          not any(os.path.isdir(d) for d in autox_dirs), str(autox_dirs))
    left = os.listdir(home)
    check('nothing is written to HOME', left == [], str(left))
    shutil.rmtree(home, ignore_errors=True)


# ---- session: "cmd":"session" save/load picks a long autoX run back up -----

def open_session(s, name):
    """session load of a .snapx, answering Open's "save first?" with d: the events"""
    s.send(cmd='session.open', file=name)
    evs = []
    while True:
        got, e = s.collect(lambda e: is_ask(e) or is_done(e), timeout=20 * SLOW)
        evs += got
        if e is None or is_done(e):
            return evs
        s.send(cmd='answer', id=e['id'], key='d')


def section_session():
    import zipfile
    home1 = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LECAR, env={'HOME': home1}, verbose=args.v)
    s.collect(is_done)
    open_autox(s)
    run_menu(s, 's')
    grab_hopf(s)
    run_menu(s, 'p', timeout=120 * SLOW)

    s.send(cmd='session.save_as', file='s1')
    evs, e = s.collect(is_done, timeout=20 * SLOW)
    st = [x for x in evs if x.get('ev') == 'session.state']
    saved = st[-1] if st else None
    sess = saved.get('session') if saved else None
    check('session save reports the session file', sess == {'file': 's1.snapx'}, str(sess))
    snap_path = os.path.join(s.run, 's1.snapx')
    members = zipfile.ZipFile(snap_path).namelist() if os.path.exists(snap_path) else []
    check('session save writes s1.snapx with the model, the state file and autoX\'s files in it',
          'model/lecar.odex' in members and 'session.odex' in members and 'autox/diagram.csv' in members, str(members))
    pars1 = dict(saved['pars']) if saved else {}

    # a NEW server, in a new directory, with only the session file copied in
    home2 = tempfile.mkdtemp(prefix='xpphome')
    s2 = Server(args.server, LECAR, env={'HOME': home2}, verbose=args.v)
    s2.collect(is_done)
    if os.path.exists(snap_path): shutil.copy(snap_path, s2.run)
    # and a server of another model, with no lecar.odex in its folder
    home3 = tempfile.mkdtemp(prefix='xpphome')
    s3 = Server(args.server, HEAVY, env={'HOME': home3}, verbose=args.v)
    s3.collect(is_done)
    if os.path.exists(snap_path): shutil.copy(snap_path, s3.run)
    s.close()
    shutil.rmtree(home1, ignore_errors=True)

    evs = open_session(s2, 's1')
    st = [x for x in evs if x.get('ev') == 'session.state']
    loaded = st[-1] if st else None
    sess2 = loaded.get('session') if loaded else None
    check('session load reports the file opened', bool(sess2) and sess2.get('file', '').endswith('s1.snapx'), str(sess2))
    pars2 = dict(loaded['pars']) if loaded else {}
    check("session load restores the first session's parameters",
          bool(pars1) and pars1 == pars2, 'saved %s loaded %s' % (pars1, pars2))
    check('session load opens the autoX window',
          any(x.get('ev') == 'window.created' and x.get('win') == 'diagram_1' for x in evs))
    check('session load sends the diagram', any(is_point(e) for e in evs))

    # grab the first label and Run extending the branch, as section_sessions
    # does for a session's own orbit: new points, and no NaN
    s2.send(cmd='autox.grab')
    evs, ask = s2.collect(is_ask)
    s2.send(cmd='answer', id=ask['id'], key='Tab')
    evs, ask = s2.collect(is_ask)
    s2.send(cmd='answer', id=ask['id'], key='Return')
    s2.collect(is_done)
    evs = run_menu(s2, 'e', timeout=60 * SLOW)
    msgs = ' '.join(str(e.get('text', '')) for e in evs if e.get('ev') == 'session.message')
    check('extending the loaded branch computes new points',
          any(is_point(e) for e in evs) and s2.alive(), str(evs)[:200])
    check('extending the loaded branch draws no NaN message', 'nan' not in msgs.lower(), msgs[:200])
    s2.close()
    shutil.rmtree(home2, ignore_errors=True)

    # no lecar.odex where it opens: the saved model loads, with the session
    evs = open_session(s3, 's1')
    st = last_state(evs)
    check('session load where no .ode is: the saved lecar loads, its title names the session file',
          'lecar.odex (saved in s1.snapx)' in (hello_title(evs) or ''), str(hello_title(evs)))
    check('session load where no .ode is: the parameters and the diagram are the saved ones',
          st is not None and dict(st['pars']) == pars1 and any(is_point(e) for e in evs), str(st and st['pars']))
    check('session load writes nothing beside the file', sorted(os.listdir(s3.run)) == ['heavy.odex', 's1.snapx'],
          str(os.listdir(s3.run)))
    # a session file without its model: refused, the model open stays
    import zipfile as zf
    if os.path.exists(os.path.join(s3.run, 's1.snapx')):
        z = zf.ZipFile(os.path.join(s3.run, 's1.snapx'))
        with zf.ZipFile(os.path.join(s3.run, 'nomodel.snapx'), 'w') as out:
            for n in z.namelist():
                if not n.startswith('model/'):
                    out.writestr(n, z.read(n))
    evs = open_session(s3, 'nomodel')
    st2 = last_state(evs)
    check('session load of a file without its model: an error says so, nothing changes',
          'model is missing' in messages(evs) and hello_title(evs) is None and st2 is not None
          and dict(st2['pars']) == pars1, messages(evs)[:300])
    s3.close()
    shutil.rmtree(home3, ignore_errors=True)

    # a model with a file table (named by its whole path): the table goes
    # with it and comes back, from the session file alone
    import math
    # macOS's /var temporary alias must match the process's canonical cwd.
    folder = os.path.realpath(tempfile.mkdtemp(prefix='xpptab'))
    table = os.path.join(folder, 'w.tab').replace(os.sep, '/')
    with open(table, 'w') as f:
        f.write(chr(10).join(['5', '0', '4', '0', '1', '4', '9', '16', '']))
    tab_ode = os.path.join(folder, 'tab.odex')
    with open(tab_ode, 'w') as f:
        f.write(versioned(chr(10).join(['table w "' + table + '"', 'par a=1.5', "x'=w(a)-x", 'init x=0', 'options { integration: { total_time: 4 } }', ''])))
    # run in the model's own folder: a table outside it would be a question (W175, W286)
    s4 = Server(args.server, tab_ode, verbose=args.v, run=folder)
    s4.collect(is_done)
    s4.send(cmd='session.save_as', file='tab')
    s4.collect(is_done, timeout=20 * SLOW)
    tab_snap = os.path.join(s4.run, 'tab.snapx')
    names = zf.ZipFile(tab_snap).namelist() if os.path.exists(tab_snap) else []
    check('session save of a model with a file table carries the table',
          'model/tab.odex' in names and 'model/' + table in names, str(names))
    s5 = Server(args.server, LECAR, verbose=args.v)
    s5.collect(is_done)
    if names: shutil.copy(tab_snap, s5.run)
    s4.close()
    shutil.rmtree(folder, ignore_errors=True)
    evs = open_session(s5, 'tab')
    s5.send(cmd='run.initial')
    s5.collect(is_done, timeout=30 * SLOW)
    s5.send(cmd='data.export', target='table', format='csv', file='tab.csv')
    s5.collect(is_done)
    csv_path = os.path.join(s5.run, 'tab.csv')
    rows = [l.strip() for l in open(csv_path) if l.strip() and not l.startswith('#')] if os.path.exists(csv_path) else []
    x_end = float(rows[-1].split(',')[1]) if len(rows) > 1 else None
    check('and the table model loads from it alone, integrating through its table (w(1.5) = 2.5)',
          'tab.odex (saved in tab.snapx)' in (hello_title(evs) or '') and x_end is not None
          and abs(x_end - 2.5 * (1 - math.exp(-4))) < 1e-3, '%s %s' % (hello_title(evs), rows[-1:]))
    s5.close()


# ---- sessiondiagram: autoX's diagram in the session, name.snapx (W92, docs/protocol.md "autoX files") ----

def messages(evs):
    return ' '.join(str(e) for e in evs if e.get('ev') == 'session.message' or (e.get('ev') == 'done' and e.get('error')))


def section_sessiondiagram():
    import re
    homes, scratch = [], tempfile.mkdtemp(prefix='xppdiagram')

    def server(ode=LECAR):
        home = tempfile.mkdtemp(prefix='xpphome')
        homes.append(home)
        s = Server(args.server, ode, env={'HOME': home}, verbose=args.v)
        s.collect(is_done)
        open_autox(s)
        return s

    s = server()
    evs = run_menu(s, 's')
    grab_hopf(s)
    evs += run_menu(s, 'p', timeout=120 * SLOW)
    dg1 = Diagram().apply(evs).pts
    # W50: a second view of the diagram, the period, the first active again: an .snapx keeps its views
    s.send(cmd='autox.new_view')
    evs += s.collect(is_done)[0]
    s.send(cmd='autox.configure_axes', **{'view': 'diagram_2', 'plot_type': 'period', 'fit': True})
    evs += s.collect(is_done)[0]
    s.send(cmd='window.select', window='diagram_1')
    evs += s.collect(is_done)[0]
    view1 = Diagram('diagram_2').apply(evs)
    # an old name asked for: the .snapx beside it
    got = save_diagram(s, 'd1', 'd1.snapx')
    check('sessiondiagram: Save diagram writes d1.snapx, a zip of the files listed, the model and its animation in it',
          list(got) == ['session.odex', 'model/lecar.odex', 'model/ml.anix', 'autox/diagram.csv', 'autox/solutions.s'], str(list(got)))
    state = got.get('session.odex', '')
    check('sessiondiagram: session.odex names lecar.odex and its animation, and lecar.odex is in the file byte for byte',
          '\nsession {\n  model: "lecar.odex",\n  animation: "ml.anix",\n  data: false,\n' in state
          and got.get('model/lecar.odex') == open(LECAR).read(), state[:300])
    check('sessiondiagram: session.odex has the numerics, the parameters and the axes of autoX',
          all(re.search(r'(^|\n)\s+%s: ' % k, state) for k in ('mesh_intervals', 'step_size', 'plot', 'xmin')) and state.startswith('version 4\npar '),
          state[:300])
    rows = diagram_rows(got.get('autox/diagram.csv', '').encode('utf-8'))
    check("sessiondiagram: diagram.csv has branch records and the diagram's points", len(rows) == len(dg1) > 50
          and got['autox/diagram.csv'].startswith('branch,0,'),
          '%d rows' % len(rows))
    check('sessiondiagram: solutions.s holds the orbits', len(got.get('autox/solutions.s', '')) > 1000, str(len(got.get('autox/solutions.s', ''))))
    views = state[state.index('    views: ['):] if '    views: [' in state else ''
    check('sessiondiagram: the autox group holds the two views of the diagram, the first active',
          re.search(r'name: "diagram_1",\n\s+hidden: false,\n\s+plot: maximum_and_minimum,\n\s+var: V,\n\s+par1: iapp,', views) is not None
          and re.search(r'name: "diagram_2",\n\s+hidden: false,\n\s+plot: period,\n\s+var: V,\n\s+par1: iapp,', views) is not None
          and '    active: "diagram_1",' in views, views[:300])
    d1 = os.path.join(scratch, 'd1.snapx')
    if os.path.exists(os.path.join(s.run, 'd1.snapx')):
        shutil.copy(os.path.join(s.run, 'd1.snapx'), d1)
    s.close()

    # a new server loads it: the same diagram exactly, and a grab restarts from it
    s = server()
    # nothing integrated yet: Start/Periodic has no orbit to start from
    evs = run_menu(s, 'p')
    check('sessiondiagram: Start/Periodic with nothing integrated says to integrate first, with its 0 stored rows and the 2-row requirement',
          'Integrate first' in messages(evs) and 'stored 0 row(s)' in messages(evs)
          and 'at least 2' in messages(evs) and s.alive(), messages(evs)[:200])
    # some data: the same model open keeps it, only the diagram loads
    s.send(cmd='run.initial')
    evs, _ = s.collect(is_done, timeout=30 * SLOW)
    rows_before = last_state(evs)['rows'] if last_state(evs) else None
    evs = load_diagram(s, d1)
    dg2 = Diagram().apply(evs).pts
    v1 = Diagram('diagram_2').apply(evs)
    check('sessiondiagram: Load diagram in a new server gives its two views, the second the period, its points exactly',
          view1.pts and v1.pts == view1.pts and v1.axes and v1.axes['plot'] == 'period' and v1.axes['ylabel'] == 'Period',
          '%d vs %d points, %s' % (len(v1.pts), len(view1.pts), v1.axes))
    check('sessiondiagram: Load diagram in a new server gives the same diagram, exactly',
          dg1 and dg2 == dg1 and 'left as they were' not in messages(evs), '%d vs %d points, first difference %s' % (
              len(dg2), len(dg1), next(((a, b) for a, b in zip(dg2, dg1) if a != b), None)))
    st = last_state(evs)
    check('sessiondiagram: the same model open: asks first and restores the saved session, replacing its data',
          hello_title(evs) is not None and any(is_ask(e) and e['kind'] == 'choice' for e in evs)
          and rows_before and st and st['rows'] == 0, '%s rows before, %s after' % (rows_before, st and st['rows']))
    # the periodic branch's last label (its end point): extending it restarts
    # from the orbit in the restored solutions.s
    last = max((p['lab'] for p in dg2 if p['lab']), default=0)
    s.send(cmd='autox.grab', label=last)
    evs, _ = s.collect(is_done)
    check("sessiondiagram: the loaded diagram's last label is grabbed", last > 0 and 'error' not in messages(evs),
          'label %d; %s' % (last, messages(evs)[:200]))
    evs = run_any(s, 'e')
    more = Diagram()
    more.pts = list(dg2)
    more.apply(evs)
    check('sessiondiagram: extending the grabbed periodic branch continues the loaded diagram',
          len(more.pts) > len(dg2) and more.pts[:len(dg2)] == dg2 and s.alive() and 'error' not in messages(evs)
          and 'nan' not in messages(evs).lower(),
          '%d points after %d; %s' % (len(more.pts), len(dg2), messages(evs)[:200]))
    s.close()

    # an XPPAUT .auto: imported, then saved as .snapx, which loads as imported
    s = server()
    shutil.copy(OLD_AUTOX, os.path.join(s.run, 'old.auto'))
    with open(OLD_AUTOX) as f:
        old_points = int(f.read().split('\n')[15]) + 1  # after the settings: the last point's index
    dg3 = Diagram().apply(import_diagram(s, 'old.auto')).pts
    check('sessiondiagram: Load diagram imports an XPPAUT .auto', len(dg3) > 10, '%d points' % len(dg3))
    got = save_diagram(s, 'old.snapx')
    rows = diagram_rows(got.get('autox/diagram.csv', '').encode('utf-8'))
    check('sessiondiagram: saving the imported diagram writes old.snapx with every point, and leaves old.auto as it was',
          len(rows) == old_points and open(OLD_AUTOX, 'rb').read() == open(os.path.join(s.run, 'old.auto'), 'rb').read(),
          '%d rows for %d points' % (len(rows), old_points))
    # W327: rejected imports leave every saved autoX member byte-for-byte intact.
    with open(OLD_AUTOX) as source:
        foreign = source.read()
    foreign_rows = foreign.splitlines()
    def altered(row, value):
        lines = list(foreign_rows)
        lines[row] = value
        return '\n'.join(lines) + '\n'
    invalid_imports = (
        ('marks', altered(0, '8 0 1 2 3 4 5 6 7 21'), 1),
        ('parameter', altered(0, '8 999 1 2 3 4 5 6 7 0'), 1),
        ('axis', altered(14, '-0.2 -0.5 0.5 0.4 999 2'), 15),
        ('point', altered(20, '9 1 2 0 0 1 1 999 1 2 3 0'), 21),
        ('truncated', '\n'.join(foreign_rows[:21]) + '\n', 22),
        ('solutions', foreign + 'bad solution header\n', len(foreign_rows) + 1),
    )
    for name, text, line in invalid_imports:
        path = 'bad-' + name + '.auto'
        with open(os.path.join(s.run, path), 'w') as output:
            output.write(text)
        events = import_diagram(s, path)
        errors = [event for event in events if event.get('ev') in ('session.message', 'done') and 'error' in event]
        after = save_diagram(s, 'after-' + name + '.snapx')
        check('W327: %s import is refused whole with file and line' % name,
              len(errors) == 1 and errors[0].get('file', '').endswith(path) and errors[0].get('line') == line
              and errors[0].get('source', '') == (text.splitlines()[line - 1] if line <= len(text.splitlines()) else '')
              and {k: v for k, v in after.items() if k.startswith('autox/')} ==
                  {k: v for k, v in got.items() if k.startswith('autox/')}, str(errors)[:300])
    # the client keeps its points across the reset and redraw: only a
    # difference would be sent (a reset dropping points, and new ones)
    dg4 = Diagram()
    dg4.pts = []
    evs = load_diagram(s, 'old.snapx')
    dg4.apply(evs)
    check('sessiondiagram: old.snapx loads the diagram the .auto imported', dg3 and dg4.pts == dg3 and 'error' not in messages(evs),
          '%d vs %d points; %s' % (len(dg4.pts), len(dg3), messages(evs)[:200]))
    # W333: the foreign fixture's solutions are algebraic fold-curve
    # checkpoints, including labels hidden by the current one-parameter view.
    # Test storage data, not that view's filtered points. Keep the historical
    # diagram's 1e-6 print tolerance; the native numerical baseline is exact.
    foreign_diagram_tolerance = 1e-6
    reference_rows = diagram_rows(got['autox/diagram.csv'].encode())
    reference = next((r for r in reference_rows if int(r['lab']) and r['itp'] in ('output_from_fold', 'user_point_from_fold')), None)
    check('sessiondiagram: the reference has an augmented fold checkpoint', reference is not None)
    for path in ('old.auto', 'old.snapx'):
        if path.endswith('.auto'):
            import_diagram(s, path)
        else:
            load_diagram(s, path)
        if reference is None:
            continue
        s.send(cmd='autox.configure_numerics', **{'max_steps': 4, 'output_every': 1})
        s.collect(is_done)
        s.send(cmd='autox.grab', label=int(reference['lab']))
        grabbed, _ = s.collect(is_done)
        evs = run_any(s, 'e')
        saved = save_diagram(s, 'ref-restart.snapx')
        resumed = diagram_rows(saved.get('autox/diagram.csv', '').encode())
        added = resumed[len(reference_rows):]
        state_columns = [k for k in reference if k.startswith('u0.')]
        check('sessiondiagram: augmented reference restart from %s preserves its starting state and computes new points' % path,
              len(added) >= 3 and resumed[:len(reference_rows)] == reference_rows
              and all(abs(float(added[0][k]) - float(reference[k])) <= foreign_diagram_tolerance
                      for k in ['continued_parameter', *state_columns])
              and not errors_of(grabbed + evs),
              '%d new points; first=%s; reference=%s; %s' % (len(added), added[:1], reference,
                  str(errors_of(grabbed + evs))[:200]))
    load_diagram(s, 'old.snapx')

    # A previously omitted parameter changes branch data even when every plotted coordinate agrees.
    s.send(cmd='value.set_parameters', values={'om': 2})
    s.collect(is_done)
    refreshed=import_diagram(s,'old.auto')
    parameters=[r['parameters'] for event in refreshed if is_point(event) for r in event['runs'] if 'parameters' in r]
    check('W339: diagram replay refreshes changed branch constants with identical coordinates',
          bool(parameters) and all(dict(values).get('om')==2 for values in parameters),str(parameters)[:200])
    s.close()

    # the .ode edited (the same names, or phi renamed) and loaded: the
    # .snapx's own model loads, from it, with its diagram exactly
    with open(LECAR) as f:
        text = f.read()
    for edit, what in (('# edited since\n' + text, 'a comment added'), (re.sub(r'\bphi\b', 'phi2', text), 'phi renamed')):
        folder = tempfile.mkdtemp(prefix='xppode', dir=scratch)
        ode = os.path.join(folder, 'lecar.odex')
        with open(ode, 'w') as f:
            f.write(edit)
        s = server(ode)
        evs = load_diagram(s, d1)
        pts = Diagram().apply(evs).pts
        st = last_state(evs)
        check('sessiondiagram: the .ode edited (%s): Load diagram loads the saved model, its diagram exactly' % what,
              'lecar.odex (saved in d1.snapx)' in (hello_title(evs) or '') and pts == dg1
              and st is not None and 'PHI' in [n.upper() for n, v in st['pars']], '%s; %s' % (hello_title(evs), messages(evs)[:300]))
        s.close()

    # another model open, `open` of the .snapx (where no lecar.odex is): its model and diagram
    s = server(HEAVY)
    shutil.copy(d1, s.run)
    evs = open_file(s, 'd1.snapx')
    pts = Diagram().apply(evs).pts
    check('sessiondiagram: open of an .snapx with another model open loads its saved model and diagram',
          'lecar.odex (saved in d1.snapx)' in (hello_title(evs) or '') and pts == dg1, '%s; %d points' % (hello_title(evs), len(pts)))
    check('sessiondiagram: nothing is written beside it', sorted(os.listdir(s.run)) == ['d1.snapx', 'heavy.odex'], str(os.listdir(s.run)))
    # a file without its model: an error, nothing changes
    import zipfile
    z = zipfile.ZipFile(d1)
    with zipfile.ZipFile(os.path.join(s.run, 'nomodel.snapx'), 'w') as out:
        for n in z.namelist():
            if not n.startswith('model/'):
                out.writestr(n, z.read(n))
    evs = open_file(s, 'nomodel.snapx')
    check('sessiondiagram: an .snapx without its model is refused, an error says the model is missing',
          'model is missing' in messages(evs) and hello_title(evs) is None, messages(evs)[:300])
    # W116: a member missing or cut short fails the open before its model is
    # kept: an error names it, and the model open stays (no hello)
    members = {n: z.read(n) for n in z.namelist()}
    rows_csv = members['autox/diagram.csv'].decode().split('\n')
    state = members['session.odex'].decode()
    state_rows = state.split('\n')
    line_of = lambda prefix, start=0: next(k + 1 for k in range(start, len(state_rows)) if state_rows[k].lstrip().startswith(prefix))
    set_state = lambda m, text: m.__setitem__('session.odex', text.encode())
    settings_line = line_of('settings: {')
    views_line = line_of('views: [')
    for name, change, expect in (
            ('nosolutions.snapx', lambda m: m.pop('autox/solutions.s'), 'its autox/solutions.s is missing'),
            ('noautoxsettings.snapx', lambda m: set_state(m, re.sub(r'\n  autox: \{.*?\n  \},', '', state, count=1, flags=re.S)),
             'noautoxsettings.snapx/session.odex:%d:9: the session has no `autox`' % line_of('session {')),
            ('badsolution.snapx', lambda m: m.__setitem__('autox/solutions.s', b'1 1 4 1 2 0 1 9999 2 0 0 2\n0 1 2\n0 0\n'),
             'badsolution.snapx/autox/solutions.s:1: '),
            ('cutdiagram.snapx', lambda m: m.__setitem__('autox/diagram.csv', '\n'.join(rows_csv[:5])[:-7].encode()),
             'cutdiagram.snapx/autox/diagram.csv:5: '),
            ('latersettings.snapx', lambda m: set_state(m, state.replace('\n      marks: [', '\n      later: 1,\n      marks: [', 1)),
             'latersettings.snapx/session.odex:%d:7: `later` is not a setting of settings' % line_of('marks: [', settings_line)),
            # W125: a value autoX refuses (Ncol 9), at its line; a bad value on the last view's line
            ('badncol.snapx', lambda m: set_state(m, re.sub(r'(?m)^(\s+)collocation_points: .*$', r'\1collocation_points: 9,', state, count=1)),
             'badncol.snapx/session.odex:%d:27: collocation_points=9: Collocation points must be' % line_of('collocation_points:')),
            ('lastviews.snapx', lambda m: set_state(m, re.sub(r'(?m)^(\s+)active: "[^"]*",$', r'\1active: "x",', state, count=1)),
             'lastviews.snapx/session.odex:%d:5: Unknown window "x"' % line_of('active:', views_line)),
            ('latermanifest.snapx', lambda m: set_state(m, state.replace('\nsession {\n', '\nsession {\n  later: 1,\n', 1)),
             'latermanifest.snapx/session.odex:%d:3: `later` is not a setting' % (line_of('session {') + 1))):
        damaged = dict(members)
        change(damaged)
        with zipfile.ZipFile(os.path.join(s.run, name), 'w') as out:
            for n, b in damaged.items():
                out.writestr(n, b)
        evs = open_file(s, name)
        errors = ' '.join(placed(e) for e in evs if e.get('ev') in ('session.message', 'done'))
        expected_place, separator, expected_detail = expect.partition(': ')
        check('sessiondiagram: %s is refused before its model is kept (%s), the model open stays' % (name, expect),
              (expected_place + separator) in errors and expected_detail in errors and hello_title(evs) is None and s.alive(), errors[:300])
    # W155: malformed autoX settings on the last line leave the session whole.
    damaged = dict(members)
    settings = damaged['session.odex'].decode()
    lines = settings.replace('max_steps: 2000,', 'max_steps: 43,').split('\n')
    ymax_line = next(k for k in range(len(lines)) if lines[k].lstrip().startswith('ymax:'))
    lines[ymax_line] = '      ymax: 1e999,'
    damaged['session.odex'] = '\n'.join(lines).encode()
    with zipfile.ZipFile(os.path.join(s.run, 'lastsettings.snapx'), 'w') as out:
        for n, b in damaged.items(): out.writestr(n, b)
    s.send(cmd='events.subscribe', events=['autox.settings'])
    before = s.collect(is_done)[0]
    before_settings = next(e for e in before if e.get('ev') == 'autox.settings')
    evs = open_file(s, 'lastsettings.snapx')
    errs = [e for e in evs if e.get('ev') in ('session.message', 'done') and 'error' in e]
    s.send(cmd='events.subscribe', events=['autox.settings'])
    after = s.collect(is_done)[0]
    after_settings = next(e for e in after if e.get('ev') == 'autox.settings')
    check('W155: bad last autoX settings line refuses the entire session, keeping the previous settings',
          errs and errs[0].get('line') == ymax_line + 1 and errs[0].get('source') == lines[ymax_line]
          and hello_title(evs) is None
          and all(before_settings[k] == after_settings[k] for k in ('numerics', 'axes', 'marks')), str(errs)[:300])

    # Reject duplicate members and oversized declared output before decompression.
    import struct, warnings
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', UserWarning)
        with zipfile.ZipFile(os.path.join(s.run, 'duplicate.snapx'), 'w') as out:
            for n, b in members.items(): out.writestr(n, b)
            out.writestr('session.odex', members['session.odex'])
    evs = open_file(s, 'duplicate.snapx')
    check('W155: duplicate archive members are refused without replacing the session',
          'archive size and member limits' in messages(evs) and hello_title(evs) is None and s.alive())
    bomb = bytearray(open(d1, 'rb').read())
    central = bomb.index(b'PK\x01\x02')
    struct.pack_into('<I', bomb, central + 24, 512 * 1024 * 1024 + 1)
    with open(os.path.join(s.run, 'bomb.snapx'), 'wb') as out: out.write(bomb)
    evs = open_file(s, 'bomb.snapx')
    check('W155: an oversized declared archive member is refused before allocation',
          'archive size and member limits' in messages(evs) and hello_title(evs) is None and s.alive())
    shutil.copy(d1, os.path.join(s.run, 'obsolete.autox'))
    evs = open_file(s, 'obsolete.autox')
    check('W155: an .autox is refused by the .odex-only loader, never opened as an archive',
          'loader reads .odex only' in messages(evs) and hello_title(evs) is None and s.alive())
    # a binary file opened as a model: refused, its bytes never shown
    with open(os.path.join(s.run, 'bin.odex'), 'wb') as out:
        out.write(b'x\'=-x\n\x00\x01\x02\xff\n')
    evs = open_file(s, 'bin.odex')
    check('sessiondiagram: a binary file opened as a model is refused, its bytes never shown',
          'not a model' in messages(evs) and hello_title(evs) is None and '\x00' not in messages(evs), messages(evs)[:300])
    s.close()
    for h in homes + [scratch]:
        shutil.rmtree(h, ignore_errors=True)


# ---- control: Abort, Quit and dropped commands during a long integration ----

def set_total(s, total):
    """run.configure's total time"""
    s.send(cmd='run.configure', total_time=total)
    s.collect(is_done)


def rows(evs):
    st = [e for e in evs if e.get('ev') == 'session.state']
    return st[-1]['rows'] if st else None


# run.initial integrates at once: `set` (no run any more, W69) puts mu
# where these checks want it (arm_run/run_now).
def arm_run(s):
    """set mu: the run that follows needs only its command, so a caller that must race the engine (an Abort
       sent right behind the command) races that one command"""
    s.send(cmd='value.set_parameters', values={'mu': -1})
    s.collect(is_done)


def run_now(s):
    """arm_run, then run at once: one full run"""
    arm_run(s)
    s.send(cmd='run.initial')


def section_control():
    s = Server(args.server, HEAVY, verbose=args.v)
    s.collect(is_done)
    set_total(s, 400)  # 40001 rows, some 10 s

    # an Abort sent right behind the run's command, before the engine has taken it
    arm_run(s)
    s.proc.stdin.write(json.dumps({'cmd': 'run.initial'}) + '\n'
                       + json.dumps({'cmd': 'run.stop'}) + '\n')
    s.proc.stdin.flush()
    t = time.monotonic()
    evs, e = s.collect(is_done, timeout=60 * SLOW)
    took = (e['_t'] if e else time.monotonic()) - t
    n = rows(evs)
    check('an Abort sent with the command still stops it', e is not None and n is not None and n < 40001,
          'rows %s' % n)
    perf('abort right after the command -> done', '%.2f s, %s rows' % (took, n))
    s.send(cmd='session.get_state')
    evs, e = s.collect(is_done, timeout=10 * SLOW)
    check('an Abort has no done of its own', len([x for x in evs if is_done(x)]) == 1 and
          any(x.get('ev') == 'session.state' for x in evs), str([x.get('ev') for x in evs]))

    # a command sent after an Abort is not cancelled by it
    set_total(s, 20)
    run_now(s)
    evs, e = s.collect(is_done, timeout=60 * SLOW)
    check('a command sent after an Abort runs in full', rows(evs) == 2001, 'rows %s' % rows(evs))

    # a view command sent during a computation is kept and runs after it,
    # not dropped (W95, docs/protocol.md "Commands during a command"): Close
    set_total(s, 400)
    open_autox(s)
    run_now(s)
    s.collect(lambda e: e.get('ev') == 'job.progress', timeout=30 * SLOW)
    s.send(cmd='window.close', window='diagram_1')
    t = s.send(cmd='run.stop')
    evs, e = s.collect(is_done, timeout=60 * SLOW)
    took = (e['_t'] if e else time.monotonic()) - t
    check('Abort stops an integration', e is not None and (rows(evs) or 0) < 40001, 'rows %s' % rows(evs))
    perf('abort during an integration -> done', '%.2f s' % took)
    s.send(cmd='session.get_state')
    more, _ = s.collect(is_done, timeout=10 * SLOW)
    closed = [x for x in evs + more if x.get('ev') == 'window.destroyed' and x.get('win') == 'diagram_1']
    check('W95: a view command sent during a computation runs after it (Close)', bool(closed))

    # Quit during an integration
    run_now(s)
    s.collect(lambda e: e.get('ev') == 'job.progress', timeout=30 * SLOW)
    t = s.send(cmd='session.quit', save=False)
    try:
        s.proc.wait(timeout=30 * SLOW)
        exited = True
    except subprocess.TimeoutExpired:
        s.proc.kill()
        exited = False
    took = time.monotonic() - t
    check('Quit during an integration exits the process on its own', exited, '%.2f s' % took)
    perf('quit during an integration -> exit', '%.2f s' % took)
    s.close()


# ---- names: a model with long names works like the same model with short --

LONG = os.path.join(here, 'models', 'longnames.odex')
SHORT = os.path.join(here, 'models', 'shortnames.odex')


def long_name(base):
    """base padded to the 200 characters longnames.odex's names have (W76: a
    name has no length limit); the model was written with this"""
    words = ['of', 'the', 'model', 'with', 'a', 'name', 'two', 'hundred', 'characters', 'long']
    s, i = base, 0
    while len(s) < 200:
        s += '_' + words[i % len(words)]
        i += 1
    s = s[:200]
    return s[:-1] + 'z' if s.endswith('_') else s


LONG_PAR = long_name('applied_stimulus_current_amplitude')
LONG_B = long_name('recovery_slope_parameter_b')
LONG_V = long_name('membrane_potential_fast_variable').upper()
LONG_AUX = long_name('total_membrane_drive_current').upper()


def silent_output(ode):
    """xppautX ODE --silent in a scratch dir: output.dat's text (None if none)"""
    run = tempfile.mkdtemp(prefix='xppnames')
    shutil.copy(ode, run)
    subprocess.run([os.path.abspath(args.server), os.path.basename(ode), '--silent'], cwd=run,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60 * SLOW)
    path = os.path.join(run, 'output.dat')
    text = open(path).read() if os.path.exists(path) else None
    shutil.rmtree(run, ignore_errors=True)
    return text


def last_state(evs):
    st = [e for e in evs if e.get('ev') == 'session.state']
    return st[-1] if st else None


def names_diagram(ode):
    """load ODE, integrate, start from the end (the rest state), autoX steady
    state over the first parameter; returns the Diagram and the symbols of
    its labelled points"""
    s = Server(args.server, ode, verbose=args.v)
    s.collect(is_done)
    for command in ('run.initial', 'run.last'):
        s.send(cmd=command)
        s.collect(is_done, timeout=30 * SLOW)
    s.send(cmd='autox.open')
    evs, _ = s.collect(lambda e: e.get('ev') == 'window.created' and e.get('win') == 'diagram_1')
    more, _ = s.collect(is_done)
    evs += more + run_menu(s, 's', timeout=60 * SLOW)
    syms = [sym for e in evs if e.get('ev') == 'autox.diagram' and e['op'] == 'add'
            for r in e['runs'] for i, lab, sym in r.get('lab', [])]
    s.close()
    return Diagram().apply(evs), syms


def periodic_branches(data):
    """The periodic branches of a description run's diagram (its CSV, CRs removed): a list of the branches' points
    (iapp, period, amplitude) in the order each run stored them. The amplitude is the orbit's uhi.V less ulo.V."""
    lines = data.decode('utf-8').replace('\r', '').splitlines()
    rows = csv.DictReader(lines[next(i for i, line in enumerate(lines) if line.startswith('calc,')):])
    branches = {}
    for r in rows:
        if r['calc'] == 'periodic':
            branches.setdefault(r['ibr'], []).append((float(r['values']), float(r['per']), float(r['uhi.V']) - float(r['ulo.V'])))
    return list(branches.values())


def section_orbit_start():
    """W603 (#670), W608 (#675): a Periodic start from the stored integration takes its period from the orbit's own return,
    and the orbit runs of tests/autox's ode_lecar_orbit_* compute. A total time of 1.5 periods of the class-I
    Morris-Lecar cycle (iapp 0.1, period 16.4695) is not refused: the first return is taken; a total time of less than a
    period stores an orbit that never returns, so the start stops and says so, with the total time and the rows. The orbit run of
    ode_lecar_orbit_hopf and its Hopf-label run follow one cycle family, the upper side from the Hopf point near 0.2624
    and the lower side below it; over the iapp values both cover (0.3 to 0.397) the period and amplitude agree."""
    home = tempfile.mkdtemp(prefix='xpporbit')
    s = Server(args.server, LECAR, env={'HOME': home}, verbose=args.v)
    try:
        s.collect(is_done)
        open_autox(s)
        s.send(cmd='value.set_parameters', values={'iapp': 0.1})
        s.collect(is_done)
        s.send(cmd='run.configure', total_time=25)
        s.collect(is_done)
        s.send(cmd='run.initial')
        s.collect(is_done, timeout=30 * SLOW)
        evs = run_menu(s, 'p')
        diagram = Diagram().apply(evs)  # the runs that compute add to one diagram
        points = len(diagram.pts)
        check('orbit_start: a Periodic start over 1.5 periods takes the first return and computes points',
              'start stopped' not in messages(evs) and points > 1 and s.alive(), '%d points: %s' % (points, messages(evs)[:300]))
        s.send(cmd='value.set_parameters', values={'iapp': 0.1})
        s.collect(is_done)
        s.send(cmd='run.configure', total_time=10)
        s.collect(is_done)
        s.send(cmd='run.initial')
        s.collect(is_done, timeout=30 * SLOW)
        evs = run_menu(s, 'p')
        check('orbit_start: a Periodic start over less than a period says the stored orbit never comes back to its first row, with the total time and the rows',
              'periodic start stopped' in messages(evs).lower() and 'never comes back to its first row' in messages(evs)
              and 't = 0 to 10' in messages(evs) and s.alive(), messages(evs)[:300])
        s.send(cmd='value.set_parameters', values={'iapp': 0.1})
        s.collect(is_done)
        s.send(cmd='run.configure', total_time=17)
        s.collect(is_done)
        s.send(cmd='run.initial')
        s.collect(is_done, timeout=30 * SLOW)
        evs = run_menu(s, 'p')
        before, points = points, len(diagram.apply(evs).pts)
        check('orbit_start: a Periodic start over a period and a little (17) is not refused and computes points beyond its start',
              'never comes back' not in messages(evs) and 'start stopped' not in messages(evs) and points > before,
              '%d points: %s' % (points, messages(evs)[:300]))
    finally:
        s.close()
        shutil.rmtree(home, ignore_errors=True)

    # The two runs of ode_lecar_orbit_hopf: the orbit run starts at the description's iapp 0.3 and goes up; the Hopf-label
    # run starts at the Hopf point (near 0.2624), goes down past its fold near 0.2484, and comes back up through 0.3.
    # Its stored order after the fold is the upper side. A point of the orbit run is compared with that side interpolated
    # at its iapp; the orbit run's start point is left out (its first point is not converged, W603).
    # Tolerances, each with its reason: the period within 1e-3 relative (the corrector stops at parameter_tolerance 1e-4 on
    # 15-interval meshes; the largest difference measured is 1.3e-3 absolute on a period of about 15, 8e-5 relative), and the
    # amplitude within 2e-3 absolute (1e-3 measured, the same corrector).
    description = read_description(BASE / 'ode_lecar_orbit_hopf.json')
    branches = periodic_branches(compute(args.server, description, 1)[0])
    orbit = next(b for b in branches if min(p[0] for p in b) >= 0.3 - 1e-9)
    hopf = next(b for b in branches if min(p[0] for p in b) < 0.26)
    fold = min(range(len(hopf)), key=lambda i: hopf[i][0])
    upper = hopf[fold:]
    xs = [p[0] for p in upper]
    compared = 0
    worst_period = worst_amplitude = 0.0
    for iapp, period, amplitude in orbit:
        if not (iapp > 0.3 and xs[0] <= iapp <= xs[-1]):
            continue
        i = next(k for k in range(1, len(xs)) if xs[k] >= iapp)
        t = (iapp - xs[i - 1]) / (xs[i] - xs[i - 1])
        hopf_period = upper[i - 1][1] + t * (upper[i][1] - upper[i - 1][1])
        hopf_amplitude = upper[i - 1][2] + t * (upper[i][2] - upper[i - 1][2])
        worst_period = max(worst_period, abs(period - hopf_period) / period)
        worst_amplitude = max(worst_amplitude, abs(amplitude - hopf_amplitude))
        compared += 1
    check('orbit_start: the orbit run and the Hopf-label run agree on the cycle family where both cover it (period within 1e-3 relative, amplitude within 2e-3)',
          compared >= 10 and worst_period <= 1e-3 and worst_amplitude <= 2e-3,
          '%d points compared, worst period %.2e relative, worst amplitude %.2e' % (compared, worst_period, worst_amplitude))


def section_names():
    short, long_ = silent_output(SHORT), silent_output(LONG)
    check('names: --silent integrates the model with 200-character names',
          long_ is not None and len(long_.splitlines()) == 2001, str(long_ and len(long_.splitlines())))
    check("names: and its output.dat is the short-named model's", long_ is not None and long_ == short)

    home = tempfile.mkdtemp(prefix='xpphome')
    s = Server(args.server, LONG, env={'HOME': home}, verbose=args.v)
    evs, _ = s.collect(is_done)
    hello = next((e for e in evs if e.get('ev') == 'hello'), None)
    st = last_state(evs)
    msgs = [e for e in evs if e.get('ev') == 'session.message' or (e.get('ev') == 'done' and e.get('error'))]
    check('names: the model loads with no message', st is not None and not msgs, str(msgs)[:200])
    pars = dict(st['pars']) if st else {}
    check("names: state carries the parameters' full 200-character names", LONG_B in pars and LONG_PAR in pars,
          str(list(pars))[:300])
    check("names: and the variables'", st is not None and [n for n, v in st['ics']] ==
          [LONG_V, 'SLOW_RECOVERY_VARIABLE_W'], str(st and st['ics'])[:300])
    check('names: hello lists the auxiliary by its full name', hello is not None and
          LONG_AUX in hello['lists'][0], str(hello and hello['lists'][0])[:300])

    s.send(cmd='value.set_parameters', values={LONG_B: 0.9})
    s.collect(is_done)
    s.send(cmd='initial_conditions.set', values={'slow_recovery_variable_w': -0.25})
    evs, _ = s.collect(is_done)
    st = last_state(evs)
    check('names: set a parameter by its 200-character name', st is not None and dict(st['pars']).get(LONG_B) == 0.9,
          str(st and st['pars'])[:300])
    check('names: set an initial condition by its name, in any case',
          st is not None and dict(st['ics']).get('SLOW_RECOVERY_VARIABLE_W') == -0.25, str(st and st['ics'])[:300])
    # a formula naming a long name twice (over 400 characters) is read whole
    s.send(cmd='value.set_by_formula', name=LONG_PAR.upper(), formula=LONG_B + '-' + LONG_B.upper() + '+0.125')
    evs, _ = s.collect(is_done)
    st = last_state(evs)
    errs = [e for e in evs if e.get('ev') == 'session.message' or (e.get('ev') == 'done' and e.get('error'))]
    check('names: a formula of 400 characters sets a parameter', not errs and st is not None
          and dict(st['pars']).get(LONG_PAR) == 0.125, '%s %s' % (errs, st and dict(st['pars']).get(LONG_PAR)))
    s.send(cmd='value.set_parameters', values={LONG_PAR: 0})
    s.collect(is_done)
    # a name longer than any matches nothing, not the name it starts with
    s.send(cmd='value.set_parameters', values={LONG_B + 'x' * 40: 5})
    evs, _ = s.collect(is_done)
    st = last_state(evs)
    check('names: a longer name sets nothing', st is not None and dict(st['pars']).get(LONG_B) == 0.9,
          str(st and st['pars'])[:300])

    # autoX's settings take and show the names whole (its forms too)
    def autoxsettings(evs):
        au = [e for e in evs if e.get('ev') == 'autox.settings']
        return au[-1] if au else None

    up = lambda names: [n.upper() for n in names]
    s.send(cmd='events.subscribe', events=['autox.settings'])
    evs, _ = s.collect(is_done)
    au = autoxsettings(evs)
    check("names: autoX's parameters are the long names", au is not None
          and 'pars' not in au and au['axes']['par1'].upper()==LONG_PAR.upper(), str(au)[:300])
    s.send(cmd='autox.configure_axes', **{'plot_type': 'norm', 'variable': LONG_V, 'parameter1': LONG_B})
    evs, _ = s.collect(is_done)
    au = autoxsettings(evs)
    errs = [e for e in evs if e.get('ev') == 'session.message' or (e.get('ev') == 'done' and e.get('error'))]
    check('names: auto set takes 200-character names for the parameters and the axes', not errs and au is not None
          and au['axes']['par1'].upper() == LONG_B.upper() and au['axes']['var'].upper() == LONG_V,
          '%s %s' % (errs, str(au)[:300]))
    s.send(cmd='autox.configure_axes', **{'parameter2':LONG_PAR})
    evs,_=s.collect(is_done)
    au=autoxsettings(evs)
    check('W339: both axes accept complete long model parameter names', au is not None
          and au['axes']['par1'].upper()==LONG_B.upper() and au['axes']['par2'].upper()==LONG_PAR.upper())
    s.send(cmd='autox.configure_axes', **{'plot_type': 'norm', 'variable': LONG_V, 'parameter1': LONG_PAR})
    s.collect(is_done)

    s.send(cmd='run.initial')
    s.collect(is_done, timeout=30 * SLOW)
    s.send(cmd='data.page', **{'from_row': 0, 'count': 1, 'first_column': 1, 'columns': 3})
    evs, br = s.collect(lambda e: e.get('ev') == 'data.updated')
    s.collect(is_done)
    cols = ['T', LONG_V, 'SLOW_RECOVERY_VARIABLE_W', LONG_AUX]
    check('names: the data browser heads its columns with the long names', br is not None and br['cols'] == cols,
          str(br and br['cols'])[:300])
    # a column added under a 200-character name, from a formula over 200
    added = long_name('added_column_of_the_product')
    s.send(cmd='data.add_column', name=added, formula=LONG_V.lower() + '*slow_recovery_variable_w')
    evs,_ = s.collect(is_done)
    errs = [e for e in evs if e.get('ev') == 'session.message' or (e.get('ev') == 'done' and e.get('error'))]
    s.send(cmd='data.page', **{'from_row': 0, 'count': 1, 'first_column': 1, 'columns': 4})
    evs, br = s.collect(lambda e: e.get('ev') == 'data.updated')
    s.collect(is_done)
    row = br['data'][0] if br and br.get('data') else []
    check('names: Add column takes a 200-character name and a long formula', not errs and br is not None
          and br['cols'] == cols + [added.upper()] and len(row) == 5 and abs(row[4] - row[1] * row[2]) < 1e-5,
          '%s %s' % (errs, str(br)[:300]))
    s.send(cmd='data.export', target='table', format='csv', file='ln.csv')
    s.collect(is_done)
    csv_path = os.path.join(s.run, 'ln.csv')
    lines = [l.strip() for l in open(csv_path)] if os.path.exists(csv_path) else []
    lines = [l for l in lines if l and not l.startswith('#')]
    check('names: Save data as CSV heads its columns with the whole names',
          bool(lines) and lines[0].split(',') == cols + [added.upper()], str(lines[:1])[:300])

    # the settings of a session keep the values under the long names
    import zipfile
    s.send(cmd='session.save_as', file='ln')
    s.collect(is_done, timeout=20 * SLOW)
    snap_path = os.path.join(s.run, 'ln.snapx')
    text = zipfile.ZipFile(snap_path).read('session.odex').decode() if os.path.exists(snap_path) else ''
    check('names: the settings name the long parameter whole', LONG_B in text)
    s2 = Server(args.server, LONG, env={'HOME': home}, verbose=args.v)
    s2.collect(is_done)
    if os.path.exists(snap_path):
        shutil.copy(snap_path, s2.run)
    s.close()
    evs = open_session(s2, 'ln')
    st = last_state(evs)
    check('names: a session file round trip keeps the long-named values',
          st is not None and dict(st['pars']).get(LONG_B) == 0.9 and
          dict(st['ics']).get('SLOW_RECOVERY_VARIABLE_W') == -0.25, str(st and (st['pars'], st['ics']))[:300])
    s2.close()
    shutil.rmtree(home, ignore_errors=True)

    dl, syml = names_diagram(LONG)
    ds, syms = names_diagram(SHORT)
    check('names: autoX continues the steady state over a long-named parameter', len(dl.pts) > 20,
          '%d points' % len(dl.pts))
    check('names: the diagram\'s axis is labelled with the parameter\'s full name',
          dl.axes is not None and dl.axes.get('xlabel') == LONG_PAR, str(dl.axes and dl.axes.get('xlabel')))
    key = lambda d: [(p['br'], p['pt'], p['lab'], p['x'], p['y']) for p in d.pts]
    check('names: and the diagram is the short-named model\'s', key(dl) == key(ds),
          '%d vs %d points' % (len(dl.pts), len(ds.pts)))
    check('names: the Hopf bifurcation is labelled in both', 'hopf' in syml and syml == syms, '%s %s' % (syml, syms))


# ---- recording: the one replay format, including headless batch runs ----

def run_recording(path=None, lines=None, steps=None):
    run = tempfile.mkdtemp(prefix='xppreplay')
    if path is None:
        path = make_recording(args.server, LECAR, lines or [], run)
        if steps is not None:
            with open(path, encoding='utf-8') as f:
                template = f.read()
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(recording_text(template, steps))
    with open(path, encoding='utf-8') as f:
        run_recording.places = [(i, l.rstrip()) for i, l in enumerate(f, 1) if l.startswith('{')]
    result = replay_recording(args.server, LECAR, path, run)
    run_recording.stderr = result.stderr
    return result.returncode, result.stdout, run


def shipped_session_zips():
    """(path, zip) of every shipped .snapx and every .recx's @snapshot (W206: a text grep cannot see inside them)"""
    for folder in ('examples', 'tests', 'tools', 'docs'):
        for root, _, names in os.walk(folder):
            for name in names:
                path = os.path.join(root, name)
                if name.endswith('.snapx'):
                    yield path, zipfile.ZipFile(path)
                elif name.endswith('.recx'):
                    with open(path, encoding='utf-8') as f:
                        lines = f.read().splitlines()
                    start = lines.index('@snapshot')
                    yield path, zipfile.ZipFile(io.BytesIO(base64.b64decode(''.join(lines[start + 1:lines.index('@end', start)]))))


def check_session_names():
    old = []
    for path, z in shipped_session_zips():
        for member in z.namelist():
            if member.endswith('.set') or member == 'session.txt':
                old.append(path + '/' + member)
    check("no shipped .snapx or .recx carries a member of the older layout (XPPAUT's set lines, session.txt; W285)", not old, str(old))
    # W216: nor a model whose @ line names an option by XPPAUT's word
    xppaut_words = re.compile(r'^@\s.*\b(total|t0|trans|nmesh|newt_iter|newt_tol|jac_eps|meth|tol|dtmin|dtmax|atol|maxstor|tor_per|poimap|poivar|poisgn|poistop|poipln)\s*=', re.I | re.M)
    old = [path + '/' + member for path, z in shipped_session_zips() for member in z.namelist()
           if member.endswith('.odex') and xppaut_words.search(z.read(member).decode())]
    check('no shipped .snapx or .recx carries a model with an XPPAUT option word (W216)', not old, str(old))


def section_recording():
    check_session_names()
    code, out, run = run_recording(RECORDING)
    dones = out.count('"ev":"done"') + out.count('"ev":"done"')
    check('xppautX lecar_autox.recx --silent plays to the end with no interface', code == 0,
          'exit %d, %s' % (code, run_recording.stderr[-300:]))
    check('the player reaches done after each recorded command', dones >= 8, '%d dones' % dones)
    check('it saves the autoX diagram (File/Save diagram, key s)', os.path.exists(os.path.join(run, 'lecar.snapx')))
    shutil.rmtree(run, ignore_errors=True)

    # The snapshot precedes the steps: the model's defaults cannot replace it.
    with tempfile.TemporaryDirectory(prefix='xppsnapshot') as run:
        run_commands(args.server, LECAR, [
            {'cmd':'value.set_parameters', 'values':{'iapp':0.9}},
            {'cmd': 'recording.start'}, {'cmd': 'run.initial'},
            {'cmd': 'recording.stop', 'file': 'start.recx'}], run)
        r = replay_recording(args.server, LECAR, os.path.join(run, 'start.recx'), run)
        states = [json.loads(l) for l in r.stdout.splitlines() if '"ev":"session.state"' in l]
        check('W144: silent playback loads the nondefault starting snapshot',
              r.returncode == 0 and states and dict(states[-1]['pars'])['iapp'] == 0.9, r.stderr)

    # A sent recording may be hostile; validate it whole before any step runs.
    with tempfile.TemporaryDirectory(prefix='xpphostile') as run:
        path = make_recording(args.server, LECAR, [], run)
        with open(path, encoding='utf-8') as f:
            template = f.read()
        marker = os.path.join(run, 'outside.dat')
        with open(marker, 'wb') as f:
            f.write(b'keep')
        def quiet(steps):
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(recording_text(template, steps))
            return subprocess.run([os.path.abspath(args.server), path, '--silent'], cwd=run,
                capture_output=True, text=True, encoding='utf-8', timeout=60 * SLOW)
        outside = {'cmd':{'cmd':'session.export_info', 'file':marker}}
        r = quiet([outside])
        check('W144: a recording cannot overwrite a path using a recorded path',
              r.returncode == 1 and r.stdout == '' and file_content(marker, 'rb') == b'keep', r.stderr)
        # (a model's options no longer open a file at the load: the log is the user's, W619)
        r = quiet([{'cmd':{'cmd':'file','op':'put','name':'../outside.dat','data':'AA=='}}])
        check('W144: a file refusal is printed once at the recording step',
              r.returncode == 1 and r.stdout == '' and r.stderr.count('run.recx:') == 1
              and file_content(marker, 'rb') == b'keep', r.stderr)
        early = {'cmd':{'cmd':'session.export_info', 'file':'early.pars'}}
        r = quiet([early, {'cmd':{'cmd':'run.initial'}, 'files':[0.5]}])
        check('W144: an invalid final step is refused before any earlier step executes',
              r.returncode == 1 and r.stdout == '' and 'file section 0.5' in r.stderr
              and r.stderr.count('run.recx:') == 1 and not os.path.exists(os.path.join(run, 'early.pars')), r.stderr)
        r = quiet([{'cmd':{'cmd':'run.initial'}, 'abort':{'what':'integrate','rows':1e100}}])
        check('W144: an out-of-range interruption is refused before playback',
              r.returncode == 1 and 'invalid recorded interruption' in r.stderr and r.stdout == '', r.stderr)
        r = quiet([])
        check('W144: an empty recording loads its snapshot and exits cleanly', r.returncode == 0 and r.stdout == '', r.stderr)
        r = quiet([{'cmd':{'cmd': 'session.quit', 'save': False}}, {'cmd':{'cmd':'value.set_parameters','values':{'iapp':0.8}}}])
        leftover = quiet([{'cmd':{'cmd': 'session.quit', 'save': False}, 'answers':[{'save_replace':1}]}])
        check('W144: a recorded quit cannot report success while skipping later steps',
              r.returncode == 1 and r.stderr.count('Playback ended before') == 1 and r.stdout == ''
              and leftover.returncode == 1 and leftover.stderr.count('Playback ended before') == 1
              and leftover.stdout == '', r.stderr + leftover.stderr)
        r = quiet([{'cmd':{'cmd': 'recording.pause'}}])
        check('W144: a hostile step cannot pause its own player and stall headless playback',
              r.returncode == 1 and 'cannot control its own player' in r.stderr and r.stdout == '', r.stderr)
        # Matches XPP_FILES_CAP: the shared upload/recording byte bound.
        RECORDING_CAP = 64 << 20
        with open(path, 'wb') as f:
            f.truncate(RECORDING_CAP + 1)
        r = subprocess.run([os.path.abspath(args.server), path, '--silent'], cwd=run,
            capture_output=True, text=True, encoding='utf-8', timeout=60 * SLOW)
        check('W144: the bounded recording reader refuses oversized input',
              r.returncode == 1 and 'file size limit' in r.stderr and r.stdout == '', r.stderr)
    code, out, run = run_recording(lines=[{'cmd':'value.set_by_formula', 'name':'iapp', 'formula':'not a valid formula('}])
    check('a value.set_by_formula with a bad formula exits 1', code == 1, 'exit %d' % code)
    shutil.rmtree(run, ignore_errors=True)
    code, out, run = run_recording(lines=[{'cmd':'answer', 'key':'s'}])
    check('an answer to nothing exits 1', code == 1, 'exit %d' % code)
    shutil.rmtree(run, ignore_errors=True)
    code, out, run = run_recording(lines=[{'cmd':'session.open_model'}, {'cmd':'run.initial'}])
    check('a recording missing the answer to a command\'s question exits 1, naming the step once',
          code == 1 and 'run.recx:%d:' % run_recording.places[0][0] in run_recording.stderr
          and run_recording.stderr.count('the player stopped') == 1,
          'exit %d, %s' % (code, run_recording.stderr[-300:]))
    shutil.rmtree(run, ignore_errors=True)


def stopped_events(out):
    return [json.loads(l)['at'] for l in out.splitlines() if l.startswith('{"ev":"job.stopped"')]


def last_rows(out):
    st = [json.loads(l) for l in out.splitlines() if l.startswith('{"ev":"session.state"')]
    return st[-1]['rows'] if st else None


def file_content(path, mode='r'):
    if not os.path.exists(path):
        return None
    with open(path, mode) as f:
        return f.read()


def section_replay():
    full = silent_output(LECAR)
    total_rows = len(full.strip().splitlines()) if full else 0
    check('replay: the default run stores rows to replay against', total_rows > 4, '%d rows' % total_rows)
    target = max(total_rows // 2, 1)
    code, out, run = run_recording(lines=[
        {'cmd':'run.initial'},
        {'cmd':'run.stop','at':{'what':'integrate','rows':target,'t':0}},
        {'cmd':'data.export','target':'table','format':'dat'}, {'cmd':'answer','file':'run.dat'},
        {'cmd':'run.initial'}])
    at = (stopped_events(out) or [None])[0]
    check('replay: an interrupted integration stops exactly at the armed row', code == 0 and at is not None
          and at.get('what') == 'integrate' and at.get('rows') == target, 'exit %d, %s' % (code, at))
    written = file_content(os.path.join(run, 'run.dat'))
    written_rows = len(written.strip().splitlines()) if written else 0
    check('replay: it writes the rows it kept', written_rows == target, '%d rows vs %d' % (written_rows, target))
    check('replay: a second run after the interruption computes to the end again', last_rows(out) == total_rows,
          'last state rows %s vs %s' % (last_rows(out), total_rows))
    shutil.rmtree(run, ignore_errors=True)

    with open(RECORDING, encoding='utf-8') as f:
        text = f.read()
    steps = [json.loads(l) for l in text.split('@steps\n',1)[1].splitlines() if l.startswith('{')]
    body, save = steps[:-1], steps[-1:]
    code0, out0, run0 = run_recording(steps=body)
    dg0 = Diagram().apply([json.loads(l) for l in out0.splitlines()])
    br2 = [p for p in dg0.pts if p['br'] == 2]
    check('replay: the periodic run computes points to replay against', code0 == 0 and len(br2) > 4,
          'exit %d, %d points' % (code0, len(br2)))
    shutil.rmtree(run0, ignore_errors=True)
    if br2:
        target_point = br2[len(br2) // 2]['pt']
        abort_at = {'what':'autox','branch':2,'point':target_point+1}
        interrupted = [dict(st) for st in body]
        interrupted[-1]['abort'] = abort_at
        code, out, run = run_recording(steps=interrupted + save)
        at = (stopped_events(out) or [None])[0]
        check('replay: an interrupted autoX run stops exactly at the armed point', code == 0 and at == abort_at,
              'exit %d, %s vs %s' % (code, at, abort_at))
        diagram = file_content(os.path.join(run, 'lecar.snapx'), 'rb')
        check('replay: and saves a diagram at that point', diagram is not None and len(diagram) > 0,
              str(diagram and len(diagram)))
        shutil.rmtree(run, ignore_errors=True)

    code, out, run = run_recording(lines=[{'cmd':'run.initial'},
        {'cmd':'run.stop','at':{'what':'integrate','rows':100000,'t':5000}},
        {'cmd':'run.initial'}])
    check('replay: an interruption never reached exits 1, naming its step once', code == 1
          and 'run.recx:%d:' % run_recording.places[0][0] in run_recording.stderr
          and run_recording.stderr.count('never got to its recorded interruption') == 1,
          'exit %d, %s' % (code, run_recording.stderr[-300:]))
    check('replay: and plays nothing after it', out.count('"ev":"job.computing"') == 1,
          '%d computations' % out.count('"ev":"job.computing"'))
    shutil.rmtree(run, ignore_errors=True)
    code, out, run = run_recording(lines=[{'cmd':'run.stop'}, {'cmd':'run.initial'},
        {'cmd':'run.stop','at':{'what':'other'}}, {'cmd':'run.stop'}])
    check('replay: interruptions that cannot be placed do not stall the player',
          code == 0 and last_rows(out) == 601 and not stopped_events(out),
          'exit %d, rows %s, %s' % (code, last_rows(out), run_recording.stderr[-300:]))
    shutil.rmtree(run, ignore_errors=True)


# ---- scratch: a start sweeps a dead pid's leftover autoX folder, but never a
# live pid's (issue #32) ------------------------------------------------------

def section_scratch():
    tmp = tempfile.gettempdir() if os.name == 'nt' else (os.environ.get('TMPDIR') or '/tmp')

    # a pid that is definitely not running any more: a child reaped before
    # xppautX starts (pid reuse in that instant is not realistic here)
    dead = subprocess.Popen([sys.executable, '-c', 'pass'])
    dead.wait()
    dead_dir = os.path.join(tmp, 'xppautoX-%d-0' % dead.pid)
    os.makedirs(dead_dir, exist_ok=True)
    with open(os.path.join(dead_dir, 'fort.7'), 'w') as f:
        f.write('a leftover from a killed run')

    # a pid that is still running: this test process itself
    live_dir = os.path.join(tmp, 'xppautoX-%d-0' % os.getpid())
    os.makedirs(live_dir, exist_ok=True)
    with open(os.path.join(live_dir, 'fort.7'), 'w') as f:
        f.write('a live session, not to be touched')

    try:
        s = Server(args.server, LECAR, verbose=args.v)
        s.collect(is_done)
        check('a dead pid\'s leftover scratch folder is gone after a start', not os.path.isdir(dead_dir))
        check('a live pid\'s scratch folder is left alone', os.path.isdir(live_dir))
        s.close()
    finally:
        shutil.rmtree(live_dir, ignore_errors=True)
        shutil.rmtree(dead_dir, ignore_errors=True)


# ---- errors: a failed autoX computation ends the run, not the program ----

NUMERICS = ['mesh_intervals', 'max_steps', 'output_every', 'collocation_points']  # the Numerics form's first fields, in its order


def set_numerics(s, **values):
    """Configure the registry's numerics and return its result events."""
    s.send(cmd='autox.configure_numerics', **values)
    return s.collect(is_done)[0]


def errors_of(evs):
    return [e['error'] for e in evs if e.get('ev') in ('session.message', 'done') and 'error' in e]


def section_subscriptions():
    """W335: autoX's subscriptions are the protocol client's, its diagram the
    Session's. A reload (a new session) keeps the subscriptions, sends fresh
    autoxsettings and no point of the diagram before; after it a grab answered
    with a point and Return in one answer takes that point (grab_key_after)."""
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_done)
    s.send(cmd='events.subscribe', events=['autox.info', 'autox.settings'])
    s.collect(is_done)
    dg = Diagram().apply(hopf_steady(s))
    dg.apply(run_any(s, 's'))
    check('subscriptions: the steady branch gives points', len(dg.pts) > 10, '%d points' % len(dg.pts))
    s.send(cmd='session.reload_model')
    evs, end = s.answer_asks(is_done, {'choice': lambda e: {'key': 'd'}})
    check('subscriptions: a reload keeps the subscriptions: fresh autoxsettings without asking again',
          end is not None and any(e.get('ev') == 'autox.settings' for e in evs), str([e.get('ev') for e in evs])[:300])
    check('subscriptions: ... and sends no point of the diagram before',
          not any(e.get('ev') == 'autox.diagram' and e.get('op') == 'add' for e in evs),
          str([e for e in evs if e.get('ev') == 'autox.diagram'])[:300])
    # the reload keeps the values by name: iapp is where the first run ended, at autoX's upper limit
    s.send(cmd='value.set_parameters', values={'iapp': 0.05})
    s.collect(is_done)
    dg = Diagram().apply(hopf_steady(s))
    dg.apply(run_any(s, 's'))
    check('subscriptions: after the reload a steady branch computes again', len(dg.pts) > 10, '%d points' % len(dg.pts))
    info = grab_point(s, 5, take=True)
    check('subscriptions: after the reload a grab takes point 5 with its key in the same answer',
          info is not None and info['info']['point'] == 5 and s.alive(), str(info)[:300])
    s.close()


def section_errors():
    """Invalid settings stop at their input; the existing settings and
    periodic branches survive. Internal error paths have unit tests."""
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_done)
    dg = Diagram().apply(hopf_steady(s))
    dg.apply(run_any(s, 's'))

    def periodic():
        """grab the first HB, a periodic run; its events and new points"""
        s.send(cmd='autox.grab', type='hopf', index=1)
        s.collect(is_done)
        n = len(dg.pts)
        evs = run_any(s, 'p')
        dg.apply(evs)
        return evs, dg.pts[n:]

    _, good = periodic()
    check('errors: a periodic run from the Hopf point computes points', len(good) > 2, '%d points' % len(good))
    for field, bad, word in (('collocation_points', 8, 'Collocation points'), ('mesh_intervals', 0, 'Mesh intervals')):
        normal = {'collocation_points': 4, 'mesh_intervals': 15}[field]
        evs = set_numerics(s, **{field: bad})
        errs = errors_of(evs)
        check('errors: %s %d: the form refuses the value before a run' % (word, bad),
              len(errs) == 1 and word in errs[0] and '%s=%d' % (field, bad) in errs[0], str(errs))
        check('errors: %s %d: the session is still up' % (word, bad), s.proc.poll() is None)
        s.send(cmd='events.subscribe', events=['autox.settings'])
        evs, done = s.collect(is_done)
        check('errors: %s %d: a command after it answers' % (word, bad), done is not None)
        set_numerics(s, **{field: normal})
        evs, again = periodic()
        check('errors: %s back to %d: the next run computes the same points as before' % (word, normal),
              not errors_of(evs) and [p['y'] for p in again] == [p['y'] for p in good],
              '%d points vs %d, errors %s' % (len(again), len(good), errors_of(evs)))
    s.close()

    # W328: deny writes in this process's own scratch, after opening autoX: the folder the solutions are
    # kept in (a run's fort files are in a folder of the run's own, W595).
    s = Server(args.server, LECAR, verbose=args.v)
    denied = False
    scratch = None
    try:
        s.collect(is_done)
        hopf_steady(s)
        folders = s.autox_dirs()
        assert len(folders) == 1, folders
        scratch = folders[0]
        if os.name == 'nt':
            r = subprocess.run(['icacls', scratch, '/deny', '*S-1-1-0:(W)'], capture_output=True)
            assert r.returncode == 0, r.stderr
            denied = True
        elif os.geteuid() != 0:  # root bypasses mode permissions: nothing can be denied
            os.chmod(scratch, 0o555)
            denied = True
        if denied:
            evs = run_any(s, 's')
            errors = [e for e in evs if e.get('ev') in ('session.message', 'done') and e.get('error')]
            check('W328: a folder autoX cannot write to is reported once, with the original Place',
                  len(errors) == 1 and scratch.replace(os.sep, '/') in errors[0]['error']  # the error names the folder with / on Windows too
                  and errors[0].get('file') == 'command' and errors[0].get('line') == 1
                  and json.loads(errors[0].get('source', '{}')).get('cmd') == 'autox.run'
                  and not s.autox_dirs()[1:] and s.alive(), str(errors))
    finally:
        if denied:
            if os.name == 'nt':
                subprocess.run(['icacls', scratch, '/remove:d', '*S-1-1-0'], capture_output=True, check=True)
            else:
                os.chmod(scratch, 0o755)
        s.close()

    # W328: the shipped recording already has a periodic restart. Inject the
    # same bad Ncol through its Numerics form before replaying that restart.
    with open(RECORDING, encoding='utf-8') as f:
        template = f.read()
    body = [json.loads(line) for line in template.split('@steps\n', 1)[1].splitlines()
            if line.startswith('{')]
    # Inject immediately before the periodic autoX/Run step.
    insert = next(i for i, step in enumerate(body) if isinstance(step.get('cmd'), dict)
                  and step['cmd'].get('cmd') == 'autox.run' and step['cmd'].get('kind') == 'periodic')
    body = body[:insert] + [{'cmd':dict(cmd='autox.configure_numerics',collocation_points=8)}] + body[insert:insert + 1]
    code, out, run = run_recording(steps=body)
    try:
        errors = [json.loads(line) for line in out.splitlines() if line.startswith('{')
                  and json.loads(line).get('ev') in ('session.message', 'done') and json.loads(line).get('error')]
        place = run_recording.places[insert]
        check('W328: a refused autoX value names its recorded form step once in server and silent',
              code == 1 and len(errors) == 1 and 'Collocation points' in errors[0]['error']
              and errors[0].get('file', '').endswith('run.recx')
              and (errors[0].get('line'), errors[0].get('source')) == place
              and 'run.recx:%d:' % place[0] in run_recording.stderr
              and run_recording.stderr.count('Collocation points must be a whole number from 2 to 7') == 1
              and 'autoX stopped' not in run_recording.stderr,
              str((code, errors, run_recording.stderr[-300:])))
    finally:
        shutil.rmtree(run, ignore_errors=True)

    # W340: autoX continues a model parameter, so a model without one is refused
    model_dir = tempfile.mkdtemp(prefix='xppnopar')
    try:
        nopar = os.path.join(model_dir, 'nopar.ode')
        with open(nopar, 'w') as f:
            f.write("x'=-x\ndone\n")
        s = Server(args.server, nopar, verbose=args.v)
        s.collect(is_done)
        step(s, cmd='autox.open')
        errs = errors_of(run_any(s, 's'))
        check('errors: a model without parameters: autoX refuses to run, naming why, and the server lives',
              any('needs a model parameter' in e for e in errs) and s.alive(), str(errs))
        s.close()
    finally:
        shutil.rmtree(model_dir, ignore_errors=True)


# ---- memory: borrowed columns, CVODE's memory on a failed run (W117) ----

BORROWED = os.path.join(here, 'models', 'borrowed.odex')
CVODE_STOP = os.path.join(here, 'models', 'cvode_stop.odex')
CVODE_POINCARE = os.path.join(here, 'models', 'cvode_poincare.odex')


def integrate(s):
    """run.initial: the events to done"""
    s.send(cmd='run.initial')
    evs, _ = s.collect(is_done, timeout=60 * SLOW)
    return evs


def numerics_menu(s, key, item, replies=None):
    """Registry operation used by the old numerical-menu fixture."""
    operation = {('a','n'):'adjoint.compute',('a','m'):'adjoint.compute_h_function',
        ('a','a'):'adjoint.show',('a','h'):'adjoint.show_h_function',
        ('h','o'):'histogram.show_last',('h','d'):'stochastic.show_run'}[(key,item)]
    s.send(cmd=operation, **({'couplings':{'x':'0','y':'0'}} if operation == 'adjoint.compute_h_function' else {}))
    return s.collect(is_done)[0]


def browser_block(s, rows):
    """the data browser's first rows, every column"""
    s.send(cmd='data.page', **{'from_row': 0, 'count': rows, 'first_column': 1, 'columns': 500})
    evs, br = s.collect(lambda e: e.get('ev') == 'data.updated')
    s.collect(is_done)
    return br


def lent_columns_match(s, what, menu, item, first):
    """what (shown by nUmerics/menu's item) shows the stored columns from
    first on beside its own: they are the store's as it is now, read back
    from the store itself (the browser's Data)"""
    numerics_menu(s, menu, item)
    shown = browser_block(s, 2000)
    numerics_menu(s, 'h', 'd')  # stocHast/Data: the stored data back
    stored = browser_block(s, 2000)
    n = shown['rows'] if shown else 0
    lent = [row[first:] for row in shown['data'][:n]] if shown else []
    store = [row[first:] for row in stored['data'][:n]] if stored else []
    check('memory: %s shown again after a run grew the store shows the stored columns as they are now' % what,
          n > 1 and len(lent) == n and lent == store,
          '%d rows; first rows %s vs %s' % (n, lent[:2], store[:2]))


def section_memory():
    """The derived data sets (the adjoint, its H function, a histogram)
    show the stored columns beside their own; shown again from their menus
    after a longer run grew (and so moved) the store, they show it as it
    is now, and after a load nothing of the session before (W117). CVODE
    stopped by a step error, or by the Poincare map's 'Cannot zero RHS',
    leaves no memory behind for the next run to drop (asancheck's
    LeakSanitizer sees that; here, the runs go on)."""
    s = Server(args.server, BORROWED, verbose=args.v)
    s.collect(is_done)
    integrate(s)  # one period, 601 rows
    evs = numerics_menu(s, 'a', 'n')  # Averaging/New adjoint
    check('memory: the adjoint of the limit cycle is computed', not errors_of(evs), str(errors_of(evs)))
    evs = numerics_menu(s, 'a', 'm', {'string': lambda e: {'ok': 1, 'value': '0'}})  # Make H
    check('memory: its H function is computed', not errors_of(evs), str(errors_of(evs)))
    s.send(cmd='histogram.compute')  # the model's histogram
    s.collect(is_done)
    # another orbit, from elsewhere, long enough to grow the store
    # (the store starts with 5000 rows) once
    s.send(cmd='initial_conditions.set', values={'x': 0.5})
    s.collect(is_done)
    s.send(cmd='run.configure', total_time=60)
    s.collect(is_done)
    evs = integrate(s)
    st = [e for e in evs if e.get('ev') == 'session.state']
    check('memory: the longer run stores more rows than the store starts with', st and st[-1].get('rows', 0) > 5000,
          str(st and st[-1].get('rows')))
    # the stored columns each shows, from: the adjoint's node+1 = 3, the H
    # function's 4 (its own: phase, H, odd and even parts), the
    # histogram's 2 (a browser row is the columns in order, T first)
    lent_columns_match(s, 'the adjoint', 'a', 'a', 3)
    lent_columns_match(s, 'the H function', 'a', 'h', 4)
    lent_columns_match(s, 'the histogram', 'h', 'o', 2)
    # a load: a new session, with no adjoint or H function to show
    s.send(cmd='session.reload_model')
    s.answer_asks(is_done, {'choice': lambda e: {'key': 'd'}})
    for what, item in (('adjoint', 'a'), ('H function', 'h')):
        numerics_menu(s, 'a', item)
        br = browser_block(s, 10)
        check('memory: after a load, the %s menu item shows nothing of the session before' % what,
              br is not None and br['rows'] == 0, str(br and br['rows']))
    check('memory: the session is still up', s.alive())
    s.close()

    for model, why in ((CVODE_STOP, 'a delay longer than the maximal one'),
                       (CVODE_POINCARE, "the Poincare map's Cannot zero RHS")):
        s = Server(args.server, model, verbose=args.v)
        s.collect(is_done)
        first = errors_of(integrate(s))
        second = errors_of(integrate(s))
        check('memory: CVODE stopped by %s: each run says so, the session goes on' % why,
              len(first) == 1 and second == first and s.alive(), '%s then %s' % (first, second))
        s.close()


# ---- sessiondata: a session's stored rows come back in the main window's plot ----

def series_of(evs, win='graph_1'):
    """the last full series event of window win (the curves' rows), or None"""
    full = whole_series(evs, win)
    return full[-1] if full else None


PAGE_EVENTS = ['plot.series', 'plot.shown', 'nullcline.curves', 'direction_field.field', 'plot.marks', 'animation.frame', 'autox.info', 'autox.settings']  # web2's subscription


def section_sessiondata():
    for with_autox in (False, True):
        tag = 'sessiondata (%s)' % ('autoX diagram' if with_autox else 'no autoX')
        home1 = tempfile.mkdtemp(prefix='xpphome')
        s = Server(args.server, LECAR, env={'HOME': home1}, verbose=args.v)
        s.collect(is_done)
        s.send(cmd='events.subscribe', events=PAGE_EVENTS, encoding='f32')
        evs, _ = s.collect(is_done)
        # W against V, as a phase plane
        s.send(cmd='plot.set_2d', x='V', y='W')
        s.collect(is_done)
        s.send(cmd='run.initial')
        evs, _ = s.collect(is_done, timeout=30 * SLOW)
        saved = series_of(evs)
        if with_autox:
            open_autox(s)
            run_menu(s, 's')
            grab_hopf(s)
            run_menu(s, 'p', timeout=120 * SLOW)
        rows = saved['rows'] if saved else -1
        s.send(cmd='session.save_as', file='d1')
        s.collect(is_done, timeout=20 * SLOW)
        snap = os.path.join(s.run, 'd1.snapx')
        home2 = tempfile.mkdtemp(prefix='xpphome')
        s2 = Server(args.server, LECAR, env={'HOME': home2}, verbose=args.v)
        s2.collect(is_done)
        if os.path.exists(snap): shutil.copy(snap, s2.run)
        # the same server opens its own session again (the desktop window's File/Open)
        evs = open_session(s, 'd1')
        same = series_of(evs)
        check('%s: reopening the session in the same server keeps its rows in the plot' % tag,
              same is not None and same['rows'] == rows, 'saved %s, after open %s' % (rows, same and same['rows']))
        s.send(cmd='events.subscribe', events=PAGE_EVENTS, encoding='f32')
        evs, _ = s.collect(is_done)
        same = series_of(evs)
        check('%s: and after the page asks again' % tag, same is not None and same['rows'] == rows, str(same and same['rows']))
        s.close()
        shutil.rmtree(home1, ignore_errors=True)
        # the page's subscription is sent again after a model switch's new hello
        s2.send(cmd='events.subscribe', events=PAGE_EVENTS, encoding='f32')
        s2.collect(is_done)
        evs = open_session(s2, 'd1')
        got = series_of(evs)
        check('%s: the saved rows are in the main plot after Open session' % tag,
              rows > 0 and got is not None and got['rows'] == rows, 'saved %s, after open %s' % (rows, got and got['rows']))
        if saved and got and saved['rows'] == got['rows']:
            check('%s: the plotted values are the saved ones' % tag,
                  [c.get('data') for c in saved['columns']] == [c.get('data') for c in got['columns']] if 'columns' in saved else True)
        s2.send(cmd='events.subscribe', events=PAGE_EVENTS, encoding='f32')
        evs, _ = s2.collect(is_done)
        again = series_of(evs)
        check('%s: asking for the series again sends the saved rows' % tag,
              again is not None and again['rows'] == rows, str(again and again['rows']))
        s2.close()
        shutil.rmtree(home2, ignore_errors=True)


def play_recording(s, path):
    """play open path (the model switch's "save first?" answered don't),
    then play it at 8x to the end of its last step: the player event and
    the events"""
    s.send(cmd='recording.open', play=False, file=path)
    evs, _ = s.answer_asks(is_done, {'choice': lambda e: {'key': 'd'}}, timeout=60 * SLOW)
    pl = next((e for e in evs if e.get('ev') == 'recording.player'), None)
    if pl is None:
        return None, evs
    n = len(pl['steps'])
    s.send(cmd='recording.set_speed', speed=8)
    s.collect(is_done)
    s.send(cmd='recording.play')
    more, _ = s.collect(lambda e: is_state(e) and (e.get('player') or {}).get('step') == n
                        and e['player']['running'] == -1, timeout=300 * SLOW)
    return pl, evs + more + s.collect(is_done)[0]



def section_play():
    """W59b: a recording played back computes what the session recorded:
    (a) lecar's "hopf" set, its fixed point, an autoX steady run: the same
    diagram; (b) a range ended with / (a key the running job reads itself,
    recorded with where it was): the same rows"""
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_done)
    s.send(cmd='recording.start')
    s.collect(is_done)
    dg = Diagram().apply(hopf_steady(s))
    dg.apply(run_any(s, 's'))
    s.send(cmd='recording.stop', file='autox')
    s.collect(is_done)
    orig = [(p['br'], p['pt'], p['x'], p['y'], p['lab']) for p in dg.pts]
    pl, evs = play_recording(s, os.path.join(s.run, 'autox.recx'))
    again = [(p['br'], p['pt'], p['x'], p['y'], p['lab']) for p in Diagram().apply(evs).pts]
    errors = [e.get('error') for e in evs if e.get('ev') in ('session.message', 'done') and e.get('error')]
    check('play: the autoX steady run replayed gives the same diagram', pl and len(orig) > 5 and again == orig and not errors,
          '%s steps, %d vs %d points, %s' % (pl and len(pl['steps']), len(orig), len(again), errors))
    s.close()

    s = Server(args.server, HEAVY, verbose=args.v)
    s.collect(is_done)
    s.send(cmd='recording.start')
    s.collect(is_done)
    s.send(cmd='run.range')
    _, e = s.collect(lambda e: e.get('ev') == 'job.computing', timeout=30 * SLOW)
    s.collect(lambda e: e.get('ev') == 'job.progress' and e.get('n', 0) >= 200, timeout=60 * SLOW)
    s.send(cmd='run.stop')
    s.collect(is_done, timeout=120 * SLOW)
    s.send(cmd='recording.stop', file='range')
    s.collect(is_done)
    text = open(os.path.join(s.run, 'range.recx'), encoding='utf-8').read() if os.path.exists(os.path.join(s.run, 'range.recx')) else ''
    check('play: a / during a range is recorded in its step, with where the job was',
          '"abort":{"what":"integrate","rows":' in text, text[text.find('@steps'):][:400])
    # the replay's range ends where the recorded one did: had the / not
    # come there (the stop is armed at its row), the step would say so,
    # or run all 20 of heavy.odex's runs
    t0 = time.monotonic()
    pl, evs = play_recording(s, os.path.join(s.run, 'range.recx'))
    errors = [e.get('error') for e in evs if e.get('ev') in ('session.message', 'done') and e.get('error')]
    check('play: the replayed / ends the range at the recorded row', pl and not errors
          and (last_state(evs) or {}).get('player', {}).get('step') == 1, '%s, %.1f s' % (errors, time.monotonic() - t0))
    s.close()

    # (c) W59d: a recording begun after the diagram was computed starts from
    # the session as it was (its snapshot: the values, the diagram): the
    # Hopf point it grabs is there, and its periodic branch comes out the same
    s = Server(args.server, LECAR, verbose=args.v)
    s.collect(is_done)
    dg = Diagram().apply(hopf_steady(s))
    dg.apply(run_any(s, 's'))
    steady = len(dg.pts)
    s.send(cmd='recording.start')
    s.collect(is_done)
    grab_hopf(s)
    new = dg.apply(run_any(s, 'p', timeout=300 * SLOW)).pts[steady:]
    s.send(cmd='recording.stop', file='later')
    s.collect(is_done)
    branches = {p['br'] for p in new}
    orig = [(p['br'], p['pt'], p['x'], p['y'], p['lab']) for p in new]
    pl, evs = play_recording(s, os.path.join(s.run, 'later.recx'))
    again = [(p['br'], p['pt'], p['x'], p['y'], p['lab']) for p in Diagram().apply(evs).pts if p['br'] in branches]
    errors = [e.get('error') for e in evs if e.get('ev') in ('session.message', 'done') and e.get('error')]
    check('play: a recording begun after the steady run starts from its diagram: the periodic branch replayed is the same',
          pl and len(pl['steps']) == 2 and len(orig) > 5 and again == orig and not errors,
          '%s steps, %d vs %d points, %s' % (pl and len(pl['steps']), len(orig), len(again), errors))
    s.close()



def section_limits():
    """W298: 401 variables, 40 model/autoX parameters; continue the fortieth."""
    s = Server(args.server, os.path.join(here, 'models', 'autox_limits.odex'), verbose=args.v)
    try:
        startup, _ = s.collect(is_done)
        check('limits: 401 variables and 40 parameters load',
              len((last_state(startup) or {}).get('ics', [])) == 401
              and len((last_state(startup) or {}).get('pars', [])) == 40)
        open_autox(s)
        completed(s, dict(cmd='autox.configure_numerics', max_steps=4, output_every=1, suppress_branch_points=True))
        s.send(cmd='autox.configure_axes', parameter1='P39', variable='X400')
        evs, _ = s.collect(is_done)
        check('limits: the fortieth model parameter is available without selection', not errors_of(evs), str(errors_of(evs)))
        dg = Diagram().apply(run_any(s, 's'))
        check('limits: autoX computes four points beyond 400 variables', len(dg.pts) == 4, str(len(dg.pts)))
        check('limits: the fortieth parameter gives x400=p39', len(dg.pts) == 4
              and dg.pts[-1]['x'] > 0 and all(abs(p['x']-p['y']) < 1e-8 for p in dg.pts))
        members = save_diagram(s, 'limits.snapx')
        text = members.get('autox/diagram.csv', '')
        rows = diagram_rows(text.encode('utf-8'))
        check('limits: snapshot stores all forty parameters and 401 states', len(rows) == 4 and 'par40' in rows[-1]
              and abs(float(rows[-1]['par40'])-float(rows[-1]['u0.X400'])) < 1e-8)
        reopened = load_diagram(s, os.path.join(s.run, 'limits.snapx'))
        check('limits: reopening the snapshot restores the expanded diagram',
              not errors_of(reopened) and len(Diagram().apply(reopened).pts) == 4, str(errors_of(reopened)))
        s.send(cmd='autox.grab', label=int(rows[-1]['lab']) if rows else -1)
        evs, _ = s.collect(is_done)
        state = last_state(evs) or {}
        check('limits: grabbing restores the fortieth parameter and final state', rows and not errors_of(evs)
              and abs({k.upper(): v for k, v in state.get('pars', [])}.get('P39', -1)-float(rows[-1]['par40'])) < 1e-8
              and abs(dict(state.get('ics', [])).get('X400', -1)-float(rows[-1]['u0.X400'])) < 1e-8)
        again = Diagram().apply(reopened).apply(run_any(s, 'e'))
        check('limits: the saved label restarts with the added parameter', len(again.pts) >= 4
              and all(abs(p['x']-p['y']) < 1e-8 for p in again.pts))
    finally:
        s.close()

def section_nagumo():
    """W345/#406: every finite HomCont point against the exact front speed.

    Linear projections have O(exp(-T/sqrt(2))) truncation error; see finding
    50 for the derivation and the mesh/interval refinement behind these bounds.
    These are scientific error bounds, independent of machine speed.
    """
    speed_bound = 5e-6  # T=20, NTST=80, NCOL=4; truncation dominates.
    long_bound = 5e-7  # T=24 reduces the squared endpoint tail by exp(-4/sqrt(2)).
    homcont_calc = 'homoclinic'  # Native calculation name; XPPAUT codes are import-only.
    description = read_description(BASE / 'tstauto_tstheti_homcont.json')
    maxima = {}
    for name, interval, ntst in (('baseline', 20, 80), ('mesh', 20, 160), ('interval', 24, 160)):
        d = copy.deepcopy(description)
        d['runs'][0]['start']['settle'] = interval
        # Seed the same centred front; the starting profile is integrated,
        # never fitted to or substituted for autoX's continued speeds.
        u = 1 / (1 + math.exp(-interval / (2 * math.sqrt(2))))
        d['initial_conditions'] = dict(u=u, up=-u * (1-u) / math.sqrt(2))
        for r in d['runs']:
            r['autox']['mesh_intervals'] = ntst
        data, count, _, ranges = compute(args.server, d, 1)
        rows = diagram_rows(data)
        parameters = [float(r['par1']) for r in rows]
        errors = [abs(float(r['par2']) - (1-2*float(r['par1']))/math.sqrt(2)) for r in rows]
        maxima[name] = max(errors)
        check('nagumo %s: both directions span a=0.1..0.9 through HomCont' % name,
              len(ranges) == 2 and count > 100 and min(parameters) < .1 and max(parameters) > .9
              and all(r['calc'] == homcont_calc and float(r['per']) == interval for r in rows),
              '%d points, a=%g..%g' % (count, min(parameters), max(parameters)))
        check('nagumo %s: each orbit connects near u=1 to near u=0' % name,
              all(.99 < float(r['u0.U']) < 1 and 0 < float(r['ulo.U']) < .01
                  and float(r['u0.UP']) < 0 and float(r['ulo.UP']) < -.1 for r in rows))
        bound = long_bound if name == 'interval' else speed_bound
        check('nagumo %s: every c(a) agrees with (1-2*a)/sqrt(2)' % name,
              all(math.isfinite(e) and e <= bound for e in errors),
              'max error %.9g, bound %.9g' % (maxima[name], bound))
        print('nagumo: %s T=%d NTST=%d NCOL=4 points=%d max speed error=%.9g' %
              (name, interval, ntst, count, maxima[name]))
    check('nagumo: doubling the mesh leaves the truncation error unchanged',
          abs(maxima['mesh'] - maxima['baseline']) < 1e-8,
          'NTST80 %.9g, NTST160 %.9g' % (maxima['baseline'], maxima['mesh']))
    check('nagumo: extending the interval reduces the maximum speed error',
          maxima['interval'] < maxima['mesh'] / 5,
          'T20 %.9g, T24 %.9g' % (maxima['mesh'], maxima['interval']))



def section_hopf_reduced():
    """W363: the BT normal form continues from imaginary to real eigenvalues."""
    with tempfile.TemporaryDirectory() as folder:
        model = os.path.join(folder, 'bt.odex')
        with open(model, 'w') as f:
            f.write(versioned("x'=y\ny'=a+b*y+x*x\npar a=-0.25,b=-0.1\ninit x=-0.5,y=0\n"))
        constants = dict(step_size=.02, min_step_size=1e-6, max_step_size=.05, max_steps=40, output_every=5,
                         parameter_min=-.2, parameter_max=.2, mesh_intervals=15, max_branch_switches=0, parameter_tolerance=1e-9, state_tolerance=1e-9, special_point_tolerance=1e-8)
        d = dict(model=model, runs=[
            dict(name='steady', kind='equilibrium', start=dict(state='current'),
                 parameter='b', autox=constants),
            dict(name='hopf', kind='two_parameter', start=dict(run='steady', type='hopf', index=1),
                 parameters=['a', 'b'], autox=dict(constants, step_size=.03, max_step_size=.1, max_steps=70, parameter_min=-2, parameter_max=1))])
        data, _, _, ranges = compute(args.server, d, 1)
        rows = diagram_rows(data)[slice(*ranges['hopf'])]
        x = [float(r['u0.X']) for r in rows]
        check('codim: BT is a typed label on the Hopf curve', sum(r['itp']=='bogdanov_takens_from_hopf' for r in rows)==1)
        check('reduced Hopf: continuation crosses BT onto the neutral saddle',
              min(x) < -.4 and max(x) > .4 and any(r['flag2'] == 'neutral_saddle' for r in rows), str((min(x), max(x))))
        check('reduced Hopf: every point solves a=-x^2, b=0, y=0',
              all(abs(float(r['par1']) + xx*xx) < 1e-5 and abs(float(r['par2'])) < 1e-5
                  and abs(float(r['u0.Y'])) < 1e-5 for r, xx in zip(rows, x)),
              str([max(abs(float(r['par1']) + xx*xx) for r, xx in zip(rows, x)),
                   max(abs(float(r['par2'])) for r in rows), max(abs(float(r['u0.Y'])) for r in rows)]))
        check('reduced Hopf: negative kappa is labelled neutral saddle, never Hopf',
              all(r['flag2'] == ('neutral_saddle' if xx > 0 else 'hopf') for r, xx in zip(rows, x)))
        first_saddle = next(i for i, xx in enumerate(x) if xx > 0)
        period = float(rows[first_saddle-1]['per'])
        check('reduced Hopf: neutral saddle retains the last positive-kappa period',
              all(float(r['per']) == period for r in rows[first_saddle:]))

def section_codim():
    """W364: the transverse pair crosses zero on the fold; named booleans gate detection."""
    d = read_description(BASE / 'zero_hopf.json')
    s = Server(args.server, d['model'])
    try:
        completed(s)
        diagram = autoxcompare.Diagram().apply(open_autox(s))
        ranges = {}
        settings = completed(s, dict(cmd='events.subscribe', events=['autox.info', 'autox.settings']))
        flags = next(e['special_points'] for e in settings if e.get('ev') == 'autox.settings')
        check('codim: BT CP ZH GH enabled by default', flags == dict(bogdanov_takens=True, cusp=True, zero_hopf=True, generalized_hopf=True, resonance_1_1=True, resonance_1_2=True, resonance_1_3=True, resonance_1_4=True, fold_flip=True, fold_neimark_sacker=True, flip_neimark_sacker=True, double_neimark_sacker=True))
        for mode, changes in (('default', {}), ('ZH disabled', {'zero_hopf': False}),
                              ('all disabled', dict.fromkeys(flags, False)), ('restored', dict.fromkeys(flags, True))):
            s.close()
            s = Server(args.server, d['model'])
            completed(s)
            completed(s, dict(cmd='events.subscribe', events=['autox.info', 'autox.settings']))
            diagram = autoxcompare.Diagram().apply(open_autox(s))
            ranges = {}
            execute_run(s, d['runs'][0], diagram, ranges)
            if changes: completed(s, dict(cmd='autox.configure_numerics', special_points=changes))
            execute_run(s, d['runs'][1], diagram, ranges)
            lo, hi = ranges['fold']
            rows = diagram_rows(session_diagram(s))[lo:hi]
            zh = [r for r in rows if r['itp'] == 'zero_hopf_from_fold']
            expected = mode in ('default', 'restored')
            check('codim: ' + mode, len(zh) == (1 if expected else 0)
                  and (not zh or abs(float(zh[0]['continued_parameter'])) < 1e-4))
        for bad in ({'zero_hopf': 0}, {'zero_hopf': 'false'}, {'unknown': False}):
            s.send(cmd='autox.configure_numerics', special_points=bad, max_steps=7)
            events, _ = s.collect(is_done)
            check('codim: invalid named boolean refuses transaction ' + str(bad), len(errors_of(events)) == 1)
            after = completed(s, dict(cmd='events.subscribe', events=['autox.settings']))
            current = next(e for e in after if e.get('ev') == 'autox.settings')
            check('codim: refusal applies nothing', current['numerics']['max_steps'] == 50 and current['special_points'] == flags)
    finally:
        s.close()


    # The same zero-Hopf point reached along the Hopf curve.
    hopf = copy.deepcopy(d)
    hopf['runs'][0]['parameter'] = 'b'
    hopf['runs'][0]['autox'].update(parameter_min=-.2, parameter_max=.2)
    hopf['runs'][1].update(name='hopf', start=dict(run='steady', type='hopf', index=1), parameters=['a','b'])
    hopf['runs'][1]['autox'].update(max_steps=200, parameter_min=-2, parameter_max=.1)
    data, _, _, intervals = compute(args.server, hopf, 1)
    rows = diagram_rows(data)[slice(*intervals['hopf'])]
    zh = [r for r in rows if r['itp']=='zero_hopf_from_hopf']
    check('codim: ZH located on the Hopf curve', len(zh)==1 and abs(float(zh[0]['u0.X']))<1e-4)
    with tempfile.TemporaryDirectory() as folder:
        model = os.path.join(folder, 'bautin.odex')
        with open(model, 'w') as f:
            f.write(versioned("x'=-y+a*x+b*x*(x*x+y*y)\ny'=x+a*y+b*y*(x*x+y*y)\npar a=-0.1,b=-0.1\ninit x=0,y=0\n"))
        constants = dict(step_size=.02, min_step_size=1e-6, max_step_size=.05, max_steps=30, output_every=10,
                         parameter_min=-.2, parameter_max=.2, mesh_intervals=15, max_branch_switches=0, parameter_tolerance=1e-9, state_tolerance=1e-9, special_point_tolerance=1e-8)
        normal = dict(model=model, runs=[
            dict(name='steady',kind='equilibrium',start=dict(state='current'),parameter='a',autox=constants),
            dict(name='hopf',kind='two_parameter',start=dict(run='steady',type='hopf',index=1),parameters=['b','a'],autox=constants)])
        data, _, _, intervals = compute(args.server, normal, 1)
        rows = diagram_rows(data)[slice(*intervals['hopf'])]
        gh = [r for r in rows if r['itp']=='generalized_hopf_from_hopf']
        check('codim: GH located where the Bautin normal form has l1=2*b=0', len(gh)==1
              and abs(float(gh[0]['continued_parameter']))<1e-6)

    # W400: independent closed forms for the uncoupled Hill equation.
    cycles = read_description(BASE / 'hill_cycles_torus.json')
    cycles['parameters'] = {'q': 0.0}
    cycles['runs'][3]['autox']['max_steps'] = 15  # reaches R3 before the degenerate 1:2 endpoint
    data, _, _, intervals = compute(args.server, cycles, 1)
    rows = autoxcompare.stored_rows(data, intervals)
    for run, kind, exact in (('torus_up', 'resonance_1_4_from_neimark_sacker', 9/16), ('torus_down', 'resonance_1_3_from_neimark_sacker', 4/9)):
        labels = [r for r in rows[run] if r['itp'] == kind]
        constants = next(r['autox'] for r in cycles['runs'] if r['name'] == run)
        location_tolerance = 2 * constants['special_point_tolerance'] * (1 + constants['max_step_size'])
        check('cycle codim: q=0 ' + kind + ' at closed-form d=' + str(exact),
              len(labels) == 1 and abs(float(labels[0]['continued_parameter2']) - exact) < location_tolerance)
    cycles = read_description(BASE / 'hill_cycles_torus.json')
    s = Server(args.server, cycles['model'])
    try:
        completed(s)
        completed(s, dict(cmd='events.subscribe', events=['autox.info', 'autox.settings']))
        diagram = autoxcompare.Diagram().apply(open_autox(s)); intervals = {}
        for run in cycles['runs'][:2]: execute_run(s, run, diagram, intervals)
        completed(s, dict(cmd='autox.configure_numerics', special_points={'resonance_1_4': False}))
        execute_run(s, cycles['runs'][2], diagram, intervals)
        rows = autoxcompare.stored_rows(session_diagram(s), intervals)['torus_up']
        check('cycle codim: disabling R4 keeps the curve running without its label',
              any('continued_parameter2' in r for r in rows) and not any(r['itp'] == 'resonance_1_4_from_neimark_sacker' for r in rows))
    finally:
        s.close()


    # W404: independent fixed-point and secondary-curve identities of the map.
    for curve in ('tr','pd','lp'):
        maps = read_description(BASE / ('henon_rot_' + curve + '.json'))
        if curve=='tr':
            maps['runs'][2].pop('skip',None)
            maps['runs'][2]['autox']['max_steps']=48 # before the traced singular R1 endpoint
        data, _, _, intervals = compute(args.server, maps, 1)
        runs = autoxcompare.stored_rows(data, intervals)
        for direction in ('up','down'):
            rows = runs[direction]
            residual = max(abs(float(r['continued_parameter2'])-1.) if curve=='tr' else
                           abs(float(r['continued_parameter'])-(3. if curve=='pd' else -1.)*
                               (1+float(r['continued_parameter2']))**2/4) for r in rows)
            check('map codim: ' + curve + '/' + direction + ' closed-form curve', residual < 1e-4)
            if curve=='tr':
                exact={'R1':-1.,'R2':3.,'R3':1.25,'R4':0.,'TTR':1.1}
                for kind,index in autoxcompare.labelled(rows,'two_parameter'):
                    if kind in exact:
                        check('map codim: ' + direction + ' ' + kind + ' closed-form place',
                              abs(float(rows[index]['continued_parameter'])-exact[kind])<3e-4)
    # W419: torus angles past pi and below 0, every resonance at its closed form's angle modulo 2*pi.
    # torus_up's last R1, at b = 0 beside the branch point, is W585's (docs/studies/w585_torus_r2_check.py).
    # henon_rot_th continues the rotation's angle th itself; res_cycles' is 2*pi*(1-sqrt(2*w)), w = -c-mu, mu = 1/2.
    folded = {'R1': 0., 'R2': math.pi, 'R3': 2 * math.pi / 3, 'R4': math.pi / 2}
    for name, angle, expected in (
            ('henon_rot_th', lambda r: float(r['continued_parameter']), {'turn': ['R4', 'R3', 'R2', 'R3', 'R4']}),
            ('res_cycles', lambda r: 2 * math.pi * (1 - math.sqrt(2 * (-float(r['continued_parameter']) - .5))),
             {'torus_down': ['R1', 'R4', 'R3', 'R2', 'R3'], 'torus_up': ['R4', 'R3', 'R2', 'R3', 'R4', 'R1']})):
        data, _, _, intervals = compute(args.server, read_description(BASE / (name + '.json')), 1)
        runs = autoxcompare.stored_rows(data, intervals)
        for run, kinds in expected.items():
            labels = [(kind, angle(runs[run][index])) for kind, index in autoxcompare.labelled(runs[run], 'two_parameter')
                      if kind in folded]
            check('torus past pi: %s/%s labels %s at their angles' % (name, run, ' '.join(kinds)),
                  [kind for kind, _ in labels] == kinds and
                  all(abs(abs(math.remainder(a, 2 * math.pi)) - folded[kind]) < 1e-3 for kind, a in labels))
    # A disabled map kind leaves the curve and the other tests active; all
    # settings and typed point codes survive a session round trip.
    maps = read_description(BASE / 'henon_rot_tr.json')
    maps['runs'][2].pop('skip',None)
    maps['runs'][2]['autox']['max_steps']=40 # includes R3 and R4; avoids the singular R1 endpoint
    # The server stops before its session's folder goes: Windows cannot
    # remove a folder a running process still holds (CI run 38913397765).
    with tempfile.TemporaryDirectory() as folder:
        s = Server(args.server, maps['model'])
        try:
            completed(s)
            completed(s, dict(cmd='value.set_parameters',values=dict(maps['parameters'])))
            completed(s, dict(cmd='events.subscribe', events=['autox.info', 'autox.settings']))
            diagram=autoxcompare.Diagram().apply(open_autox(s)); intervals={}
            execute_run(s,maps['runs'][0],diagram,intervals)
            completed(s,dict(cmd='autox.configure_numerics', special_points={'double_neimark_sacker':False,'resonance_1_4':False}))
            execute_run(s,maps['runs'][2],diagram,intervals)
            rows=autoxcompare.stored_rows(session_diagram(s),intervals)['down']
            kinds=[kind for kind,_ in autoxcompare.labelled(rows,'two_parameter')]
            check('map codim: disabling TTR/R4 preserves R3', 'TTR' not in kinds and 'R4' not in kinds and 'R3' in kinds)
            path='maps.snapx'  # a file of the server's own folder
            completed(s,dict(cmd='session.save_as', file=path, data=False))
            before=session_diagram(s)
            check('map codim: session load accepts current layout', not errors_of(open_session(s,path)))
            check('map codim: session restores typed diagram', session_diagram(s)==before)
            events=completed(s,dict(cmd='events.subscribe', events=['autox.settings']))
            flags=next(e['special_points'] for e in events if e.get('ev')== 'autox.settings')
            check('map codim: session restores named switches', not flags['double_neimark_sacker'] and not flags['resonance_1_4'] and flags['fold_flip'])
        finally:
            s.close()


def pp2_equations(u, p):
    """AUTO-07p's and XPPAUT's pp2 (par1 = 3, par2 = 5, par3 = 3): the right-hand side and its Jacobian, by hand"""
    import numpy as np
    u0, u1 = u
    e = math.exp(-5*u0)
    return (np.array([3*u0*(1-u0) - u0*u1 - p*(1-e), -u1 + 3*u0*u1]),
            np.array([[3*(1-2*u0) - u1 - 5*p*e, -u0], [3*u1, -1 + 3*u0]]))


def section_switch():
    """W563/#628: a restart at a branch point of equilibria with switch follows the other branch.

    Independent of AUTO-07p and of the baselines: every point of the switched branch is an equilibrium of
    the model's own equations, its stored multipliers are e^lambda of the Jacobian's eigenvalues (numpy),
    and the pitchfork's branch is x = sqrt(r) from the first step.
    """
    import numpy as np
    d = read_description(BASE / 'tstauto_pf_switch.json')
    data, _, _, ranges = compute(args.server, d, 1)
    steady = diagram_rows(data)[slice(*ranges['steady'])]
    rows = diagram_rows(data)[slice(*ranges['switched'])]
    x = np.array([float(r['u0.X']) for r in rows])
    r_ = np.array([float(r['continued_parameter']) for r in rows])
    check('switch pf: the branch point is typed and the switched branch is a new branch',
          sum(r['itp'] == 'branch_point' for r in steady) == 1 and {r['branch'] for r in rows} == {'1'} and {r['ibr'] for r in rows} == {'2'}, str({r['ibr'] for r in rows}))
    check('switch pf: every point solves r*x - x^3 = 0 with x = sqrt(r) (x > 0 after the branch point)',
          len(rows) > 10 and np.abs(r_*x - x**3).max() < 1e-6 and np.abs(x[1:]**2 - r_[1:]).max() < 1e-6 and (x[1:] > 0).all(),
          str((np.abs(x[1:]**2 - r_[1:]).max(), x[:3])))
    stable = [(float(r['evr1']), rr, xx) for r, rr, xx in zip(rows[1:], r_[1:], x[1:]) if float(r['evr1']) != 0.]
    check('switch pf: the stored multipliers are e^(r - 3 x^2) of the new branch (stable: below 1)',
          len(stable) > 10 and all(abs(m - math.exp(rr - 3*xx*xx)) < 1e-8 and m < 1 for m, rr, xx in stable), str(len(stable)))

    pp2 = pp2_equations
    d = read_description(BASE / 'tstauto_pp2_switch.json')
    data, _, _, ranges = compute(args.server, d, 1)
    rows = diagram_rows(data)[slice(*ranges['switched'])]
    worst = eigen = u1_max = 0.
    for r in rows:
        u = (float(r['u0.U0']), float(r['u0.U1']))
        f, jac = pp2(u, float(r['continued_parameter']))
        worst = max(worst, np.abs(f).max())
        u1_max = max(u1_max, abs(u[1]))
        recorded = [complex(float(r['evr%d' % k]), float(r['evi%d' % k])) for k in (1, 2)]
        if any(recorded):  # the branch's second point has no stability (not computed: all zero)
            eigen = max(eigen, max(abs(a - b) for a, b in zip(sorted(np.exp(np.linalg.eigvals(jac)), key=lambda z: (z.real, z.imag)),
                                                              sorted(recorded, key=lambda z: (z.real, z.imag)))))
    check('switch pp2: every point of the other branch solves the model with u1 = 0, and its multipliers are e^lambda',
          len(rows) > 20 and worst < 1e-6 and u1_max < 1e-6 and eigen < 1e-6, str((len(rows), worst, u1_max, eigen)))
    # W569 (#634): the branch turns between its second and third points, where the equilibrium's Jacobian is
    # singular (a fold of the model's own equations: f = 0 and det J = 0 solved with scipy), and it is labelled
    from scipy.optimize import fsolve
    u0_fold, p_fold = fsolve(lambda v: [pp2((v[0], 0.), v[1])[0][0], np.linalg.det(pp2((v[0], 0.), v[1])[1])], (0.41, 0.83), xtol=1e-12)
    folds = [r for r in rows if r['itp'] == 'fold']
    near = len(folds) == 1 and abs(float(folds[0]['continued_parameter']) - p_fold) < 1e-9
    check('switch pp2: the fold between the second and third points is labelled once, at the fold of the equations (numpy/scipy)',
          near and abs(float(folds[0]['u0.U0']) - u0_fold) < 1e-4 and rows.index(folds[0]) == 2,
          str((p_fold, u0_fold, [(r['continued_parameter'], r['u0.U0']) for r in folds])))

    # The direction a branch point keeps was made for one parameter: another is refused, naming both.
    s = Server(args.server, d['model'])
    try:
        completed(s)
        completed(s, dict(cmd='events.subscribe', events=['autox.info', 'autox.settings']))
        diagram = autoxcompare.Diagram().apply(open_autox(s))
        intervals = {}
        execute_run(s, d['runs'][0], diagram, intervals)
        d['runs'][1]['parameter'] = 'par1'
        d['runs'][1]['autox'].update(parameter_min=0, parameter_max=5)  # par1 = 3 starts inside
        s_events = None
        try:
            execute_run(s, d['runs'][1], diagram, intervals)
        except RuntimeError as e:
            s_events = str(e)
        check('switch: a restart in another parameter than the branch point keeps is an error naming both',
              s_events is not None and 'par0' in s_events and 'par1' in s_events and 'switching continues the same parameter' in s_events, str(s_events))
    finally:
        s.close()


def section_first_steps():
    """W572/#637: the tests of a branch's first steps, where AUTO-07p's and XPPAUT's start at the third point.

    Each case is a published model run so that a special point lies in a step they never test: between the
    start and the second point, or the second and third point of a branch (the equilibria's second point is
    computed and not stored), or the second and third point of a branch started by a switch, or between the
    first and second point of a cycle branch. The new label is settled by the model's own equations, with
    numpy and scipy and none of the continuation: the Jacobian's eigenvalues of an equilibrium, the exact
    radial equations of a radial oscillator's cycles, the monodromy matrix of the forced predator-prey
    cycle. Where no zero lies the diagrams are unchanged (autoxcompare's baselines, byte for byte), and the
    stored stability of the point the tests start from stays as it was (the second point of a switched branch).
    """
    import numpy as np
    from scipy.integrate import solve_ivp
    from scipy.optimize import brentq, fsolve

    def rows_of(d, name, threads=1):
        data, _, _, ranges = compute(args.server, d, threads)
        return diagram_rows(data)[slice(*ranges[name])]

    def kinds(rows, kind):
        return [(i, r) for i, r in enumerate(rows) if r['itp'] == kind]

    def run_settings(r, **values):
        r['autox'].update(values)
        if 'step_size' in values:
            r['autox']['max_step_size'] = abs(values['step_size'])
        r['autox']['output_every'] = 1
        return r

    # pp2 (Doedel 1984; AUTO-07p's and XPPAUT's demo), the equilibria u0 = 1/3 and u1 = 0 (the exact solutions of its equations)
    e53 = math.exp(-5 / 3)

    def pp2_case(par0, u0, u1, steps, marks=None, step=0.05):
        d = read_description(BASE / 'tstauto_pp2.json')
        d['runs'] = d['runs'][:1]
        d['parameters'], d['initial_conditions'] = {'par0': par0}, {'u0': u0, 'u1': u1}
        r = run_settings(d['runs'][0], step_size=step, max_steps=steps)
        if marks:
            r['marks'] = marks
        return d

    def on_third(par0):  # u1 on the branch u0 = 1/3 at par0: f0 = 0
        return 2 - 3 * par0 * (1 - e53)

    def hopf_par0():  # the Jacobian's trace is zero on u0 = 1/3
        return brentq(lambda p: np.trace(pp2_equations((1 / 3, on_third(p)), p)[1]), 0.6, 0.7, xtol=1e-14)

    hb = hopf_par0()
    for par0, where in ((0.66, 'first and second point'), (0.65, 'second and third point')):
        rows = rows_of(pp2_case(par0, 1 / 3, on_third(par0), 8), 'steady')
        found = kinds(rows, 'hopf')
        f, jac = pp2_equations((float(found[0][1]['u0.U0']), float(found[0][1]['u0.U1'])), float(found[0][1]['continued_parameter'])) if found else (None, None)
        eig = np.linalg.eigvals(jac) if found else None
        check('first steps pp2: the Hopf point between the %s of the branch is labelled once, at the trace of the Jacobian = 0 (scipy)' % where,
              len(found) == 1 and found[0][0] == 1 and abs(float(found[0][1]['continued_parameter']) - hb) < 1e-5 and
              abs(f).max() < 1e-6 and abs(eig.real).max() < 1e-3 and abs(eig.imag).min() > 0.5,
              str((hb, [(i, r['continued_parameter']) for i, r in found], eig)))

    def u1_zero(u0):  # the branch u1 = 0: par0 as a function of u0
        return 3 * u0 * (1 - u0) / (1 - math.exp(-5 * u0))

    u0_fold, p_fold = fsolve(lambda v: [pp2_equations((v[0], 0.), v[1])[0][0], np.linalg.det(pp2_equations((v[0], 0.), v[1])[1])],
                             (0.41, 0.83), xtol=1e-12)
    for u0, where, index in ((0.45, 'first and second point', 1), (0.36, 'second and third point', 1)):
        rows = rows_of(pp2_case(u1_zero(u0), u0, 0., 4), 'steady')
        found = kinds(rows, 'fold')
        check('first steps pp2: the fold between the %s of the branch u1 = 0 is labelled once, at the fold of the equations (scipy)' % where,
              len(found) == 1 and found[0][0] == index and abs(float(found[0][1]['continued_parameter']) - p_fold) < 1e-7 and
              abs(float(found[0][1]['u0.U0']) - u0_fold) < 1e-3, str((p_fold, u0_fold, [(i, r['continued_parameter'], r['u0.U0']) for i, r in found])))
    # from u0 = 0.43 the branch passes the fold and then u0 = 1/3, where the Jacobian's second eigenvalue, -1 + 3 u0, is zero: its
    # branch point (the branch u0 = 1/3 crosses there), between the second and third point
    rows = rows_of(pp2_case(u1_zero(0.43), 0.43, 0., 5), 'steady')
    found, folds = kinds(rows, 'branch_point'), kinds(rows, 'fold')
    check('first steps pp2: the branch point of u1 = 0 (u0 = 1/3, a zero eigenvalue) and the fold before it are labelled once each',
          len(found) == 1 and len(folds) == 1 and abs(float(found[0][1]['u0.U0']) - 1 / 3) < 1e-4 and
          abs(float(folds[0][1]['continued_parameter']) - p_fold) < 1e-7, str(([(i, r['u0.U0']) for i, r in found], [(i, r['continued_parameter']) for i, r in folds])))

    # and between the first and second point, from either side (the second point of the other direction is below it)
    for u0, step in ((0.35, -0.05), (0.31, 0.05)):
        rows = rows_of(pp2_case(u1_zero(u0), u0, 0., 3, step=step), 'steady')
        found = kinds(rows, 'branch_point')
        check('first steps pp2: the branch point u0 = 1/3 between the first and second point (start u0 = %g, step %g) is labelled once' % (u0, step),
              len(found) == 1 and found[0][0] == 1 and abs(float(found[0][1]['u0.U0']) - 1 / 3) < 1e-4,
              str([(i, r['u0.U0']) for i, r in found]))

    # a user zero: a mark crossed where the tests were not made, from the start and from the second point
    for mark, where in ((0.67, 'first and second point'), (0.69, 'second and third point')):
        rows = rows_of(pp2_case(0.66, 1 / 3, on_third(0.66), 6, marks=[['par0', mark]]), 'steady')
        found = kinds(rows, 'user_point')
        check('first steps pp2: the user zero par0 = %g between the %s is labelled once, at the mark' % (mark, where),
              len(found) == 1 and abs(float(found[0][1]['continued_parameter']) - mark) < 1e-7 and found[0][0] <= 2,
              str([(i, r['continued_parameter']) for i, r in found]))

    # the switched branch's second step: pp2 (u1 = 0) rises past the mark twice, before and after its fold
    d = read_description(BASE / 'tstauto_pp2_switch.json')
    d.pop('differences')
    d['runs'][1]['marks'] = [['par0', 0.8317]]
    rows = rows_of(d, 'switched')
    found = kinds(rows, 'user_point')
    roots = [brentq(lambda u: u1_zero(u) - 0.8317, lo, hi, xtol=1e-14) for lo, hi in ((0.34, u0_fold), (u0_fold, 0.6))]
    check('first steps pp2 switched: the user zero between the second and third point is labelled, and the one after the fold, at the roots of the equations',
          len(found) == 2 and found[0][0] == 2 and all(abs(float(r['continued_parameter']) - 0.8317) < 1e-5 and abs(float(r['u0.U0']) - u) < 1e-3
                                                         for (_, r), u in zip(found, roots)), str(([(i, r['continued_parameter'], r['u0.U0']) for i, r in found], roots)))

    # Lorenz (1963; AUTO-07p's lrz): the trivial branch has its pitchfork at r = 1, the switched branch a Hopf point at
    # r = sigma (sigma + b + 3) / (sigma - b - 1); a step that puts the second and third point on its two sides
    sigma, b = 10., 8 / 3
    r_hopf = sigma * (sigma + b + 3) / (sigma - b - 1)
    d = read_description(BASE / 'lrz_cycles.json')
    steady = copy.deepcopy(d['runs'][0])
    steady['start'] = {'state': 'current'}
    steady['autox'].update(step_size=0.1, max_step_size=0.1, max_steps=30, output_every=5, parameter_min=-1, parameter_max=40, mesh_intervals=15)
    switched = copy.deepcopy(steady)
    switched.update(name='switched', start={'run': 'steady', 'type': 'branch_point', 'index': 1}, switch=True)
    run_settings(switched, step_size=10., max_steps=5)
    d['runs'] = [steady, switched]
    d['parameters'], d['initial_conditions'] = {'r': 0., 's': sigma, 'b': b}, {'x': 0., 'y': 0., 'z': 0.}
    rows = rows_of(d, 'switched')
    found = kinds(rows, 'hopf')
    x, y, z, rr = (float(found[0][1][k]) for k in ('u0.X', 'u0.Y', 'u0.Z', 'continued_parameter')) if found else (0,) * 4
    eig = np.linalg.eigvals(np.array([[-sigma, sigma, 0.], [rr - z, -1., -x], [y, x, -b]]))
    check('first steps Lorenz switched: the Hopf point between the second and third point is labelled once, at sigma (sigma + b + 3) / (sigma - b - 1)',
          len(found) == 1 and found[0][0] == 2 and abs(rr - r_hopf) < 2e-3 and sorted(abs(eig.real))[0] < 1e-3 and sorted(abs(eig.imag))[-1] > 5,
          str((r_hopf, rr, eig, [(i, r['continued_parameter']) for i, r in found])))
    check('first steps Lorenz switched: the second point keeps its stability not computed (the Hopf test made there stores none)',
          not any(float(rows[1]['evr%d' % k]) or float(rows[1]['evi%d' % k]) for k in (1, 2, 3)), str(rows[1]))

    # cycles: the first and second point (the loop's first test is at the second point). Each case restarts from the
    # last point of a run that stops just before the zero, with a step that carries the second point past it.
    def restarted(base, steps, step, marks=None):
        d = read_description(BASE / base)
        first, second = copy.deepcopy(d['runs'][1]), copy.deepcopy(d['runs'][1])
        d['runs'] = [d['runs'][0], first, second]
        run_settings(first, max_steps=steps)
        second.update(name='restart', start={'run': first['name'], 'type': 'EP', 'index': 1})
        run_settings(second, step_size=step, max_steps=4)
        if marks:
            second['marks'] = marks
        d.pop('differences', None)
        return d

    # lamom2 (XPPAUT's demo): x' = x (a + b s - s^2) - y (1 + q s), y' = y (...) + x (...), s = x^2 + y^2: the radius' equation
    # is r' = r (a + b r^2 - r^4), its cycles r^2 = rho with a + b rho - rho^2 = 0, and the fold of cycles a = -b^2/4, rho = b/2
    rows = rows_of(restarted('tstauto_lamom2.json', 15, 0.1), 'restart')
    found = kinds(rows, 'cycle_fold')
    rho = float(found[0][1]['u0.X']) ** 2 + float(found[0][1]['u0.Y']) ** 2 if found else 0
    check('first steps lamom2 cycles: the fold of cycles between the first and second point is labelled once, at a = -1/4 and rho = 1/2 (the radial equation)',
          len(found) == 1 and found[0][0] == 1 and abs(float(found[0][1]['continued_parameter']) + 0.25) < 1e-6 and abs(rho - 0.5) < 5e-4,
          str(([(i, r['continued_parameter']) for i, r in found], rho)))
    rows = rows_of(restarted('tstauto_lamom2.json', 15, 0.05, marks=[['a', -0.247]]), 'restart')
    found = kinds(rows, 'user_point')
    rhos = [float(r['u0.X']) ** 2 + float(r['u0.Y']) ** 2 for _, r in found]
    roots = [(1 - s * math.sqrt(1 - 4 * 0.247)) / 2 for s in (1, -1)]  # a + rho - rho^2 = 0: rho = (1 -+ sqrt(1 + 4 a)) / 2
    check('first steps lamom2 cycles: the user zero a = -0.247 between the first and second point is labelled, and the one after the fold, on the two radii of the equation',
          len(found) == 2 and found[0][0] == 1 and all(abs(float(r['continued_parameter']) + 0.247) < 2e-5 for _, r in found) and
          abs(rhos[0] - roots[0]) < 1e-3 and abs(rhos[1] - roots[1]) < 1e-3, str((found and [r['continued_parameter'] for _, r in found], rhos, roots)))

    # brper (XPPAUT's demo): r' = r (a - r) (b - r), the cycles r = a and r = b meet at a = b = 1: a branch point of cycles
    rows = rows_of(restarted('tstauto_brper.json', 27, 0.1), 'restart')
    found = kinds(rows, 'cycle_branch_point')
    check('first steps brper cycles: the branch point of cycles between the first and second point is labelled once, at a = b = 1 (the radial equation)',
          len(found) == 1 and found[0][0] == 1 and abs(float(found[0][1]['continued_parameter']) - 1) < 1e-4, str([(i, r['continued_parameter']) for i, r in found]))

    # the forced predator-prey (Kuznetsov, Muratori and Rinaldi 1992; MatCont's Tutorial V): the period-1 cycle's period
    # doubling at eps sqrt(m) = 0.3050914 (b0 = 0.4); its multipliers from the monodromy matrix of the forced (x, y) system
    def multipliers(m, guess, warmup=0, eps=0.5, b0=0.4, kk=1., r=6.28, a=12.56, e=1., dd=6.28):
        amplitude = eps * math.sqrt(m)

        def rhs(t, s):
            x, y = s[0], s[1]
            big = b0 * (1 + amplitude * math.cos(2 * math.pi * t))
            q = x + big
            jac = np.array([[r * (1 - 2 * x / kk) - a * y * big / q ** 2, -a * x / q], [e * a * y * big / q ** 2, e * a * x / q - dd]])
            flow = np.array(s[2:]).reshape(2, 2)
            return [r * x * (1 - x / kk) - a * x * y / q, e * a * x * y / q - dd * y, *(jac @ flow).ravel()]

        def poincare(v):
            return solve_ivp(rhs, (0, 1), [v[0], v[1], 1, 0, 0, 1], rtol=1e-12, atol=1e-14, method='DOP853').y[:, -1]
        for _ in range(warmup):
            guess = poincare(guess)[:2]
        fixed = fsolve(lambda v: poincare(v)[:2] - v, guess, xtol=1e-13)
        return fixed, np.linalg.eigvals(poincare(fixed)[2:].reshape(2, 2))
    published = (0.3050914 / 0.5) ** 2
    point = (0.4, 0.24)  # the unforced equilibrium; the cycle is followed from m = 0.30, where it is stable, to the doubling
    for m in np.arange(0.30, 0.3601, 0.01):
        point = multipliers(m, point, warmup=30 if m < 0.301 else 0)[0]
    m_pd = brentq(lambda m: min(multipliers(m, point)[1].real) + 1, 0.36, 0.38, xtol=1e-12)
    rows = rows_of(restarted('forced_pp_pd.json', 37, 0.04), 'restart')
    found = kinds(rows, 'period_doubling')
    check('first steps forced predator-prey cycles: the period doubling between the first and second point is labelled once, at the monodromy matrix\'s multiplier -1 and the published eps sqrt(m) = 0.3050914',
          len(found) == 1 and found[0][0] == 1 and abs(float(found[0][1]['continued_parameter']) - m_pd) < 1e-5 and abs(m_pd - published) < 2e-6,
          str((m_pd, published, [(i, r['continued_parameter']) for i, r in found])))


for name in args.sections:
    globals()['section_' + name]()
print('autoX checks: %s' % ('all passed' if check.failures == 0 else '%d failed' % check.failures))
sys.exit(1 if check.failures else 0)
