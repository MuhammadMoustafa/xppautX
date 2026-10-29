#!/usr/bin/env python3
"""Drive xppautX --server through the JSON protocol and check what comes back.

usage: tools/servercheck.py [--server ./xppautX] [--ode examples/ode/lecar.odex] [-v]

Plays a fixed session (integrate, change a parameter, answer a menu, a
string prompt and a form, find an equilibrium, open a second plot window)
and prints PASS/FAIL per step. No display needed; runs in a few seconds.
"""
import argparse, base64, cmath, copy, glob, hashlib, io, json, math, os, re, shutil, struct, subprocess, sys, tempfile, threading, time, queue, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xppclient import copy_model, staged_model_names, CONFIG_ROOT, Server, Checker, recording_text, read_recx, make_recording, replay_recording, run_commands, file_bytes, LogLines, SeriesMirror, drain_stderr, is_ask, placed, whole_series, save_permission, ALL_EVENTS
import autocompare

ap = argparse.ArgumentParser()
ap.add_argument('--server', default='./xppautX')
ap.add_argument('--ode', default='examples/ode/lecar.odex')
ap.add_argument('-v', action='store_true')
ap.add_argument('--only', default='', help='run only these sections, comma-separated (registry, data, stops); the rest needs the whole run')
ap.add_argument('--update-registry-docs', action='store_true', help='regenerate the registry section of docs/protocol.md')
args = ap.parse_args()
# XPP_CHECK_SLOW=F multiplies every wait by F (tools/xppclient.py)
SLOW = float(os.environ.get('XPP_CHECK_SLOW', '1'))
MAX_VALUE_UNDO = 100  # core/value_undo.h

def _reject_non_finite(text):
    """json.loads' parse_constant: NaN/Infinity/-Infinity are not valid
    JSON (W35a, json_number.h); a strict parse must fail on them instead
    of silently making a Python float, so a "-nan" that leaks past the
    core's writer fails this check instead of round-tripping."""
    raise ValueError('non-finite JSON constant: %s' % text)

def check_logging():
    """core/xpp_log.h: --silent is quiet by default, --verbose shows the
    banner/parser stats, and a real parse error still reaches stderr even
    at the default (quiet) level. Uses --silent so this needs no JSON
    conversation; see CLAUDE.md "Logging"."""
    run_dir = tempfile.mkdtemp(prefix='xppquiet')
    try:
        copy_model(args.ode, run_dir)
        odename = os.path.basename(args.ode)

        p = subprocess.run([os.path.abspath(args.server), odename, '--silent'],
                            cwd=run_dir, capture_output=True, text=True, timeout=30 * SLOW)
        check('log: --silent is quiet by default', p.stderr.strip() == '', repr(p.stderr[:300]))

        p = subprocess.run([os.path.abspath(args.server), '--verbose', odename, '--silent'],
                            cwd=run_dir, capture_output=True, text=True, timeout=30 * SLOW)
        check('log: --verbose shows the banner and parser stats',
              'Copyright' in p.stderr and 'nvar=' in p.stderr, repr(p.stderr[:300]))

        bad_dir = tempfile.mkdtemp(prefix='xppbadode')
        try:
            bad_ode = os.path.join(bad_dir, 'bad.ode')
            with open(bad_ode, 'w') as f:
                f.write("x'=(1+2\ndone\n")
            p = subprocess.run([os.path.abspath(args.server), 'bad.ode', '--silent'],
                                cwd=bad_dir, capture_output=True, text=True, timeout=30 * SLOW)
            check('log: a syntax error still reaches stderr at the default level',
                  p.stderr.strip() != '', repr(p.stderr[:300]))
        finally:
            shutil.rmtree(bad_dir, ignore_errors=True)
    finally:
        shutil.rmtree(run_dir, ignore_errors=True)


# the values files and the data a command names, as the registry names them
VALUE_KIND = {'par': 'parameters', 'ic': 'initial_conditions'}
WHAT_TARGET = {'table': 'table', 'plot': 'plot_curves', 'output': 'output'}


def launch_server(extra_env=None, ode=None, log=None, extra_args=None, no_model=False, server=None):
    """Start one xppautX --server instance in its own scratch directory and
    return (proc, run_dir, send, collect, events) -- send/collect work just
    like the module-level ones below but are bound to this instance, so a
    second, differently-configured server (e.g. a bad HOME, another model)
    can be driven the same way without disturbing the main session. With
    `log` (a list), the server's stderr lines are appended to it. With
    `no_model` the program starts with no file (the start screen, W232). `server` is another
    copy of the program to run (W233: the bundled examples are found beside the executable)."""
    ode = ode or args.ode
    run_dir = tempfile.mkdtemp(prefix='xppserver')
    copy_model(ode, run_dir)
    env = None
    if extra_env is not None:
        env = dict(os.environ)
        env.update(extra_env)
    # encoding='utf-8' explicitly (docs/protocol.md: the protocol is UTF-8):
    # text=True alone decodes with locale.getpreferredencoding(), which on a
    # Windows box without the UTF-8 system locale is the ANSI code page
    # (cp1252 here), mojibake-ing a non-ASCII name on this side even though
    # the wire bytes and xppautX itself (card W35b) are correct UTF-8.
    proc = subprocess.Popen([os.path.abspath(server or args.server), '--server'] + ([] if no_model else [os.path.basename(ode)]) + (extra_args or []), cwd=run_dir,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, encoding='utf-8', bufsize=1, env=env)
    events = queue.Queue()
    mirror = SeriesMirror()

    def reader():
        for l in proc.stdout:
            try:
                ev = json.loads(l, parse_constant=_reject_non_finite)
                mirror.feed(ev)
                events.put(ev)
            except ValueError:
                print('BAD LINE: %r' % l[:300])
                events.put({'ev': 'bad'})

    threading.Thread(target=reader, daemon=True).start()
    drain_stderr(proc, (lambda l: log.append(l.rstrip('\n'))) if log is not None else None)

    def send(**cmd):
        save_permission(cmd)
        if args.v:
            print('  >', json.dumps(cmd))
        proc.stdin.write(json.dumps(cmd) + '\n')
        proc.stdin.flush()

    def collect(until, timeout=10 * SLOW):
        """events up to and including the first one for which until(ev) is true"""
        got = []
        while True:
            try:
                ev = events.get(timeout=timeout)
            except queue.Empty:
                return got, None
            if args.v:
                s = json.dumps(ev)
                print('  <', s[:160] + ('...' if len(s) > 160 else ''))
            got.append(ev)
            if until(ev):
                return got, ev

    return proc, run_dir, send, collect, events


check = Checker()


def check_diagram_subscription():
    """W439: delivery is presentation only; resubscribe recovers every point."""
    d = copy.deepcopy(autocompare.read_description(autocompare.BASE / 'tstauto_simplefold.json'))
    d['runs'][0]['auto']['max_steps'] = 5
    with_events = autocompare.compute(args.server, d, 1)[0]
    without_events = autocompare.compute(args.server, d, 1, diagram_data=False)[0]
    check('data.diagram: opt-out keeps every saved number', with_events == without_events)
    s = Server(args.server, autocompare.ROOT / d['model'])
    try:
        autocompare.completed(s)
        autocompare.open_auto(s)
        autocompare.completed(s, dict(cmd='events.subscribe', events=['autoinfo'], diagram=False))
        autocompare.completed(s, dict(cmd='ic.set', values=dict(d.get('initial_conditions', {}))))
        events = autocompare.execute_run(s, d['runs'][0], autocompare.Diagram(), {}, diagram_data=False)
        check('data.diagram: opt-out emits no diagram events', not any(e.get('ev') == 'diagram' for e in events))
        before = autocompare.session_diagram(s)
        events = autocompare.completed(s, dict(cmd='events.subscribe', events=['autoinfo'], diagram=True))
        check('data.diagram: resubscribe replays the existing points',
              any(e.get('ev') == 'diagram' and e.get('op') == 'add' for e in events))
        check('data.diagram: resubscribe keeps every saved number', before == autocompare.session_diagram(s))
        s.send(cmd='events.subscribe', events=[], diagram='invalid')
        events, end = s.collect(autocompare.is_idle)
        errors = [e for e in events if e.get('error')]
        check('data.diagram: invalid choice has one placed command error',
              end is not None and len(errors) == 1 and errors[0].get('file') and errors[0].get('line'), str(events))
    finally:
        s.close()


def stop_server(p, r, snd):
    snd(cmd='session.quit', save=False)
    try:
        p.wait(timeout=5 * SLOW)
    except subprocess.TimeoutExpired:
        p.kill()
    shutil.rmtree(r, ignore_errors=True)


# W480 "every stop says why" (docs/studies/w480-stops.md; card W486). One small model per stop in
# tools/models/stops/, run through --server, and through --silent where the stop has an exit code.
# A case names the parts its message must have (regular expressions, never the exact wording), the
# model line of the equation the error must name, and parts the message must not have. A case whose
# fixing card has not landed is KNOWN while a part is missing; it FAILs once every part holds, so the
# card that fixes it moves it out of this table. A case that would hang today is not started. Each run
# waits for idle, never for time.
def stops_idle(e):
    return e.get('ev') in ('idle','done')


STOPS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'stops')
STOPS_KNOWN = {}  # case id -> (card that fixes it, reason it is not started or None)
STOPS_RUN = {'cmd': 'run.initial'}
STOPS_RANGE_A = {'cmd': 'run.range', 'variable': 'a', 'steps': 2, 'start': 1, 'end': 3}  # Range over a, 1 to 3 (the three runs a = 1, 2, 3)
STOPS_CASES = [
    # says: the message's parts; at: the equation the error names; forbids: parts it must not have
    dict(id='section_crossing', model='section', command=STOPS_RUN, at=None, bottom=True,
         says=[r'Stopped at the section crossing t = (0\.99|1)', r'poincare_stop = true']),
    dict(id='section_no_crossing', model='section_no_crossing', command=STOPS_RUN, at=None, bottom=True,
         says=[r'No crossing found', r'poincare_variable', r'poincare_plane = 10', r'0, 2', r'integrate longer']),
    dict(id='monte_carlo', model='monte_carlo', command=dict(cmd='equilibrium.search', guesses=4, tolerance=0.001, append=False, shoot=False, bounds={'x': [-1, 1]}), replies=[], at=None, bottom=True,
         says=[r'Monte Carlo: 0 of 4 guesses converged to 0 distinct equilibria', r'max_iterations', r'newton_tolerance']),
    dict(id='monte_carlo_manifold', model='manifold', command=dict(cmd='equilibrium.search', guesses=4, tolerance=0.001, append=False, shoot=False, bounds={'x': [0, 0], 'y': [0, 0]}), replies=[], at='newton_tolerance: 1e-200', bottom=True,
         says=[r'No eigenvector', r'singular', r'newton_tolerance = 1e-200',
               r'Monte Carlo: 4 of 4 guesses converged to 1 distinct equilibria']),
    dict(id='nan', model='nan_sqrt', command=STOPS_RUN, at="x' = sqrt",
         says=[r'X is NaN', r't ?= ?']),
    dict(id='qualrk_bound', model='blowup_qualrk', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'X out of bounds', r'state_limit ?= ?1000\b', r't ?= ?1\b']),
    dict(id='dopri5_bound', model='blowup_dopri5', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'X reaches state_limit ?= ?100\b', r't ?= ?0\.9']),
    dict(id='rk4_bound', model='bound_rk4', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'X out of bounds', r'state_limit ?= ?100\b', r't ?= ?1']),
    dict(id='min_step', model='min_step', command=STOPS_RUN, at='method: gear',
         says=[r'\b[Dd]t\b', r'0\.01', r'min_step', r'0\.1'],
         silent=dict(says=[r'min_step'], forbids=[r'will write anyway'])),
    dict(id='gear', model='blowup_gear', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'\bGear\b', r'min_step', r't ?= ?0\.9'],
         silent=dict(says=[r'\bGear\b', r'min_step', r't ?= ?0\.9'], forbids=[r'will write anyway'])),
    dict(id='backward_euler', model='blowup_backward_euler', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'Backward Euler', r'eul_iter ?= ?10\b', r'eul_tol', r't ?= ?0\.(9|89)']),
    dict(id='cvode', model='blowup_cvode', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'CVode', r't ?= ?0\.9', r'tolerance'], forbids=[r'oh well', r'\?\?\?', r'Hmmm']),
    dict(id='rosenbrock', model='blowup_rosenbrock', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'Rosenbrock', r't ?= ?']),
    dict(id='delay', model='delay', command=STOPS_RUN, at="x' = -delay",
         says=[r'tau ?= ?2\b', r'\bdelay\b', r't ?= ?']),
    dict(id='qualrk_min_step', model='unbounded_qualrk', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'QualRK', r'min_step ?= ?1e-12', r't ?= ?1\.0']),
    dict(id='stiff_min_step', model='unbounded_stiff', command=STOPS_RUN, at="x' = a*x^2",
         says=[r'\bStiff\b', r'min_step ?= ?1e-12', r't ?= ?(0\.9|1\.0)']),
    dict(id='stiff_singular', model='stiff_singular', command=STOPS_RUN, at="x' = 2*x",
         says=[r'\bStiff\b', r'singular', r't ?= ?0\b', r'dt ?= ?1\b', r'jacobian ?= ?exact'],
         silent=dict(says=[r'\bStiff\b', r'singular', r't ?= ?0\b'], forbids=[])),
    dict(id='dopri5_rounding', model='unbounded_dopri5', command=STOPS_RUN, at=None,
         says=[r'DoPri5', r'rounding limit', r't ?= ?(0\.9|1\.0)', r'relative_tolerance'],
         silent=dict(says=[r'DoPri5', r'rounding limit'], forbids=[r'Exit of dopri5'])),
    dict(id='dae_no_root', model='dae_no_root', command=STOPS_RUN, at='0 = y^2+1',
         says=[r'algebraic equations', r't ?= ?0\b', r'max_iterations ?= ?100\b', r'newton_tolerance']),
    dict(id='event_halt', model='event_halt', command=STOPS_RUN, at=None, bottom=True,
         says=[r'Stopped at t ?= ?3:', r'global event \\"x-3\\"', r'arret', r'model line 5']),
    dict(id='store_from_past_end', model='store_from_past_end', command=STOPS_RUN, at='store_from: 20',
         says=[r'store_from ?= ?20\b', r'starts at t ?= ?0\b', r'ends at t ?= ?10\b']),
    dict(id='store_from_backward', model='store_from_backward', command=STOPS_RUN, at='store_from: -20',
         says=[r'store_from ?= ?-20\b', r'starts at t ?= ?0\b', r'ends at t ?= ?-10\b']),
    dict(id='sing_pts', model='sing_pts', command=dict(cmd='equilibrium.find'), replies=[], at="x' = x^2+1",
         says=[r'X ?= ?0\.5', r'max_iterations ?= ?100\b', r'newton_tolerance', r'iterations']),
    dict(id='bvp_max_iterates', model='bvp_max_iterates', command={'cmd': 'bvp.solve', 'show': False}, at=None,
         first=[{'cmd': 'bvp.configure', 'max_iterates': 1, 'tolerance': 1e-12}],
         says=[r'Shoot', r'bvp_max_iterates ?= ?1\b', r'bvp_tolerance ?= ?1e-12', r'residual']),
    dict(id='nullcline_delay', model='nullcline_delay', command=dict(cmd='nullcline.compute'), replies=[], at="x' = -delay",
         says=[r'tau ?= ?2\b', r'X ?= ?-?\d', r'Y ?= ?-?\d']),
    dict(id='postprocess_one_variable', model='fourier_one_variable', command=STOPS_RUN, at=None,
         then=[{'cmd': 'analysis.fourier_series'}], says=[r'1 variable', r'at least 2']),
    dict(id='eq_range', model='eq_range', command=dict(cmd='equilibrium.range', parameter='a', steps=5, start=-1, end=1), replies=[], at=None, bottom=True,
         says=[r'\d of 6 values have no equilibrium', r'a ?= ?0\.2\d*: no equilibrium found']),
    dict(id='range_gear', model='blowup_gear', command=STOPS_RANGE_A, at=None,
         says=[r'Range stopped at a ?= ?1 \(run 1 of 3\)', r'\bGear\b', r't ?= ?0\.9']),
    dict(id='range_backward_euler', model='blowup_backward_euler', command=STOPS_RANGE_A, at=None,
         says=[r'Range stopped at a ?= ?1 \(run 1 of 3\)', r'Backward Euler', r'eul_iter']),
    dict(id='range_bound', model='bound_rk4', command=STOPS_RANGE_A, at=None, bottom=True,
         says=[r'3 of 3 runs ended early', r'a ?= ?1: X out of bounds at t ?= ?(0\.9|1\.0)', r'state_limit ?= ?100\b']),
]


def stops_run(path, command, replies, then=(), first=()):
    """the messages and state of one run: the command (a registry command) and the answers to
    its asks in order, then idle; then each of the commands `then`, run to idle; the commands `first`
    before it"""
    s = Server(args.server, path)
    try:
        s.collect(stops_idle)
        for setting in first:
            s.send(**setting)
            s.collect(stops_idle, timeout=60 * SLOW)
        s.send(**command)
        pending, got = list(replies), []
        while True:
            evs, e = s.collect(lambda e: is_ask(e) or stops_idle(e), timeout=60 * SLOW)
            got += evs
            if e is None or stops_idle(e):
                break
            s.send(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))
        for command in then:
            s.send(**command)
            evs, _ = s.collect(stops_idle, timeout=60 * SLOW)
            got += evs
        return got
    finally:
        s.close()


def stops_line(path, text):
    with open(path) as f:
        for n, line in enumerate(f, 1):
            if text in line:
                return n
    raise ValueError('%s has no line %r' % (path, text))


def stops_parts(case, evs, path):
    # a bottom line is progress, except where the case reads one as its message (a stop that is no error)
    msgs = [e for e in evs if (e.get('ev') in ('message', 'done') and (case.get('bottom') or 'bottom' not in e)) or (e.get('ev') == 'done' and e.get('error'))]
    text = ' | '.join(json.dumps(e) for e in msgs)
    parts = [('a message is shown', bool(msgs), str(evs[-4:]))]
    parts.append(('at most one placed error', sum(bool(e.get('error')) for e in msgs) <= 1, text[:400]))
    for rx in case['says']:
        parts.append(('the message says %s' % rx, re.search(rx, text) is not None, text[:400]))
    for rx in case.get('forbids', []):
        parts.append(('the message does not say %s' % rx, re.search(rx, text) is None, text[:400]))
    if case['at']:
        line = stops_line(path, case['at'])
        name = os.path.basename(path)
        errs = [(e.get('file'), e.get('line')) for e in msgs if e.get('error')]
        parts.append(('the error names %s, model line %d' % (name, line), (name, line) in errs, str(errs)))
    return parts


def stops_silent(path, rule):
    folder = tempfile.mkdtemp(prefix='w486stops')
    try:
        p = subprocess.run([os.path.abspath(args.server), path, '--silent'], cwd=folder,
                           capture_output=True, text=True, timeout=60 * SLOW)
    finally:
        shutil.rmtree(folder, ignore_errors=True)
    parts = [('--silent exits with an error', p.returncode != 0, str(p.returncode))]
    for rx in rule['says']:
        parts.append(('--silent says %s' % rx, re.search(rx, p.stderr) is not None, p.stderr[:400]))
    for rx in rule['forbids']:
        parts.append(('--silent does not say %s' % rx, re.search(rx, p.stderr) is None, p.stderr[:400]))
    return parts


def check_stops():
    """W480: a computation that stops early says what stopped, the setting with its value, and the
    model line (docs/studies/w480-stops.md). Known cases are listed in STOPS_KNOWN."""
    assert set(STOPS_KNOWN) <= {c['id'] for c in STOPS_CASES}, 'a known case has no model'
    for case in STOPS_CASES:
        card, reason = STOPS_KNOWN.get(case['id'], (None, None))
        if reason:
            print('KNOWN stops %s (%s): not started, %s' % (case['id'], card, reason))
            continue
        path = os.path.join(STOPS_DIR, case['model'] + '.odex')
        try:
            parts = stops_parts(case, stops_run(path, case['command'], case.get('replies', []), case.get('then', ()), case.get('first', ())), path)
            if 'silent' in case:
                parts += stops_silent(path, case['silent'])
        except Exception as exc:  # a crash or a wait that ran out is a failure of the case, never a pass
            parts = [('the run finishes', False, repr(exc))]
        missing = [name for name, ok, _ in parts if not ok]
        if card is None:
            for name, ok, detail in parts:
                check('stops %s: %s' % (case['id'], name), ok, detail)
        elif missing:
            print('KNOWN stops %s (%s): missing %s' % (case['id'], card, '; '.join(missing)))
        else:
            check('stops %s: every part holds, so %s fixed it: move the case out of STOPS_KNOWN'
                  % (case['id'], card), False, 'all parts hold')


def auto_lost_points(itmx):
    """W491: a steady-state run of examples/ode/lecar.odex (tests/auto/ode_lecar.json's first run) with
    ITMX itmx: the last autoinfo's notes and stop"""
    d = autocompare.read_description(autocompare.BASE / 'ode_lecar.json')
    s = Server(args.server, autocompare.ROOT / d['model'])
    try:
        autocompare.completed(s)
        autocompare.completed(s, dict(cmd='events.subscribe', events=['autoinfo']))
        diagram = autocompare.Diagram().apply(autocompare.open_auto(s))
        autocompare.completed(s, dict(cmd='auto.configure_numerics', **{'max_location_iterations': itmx}))
        r = d['runs'][0]
        autocompare.restore_start(s, r, [], {})
        infos = [e for e in autocompare.execute_run(s, r, diagram, {}) if e.get('ev') == 'autoinfo']
        return (infos[-1].get('notes'), infos[-1].get('stop')) if infos else (None, None)
    finally:
        s.close()


def check_auto_stops():
    """W480's AUTO cases (docs/studies/w480-stops.md A4): a special point AUTO gives up locating is a note
    naming it, its points and ITMX; the branch goes on. Restart-file checks below cover A2 and
    A3's load-time rejection; tests/test_auto_restart.cpp also covers direct short reads."""
    notes, stop = auto_lost_points(1)
    check('stops AUTO special point: max_location_iterations 1 gives up the folds, each a note naming it, its points and the setting',
          isinstance(notes, list) and len(notes) > 0
          and all(re.search(r'^Br 1 Pt \d+: a fold \(LP\) between points \d+ and \d+ was not located \(not in max_location_iterations = 1 iterations\)', n)
                  for n in notes)
          and isinstance(stop, dict) and stop.get('why') == 'parameter_max', '%s %s' % (notes, stop))
    notes, _ = auto_lost_points(8)
    check('stops AUTO special point: the default max_location_iterations locates them, no note', notes == [], str(notes))


def check_auto_global_ends():
    """W518 (#582): a cycle branch whose period grows without bound while its orbit lingers at equilibria ends on a
    SNIC, a homoclinic orbit or a heteroclinic cycle: the run stops there, the stop says which with the period, the
    parameter and the equilibria approached, and the end point is stored and marked with the new type."""
    cases = [
        ('global_snic', 'snic', 'SNIC', ['parameter mu = 0.99999', 'period growing without bound: T = ', 'times the shortest period of the branch (6.28319)',
                                         'the ghost of an equilibrium', 'zero eigenvalue, on a fold of the equilibria', 'point type snic (SNIC)']),
        ('global_homoclinic', 'homoclinic', 'HOC', ['parameter mu = 0.3287', 'period growing without bound: T = ', 'times the shortest period of the branch (4.44288)',
                                                    'lingers near a saddle (1 unstable and 1 stable eigenvalues) at "X" = ', 'point type homoclinic (HOC)']),
        ('global_heteroclinic', 'heteroclinic', 'HEC', ['parameter mu = 0.3314', 'period growing without bound: T = ',
                                                        'lingers near a saddle (1 unstable and 1 stable eigenvalues) at "X" = 1, "Y" = 0; then a saddle (1 unstable and 1 stable eigenvalues) at "X" = -1',
                                                        'point type heteroclinic (HEC)'])]
    for name, key, mark, words in cases:
        d = autocompare.read_description(autocompare.BASE / (name + '.json'))
        s = Server(args.server, autocompare.ROOT / d['model'])
        try:
            autocompare.completed(s)
            diagram = autocompare.Diagram().apply(autocompare.open_auto(s))
            events = []
            for r in d['runs']:
                autocompare.restore_start(s, r, [], {})
                events = autocompare.execute_run(s, r, diagram, {})
            stops = [e['stop'] for e in events if e.get('ev') == 'autoinfo' and e.get('stop')]
            stop = stops[-1] if stops else {}
            check('stops AUTO %s: the period growing without bound ends the cycles, says why with the period, the parameter and the equilibria' % key,
                  stop.get('why') == key and all(w in stop.get('text', '') for w in words), str(stop))
            ends = [lab for e in events if e.get('ev') == 'diagram' and e.get('op') == 'add' for r in e['runs'] for lab in r.get('lab', [])]
            check('stops AUTO %s: the end point is stored with its type, which the page marks %s (hello\'s point types)' % (key, mark),
                  bool(ends) and ends[-1][2] == key and diagram.points[-1]['sym'] == mark, str(ends[-1:]))
        finally:
            s.close()


def check_auto_starts():
    """W495: reject bad starts before a first diagram point is produced. A start at or past a
    bound is refused only when Ds leads away from the interval (autocheck's extend of a branch
    that ended just past Par Min goes back in)."""
    path = os.path.join(STOPS_DIR, 'auto_start.odex')
    # a start Newton cannot refine (no steady state) is refused; auto_start.odex's X = 1 is refined instead (below)
    no_root = os.path.join(STOPS_DIR, 'auto_start_no_root.odex')
    cases = [
        ('residual', no_root, {}, {}, ['largest residual', 'did not converge', 'Newton settings', 'Sing pts']),
        ('parameter low', path, {'pars': {'a': -3}}, {'step_size': -0.02}, ['parameter "a" = -3', 'parameter_min = -2', 'parameter_max = 2']),
        ('parameter high', path, {'pars': {'a': 3}}, {}, ['parameter "a" = 3', 'parameter_min = -2', 'parameter_max = 2']),
        ('parameter below Par Min', path, {}, {'parameter_min': 0.2, 'parameter_max': 0.9, 'step_size': -0.02}, ['parameter "a" = 0', 'parameter_min = 0.2', 'parameter_max = 0.9']),
        ('bound outward', path, {}, {'parameter_min': 0, 'step_size': -0.02}, ['step_size = -0.02', 'pointing away']),
        ('norm low', path, {}, {'norm_min': 1}, ['Start norm = 0', 'norm_min = 1', 'widen']),
        ('norm high', path, {'ics': {'X': 1}, 'pars': {'a': 1}}, {'norm_max': 0.5}, ['Start norm = 1', 'norm_max = 0.5', 'widen']),
    ]
    for name, case_path, values, limits, words in cases:
        s = Server(args.server, case_path)
        try:
            autocompare.completed(s)
            autocompare.open_auto(s)
            if values.get('ics'):
                autocompare.completed(s, dict(cmd='ic.set', values=values['ics']))
            if values.get('pars'):
                autocompare.completed(s, dict(cmd='value.set_parameters', values=dict(values['pars'])))
            autocompare.completed(s, dict(cmd='auto.configure_numerics', **{'parameter_min': -2, 'parameter_max': 2, **limits}))
            evs = autocompare.run_any(s, 's')
            errors = [e for e in evs if e.get('error')]
            text = errors[0]['error'] if len(errors) == 1 else str(errors)
            check('stops AUTO start ' + name, len(errors) == 1 and all(w in text for w in words)
                  and bool(errors[0].get('file')) and bool(errors[0].get('line'))
                  and not any(e.get('ev') == 'diagram' and e.get('op') == 'add' and e.get('runs') for e in evs), text)
            if name == 'residual':
                check('stops AUTO residual names the equation line', len(errors) == 1
                      and errors[0].get('line') == stops_line(case_path, "x' ="), str(errors))
        finally:
            s.close()
    # W495 (maintainer, 2026-10-26): auto_start.odex's X = 1 (residual 1 at a = 0) is refined by Newton to X = 0, and the run starts
    s = Server(args.server, path)
    try:
        autocompare.completed(s)
        autocompare.open_auto(s)
        autocompare.completed(s, dict(cmd='ic.set', values={'X': 1}))
        autocompare.completed(s, dict(cmd='auto.configure_numerics', parameter_min=-2, parameter_max=2))
        evs = autocompare.run_any(s, 's')
        errors = [e for e in evs if e.get('error')]
        check('stops AUTO start refined by Newton: X = 1 is refined and the run starts',
              not errors and any(e.get('ev') == 'diagram' and e.get('op') == 'add' for e in evs), str(errors))
    finally:
        s.close()


def check_auto_two_parameter_starts():
    d = autocompare.read_description(autocompare.BASE / 'tstauto_ab_fold.json')
    s = Server(args.server, autocompare.ROOT / d['model'])
    try:
        autocompare.completed(s)
        diagram = autocompare.Diagram().apply(autocompare.open_auto(s))
        autocompare.execute_run(s, d['runs'][0], diagram, {})
        autocompare.completed(s, dict(cmd='auto.grab', type='fold', index=1))
        autocompare.completed(s, dict(cmd='auto.configure_plot', **{'parameter1': 'p0', 'parameter2': 'p1'}))
        evs = autocompare.run_any(s, 't')
        errors = [e for e in evs if e.get('error')]
        # W495: AUTO bounds only the principal parameter (ae.f90 tests U(NDIM+1)), so p1 = 14 starts the run
        check('stops AUTO second parameter is not bounded: p1 = 14 outside [Par Min, Par Max] starts the run',
              not errors and any(e.get('ev') == 'diagram' and e.get('op') == 'add' for e in evs), str(errors))
        autocompare.completed(s, dict(cmd='auto.grab', type='fold', index=1))
        autocompare.completed(s, dict(cmd='auto.configure_plot', **{'parameter2': 'p0'}))
        evs = autocompare.run_any(s, 't')
        errors = [e for e in evs if e.get('error')]
        check('stops AUTO duplicate parameters', len(errors) == 1
              and 'same parameter "p0"' in errors[0]['error']
              and 'two different parameters' in errors[0]['error'], str(errors))
        # Restart uses the last run's two-parameter formulation but this label is a plain EP.
        evs = autocompare.completed(s, dict(cmd='auto.configure_plot', **{'parameter2': 'p1'}))
        s.send(cmd='auto.run', kind='two_parameter', label=1)
        evs, _ = s.collect(stops_idle)
        errors = [e for e in evs if e.get('error')]
        check('stops AUTO incompatible start kind: the error names the kind, the label and its type', len(errors) == 1
              and 'two_parameter needs a point of type hopf' in errors[0]['error'] and 'label 1 is an end point' in errors[0]['error'], str(errors))
    finally:
        s.close()


def check_auto_registry():
    """W516: the auto.* commands: a good call with its result, a refused argument with its message, and the old forms unknown"""
    folder = tempfile.mkdtemp(prefix='xppauto-registry')
    shutil.copy(autocompare.ROOT / 'examples/ode/lecar.odex', folder)
    s = Server(args.server, os.path.join(folder, 'lecar.odex'))

    def call(**c):
        s.send(**c)
        evs, _ = s.collect(stops_idle)
        done = next((e for e in reversed(evs) if e.get('ev') == 'done'), {})
        return evs, done
    try:
        autocompare.completed(s)
        autocompare.completed(s, dict(cmd='events.subscribe', events=['autoinfo', 'autosettings']))
        _, done = call(cmd='auto.run', kind='equilibrium')
        check('auto.run: AUTO not open is refused naming auto.open', done['status'] == 'error' and 'auto.open' in done['error'], str(done))
        evs_open, done = call(cmd='auto.open')
        check('auto.open: the window opens', done['status'] == 'ok' and done['result'] == {'open': True}, str(done))
        d = autocompare.read_description(autocompare.BASE / 'ode_lecar.json')
        diagram = autocompare.Diagram().apply(evs_open)
        autocompare.restore_start(s, d['runs'][0], [], {})
        evs = autocompare.execute_run(s, d['runs'][0], diagram, {})
        check('auto.run: a steady run ends with its points and branches',
              any(e.get('ev') == 'done' and e.get('result', {}).get('points_computed', 0) > 5 and e['result']['branches_tracked'] == 1 for e in evs), str(evs[-2:])[:300])
        _, done = call(cmd='auto.run', kind='equilibrium')
        check('auto.run: a new start from the current state with a diagram needs replace_diagram', done['status'] == 'error'
              and 'replace_diagram' in done['error'], str(done))
        _, done = call(cmd='auto.run', kind='two_parameter', label=1)
        check('auto.run: a kind that does not fit the point is an error naming the kind, the label and the type',
              done['status'] == 'error' and 'two_parameter needs a point of type hopf' in done['error']
              and 'label 1 is an end point of a steady branch' in done['error'], str(done))
        _, done = call(cmd='auto.run', kind='equilibrium', switch=True, label=1)
        check('auto.run: a switch needs a branch point or period doubling', done['status'] == 'error' and 'switch' in done['error'], str(done))
        _, done = call(cmd='auto.run', kind='periodic', label=1, type='hopf', index=1)
        check('auto.run: two starts are refused', done['status'] == 'error' and 'At most one of' in done['error'], str(done))
        _, done = call(cmd='auto.run', kind='homoclinic', unstable_dimension=1, stable_dimension=2)
        check('auto.run: homoclinic dimensions must add up to the variables', done['status'] == 'error'
              and 'must add up to the number of variables, 2' in done['error'] and 'unstable_dimension (1)' in done['error'], str(done))
        grab_events, done = call(cmd='auto.grab', type='hopf', index=1)
        check('auto.grab: the 1st hopf', done['status'] == 'ok' and done['result']['type'] == 'hopf', str(done))
        before_numerics = next(e for e in call(cmd='events.subscribe', events=['autosettings'])[0] if e.get('ev') == 'autosettings')['numerics']
        evs, done = call(cmd='auto.run', kind='periodic', type='hopf', index=1, max_steps=10, mesh_intervals=10)
        starts = [e['start'] for e in grab_events if e.get('ev') == 'autoinfo' and e.get('start')]
        check('auto.run: a periodic run from the 1st hopf computes; autoinfo says the kinds that fit the grabbed point',
              done['status'] == 'ok' and done['result']['points_computed'] > 2
              and any(x['type'] == 'hopf' and x['kinds'] == ['equilibrium', 'periodic', 'two_parameter'] for x in starts), str(done)[:200] + str(starts)[:200])
        after_numerics = next(e for e in call(cmd='events.subscribe', events=['autosettings'])[0] if e.get('ev') == 'autosettings')['numerics']
        check('auto.run: the settings given hold for that run only', after_numerics == before_numerics and before_numerics['max_steps'] != 10,
              str((before_numerics['max_steps'], after_numerics['max_steps'])))
        _, done = call(cmd='auto.grab', label=999999)
        check('auto.grab: an unknown label is refused', done['status'] == 'error' and 'no point labelled 999999' in done['error'], str(done))
        _, done = call(cmd='auto.grab', label=1, type='hopf', index=1)
        check('auto.grab: a label and a type are refused', done['status'] == 'error' and 'At most one of' in done['error'], str(done))
        _, done = call(cmd='auto.fit')
        check('auto.fit: the view ranges are the result', done['status'] == 'ok' and done['result']['window'] == 'diagram_1'
              and done['result']['x_range'][0] < done['result']['x_range'][1], str(done))
        _, done = call(cmd='auto.zoom', x_min=0, x_max=0.05, y_min=-0.5, y_max=0)
        check('auto.zoom: the box is the view', done['result']['x_range'] == [0, 0.05] and done['result']['y_range'] == [-0.5, 0], str(done))
        _, done = call(cmd='auto.zoom', x_min=1, x_max=0, y_min=-0.5, y_max=0)
        check('auto.zoom: an unordered box is refused', done['status'] == 'error' and 'x_min' in done['error'], str(done))
        _, done = call(cmd='auto.scroll', dx=0.01, dy=0.1)
        check('auto.scroll: the ranges move', abs(done['result']['x_range'][0] - 0.01) < 1e-12 and abs(done['result']['x_range'][1] - 0.06) < 1e-12, str(done))
        _, done = call(cmd='auto.set_display', x_range=[0.02, 0.04], y_range=[])
        check('auto.set_display: a zoom for x, the whole range for y', done['result']['x_range'] == [0.02, 0.04] and done['result']['y_range'] == [], str(done))
        _, done = call(cmd='auto.set_display', x_range=[3, 3])
        check('auto.set_display: a bad range is refused', done['status'] == 'error', str(done))
        _, done = call(cmd='auto.configure_plot', plot_type='norm', x_min=1, x_max=0)
        check('auto.configure_plot: ranges in order', done['status'] == 'error' and 'x_min' in done['error'].lower().replace('xmin', 'x_min'), str(done))
        _, done = call(cmd='auto.configure_marks', parameters=['iapp', 'T'], values=[0.02])
        check('auto.configure_marks: parameters and values of one length', done['status'] == 'error' and '2 parameters but 1 values' in done['error'], str(done))
        _, done = call(cmd='auto.configure_marks', parameters=['iapp', 'T'], values=[0.02, 30])
        check('auto.configure_marks: the marks are set', done['status'] == 'ok' and done['result'] == {'marks_count': 2}, str(done))
        _, done = call(cmd='auto.configure_numerics', output_detail='loud')
        check('auto.configure_numerics: a level is a name', done['status'] == 'error' and 'loud' in done['error'], str(done))
        _, done = call(cmd='auto.configure_numerics', switch_one_direction=True, output_detail='extensive', special_points={'cusp': False})
        check('auto.configure_numerics: the levels and the boolean are set', done['status'] == 'ok', str(done))
        _, done = call(cmd='auto.export_csv', file='w516.csv')
        check('auto.export_csv: a file argument writes it and says how many points', done['status'] == 'ok'
              and done['result']['points_written'] > 5 and (glob.glob(os.path.join(folder, 'w516*.csv')) or glob.glob(os.path.join(s.run, 'w516*.csv'))), str(done))
        _, done = call(cmd='auto.export_csv', file='../w516.csv')
        check('auto.export_csv: a file name is a base name', done['status'] == 'error', str(done))
        _, done = call(cmd='auto.set_threads', threads=1)
        check('auto.set_threads: a count', done['status'] == 'ok' and done['result']['threads'] == 1 and done['result']['automatic'] is False, str(done))
        _, done = call(cmd='auto.set_threads', threads=1, automatic=True)
        check('auto.set_threads: a count and automatic are refused together', done['status'] == 'error', str(done))
        for old in ({'cmd': 'auto', 'op': 'grab', 'label': 1}, {'cmd': 'auto', 'op': 'set', 'numerics': {}}, {'cmd': 'auto', 'op': 'restart', 'label': 1}):
            s.send(**old)
            evs, _ = s.collect(stops_idle)
            check('the old form %s is an unknown command' % json.dumps(old),
                  any('Unknown command' in e.get('error', '') for e in evs), str(evs)[:200])
    finally:
        s.close()
        shutil.rmtree(folder, ignore_errors=True)
    # a model whose AUTO equations use t is refused, naming each equation's line
    with tempfile.TemporaryDirectory() as folder:
        model = os.path.join(folder, 'timed.odex')
        with open(model, 'w') as f:
            f.write("par a = 0.5\nfun g(u) = u*t\nx' = -x + a + sin(t)\ny' = g(y) - y\nz' = -z\ninit x = 0, y = 0, z = 0\n")
        s = Server(args.server, model)
        try:
            autocompare.completed(s)
            autocompare.completed(s, dict(cmd='auto.open'))
            _, done = call(cmd='auto.run', kind='equilibrium', replace_diagram=False)
            check('auto.run: equations that use t are refused with each one\'s file and line',
                  done['status'] == 'error' and "x' (" in done['error'].lower() and "y' (" in done['error'].lower() and "z' (" not in done['error'].lower()
                  and 'timed.odex:3' in done['error'] and 'timed.odex:4' in done['error'] and done.get('line') == 3, str(done))
        finally:
            s.close()


def check_auto_periodic_start_kind():
    d = autocompare.read_description(autocompare.BASE / 'ode_lecar.json')
    s = Server(args.server, autocompare.ROOT / d['model'])
    try:
        autocompare.completed(s)
        diagram = autocompare.Diagram().apply(autocompare.open_auto(s))
        autocompare.restore_start(s, d['runs'][0], [], {})
        for r in d['runs']:
            autocompare.execute_run(s, r, diagram, {})
        s.send(cmd='auto.run', kind='periodic', label=1)
        evs, _ = s.collect(stops_idle)
        errors = [e for e in evs if e.get('error')]
        check('stops AUTO periodic from a steady EP', len(errors) == 1
              and 'periodic needs a point of type hopf' in errors[0]['error'] and 'label 1 is an end point of a steady branch' in errors[0]['error'], str(errors))
    finally:
        s.close()


def check_fit_stops():
    for name, parameters, maxiter, words in [
            ('maxiter', 'a', 1, ['maxiter = 1', 'chisq', 'tol', 'last iterate']),
            ('singular', 'a,b', 50, ['Singular matrix', 'lambda', 'b', 'column 2 of 2'])]:
        s = Server(args.server, os.path.join(STOPS_DIR, 'fit.odex'))
        try:
            autocompare.completed(s)
            with open(os.path.join(s.run, 'fit.dat'), 'w') as f:
                f.write(''.join('%d %.17g\n' % (i, math.exp(0.5*i)) for i in range(6)))
            s.send(cmd='analysis.fit_curve',variables=['x'],parameters=parameters.split(','),tolerance=1e-12,points=6,columns=2,data_columns=[2],epsilon=1e-6,max_iterations=maxiter,file='fit.dat')
            evs,end=s.collect(stops_idle)
            msgs = [e for e in evs if e.get('ev') in ('message', 'done')]
            text = ' | '.join(str(e) for e in msgs)
            check('stops fit ' + name, end is not None and all(w in text for w in words), text)
            if name == 'singular':
                errors = [e for e in msgs if e.get('error')]
                check('stops singular fit is one placed error', len(errors) == 1
                      and bool(errors[0].get('file')) and bool(errors[0].get('line')), str(errors))
        finally:
            s.close()



def check_event_count_note():
    s = Server(args.server, os.path.join(STOPS_DIR, 'events.odex'))
    try:
        autocompare.completed(s)
        s.send(cmd='run.initial')
        evs, end = s.collect(stops_idle)
    finally:
        s.close()  # Joins stderr's reader before inspecting a log note.
    text = ''.join(s.log)
    check('stops event count: the incomplete step says how many, where and what was omitted',
          end is not None and re.search(r'more than 3 events inside one step at t =', text)
          and 'step was taken without the later ones' in text, text)


def check_auto_restart_file_stops():
    s = Server(args.server, os.path.join(STOPS_DIR, 'auto_start.odex'))
    try:
        autocompare.completed(s)
        autocompare.open_auto(s)
        autocompare.completed(s, dict(cmd='auto.configure_numerics', **{'parameter_min': -2, 'parameter_max': 2, 'max_steps': 4}))
        evs = autocompare.run_any(s, 's')
        snapshot = os.path.join(s.run, 'base.snapx')
        autocompare.completed(s, dict(cmd='session.save_as', file='base.snapx', data=False))
        with zipfile.ZipFile(snapshot) as z:
            members = {n: z.read(n) for n in z.namelist()}
        # A3: corrupt input is refused by the all-or-nothing session reader,
        # before AUTO can reach a short restart read (also unit-tested directly).
        short = dict(members)
        short['auto/solutions.s'] = short['auto/solutions.s'].rsplit(b'\n', 2)[0] + b'\n'
        path = os.path.join(s.run, 'short.snapx')
        with zipfile.ZipFile(path, 'w') as z:
            for name, value in short.items():
                z.writestr(name, value)
        s.send(cmd='session.open_model', file=path)
        evs, _ = s.answer_asks(stops_idle, {'choice': lambda e: dict(key='d')})
        errors = [e for e in evs if e.get('error')]
        check('stops AUTO A3: a short solution is rejected at its file line, with the label',
              len(errors) == 1 and 'label' in errors[0]['error']
              and 'solutions.s' in errors[0].get('file', '') and errors[0].get('line', 0) > 0
              and not any(e.get('ev') == 'hello' for e in evs), str(errors))
        # A2: a valid empty solution stream has no label 1, even though the diagram does.
        missing = dict(members)
        missing['auto/solutions.s'] = b''
        path = os.path.join(s.run, 'missing.snapx')
        with zipfile.ZipFile(path, 'w') as z:
            for name, value in missing.items():
                z.writestr(name, value)
        s.send(cmd='session.open_model', file=path)
        evs, _ = s.answer_asks(stops_idle, {'choice': lambda e: dict(key='d')})
        s.send(cmd='auto.run', kind='equilibrium', label=1)
        evs, _ = s.collect(stops_idle)
        errors = [e for e in evs if e.get('error')]
        check('stops AUTO A2: a missing restart label is one placed error',
              len(errors) == 1 and 'restart label 1 is not in' in errors[0]['error']
              and bool(errors[0].get('file')) and errors[0].get('line', 0) > 0, str(errors))
    finally:
        s.close()


def check_auto_multiplier_note():
    d = autocompare.read_description(autocompare.BASE / 'ode_lecar.json')
    s = Server(args.server, autocompare.ROOT / d['model'])
    try:
        autocompare.completed(s)
        autocompare.completed(s, dict(cmd='events.subscribe', events=['autoinfo']))
        diagram = autocompare.Diagram().apply(autocompare.open_auto(s))
        autocompare.restore_start(s, d['runs'][0], [], {})
        autocompare.execute_run(s, d['runs'][0], diagram, {})
        r = copy.deepcopy(d['runs'][1])
        r['auto'].update(mesh_intervals=2, collocation_points=2, max_steps=50, step_size=0.05, max_step_size=0.1)
        # The coarse mesh intentionally loses the unit Floquet multiplier.
        evs = autocompare.execute_run(s, r, diagram, {})
        notes = [n for e in evs if e.get('ev') == 'autoinfo' for n in e.get('notes', [])]
        check('stops AUTO A8: inaccurate Floquet multipliers name the point and mesh fix',
              any(re.search(r'Br \d+ Pt \d+: the Floquet multipliers are inaccurate', n)
                  and 'mesh_intervals or collocation_points' in n and 'detection are off' in n for n in notes), str(notes))
    finally:
        s.close()


def check_auto_singular_pivot():
    s = Server(args.server, os.path.join(STOPS_DIR, 'auto_singular.odex'))
    try:
        autocompare.completed(s)
        s.send(cmd='run.initial')
        evs, _ = s.collect(stops_idle)
        autocompare.completed(s, dict(cmd='events.subscribe', events=['autoinfo']))
        autocompare.open_auto(s)
        autocompare.completed(s, dict(cmd='auto.configure_numerics', **{'parameter_min': -1, 'parameter_max': 1, 'max_steps': 4, 'step_adapt_every': 0, 'max_newton_iterations': 1, 'state_tolerance': 1e-30, 'parameter_tolerance': 1e-30}))
        evs = autocompare.run_any(s, 'p')
        stops = [e['stop'] for e in evs if e.get('ev') == 'autoinfo' and e.get('stop')]
        check('stops AUTO A7: a singular periodic solve names its pivot and bound',
              bool(stops) and 'singular' in stops[-1]['text']
              and re.search(r'smallest pivot [^ ]+ < 1e-30', stops[-1]['text'])
              and stops[-1]['br'] == 1 and stops[-1]['pt'] > 0, str(stops))
    finally:
        s.close()


def check_registry():
    """Exercise typed commands, named graphs/diagrams, errors and recordings."""
    proc, folder, snd, col, _ = launch_server()
    try:
        initial, _ = col(lambda e: e.get('ev') in ('idle', 'done'))
        hello = next(e for e in initial if e.get('ev') == 'hello')
        registry = [c for c in hello['commands'] if c.get('completion') == 'done']
        by_name = {c['cmd']: c for c in registry}
        check('registry: window commands declare entry-driven argument types',
              by_name['window.rename']['arguments'][0]['type'] == 'window' and
              by_name['window.rename']['arguments'][1]['type'] == 'new_name' and
              by_name['window.set_redraw_mode']['arguments'][1]['choices'] == ['auto', 'manual'] and
              by_name['window.set_simultaneous_plot']['arguments'][0]['type'] == 'boolean')
        counter = 0
        def request(command, ok=True, client_id=None):
            nonlocal counter
            counter += 1
            if isinstance(command, dict):
                command = dict(command)
                command.setdefault('id', 'registry %d' % counter)
                client_id = command['id'] if isinstance(command['id'], str) and len(command['id'].encode('utf-8')) <= 1024 else None
                source = json.dumps(command, ensure_ascii=False)
            else:
                source = command
            proc.stdin.write(source + '\n')
            proc.stdin.flush()
            evs, done = col(lambda e: e.get('ev') == 'done')
            snd(cmd='answer')  # an independent legacy fence (it ends in idle) proves no extra terminal event
            tail, _ = col(lambda e: e.get('ev') == 'idle')
            evs += tail
            check('registry: %d one %s done, text id and no duplicate error' % (counter, 'ok' if ok else 'error'),
                  done is not None and done.get('status') == ('ok' if ok else 'error') and
                  done.get('id') == client_id and sum(e.get('ev') == 'done' for e in evs) == 1 and
                  sum(e.get('ev') == 'idle' for e in evs) == 1 and
                  not any(e.get('ev') == 'error' or (e.get('ev') == 'message' and e.get('error')) for e in evs), str(evs)[-1000:])
            if not ok:
                check('registry: %d error retains original command place' % counter,
                      done and done.get('file') == 'command' and done.get('line') == 1 and
                      done.get('source') == source and 'result' not in done, str(done))
            return (done or {}).get('result', {}), evs
        result, _ = request({'cmd': 'help'})
        check('registry: help and hello share metadata', result == {'commands': registry})
        result, _ = request('help window.rename --id "text client"', client_id='text client')
        check('registry: help NAME filters metadata', result == {'commands': [by_name['window.rename']]})
        for bad in [
            {'cmd': 'help', 'id': 3}, {'cmd': 'help', 'id': True}, {'cmd': 'help', 'id': None},
            {'cmd': 'help', 'id': 'x' * 1025}, {'cmd': 'help', 'extra': True},
            {'cmd': 'window.rename'}, {'cmd': 'window.select'},
            {'cmd': 'window.create', 'name': ''}, {'cmd': 'window.create', 'name': ' leading'},
            {'cmd': 'window.create', 'name': 'trailing '}, {'cmd': 'window.create', 'name': 'x' * 1025},
            {'cmd': 'window.create', 'name': 'graph_1'}, {'cmd': 'window.create', 'name': None},
            {'cmd': 'window.select', 'window': 1}, {'cmd': 'window.select', 'window': 'Graph_1'},
            {'cmd': 'window.select', 'window': 'missing'},
            {'cmd':'window.rename','window':'missing','name':'new'},
            {'cmd':'window.rename','window':'graph_1'},
            {'cmd':'window.destroy','window':'missing'},
            {'cmd':'window.send_to_bottom','window':'missing'},
            {'cmd':'window.set_redraw_mode','window':'missing','mode':'auto'},
            {'cmd':'window.destroy_all','extra':True},
            {'cmd':'window.set_simultaneous_plot'}, {'cmd': 'window.destroy', 'window': 'graph_1'},
            {'cmd': 'window.rename', 'window': 'graph_1', 'name': 'bad\nname'},
            {'cmd': 'window.set_redraw_mode', 'mode': 1}, {'cmd': 'window.set_redraw_mode', 'mode': 'invalid'},
            {'cmd': 'window.set_simultaneous_plot', 'enabled': 1},
            {'cmd': 'window.set_simultaneous_plot', 'enabled': 'true'},
            'window.set_redraw_mode --mode \"auto\"',
            'window.set_simultaneous_plot --enabled \"true\"',
            'window.create --name unquoted', 'window.create --name "unterminated',
            'window.create --name "a" --name "b"', 'window.create --name',
            'window.create --name "a"tail', 'window.create --name "bad\\q"',
            'window.create --name "' + 'x' * 16384 + '"',
            '{"cmd":"window.create","name":' + '[' * 65 + '0' + ']' * 65 + '}',
        ]:
            request(bad, False)
        result, _ = request({'cmd': 'window.create'})
        check('registry: next default graph name', result == {'window': 'graph_2'})
        result, _ = request({'cmd': 'window.create', 'name': 'phase plane'})
        check('registry: create preserves user text', result == {'window': 'phase plane'})
        request({'cmd': 'window.rename', 'window': 'phase plane', 'name': 'graph_1'}, False)
        result, evs = request({'cmd': 'window.rename', 'window': 'phase plane', 'name': 'Phase "plane"'})
        check('registry: rename reports exact old and new names', result == {'window': 'phase plane', 'new_name': 'Phase "plane"'} and
              any(e.get('ev') == 'window.renamed' and e.get('new_name') == 'Phase "plane"' for e in evs))
        request({'cmd':'window.hide','window':'Phase "plane"'})
        request({'cmd':'window.hide','window':'browser'})
        snd(cmd='session.save_as', file='named windows.snapx', data=False)
        col(lambda e: e.get('ev') in ('idle', 'done'))
        request({'cmd':'window.rename','window':'Phase "plane"','name':'Temporary name'})
        snd(cmd='session.open_model', file='named windows.snapx')
        while True:
            _,end=col(lambda e:e.get('ev') in ('ask','idle','done'))
            if end is None or end['ev'] in ('idle','done'): break
            snd(cmd='answer',id=end['id'],key='d')
        snd(cmd='events.subscribe', events=['plots','series'])
        restored,_=col(lambda e: e.get('ev') in ('idle', 'done'))
        check('registry: session restore preserves exact user names and active graph',
              any(e.get('ev')=='plots' and e.get('active')=='Phase "plane"'
                  and 'Phase "plane"' in [w['win'] for w in e['windows']] for e in restored))
        result, _ = request({'cmd': 'window.list'})
        shown = {w['name']: w['shown'] for w in result['windows']}
        check('window.hide: the session file keeps hidden windows (a graph and the data browser)',
              shown.get('Phase "plane"') is False and shown.get('browser') is False and shown.get('graph_1') is True, str(shown))
        request({'cmd': 'window.show', 'window': 'Phase "plane"'})
        request({'cmd': 'window.show', 'window': 'browser'})
        request({'cmd': 'window.select', 'window': 'phase plane'}, False)
        request({'cmd': 'window.select', 'window': 'Phase "plane"'})
        result, _ = request('window.set_redraw_mode --mode manual --id "mode"', client_id='mode')
        check('registry: bare choice is typed once', result == {'redraw_mode': 'manual'})
        result, _ = request('window.set_simultaneous_plot --enabled true --id "boolean"', client_id='boolean')
        check('registry: boolean is typed once', result == {'simultaneous_plot': True})
        request({'cmd': 'window.send_to_bottom'})
        result, _ = request({'cmd': 'window.destroy'})
        check('registry: omitted target is the selected named graph', result == {'window': 'Phase "plane"', 'closed': True})
        result, _ = request({'cmd': 'window.destroy_all'})
        check('registry: close all reports count', result == {'closed_count': 1})
        request({'cmd': 'window.select', 'window': 'graph_2'}, False)
        snd(cmd='auto.open')
        col(lambda e: e.get('ev') in ('idle', 'done'))
        request({'cmd':'window.create','name':'diagram_1'},False)
        request({'cmd':'window.rename','window':'diagram_1','name':'graph_1'},False)
        request({'cmd':'window.send_to_bottom','window':'diagram_1'},False)
        request({'cmd':'window.set_redraw_mode','window':'diagram_1','mode':'manual'},False)
        request({'cmd': 'window.rename', 'window': 'diagram_1', 'name': 'Bifurcation diagram'})
        request({'cmd': 'window.select', 'window': 'Bifurcation diagram'})
        request({'cmd': 'window.destroy'})
        # W515: hide, show, list and destroy, one way for every window
        snd(cmd='events.subscribe', events=['windows']); col(lambda e: e.get('ev') in ('idle', 'done'))
        request({'cmd': 'window.create', 'name': 'kept'})
        snd(cmd='annotation.add_text', window='kept', text='note', x=0, y=0); col(lambda e: e.get('ev') in ('idle', 'done'))
        result, evs = request({'cmd': 'window.hide', 'window': 'kept'})
        check('window.hide: the window stays and is not shown', result == {'window': 'kept', 'shown': False})
        check('window.hide: the windows event says so', any(e.get('ev') == 'windows' and
              {'name': 'kept', 'kind': 'graph', 'shown': False} in e['windows'] for e in evs), str(evs)[-500:])
        result, _ = request({'cmd': 'window.list'})
        listed = {w['name']: w for w in result['windows']}
        check('window.list: every window with its kind and whether it is shown',
              listed['kept'] == {'name': 'kept', 'kind': 'graph', 'shown': False} and listed['graph_1']['shown'] is True and
              listed['browser'] == {'name': 'browser', 'kind': 'browser', 'shown': True}, str(result))
        result, _ = request({'cmd': 'window.show', 'window': 'kept'})
        check('window.show: the window comes back', result == {'window': 'kept', 'shown': True})
        snd(cmd='annotation.list', window='kept')
        evs, _ = col(lambda e: e.get('ev') in ('idle', 'done'))
        check('window.hide and show keep what the window holds (its text)', any('note' in json.dumps(e) for e in evs if e.get('ev') == 'done'), str(evs)[-500:])
        result, _ = request({'cmd': 'window.list'})
        check('a model\'s animation (lecar names ml.anix) is offered: its window is listed, hidden',
              {'name': 'animation', 'kind': 'animation', 'shown': False} in result.get('windows', []), str(result))
        for refused in [{'cmd': 'window.show', 'window': 'array_plot_1'},
                        {'cmd': 'window.hide', 'window': 'nothing'}, {'cmd': 'window.destroy', 'window': 'browser'},
                        {'cmd': 'window.rename', 'window': 'browser', 'name': 'data'}, 
                        {'cmd': 'window.create', 'name': 'browser'}, {'cmd': 'window.hide', 'confirm': True},
                        {'cmd': 'window.list', 'window': 'kept'}, {'cmd': 'window.destroy', 'window': 'kept', 'confirm': 1}]:
            request(refused, False)
        result, _ = request({'cmd': 'window.hide', 'window': 'browser'})
        result, _ = request({'cmd': 'window.show', 'window': 'browser'})
        check('window.hide and show take the data browser', result == {'window': 'browser', 'shown': True})
        # a graph that holds a text asks before it is destroyed: the answer decides
        counter += 1
        snd(cmd='window.destroy', window='kept', id='declined')
        _, ask = col(lambda e: e.get('ev') == 'ask')
        check('window.destroy of a graph holding a text asks first', ask is not None and ask.get('kind') == 'choice' and 'kept' in json.dumps(ask), str(ask))
        snd(cmd='answer', id=ask['id'], key='n')
        _, done = col(lambda e: e.get('ev') == 'done'); col(lambda e: e.get('ev') in ('idle', 'done'))
        check('window.destroy declined: an error that says what was not confirmed, the window kept',
              done is not None and done.get('status') == 'error' and 'not confirmed' in done.get('error', ''), str(done))
        result, _ = request({'cmd': 'window.destroy', 'window': 'kept', 'confirm': True})
        check('window.destroy with confirm true needs no question', result == {'window': 'kept', 'closed': True})
        request({'cmd': 'window.create', 'name': 'empty'})
        result, _ = request({'cmd': 'window.destroy', 'window': 'empty'})
        check('window.destroy of a graph that holds nothing asks nothing', result == {'window': 'empty', 'closed': True})
        # W523: the view's operations, in plot coordinates
        bounds = lambda x0, x1, y0, y1: {'x_min': x0, 'x_max': x1, 'y_min': y0, 'y_max': y1}
        result, _ = request({'cmd': 'window.set_bounds', 'window': 'graph_1', 'x_min': 0, 'x_max': 10, 'y_min': -1, 'y_max': 1})
        check('window.set_bounds sets the view and reports it', result == bounds(0, 10, -1, 1), str(result))
        result, _ = request({'cmd': 'window.set_bounds', 'x_max': 20})
        check('window.set_bounds keeps a bound left out', result == bounds(0, 20, -1, 1), str(result))
        result, _ = request({'cmd': 'window.zoom', 'x_min': 5, 'x_max': 10, 'y_min': 0, 'y_max': 1})
        check('window.zoom makes the box the view', result == bounds(5, 10, 0, 1), str(result))
        result, _ = request({'cmd': 'window.zoom_out', 'factor': 2})
        check('window.zoom_out by a factor grows the view about its center', result == bounds(2.5, 12.5, -0.5, 1.5), str(result))
        result, _ = request({'cmd': 'window.zoom_out', 'factor': 2, 'center_x': 0, 'center_y': 0})
        check('window.zoom_out by a factor about a point', result == bounds(-10, 10, -2, 2), str(result))
        result, _ = request({'cmd': 'window.scroll', 'dx': 1, 'dy': -1})
        check('window.scroll moves the view and reports its center', result == {'new_center': [1, -1]}, str(result))
        result, _ = request({'cmd': 'window.zoom_out', 'x_min': 0, 'x_max': 10, 'y_min': 0, 'y_max': 1})
        check('window.zoom_out into a box makes the view fill the box', set(result) == set(bounds(0, 0, 0, 0)), str(result))
        for refused in [{'cmd': 'window.set_bounds', 'x_min': 5, 'x_max': 5}, {'cmd': 'window.zoom', 'x_min': 0, 'x_max': 1},
                        {'cmd': 'window.zoom', 'x_min': 1, 'x_max': 0, 'y_min': 0, 'y_max': 1}, {'cmd': 'window.zoom_out'},
                        {'cmd': 'window.zoom_out', 'factor': 2, 'x_min': 0, 'x_max': 1, 'y_min': 0, 'y_max': 1},
                        {'cmd': 'window.zoom_out', 'factor': 0.5}, {'cmd': 'window.zoom_out', 'factor': 2, 'center_x': 1},
                        {'cmd': 'window.scroll', 'dx': 1}, {'cmd': 'window.fit', 'window': 'browser'},
                        {'cmd': 'window.reset_default', 'window': 'nothing'}, {'cmd': 'window.clear', 'window': 'browser'},
                        {'cmd': 'window.redraw', 'window': 'browser'}, {'cmd': 'window.set_display', 'x_range': [3, 1]},
                        {'cmd': 'window.set_display', 'x_range': [1]}, {'cmd': 'window.set_display', 'x_range': [1, 2, 3]},
                        {'cmd': 'window.set_display', 'show_earlier_runs': 1}]:
            request(refused, False)
        result, _ = request({'cmd': 'window.reset_default'})
        check('window.reset_default gives the default bounds', set(result) == set(bounds(0, 0, 0, 0)) and result['x_min'] < result['x_max'], str(result))
        result, _ = request({'cmd': 'window.fit'})
        check('window.fit reports the view', set(result) == set(bounds(0, 0, 0, 0)), str(result))
        result, _ = request({'cmd': 'window.set_display', 'x_range': [0, 5], 'show_earlier_runs': False, 'keep_replaced_run': False})
        check('window.set_display sets the zoom the page shows', result == {'x_range': [0, 5], 'y_range': []}, str(result))
        result, _ = request({'cmd': 'window.set_display', 'x_range': []})
        check('window.set_display with [] returns an axis to the window own', result == {'x_range': [], 'y_range': []}, str(result))
        result, _ = request({'cmd': 'window.redraw'})
        check('window.redraw reports the window', result == {'window': 'graph_1', 'redrawn': True}, str(result))
        result, _ = request({'cmd': 'window.clear'})
        check('window.clear reports the window', result == {'window': 'graph_1', 'cleared': True}, str(result))
        result, _ = request({'cmd': 'annotation.add_text', 'text': 'first', 'x': 1, 'y': 2, 'text_size': 3, 'style': 'bold', 'color': '#ff0000'})
        check('annotation.add_text reports the id', 'id' in result, str(result))
        note = result['id']
        result, _ = request({'cmd': 'annotation.edit', 'annotation_id': note, 'text': 'second'})
        check('annotation.edit reports the id', result == {'id': note, 'updated': True}, str(result))
        result, _ = request({'cmd': 'annotation.move', 'annotation_id': note, 'new_x': 3, 'new_y': 4})
        check('annotation.move reports the id', result == {'id': note, 'moved': True}, str(result))
        result, _ = request({'cmd': 'annotation.list'})
        check('annotation.list gives the text moved and changed', [(x['id'], x['text'], x['x'], x['y']) for x in result.get('text', [])] == [(note, 'second', 3, 4)], str(result))
        result, _ = request({'cmd': 'annotation.delete', 'annotation_id': note})
        check('annotation.delete reports the id', result == {'id': note, 'deleted': True}, str(result))
        for refused in [{'cmd': 'annotation.delete', 'annotation_id': note}, {'cmd': 'annotation.delete', 'id': 'x'},
                        {'cmd': 'annotation.add_arrow', 'tip_x': 1, 'tip_y': 1, 'tail_x': 1, 'tail_y': 1},
                        {'cmd': 'annotation.add_marker', 'shape': 'star', 'x': 0, 'y': 0}, {'cmd': 'annotation.list', 'window': 'browser'},
                        {'cmd': 'annotation.edit', 'annotation_id': 0, 'shape': 'box'}]:
            request(refused, False)
        request({'cmd': 'annotation.add_marker', 'shape': 'circle', 'x': 1, 'y': 1})
        result, _ = request({'cmd': 'annotation.delete_all'})
        check('annotation.delete_all clears the graph', result == {'cleared': True}, str(result))
        # W524: the plot's settings (curves, frozen curves, axes, exports) by name, in plot coordinates
        snd(cmd='events.subscribe', events=['plots', 'marks']); col(lambda e: e.get('ev') in ('idle', 'done'))
        request({'cmd': 'run.initial'})
        main_plot = lambda evs: next((w for e in evs if e.get('ev') == 'plots' for w in e['windows'] if w['win'] == 'graph_1'), None)
        result, evs = request({'cmd': 'plot.add_curve', 'x': 't', 'y': 'w', 'color': 'red', 'draw': 'both', 'dash': 'dashed', 'width': 2,
                               'marker': 'star', 'marker_size': 4})
        check('plot.add_curve reports the curve, and its columns and style reach the plot',
              result == {'curve_index': 1} and main_plot(evs) and main_plot(evs)['curves'][1] ==
              {'x': 0, 'y': 2, 'z': 1, 'color': '#ff0000', 'draw': 'both', 'dash': 'dashed', 'width': 2, 'marker': 'star', 'marker_size': 4}, str(result))
        result, evs = request({'cmd': 'plot.edit_curve', 'curve_index': 1, 'y': 'v', 'dash': 'dotted'})
        check('plot.edit_curve changes what is given and keeps the rest',
              result == {'curve_index': 1, 'updated': True} and main_plot(evs)['curves'][1] ==
              {'x': 0, 'y': 1, 'z': 1, 'color': '#ff0000', 'draw': 'both', 'dash': 'dotted', 'width': 2, 'marker': 'star', 'marker_size': 4}, str(result))
        request({'cmd': 'plot.add_curve', 'x': 'v', 'y': 'w'})
        result, evs = request({'cmd': 'plot.delete_curve', 'curve_index': 1})
        check('plot.delete_curve of a curve in the middle moves the later one up',
              result == {'remaining_curves': 2} and [c['x'] for c in main_plot(evs)['curves']] == [1, 1], str(result))
        result, _ = request({'cmd': 'plot.delete_curve'})
        check('plot.delete_curve without an index deletes the last curve', result == {'remaining_curves': 1}, str(result))
        request({'cmd': 'plot.add_curve', 'x': 't', 'y': 'v'})
        result, evs = request({'cmd': 'plot.delete_all_curves'})
        check('plot.delete_all_curves keeps the first curve', result == {'cleared': True} and len(main_plot(evs)['curves']) == 1, str(result))
        for refused in [{'cmd': 'plot.delete_curve', 'curve_index': 0}, {'cmd': 'plot.delete_curve', 'curve_index': 7},
                        {'cmd': 'plot.add_curve', 'x': 't'}, {'cmd': 'plot.add_curve', 'x': 't', 'y': 'nothing'},
                        {'cmd': 'plot.add_curve', 'x': 't', 'y': 'v', 'width': 0}, {'cmd': 'plot.add_curve', 'x': 't', 'y': 'v', 'draw': 'dots'},
                        {'cmd': 'plot.add_curve', 'x': 't', 'y': 'v', 'color': 'nocolor'}, {'cmd': 'plot.add_curve', 'x': 't', 'y': 'v', 'window': 'browser'},
                        {'cmd': 'plot.edit_curve'}, {'cmd': 'plot.edit_curve', 'curve_index': 5}, {'cmd': 'plot.edit_curve', 'curve_index': 0, 'marker_size': -1}]:
            request(refused, False)
        # frozen curves
        result, evs = request({'cmd': 'plot.freeze_curve', 'name': 'first', 'key': 'first run', 'color': 'blue', 'dash': 'dotted'})
        slot = result.get('slot', -1)
        frozen = next((f for e in evs if e.get('ev') == 'marks' for f in e['frozen'] if f['slot'] == slot), None)
        check('plot.freeze_curve gives the slot and name, the marks event the curve by its slot',
              result == {'slot': slot, 'name': 'first'} and frozen and frozen['key'] == 'first run' and frozen['color'] == '#0000ff'
              and frozen['dash'] == 'dotted', str(result))
        result, evs = request({'cmd': 'plot.edit_frozen', 'slot': slot, 'name': 'second', 'width': 3})
        check('plot.edit_frozen changes what is given', result == {'slot': slot, 'updated': True}, str(result))
        result, evs = request({'cmd': 'plot.set_frozen_legend', 'visible': True, 'x': 0.5, 'y': 0.5})
        check('plot.set_frozen_legend shows the legend at plot coordinates', result == {'legend_visible': True}, str(result))
        result, _ = request({'cmd': 'plot.set_frozen_legend', 'visible': False})
        check('plot.set_frozen_legend hides it', result == {'legend_visible': False}, str(result))
        result, _ = request({'cmd': 'plot.delete_frozen', 'slot': slot})
        check('plot.delete_frozen deletes the curve of that slot', result == {'slot': slot, 'deleted': True}, str(result))
        request({'cmd': 'plot.freeze_curve'})
        result, _ = request({'cmd': 'plot.delete_all_frozen'})
        check('plot.delete_all_frozen clears the graph of frozen curves', result == {'cleared': True}, str(result))
        result, _ = request({'cmd': 'plot.set_auto_freeze', 'enabled': True})
        check('plot.set_auto_freeze reports the setting', result == {'auto_freeze': True}, str(result))
        request({'cmd': 'plot.set_auto_freeze', 'enabled': False})
        for refused in [{'cmd': 'plot.delete_frozen', 'slot': slot}, {'cmd': 'plot.delete_frozen'}, {'cmd': 'plot.edit_frozen', 'slot': slot},
                        {'cmd': 'plot.freeze_curve', 'name': 'a name over nine'}, {'cmd': 'plot.freeze_curve', 'key': 'k' * 20},
                        {'cmd': 'plot.freeze_curve', 'width': -1}, {'cmd': 'plot.set_frozen_legend'},
                        {'cmd': 'plot.set_frozen_legend', 'visible': True, 'x': 1}, {'cmd': 'plot.set_auto_freeze'}]:
            request(refused, False)
        result, _ = request({'cmd': 'plot.clear_bifurcation'})
        check('plot.clear_bifurcation reports', result == {'cleared': True}, str(result))
        with open(os.path.join(folder, 'diagram.dat'), 'w') as f:
            f.write('0 0 0 1 1 0\n1 1 1 1 1 0\n2 3 3 2 1 0\n3 4 4 2 1 0\n')
        result, _ = request({'cmd': 'plot.import_bifurcation', 'file': 'diagram.dat'})
        check('plot.import_bifurcation reports the curves made', result == {'file': 'diagram.dat', 'curves_imported': 2}, str(result))
        with open(os.path.join(folder, 'bad.dat'), 'w') as f:
            f.write('0 0 0\n')
        for name, line in (('bad.dat', 1), ('missing.dat', 0)):  # a file's own error names that file and its line
            snd(cmd='plot.import_bifurcation', file=name)
            _, refused = col(lambda e: e.get('ev') == 'done')
            col(lambda e: e.get('ev') in ('idle', 'done'))
            check('plot.import_bifurcation of %s is an error at its file and line' % name,
                  refused and refused.get('status') == 'error' and refused.get('file') == name and refused.get('line') == line, str(refused))
        request({'cmd': 'plot.import_bifurcation', 'file': '../diagram.dat'}, False)
        # the axes
        result, evs = request({'cmd': 'plot.set_2d', 'x': 't', 'y': 'v', 'x_min': 0, 'x_max': 10, 'y_label': 'volts'})
        check('plot.set_2d sets the columns, limits and label', result == {'axes': '2d', 'x': 'T', 'y': 'V'} and main_plot(evs)['xlo'] == 0
              and main_plot(evs)['xhi'] == 10 and main_plot(evs)['three'] == 0, str(result))
        result, evs = request({'cmd': 'plot.set_3d', 'x': 'v', 'y': 'w', 'z': 't', 'z_min': 0, 'z_max': 5, 'window_x_min': -1, 'window_x_max': 1})
        check('plot.set_3d makes the graph 3D', result == {'axes': '3d', 'x': 'V', 'y': 'W', 'z': 'T'} and main_plot(evs)['three'] == 1
              and main_plot(evs)['box']['zmax'] == 5, str(result))
        result, evs = request({'cmd': 'plot.set_3d_view', 'theta': 30, 'phi': 60, 'perspective': True, 'z_plane': -5, 'z_view': 8})
        check('plot.set_3d_view sets the angles and the perspective', result == {'view_updated': True} and
              (main_plot(evs)['theta'], main_plot(evs)['phi'], main_plot(evs)['persp'], main_plot(evs)['zplane'], main_plot(evs)['zview']) == (30, 60, 1, -5, 8), str(result))
        result, _ = request({'cmd': 'plot.set_3d_view', 'movie': True, 'vary': 'phi', 'start_angle': 0, 'increment': 10, 'increments': 2})
        check('plot.set_3d_view films a rotation', result == {'view_updated': True}, str(result))
        for refused in [{'cmd': 'plot.set_3d_view', 'vary': 'phi'}, {'cmd': 'plot.set_3d_view', 'movie': True, 'vary': 'psi'},
                        {'cmd': 'plot.set_3d_view', 'movie': True, 'increments': 0}, {'cmd': 'plot.set_3d', 'window_x_min': 3, 'window_x_max': 1},
                        {'cmd': 'plot.set_3d', 'z_min': 9, 'z_max': 1}, {'cmd': 'plot.set_3d', 'x': 'nothing'}]:
            request(refused, False)
        result, _ = request({'cmd': 'plot.set_2d'})
        check('plot.set_2d with nothing given makes the graph 2D of the first two columns', result['axes'] == '2d', str(result))
        request({'cmd': 'plot.set_3d_view', 'theta': 1}, False)
        for refused in [{'cmd': 'plot.set_2d', 'x_min': 5, 'x_max': 1}, {'cmd': 'plot.set_2d', 'z': 'v'}, {'cmd': 'plot.set_2d', 'x': 'nothing'}]:
            request(refused, False)
        result, _ = request({'cmd': 'plot.set_axes_origin', 'x_origin': 0.5, 'x_origin_shown': False, 'axes_font_size': 14})
        check('plot.set_axes_origin reports', result == {'origin_updated': True}, str(result))
        for refused in [{'cmd': 'plot.set_axes_origin', 'axes_font_size': 0.5}, {'cmd': 'plot.set_axes_origin', 'axes_font_size': 101},
                        {'cmd': 'plot.set_axes_origin', 'x_origin_shown': 1}]:
            request(refused, False)
        result, _ = request({'cmd': 'plot.set_shifted_axes', 'x_shift': 2, 'z_shift': 1})
        check('plot.set_shifted_axes reports every shift, a shift left out unchanged', result == {'x_shift': 2, 'y_shift': 0, 'z_shift': 1}, str(result))
        result, _ = request({'cmd': 'plot.set_shifted_axes', 'x_shift': 0, 'z_shift': 0})
        request({'cmd': 'plot.set_shifted_axes', 'x_shift': -1}, False)
        result, evs = request({'cmd': 'plot.variable_vs_time', 'variable': 'w'})
        check('plot.variable_vs_time plots the column against time', result == {'x': 't', 'y': 'w'} and main_plot(evs)['curves'][0]['x'] == 0
              and main_plot(evs)['curves'][0]['y'] == 2, str(result))
        request({'cmd': 'plot.variable_vs_time', 'variable': 'nothing'}, False)
        result, evs = request({'cmd': 'plot.set_variables', 'how': 'phase_plane', 'names': ['v', 'w']})
        check('plot.set_variables makes a phase portrait of the names in order', result == {'how': 'phase_plane', 'names': ['v', 'w']}
              and [(c['x'], c['y']) for c in main_plot(evs)['curves']] == [(1, 2)], str(result))
        result, evs = request({'cmd': 'plot.set_variables', 'how': 'time_series', 'names': ['w', 'v']})
        check('plot.set_variables makes curves against time', [(c['x'], c['y']) for c in main_plot(evs)['curves']] == [(0, 2), (0, 1)], str(result))
        for refused in [{'cmd': 'plot.set_variables', 'how': 'phase_plane', 'names': ['v']}, {'cmd': 'plot.set_variables', 'how': 'array_plot', 'names': ['w', 'v']},
                        {'cmd': 'plot.set_variables', 'how': 'time_series', 'names': []}, {'cmd': 'plot.set_variables', 'how': 'time_series', 'names': ['t']},
                        {'cmd': 'plot.set_variables', 'how': 'pp', 'names': ['v']}, {'cmd': 'plot.set_variables', 'how': 'time_series'}]:
            request(refused, False)
        result, evs = request({'cmd': 'plot.add_data_markers', 'shape': 'box', 'count': 3, 'first_row': 1, 'stride': 10, 'color': 'green', 'size': 2})
        check('plot.add_data_markers reports how many were put', result == {'count': 3}, str(result))
        result, _ = request({'cmd': 'plot.add_data_markers', 'count': 2})
        check('plot.add_data_markers keeps the settings left out from the last time', result == {'count': 2}, str(result))
        for refused in [{'cmd': 'plot.add_data_markers', 'count': 3, 'stride': 100000}, {'cmd': 'plot.add_data_markers', 'shape': 'star'},
                        {'cmd': 'plot.add_data_markers', 'count': 0}, {'cmd': 'plot.add_data_markers', 'first_row': -1}]:
            request(refused, False)
        result, _ = request({'cmd': 'plot.set_colormap', 'colormap': 'viridis'})
        check('plot.set_colormap reports the scale', result == {'colormap': 'viridis'}, str(result))
        request({'cmd': 'plot.set_colormap', 'colormap': 'nothing'}, False)
        request({'cmd': 'plot.set_colormap'}, False)
        # the exports
        result, _ = request({'cmd': 'plot.export_svg', 'file': 'plot.svg', 'color': False, 'axes_font_size': 10})
        check('plot.export_svg writes the picture into the model folder', result['file'] == 'plot.svg' and result['bytes_written'] > 100 and
              open(os.path.join(folder, 'plot.svg')).read().lstrip().startswith(('<?xml', '<svg')), str(result))
        result, _ = request({'cmd': 'plot.export_pdf', 'file': 'plot.pdf'})
        check('plot.export_pdf writes the picture into the model folder', result['file'] == 'plot.pdf' and result['bytes_written'] > 100 and
              open(os.path.join(folder, 'plot.pdf'), 'rb').read(4) == b'%PDF', str(result))
        result, _ = request({'cmd': 'plot.export_data', 'file': 'plot.csv'})
        check('plot.export_data writes what the plot shows in the format of the extension', result['file'] == 'plot.csv' and result['rows_written'] > 0 and
              'curve,x,y' in open(os.path.join(folder, 'plot.csv')).read().splitlines()[:2], str(result))
        result, _ = request({'cmd': 'plot.export_data', 'file': 'plot.out', 'format': 'dat'})
        check('plot.export_data takes the format by name', result['rows_written'] > 0, str(result))
        for refused in [{'cmd': 'plot.export_svg'}, {'cmd': 'plot.export_svg', 'file': '../plot.svg'}, {'cmd': 'plot.export_pdf', 'file': 'plot.pdf', 'axes_font_size': 0},
                        {'cmd': 'plot.export_pdf', 'file': 'plot.pdf', 'color': 'yes'}, {'cmd': 'plot.export_data', 'file': 'plotdata'},
                        {'cmd': 'plot.export_data', 'file': 'plot.zip', 'format': 'zip'}]:
            request(refused, False)
        # the old forms are gone
        for obsolete in [{'cmd': 'plotvars', 'how': 0, 'names': ['v']}, {'cmd': 'colormap', 'name': 'jet'}]:
            snd(**obsolete)
            evs, _ = col(lambda e: e.get('ev') in ('idle', 'done'))
            check('W524: the old %s command is unknown' % obsolete['cmd'], any('Unknown command' in e.get('error', '') for e in evs if e.get('ev') in ('message', 'done')), str(evs)[-300:])
        for obsolete in [
            {'cmd':'click','win':1},
            {'cmd':'key','menu':'main','item':'makewindow'},
            {'cmd':'auto','op':'view','active':0},
            {'cmd':'auto','op':'view','close':0},
            {'cmd':'auto','op':'close'},
            {'cmd':'window.close'},
            {'cmd':'window.close','window':'graph_2'},
            {'cmd':'display','window':'graph_1','x':[0,1]},
            {'cmd':'redraw'},
            {'cmd':'annotation','op':'list'},
            {'cmd':'key','menu':'main','item':'erase'},
            {'cmd':'annotation','op':'list','win':1},
        ]:
            snd(**obsolete)
            events,_=col(lambda e: e.get('ev') in ('idle', 'done'))
            check('registry: old window forms are errors',
                  sum(e.get('ev')=='message' and 'error' in e for e in events)==1
                  and not any(e.get('ev')=='done' for e in events), str(obsolete))
        # W522: every analysis row returns its declared data, validates before mutation, and has no old route.
        request({'cmd': 'plot.set_variables', 'how': 'phase_plane', 'names': ['V', 'W']})
        result, _ = request({'cmd': 'equilibrium.find'})
        check('W522: equilibrium.find returns values, eigenvalues and named stability',
              result.get('converged') is True and len(result.get('equilibrium', {})) == 2
              and len(result.get('eigenvalues', [])) == 2 and result.get('stability') in ('stable', 'unstable', 'saddle'), str(result))
        bounds = {name: [value, value] for name, value in result.get('equilibrium', {}).items()}
        calls = [
            ({'cmd': 'equilibrium.guess', 'coordinates': {'V': -0.5, 'W': 0.1}}, 'converged'),
            ({'cmd': 'equilibrium.import_as_ic'}, 'applied'),
            ({'cmd': 'equilibrium.export', 'file': 'w522-equilibrium.dat'}, 'written'),
            ({'cmd': 'equilibrium.configure', 'max_iterates': 100}, 'settings'),
            ({'cmd': 'equilibrium.search', 'guesses': 2, 'tolerance': 0.001, 'bounds': bounds}, 'found'),
            ({'cmd': 'equilibrium.range', 'parameter': 'iapp', 'start': 0, 'end': 0.1, 'steps': 1}, 'points_found'),
            ({'cmd': 'nullcline.set_mesh', 'mesh': 20}, 'nullcline_mesh'),
            ({'cmd': 'nullcline.compute'}, 'curves_computed'),
            ({'cmd': 'nullcline.redraw'}, 'drawn'),
            ({'cmd': 'nullcline.auto_mode', 'enabled': True}, 'auto_redraw'),
            ({'cmd': 'nullcline.freeze'}, 'frozen_curves_count'),
            ({'cmd': 'nullcline.range_freeze', 'parameter': 'iapp', 'start': 0, 'end': 0.1, 'steps': 1}, 'steps_frozen'),
            ({'cmd': 'nullcline.animate', 'delay_ms': 0}, 'animated'),
            ({'cmd': 'nullcline.export', 'file': 'w522-nullclines.csv', 'format': 'csv'}, 'points_written'),
            ({'cmd': 'nullcline.clear_frozen'}, 'cleared'),
            ({'cmd': 'dirfield.compute', 'grid': 2, 'scaled': True}, 'vectors_drawn'),
            ({'cmd': 'dirfield.export', 'file': 'w522-dirfield.dat'}, 'written'),
            ({'cmd': 'dirfield.flow', 'grid': 2}, 'trajectories_run'),
            ({'cmd': 'dirfield.colorize', 'grid': 2}, 'cells_colored'),
            ({'cmd': 'dirfield.clear'}, 'cleared')]
        for command, field in calls:
            result, _ = request(command)
            check('W522: ' + command['cmd'] + ' declares and returns ' + field, field in result, str(result))
            request({**command, 'unknown_argument': True}, False)
        request({'cmd': 'equilibrium.find', 'print_eigenvalues': False}, False)
        request({'cmd': 'equilibrium.configure', 'print_eigenvalues': 'yes'}, False)
        request({'cmd': 'equilibrium.search', 'guesses': 1, 'tolerance': 0.001, 'bounds': {'V': [0, 1, 2]}}, False)
        request({'cmd': 'equilibrium.search', 'guesses': 1, 'tolerance': 0.001, 'bounds': {'V': [1, 0]}}, False)
        request({'cmd': 'equilibrium.configure', 'max_iterates': 20, 'newton_tolerance': -1}, False)
        result, _ = request({'cmd': 'equilibrium.configure'})
        check('W522: configure is all or nothing', result.get('settings', {}).get('max_iterates') == 100, str(result))
        request('equilibrium.search --guesses 1 --tolerance 0.001 --bounds {"V" [0 0] "W" [0 0]}')
        request({'cmd': 'nullcline.export', 'file': '../outside.dat'}, False)
        for obsolete in [dict(cmd='equilibrium', op='write', name='old.dat'), dict(cmd='dfield', op='write', name='old.dat'),
                         dict(cmd='key', win='equilibrium', key='i'),
                         dict(cmd='set', kind='num', name='nullcline_mesh', value=20)]:
            snd(**obsolete); evs, _ = col(lambda e: e.get('ev') in ('idle', 'done'))
            check('W522: old analysis form is unknown', any('Unknown' in e.get('error', '') or 'needs key, and nothing else' in e.get('error', '')
                  for e in evs), str(evs)[-500:])
        # A key runs the registry command bound to it, with no menu ask; XPPAUT's letters are no keys.
        request({'cmd':'keymap.set','bindings':{}})
        snd(cmd='key', key='Ctrl+R'); evs, _ = col(lambda e: e.get('ev') in ('idle','done'))
        check('W536: the default key runs the registry command without a menu ask',
              not any(e.get('ev') == 'ask' for e in evs) and any(e.get('ev') == 'computing' for e in evs), str(evs)[-500:])
        for sequence in ('I G', 'I'):
            snd(cmd='key', key=sequence); evs, _ = col(lambda e: e.get('ev') in ('idle','done'))
            check('W536: %r is no key: XPPAUT letters are no bindings' % sequence,
                  len([e for e in evs if e.get('error')]) == 1 and not any(e.get('ev') in ('ask', 'computing') for e in evs), str(evs)[-500:])
        request({'cmd':'keymap.reset'})
        # These are real recorded steps, whose JSON commands retain names.
        snd(cmd='recording.start'); col(lambda e: e.get('ev') in ('idle', 'done'))
        request({'cmd': 'window.create', 'name': 'Recorded graph'})
        request({'cmd': 'window.rename', 'window': 'Recorded graph', 'name': 'Recorded phase plane'})
        request({'cmd': 'window.select', 'window': 'graph_1'})
        request({'cmd': 'window.destroy', 'window': 'Recorded phase plane'})
        snd(cmd='recording.stop', file='named.recx'); col(lambda e: e.get('ev') in ('idle', 'done'))
        with open(os.path.join(folder, 'named.recx'), encoding='utf-8') as f: text = f.read()
        _, _, steps, written, computed = read_recx(text)
        check('registry: recording keeps dotted commands and quoted window names', written == computed and
              [step['cmd']['cmd'] for step, _ in steps] == ['window.create', 'window.rename', 'window.select', 'window.destroy'] and
              steps[1][0]['cmd']['window'] == 'Recorded graph' and 'win' not in steps[1][0]['cmd'])
        replay_dir = tempfile.mkdtemp(prefix='xppnamed-replay')
        try:
            replay = replay_recording(args.server, args.ode, os.path.join(folder, 'named.recx'), replay_dir)
            check('registry: named window recording replays silently', replay.returncode == 0, replay.stderr[-1000:])
        finally:
            shutil.rmtree(replay_dir)
        generated = ['| Command | Arguments | Kind / category | Menu | Result | Events | Description |',
                     '|---|---|---|---|---|---|---|']
        for command in registry:
            def describe_argument(a):
                text = '`%s`: %s%s%s' % (a['name'], 'required ' if a['required'] else 'optional ', a['type'],
                                         ' (' + ', '.join(a['choices']) + ')' if a['choices'] else
                                         ' (max %s bytes)' % a['max_length'] if a['type'] not in ('boolean', 'integer', 'real', 'list', 'map') else '')
                extras = []
                if 'min' in a: extras.append('at least %s' % a['min'])
                if 'min_of' in a: extras.append('at least the session\'s %s' % a['min_of'])
                if 'max' in a: extras.append('at most %s' % a['max'])
                if 'max_of' in a: extras.append('at most the session\'s %s' % a['max_of'])
                if 'names' in a: extras.append('of %s' % a['names'])
                if 'element' in a: extras.append('of %s, at most %s' % (a['element'], a['max_items']))
                return text + (' [' + ', '.join(extras) + ']' if extras else '')
            groups = {'exactly_one': 'exactly one of', 'at_most_one': 'at most one of', 'together': 'all or none of'}
            arguments = ', '.join(describe_argument(a) for a in command['arguments']) or 'none'
            if command.get('groups'):
                arguments += '; ' + '; '.join('%s %s' % (groups[g['rule']], ', '.join('`%s`' % n for n in g['arguments'])) for g in command['groups'])
            results = ', '.join('`%s`: `%s`' % (r['name'], r['type']) for r in command['result'])
            generated.append('| `%s` | %s | `%s` / %s | %s | %s | %s | %s |' %
                             (command['cmd'], arguments, command['kind'], command['category'],
                              command['menu_path'] or 'none', results,
                              ', '.join('`%s`' % e for e in command['events']), command['description']))
        generated = '\n'.join(generated) + '\n'
        path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'docs', 'protocol.md')
        with open(path, encoding='utf-8', newline='') as f: doc = f.read()
        start, end = '<!-- registry:start -->\n', '<!-- registry:end -->'
        before, rest = doc.split(start, 1); existing, after = rest.split(end, 1)
        if args.update_registry_docs:
            with open(path, 'w', encoding='utf-8', newline='') as f: f.write(before + start + generated + end + after)
            existing = generated
        check('registry: generated docs match metadata', existing == generated,
              'uv run --locked python tools/servercheck.py --only registry --update-registry-docs')
    finally:
        if proc.poll() is None:
            snd(cmd='session.quit', save=False)
            try: proc.wait(timeout=5 * SLOW)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()
        shutil.rmtree(folder, ignore_errors=True)


def check_manifold_export_clock():
    """W533: each branch of a saddle's export starts at the saddle and is written whole, whatever store_from says, and the session's time is left as it was."""
    for model, store_from in (('saddle_manifolds.odex', 0), ('saddle_manifolds_transient.odex', .5)):
        s = Server(args.server, 'tools/models/' + model)
        try:
            s.collect(lambda e: e.get('ev') in ('idle', 'done'))
            s.send(cmd='run.initial')  # the session's clock is now off zero: every branch must still start at the saddle
            s.collect(lambda e: e.get('ev') in ('idle', 'done'))
            s.send(cmd='session.get_state')
            evs, _ = s.collect(lambda e: e.get('ev') in ('idle', 'done'))
            t_before = [e for e in evs if e.get('ev') == 'state'][-1]['time']
            check('W533: the session clock is off zero before the export (store_from %g)' % store_from, t_before != 0,
                  str(t_before))
            s.send(cmd='equilibrium.export', file='seed.dat', shoot=True, confirm=True)
            evs, _ = s.collect(lambda e: e.get('ev') == 'done')
            done = next((e for e in evs if e.get('ev') == 'done'), {})
            files = sorted(os.path.basename(f) for f in glob.glob(os.path.join(s.run, 'seed-eigen*.dat')))
            check('W533: a saddle export with store_from %g writes every branch, named by its type' % store_from,
                  done.get('status') == 'ok' and files == ['seed-eigen0-unstable.dat', 'seed-eigen1-unstable.dat',
                                                          'seed-eigen2-stable.dat', 'seed-eigen3-stable.dat'], str(done)[:300] + str(files))
            rows = {f: [list(map(float, line.split())) for line in open(os.path.join(s.run, f)) if line.strip()] for f in files}
            whole = all(len(table) == 21 and table[0][0] == 0 and abs(table[-1][0]) == 1 for table in rows.values())
            check('W533: each exported branch starts at the saddle (t = 0) and holds its whole run (store_from %g)' % store_from, whole,
                  str({f: (len(t), t[0][0], t[-1][0]) for f, t in rows.items()})[:500])
            s.send(cmd='session.get_state')
            evs, _ = s.collect(lambda e: e.get('ev') in ('idle', 'done'))
            t_after = [e for e in evs if e.get('ev') == 'state'][-1]['time']
            check('W533: the export leaves the session time as it found it (store_from %g)' % store_from, t_after == t_before,
                  '%s -> %s' % (t_before, t_after))
        finally:
            s.close()


def check_session_commands():
    """W528: the session, recording, keymap, events and tool commands of the registry: each answered with its result
    and ending in done, a refused argument naming itself and the old protocol forms refused as unknown commands."""
    folder = tempfile.mkdtemp(prefix='xppbuttons')
    model = os.path.join(folder, 'buttons.ode')
    with open(model, 'w') as f:
        f.write("x'=-x\ninit x=1\n@ total=1,dt=0.1,but=Go:ig\ndone\n")
    config = tempfile.mkdtemp(prefix='xppconfig')
    proc, run_dir, snd, col, _ = launch_server(ode=model, extra_env={'XPP_CONFIG_DIR': config})
    try:
        def last_state(evs):
            return next((e for e in reversed(evs) if e.get('ev') == 'state'), None)

        initial, _ = col(lambda e: e.get('ev') in ('idle', 'done'))
        hello = next(e for e in initial if e.get('ev') == 'hello')
        check('W528: the model makes the button, and hello names every event stream the commands take',
              hello.get('userbuttons') == ['Go'] and 'player' in hello.get('features', []) and 'features' in hello, str(hello.get('userbuttons')))

        def call(answers=(), **command):
            """the done of a command, answering its asks with `answers` (keys or fields), and every event up to it"""
            snd(**command)
            got, pending = [], list(answers)
            while True:
                evs, end = col(lambda e: e.get('ev') in ('done', 'ask'))
                got += evs
                if end is None or end['ev'] == 'done':
                    return (end or {}), got
                reply = pending.pop(0) if pending else {'ok': 0}
                snd(cmd='answer', id=end['id'], **(reply if isinstance(reply, dict) else {'key': reply}))

        def error_of(done):
            return done.get('error', '') if done.get('status') == 'error' else ''

        # the old forms: refused as unknown commands, nothing run
        for old in ('open', 'reload', 'session', 'record', 'play', 'keymap', 'data', 'equations', 'state', 'quit', 'action', 'userbut'):
            snd(cmd=old)
            evs, _ = col(lambda e: e.get('ev') == 'idle')
            check('W528: the old command %s is unknown' % old,
                  any('Unknown command %s' % old in e.get('error', '') for e in evs), str(evs)[-300:])
        # the tools
        done, _ = call(cmd='tool.calculator', expression='2*3+1')
        check('W528: tool.calculator works a formula out', done.get('status') == 'ok' and done['result'] == {'expression': '2*3+1', 'value': 7}, str(done))
        done, _ = call(cmd='tool.calculator', expression='2*')
        check('W528: tool.calculator refuses a formula that does not compile', 'expression' in error_of(done), str(done))
        done, _ = call(cmd='tool.run_user_button', index=5)
        check('W528: tool.run_user_button refuses a button the model lacks', 'no button 5' in error_of(done), str(done))
        done, evs = call(cmd='tool.run_user_button', index=0)
        st = last_state(evs)
        check('W528: tool.run_user_button presses the model\'s button: Go ran', done.get('result') == {'ran': True} and st and st['rows'] > 1, str(done))
        done, _ = call(cmd='tool.run_source_action', index=0)
        check('W528: tool.run_source_action refuses a comment with no action', 'no action' in error_of(done), str(done))
        # the state and the source
        done, evs = call(cmd='session.get_state')
        check('W528: session.get_state answers the state\'s members and sends the state event too',
              done.get('result', {}).get('rows') == st['rows'] and done['result']['pars'] == [] and
              any(e.get('ev') == 'state' for e in evs), str(done)[:200])
        done, evs = call(cmd='session.get_source')
        check('W528: session.get_source answers the source and the equations, and sends both events',
              done['result']['equations'] == ['dX/dT=(-X)'] and "x' = -x" in done['result']['source']
              and {'source', 'equations'} <= {e.get('ev') for e in evs}, str(done)[:300])
        # the event subscriptions: added, removed, the encoding kept
        done, _ = call(cmd='events.subscribe', events=['series', 'plots'], encoding='f32')
        check('W528: events.subscribe answers the events now subscribed', done['result'] == {'subscribed': ['series', 'plots']}, str(done))
        done, _ = call(cmd='events.subscribe', events=['marks'])
        check('W528: events.subscribe adds to what the client has', done['result'] == {'subscribed': ['series', 'plots', 'marks']}, str(done))
        done, _ = call(cmd='events.unsubscribe', events=['plots', 'marks'])
        check('W528: events.unsubscribe removes the ones named', done['result'] == {'unsubscribed': ['plots', 'marks']}, str(done))
        done, evs = call(cmd='events.subscribe', events=[])
        check('W528: events.subscribe with no event keeps the rest, and the encoding', done['result'] == {'subscribed': ['series']}, str(done))
        for bad, text in ((dict(events=['nothing']), 'Invalid choice'), (dict(events=['series'], encoding='f64'), 'Invalid choice'),
                          (dict(), 'events'), (dict(events=['series'], diagram='yes'), 'diagram')):
            done, _ = call(cmd='events.subscribe', **bad)
            check('W528: events.subscribe %s is refused' % json.dumps(bad), text in error_of(done), str(done))
        call(cmd='events.unsubscribe', events=ALL_EVENTS)
        # the keymap
        done, evs = call(cmd='keymap.get')
        check('W528: keymap.get answers the user\'s own differences, and sends the keymap event',
              done['result'] == {'bindings': {}, 'pinned': []} and any(e.get('ev') == 'keymap' and e['op'] == 'get' for e in evs), str(done))
        done, evs = call(cmd='keymap.set', bindings={'session.reload_model': ['Ctrl+B']}, pinned=['tool.calculator'])
        km = next((e['keymap'] for e in evs if e.get('ev') == 'keymap'), {})
        check('W528: keymap.set writes the parts given and sends the effective keymap',
              done['result'] == {'saved': True} and 'preset' not in km and km.get('pinned') == ['tool.calculator']
              and os.path.exists(os.path.join(config, 'keymap.json')), str(done) + str(km)[:200])
        done, _ = call(cmd='keymap.set', pinned=['session.reload_model'])
        done2, _ = call(cmd='keymap.get')
        check('W528: keymap.set leaves out what it is not given: the bindings stay',
              done2['result'] == {'bindings': {'session.reload_model': ['Ctrl+B']}, 'pinned': ['session.reload_model']}, str(done2))
        for bad, text in ((dict(bindings={'session.reload_model': 'Ctrl+B'}), 'bindings'), (dict(bindings={'nosuch': ['Ctrl+B']}), 'nosuch'),
                          (dict(bindings={'session.reload_model': ['ctrl+b']}), 'ctrl+b'), (dict(bindings={'session.reload_model': ['Ctrl+W']}), 'Ctrl+W'),
                          (dict(bindings={'session.reload_model': ['Ctrl+B'], 'tool.calculator': ['Ctrl+B']}), 'Ctrl+B'), (dict(pinned=['quit']), 'quit'),
                          (dict(preset='default'), 'preset'),
                          (dict(bindings={'session.reload_model': ['Ctrl+K S']}), 'never a sequence'),
                          (dict(bindings={'session.reload_model': ['Ctrl+K Ctrl+S']}), 'Ctrl+K Ctrl+S'),
                          (dict(bindings={'auto.run': ['Ctrl+R']}), 'run.initial')):
            done, _ = call(cmd='keymap.set', **bad)
            kept, _ = call(cmd='keymap.get')
            check('W528: keymap.set %s is refused, the keymap as it was' % json.dumps(bad),
                  text in error_of(done) and kept['result'] == done2['result'], str(done)[:300])
        done, evs = call(cmd='keymap.reset')
        check('W528: keymap.reset removes the file', done['result'] == {'reset': True}
              and not os.path.exists(os.path.join(config, 'keymap.json')) and any(e.get('ev') == 'keymap' and e['op'] == 'reset' for e in evs), str(done))
        # the recording and the player
        done, _ = call(cmd='recording.add_note', note='no recording yet')
        check('W528: recording.add_note refuses while not recording', 'Not recording' in error_of(done), str(done))
        done, _ = call(cmd='recording.stop', file='none')
        check('W528: recording.stop refuses while not recording', 'Not recording' in error_of(done), str(done))
        done, evs = call(cmd='recording.start')
        check('W528: recording.start answers and the state says recording',
              done['result'] == {'recording': True} and last_state(evs)['recording'] == {'steps': 0, 'note': ''}, str(done))
        done, _ = call(cmd='recording.start')
        check('W528: recording.start refuses a second recording', 'Already recording' in error_of(done), str(done))
        done, evs = call(cmd='recording.add_note', note='First run.\nSettles.')
        check('W528: recording.add_note answers, a note may hold line breaks',
              done['result'] == {'note_added': True} and last_state(evs)['recording']['note'] == 'First run.\nSettles.', str(done))
        done, _ = call(cmd='recording.add_note', note='bell\u0007')
        check('W528: recording.add_note refuses another control character', 'note' in error_of(done), str(done))
        call(cmd='run.initial')
        done, evs = call(cmd='recording.stop', file='mine.recx')
        check('W528: recording.stop answers the file and the steps, and ends the recording',
              done['result'] == {'file': 'mine.recx', 'steps_recorded': 1} and 'recording' not in last_state(evs)
              and os.path.exists(os.path.join(run_dir, 'mine.recx')), str(done))
        # the session files
        done, evs = call(answers=[{'ok':0}], cmd='session.save')
        check('W517: first session.save asks for its filename and allows cancel', any(is_ask(e) and e.get('kind') == 'file' for e in evs) and done.get('status') == 'ok', str(done))
        done, _ = call(cmd='session.save_as', file='first', data=False)
        check('W528: session.save_as writes name.snapx and answers it',
              done['result'] == {'file': 'first.snapx', 'saved': True} and os.path.exists(os.path.join(run_dir, 'first.snapx')), str(done))
        done, evs = call(cmd='session.save', data=False)
        check('W528: session.save then saves to that file with no question', done['result'] == {'file': 'first.snapx', 'saved': True}
              and not any(e.get('ev') == 'ask' for e in evs), str(done))
        done, evs = call(answers=[{'key': 'n'}], cmd='session.save_as', file='first', confirm=False)
        check('W528: session.save_as asks before replacing without confirm, and a refusal saves nothing',
              done['result']['saved'] is False and any(e.get('ev') == 'ask' for e in evs), str(done))
        done, _ = call(cmd='session.save_copy', file='copy')
        check('W528: session.save_copy answers the file and leaves the session\'s own file', done['result'] == {'file': 'copy.snapx', 'copied': True}
              and last_state(call(cmd='session.get_state')[1])['session']['file'].endswith('first.snapx'), str(done))
        for bad in (dict(file='../up'), dict(file='/tmp/x'), dict()):
            done, _ = call(cmd='session.save_as', **bad)
            check('W528: session.save_as %s is refused' % json.dumps(bad), error_of(done) != '' and 'file' in error_of(done), str(done))
        done, _ = call(cmd='session.export_info', file='info.pars')
        check('W528: session.export_info writes the file', done['result'] == {'file': 'info.pars', 'written': True}
              and 'Equations' in open(os.path.join(run_dir, 'info.pars')).read(), str(done))
        done, _ = call(cmd='session.import_settings', file='absent.set')
        check('W528: session.import_settings names the file it cannot read', 'absent.set' in error_of(done), str(done))
        done, evs = call(answers=['d'], cmd='session.open', file='first.snapx')
        check('W528: session.open loads the saved session and sends a new hello',
              done['result'] == {'file': 'first.snapx', 'loaded': True} and any(e.get('ev') == 'hello' for e in evs), str(done))
        done, _ = call(cmd='session.open', file='missing.snapx')
        check('W528: session.open names a session file that is not there', 'missing.snapx' in error_of(done), str(done))
        done, evs = call(answers=['d'], cmd='session.reload_model')
        check('W528: session.reload_model reloads the model', done['result'] == {'reloaded': True} and any(e.get('ev') == 'hello' for e in evs), str(done))
        done, evs = call(answers=['d'], cmd='session.open_model', file=model)
        res = done.get('result', {})
        check('W528: session.open_model answers the model now loaded, its variables and parameters',
              res.get('variables') == ['X'] and res.get('parameters') == [] and 'buttons' in res.get('model', ''), str(done))
        done, _ = call(cmd='session.open_model', file='/no/such/model.ode')
        check('W528: session.open_model names a model that is not there and keeps this one',
              'model.ode' in error_of(done) and call(cmd='session.get_state')[0]['result']['ics'] == [['X', 1]], str(done))
        for bad, text in ((dict(file='a.ode', example='lecar.odex'), 'one of'), (dict(recent=-1), 'recent'),
                          (dict(recent=5), 'no recent model 5'), (dict(file='x.recx'), 'recording')):
            done, _ = call(cmd='session.open_model', **bad)
            check('W528: session.open_model %s is refused' % json.dumps(bad), text in error_of(done), str(done))
        # the player (it loads the recording's model in its own folder, so after the session files)
        done, _ = call(answers=['d'], cmd='recording.open', file=os.path.join(run_dir, 'mine.recx'), play=False)
        check('W528: recording.open answers the file and the number of steps', done['result'] == {'file': os.path.join(run_dir, 'mine.recx'), 'total_steps': 1}, str(done))
        for bad, text in ((dict(file=os.path.join(run_dir, 'mine.recx')), 'play'), (dict(play=True), 'file')):
            done, _ = call(cmd='recording.open', **bad)
            check('W528: recording.open %s is refused' % json.dumps(bad), text in error_of(done), str(done))
        done, evs = call(cmd='recording.set_speed', speed=2)
        check('W528: recording.set_speed answers the speed', done['result'] == {'speed': 2}, str(done))
        for speed in (0.1, 9):
            done, _ = call(cmd='recording.set_speed', speed=speed)
            check('W528: recording.set_speed %s is outside 0.25 to 8' % speed, 'speed' in error_of(done), str(done))
        done, _ = call(cmd='recording.set_step_note', step=5, note='x')
        check('W528: recording.set_step_note refuses a step the recording lacks', 'no step 5' in error_of(done), str(done))
        done, evs = call(cmd='recording.set_step_note', step=0, note='A note.', confirm=True)
        check('W528: recording.set_step_note answers the step and sends the player', done['result'] == {'step': 0} and any(e.get('ev') == 'player' for e in evs), str(done))
        done, _ = call(cmd='recording.pause')
        check('W528: recording.pause answers', done['result'] == {'paused': True}, str(done))
        done, _ = call(cmd='recording.seek', step=0, play=False)
        check('W528: recording.seek answers the step', done['result'] == {'step': 0}, str(done))
        done, _ = call(cmd='recording.close')
        check('W528: recording.close leaves the player', done['result'] == {'closed': True}, str(done))
        for name in ('play', 'pause', 'step', 'close'):
            done, _ = call(cmd='recording.' + name)
            check('W528: recording.%s with no recording open is refused' % name, 'No recording is open' in error_of(done), str(done))
        # the quit: left out asks and a cancel keeps the session; save false leaves at once
        done, evs = call(answers=[{'ok': 0}], cmd='session.quit')
        check('W528: session.quit with save left out asks the leave question, and a cancel keeps the session',
              done['result'] == {'quitting': False} and any(e.get('ev') == 'ask' and e.get('keys') == 'sd' for e in evs), str(done))
        snd(cmd='session.quit', save=False)
        evs, bye = col(lambda e: e.get('ev') == 'bye')
        check('W528: session.quit with save false says bye and leaves at once, with no done',
              bye is not None and not any(e.get('ev') == 'done' for e in evs), str([e.get('ev') for e in evs]))
    finally:
        if proc.poll() is None:
            snd(cmd='session.quit', save=False)
        try:
            proc.wait(timeout=5 * SLOW)
        except subprocess.TimeoutExpired:
            proc.kill()
        shutil.rmtree(run_dir, ignore_errors=True)
        shutil.rmtree(folder, ignore_errors=True)
        shutil.rmtree(config, ignore_errors=True)


def check_equilibrium_exports():
    """W522: explicit side filenames and one error value, including a refused derived destination."""
    s = Server(args.server, 'tools/models/equilibrium_registry.odex')
    try:
        s.collect(lambda e: e.get('ev') in ('idle', 'done'))
        def export(file):
            s.send(cmd='equilibrium.export', file=file, shoot=True, confirm=True)
            events, end = s.collect(lambda e: e.get('ev') in ('ask', 'done'))
            if end and end.get('ev') == 'ask':
                s.send(cmd='answer', id=end['id'], ok=0)
                tail, _ = s.collect(lambda e: e.get('ev') == 'done')
                events += tail
            return events, next((e for e in events if e.get('ev') == 'done'), {})
        events, done = export('seed.dat')
        files = sorted(glob.glob(os.path.join(s.run, 'seed-eigen*.dat')))
        check('W522: shoot export writes two derived manifold files without filename asks',
              done.get('status') == 'ok' and done.get('result', {}).get('written') is True and len(files) == 2
              and not any(e.get('ev') == 'ask' for e in events), str(events)[-500:])
        rows = [[list(map(float, row.split())) for row in open(file) if row.strip()] for file in files]
        check('W522: manifold exports hold finite time and state columns', len(rows) == 2 and
              all(len(table) >= 2 and all(len(row) == 4 and all(math.isfinite(v) for v in row) for row in table) for table in rows), str(rows)[:500])
        os.mkdir(os.path.join(s.run, 'blocked-eigen0-stable.dat'))
        events, done = export('blocked.dat')
        check('W522: a derived output failure returns one placed error through done',
              done.get('status') == 'error' and done.get('file') == 'command' and done.get('line') == 1
              and 'blocked-eigen0-stable.dat' in done.get('error', '')
              and not any(e.get('ev') == 'message' and e.get('error') for e in events), str(events)[-800:])
    finally:
        s.close()
    check_manifold_export_clock()


def check_data_value_set():
    """W526: the data browser, the values and the sets as registry commands: a good call with its result, a
    refused argument with its message, and the old forms refused as unknown."""
    folder = tempfile.mkdtemp(prefix='w526')
    sets_model = os.path.join(folder, 'w526sets.odex')
    with open(sets_model, 'w') as f:
        f.write("x'=-a*x+b\ny'=-y+c\npar a=1,b=2,c=0.5\ninit x=0.5,y=2\n"
                "options { integration: { dt: 0.02, total_time: 9 } }\n"
                "set \"pa\" { a: 10, b: 20 }\n"
                "set \"mix\" { a: 5, x: 0.9, options: { integration: { dt: 0.1, total_time: 7 }, plot: { xlo: -3, xhi: 3 } } }\n")
    post_model = os.path.join(folder, 'w526post.odex')
    with open(post_model, 'w') as f:
        f.write("x'=-x+sin(t)\ninit x=0.5\noptions { integration: { total_time: 50, dt: .05 }, analysis: { postprocess: 1, histcol: x, histlo: -1, histhi: 1, histbins: 20 } }\n")
    servers = []
    # this section also runs alone (--only), before the helpers below the registry's are defined
    is_idle = lambda e: e.get('ev') in ('idle', 'done')
    last_state = lambda evs: next((e for e in reversed(evs) if e.get('ev') == 'state'), None)

    def stop_server(proc, folder, snd):
        if proc.poll() is None:
            snd(cmd='session.quit', save=False)
            try: proc.wait(timeout=5 * SLOW)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()
        shutil.rmtree(folder, ignore_errors=True)

    def start(ode=None):
        s = launch_server(ode=ode)
        servers.append(s)
        s[3](is_idle)
        return s

    def requester(snd, col):
        def request(**command):
            snd(**command)
            evs, done = col(is_idle, timeout=60 * SLOW)
            return evs, (done if done and done.get('ev') == 'done' else {}), [e for e in evs if e.get('ev') == 'browser']
        return request

    def good(request, what, expect, **command):
        evs, done, _ = request(**command)
        result = done.get('result')
        check('W526: %s' % what, done.get('status') == 'ok' and (expect(result) if callable(expect) else result == expect),
              json.dumps(done)[:400])
        return result

    def refused(request, why, words, **command):
        evs, done, _ = request(**command)
        check('W526: %s is refused, saying %s' % (json.dumps(command)[:100], words),
              done.get('status') == 'error' and words in done.get('error', '') and 'result' not in done, json.dumps(done)[:400])

    try:
        p, r, snd, col, _ = start()
        request = requester(snd, col)
        # the old forms are no commands
        for old in ({'cmd': 'browser', 'from': 0, 'count': 1}, {'cmd': 'browser', 'op': 'write', 'what': 'table'},
                    {'cmd': 'values', 'op': 'write', 'kind': 'par'}, {'cmd': 'applyset', 'set': 'pa', 'scope': 'all'},
                    {'cmd': 'saveset', 'set': 's', 'mode': 'new', 'scope': 'all'}, {'cmd': 'savedefaults', 'scope': 'all'},
                    {'cmd': 'exportset', 'set': 's', 'scope': 'all', 'file': 'x.odex'}, {'cmd': 'renameset', 'set': 'a', 'to': 'b'},
                    {'cmd': 'deleteset', 'set': 'a'}, {'cmd': 'restoredefaults', 'scope': 'all'},
                    {'cmd': 'slide', 'name': 'iapp', 'value': 1}, {'cmd': 'slider', 'slot': 0, 'name': 'iapp', 'lo': 0, 'hi': 1, 'step': 0.1}):
            snd(**old)
            evs, _ = col(is_idle)
            check('W526: the old form %s is an unknown command' % json.dumps(old)[:60],
                  any(e.get('ev') == 'message' and 'Unknown command' in e.get('error', '') for e in evs), str(evs)[:300])
        refused_set = request(cmd='set', kind='par', name='iapp', value=1)
        # ---- value ----
        good(request, 'value.set_parameters sets some parameters and tells the values now',
             {'updated': {'iapp': 0.1, 'gk': 2.0}}, cmd='value.set_parameters', values={'iapp': 0.1, 'gk': 2})
        state = last_state(request(cmd='session.get_state')[0])
        check('W526: ... and the state has them', dict(state['pars'])['iapp'] == 0.1 and dict(state['pars'])['gk'] == 2.0, str(state['pars'][:4]))
        for words, command in (('no parameter named "nosuch"', dict(values={'nosuch': 1})), ('finite number', dict(values={'iapp': 'x'})),
                               ('Missing argument "values"', dict()), ('Unknown argument "extra"', dict(values={'iapp': 1}, extra=1))):
            refused(request, 'value.set_parameters', words, cmd='value.set_parameters', **command)
        check('W526: set with a parameter says to send value.set_parameters',
              refused_set[1].get('status') == 'error' or any('value.set_parameters' in e.get('error', '') for e in refused_set[0]), str(refused_set[0])[:300])
        good(request, 'value.set_by_formula sets a parameter to what a formula works out to', {'name': 'iapp', 'value': 0.2},
             cmd='value.set_by_formula', name='iapp', formula='0.1*2')
        refused(request, 'value.set_by_formula', 'Argument "formula"', cmd='value.set_by_formula', name='iapp', formula='nosuch+')
        good(request, 'value.undo takes the last edit back', {'undone': True}, cmd='value.undo')
        check('W526: ... iapp is 0.1 again', dict(last_state(request(cmd='session.get_state')[0])['pars'])['iapp'] == 0.1)
        good(request, 'value.redo puts it back', {'redone': True}, cmd='value.redo')
        refused(request, 'value.redo with nothing to redo', 'No value edit to redo', cmd='value.redo')
        good(request, 'value.slide sets a parameter', {'name': 'iapp', 'value': 0.07}, cmd='value.slide', name='iapp', value=0.07)
        refused(request, 'value.slide', 'no parameter or variable named "nosuch"', cmd='value.slide', name='nosuch', value=1)
        good(request, 'value.configure_slider defines slot 0', {'slot': 0, 'configured': True},
             cmd='value.configure_slider', slot=0, name='iapp', low=-1, high=2, step=0.01)
        sliders = last_state(request(cmd='session.get_state')[0])['sliders']
        check('W526: ... and the state has the slider', sliders and sliders[0]['name'] == 'iapp' and sliders[0]['lo'] == -1
              and sliders[0]['hi'] == 2 and sliders[0]['step'] == 0.01, str(sliders))
        good(request, 'value.configure_slider keeps the slot\'s step when none is given', {'slot': 0, 'configured': True},
             cmd='value.configure_slider', slot=0, name='iapp', low=0, high=1)
        check('W526: ... the step stayed', last_state(request(cmd='session.get_state')[0])['sliders'][0]['step'] == 0.01)
        refused(request, 'value.configure_slider', 'Slot 9', cmd='value.configure_slider', slot=9, name='iapp', low=0, high=1)
        refused(request, 'value.configure_slider', 'finite low < high', cmd='value.configure_slider', slot=0, name='iapp', low=1, high=0)
        good(request, 'value.remove_slider empties the slot', {'slot': 0, 'removed': True}, cmd='value.remove_slider', slot=0)
        refused(request, 'value.remove_slider', 'Slot 5', cmd='value.remove_slider', slot=5)
        good(request, 'value.save_file writes the parameters into the model folder',
             {'file': 'w526.par', 'kind': 'parameters'}, cmd='value.save_file', kind='parameters', file='w526.par')
        good(request, 'value.save_file writes the initial conditions', {'file': 'w526.ic', 'kind': 'initial_conditions'},
             cmd='value.save_file', kind='initial_conditions', file='w526.ic')
        request(cmd='value.set_parameters', values={'iapp': 0.5})
        good(request, 'value.load_file reads the parameters back', {'file': 'w526.par', 'kind': 'parameters'},
             cmd='value.load_file', kind='parameters', file='w526.par')
        check('W526: ... iapp is what was saved', dict(last_state(request(cmd='session.get_state')[0])['pars'])['iapp'] == 0.07)
        refused(request, 'value.load_file of a file that is not there', 'w526gone.par', cmd='value.load_file', kind='parameters', file='w526gone.par')
        refused(request, 'value.save_file with a kind it does not know', 'Invalid choice "xyz"', cmd='value.save_file', kind='xyz', file='x.par')
        refused(request, 'value.save_file with a path', 'not a plain file name', cmd='value.save_file', kind='parameters', file='../x.par')
        # ---- browser ----
        request(cmd='run.initial')
        result = good(request, 'browser.page gives the block asked for',
                      lambda v: v['first_row'] == 10 and v['total_rows'] == 601 and len(v['rows']) == 3 and len(v['rows'][0]) == 2,
                      cmd='browser.page', from_row=10, count=3, first_column=2, columns=1)
        evs, done, blocks = request(cmd='browser.page', from_row=10, count=3, first_column=2, columns=1)
        check('W526: ... the browser event carries the same rows', blocks and blocks[-1]['data'] == result['rows'] and blocks[-1]['from'] == 10
              and blocks[-1]['cols'][:3] == ['T', 'V', 'W'], str(blocks)[:300])
        for words, command in (('at least 0', dict(from_row=-1, count=1)), ('at most 2000', dict(from_row=0, count=2001)),
                               ('at most 500', dict(from_row=0, count=1, columns=501)), ('at least 1', dict(from_row=0, count=1, first_column=0)),
                               ('Missing argument "count"', dict(from_row=0))):
            refused(request, 'browser.page', words, cmd='browser.page', **command)
        row100 = request(cmd='browser.page', from_row=100, count=1, first_column=1, columns=2)[1]['result']['rows'][0]
        good(request, 'browser.find selects the row nearest a value', {'row_found': 100}, cmd='browser.find', column='v', value=row100[1])
        refused(request, 'browser.find of a column the data has not', 'no column named "nosuch"', cmd='browser.find', column='nosuch', value=1)
        refused(request, 'browser.find of a value that is no number', 'finite number', cmd='browser.find', column='v', value='x')
        good(request, 'browser.set_slice_start keeps the data from a row on', {'first_row': 10}, cmd='browser.set_slice_start', row=10)
        refused(request, 'browser.set_slice_end before the first row kept', 'must not come before the first row kept, 10', cmd='browser.set_slice_end', row=5)
        good(request, 'browser.set_slice_end keeps the data up to a row', {'last_row': 50}, cmd='browser.set_slice_end', row=50)
        evs, done, blocks = request(cmd='browser.page', from_row=0, count=1)
        check('W526: ... the block says the rows kept', blocks and blocks[-1]['start'] == 10 and blocks[-1]['end'] == 51, str(blocks)[:200])
        refused(request, 'browser.set_slice_start past the data', 'not in the data', cmd='browser.set_slice_start', row=601)
        good(request, 'browser.tabulate_column writes the rows kept as a lookup table', {'file': 'w526.tab', 'rows_written': 41},
             cmd='browser.tabulate_column', variable='v', x_low=0, x_high=1, file='w526.tab')
        check('W526: ... its first line is the count', open(os.path.join(r, 'w526.tab')).readline().strip() == '41')
        refused(request, 'browser.tabulate_column of a reversed range', 'low end below its high end', cmd='browser.tabulate_column', variable='v', x_low=1, x_high=0, file='x.tab')
        refused(request, 'browser.tabulate_column of an unknown column', 'no column named "nosuch"', cmd='browser.tabulate_column', variable='nosuch', x_low=0, x_high=1, file='x.tab')
        refused(request, 'browser.tabulate_column to a path', 'not a plain file name', cmd='browser.tabulate_column', variable='v', x_low=0, x_high=1, file='a/b.tab')
        good(request, 'browser.restore_full_slice keeps all the data again', {'first_row': 0, 'last_row': 600}, cmd='browser.restore_full_slice')
        evs, done, blocks = request(cmd='browser.page', from_row=0, count=1)
        check('W526: ... the block says all the rows', blocks and blocks[-1]['start'] == 0 and blocks[-1]['end'] == 601, str(blocks)[:200])
        good(request, 'browser.add_column adds a column computed from the others', {'column': 'VW', 'added': True},
             cmd='browser.add_column', name='vw', formula='v*w')
        evs, done, blocks = request(cmd='browser.page', from_row=0, count=2, first_column=1, columns=500)
        check('W526: ... the block has it, computed', blocks and blocks[-1]['cols'][-1] == 'VW'
              and abs(blocks[-1]['data'][1][-1] - blocks[-1]['data'][1][1] * blocks[-1]['data'][1][2]) < 1e-5, str(blocks)[:300])
        refused(request, 'browser.add_column of a name the data has', 'already has a column named "VW"', cmd='browser.add_column', name='VW', formula='v')
        refused(request, 'browser.add_column with a formula that does not compile', 'Argument "formula"', cmd='browser.add_column', name='zz', formula='v*')
        good(request, 'browser.replace_column replaces a column by a sequence', {'column': 'vw', 'replaced': True},
             cmd='browser.replace_column', column='vw', formula='1:601')
        evs, done, blocks = request(cmd='browser.page', from_row=0, count=3, first_column=1, columns=500)
        check('W526: ... its values are the sequence', blocks and [row[-1] for row in blocks[-1]['data']] == [1, 2, 3], str(blocks)[:300])
        good(request, 'browser.undo_replace puts the column back', {'undone': True}, cmd='browser.undo_replace')
        refused(request, 'browser.undo_replace with nothing replaced', 'No replacement to undo', cmd='browser.undo_replace')
        refused(request, 'browser.replace_column with a bad formula', 'Illegal formula', cmd='browser.replace_column', column='vw', formula='v*')
        refused(request, 'browser.replace_column of a variable that is not there', 'No such variable', cmd='browser.replace_column', column='vw', formula='@nosuch')
        refused(request, 'browser.delete_column of a model column', 'Choose an added column', cmd='browser.delete_column', name='v')
        good(request, 'browser.export writes the table in a format', {'file': 'w526.csv', 'rows_written': 601},
             cmd='browser.export', target='table', format='csv', file='w526.csv')
        refused(request, 'browser.export with a format it has not', 'Invalid choice "xls"', cmd='browser.export', target='table', format='xls', file='x.xls')
        refused(request, 'browser.export with a target it has not', 'Invalid choice "all"', cmd='browser.export', target='all', format='csv', file='x.csv')
        good(request, 'browser.export writes what the plot shows', {'file': 'w526c.csv', 'rows_written': 601},
             cmd='browser.export', target='plot_curves', format='csv', file='w526c.csv')
        good(request, 'browser.load reads a data file into the table', {'file': 'w526.csv', 'rows_loaded': 601}, cmd='browser.load', file='w526.csv')
        refused(request, 'browser.load of a file that is not there', 'w526gone.csv', cmd='browser.load', file='w526gone.csv')
        good(request, 'browser.delete_column deletes an added column', {'deleted': True}, cmd='browser.delete_column', name='vw')
        refused(request, 'browser.delete_column with none left', 'no column named "vw"', cmd='browser.delete_column', name='vw')
        good(request, 'browser.transpose shows the data transposed', {'transposed_rows': 2, 'transposed_cols': 1},
             cmd='browser.transpose', first_column='v', columns=2, column_skip=1, first_row=0, rows=1, row_skip=1)
        for words, command in (('reach column 7', dict(columns=7)), ('at most 5 columns', dict(rows=6)), ('reach row 601', dict(first_row=1, rows=2, row_skip=600)),
                               ('at least 1', dict(columns=0)), ('no column named "nosuch"', dict(first_column='nosuch'))):
            refused(request, 'browser.transpose', words, cmd='browser.transpose', **command)
        # ---- the model's @ postprocess ----
        pp, rp, sp, cp, _ = start(post_model)
        post = requester(sp, cp)
        post(cmd='run.initial')
        post(cmd='analysis.histogram')
        evs, _, blocks = post(cmd='browser.page', from_row=0, count=1)
        check('W526: the data browser shows the histogram of the model\'s @ postprocess', blocks and blocks[-1]['rows'] == 21, str(blocks)[:200])
        # ---- set ----
        ps, rs, ss, cs, _ = start(sets_model)
        sets = requester(ss, cs)
        text = lambda: open(os.path.join(rs, 'w526sets.odex')).read()
        good(sets, 'set.apply applies the parameters of a set', {'applied': 'pa', 'scope': 'parameters'}, cmd='set.apply', name='pa', scope='parameters')
        check('W526: ... the state has them', dict(last_state(sets(cmd='session.get_state')[0])['pars'])['a'] == 10)
        for words, command in (('the model has no set nosuch', dict(name='nosuch', scope='all')), ('Invalid choice "bogus"', dict(name='pa', scope='bogus')),
                               ('Missing argument "scope"', dict(name='pa')), ('Unknown window "graph_9"', dict(name='pa', scope='all', window='graph_9')),
                               ('a graph is a target of scope all or plot', dict(name='pa', scope='parameters', window='graph_1'))):
            refused(sets, 'set.apply', words, cmd='set.apply', **command)
        good(sets, 'set.save_new writes a new set into the file', {'name': 's1', 'saved': True}, cmd='set.save_new', name='s1', scope='parameters')
        check('W526: ... the file has it', 'set "s1" {' in text())
        refused(sets, 'set.save_new of a name that is a set', 'already a set', cmd='set.save_new', name='s1', scope='parameters')
        refused(sets, 'set.save_new with a state it has not', 'Invalid choice "next"', cmd='set.save_new', name='s2', scope='parameters', state='next')
        refused(sets, 'set.save_new of the last state before any run', 'no run has reached a state', cmd='set.save_new', name='s2', scope='initial', state='last')
        good(sets, 'set.save_into replaces a part of a set', {'name': 'mix', 'updated': True}, cmd='set.save_into', name='mix', scope='initial')
        refused(sets, 'set.save_into of a set the file has not', 'no set nosuch', cmd='set.save_into', name='nosuch', scope='parameters')
        good(sets, 'set.save_defaults writes the values as the model\'s own', {'saved_as_defaults': True}, cmd='set.save_defaults', scope='parameters')
        good(sets, 'set.export writes a set into another file', {'file': 'w526out.odex', 'name': 'e1', 'exported': True},
             cmd='set.export', name='e1', scope='parameters', file='w526out.odex')
        check('W526: ... the file has it', 'set "e1" {' in open(os.path.join(rs, 'w526out.odex')).read())
        refused(sets, 'set.export into the model file', 'is the model file', cmd='set.export', name='x1', scope='parameters', file='w526sets.odex')
        good(sets, 'set.rename renames a set of the file', {'from': 's1', 'to': 's3', 'renamed': True}, cmd='set.rename', **{'from': 's1'}, to='s3')
        refused(sets, 'set.rename to a name that is a set', 'already a set', cmd='set.rename', **{'from': 's3'}, to='mix')
        good(sets, 'set.delete deletes a set of the file', {'name': 's3', 'deleted': True}, cmd='set.delete', name='s3')
        check('W526: ... the file no longer has it', 'set "s3"' not in text())
        refused(sets, 'set.delete of a set the file has not', 'no set nosuch', cmd='set.delete', name='nosuch')
        good(sets, 'set.restore_defaults puts the model\'s values back', {'restored': True}, cmd='set.restore_defaults', scope='all')
        good(sets, 'set.restore_defaults of one parameter', {'restored': True}, cmd='set.restore_defaults', scope='parameters', field='a')
        refused(sets, 'set.restore_defaults of one field of all', 'restoring all takes no field', cmd='set.restore_defaults', scope='all', field='a')
        # the recording of these commands replays
        sets(cmd='recording.start')
        sets(cmd='value.set_parameters', values={'a': 3})
        sets(cmd='set.apply', name='pa', scope='parameters')
        sets(cmd='recording.stop', file='w526.recx')
        replay = replay_recording(args.server, sets_model, os.path.join(rs, 'w526.recx'), rs)
        check('W526: a recording of value.set_parameters and set.apply replays', replay.returncode == 0, replay.stderr[-300:])
    finally:
        for server in servers:
            stop_server(server[0], server[1], server[2])
        shutil.rmtree(folder, ignore_errors=True)


def check_kinescope_animation_array_plot():
    """W527: the kinescope, the animation and the array plot on the registry: each command's good call with its result,
    a refused argument with its message, the pictures asked of the client, a Go steered by commands sent while it plays,
    and the old forms unknown."""
    proc, run_dir, snd, col, _ = launch_server()
    shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gui_test.ani'), run_dir)
    with open(os.path.join(run_dir, 'nograb.ani'), 'w') as f:
        f.write('dimension 0;0;1;1\nline 0;0;1;1\ndone\n')
    aplots = lambda evs: [e for e in evs if e.get('ev') == 'aplot']
    exists = lambda *names: all(os.path.exists(os.path.join(run_dir, n)) for n in names)
    head = lambda name, n: open(os.path.join(run_dir, name), 'rb').read(n)

    def req(command, ok=True):
        """the command's done event and every event up to it, the client's pictures asked for answered"""
        snd(**command)
        got = []
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'done'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'done':
                break
            snd(cmd='answer', id=e['id'], **(pixels(e) if e['kind'] == 'pixels' else {'ok': 0}))
        done = got[-1] if got and got[-1].get('ev') == 'done' else {}
        check('W527: %s %s' % (command['cmd'], 'is done' if ok else 'is refused: ' + str(done.get('error'))[:80]),
              done.get('status') == ('ok' if ok else 'error') and (ok or 'result' not in done), str(got[-3:])[:400])
        return done.get('result', {}), got, done.get('error', '')

    is_idle = lambda e: e.get('ev') == 'idle'
    pixels = lambda e: {'w': 2, 'h': 1, 'rgb': base64.b64encode(bytes([255, 0, 0, 0, 0, 255])).decode()}

    try:
        col(is_idle)
        # the kinescope
        result, evs, _ = req({'cmd': 'kinescope.capture'})
        result2, evs2, _ = req({'cmd': 'kinescope.capture'})
        films = [e for e in evs2 if e.get('ev') == 'film']
        check('W527: kinescope.capture counts the frames and sends the film event', result == {'frame_count': 1} and result2 == {'frame_count': 2}
              and films and films[-1].get('op') == 'capture' and films[-1].get('count') == 2, str((result, result2, films)))
        result, evs, _ = req({'cmd': 'kinescope.play'})
        check('W527: kinescope.play shows every frame', result == {'frames_played': 2}
              and any(e.get('ev') == 'film' and e.get('op') == 'play' for e in evs), str(result))
        result, evs, _ = req({'cmd': 'kinescope.autoplay', 'cycles': 2, 'frame_ms': 5})
        check('W527: kinescope.autoplay takes the cycles and the delay', result == {'cycles_completed': 2}
              and any(e.get('ev') == 'film' and e.get('op') == 'autoplay' and e.get('cycles') == 2 and e.get('delay') == 5 for e in evs), str(result))
        result, evs, _ = req({'cmd': 'kinescope.autoplay'})
        check('W527: kinescope.autoplay keeps the last settings', any(e.get('ev') == 'film' and e.get('cycles') == 2 and e.get('delay') == 5 for e in evs), str(evs)[-300:])
        for refused in [{'cmd': 'kinescope.autoplay', 'cycles': 0}, {'cmd': 'kinescope.autoplay', 'frame_ms': -1}, {'cmd': 'kinescope.autoplay', 'cycles': 1.5},
                        {'cmd': 'kinescope.export_frames'}, {'cmd': 'kinescope.export_frames', 'file': '../k.gif'}, {'cmd': 'kinescope.export_gif', 'file': 'a/b.gif'},
                        {'cmd': 'kinescope.capture', 'file': 'x'}]:
            req(refused, False)
        result, evs, _ = req({'cmd': 'kinescope.export_frames', 'file': 'w527_kin.gif'})
        check('W527: kinescope.export_frames asks the pictures and writes the GIFs, the second named from the first',
              result == {'file': 'w527_kin.gif', 'frames_saved': 2} and exists('w527_kin.gif', 'w527_kin-frame1.gif')
              and head('w527_kin.gif', 6) [:4] == b'GIF8', str(result))
        os.mkdir(os.path.join(run_dir, 'atomic-frame1.gif'))
        req({'cmd': 'kinescope.export_frames', 'file': 'atomic.gif'}, False)
        check('W534: refusing the last frame file writes no earlier frame', not exists('atomic.gif'))
        result, evs, _ = req({'cmd': 'kinescope.export_gif', 'file': 'w527_movie.gif'})
        check('W527: kinescope.export_gif writes one animated GIF', result.get('file') == 'w527_movie.gif' and exists('w527_movie.gif')
              and result.get('bytes_written') == os.path.getsize(os.path.join(run_dir, 'w527_movie.gif')) > 20, str(result))
        result, evs, _ = req({'cmd': 'kinescope.reset'})
        check('W527: kinescope.reset clears the frames', result == {'cleared': True}
              and any(e.get('ev') == 'film' and e.get('op') == 'reset' and e.get('count') == 0 for e in evs), str(result))
        for empty in ('kinescope.play', 'kinescope.autoplay'):
            _, _, error = req({'cmd': empty}, False)
            check('W527: %s says the kinescope holds no frames' % empty, 'holds no frames' in error, error)
        _, _, error = req({'cmd': 'kinescope.export_gif', 'file': 'none.gif'}, False)
        check('W527: kinescope.export_gif with no frames writes nothing', 'holds no frames' in error and not exists('none.gif'), error)

        # the animation
        snd(cmd='events.subscribe', events=['ani'])
        col(is_idle)
        _, _, error = req({'cmd': 'animation.play'}, False)
        check('W527: animation.play with no stored data says so', 'no stored data' in error, error)
        req({'cmd': 'run.initial'})
        result, evs, _ = req({'cmd': 'animation.open'})
        check('W527: animation.open opens the window and says so', result == {'open': True}
              and any(e.get('ev') == 'ani' and e.get('open') == 1 for e in evs), str((result, evs[-3:]))[:300])
        result, evs, _ = req({'cmd': 'animation.load', 'file': 'gui_test.ani'})
        frames = [e for e in evs if e.get('ev') == 'ani' and e.get('op') == 'frame']
        _, evs_bad, _ = req({'cmd': 'animation.load', 'file': 'missing.ani'}, False)
        check('W527: animation.load loads the file and its first frame shows; a missing file is an error naming it',
              result == {'file': 'gui_test.ani', 'loaded': True} and frames and frames[-1]['pos'] == 0 and evs_bad[-1].get('file') == 'missing.ani', str((result, evs_bad[-1:])))
        for refused in [{'cmd': 'animation.load'}, {'cmd': 'animation.load', 'file': '../gui_test.ani'}]:
            req(refused, False)
        result, _, _ = req({'cmd': 'animation.step', 'rows': 3})
        check('W527: animation.step moves the frame shown', result == {'row': 3}, str(result))
        result, _, _ = req({'cmd': 'animation.seek', 'row': 10})
        check('W527: animation.seek goes to a stored row', result == {'row': 10}, str(result))
        result, _, _ = req({'cmd': 'animation.step', 'rows': -4})
        check('W527: animation.step goes back with a negative number', result == {'row': 6}, str(result))
        result, _, _ = req({'cmd': 'animation.step', 'rows': 2147483647})
        check('W527: animation.step past the last row stops on it', result == {'row': 600}, str(result))
        req({'cmd': 'animation.seek', 'row': 6})
        result, _, _ = req({'cmd': 'animation.set_speed', 'ms': 20})
        check('W527: animation.set_speed sets the delay', result == {'speed_ms': 20}, str(result))
        result, _, _ = req({'cmd': 'animation.faster'})
        result_slower, _, _ = req({'cmd': 'animation.slower'})
        check('W527: animation.faster and slower change the delay by a step', result == {'speed_ms': 18} and result_slower == {'speed_ms': 20}, str((result, result_slower)))
        result, _, _ = req({'cmd': 'animation.pause'})
        check('W527: animation.pause with no Go running changes nothing', result == {'speed_ms': 20}, str(result))
        result, _, _ = req({'cmd': 'animation.set_frame_skip', 'skip': 100})
        check('W527: animation.set_frame_skip sets the skip', result == {'skip': 100}, str(result))
        result, _, _ = req({'cmd': 'animation.set_live_draw', 'enabled': True})
        check('W527: animation.set_live_draw switches drawing while integrating', result == {'live_draw': True}, str(result))
        req({'cmd': 'animation.set_live_draw', 'enabled': False})
        for refused in [{'cmd': 'animation.step'}, {'cmd': 'animation.step', 'rows': 1.5}, {'cmd': 'animation.seek', 'row': -1}, {'cmd': 'animation.seek'},
                        {'cmd': 'animation.set_speed', 'ms': 1001}, {'cmd': 'animation.set_speed', 'ms': -1}, {'cmd': 'animation.set_speed'},
                        {'cmd': 'animation.set_frame_skip', 'skip': 0}, {'cmd': 'animation.set_frame_skip', 'skip': 100000001}, {'cmd': 'animation.set_live_draw', 'enabled': 'yes'},
                        {'cmd': 'animation.set_live_draw'}, {'cmd': 'animation.pause', 'ms': 1}, {'cmd': 'animation.grab_point', 'x': 0}]:
            req(refused, False)
        result, _, _ = req({'cmd': 'animation.reset'})
        check('W527: animation.reset goes back to the first frame', result == {'frame': 0}, str(result))
        req({'cmd': 'animation.set_speed', 'ms': 0})
        result, evs, _ = req({'cmd': 'animation.play'})
        frames = [e for e in evs if e.get('ev') == 'ani' and e.get('op') == 'frame']
        check('W527: animation.play plays to the last frame', result == {'playing': False} and frames, str(result))
        result, evs, _ = req({'cmd': 'animation.export_frames', 'animated_gif': True, 'file': 'w527_an.gif'})
        check('W527: animation.export_frames writes one animated GIF of the frames', result == {'frames_saved': 7} and exists('w527_an.gif')
              and head('w527_an.gif', 6) [:4] == b'GIF8', str(result))
        result, evs, _ = req({'cmd': 'animation.export_frames', 'ppm': True, 'file': 'w527_an.ppm'})
        check('W527: animation.export_frames writes PPM files, the later ones named from the first', result == {'frames_saved': 7}
              and exists('w527_an.ppm', 'w527_an-frame1.ppm', 'w527_an-frame6.ppm') and head('w527_an.ppm', 2) == b'P6', str(result))
        for refused in [{'cmd': 'animation.export_frames', 'file': 'w527_x.ppm'}, {'cmd': 'animation.export_frames', 'ppm': True, 'animated_gif': True, 'file': 'w527_x.gif'},
                        {'cmd': 'animation.export_frames', 'ppm': True}, {'cmd': 'animation.export_frames', 'ppm': True, 'file': '../x.ppm'}]:
            req(refused, False)
        check('W527: a refused export writes nothing', not exists('w527_x.ppm', 'w527_x.gif'), str(os.listdir(run_dir)))
        _, _, error = req({'cmd': 'animation.grab_point', 'x': 0.5, 'y': 0.5, 'to_x': 0.5}, False)
        check('W527: animation.grab_point takes to_x and to_y together', 'to_x' in error or 'to_y' in error, error)
        req({'cmd': 'animation.load', 'file': 'nograb.ani'})
        _, _, error = req({'cmd': 'animation.grab_point', 'x': 0.5, 'y': 0.5}, False)
        check('W527: animation.grab_point on an animation with no grab points says so', 'no grab points' in error, error)
        req({'cmd': 'animation.load', 'file': 'gui_test.ani'})
        # a Go steered by the commands sent while it plays: its speed, then Pause stops it where it is
        req({'cmd': 'animation.set_speed', 'ms': 30})
        req({'cmd': 'animation.reset'})
        snd(cmd='animation.play')
        pre, _ = col(lambda e: e.get('ev') == 'ani' and e.get('op') == 'frame' and e.get('pos', 0) >= 3, timeout=60 * SLOW)
        snd(cmd='animation.faster')
        snd(cmd='animation.pause')
        evs, first = col(lambda e: e.get('ev') == 'done', timeout=60 * SLOW)
        evs2, second = col(lambda e: e.get('ev') == 'done', timeout=60 * SLOW)
        evs3, third = col(lambda e: e.get('ev') == 'done', timeout=60 * SLOW)
        stopped = [e['pos'] for e in pre + evs + evs2 + evs3 if e.get('ev') == 'ani'][-1:]
        check('W527: Go ends on Pause, and each command sent meanwhile has its own done after it, in the order sent',
              first and first.get('result') == {'playing': False} and second and second.get('result') == {'speed_ms': 28}
              and third and third.get('result') == {'speed_ms': 28} and stopped and stopped[0] < 600, str((first, second, third, stopped)))

        # the array plot
        _, _, error = req({'cmd': 'array_plot.redraw'}, False)
        check('W527: array_plot.redraw with no window says so', 'No array plot is open' in error, error)
        for command in ('array_plot.configure', 'array_plot.fit', 'array_plot.scroll'):
            _, _, error = req({'cmd': command, **({'first_row': 1} if command.endswith('scroll') else {})}, False)
            check('W527: %s with no window says so' % command, 'No array plot is open' in error, error)
        result, evs, _ = req({'cmd': 'array_plot.open'})
        check('W527: array_plot.open opens the window', result == {'open': True}
              and any(e.get('ev') == 'window' and e.get('op') == 'create' and e.get('win') == 'array_plot_1' for e in evs), str(result))
        result, evs, _ = req({'cmd': 'array_plot.configure', 'first_column': 'V', 'columns': 2, 'rows': 20, 'row_skip': 5, 'z_min': -1, 'z_max': 1, 'auto_redraw': False})
        ap = aplots(evs)
        check('W527: array_plot.configure sets the columns, rows and scale and the plot shows them',
              result == {'configured': True} and ap and (ap[-1]['nx'], ap[-1]['ny'], ap[-1]['zmin'], ap[-1]['zmax'], ap[-1]['first_row']) == (2, 20, -1, 1, 0), str(ap[-1:])[:300])
        result, evs, _ = req({'cmd': 'array_plot.configure', 'rows': 10})
        check('W527: array_plot.configure leaves what it does not name', aplots(evs) and (aplots(evs)[-1]['ny'], aplots(evs)[-1]['zmin']) == (10, -1), str(aplots(evs)[-1:])[:200])
        for refused in [{'cmd': 'array_plot.configure', 'z_min': 2}, {'cmd': 'array_plot.configure', 'z_max': -5}, {'cmd': 'array_plot.configure', 'first_column': 'nothing'},
                        {'cmd': 'array_plot.configure', 'columns': 0}, {'cmd': 'array_plot.configure', 'rows': 0}, {'cmd': 'array_plot.configure', 'row_skip': 0},
                        {'cmd': 'array_plot.configure', 'column_skip': 0}, {'cmd': 'array_plot.configure', 'first_row': -1}, {'cmd': 'array_plot.configure', 'auto_redraw': 'yes'},
                        {'cmd': 'array_plot.configure', 'columns': 2000000}, {'cmd': 'array_plot.configure', 'columns': 1001, 'rows': 1001},
                        {'cmd': 'array_plot.configure', 'rows': 1000, 'row_skip': 2147483647}]:
            req(refused, False)
        result, evs, _ = req({'cmd': 'array_plot.redraw'})
        check('W527: array_plot.redraw sends the plot again', result == {'redrawn': True} and aplots(evs), str(result))
        result, evs, _ = req({'cmd': 'array_plot.fit'})
        ap = aplots(evs)
        check('W527: array_plot.fit sets the scale to the data shown', ap and abs(result['fitted_min'] - ap[-1]['zmin']) < 1e-5
              and abs(result['fitted_max'] - ap[-1]['zmax']) < 1e-5 and result['fitted_min'] < result['fitted_max'], str((result, ap[-1:]))[:300])
        result, evs, _ = req({'cmd': 'array_plot.scroll', 'first_row': 7})
        check('W527: array_plot.scroll shows the plot from a stored row', result == {'first_row': 7} and aplots(evs) and aplots(evs)[-1]['first_row'] == 7, str(result))
        for refused in [{'cmd': 'array_plot.scroll', 'first_row': -1}, {'cmd': 'array_plot.scroll'}, {'cmd': 'array_plot.scroll', 'first_row': 1.5},
                        {'cmd': 'array_plot.scroll', 'first_row': 2147483647}]:
            req(refused, False)
        result, evs, _ = req({'cmd': 'array_plot.export_pdf', 'file': 'w527_ap.pdf', 'top_label': 'top', 'side_label': 'side', 'bottom_label': 'bottom'})
        check('W527: array_plot.export_pdf writes the picture', result == {'file': 'w527_ap.pdf'} and head('w527_ap.pdf', 4) == b'%PDF', str(result))
        result, evs, _ = req({'cmd': 'array_plot.export_gif', 'file': 'w527_ap.gif'})
        check('W527: array_plot.export_gif asks the picture and writes it', result.get('file') == 'w527_ap.gif'
              and result.get('bytes_written') == os.path.getsize(os.path.join(run_dir, 'w527_ap.gif')) > 20 and head('w527_ap.gif', 6) [:4] == b'GIF8', str(result))
        for refused in [{'cmd': 'array_plot.export_pdf'}, {'cmd': 'array_plot.export_pdf', 'file': '../a.pdf'}, {'cmd': 'array_plot.export_gif'},
                        {'cmd': 'array_plot.export_gif', 'file': 'a/b.gif'}, {'cmd': 'array_plot.range', 'file': 'r.gif', 'steps': 0}, {'cmd': 'array_plot.range'},
                        {'cmd': 'array_plot.range', 'file': 'r.gif', 'still': 'yes'}, {'cmd': 'array_plot.range', 'file': 'r.gif', 'variable': 'nothing'}]:
            req(refused, False)
        check('W527: a refused range leaves the next run alone', not exists('r.gif'), str(os.listdir(run_dir)))
        result, evs, _ = req({'cmd': 'array_plot.range', 'still': True, 'tag': True, 'file': 'w527_ar.gif', 'variable': 'iapp', 'start': 0, 'end': 0.1, 'steps': 1})
        check('W527: array_plot.range saves a GIF per run, tagged with the value, the later ones named from the first',
              result == {'steps_completed': 2} and exists('w527_ar.gif', 'w527_ar-frame1.gif')
              and any(e.get('tag') for e in aplots(evs)), str((result, os.listdir(run_dir))))
        result, evs, _ = req({'cmd': 'array_plot.range', 'still': False, 'file': 'w527_arm.gif', 'steps': 1})
        check('W527: array_plot.range with still false writes one movie', result == {'steps_completed': 2} and exists('w527_arm.gif')
              and not exists('w527_arm-frame1.gif'), str((result, os.listdir(run_dir))))
        # the old forms are gone
        for obsolete in [{'cmd': 'ani', 'op': 'pause'}, {'cmd': 'ani', 'op': 'speed', 'ms': 5}, {'cmd': 'ani', 'op': 'step', 'n': 1}, {'cmd': 'ani', 'op': 'seek', 'pos': 1},
                         {'cmd': 'ani', 'op': 'mouse', 'what': 'down', 'u': 0.5, 'v': 0.5}, {'cmd': 'aplot', 'op': 'scroll', 'dy': 1}]:
            snd(**obsolete)
            evs, _ = col(lambda e: e.get('ev') == 'idle')
            check('W527: the old %s %s form is unknown' % (obsolete['cmd'], obsolete['op']),
                  any('Unknown command' in e.get('error', '') for e in evs if e.get('ev') == 'message'), str(evs)[-300:])
    finally:
        stop_server(proc, run_dir, snd)


def check_equilibrium_settings():
    """W535: the equilibrium search asks no questions. Print eigenvalues, draw invariant sets and draw
    strong sets are numerics settings: equilibrium.configure sets them and the numerics event shows
    them, a bad value or an argument find no longer takes is refused and changes nothing, and find
    prints the eigenvalues exactly when print eigenvalues is on (the log, which --verbose shows)."""
    log = LogLines()
    p, r, snd, col, _ = launch_server(log=log, extra_args=['--verbose'])
    idle = lambda e: e.get('ev') in ('idle', 'done')
    printed = lambda lines: sum('Eigenvalues:' in line for line in lines)

    def call(**cmd):
        snd(**cmd)
        evs, _ = col(idle, timeout=60 * SLOW)
        return next((e for e in reversed(evs) if e.get('ev') == 'done'), {}), evs

    def setting(key):
        snd(cmd='events.subscribe', events=['numerics'])
        evs, _ = col(idle, timeout=60 * SLOW)
        numerics = next(e for e in reversed(evs) if e.get('ev') == 'numerics')
        return next((f['value'] for f in numerics['fields'] if f['key'] == key), None)

    try:
        col(idle)
        done, _ = call(cmd='equilibrium.find')
        check('W535: find with no arguments converges', done.get('status') == 'ok' and done.get('result', {}).get('converged') is True, str(done)[:300])
        check('W535: print eigenvalues is off by default', setting('print_eigenvalues') is False, str(done)[:300])

        done, _ = call(cmd='equilibrium.configure', print_eigenvalues=True)
        check('W535: configure sets print eigenvalues', done.get('status') == 'ok' and setting('print_eigenvalues') is True, str(done)[:300])
        call(cmd='equilibrium.find')
        log.wait_for(lambda lines: printed(lines) >= 1)
        check('W535: find prints the eigenvalues when print eigenvalues is on', printed(log) == 1, str(log)[-600:])

        call(cmd='equilibrium.configure', print_eigenvalues=False)
        call(cmd='equilibrium.find')
        call(cmd='equilibrium.configure', print_eigenvalues=True)
        call(cmd='equilibrium.find')
        log.wait_for(lambda lines: printed(lines) >= 2)
        check('W535: find prints nothing when print eigenvalues is off', printed(log) == 2, str(log)[-600:])

        done, _ = call(cmd='equilibrium.find', print_eigenvalues=True)
        check('W535: find refuses the old print_eigenvalues argument', done.get('status') == 'error', str(done)[:300])
        done, _ = call(cmd='equilibrium.configure', print_eigenvalues='yes')
        check('W535: configure refuses a word for a boolean and keeps the setting', done.get('status') == 'error' and setting('print_eigenvalues') is True, str(done)[:300])

        done, _ = call(cmd='equilibrium.configure', draw_strong_sets=True, newton_tolerance=-1)
        check('W535: configure is all or nothing for the boolean settings', done.get('status') == 'error' and setting('draw_strong_sets') is False, str(done)[:300])

        done, _ = call(cmd='equilibrium.configure', print_eigenvalues=False, draw_invariant_sets=True, draw_strong_sets=True)
        check('W535: configure sets all three settings at once',
              done.get('status') == 'ok' and setting('print_eigenvalues') is False and setting('draw_invariant_sets') is True
              and setting('draw_strong_sets') is True, str(done)[:300])
    finally:
        stop_server(p, r, snd)


def check_atomic_commands():
    """W534: a bad final argument leaves values, windows, files and settings untouched."""
    p, folder, send, collect, _ = launch_server()
    is_idle = lambda event: event.get("ev") in ("idle", "done")
    try:
        initial, _ = collect(is_idle)
        registry = next(e['commands'] for e in initial if e.get('ev') == 'hello')

        def call(**command):
            send(**command)
            events, _ = collect(is_idle, timeout=60 * SLOW)
            return next((e for e in reversed(events) if e.get('ev') == 'done'), {}), events

        def state():
            done, _ = call(cmd='session.get_state')
            return done.get('result', {}).get('state', done.get('result'))

        def saved():
            done, _ = call(cmd='session.save_copy', file='atomic.snapx', data=True, confirm=True)
            assert done.get('result', {}).get('copied'), done
            with zipfile.ZipFile(os.path.join(folder, 'atomic.snapx')) as archive:
                return {name: archive.read(name) for name in archive.namelist()}

        def refused(command, archive=True):
            before_state = state()
            before = saved() if archive else None
            done, events = call(**command)
            after_state = state()
            after = saved() if archive else None
            changed = [name for name in set(before or {}) | set(after or {}) if (before or {}).get(name) != (after or {}).get(name)]
            # A rejection may publish an unchanged state, but no event claiming a value, plot or file changed.
            claims = [e for e in events if e.get('ev') in ('window', 'marks', 'plots', 'browser', 'series', 'aplot') or (e.get('ev') == 'saved' and e.get('saved'))]
            check('W534: %s refuses the last value without changing the session' % command['cmd'],
                  done.get('status') == 'error' and before_state == after_state and not changed and not claims,
                  {'done': done, 'changed': changed, 'state': (before_state, after_state) if before_state != after_state else 'unchanged', 'claims': claims})

        # The schema checks the whole argument vector before entering any handler. Exercise every command
        # with several values, including the front end's own, with valid earlier types and a bad final type.
        def valid(argument):
            kind = argument['type']
            if kind == 'boolean': return True
            if kind in ('integer', 'real'):
                low, high = argument.get('min', 1), argument.get('max', 100)
                return max(low, min(high, 2))
            if kind == 'choice': return argument['choices'][0]
            if kind == 'name': return 'iapp' if argument.get('names') == 'parameter' else 'v'
            if kind == 'window': return 'graph_1'
            if kind == 'new_name': return 'atomic candidate'
            if kind == 'color': return 'red'
            if kind == 'list[real, 2]': return [0, 1]
            if kind in ('list', 'list[text]'): return []
            if kind == 'map': return {}
            if kind in ('file', 'path'): return 'atomic-output.dat'
            if kind == 'expression': return '1'
            return 'atomic candidate'

        for row in registry:
            arguments = row.get('arguments', [])
            if len(arguments) == 1 and arguments[0]['type'] in ('map', 'list'):
                argument = arguments[0]
                if argument['type'] == 'map':
                    first = 'iapp' if argument.get('names') == 'parameter' else 'v'
                    element = '1' if argument.get('element') == 'expression' else 17
                    value = {first: element, 'absent': {}}
                else:
                    value = [argument.get('choices', ['series'])[0], {}]
                refused({'cmd': row['cmd'], argument['name']: value}, archive=False)
                continue
            if len(arguments) < 2: continue
            command = {'cmd': row['cmd']}
            for argument in arguments[:-1]: command[argument['name']] = valid(argument)
            last = arguments[-1]
            command[last['name']] = 'wrong container' if last['type'] in ('map', 'list', 'list[text]', 'list[real, 2]') else {}
            refused(command, archive=False)

        # Handler/owner checks go beyond the schema's individual types. These cases reached apply-then-check
        # paths before W534; the saved members expose settings that session.get_state does not list.
        refused(dict(cmd='value.set_parameters', values={'iapp': 17, 'absent': 2}))
        refused(dict(cmd='ic.set', values={'v': 17, 'absent': 2}))
        refused(dict(cmd='equilibrium.configure', max_iterates=17, newton_tolerance=-1))
        refused(dict(cmd='run.configure', total_time=17, delay_grid=3))
        refused(dict(cmd='run.configure', total_time=17, min_step=10, max_step=1))
        refused(dict(cmd='bvp.configure', max_iterates=17, tolerance=-1))
        call(cmd='run.initial')
        analyses = [
            (dict(cmd='analysis.histogram', bins=10, low=-1, high=1, condition=''),
             dict(cmd='analysis.histogram', bins=17, low=0, high=-1)),
            (dict(cmd='analysis.histogram', bins=10, low=-1, high=1, condition=''),
             dict(cmd='analysis.histogram', bins=17, condition='(')),
            (dict(cmd='analysis.histogram_2d', bins1=3, bins2=4, low1=-1, high1=1, low2=-1, high2=1),
             dict(cmd='analysis.histogram_2d', bins1=17, low1=0, high1=1, low2=0, high2=-1)),
            (dict(cmd='analysis.autocorrelation', bins=10, low=-1, high=1),
             dict(cmd='analysis.autocorrelation', bins=17, low=0, high=-1)),
            (dict(cmd='analysis.cross_correlation', column1='v', column2='w', lags=10),
             dict(cmd='analysis.cross_correlation', column1='w', lags=1000)),
            (dict(cmd='analysis.spectral_density', column='v', window_length=32),
             dict(cmd='analysis.spectral_density', column='w', window_length=100000)),
        ]
        for setup, command in analyses:
            call(cmd='analysis.load_stochastic_run')
            expected, _ = call(**setup)
            call(cmd='analysis.load_stochastic_run')
            refused(command)
            got, _ = call(cmd=setup['cmd'])
            check('W534: %s keeps its earlier settings for the next call' % command['cmd'],
                  got.get('status') == 'ok' and got.get('result') == expected.get('result'), got)
        call(cmd='analysis.load_stochastic_run')
        fit_probe = dict(cmd='analysis.fit_curve', variables=['v'], parameters=['iapp'])
        expected, _ = call(**fit_probe)
        refused(dict(cmd='analysis.fit_curve', variables=['v'], parameters=['iapp'], data_columns=[2], points=17, columns=3, file='absent.dat'))
        got, _ = call(**fit_probe)
        check('W534: a refused fit keeps its earlier lists and file settings', got.get('error') == expected.get('error'), got)
        refused(dict(cmd='analysis.adjoint_interaction', couplings={'v': 'v', 'w': 'w'}))
        call(cmd='array_plot.open')
        call(cmd='array_plot.configure', first_column='v', columns=2, rows=10, z_min=-1, z_max=1)
        os.mkdir(os.path.join(folder, 'blocked.pdf'))
        os.mkdir(os.path.join(folder, 'blocked.svg'))
        for command in (
            dict(cmd='plot.export_pdf', axes_font_size=27, color=False, file='blocked.pdf'),
            dict(cmd='plot.export_svg', axes_font_size=27, color=False, file='blocked.svg'),
            dict(cmd='array_plot.export_pdf', top_label='must not remain', file='blocked.pdf'),
        ):
            suffix = '.svg' if command['cmd'].endswith('svg') else '.pdf'
            before_file = 'before-' + command['cmd'] + suffix
            after_file = 'after-' + command['cmd'] + suffix
            baseline = dict(axes_font_size=12, color=True) if command['cmd'].startswith('plot.') else dict(top_label='original top', side_label='original side', bottom_label='original bottom')
            call(cmd=command['cmd'], file=before_file, **baseline)
            refused(command)
            call(cmd=command['cmd'], file=after_file)
            with open(os.path.join(folder, before_file), 'rb') as before, open(os.path.join(folder, after_file), 'rb') as after:
                check('W534: %s keeps export settings for the next file' % command['cmd'], before.read() == after.read())
        call(cmd='auto.open')
        call(cmd='auto.new_view')
        refused(dict(cmd='auto.set_display', view='diagram_1', x_range=[0, 1], y_range=[1, 0]))
        call(cmd='window.select', window='diagram_2')
        refused(dict(cmd='auto.export_pdf', window='diagram_1', file='blocked.pdf'))
        call(cmd='window.select', window='diagram_2')
        refused(dict(cmd='auto.export_svg', window='diagram_1', file='blocked.svg'))
        refused(dict(cmd='auto.run', kind='equilibrium', decimals=4, hold=1, maximum=1, parameter_min=10, parameter_max=1))
        call(cmd='window.create', name='second graph')
        call(cmd='window.select', window='diagram_2')
        refused(dict(cmd='plot.set_2d', window='graph_1', x_min=0, x_max=1, y_min=1, y_max=0))
        selection, _ = call(cmd='window.set_redraw_mode', mode='auto')
        check('W534: a refused graph edit preserves the selected AUTO window', selection.get('status') == 'error', selection)
        refused(dict(cmd='auto.configure_numerics', max_steps=17, parameter_min=10, parameter_max=1))
        call(cmd='recording.start')
        call(cmd='recording.add_note', note='keep for the next successful step')
        refused(dict(cmd='run.configure', total_time=17, dt=0), archive=False)
        call(cmd='recording.stop', file='atomic-note.recx', confirm=True)
        # Refusing a numerics edit must not consume or clear redo history either.
        call(cmd='value.set_parameters', values={'iapp': 0.123})
        call(cmd='value.undo')
        refused(dict(cmd='run.configure', total_time=17, bandlo=1))
        done, _ = call(cmd='value.redo')
        check('W534: refused numerics keeps redo history', done.get('status') == 'ok', done)
    finally:
        stop_server(p, folder, send)
    # Keep the published Morris-Lecar equations and values; only the demonstration action is made invalid.
    with tempfile.TemporaryDirectory(prefix='xppatomic-action') as source_folder:
        model = os.path.join(source_folder, 'lecar.odex')
        with open('examples/ode/lecar.odex') as original, open(model, 'w') as changed:
            changed.write(original.read().replace('{total_time=100,iapp=.1}', '{iapp=.1,dt=0}'))
        p, folder, send, collect, _ = launch_server(ode=model)
        try:
            collect(is_idle)
            _, events = call(cmd='session.get_source')
            comments = next(e['comments'] for e in events if e.get('ev') == 'source')
            index = next(i for i, comment in enumerate(comments) if comment[1])
            refused(dict(cmd='tool.run_source_action', index=index))
        finally:
            stop_server(p, folder, send)



INDEPENDENT_SECTIONS = {'atomic': check_atomic_commands, 'equilibria': check_equilibrium_settings, 'registry': check_registry, 'session': check_session_commands, 'data': check_data_value_set, 'kinescope': check_kinescope_animation_array_plot, 'stops': lambda: (check_stops(), check_auto_stops(), check_auto_starts(), check_auto_two_parameter_starts(), check_auto_periodic_start_kind(), check_auto_registry(), check_fit_stops(), check_event_count_note(), check_auto_restart_file_stops(), check_auto_multiplier_note(), check_auto_singular_pivot())}
if args.only:
    for name in args.only.split(','):
        if name not in INDEPENDENT_SECTIONS:
            sys.exit('servercheck: --only has no section %r (%s)' % (name, ', '.join(INDEPENDENT_SECTIONS)))
        INDEPENDENT_SECTIONS[name]()
    print('server checks: %d checks, %s' % (check.checks, 'all passed' if check.failures == 0 else '%d failed' % check.failures))
    sys.exit(1 if check.failures else 0)


check_atomic_commands()
check_registry()
check_session_commands()
check_equilibrium_exports()
check_data_value_set()
check_kinescope_animation_array_plot()
check_diagram_subscription()

proc, run, send, collect, events = launch_server()


check_logging()


def check_load_error():
    """A model that does not load (W63c, docs/protocol.md "A model that does
    not load"): the server sends one `error` event, in place of hello, with
    the file, line, column and what is wrong (`error`), and the line as
    written, then exits 1; the log reads as it did, its first line at the
    place (file:line: what, W140). A problem in an included file is at its
    line in that file, in the event and the log alike."""
    bad_dir = tempfile.mkdtemp(prefix='xppbadload')
    try:
        def load(ode):
            p = subprocess.run([os.path.abspath(args.server), '--server', ode], cwd=bad_dir, input='',
                               capture_output=True, text=True, encoding='utf-8', timeout=30 * SLOW)
            evs = [json.loads(l) for l in p.stdout.splitlines() if l.strip()]
            return p, evs

        with open(os.path.join(bad_dir, 'bad.ode'), 'w') as f:
            f.write("# a model that does not load\npar a=1\nx'=-x+a*\ninit x=1\ndone\n")
        p, evs = load('bad.ode')
        errs = [e for e in evs if e.get('ev') == 'error']
        e = errs[0] if errs else {}
        check('load error: a model that does not load exits 1 after one error event, no hello',
              p.returncode == 1 and len(errs) == 1 and not any(v.get('ev') == 'hello' for v in evs),
              '%r %r' % (p.returncode, [v.get('ev') for v in evs]))
        check('load error: the event names the file, the line and what is wrong',
              e.get('file') == 'bad.ode' and e.get('line') == 3 and e.get('col') == 0
              and "ERROR compiling X'" in e.get('error', '') and e.get('source') == "x'=-x+a*", str(e))
        check('load error: the log reads as before', "ERROR compiling X'" in p.stderr
              and 'Premature end of expression' in p.stderr, repr(p.stderr[-300:]))

        with open(os.path.join(bad_dir, 'inc.ode'), 'w') as f:
            f.write("par b=2\ny'=-y+(b\n")
        with open(os.path.join(bad_dir, 'main.ode'), 'w') as f:
            f.write("par a=1\n#include inc.ode\nx'=-x\ndone\n")
        p, evs = load('main.ode')
        e = next((v for v in evs if v.get('ev') == 'error'), {})
        check('load error: a problem in an included file is at its line there',
              p.returncode == 1 and e.get('file') == 'inc.ode' and e.get('line') == 2
              and e.get('source') == "y'=-y+(b", str(e))

        # W140: an option an included file refuses names that file and line,
        # in the event and in the log (written once, at its place)
        with open(os.path.join(bad_dir, 'opts.inc'), 'w') as f:
            f.write("@ total=abc\n")
        with open(os.path.join(bad_dir, 'withopts.ode'), 'w') as f:
            f.write("par a=1\nx'=-a*x\n#include opts.inc\ninit x=1\ndone\n")
        p, evs = load('withopts.ode')
        e = next((v for v in evs if v.get('ev') == 'error'), {})
        check("load error: an included file's refused option is at its file and line (W140)",
              p.returncode == 1 and e.get('file') == 'opts.inc' and e.get('line') == 1
              and e.get('source') == '@ total=abc' and e.get('error') == 'the option `total=abc`: not a number', str(e))
        check('load error: the log names the file and line, once (W140)',
              p.stderr.count('opts.inc:1: the option `total=abc`: not a number') == 1
              and p.stderr.count('not a number') == 1, repr(p.stderr[-300:]))

        with open(os.path.join(bad_dir, 'bad.odex'), 'w') as f:
            f.write("par a = 1\nx' = -x + * a\n")
        p, evs = load('bad.odex')
        e = next((v for v in evs if v.get('ev') == 'error'), {})
        check('load error: an .odex problem has its line and column',
              p.returncode == 1 and e.get('file') == 'bad.odex' and e.get('line') == 2 and e.get('col', 0) > 0
              and e.get('source') == "x' = -x + * a" and e.get('error'), str(e))

        # a value an option refuses (W119): the load stops at the option's line
        for name, text, line, src, needle in (
                ('badopt.ode', "par a=1\nx'=-x\n@ total=5\n@ ync=12\ndone\n", 4, '@ ync=12', 'ync=12'),
                ('badnum.ode', "par a=1\nx'=-x\n@ total=5\n@ dt=2*3\ndone\n", 4, '@ dt=2*3', 'dt=2*3'),
                ('badopt.odex', "par a = 1\nx' = -x\noptions {\n  integration: { total_time: 5 },\n  plot: { ync: 12 },\n}\n",
                 5, '  plot: { ync: 12 },', 'double quotes'),
                # W206: XPPAUT's name for store_every is a .ode's; a .odex refuses it
                ('oldname.odex', "par a = 1\nx' = -x\noptions {\n  integration: { total_time: 5, nout: 2 },\n}\n",
                 4, '  integration: { total_time: 5, nout: 2 },', 'nout')):
            with open(os.path.join(bad_dir, name), 'w') as f:
                f.write(text)
            p, evs = load(name)
            e = next((v for v in evs if v.get('ev') == 'error'), {})
            check('load error: a value an option refuses stops the load at its line (%s)' % name,
                  p.returncode == 1 and e.get('file') == name and e.get('line') == line
                  and needle in e.get('error', '') and e.get('source') == src, str(e))
        for method, reason in (('symplectic', 'even dimensions'), ('rubbish', 'is none of')):
            with open(os.path.join(bad_dir, 'badmethod.odex'), 'w') as f:
                f.write("x' = -x\ny' = x\nz' = y\noptions { integration: { method: " + method + " } }\n")
            p, evs = load('badmethod.odex')
            e = next((v for v in evs if v.get('ev') == 'error'), {})
            check('W132: method: %s fails the whole load at its line' % method,
                  p.returncode == 1 and e.get('file') == 'badmethod.odex' and e.get('line') == 4
                  and e.get('source') == 'options { integration: { method: ' + method + ' } }' and reason in e.get('error', '')
                  and not any(v.get('ev') == 'hello' for v in evs), str(e))
    finally:
        shutil.rmtree(bad_dir, ignore_errors=True)


check_load_error()


def is_state(e):
    return e.get('ev') == 'state'


def is_idle(e):
    return e.get('ev') in ('idle','done')


def last_state(evs):
    st = [e for e in evs if e.get('ev') == 'state']
    return st[-1] if st else None


evs, _ = collect(is_idle)
st = last_state(evs)
hello = next((e for e in evs if e.get('ev') == 'hello'), None)
check('hello', hello is not None and len([c for c in hello.get('commands', []) if 'arguments' in c]) > 200)
check('hello has no legacy menu tables: no command_table, menu_names, command_categories or windows',
      hello is not None and not {'command_table', 'menu_names', 'command_categories', 'windows'} & set(hello), str(sorted(hello or {})))
check('hello says protocol 4, and no draw ops or palette follow (removed in 2)',
      hello is not None and hello.get('protocol') == 4 and not any(e.get('ev') in ('draw', 'palette') for e in evs),
      str(hello and hello.get('protocol')))
about_parts = [p for line in (hello or {}).get('about', []) for p in line]
about_links = {p['text']: p['url'] for p in about_parts if 'url' in p}
about_text = ''.join(p['text'] for p in about_parts)
check('hello carries the About lines: version, author, credit, build, and the five links with their targets',
      hello is not None and hello['about'][0][0]['text'].startswith('xppautX ')
      and about_links == {'Email': 'mailto:muhammadmoustafa22@gmail.com',
                          'GitHub': 'https://github.com/MuhammadMoustafa',
                          'LinkedIn': 'https://www.linkedin.com/in/muhammad-ahmad-62743a125/',
                          'Source': 'https://github.com/MuhammadMoustafa/xppautX',
                          'Report a problem': 'https://github.com/MuhammadMoustafa/xppautX/issues'}
      and 'by Muhammad Ahmad' in about_text and 'Based on XPPAUT 8.0 by G. Bard Ermentrout' in about_text
      and 'Protocol 4' in about_text,
      str(hello and hello.get('about')))
check('state after hello', st is not None and any(p[0] == 'iapp' for p in st['pars']), str(st))
check('window 1 is created', any(e.get('ev') == 'window' and e.get('op') == 'create' and e.get('win') == 'graph_1'
                                 for e in evs))
send(cmd='size', window='graph_1', w=800, h=600)
evs, _ = collect(is_idle)
check('size is no command any more (removed in protocol 2): an unknown-command error, then state and idle',
      [e.get('ev') for e in evs] == ['message', 'state', 'idle'] and 'Unknown command' in evs[0].get('error', ''),
      str(evs)[:200])
# W493: a command's arguments are read by their kinds; a bad one is an error naming the command and the argument
for bad_args, what in (({'label': 'x'}, 'Argument "label" must be a whole number'),
                   ({'label': 1.5}, 'Argument "label" must be a whole number')):
    send(cmd='auto.run', kind='equilibrium', **bad_args)
    evs, _ = collect(is_idle)
    msg = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
    check('a command argument of the wrong kind is a command error that names it: ' + what,
          what in msg.get('error', ''), str(evs)[:300])
proc.stdin.write('{"cmd":"auto" "op":"restart"}\n')
proc.stdin.flush()
evs, _ = collect(is_idle)
msg = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
check('a command line that is not JSON is an error naming where it stops',
      'the command line is not valid JSON (at character 15)' in msg.get('error', ''), str(evs)[:300])
check('hello lists the series feature', 'series' in hello.get('features', []), str(hello.get('features')))

# W95: every action's kind, one letter (c control, v view, d data, x
# computation): every command of the registry and the few that are not (the
# command table core/ui_json.cpp dispatches from); the documented commands
# (docs/protocol.md's table) are each in it
def check_hello_kinds():
    cmds = hello.get('commands', [])
    kinds = {(c['cmd'], c.get('op')): c['kind'] for c in cmds}
    doc = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'protocol.md'),
               encoding='utf-8').read()
    section = doc[doc.index('## Commands (client to server)'):doc.index('## Action kinds')]
    documented = set(re.findall(r'^\| `([\w.]+)` \|', section, re.M)) - {'key'}
    missing = sorted(d for d in documented if not any(c == d for c, _ in kinds))
    check('W95: hello gives every documented command but key a kind', not missing and cmds
          and all(k in 'cvsdx' for k in kinds.values()), str(missing))
    want = {('window.set_display', None): 'v', ('set', None): 's', ('value.set_parameters', None): 's', ('value.save_file', None): 'd',
            ('run.abort', None): 'c', ('run.initial', None): 'x', ('run.configure', None): 's', ('ic.set', None): 's', ('browser.export', None): 'd', ('browser.page', None): 'v', ('auto.configure_numerics', None): 's',
            ('auto.grab', None): 'd', ('auto.run', None): 'x', ('tool.run_user_button', None): 'x',
            ('equilibrium.find', None): 'x', ('session.save', None): 'd', ('session.quit', None): 'c'}
    check('W95, W106: kinds as decided: display a view, set/value.set_parameters/auto.configure_numerics settings, Save data and auto.grab data, auto.run a computation, '
          'abort control, a user button a computation',
          all(kinds.get(k) == v for k, v in want.items()), str({k: kinds.get(k) for k in want}))


check_hello_kinds()

# W517: the page's menu bar, palette, keymap editor and quick access are made from the registry rows of hello:
# each row has a menu path in one of the bar's menus, a description and a category, and the default keys are the table's
MENUS = ['File', 'Run', 'Analysis', 'Plot', 'Data', 'AUTO', 'Windows', 'Help']
CATEGORIES = ['run', 'files', 'analysis', 'plot', 'tools', 'panels']


def check_registry_menu(table):
    bad = [r['cmd'] for r in table if r['menu_path'].split('/')[0] not in MENUS or not r['description'].strip() or r['category'] not in CATEGORIES]
    check('W517: every registry command has a menu path in File, Run, Analysis, Plot, Data, AUTO, Windows or Help, a description and a known category',
          not bad, str(bad))
    paths = [r['menu_path'] for r in table if r['menu_path'].split('/')[0] not in ('Help',)]
    check('W517: every one of the bar\'s menus has commands', all(any(p.split('/')[0] == m for p in paths) for m in MENUS[:-1]), str(sorted(set(p.split('/')[0] for p in paths))))
    chords = {r['cmd']: r['default_keys'] for r in table if r['default_keys']}
    check('W517: only the frequent commands have default keys, none reserved (Alt+F4, Ctrl+W, Ctrl+Q, F11, F12, F5)',
          chords == {'run.initial': ['Ctrl+R'], 'run.abort': ['Escape'], 'value.undo': ['Ctrl+Z'], 'value.redo': ['Ctrl+Shift+Z'],
                     'session.open_model': ['Ctrl+O'], 'session.save': ['Ctrl+S'], 'session.save_as': ['Ctrl+Shift+S'],
                     'tool.command_palette': ['Ctrl+Shift+P'], 'tool.manual': ['F1']}, str(chords))
    check('W536: a command row carries no XPPAUT letters', not any('legacy_keys' in r or 'legacy_context' in r for r in table), str(table[:1]))


check_registry_menu([c for c in hello.get('commands', []) if 'arguments' in c])


# W118: what core and page both know comes in hello, written once in the core
def check_hello_shared():
    check('W118: hello gives the limits (64 MB uploads, 2000 rows and 500 columns a browser block) and the window ids',
          hello.get('limits') == {'upload': 64 << 20, 'browser_rows': 2000, 'browser_cols': 500}
          and hello.get('window_names') == {'main':'graph_1','auto':'diagram_1','ani':'animation','aplot':'array_plot_1','browser':'browser'},
          str(hello.get('limits')) + str(hello.get('window_names')))
    check('W121b: hello gives the core player speed range', hello.get('player_speed') == {'min': 0.25, 'max': 8} and hello.get('upload_error') == 'larger than 64 MB', str(hello.get('player_speed')))
    steps = {(c['cmd'], c.get('op')): c.get('step') for c in hello.get('commands', [])}
    check('W118: every command says whether it is a step: a key, set and a write are, state, data, an answer, '
          'the browser\'s paging and the recording\'s own are not',
          all(isinstance(v, bool) for v in steps.values()) and steps.get(('key', None)) is True
          and steps.get(('set', None)) is True and steps.get(('browser.export', None)) is True
          and not any(steps.get(k) for k in (('session.get_state', None), ('events.subscribe', None), ('answer', None), ('browser.page', None),
                                             ('recording.start', None), ('recording.play', None))), str(steps))


check_hello_shared()
# W118: an error about a file the command could not read names the file (the page offers to add it)
send(cmd='session.import_settings', file='w118-gone.set')
evs, _ = collect(is_idle)
err = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), None)
check('W118: File/Import settings of a file that is not there: the error names the file',
      err is not None and err.get('file') == 'w118-gone.set', str(err))
send(cmd='events.subscribe', events=['series'])
evs, _ = collect(is_idle)
ser = [e for e in evs if e.get('ev') == 'series']
check('asking for the series data sends the plot at once', len(ser) == 1 and ser[0]['win'] == 'graph_1' and ser[0]['rows'] == 0
      and len(ser[0]['curves']) == 1, str(ser)[:300])
send(cmd='run.initial')
evs, _ = collect(is_idle, timeout=30 * SLOW)
st = last_state(evs)
check('storage has 601 rows', st is not None and st['rows'] == 601, str(st and st['rows']))


f32 = lambda v: struct.unpack('<f', struct.pack('<f', v))[0]


def values(ev_col, enc):
    """a series column's data as floats (NaN for null), whatever the encoding"""
    d = ev_col['data']
    if enc == 'f32':
        raw = base64.b64decode(d)
        return list(struct.unpack('<%df' % (len(raw) // 4), raw))
    return [float('nan') if v is None else f32(v) for v in d]


def same_floats(a, b):
    return len(a) == len(b) and all(x == y or (x != x and y != y) for x, y in zip(a, b))


def series_matches_output_dat(ser, ode=None):
    """the series' columns hold the numbers output.dat has for the same run
    (xppautX --silent): both are the stored floats, output.dat prints %.8g"""
    ode = ode or args.ode
    silent = tempfile.mkdtemp(prefix='xppsilent')
    copy_model(ode, silent)
    subprocess.run([os.path.abspath(args.server), os.path.basename(ode), '--silent'], cwd=silent,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60 * SLOW)
    with open(os.path.join(silent, 'output.dat')) as f:
        rows = [l.split() for l in f if l.strip()]
    shutil.rmtree(silent, ignore_errors=True)
    for c in ser['columns']:
        want = [r[c['col']] for r in rows]
        got = ['%.8g' % v for v in values(c, ser.get('enc'))]
        if got != want:
            bad = next(i for i, (a, b) in enumerate(zip(got, want)) if a != b) if len(got) == len(want) else -1
            return 'column %s differs at row %d (%d rows, output.dat %d)' % (c['name'], bad, len(got), len(want))
    return None


ser = whole_series(evs)  # the whole series: a full one, or a live run's appends and end
check('integration sends the series: the curve V against W, 601 rows',
      len(ser) == 1 and ser[0]['rows'] == 601 and ser[0]['curves'][0]['x'] == 1 and ser[0]['curves'][0]['y'] == 2
      and [c['name'] for c in ser[0]['columns']] == ['T', 'V', 'W'], str(ser)[:300])
if ser:
    problem = series_matches_output_dat(ser[0])
    check('the series carries the numbers of output.dat', problem is None, problem or '')
send(cmd='window.redraw')
evs, _ = collect(is_idle)
check('a redraw of unchanged data sends no series', not any(e.get('ev') == 'series' for e in evs))
send(cmd='plot.variable_vs_time', variable='V')
evs, _ = collect(is_idle)
ser = [e for e in evs if e.get('ev') == 'series']
check('Xi vs t sends the series of V against T', len(ser) == 1 and ser[0]['curves'][0]['x'] == 0
      and ser[0]['curves'][0]['y'] == 1 and [c['name'] for c in ser[0]['columns']] == ['T', 'V'], str(ser)[:300])
send(cmd='events.unsubscribe', events=ALL_EVENTS)
collect(is_idle)
send(cmd='run.initial')
evs, _ = collect(is_idle, timeout=30 * SLOW)
check('an empty data list stops the series', not any(e.get('ev') == 'series' for e in evs))

send(cmd='value.set_parameters', values={'iapp': 0.1})
send(cmd='session.get_state')
evs, st = collect(is_state)
collect(is_idle)
check('set parameter', st is not None and dict(st['pars'])['iapp'] == 0.1, str(st and st['pars']))

collect(is_idle)  # the idle of the state command above
send(cmd='value.set_by_formula', name='iapp', formula='0.1*2')
evs, _ = collect(is_idle)
st = last_state(evs)
check('set parameter by a formula', st is not None and abs(dict(st['pars'])['iapp'] - 0.2) < 1e-12, str(st and st['pars']))
send(cmd='set.restore_defaults', scope='parameters')
evs, _ = collect(is_idle)
st = last_state(evs)
check('default parameters', st is not None and dict(st['pars'])['iapp'] == 0.05, str(st and st['pars']))

# W66: the values panel's Save/Load of XPP's own .par/.ic file, through the
# core (docs/protocol.md "values") -- omitting `name` asks for one, like
# any other Save/Load; given, it skips the ask and writes into the
# model's folder at once
send(cmd='value.save_file', kind='parameters')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('value.save_file with no file asks for one', ask is not None and ask['kind'] == 'file'
      and ask.get('mode') == 'write', str(ask))
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
collect(is_idle)

send(cmd='value.save_file', kind='parameters', file='t_values.par')
collect(is_idle)
with open(os.path.join(run, 't_values.par')) as f:
    par_text = f.read()
first_line = par_text.split('\n', 1)[0]
check('values write par: XPP\'s format, a count then "value  name" lines',
      first_line.endswith('   Number params') and first_line.split()[0].isdigit()
      and '0.05  iapp\n' in par_text, par_text[:80])

send(cmd='value.save_file', kind='initial_conditions', file='t_values.ic')
collect(is_idle)
with open(os.path.join(run, 't_values.ic')) as f:
    ic_lines = [l.strip() for l in f if l.strip()]
check('values write ic: the values alone, one per line, in the model order',
      ic_lines == ['-0.144', '0.03'], str(ic_lines))

# change both, away from what was just written
send(cmd='value.set_parameters', values={'iapp': 0.09})
collect(is_idle)
send(cmd='ic.set', values={'V': -0.4})
evs, _ = collect(is_idle)
st = last_state(evs)
check('values round trip: the values changed before reading back',
      st is not None and dict(st['pars'])['iapp'] == 0.09 and dict(st['ics'])['V'] == -0.4,
      str(st and (st['pars'], st['ics'])))

# read them back through the existing load path (io_parameter_file/io_ic_file, READEM)
send(cmd='value.load_file', kind='parameters', file='t_values.par')
evs, _ = collect(is_idle)
st = last_state(evs)
check('values read par restores the written value exactly',
      st is not None and dict(st['pars'])['iapp'] == 0.05, str(st and st['pars']))

send(cmd='value.load_file', kind='initial_conditions', file='t_values.ic')
evs, _ = collect(is_idle)
st = last_state(evs)
check('values read ic restores the written values exactly',
      st is not None and dict(st['ics'])['V'] == -0.144 and dict(st['ics'])['W'] == 0.03,
      str(st and st['ics']))

send(cmd='value.save_file', kind='xyz', file='t_values.bad')
evs, msg = collect(lambda e: e.get('ev') in ('message', 'done') and 'error' in e)
check('value.save_file with a kind other than parameters/initial_conditions is refused', msg is not None, str(msg))
collect(is_idle)

send(cmd='browser.page', **{'from_row': 10, 'count': 3, 'first_column': 2, 'columns': 1})
evs, br = collect(lambda e: e.get('ev') == 'browser')
collect(is_idle)
check('browser sends the rows asked for', br is not None and br['rows'] == 601 and br['from'] == 10
      and len(br['data']) == 3 and len(br['data'][0]) == 2 and br['cols'][:3] == ['T', 'V', 'W'], str(br)[:200])
send(cmd='ic.from_data_row', row=10)
evs, _ = collect(is_idle)
st = last_state(evs)
check('ic.from_data_row sets the initial conditions from a row',
      st is not None and abs(dict(st['ics'])['W'] - br['data'][0][1]) < 1e-6, str(st and st['ics']))
send(cmd='browser.page', **{'from_row': 0, 'count': 0})
collect(is_idle)

# W77: the data browser's Add column is the Session's, not the Model's
# (docs/roadmap.md W77, #125): it must not grow neq/nvar or add a
# variable, so it stays invisible to everything that reads the model --
# but docs/manual/07-data-browser.md promises it stays computed "as
# though ... another auxiliary variable", so it is recomputed (not
# dropped) after every run, and find_variable resolves it by name so it
# can be plotted.
def fetch_browser(nrows):
    send(cmd='browser.page', **{'from_row': 0, 'count': nrows, 'first_column': 1, 'columns': 500})
    evs, br = collect(lambda e: e.get('ev') == 'browser')
    collect(is_idle)
    return br


def near(a, b, tol=1e-5):
    """elementwise, for a value recomputed in double precision from JSON
    text against a value the core computed in float32: same_floats wants
    bit-identical floats, appropriate for the same stored value read two
    ways, not for redoing its arithmetic"""
    return len(a) == len(b) and all(abs(x - y) < tol for x, y in zip(a, b))


send(cmd='browser.add_column', name='VW', formula='v*w')
evs, _ = collect(is_idle)
st = last_state(evs)
check("addcol does not grow the model (still 2 ICs: V, W)", st is not None and len(st['ics']) == 2,
      str(st and st['ics']))

br = fetch_browser(601)
vi, wi, vwi = br['cols'].index('V'), br['cols'].index('W'), br['cols'].index('VW')
orig_cols = br['cols'][:-1]
check('the browser shows the added column, computed from the others, for every row',
      br['cols'][-1] == 'VW' and near([row[vwi] for row in br['data']],
                                       [row[vi] * row[wi] for row in br['data']]), str(br)[:200])
send(cmd='browser.export', target='table', format='csv', file='added_col.csv')
collect(is_idle)
with open(os.path.join(run, 'added_col.csv')) as f:
    csv_lines = [l.strip() for l in f if l.strip() and not l.startswith('#')]
check('the saved CSV has a VW column, one more than the model columns',
      csv_lines[0].split(',') == orig_cols + ['VW']
      and len(csv_lines[1].split(',')) == len(orig_cols) + 1, str(csv_lines[:2]))

# plot it by name (Xi vs t: graf_par.cpp find_variable resolves it)
send(cmd='events.subscribe', events=['series'])
collect(is_idle)
send(cmd='plot.variable_vs_time', variable='VW')
evs, _ = collect(is_idle)
ser = [e for e in evs if e.get('ev') == 'series']
check('Xi vs t can plot the added column by name',
      len(ser) == 1 and [c['name'] for c in ser[0]['columns']] == ['T', 'VW'], str(ser)[:200])
if ser:
    plotted = values(ser[0]['columns'][1], ser[0].get('enc'))
    expected = [f32(row[vwi]) for row in br['data']]
    check('the plotted series is exactly the added column', same_floats(plotted, expected), '')

# a re-run, with a changed parameter so V, W (and VW) differ, recomputes
# the added column instead of dropping it
send(cmd='value.set_parameters', values={'iapp': 0.2})
collect(is_idle)
send(cmd='run.initial')
evs, _ = collect(is_idle, timeout=30 * SLOW)
st = last_state(evs)
check('a re-run after addcol still has 601 rows and 2 ICs (the model unchanged)',
      st is not None and st['rows'] == 601 and len(st['ics']) == 2, str(st and (st['rows'], st['ics'])))
br2 = fetch_browser(601)
check("the re-run's browser columns are the model's plus VW again, recomputed",
      br2['cols'] == br['cols'] and near([row[vwi] for row in br2['data']],
                                          [row[vi] * row[wi] for row in br2['data']]), str(br2)[:200])
check('the added column actually changed after the re-run (recomputed, not stale)',
      not same_floats([row[vwi] for row in br['data']], [row[vwi] for row in br2['data']]), '')
send(cmd='events.unsubscribe', events=ALL_EVENTS)
collect(is_idle)

send(cmd='events.subscribe', events=['series'])
collect(is_idle)
send(cmd='value.slide', name='iapp', value=0.07)
evs, _ = collect(is_idle, timeout=30 * SLOW)
st = last_state(evs)
ser = whole_series(evs)
check('slide sets the parameter, no run any more (the rerun flag is gone, W69)',
      st is not None and dict(st['pars'])['iapp'] == 0.07 and not ser,
      str(st and st['pars']) + str(ser)[:200])
send(cmd='run.initial')
evs, _ = collect(is_idle, timeout=30 * SLOW)
ser = whole_series(evs)
check('a later Go runs with the slid value (a new series of 601 rows)',
      ser and ser[-1]['rows'] == 601, str(ser)[:200])
send(cmd='events.unsubscribe', events=ALL_EVENTS)
collect(is_idle)
send(cmd='session.get_source')
evs, eqs = collect(lambda e: e.get('ev') == 'equations')
collect(is_idle)
check('equations', eqs is not None and eqs['lines'][0].startswith('dV/dT='), str(eqs))
send(cmd='session.get_source')
evs, src = collect(lambda e: e.get('ev') == 'source')
collect(is_idle)
acts = [i for i, c in enumerate(src['comments']) if c[1]] if src else []
check('source lists the comments with actions', len(acts) >= 5, str(src and src['comments'][:3]))
if acts:
    send(cmd='tool.run_source_action', index=acts[1])  # {gk=0}
    evs, _ = collect(is_idle)
    st = last_state(evs)
    check('a comment action sets its parameters', st is not None and dict(st['pars'])['gk'] == 0, str(st and st['pars']))
    send(cmd='value.set_parameters', values={'gk': 2})
    send(cmd='value.set_parameters', values={'iapp': 0.1})
    collect(is_idle)
    collect(is_idle)

send(cmd='session.export_info', file='info.txt')
evs, _ = collect(is_idle)
check('simulation information is written by the registry command', os.path.exists(os.path.join(run,'info.txt')), str(evs)[-300:])
send(cmd='session.open_model')
evs, ask = collect(lambda e:e.get('ev') == 'ask')
check('W517: the model picker is a read file question', ask and ask.get('kind') == 'file' and ask.get('mode') == 'read', str(ask))
if ask:
    long_wild='*' * 100000
    send(cmd='answer', id=ask['id'], wild=long_wild)
    _, listed=collect(lambda e:e.get('ev') == 'ask')
    check('W121b: 100,000 wildcard stars list without recursion', listed and listed.get('wild') == long_wild and 'lecar.odex' in listed.get('files',[]), str(listed)[:200])
    if listed:send(cmd='answer',id=listed['id'],ok=0)
collect(is_idle)

# W88: a file ask answered with a full path outside the model's folder (what
# the desktop window's own dialog answers) is read and written right there,
# the path whole, however long (Windows' fopen stops at MAX_PATH, so shorter
# there)
elsewhere = tempfile.mkdtemp(prefix='xppelsewhere')
far = os.path.join(elsewhere, 'd' * (40 if os.name == 'nt' else 200))
os.makedirs(far)
far_set = os.path.join(far, 'w88 far.setx')
send(cmd='value.set_parameters', values={'iapp': 0.33})
collect(is_idle)
send(cmd='session.save_as', file='w88', data=False)
collect(is_idle)
# the settings file is the session's model.setx
with zipfile.ZipFile(os.path.join(run, 'w88.snapx')) as z:
    with open(far_set, 'wb') as f:
        f.write(z.read('model.setx'))
send(cmd='value.set_parameters', values={'iapp': 0.77})
collect(is_idle)
with open(far_set,'rb') as f: uploaded=f.read()
send(cmd='file',op='put',name='w88-selected.setx',data=base64.b64encode(uploaded).decode())
collect(is_idle)
send(cmd='session.import_settings',file='w88-selected.setx')
evs,_=collect(is_idle)
st=last_state(evs)
check('W517: a selected external settings file is imported through its uploaded basename',
      st is not None and dict(st['pars'])['iapp'] == 0.33, str(st and st['pars']))
shutil.rmtree(elsewhere,ignore_errors=True)
transpose=next(c for c in hello['commands'] if c['cmd']=='browser.transpose')
check('T32: Transpose arguments retain their name and integer types',
      [a['type'] for a in transpose['arguments']] == ['name','integer','integer','integer','integer','integer'],str(transpose['arguments']))


# The model's folder for a client that cannot reach it (docs/protocol.md
# "Files"): the `file` command puts, gets and lists files there, base names
# only, as the /files endpoints of browser mode do (tools/webcheck.py).
def file_cmd(**kw):
    send(cmd='file', **kw)
    evs, ev = collect(lambda e: e.get('ev') == 'file')
    collect(is_idle)
    return ev or {}


long_op = 'list' + 'x' * 40
long_reply = file_cmd(op=long_op)
check('W121b: file operation is returned whole, never cut', long_reply.get('op') == long_op and long_reply.get('ok') == 0, str(long_reply))
long_name = 'missing' + 'n' * 400
send(cmd='set.apply', name=long_name, scope='all')
long_events, _ = collect(is_idle)
check('W121b: names over 300 bytes reach the handler whole', any(e.get('ev') in ('message', 'done') and long_name in e.get('error', '') for e in long_events), str(long_events)[:500])

blob = bytes(range(256)) * 4 + b'\x00\r\n\x1a'
before = sorted(os.listdir(run))
outside = os.path.join(os.path.dirname(run), 'x')  # where ../x would land
stat_of = lambda p: (os.stat(p).st_mtime_ns, os.stat(p).st_size) if os.path.exists(p) else None
above = stat_of(outside)
ev = file_cmd(op='put', name='srv.bin', data=base64.b64encode(blob).decode())
with open(os.path.join(run, 'srv.bin'), 'rb') as f:
    on_disk = f.read()
check('file put writes the bytes into the model\'s folder', ev.get('ok') == 1 and ev.get('name') == 'srv.bin'
      and ev.get('size') == len(blob) and ev.get('sha256') == hashlib.sha256(blob).hexdigest() and on_disk == blob, str(ev))
ev = file_cmd(op='get', name='srv.bin')
check('file get gives them back', ev.get('ok') == 1 and base64.b64decode(ev.get('data', '')) == blob
      and ev.get('sha256') == hashlib.sha256(blob).hexdigest(), str(ev)[:200])
ev = file_cmd(op='list')
names = [f['name'] for f in ev.get('files', [])]
check('file list names the folder\'s files with their digests', ev.get('ok') == 1 and 'srv.bin' in names
      and 'lecar.odex' in names and all(len(f['sha256']) == 64 for f in ev['files']), str(ev)[:300])
refused = {n: file_cmd(op='put', name=n, data='eA==').get('ok') for n in ['../x', 'a/b', 'a\\b', '.hidden', '..', 'C:x', '']}
refused['a\\u0000b'] = file_cmd(op='put', name='a\x00b', data='eA==').get('ok')
refused['get ../lecar.odex'] = file_cmd(op='get', name='../' + os.path.basename(run) + '/lecar.odex').get('ok')
check('file put and get refuse anything but a base name', all(v == 0 for v in refused.values()), str(refused))
ev = file_cmd(op='put', name='../escape', data='AA==')
check('W121b: refused upload keeps its name and all error-place fields',
      ev.get('ok') == 0 and ev.get('name') == '../escape' and ev.get('file') == '../escape'
      and ev.get('line') == 0 and ev.get('col') == 0 and ev.get('source') == ''
      and ev.get('field') == '' and bool(ev.get('error'))
      and not os.path.exists(os.path.join(run, '../escape')), str(ev))
ev = file_cmd(op='put', name='bad.bin', data='not base64!')
check('file put refuses data that is not base64', ev.get('ok') == 0 and 'base64' in ev.get('error', ''), str(ev))
ev = file_cmd(op='get', name='none.bin')
check('file get of a missing file says so', ev.get('ok') == 0 and ev.get('error'), str(ev))
check('refused file commands leave nothing behind', sorted(os.listdir(run)) == sorted(before + ['srv.bin'])
      and stat_of(outside) == above, str(sorted(os.listdir(run))))

# UTF-8 file names (card W35b, MI-001/WF-002): "mu settings.set" ('μ',
# a Greek mu) written, listed and read back with its real name, not the
# mojibake ('Î¼...') the old byte-as-Latin-1 JSON string
# reader/writer produced -- and, on Windows, not mangled by the ANSI code
# page either (assets/xppautx.manifest's activeCodePage UTF-8).
utf8_name = 'μ settings.set'
ev = file_cmd(op='put', name=utf8_name, data=base64.b64encode(b'hi').decode())
check('file put takes a UTF-8 name and writes that file', ev.get('ok') == 1 and ev.get('name') == utf8_name
      and os.path.exists(os.path.join(run, utf8_name)), str(ev))
ev = file_cmd(op='list')
names = [f['name'] for f in ev.get('files', [])]
check('file list gives the UTF-8 name back whole', utf8_name in names, str(names))
ev = file_cmd(op='get', name=utf8_name)
check('file get of the UTF-8 name gives its bytes back', ev.get('ok') == 1 and base64.b64decode(ev.get('data', '')) == b'hi', str(ev))

send(cmd='run.configure',total_time=40)
collect(is_idle)
poincare=next(c for c in hello['commands'] if c['cmd']=='analysis.poincare_map')
check('W485: Poincare arguments declare a real section, named direction and boolean stop',
      {a['name']:a['type'] for a in poincare['arguments']} == {'type':'choice','variable':'name','section':'real','direction':'choice','stop_on_section':'boolean'}, str(poincare['arguments']))
send(cmd='plot.set_2d',x_max=40)
collect(is_idle)
send(cmd='session.get_state')
evs,_=collect(is_idle)
before_axes=last_state(evs)['view']
send(cmd='plot.set_2d',x_label='changed',y_min=1e9)
evs,_=collect(is_idle)
errors=[e for e in evs if e.get('error')]
check('W225: invalid axes have one placed error and apply nothing',len(errors)==1 and 'y_min' in errors[0]['error'] and last_state(evs)['view']==before_axes,str(evs)[-500:])

# T31: an integer field (run.range's steps) and a name (Xi vs t: T or a variable, hello.lists[0])
range_command = next((c for c in hello.get('commands', []) if c.get('cmd') == 'run.range'), {})
types = {a['name']: a['type'] for a in range_command.get('arguments', [])}
check('T31: run.range says steps is an integer, start and end numbers and its options booleans',
      types.get('steps') == 'integer' and types.get('start') == 'real' and types.get('end') == 'real'
      and types.get('variable') == 'name' and [types[k] for k in ('reset_storage', 'use_old_ic', 'cycle_color', 'movie', 'write_each_run_to_file')] == ['boolean'] * 5,
      str(types))
variable_time=next(c for c in hello['commands'] if c['cmd']=='plot.variable_vs_time')
check('T31: variable versus time takes a model column name',any(a['name']=='variable' and a['type']=='name' and a['names']=='column' for a in variable_time['arguments']),str(variable_time['arguments']))

send(cmd='run.initial')
evs, _ = collect(is_idle, timeout=30 * SLOW)
st = last_state(evs)
check('total 40 gives 801 rows', st is not None and st['rows'] == 801, str(st and st['rows']))
check('a run that is not cancelled sends no stopped', not any(e.get('ev') == 'stopped' for e in evs))

# An Abort right behind the answer that starts the run: the run stops, and
# says where (docs/protocol.md "stopped") before its state and idle. Armed
# by row count ahead of time (xpp::job::stop_at_rows, the same mechanism
# .recx --silent's own abort replay uses: docs/protocol.md "Playing a recording without an interface"), in a
# one-shot .recx --silent subprocess of its own, not raced against the clock in
# the middle of this session: how many rows are kept is known in advance,
# not discovered by luck.
def run_replay(lines, ode=None):
    """A .recx played silently for its exit code, and through the same
    player over --server to inspect its events (xppclient owns both)."""
    ode = ode or args.ode
    with tempfile.TemporaryDirectory(prefix='xppreplay') as run:
        path = make_recording(args.server, ode, lines, run)
        with open(path, encoding='utf-8') as f:
            run_replay.places = [(i, l.rstrip()) for i, l in enumerate(f, 1) if l.startswith('{')]
        r = replay_recording(args.server, ode, path, run)
        return r.returncode, r.stdout, r.stderr

STOP_ROWS = 400  # well inside total 40's 801 rows
code, out, err = run_replay([
    {'cmd': 'run.configure', 'total_time': 40},
    {'cmd': 'run.initial'},
    {'cmd': 'run.abort', 'at': {'what': 'integrate', 'rows': STOP_ROWS, 't': 0}},
    {'cmd': 'run.initial'},
])
check('xppautX .recx --silent plays the armed interruption', code == 0, 'exit %d, %s' % (code, err[-300:]))
evs = [json.loads(l) for l in out.splitlines() if l.strip()]
kinds = [e.get('ev') for e in evs]
stopped = [e for e in evs if e.get('ev') == 'stopped']
# every command ends with state then idle (docs/protocol.md), so the whole
# session's transcript has one state per idle, not just the run's: the row
# count that matters is the state right before each idle, in idle order
rows_at_idle, last_rows = [], None
for e in evs:
    if e.get('ev') == 'state':
        last_rows = e.get('rows')
    elif e.get('ev') in ('idle','done'):
        rows_at_idle.append(last_rows)
check('a cancelled integration sends stopped, then state and idle',
      len(stopped) == 1 and kinds.index('stopped') < len(kinds) - 1 - kinds[::-1].index('state'), str(kinds[-8:]))
at = stopped[0]['at'] if stopped else {}
check('stopped says exactly how many rows the armed stop kept, and the last time',
      at.get('what') == 'integrate' and at.get('rows') == STOP_ROWS and isinstance(at.get('t'), (int, float)),
      str(at))
check('state at the interrupted run\'s idle agrees with the armed row count',
      len(rows_at_idle) >= 2 and rows_at_idle[-2] == STOP_ROWS, str(rows_at_idle[-2:]))
check('the next run is whole again', len(rows_at_idle) >= 1 and rows_at_idle[-1] == 801 and len(stopped) == 1,
      str(rows_at_idle[-1:]))

send(cmd='equilibrium.find')
evs, _ = collect(is_idle)
eq = next((e for e in evs if e.get('ev') == 'equilibrium'), None)
check('Sing pts/Go reports an equilibrium', eq is not None and eq['type'] in ('STABLE', 'UNSTABLE', 'NEUTRAL'), str(eq))
ev = (eq or {}).get('eigenvalues') or []
check('with its eigenvalues, one [re, im] per variable, as its counts say',
      len(ev) == len(eq['values']) and sum(1 for r, i in ev if r < 0) == eq['cminus'] + eq['rminus'], str(ev))
collect(is_idle)

send(cmd='window.create')
evs, _ = collect(lambda e: e.get('ev') == 'window' and e['op'] == 'create', timeout=5 * SLOW)
check('Makewindow/Create opens window 2', any(e.get('ev') == 'window' and e.get('win') == 'graph_2' for e in evs))
collect(is_idle)


def answer_asks(until, replies, timeout=20 * SLOW, sender=send, collector=collect):
    """collect up to until(ev), answering asks whose kind is in replies"""
    got = []
    while True:
        evs, e = collector(lambda e: e.get('ev') == 'ask' or until(e), timeout)
        got += evs
        if e is None or e.get('ev') != 'ask' or e['kind'] not in replies:
            return got, e
        sender(cmd='answer', id=e['id'], **replies[e['kind']](e))


pixels = lambda e: {'w': 2, 'h': 1, 'rgb': base64.b64encode(bytes([255, 0, 0, 0, 0, 255])).decode()}
menu = lambda key: (lambda e: {'key': key})
for _ in range(2):
    send(cmd='kinescope.capture')
    evs, _ = collect(lambda e:e.get('ev')=='film')
    collect(is_idle)
check('Kinescope/Capture sends film captures', evs and evs[-1].get('count') == 2, str(evs[-1:]))
send(cmd='kinescope.export_frames',file='kin_0.gif')
evs, e = answer_asks(is_idle, {'pixels':pixels})
check('Kinescope/Save asks for the pixels and writes GIFs, the second named from the first',
      all(os.path.exists(os.path.join(run, n)) for n in ('kin_0.gif', 'kin_0-frame1.gif')), str(os.listdir(run)))
send(cmd='animation.open')
evs, _ = collect(is_idle)
check('the animation window opens', any(e.get('ev') == 'ani' and e.get('open') == 1 for e in evs) or any(e.get('ev') == 'window' and e.get('win') == 'animation'
      and e.get('op') == 'create' for e in evs), str([e for e in evs if e.get('ev') == 'window']))
send(cmd='window.hide', window='animation')
evs, _ = collect(is_idle)
check('window.hide hides the animation window and keeps it',
      any(e.get('ev') == 'done' and e.get('status') == 'ok' and e['result'] == {'window': 'animation', 'shown': False} for e in evs), str(evs[-3:]))
send(cmd='window.list')
evs, _ = collect(is_idle)
check('window.list shows the hidden animation', any({'name': 'animation', 'kind': 'animation', 'shown': False} in e.get('result', {}).get('windows', [])
      for e in evs if e.get('ev') == 'done'), str(evs[-3:]))
send(cmd='window.show', window='animation')
collect(is_idle)
send(cmd='window.destroy', window='animation')
evs, _ = collect(is_idle)
check('the animation window closes', any(e.get('ev') == 'window' and e.get('op') == 'destroy' for e in evs))

send(cmd='plot.set_variables', how='array_plot', names=['V', 'W'])
evs, _ = collect(is_idle)
ap = [e for e in evs if e.get('ev') == 'aplot']
check('the IC arry button opens an array plot, its colour scale a ramp of 256 colours (W492)',
      ap and ap[-1]['nx'] == 2 and 'cells' not in ap[-1] and ap[-1]['colormap'] == 'jet' and len(ap[-1]['ramp']) == 256
      and ap[-1]['ramp'][0] == '#000081' and ap[-1]['ramp'][255] == '#fe2600', str(ap[-1]['ramp'][-3:]))

# docs/ui-v2.md T12: `values`, the cells' numbers before XPP maps them to a
# colour, equal the browser's own numbers for the same rows and columns.
# optimize_aplot (core/graf_par.cpp) always starts at row 0 with ColSkip 1 and
# picks RowSkip so `ny` rows span the run (ndown = min(201, nrows), nskip =
# nrows // ndown, both integer division as the core computes them); the
# columns are V, W (set_variables picked them, in that order).
send(cmd='session.get_state')
evs, st = collect(is_state)
collect(is_idle)
nrows = st['rows']
send(cmd='browser.page', **{'from_row': 0, 'count': nrows, 'first_column': 1, 'columns': 500})
evs, br = collect(lambda e: e.get('ev') == 'browser')
collect(is_idle)
iv, iw = br['cols'].index('V'), br['cols'].index('W')
nskip = max(1, nrows // ap[-1]['ny'])
want = []
for j in range(ap[-1]['ny']):
    row = br['data'][nskip * j]
    want += [f32(row[iv]), f32(row[iw])]
got = values({'data': ap[-1]['values']}, ap[-1].get('enc'))
check('aplot values equal the browser numbers for the same rows and columns',
      same_floats(got, want), '%s != %s' % (got[:6], want[:6]))

# the same, base64 float32, when the client last asked for it (reused from
# the "data" subscription's own "enc", core/plot_data_want_f32)
send(cmd='events.unsubscribe', events=ALL_EVENTS)
collect(is_idle)
send(cmd='events.subscribe', events=[], encoding='f32')
collect(is_idle)
send(cmd='array_plot.redraw')
evs, _ = collect(is_idle)
ap2 = [e for e in evs if e.get('ev') == 'aplot']
check('aplot values as base64 float32 when the client asked for it',
      ap2 and ap2[-1].get('enc') == 'f32' and isinstance(ap2[-1]['values'], str)
      and same_floats(values({'data': ap2[-1]['values']}, 'f32'), got), str(ap2[-1:])[:200])
send(cmd='events.unsubscribe', events=ALL_EVENTS)
collect(is_idle)

array_range=next(c for c in hello['commands'] if c['cmd']=='array_plot.range')
check('T32: array range arguments retain named booleans and tags',any(a['name']=='tag' and a['type']=='boolean' for a in array_range['arguments']),str(array_range['arguments']))

send(cmd='window.destroy', window='array_plot_1')
collect(is_idle)
send(cmd='window.select', window='graph_1')
evs, _ = collect(is_idle)
send(cmd='session.get_state')
evs, st = collect(is_state)
collect(is_idle)
view = st and st.get('view')
check('state has the view of the active window', view is not None and view['right'] > view['left'], str(view))
send(cmd='window.scroll',dx=-0.1,dy=0)
evs,_=collect(is_idle)
st=last_state(evs)
check('Window/Scroll moves the view through the registry',st is not None and st['view']['xlo']<view['xlo'],str(st and st['view']))

send(cmd='auto.open')
evs, _ = collect(lambda e: e.get('ev') == 'window' and e.get('win') == 'diagram_1')
check('File/Auto opens the AUTO window', any(e.get('ev') == 'window' and e.get('win') == 'diagram_1' for e in evs))
collect(is_idle)
send(cmd='auto.redraw')
evs, _ = collect(is_idle)
axes = [e for e in evs if e.get('ev') == 'diagram' and e['op'] in ('axes', 'reset')]
check('the AUTO diagram sends its axes as data', axes and axes[-1]['wid'] > 0 and axes[-1]['xlabel'], str(axes))

# AUTO restart still reads a labelled solution through the registry.
send(cmd='auto.run',kind='equilibrium')
evs,_=collect(is_idle,timeout=60*SLOW)
check('W517: AUTO starts without a menu ask',not any(e.get('ev')=='ask' for e in evs) and not any(e.get('error') for e in evs),str(evs)[-500:])
send(cmd='auto.grab',label=1)
evs,_=collect(is_idle)
check('AUTO: a labelled solution is grabbed by name',not any(e.get('error') for e in evs),str(evs)[-300:])
send(cmd='auto.run',kind='equilibrium',label=1)
evs,_=collect(is_idle,timeout=60*SLOW)
check('AUTO: restarting from a labelled solution stays clean',not any(e.get('error') for e in evs),str(evs)[-500:])

# Live plotting (docs/protocol.md "The plot as data"): while an integration
# runs, a subscribed client gets the rows as they are stored, in "append"
# series events, then the full series. tools/models/live.odex stores 20 001
# rows in about a second, long enough for several appends.
LIVE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'live.odex')


def live_run(send, collect, command=None):
    """integrate (run.initial, or the `command` given, as Continue) and return the series events of the command"""
    send(**(command or {'cmd': 'run.initial'}))
    evs, _ = collect(is_idle, timeout=120 * SLOW)
    return [e for e in evs if e.get('ev') == 'series']


def check_appends(what, ser, first_from, enc=None):
    """appends, then the end (docs/protocol.md "Live runs"); returns the whole
    series the client then holds (SeriesMirror), its columns as floats"""
    apps = [e for e in ser if e.get('op') == 'append']
    ends = [e for e in ser if e.get('op') == 'end']
    check('%s: several appends before the end' % what, len(apps) >= 3, '%d appends' % len(apps))
    check('%s: one end, nothing after it, and no full series' % what,
          len(ends) == 1 and ser and ser[-1] is ends[0] and len(apps) + 1 == len(ser),
          str([(e.get('op'), e.get('rows')) for e in ser])[:300])
    if not apps or not ends:
        return None
    froms = [e['from'] for e in apps]
    contiguous = froms[0] == first_from and all(apps[i]['from'] == apps[i - 1]['rows'] for i in range(1, len(apps)))
    sizes = all(len(values(c, enc)) == e['rows'] - e['from'] for e in apps for c in e['columns'])
    check('%s: the appends go from row %d on, contiguous' % (what, first_from), contiguous and sizes,
          str([(e['from'], e['rows']) for e in apps])[:300])
    check('%s: in the encoding asked for' % what, all(e.get('enc') == enc for e in apps),
          str([e.get('enc') for e in apps]))
    held = ends[0].get('held')
    check('%s: the end has the rows the appends gave, and the data version' % what,
          held is not None and ends[0]['rows'] == apps[-1]['rows'] == held['rows'] and isinstance(ends[0].get('version'), int)
          and all(len(c['data']) == held['rows'] for c in held['columns']), str(ends[0]))
    return {c['col']: c['data'] for c in held['columns']} if held else None


def check_live_series():
    proc3, run3, send3, collect3, _ = launch_server(ode=LIVE)
    try:
        collect3(is_idle)
        send3(cmd='events.subscribe', events=['series'])
        collect3(is_idle)
        ser = live_run(send3, collect3)
        final = check_appends('live run', ser, 0)
        full = whole_series(ser)
        if full:
            check('live run: the whole series (appends, end) has the 20 001 rows of output.dat',
                  full[0]['rows'] == 20001 and series_matches_output_dat(full[0], LIVE) is None,
                  str(full[0]['rows']) + ' ' + str(series_matches_output_dat(full[0], LIVE)))
        ser = live_run(send3, collect3)
        again = check_appends('run again', ser, 0)
        check('run again: the same numbers', final is not None and again is not None
              and all(same_floats(final[c], again[c]) for c in final))
        ser = live_run(send3, collect3, command={'cmd': 'run.continue', 'until': 2000})
        more = check_appends('Continue', ser, 20001)
        check('Continue: the rows before it are kept', more is not None and final is not None
              and all(same_floats(more[c][:20001], final[c]) for c in final))

        send3(cmd='events.subscribe', events=['series'], encoding='f32')
        evs, _ = collect3(is_idle)
        now = [e for e in evs if e.get('ev') == 'series']
        check('enc f32: the series comes at once, base64 in each column',
              len(now) == 1 and now[0].get('enc') == 'f32' and all(isinstance(c['data'], str) for c in now[0]['columns']),
              str(now)[:200])
        if now and more is not None:
            check('enc f32: the decoded columns equal the JSON ones',
                  all(same_floats(values(c, 'f32'), more[c['col']]) for c in now[0]['columns']))
        ser = live_run(send3, collect3)
        dec = check_appends('enc f32 run', ser, 0, 'f32')
        check('enc f32 run: the decoded numbers equal the JSON run\'s', dec is not None and final is not None
              and all(same_floats(dec[c], final[c]) for c in final))

        send3(cmd='events.unsubscribe', events=ALL_EVENTS)
        collect3(is_idle)
        ser = live_run(send3, collect3)
        check('a client not subscribed gets no series, appended or full', ser == [], str(ser)[:200])
    finally:
        send3(cmd='session.quit', save=False)
        try:
            proc3.wait(timeout=5 * SLOW)
        except subprocess.TimeoutExpired:
            proc3.kill()
        shutil.rmtree(run3, ignore_errors=True)


check_live_series()


# Answers in data coordinates (docs/protocol.md "Asks"): a mouse, rubber or
# drag ask also takes xd,yd (xd2,yd2), which the core turns into the pixels
# of its window that map to them. Two fresh servers zoom by the same box,
# one answered in pixels, the other in the data coordinates of those pixels
# (state.view's mapping): the views must be the same.
def zoomed_view(answer_of):
    """a fresh lecar server's view after Window/Zoom answered with answer_of(view), and the server's session"""
    p, r, snd, col, _ = launch_server()
    col(is_idle)
    snd(cmd='session.get_state')
    evs, st = col(is_state)
    col(is_idle)
    view = st['view']
    answer = answer_of(view)
    if 'xd' in answer:
        x, y, x2, y2 = (answer[k] for k in ('xd', 'yd', 'xd2', 'yd2'))
    else:
        x, y = pixel_to_data(view, answer['x'], answer['y'])
        x2, y2 = pixel_to_data(view, answer['x2'], answer['y2'])
    snd(cmd='window.set_bounds', x_min=min(x,x2), x_max=max(x,x2), y_min=min(y,y2), y_max=max(y,y2))
    evs, _ = col(is_idle)
    ok = not any('error' in e for e in evs)
    st = last_state(evs)
    return ok, view, st and st['view'], (p, r, snd, col)


def pixel_to_data(v, i, j):
    return (v['xlo'] + (v['xhi'] - v['xlo']) * (i - v['left']) / (v['right'] - v['left']),
            v['ylo'] + (v['yhi'] - v['ylo']) * (j - v['bottom']) / (v['top'] - v['bottom']))


def check_stable_navigation():
    server=Server(args.server,args.ode)
    try:
        server.collect(is_idle)
        server.send(cmd='keymap.set',bindings={})
        server.collect(is_idle)
        server.send(cmd='key',key='Ctrl+R')
        evs,_=server.collect(is_idle)
        check('W517: the default key opens no menu ask',not any(e.get('ev')=='ask' for e in evs),str(evs)[-300:])
        check('W517: the key starts integration',any(e.get('ev')=='computing' for e in evs),str(evs)[-300:])
        server.send(cmd='keymap.reset');server.collect(is_idle)
        for command in [dict(cmd='key',menu='main',item='undo'),dict(cmd='key',win='auto',key='r')]:
            server.send(**command);evs,_=server.collect(is_idle)
            check('W517: obsolete key layer is refused with one placed error',len([e for e in evs if e.get('error')])==1 and not any(e.get('ev')=='ask' for e in evs),str(evs)[-300:])
    finally:server.close()


check_stable_navigation()


def check_continue():
    """W213: one Continue: `continue` takes a duration or an end time, rounds the end up to the output
    grid (Dt times Store every N steps), stores at that stride, and the old `C` prompt is gone"""
    p, r, snd, col, _ = launch_server()

    def command(**cmd):
        snd(**cmd)
        evs, _ = col(is_idle)
        return evs, next((e for e in reversed(evs) if e.get('ev') == 'state'), {})
    try:
        col(is_idle)
        snd(cmd='run.initial')
        col(is_idle)
        _, st = command(cmd='session.get_state')
        t0, rows0 = st['time'], st['rows']
        evs, st = command(cmd='run.continue', extra=1)
        check('W213: continue extra appends a Dt grid of rows from the core time',
              abs(st['time'] - (t0 + 1)) < 1e-8 and st['rows'] == rows0 + 20 and not any(e.get('ev') == 'ask' for e in evs), str(st.get('time')))
        t1, rows1 = st['time'], st['rows']
        evs, st = command(cmd='run.continue', until=t1 + .02)
        check('W213: continue until off the grid is rounded up to the next Dt and the state reports the end',
              abs(st['time'] - (t1 + .05)) < 1e-8 and st['rows'] == rows1 + 1, str(st.get('time')))
        t2, rows2 = st['time'], st['rows']
        evs, _ = command(cmd='run.configure', store_every=3)
        evs, st = command(cmd='run.continue', extra=1)
        check('W213: with Store every 3 the end is rounded up to the output grid (7 groups of 3 Dt) and every third row is stored',
              abs(st['time'] - (t2 + 1.05)) < 1e-8 and st['rows'] == rows2 + 7, '%s %s' % (st.get('time'), st.get('rows')))
        t3, rows3 = st['time'], st['rows']
        evs, st = command(cmd='run.continue', until=t3 + .2)
        check('W213: continue until at that stride ends on an output grid point',
              abs(st['time'] - (t3 + .3)) < 1e-8 and st['rows'] == rows3 + 2, '%s %s' % (st.get('time'), st.get('rows')))
        t4, rows4 = st['time'], st['rows']
        evs, st = command(cmd='run.continue')
        check('W213: run.continue with no value asks nothing (the legacy prompt is gone) and continues for another Total',
              not any(e.get('ev') == 'ask' for e in evs) and abs(st['time'] - (t4 + 30)) < 1e-8 and st['rows'] == rows4 + 200, '%s %s' % (st.get('time'), st.get('rows')))
        t5 = st['time']
        for bad in (dict(extra=1, until=t5 + 1), dict(extra='1'), dict(until=t5 - 1), dict(extra=0)):
            evs, st = command(cmd='run.continue', **bad)
            check('W213: continue %r is an error and changes nothing' % (bad,),
                  any(e.get('ev') in ('message', 'done') and e.get('error') for e in evs) and st.get('time') == t5, str(evs[-3:]))
    finally:
        stop_server(p, r, snd)


check_continue()


def box_of(v):
    """a box from 20% to 60% of the window across and 30% to 80% down, in whole pixels"""
    w, h = v['right'] - v['left'], v['bottom'] - v['top']
    return (v['left'] + int(0.2 * w), v['top'] + int(0.3 * h), v['left'] + int(0.6 * w), v['top'] + int(0.8 * h))


def check_data_coordinates():
    ok1, v1, by_pixels, s1 = zoomed_view(lambda v: dict(zip(('x', 'y', 'x2', 'y2'), box_of(v))))
    stop_server(*s1[:3])

    def data_box(v):
        i1, j1, i2, j2 = box_of(v)
        (xd, yd), (xd2, yd2) = pixel_to_data(v, i1, j1), pixel_to_data(v, i2, j2)
        return {'xd': xd, 'yd': yd, 'xd2': xd2, 'yd2': yd2}
    ok2, v2, by_data, s2 = zoomed_view(data_box)
    p, r, snd, col = s2
    try:
        check('Window/Zoom asks for a box (rubber)', ok1 and ok2)
        keys = ('xlo', 'xhi', 'ylo', 'yhi')
        check('a box answered in data coordinates zooms as the same box in pixels',
              by_pixels is not None and by_data is not None and v1 == v2
              and all(by_pixels[k] == by_data[k] for k in keys) and by_data['xhi'] - by_data['xlo'] < v2['xhi'] - v2['xlo'],
              '%s vs %s' % (by_pixels, by_data))
        want = data_box(v2)
        px = (v2['xhi'] - v2['xlo']) / (v2['right'] - v2['left'])
        py = (v2['yhi'] - v2['ylo']) / (v2['bottom'] - v2['top'])
        got = by_data or {}
        check('the zoomed view is the box asked for, within a pixel',
              by_data is not None and abs(min(got['xlo'], got['xhi']) - min(want['xd'], want['xd2'])) <= px
              and abs(max(got['xlo'], got['xhi']) - max(want['xd'], want['xd2'])) <= px
              and abs(min(got['ylo'], got['yhi']) - min(want['yd'], want['yd2'])) <= py
              and abs(max(got['ylo'], got['yhi']) - max(want['yd'], want['yd2'])) <= py, '%s vs %s' % (got, want))
        # ic.at in data coordinates: the ICs are that point
        if by_data:
            v = by_data
            xd, yd = v['xlo'] + 0.3 * (v['xhi'] - v['xlo']), v['ylo'] + 0.6 * (v['yhi'] - v['ylo'])
            snd(cmd='ic.at', coordinates={'V': xd, 'W': yd})
            evs, _ = col(is_idle, timeout=30 * SLOW)
            st = last_state(evs)
            ics = dict(st['ics']) if st else {}
            pxm = abs(v['xhi'] - v['xlo']) / (v['right'] - v['left'])
            pym = abs(v['yhi'] - v['ylo']) / (v['bottom'] - v['top'])
            check('ic.at in data coordinates starts from that point, within a pixel',
                  abs(ics.get('V', 1e9) - xd) <= pxm
                  and abs(ics.get('W', 1e9) - yd) <= pym, '%s vs %s' % (ics, (xd, yd)))
    finally:
        stop_server(p, r, snd)


check_data_coordinates()


# Plot windows as data (docs/protocol.md "The plot as data", docs/ui-v2.md
# T6): "plots" lists every window and the active one; "series" comes per
# window, each with its win, when that window's data or curves changed;
# appends only for the active window. live.odex runs long enough for appends.
def check_plot_windows():
    proc4, run4, send4, collect4, _ = launch_server(ode=LIVE)


    def after(cmd):
        send4(**cmd)
        return collect4(is_idle, timeout=30 * SLOW)[0]

    plots = lambda evs: [e for e in evs if e.get('ev') == 'plots']
    full = whole_series
    wins = lambda p: [w['win'] for w in p['windows']]
    try:
        evs, _ = collect4(is_idle)
        hello4 = next((e for e in evs if e.get('ev') == 'hello'), {})
        check('hello lists the plots feature', 'plots' in hello4.get('features', []), str(hello4.get('features')))
        evs = after({'cmd': 'events.subscribe', 'events': ['series', 'plots']})
        pl = plots(evs)
        w1 = pl[0]['windows'][0] if pl and pl[0]['windows'] else {}
        check('plots at once: window 1, active, its title, axes and curves',
              len(pl) == 1 and pl[0]['active'] == 'graph_1' and wins(pl[0]) == ['graph_1'] and w1.get('title') == 'graph_1'
              and w1.get('three') == 0 and w1.get('xlo') == -0.6 and w1.get('yhi') == 1.2
              and w1.get('curves') and w1['curves'][0]['x'] == 1 and 'theta' in w1 and 'zmax' in w1.get('box', {}),
              str(pl)[:400])
        check('plots comes before the series', evs.index(pl[0]) < [e.get('ev') for e in evs].index('series') if pl and full(evs) else False)
        after({'cmd': 'run.initial'})
        evs = after({'cmd':'window.create'})
        pl, ser = plots(evs), full(evs)
        check('Makewindow/Create: plots has windows 1 and 2, 2 active',
              len(pl) == 1 and wins(pl[0]) == ['graph_1', 'graph_2'] and pl[0]['active'] == 'graph_2', str(pl)[:300])
        check('the new window gets its own series, with the data', [e['win'] for e in ser] == ['graph_2']
              and ser[0]['rows'] == 20001, str([(e['win'], e['rows']) for e in ser]))
        evs = after({'cmd':'plot.variable_vs_time','variable':'V'})
        pl, ser = plots(evs), full(evs)
        cur = {w['win']: (w['curves'][0]['x'], w['curves'][0]['y']) for w in pl[0]['windows']} if pl else {}
        check('Xi vs t in window 2: its curves change, window 1 keeps its own',
              cur == {'graph_1': (1, 2), 'graph_2': (0, 1)} and [w['title'] for w in pl[0]['windows']] == ['graph_1', 'graph_2'],
              str(pl)[:400])
        check('... and only window 2 gets a new series', [e['win'] for e in ser] == ['graph_2']
              and [c['name'] for c in ser[0]['columns']] == ['T', 'V'], str([(e['win'], e['rows']) for e in ser]))
        evs = after({'cmd': 'window.select', 'window': 'graph_1'})
        pl = plots(evs)
        check('selecting window 1 (click) makes it active in plots, and sends no series',
              len(pl) == 1 and pl[0]['active'] == 'graph_1' and not full(evs), str(pl)[:200])
        evs = after({'cmd': 'run.initial'})
        ser = [e for e in evs if e.get('ev') == 'series']
        apps = [e for e in ser if e.get('op') == 'append']
        check('a run appends to the active window only', len(apps) >= 3 and all(e['win'] == 'graph_1' for e in apps),
              str([(e['win'], e.get('op')) for e in ser])[:300])
        check('... and ends with the whole series of each window, the active one first (its end, then the full series of window 2)',
              [e['win'] for e in full(evs)] == ['graph_1', 'graph_2'] and all(e['rows'] == 20001 for e in full(evs)),
              str([(e['win'], e['rows']) for e in full(evs)]))
        after({'cmd': 'window.select', 'window': 'graph_2'})
        evs = after({'cmd':'window.destroy'})
        pl = plots(evs)
        check('Makewindow/Destroy of window 2: plots has window 1 only, active',
              len(pl) == 1 and wins(pl[0]) == ['graph_1'] and pl[0]['active'] == 'graph_1' and not full(evs), str(pl)[:300])
        after({'cmd':'window.create'})
        evs = after({'cmd':'window.create'})
        check('two more windows', plots(evs) and wins(plots(evs)[0]) == ['graph_1', 'graph_2', 'graph_3'], str(plots(evs))[:200])
        evs = after({'cmd':'window.destroy_all'})
        pl = plots(evs)
        check('Makewindow/Kill all leaves window 1', len(pl) == 1 and wins(pl[0]) == ['graph_1'] and pl[0]['active'] == 'graph_1',
              str(pl)[:300])
        evs = after({'cmd':'window.create'})
        check('a window made again gets its series again', [e['win'] for e in full(evs)] == ['graph_2'],
              str([(e['win'], e['rows']) for e in full(evs)]))
        evs = after({'cmd': 'window.redraw'})
        check('a redraw sends neither plots nor series', not plots(evs) and not full(evs))
        after({'cmd': 'events.unsubscribe', 'events': ['plots']})
        evs = after({'cmd': 'events.subscribe', 'events': ['series']})
        check('without plots in the list: series only', not plots(evs) and [e['win'] for e in full(evs)] == ['graph_2', 'graph_1'],
              str([(e['ev'], e.get('win')) for e in evs if e.get('ev') in ('plots', 'series')]))
    finally:
        stop_server(proc4, run4, send4)


check_plot_windows()


def check_model_plot_windows():
    """W348: model-defined graphs use the same core setup in every mode."""
    model = os.path.join('tools', 'models', 'multiwin.odex')
    proc, run, send, collect, _ = launch_server(ode=model)
    try:
        collect(is_idle)
        send(cmd='events.subscribe', events=['plots', 'series'])
        events, _ = collect(is_idle)
        plots = next((e for e in events if e.get('ev') == 'plots'), {})
        windows = plots.get('windows', [])
        check('W348: model creates three graphs, with the first active',
              [w['win'] for w in windows] == ['graph_1', 'graph_2', 'graph_3'] and plots.get('active') == 'graph_1', str(plots)[:400])
        check('W348: secondary graphs keep their axes and bounds', len(windows) == 3
              and windows[1]['curves'][0]['x'] == 2 and windows[1]['curves'][0]['y'] == 1
              and [windows[1][k] for k in ('xlo', 'xhi', 'ylo', 'yhi')] == [-2, 3, -4, 5]
              and windows[2]['curves'][0]['x'] == 0 and windows[2]['curves'][0]['y'] == 2
              and [windows[2][k] for k in ('xlo', 'xhi', 'ylo', 'yhi')] == [0, 1, 0, 2], str(windows)[:400])
        send(cmd='run.initial')
        events, _ = collect(is_idle)
        series = whole_series(events)
        check('W348: every graph carries the unchanged silent trajectory', len(series) == 3
              and all(series_matches_output_dat(s, model) is None for s in series), str([s['win'] for s in series]))
    finally:
        stop_server(proc, run, send)
    with tempfile.TemporaryDirectory(prefix='xppmultiwin') as run:
        with open(model, encoding='utf-8') as source:
            text = source.read()
        outputs = []
        for multi in (True, False):
            with open(os.path.join(run, 'plot.odex'), 'w', encoding='utf-8') as source:
                source.write(text if multi else text.replace('multiwin: 1, ', ''))
            result = subprocess.run([os.path.abspath(args.server), 'plot.odex', '--silent'], cwd=run,
                                    capture_output=True, text=True, timeout=60 * SLOW)
            check('W348: silent model runs / ' + ('multi' if multi else 'single'),
                  result.returncode == 0, result.stderr)
            with open(os.path.join(run, 'output.dat'), 'rb') as data:
                outputs.append(data.read())
        check('W348: multi-window silent output is byte-identical to single-window output',
              outputs[0] == outputs[1])


check_model_plot_windows()


# The values panel and the run history of web2 (GitHub #18, #117): hello's
# defaults, state's now, a batched set (never a run of its own: the `rerun`
# flag is gone from set/slide/default and ignored if sent), the ICs from
# where the last run ended, the state at the start of a run, the series
# version, and the erase/redraw events of the Erase and Redraw commands only.
def check_param_set_cancel():
    """W101: a cancelled Param set menu is silent; Save of an edited parameter
    writes the file with no error"""
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='value.set_parameters', values={'iapp': 0.3})
        col(is_idle)
        snd(cmd='set.apply',scope='all')
        evs,_=col(is_idle)
        check('W517: an incomplete set form applies nothing',len([e for e in evs if e.get('error')])==1 and dict(last_state(evs)['pars'])['iapp']==0.3,str(evs)[-300:])
        snd(cmd='value.save_file', kind='parameters', file='w101.par')
        evs, _ = col(is_idle)
        with open(os.path.join(r, 'w101.par')) as f:
            txt = f.read()
        check('values write par with an edited value: file written, no error',
              '0.3  iapp' in txt and not [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e],
              txt[:80] + str(evs)[:150])
    finally:
        stop_server(p, r, snd)



check_param_set_cancel()


def answer_scope(col, snd):
    """Apply set asks its scope after the set: the first item, all (docs/protocol.md "Apply set");
    the events up to the command's idle"""
    evs, ask = col(lambda e: e.get('ev') == 'ask')
    check('Apply set asks the scope after the set, all first',
          ask is not None and ask['kind'] == 'choice' and ask['name'] == 'set_scope' and ask['choices'][0].endswith('all'), str(ask))
    if ask:
        snd(cmd='answer', id=ask['id'], key='a')
    more, _ = col(is_idle)
    return evs + more


def check_param_set_list():
    """Named sets stay reachable past the old letter and fixed-count limits."""
    with tempfile.TemporaryDirectory(prefix='w229sets') as folder:
        for count,selected in [(30,29),(30,3),(500,500)]:
            model=os.path.join(folder,'sets.ode')
            with open(model,'w') as f:f.write("par a=0\ninit x=0\nx'=-x\n"+''.join(f'set s{i} {{a={i}}}\n' for i in range(1,count+1))+'done\n')
            server=Server(args.server,model)
            try:
                server.collect(is_idle)
                server.send(cmd='set.apply',name=f's{selected}',scope='all')
                evs,_=server.collect(is_idle)
                check(f'W229: set s{selected} of {count} is reachable by name',dict(last_state(evs)['pars'])['a']==selected and not any(e.get('error') for e in evs),str(evs)[-300:])
            finally:server.close()


check_param_set_list()


def check_values_protocol():
    pv, rv, sndv, colv, _ = launch_server()

    def after(**cmd):
        sndv(**cmd)
        return colv(is_idle, timeout=30 * SLOW)[0]


    full = whole_series
    pics = lambda evs: [(e['ev'], e['win']) for e in evs if e.get('ev') in ('erase', 'redraw')]
    par = lambda st, n: next(v for k, v in st['pars'] if k.lower() == n)
    ic = lambda st, n: next(v for k, v in st['ics'] if k.lower() == n)
    try:
        evs, _ = colv(is_idle)
        hv = next((e for e in evs if e.get('ev') == 'hello'), {})
        st0 = last_state(evs)
        d = hv.get('defaults', {})
        check('hello: defaults, one per parameter and IC, equal to the values as loaded',
              st0 and len(d.get('pars', [])) == len(st0['pars']) and len(d.get('ics', [])) == len(st0['ics'])
              and d['pars'] == [v for _, v in st0['pars']] and d['ics'] == [v for _, v in st0['ics']], str(d)[:200])
        check('state: no now before any run', st0 and 'now' not in st0, str(st0)[:200])
        after(cmd='events.subscribe', events=['series'])
        evs = after(cmd='run.initial')
        st, ser = last_state(evs), full(evs)
        cols = {c['col']: c['data'] for c in ser[-1]['columns']} if ser else {}
        check('a run: the full series carries the data version',
              ser and isinstance(ser[-1].get('version'), int), str(ser and {k: v for k, v in ser[-1].items() if k != 'columns'})[:200])
        check('state: now is where the run ended (the last stored row)',
              st and len(st.get('now', [])) == len(st['ics']) and ser
              and all(abs(st['now'][i] - cols[i + 1][-1]) < 1e-6 for i in range(len(st['ics'])) if i + 1 in cols),
              str(st and st.get('now')))
        v0 = ser[-1]['version'] if ser else None

        # W69: the value commands never run anything themselves:
        # the values panel holds edits pending and sends them
        # right before the next command that computes.
        evs = after(cmd='value.set_parameters', values={'iapp': 0.1})
        evs += after(cmd='ic.set', values={'v': -0.2})
        st = last_state(evs)
        check('value.set_parameters and ic.set: both set, no run (W69)',
              st and abs(par(st, 'iapp') - 0.1) < 1e-12 and abs(ic(st, 'v') + 0.2) < 1e-12 and not full(evs),
              str(st and st['pars'][:3]))
        check('value.set_parameters sends no erase event', not pics(evs), str(pics(evs)))
        evs = after(cmd='run.initial')
        st, ser = last_state(evs), full(evs)
        v_col = ser[-1]['columns'][[c['col'] for c in ser[-1]['columns']].index(1)]['data'] if ser else []
        check('a later Go runs with the values the set left (no rerun needed)',
              st and abs(par(st, 'iapp') - 0.1) < 1e-12 and len(ser) == 1
              and ser[0]['rows'] == 601 and v_col and abs(v_col[0] - f32(-0.2)) < 1e-9 and ser[0]['version'] != v0,
              str(st and st['pars'][:3]) + str([(s['rows'], s.get('version')) for s in ser]))
        evs = after(cmd='value.set_by_formula', name='iapp', formula='nosuch+')
        check('value.set_by_formula with a bad formula: an error, and no run',
              any(e.get('ev') in ('message', 'done') and 'error' in e for e in evs) and not full(evs), str(evs)[:300])
        # W131: one number rule (all of the text), a set applies all or nothing,
        # its error names the field, and default is the core's own command
        errs = lambda evs: [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        st1 = last_state(after(cmd='session.get_state'))
        evs = after(cmd='value.set_parameters', values={'iapp': 'abc'})
        st = last_state(evs)
        check('value.set_parameters with text that is not a number: refused naming the argument, nothing changes',
              len(errs(evs)) == 1 and 'values.iapp' in errs(evs)[0]['error']
              and st and par(st, 'iapp') == par(st1, 'iapp'), str(errs(evs)) + str(st and st['pars'][:3]))
        evs = after(cmd='value.set_parameters', values={'iapp': '1O0'})
        check('value.set_parameters 1O0 (a letter O): refused, nothing changes', len(errs(evs)) == 1
              and par(last_state(evs), 'iapp') == par(st1, 'iapp'), str(errs(evs)))
        evs = after(cmd='value.slide', name='iapp', value='5x')
        check('value.slide with a value that is not all a number: refused, nothing changes', len(errs(evs)) == 1
              and par(last_state(evs), 'iapp') == par(st1, 'iapp'), str(errs(evs)))
        evs = after(cmd='value.set_parameters', values={'iapp': 0.31, 'gk': '%bad(', 'gca': 1.1})
        st = last_state(evs)
        check('value.set_parameters of three with the second bad: one error naming values.gk, nothing applied',
              len(errs(evs)) == 1 and 'values.gk' in errs(evs)[0]['error'] and st
              and par(st, 'iapp') == par(st1, 'iapp') and par(st, 'gca') == par(st1, 'gca') and par(st, 'gk') == par(st1, 'gk'),
              str(errs(evs)) + str(st and st['pars'][:3]))
        evs = after(cmd='set', values=[{'kind': 'num', 'name': 'nullcline_mesh', 'text': '50'},
                                       {'kind': 'num', 'name': 'nullcline_mesh', 'text': '1O0'}])
        check('a set with a bad numerics value applies none of it, its error names num:nullcline_mesh',
              len(errs(evs)) == 1 and errs(evs)[0].get('field') == 'num:nullcline_mesh', str(errs(evs)))
        evs = after(cmd='value.set_parameters', values={'nosuch': 1})
        check('value.set_parameters of a name the model does not have: refused naming the parameter',
              len(errs(evs)) == 1 and 'nosuch' in errs(evs)[0]['error'], str(errs(evs)))
        # W494: a range or an order broken by `set num` is refused with the setting, its value and the rule
        for name, text, words in (('min_step', '1e9', 'min_step (1e+09) must be at most max_step'),
                                  ('relative_tolerance', '0', 'must be a number above 0'),
                                  ('max_iterations', '0', 'must be at least 1')):
            evs = (after(cmd='equilibrium.configure', max_iterates=int(text)) if name == 'max_iterations'
                   else after(cmd='run.configure', **{name: float(text)}))
            check('W494: setting num %s=%s is refused naming %s' % (name, text, words),
                  len(errs(evs)) == 1 and words in errs(evs)[0].get('error', '') and (name == 'max_iterations' or errs(evs)[0].get('field') == 'num:' + name),
                  str(errs(evs)))
        evs = after(cmd='run.configure', total_time='1O0')
        check('run.configure with a total time that is not a number is refused, naming the argument',
              len(errs(evs)) == 1 and 'total_time' in errs(evs)[0]['error'], str(errs(evs)))
        evs = after(cmd='value.set_parameters', values={'iapp': 0.31, 'gca': 1.2})
        evs += after(cmd='ic.set', values={'v': -0.2})
        st = last_state(evs)
        check('a good value.set_parameters of two and ic.set apply, no error', not errs(evs) and st
              and abs(par(st, 'iapp') - 0.31) < 1e-12 and abs(ic(st, 'v') + 0.2) < 1e-12, str(errs(evs)) + str(st and st['pars'][:3]))
        evs = after(cmd='set.restore_defaults', scope='parameters')
        st = last_state(evs)
        check('default par after edits: every parameter back to the model file value, no error',
              not errs(evs) and st and [v for _, v in st['pars']] == d['pars'], str(errs(evs)) + str(st and st['pars'][:3]))
        after(cmd='ic.set', values={'v': -0.2})
        before = last_state(after(cmd='session.get_state'))
        evs = after(cmd='set', kind='ic', name='v', value=-0.5)
        st = last_state(evs)
        check('set no longer takes initial conditions (ic.set does): refused naming ic.set, the ICs stay, no run',
              len(errs(evs)) == 1 and 'ic.set' in errs(evs)[0]['error'] and errs(evs)[0].get('field') == 'ic:v'
              and st and st['ics'] == before['ics'] and not full(evs), str(errs(evs)) + str(st and st['ics']))
        after(cmd='ic.set', values={'v': -0.3})
        prev_now = last_state(after(cmd='session.get_state'))['now']
        evs = after(cmd='run.last')
        states = [i for i, e in enumerate(evs) if is_state(e)]
        sers = [i for i, e in enumerate(evs) if e.get('ev') == 'series']
        first = evs[states[0]] if states else None
        check('Initialconds/Last: a state at the start of the run already has the ICs from where the last one ended',
              first and sers and states[0] < sers[-1] and [v for _, v in first['ics']] == prev_now,
              str(first and first['ics']) + ' vs ' + str(prev_now))
        evs = after(cmd='set.restore_defaults', scope='parameters')
        st = last_state(evs)
        check('set.restore_defaults: the model values, no run (W69)',
              st and par(st, 'iapp') == d['pars'][[k.lower() for k, _ in st['pars']].index('iapp')] and not full(evs),
              str(st and st['pars'][:3]))
        evs = after(cmd='run.initial')
        check('a later Go runs with the default values the default command left',
              last_state(evs) and par(last_state(evs), 'iapp') == d['pars'][[k.lower() for k, _ in st['pars']].index('iapp')]
              and len(full(evs)) == 1, str(last_state(evs) and last_state(evs)['pars'][:3]))

        evs = after(cmd='value.slide', name='iapp', value=0.07)
        check('a slide sets the parameter, no run (the rerun flag is gone, W69)',
              not pics(evs) and not full(evs) and abs(par(last_state(evs), 'iapp') - 0.07) < 1e-12, str(pics(evs)))
        evs = after(cmd='run.initial')
        check('a later Go runs with the slid value, no erase event', not pics(evs) and len(full(evs)) == 1, str(pics(evs)))
        evs = after(cmd='window.clear')
        check('Erase: one erase event for the active window, no series', pics(evs) == [('erase', 'graph_1')] and not full(evs),
              str(pics(evs)))
        evs = after(cmd='window.redraw')
        check('Redraw: one redraw event for the active window, no series', pics(evs) == [('redraw', 'graph_1')] and not full(evs),
              str(pics(evs)))
        evs = after(cmd='window.redraw')
        check('window.redraw again: one redraw event, no erase', pics(evs) == [('redraw', 'graph_1')], str(pics(evs)))
    finally:
        stop_server(pv, rv, sndv)


check_values_protocol()


# W66 review: XPPAUT's --icfile/io_ic_file reads and writes exactly `node`
# values -- one per differential-equation variable, never the Markov
# chains -- so a Markov model's .ic file stays the file XPPAUT itself
# reads (kepler.odex: x1, x2 are node, z is a markov chain, not in the file).
def check_ic_file_markov():
    pm, rm, sndm, colm, _ = launch_server(ode='examples/ode/kepler.odex')
    ic = lambda st, n: next(v for k, v in st['ics'] if k.lower() == n)
    try:
        evs, _ = colm(is_idle)
        st0 = last_state(evs)
        # state.ics (the values panel's own list, docs/protocol.md `state`)
        # still has all 3 -- x1, x2 and the markov chain z, which the panel
        # does let a user edit -- unlike the .ic file below, which is
        # exactly XPPAUT's own node-only format.
        check('kepler.odex: state.ics has x1, x2 and the markov chain z (3)',
              st0 is not None and len(st0['ics']) == 3
              and [n.lower() for n, _ in st0['ics']] == ['x1', 'x2', 'z'],
              str(st0 and st0['ics']))

        sndm(cmd='value.save_file', kind='initial_conditions', file='kepler_markov.ic')
        colm(is_idle)
        with open(os.path.join(rm, 'kepler_markov.ic')) as f:
            lines = [l.strip() for l in f if l.strip()]
        check('values write ic on a markov model: exactly `node` lines (2), z excluded',
              lines == ['0.0001', '0.0001'], str(lines))

        sndm(cmd='ic.set', values={'x1': 0.5})
        colm(is_idle)
        sndm(cmd='value.load_file', kind='initial_conditions', file='kepler_markov.ic')
        evs, _ = colm(is_idle)
        st = last_state(evs)
        check('values read ic on a markov model restores the 2 node values (z untouched)',
              st is not None and ic(st, 'x1') == 0.0001 and ic(st, 'x2') == 0.0001 and ic(st, 'z') == ic(st0, 'z'),
              str(st and st['ics']))
    finally:
        stop_server(pm, rm, sndm)


check_ic_file_markov()


def check_error_places():
    """Every error event carries what failed and where (docs/protocol.md
    "Errors", W140): a values file with a line that does not read names
    the file, the line and the line as written; a file that cannot be read
    names the file with line 0 (the page offers to add it)."""
    pe, re_, snde, cole, _ = launch_server()
    try:
        cole(is_idle)
        with open(os.path.join(re_, 'w140bad.par'), 'w') as f:
            f.write('3   Number params\n1\n2\n3\n')
        snde(cmd='value.load_file', kind='parameters', file='w140bad.par')
        evs, _ = cole(is_idle, timeout=30 * SLOW)
        m = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
        check('an error event: a values file at its line names the file, the line and the line as written (W140)',
              os.path.basename(m.get('file', '')) == 'w140bad.par' and m.get('line') == 1 and m.get('col') == 0
              and m.get('source') == '3   Number params' and 'parameters' in m.get('error', ''), str(m))
        snde(cmd='value.load_file', kind='parameters', file='w140gone.par')
        evs, _ = cole(is_idle, timeout=30 * SLOW)
        m = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
        check('an error event: a file that cannot be read is named with line 0 (W140)',
              os.path.basename(m.get('file', '')) == 'w140gone.par' and m.get('line') == 0
              and m.get('source') == '' and m.get('error'), str(m))
        snde(cmd='nosuchcommand')
        evs, _ = cole(is_idle, timeout=30 * SLOW)
        m = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
        check('an error event: a live command error has its one-line source (W130)',
              m.get('file') == 'command' and m.get('line') == 1 and m.get('col') == 0 and json.loads(m.get('source', '{}')).get('cmd') == 'nosuchcommand'
              and 'nosuchcommand' in m.get('error', ''), str(m))
    finally:
        stop_server(pe, re_, snde)


check_error_places()


def check_error_places_of_each_kind():
    """W140b: every error names its file and line, by kind (docs/protocol.md
    "Errors"): a table file a model line names and that cannot be read, at
    that model line; a table file that ends too soon, at its own last line;
    a variable an equation makes NaN at run time, at the equation's line; a
    script's step that fails, at the script's file and line."""
    d = tempfile.mkdtemp(prefix='xppw140b')
    try:
        def load(ode):
            p = subprocess.run([os.path.abspath(args.server), '--server', ode], cwd=d, input='',
                               capture_output=True, text=True, encoding='utf-8', timeout=30 * SLOW)
            return next((json.loads(l) for l in p.stdout.splitlines() if '"ev":"error"' in l), {}), p

        def write(name, text):
            with open(os.path.join(d, name), 'w') as f:
                f.write(text)

        write('gonetab.odex', "par a=1\ntable tb \"gone.tab\"\nx'=-x+tb(t)\n")
        e, p = load('gonetab.odex')
        check('an error event: a table file that is not there is named at the model line that names it (W140b)',
              p.returncode == 1 and e.get('file') == 'gonetab.odex' and e.get('line') == 2
              and e.get('source') == 'table tb "gone.tab"' and 'gone.tab' in e.get('error', ''), str(e))
        check('... and the log reads file:line:column: what', 'gonetab.odex:2:1: gone.tab: cannot be read' in p.stderr,
              repr(p.stderr[-300:]))
        write('short.tab', '5\n0\n1\n0.5\n')
        write('shorttab.odex', "table tb \"short.tab\"\nx'=-x+tb(t)\n")
        e, p = load('shorttab.odex')
        check('an error event: a table file that ends too soon is named at its own last line (W140b)',
              p.returncode == 1 and e.get('file') == 'short.tab' and e.get('line') == 4 and e.get('source') == '0.5'
              and 'too short' in e.get('error', ''), str(e))
        # W505: every table number is read through the typed reader: a word, a nan or an infinite
        # value or endpoint is refused at its own line, naming the value
        for name, text, line, source in [('word.tab', '3\n0\n2\n1\nnonsense\n3\n', 5, 'nonsense'),
                                         ('nanvalue.tab', '3\n0\n2\n1\n2\nnan\n', 6, 'nan'),
                                         ('infend.tab', '3\n0\ninf\n1\n2\n3\n', 3, 'inf')]:
            write(name, text)
            write(name[:-4] + '.odex', "table tb \"%s\"\nx'=-x+tb(t)\n" % name)
            e, p = load(name[:-4] + '.odex')
            check('an error event: a table value that is not a finite number is named at its line (W505): ' + name,
                  p.returncode == 1 and e.get('file') == name and e.get('line') == line and e.get('source') == source
                  and 'is not a finite number' in e.get('error', ''), str(e))

        write('nan.odex', "# NaN at the first step\npar a=1\nx'=sqrt(x-2*a)\ninit x=1\n")
        code, out, err = run_replay([{'cmd': 'run.initial'},
                                     {'cmd': 'nosuchcommand'}], ode=os.path.join(d, 'nan.odex'))
        errs = [json.loads(l) for l in out.splitlines() if '"error"' in l and ('"message"' in l or '"done"' in l)]
        nan = next((m for m in errs if 'NaN' in m.get('error', '')), {})
        check('an error event: a run-time NaN is named at the equation that makes it (W140b)',
              nan.get('file') == 'nan.odex' and nan.get('line') == 3 and nan.get('source') == "x'=sqrt(x-2*a)"
              and nan.get('error', '').startswith('X is NaN at t'), str(errs))
        step = next((m for m in errs if 'nosuchcommand' in m.get('error', '')), {})
        check("an error event: a script's step is named at the script's file and line (W140b)",
              step.get('file', '').endswith('run.recx') and
              (step.get('line'), step.get('source')) == run_replay.places[-1], str(step))
    finally:
        shutil.rmtree(d, ignore_errors=True)


check_error_places_of_each_kind()


def check_load_all_or_nothing():
    """W125: loading one of our files is all or nothing. A set file (File >
    Import settings), a parameter file and an initial-conditions file (`values`
    `read`) whose last value is bad are refused at that line, naming the file
    and the line as written, and the session is exactly as it was (its state,
    and the set file it writes). A model's internal set (File > Get par set)
    is checked whole when the model loads (W318), so none can fail here."""
    model = os.path.join(tempfile.mkdtemp(prefix='w125m'), 'w125.odex')
    with open(model, 'w') as f:
        f.write("x'=-a*x+b\ny'=x-y\npar a=1,b=2\ninit x=1,y=0\nset \"good\" { a: 3, b: 4 }\n")
    p, r, snd, col, _ = launch_server(ode=model)

    def answered(answers, **cmd):
        """cmd, its asks answered in order (a str is a key, a dict the answer; then cancel), up to idle"""
        snd(**cmd)
        got, n = [], 0
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] in ('idle','done'):
                return got
            a = answers[n] if n < len(answers) else {'ok': 0}
            n += 1
            snd(cmd='answer', id=e['id'], **({'key': a} if isinstance(a, str) else a))

    def state():
        st = last_state(answered((), cmd='session.get_state'))
        return {k: v for k, v in st.items() if k not in ('_t', 'session', 'changed')} if st else st

    def errors(evs):
        return [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]

    written = []

    def set_file():
        """the settings the session saves now (its model.setx); CR LF read
        as LF"""
        written.append('now%d' % len(written))
        answered((), cmd='session.save_as', file=written[-1], data=False)
        with zipfile.ZipFile(os.path.join(r, written[-1] + '.snapx')) as z:
            return z.read('model.setx').decode().replace('\r\n', '\n')

    def read_set(name):
        """File > Import settings of name"""
        return answered((), cmd='session.import_settings', file=name)

    try:
        col(is_idle)
        answered((), cmd='value.set_parameters', values={'a': 1.5})
        answered((), cmd='ic.set', values={'x': 0.25})
        st0, set0 = state(), set_file()
        rows = set0.split('\n')
        high = max(k for k in range(len(rows)) if rows[k].startswith('  high:')) + 1  # its line in the file
        lines = set0.split('\n')
        lines[high - 1] = '  high: 1e999,'
        with open(os.path.join(r, 'bad.setx'), 'w') as f:
            f.write('\n'.join(lines))
        par = '2   Number params\n9  a\noops  b\n'
        with open(os.path.join(r, 'bad.par'), 'w') as f:
            f.write(par)
        with open(os.path.join(r, 'bad.ic'), 'w') as f:
            f.write('9\nnan?\n')
        for what, cmd, answers, file, line, source in (
                ('a settings file', None, 'bad.setx', 'bad.setx', high, '  high: 1e999,'),
                ('a parameter file', dict(cmd='value.load_file', kind='parameters', file='bad.par'), (), 'bad.par', 3, 'oops  b'),
                ('an initial-conditions file', dict(cmd='value.load_file', kind='initial_conditions', file='bad.ic'), (), 'bad.ic', 2, 'nan?')):
            errs = errors(read_set(answers) if cmd is None else answered(answers, **cmd))
            e = errs[0] if len(errs) == 1 else {}
            check('W125: %s whose last value is bad is refused at that line (%s:%d)' % (what, file, line),
                  os.path.basename(e.get('file', '')) == file and e.get('line') == line
                  and e.get('source') == source, str(errs))
            st, set1 = state(), set_file()
            check('W125: after %s refused, the session is as it was (its state, its settings)' % what,
                  st == st0 and set1 == set0, str([(k, st0.get(k), st.get(k)) for k in st0 if st.get(k) != st0.get(k)])[:300])
        # W310: foreign .par names determine the destination; rejection is atomic.
        with open(os.path.join(r, 'reordered.par'), 'w') as f:
            f.write('2   Number params\n4  b\n3  a\n\nFile:w125.odex\nWed Jun 30 21:49:08 1993\n')
        evs = answered((), cmd='value.load_file', kind='parameters', file='reordered.par')
        reordered = state()
        check('W310: reordered parameters load by name, with the XPPAUT trailer accepted',
              not errors(evs) and dict(reordered['pars']) == {'a': 3, 'b': 4}, str(reordered['pars']))
        answered((), cmd='value.set_parameters', values={'a': 1.5, 'b': 2})
        for file, kind, text, line, name in (
                ('renamed.par', 'par', '2   Number params\n9  a\n8  renamed\n', 3, 'renamed'),
                ('duplicate.par', 'par', '2   Number params\n9  a\n8  a\n', 3, 'a'),
                ('missing.par', 'par', '2   Number params\n9  a\n', 1, 'b'),
                ('short.ic', 'ic', '9\n', 2, None),
                ('long.ic', 'ic', '9\n8\n7\n', 3, None),
                ('oversize.par', 'par', ' ' * (4 * 1024 * 1024 + 1), 0, None),
                ('oversize.ic', 'ic', ' ' * (4 * 1024 * 1024 + 1), 0, None)):
            with open(os.path.join(r, file), 'w') as f:
                f.write(text)
            errs = errors(answered((), cmd='value.load_file', kind={'par': 'parameters', 'ic': 'initial_conditions'}[kind], file=file))
            e = errs[0] if len(errs) == 1 else {}
            source = text.splitlines()[line - 1] if 0 < line <= len(text.splitlines()) else ''
            check('W310: %s refused with its file, line, source and name' % file,
                  os.path.basename(e.get('file', '')) == file and e.get('line') == line
                  and e.get('source') == source and (name is None or '"%s"' % name in e.get('error', '')),
                  str(errs))
            st, set1 = state(), set_file()
            check('W310: %s leaves all values and settings unchanged' % file,
                  st == st0 and set1 == set0, str(st))
        # W153: another model's names; valid earlier changes must not apply.
        other = set0.split('\n')
        named_line = next(k for k, row in enumerate(other) if row.startswith('  b:'))
        other[named_line] = '  another_models_parameter: 9,'
        with open(os.path.join(r, 'other.setx'), 'w') as f:
            f.write('\n'.join(other))
        evs = read_set('other.setx')
        errs = errors(evs)
        e = errs[0] if len(errs) == 1 else {}
        check('W153: another model refused with file, wrong line and source',
              os.path.basename(e.get('file', '')) == 'other.setx' and e.get('line') == named_line + 1
              and e.get('source') == other[named_line] and 'another_models_parameter' in e.get('error', ''), str(errs))
        check('W153: refused names apply nothing', state() == st0 and set_file() == set0)
        # a key the file does not know, and a group written twice, are refused at their line
        with open(os.path.join(r, 'extra.setx'), 'w') as f:
            f.write(set0 + 'nosuch_group: { a: 1 },\n')
        errs = errors(read_set('extra.setx'))
        e = errs[0] if len(errs) == 1 else {}
        check('W284: a group the settings do not have is refused at its line',
              os.path.basename(e.get('file', '')) == 'extra.setx' and e.get('line') == len(set0.split('\n'))
              and 'nosuch_group' in e.get('error', ''), str(errs))
        answered((), cmd='value.set_parameters', values={'a': 8})
        answered((), cmd='ic.set', values={'x': 7})
        with open(os.path.join(r, 'own.setx'), 'w') as f:
            f.write(set0)
        evs = read_set('own.setx')
        messages = [e for e in evs if e.get('ev') == 'message']
        check('W284: own settings apply, with one message and no session written',
              not errors(evs) and state() == st0 and not os.path.exists(os.path.join(r, 'own.snapx'))
              and len(messages) == 1 and 'own.setx' in messages[0].get('bottom', ''), str(messages))
        # a file that names little sets only that
        with open(os.path.join(r, 'little.setx'), 'w') as f:
            f.write('parameters: { a: 2.5 },\n')
        errs = errors(read_set('little.setx'))
        st = state()
        check('W284: a settings file that names only a parameter sets only it',
              not errs and dict(st['pars'])['a'] == 2.5 and st['ics'] == st0['ics'], str(errs))
        errs = errors(answered((), cmd='set.apply', name='good', scope='all'))
        st = state()
        check('W125: a good internal set still applies whole', not errs and st['pars'] != st0['pars'], str(errs))
    finally:
        stop_server(p, r, snd)


check_load_all_or_nothing()


def check_apply_restore():
    """W319: Apply set (`set.apply`) and Restore defaults (`set.restore_defaults`): a patch limited to a
    scope, a target graph, all or nothing, and a recording replays both"""
    folder = tempfile.mkdtemp(prefix='w319')
    model = os.path.join(folder, 'w319.odex')
    with open(model, 'w') as f:
        f.write("x'=-a*x+b\ny'=-y+c\npar a=1,b=2,c=0.5\ninit x=0.5,y=2\n"
                "options { integration: { dt: 0.02, total_time: 9 } }\n"
                "set \"pa\" { a: 10, b: 20 }\n"
                "set \"mix\" { a: 5, x: 0.9, options: { integration: { dt: 0.1, total_time: 7 }, plot: { xlo: -3, xhi: 3 } } }\n")
    p, r, snd, col, _ = launch_server(ode=model)
    errs = lambda evs: [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]

    def run(**cmd):
        snd(**cmd)
        return col(is_idle, timeout=60 * SLOW)[0]

    def view():
        """the parameters, initial values, dt and total time, and each plot window's x axis"""
        st = last_state(run(cmd='session.get_state'))
        evs = run(cmd='events.subscribe', events=['numerics', 'plots'])
        num = next(dict((f['key'], f['value']) for f in e['fields']) for e in reversed(evs) if e.get('ev') == 'numerics')
        pl = next(e for e in reversed(evs) if e.get('ev') == 'plots')
        return (dict(st['pars']), {k.lower(): v for k, v in st['ics']}, num['dt'], num['total_time'],
                {w['win']: (w['xlo'], w['xhi']) for w in pl['windows']}, pl['active'])

    try:
        col(is_idle)
        start = view()
        check('W319: the model gives its defaults (init y = 2)',
              start[0] == {'a': 1, 'b': 2, 'c': 0.5} and start[1] == {'x': 0.5, 'y': 2} and start[2] == 0.02 and start[3] == 9, str(start))
        evs = run(cmd='set.apply', name='pa', scope='parameters')
        v = view()
        check('W319: a parameter-only apply changes the parameters and leaves the initial values (y = 2 stays 2)',
              not errs(evs) and v[0] == {'a': 10, 'b': 20, 'c': 0.5} and v[1] == start[1] and v[2:] == start[2:], str(v))
        evs = run(cmd='set.apply', name='mix', scope='initial')
        v = view()
        check('W319: scope initial applies only the states of the set',
              not errs(evs) and v[1] == {'x': 0.9, 'y': 2} and v[0]['a'] == 10 and v[2:] == start[2:], str(v))
        evs = run(cmd='set.apply', name='mix', scope='integration')
        v = view()
        check('W319: scope integration applies only the numerics of the set',
              not errs(evs) and v[2] == 0.1 and v[3] == 7 and v[0]['a'] == 10 and v[4] == start[4], str(v))
        evs = run(cmd='set.apply', name='mix', scope='plot')
        v = view()
        check('W319: scope plot applies only the plot settings, to the active graph',
              not errs(evs) and v[4] == {'graph_1': (-3, 3)} and v[0]['a'] == 10, str(v))
        # restore a field, a section, then everything
        run(cmd='set.restore_defaults', scope='integration', field='dt')
        v = view()
        check('W319: restoring a field gives the model\'s value (the options block over the built-in) and no other',
              v[2] == 0.02 and v[3] == 7, str(v))
        run(cmd='set.restore_defaults', scope='parameters', field='a')
        v = view()
        check('W319: restoring one parameter leaves the others', v[0] == {'a': 1, 'b': 20, 'c': 0.5}, str(v))
        run(cmd='set.restore_defaults', scope='initial')
        v = view()
        check('W319: restoring the initial values gives the model\'s (init y = 2 as loaded)', v[1] == start[1], str(v))
        run(cmd='set.restore_defaults', scope='integration')
        v = view()
        check('W319: restoring a section gives the model\'s numerics', v[2:4] == start[2:4], str(v))
        run(cmd='set.apply', name='mix', scope='all')
        v = view()
        check('W319: scope all applies parameters, states, numerics and plot',
              v[0]['a'] == 5 and v[1]['x'] == 0.9 and v[2] == 0.1 and v[4] == {'graph_1': (-3, 3)}, str(v))
        evs = run(cmd='set.restore_defaults', scope='all')
        check('W319: restoring all gives every default back', not errs(evs) and view() == start, str(view()))
        # a failure changes nothing and says why
        before = view()
        for bad, text in ((dict(cmd='set.apply', name='nosuch', scope='all'), 'the model has no set nosuch'),
                          (dict(cmd='set.apply', name='mix', scope='bogus'), 'Invalid choice "bogus" for scope'),
                          (dict(cmd='set.apply', name='mix', scope='all', window='graph_5'), 'Unknown window "graph_5"'),
                          (dict(cmd='set.apply', name='mix', scope='all', window=1e300), 'must be text'),
                          (dict(cmd='set.restore_defaults', scope='plot', window='graph_0'), 'Unknown window "graph_0"'),
                          (dict(cmd='set.apply', name='mix', scope='parameters', window='graph_1'), 'a graph is a target of scope all or plot'),
                          (dict(cmd='set.apply', name='mix'), 'Missing argument "scope"'),
                          (dict(cmd='set.restore_defaults', scope='all', field='a'), 'restoring all takes no field'),
                          (dict(cmd='set.restore_defaults', scope='parameters', field='nosuch'), 'the model has no parameter nosuch'),
                          (dict(cmd='set.restore_defaults', scope='integration', field='xlo'), 'xlo is not a setting of integration')):
            es = errs(run(**bad))
            check('W319: %s is refused (%s), nothing changed' % (json.dumps(bad), text),
                  len(es) == 1 and text in es[0] and view() == before, str(es))
        # a target graph: window 2 is active, the set goes to window 1
        snd(cmd='window.create')
        col(is_idle)
        v = view()
        check('W319: Makewindow/Create made window 2 active', v[5] == 'graph_2' and sorted(v[4]) == ['graph_1', 'graph_2'], str(v))
        x2 = v[4]['graph_2']
        evs = run(cmd='set.apply', name='mix', scope='plot', window='graph_1')
        v = view()
        check('W319: win names the target graph: window 1 changes, window 2 and the active graph stay',
              not errs(evs) and v[4]['graph_1'] == (-3, 3) and v[4]['graph_2'] == x2 and v[5] == 'graph_2', str(v))
        evs = run(cmd='set.restore_defaults', scope='plot', window='graph_1')
        v = view()
        check('W319: restore names its target graph too', not errs(evs) and v[4]['graph_1'] == start[4]['graph_1'] and v[4]['graph_2'] == x2 and v[5] == 'graph_2', str(v))
        # a recording of both replays to the same end
        run(cmd='recording.start')
        run(cmd='set.apply', name='pa', scope='parameters')
        run(cmd='set.apply', name='mix', scope='integration')
        run(cmd='set.restore_defaults', scope='parameters', field='b')
        end = view()
        run(cmd='recording.stop', file='w319.recx')
        replay = replay_recording(args.server, model, os.path.join(r, 'w319.recx'), r)
        replayed = [json.loads(l) for l in replay.stdout.splitlines() if l.strip()]
        last = next((e for e in reversed(replayed) if is_state(e)), {})
        check('W319: a recording of Apply set and Restore defaults replays to the same values',
              replay.returncode == 0 and dict(last.get('pars', [])) == end[0] and {k.lower(): v for k, v in last.get('ics', [])} == end[1], '%d %s' % (replay.returncode, replay.stderr[-300:]))
    finally:
        stop_server(p, r, snd)
        shutil.rmtree(folder, ignore_errors=True)


check_apply_restore()


def check_save_values():
    """W320: Save values into the model file (`set.save_new`, `set.save_into`, `set.save_defaults`,
    `set.export`, `set.rename`, `set.delete`): the file's text before and after (the rest byte for byte), the live model following,
    a refused write changing nothing, and a recording replaying the saves"""
    folder = tempfile.mkdtemp(prefix='w320')
    model = os.path.join(folder, 'w320.odex')
    original = ("# W320 test model\n"
                "import \"lib.odex\"\n"
                "x' = -a*x + b   # decay\n"
                "y' = -y + c\n"
                "par a = 1, b = 2, c = 0.5\n"
                "init x = 0.5, y = 2\n"
                "\n"
                "options {\n"
                "  integration: { dt: 0.02, total_time: 9 },\n"
                "}\n"
                "\n"
                "# regimes\n"
                "set \"pa\" { a: 10, b: 20 }\n"
                "set \"mix\" { a: 5, x: 0.9, options: { integration: { dt: 0.1, total_time: 7 }, plot: { xlo: -3, xhi: 3 } } }\n")
    with open(model, 'w', newline='') as f:
        f.write(original)
    with open(os.path.join(folder, 'lib.odex'), 'w', newline='') as f:
        f.write("set \"shared\" { a: 3 }\n")
    s = Server(args.server, model, run=folder)
    errs = lambda evs: [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]

    def run(**cmd):
        s.send(**cmd)
        return s.answer_asks(is_idle, {'choice': lambda e: {'key': 'y'}}, timeout=60 * SLOW)[0]

    def text(name='w320.odex'):
        with open(os.path.join(folder, name), newline='') as f:
            return f.read()

    def view():
        st = last_state(run(cmd='session.get_state'))
        evs = run(cmd='events.subscribe', events=['numerics'])
        num = next(dict((f['key'], f['value']) for f in e['fields']) for e in reversed(evs) if e.get('ev') == 'numerics')
        return (dict(st['pars']), {k.lower(): v for k, v in st['ics']}, num['dt'], num['total_time'])

    try:
        s.collect(is_idle)
        before = text()
        run(cmd='value.set_parameters', values={'a': 7, 'b': 2.5})
        # a new set goes at the end of the file, the rest as it was
        evs = run(cmd='set.save_new', name='s1', scope='parameters')
        after = text()
        check('W320: a new set is appended; the rest of the file is byte for byte as it was',
              not errs(evs) and after.startswith(before) and after[len(before):].strip().startswith('set "s1" {')
              and 'a: 7' in after[len(before):] and 'b: 2.5' in after[len(before):] and 'x:' not in after[len(before):], repr(after[len(before):]) + str(errs(evs)))
        run(cmd='value.set_parameters', values={'a': 1})
        evs = run(cmd='set.apply', name='s1', scope='parameters')
        check('W320: the live model knows the new set at once', not errs(evs) and view()[0]['a'] == 7, str(errs(evs)))
        # a part of a set replaced, its other parts as they were
        run(cmd='ic.set', values={'x': 0.1, 'y': 0.2})
        old = text()
        evs = run(cmd='set.save_into', name='mix', scope='initial')
        new = text()
        mix = [new[new.index('set "mix"'):]]
        check('W320: replacing the initial values of mix keeps its parameter and its options as written',
              not errs(evs) and mix and 'a: 5' in mix[0] and 'x: 0.1' in mix[0] and 'y: 0.2' in mix[0] and 'x: 0.9' not in mix[0]
              and 'integration: { dt: 0.1, total_time: 7 }' in mix[0] and 'plot: { xlo: -3, xhi: 3 }' in mix[0], str(mix) + str(errs(evs)))
        check('W320: the rest of the file is byte for byte as it was', new[:old.index('set "mix"')] == old[:old.index('set "mix"')], '')
        run(cmd='ic.set', values={'x': 3})
        run(cmd='set.apply', name='mix', scope='initial')
        check('W320: the live model uses the replaced part', view()[1]['x'] == 0.1)
        evs = run(cmd='set.save_into', name='mix', scope='plot')
        mix = [text()[text().index('set "mix"'):]]
        check('W320: replacing the plot settings leaves the rest of the set', not errs(evs) and mix and 'xlo: -3' not in mix[0]
              and 'x: 0.1' in mix[0] and 'integration: { dt: 0.1, total_time: 7 }' in mix[0] and 'plot: {' in mix[0], str(mix) + str(errs(evs)))
        # the defaults: par and init lines in place, an options group
        run(cmd='value.set_parameters', values={'a': 4})
        run(cmd='run.configure', dt=0.05)
        old = text()
        evs = run(cmd='set.save_defaults', scope='parameters')
        new = text()
        check('W320: Save as defaults rewrites the changed values of the par line where they stand, nothing else',
              not errs(evs) and 'par a = 4, b = 2.5, c = 0.5' in new and new.replace('par a = 4, b = 2.5, c = 0.5', 'par a = 1, b = 2, c = 0.5') == old, new[:300] + str(errs(evs)))
        run(cmd='value.set_parameters', values={'a': 9})
        run(cmd='set.restore_defaults', scope='parameters')
        check('W320: Restore defaults uses the new defaults at once', view()[0]['a'] == 4 and view()[0]['b'] == 2.5, str(view()))
        evs = run(cmd='set.save_defaults', scope='integration')
        new = text()
        check('W320: an options group is replaced in the options block',
              not errs(evs) and 'dt: 0.05' in new and 'dt: 0.02' not in new and new.count('options {') == 1, new[new.index('options'):][:200] + str(errs(evs)))
        run(cmd='run.configure', dt=0.5)
        run(cmd='set.restore_defaults', scope='integration')
        check('W320: Restore defaults gives the saved options', view()[2] == 0.05, str(view()))
        evs = run(cmd='set.save_defaults', scope='initial')
        new = text()
        check('W320: the initial values saved as defaults replace the init line where it stands',
              not errs(evs) and 'init x = ' in new, new[new.index('init'):][:80] + str(errs(evs)))
        # export to another file, a new one then an existing one with a set of that name
        evs = run(cmd='set.export', name='e1', scope='parameters', file='out.odex')
        out = text('out.odex')
        check('W320: Export writes a new file with the Saved-from header and the set',
              not errs(evs) and out.startswith('# Saved from w320.odex at ') and 'set "e1" {' in out, out + str(errs(evs)))
        evs = run(cmd='set.export', name='e1', scope='initial', file='out.odex')
        out2 = text('out.odex')
        check('W320: Export to an existing set asks and replaces its part of that kind',
              not errs(evs) and out2.count('set "e1"') == 1 and out2.startswith(out.split('set "e1"')[0]), out2 + str(errs(evs)))
        evs = run(cmd='set.export', name='e2', scope='integration', file='out.odex')
        out3 = text('out.odex')
        check('W320: Export to an existing file appends a set of another name', not errs(evs) and 'set "e2" {' in out3 and out3.startswith(out2), out3 + str(errs(evs)))
        # rename and delete
        evs = run(cmd='set.rename', **{'from': 'pa'}, to='pb')
        new = text()
        check('W320: Rename changes the name in the file and in the model',
              not errs(evs) and 'set "pb" { a: 10, b: 20 }' in new and 'set "pa"' not in new, str(errs(evs)))
        evs = run(cmd='set.apply', name='pb', scope='parameters')
        check('W320: the renamed set applies, the old name is gone', not errs(evs) and view()[0]['a'] == 10
              and errs(run(cmd='set.apply', name='pa', scope='parameters')), '')
        old = text()
        evs = run(cmd='set.delete', name='pb')
        new = text()
        check('W320: Delete removes the set and its line only', not errs(evs) and new == old.replace('set "pb" { a: 10, b: 20 }\n', ''), new[-200:] + str(errs(evs)))
        # refusals change nothing
        old = text()
        for bad, why in ((dict(cmd='set.save_new', name='s1', scope='parameters'), 'already a set'),
                         (dict(cmd='set.save_into', name='shared', scope='parameters'), 'read-only'),
                         (dict(cmd='set.save_into', name='nosuch', scope='parameters'), 'no set nosuch'),
                         (dict(cmd='set.save_new', name='bad name', scope='parameters'), 'not a valid name'),
                         (dict(cmd='set.save_new', name='s9', scope='bogus'), 'Invalid choice "bogus" for scope'),
                         (dict(cmd='set.save_new', name='s9', scope='plot', window='graph_7'), 'Unknown window'),
                         (dict(cmd='set.save_new', name='s9', scope='parameters', state='next'), 'Invalid choice "next" for state'),
                         (dict(cmd='set.save_new', name='s9', scope='initial', state='last'), 'no run has reached a state'),
                         (dict(cmd='set.rename', **{'from': 'shared'}, to='mine'), 'read-only'),
                         (dict(cmd='set.rename', **{'from': 's1'}, to='mix'), 'already a set'),
                         (dict(cmd='set.delete', name='shared'), 'read-only'),
                         (dict(cmd='set.delete', name='nosuch'), 'no set nosuch'),
                         (dict(cmd='set.export', name='x1', scope='parameters', file='w320.odex'), 'is the model file'),
                         (dict(cmd='set.export', name='x1', scope='parameters', file='notodex.txt'), 'not an .odex file')):
            es = errs(run(**bad))
            check('W320: %s is refused (%s), the file is as it was' % (json.dumps(bad), why),
                  len(es) == 1 and why in es[0] and text() == old, str(es))
        # the file changed on disk since the load: refused, said so, left alone
        with open(model, 'a', newline='') as f:
            f.write('# someone else\n')
        changed = text()
        es = errs(run(cmd='set.save_new', name='s2', scope='parameters'))
        check('W320: a file changed on disk since the load is refused and left as it is',
              len(es) == 1 and 'changed on disk' in es[0] and text() == changed, str(es))
        with open(model, 'w', newline='') as f:
            f.write(old)
        # File > Save values, Export and Delete by their menus: the same writer through the same commands
        evs = run(cmd='set.save_new', name='menuset', scope='parameters')
        check('W320: File > Save values: New set, a name, parameters writes the set', not errs(evs) and 'set "menuset" {' in text(), str(errs(evs)))
        evs = run(cmd='set.export', file='menu_out.odex', name='menuex', scope='plot')
        check('W320: File > Export values writes the set to the file picked', not errs(evs) and 'set "menuex" {' in text('menu_out.odex'), str(errs(evs)))
        evs = run(cmd='set.delete', name='menuset')
        check('W320: File > Delete set asks, then deletes the set from the file', not errs(evs) and 'set "menuset"' not in text(), str(errs(evs)))
        evs = run(cmd='set.rename', **{'from': 's1', 'to': 's1b'})
        check('W320: File > Rename set asks which set and the new name', not errs(evs) and 'set "s1b" {' in text() and 'set "s1" {' not in text(), str(errs(evs)))
        old = text()
        # a plot's settings saved and applied again
        evs = run(cmd='set.apply', name='mix', scope='plot')
        check('W320: the plot settings saved in a set apply again', not errs(evs), str(errs(evs)))
        # a recording of the saves replays on the model it recorded (a saved model's copy changes, not the disk)
        run(cmd='recording.start')
        run(cmd='value.set_parameters', values={'a': 3})
        run(cmd='set.save_new', name='rec1', scope='parameters')
        run(cmd='value.set_parameters', values={'a': 1})
        run(cmd='set.rename', **{'from': 'rec1'}, to='rec2')
        run(cmd='set.apply', name='rec2', scope='parameters')
        end = view()
        run(cmd='recording.stop', file='w320.recx')
        on_disk = text()
        replay = replay_recording(args.server, model, os.path.join(folder, 'w320.recx'), folder)
        replayed = [json.loads(l) for l in replay.stdout.splitlines() if l.strip()]
        last = next((e for e in reversed(replayed) if is_state(e)), {})
        check('W320: a recording of Save values, Rename and Apply set replays to the same values',
              replay.returncode == 0 and dict(last.get('pars', [])) == end[0], '%d %s' % (replay.returncode, replay.stderr[-300:]))
        check('W320: the replay wrote nothing to the model file on the disk', text() == on_disk)
        with open(model, 'w', newline='') as f:
            f.write(old)
        # the saved file loads again and gives the same values
        t = Server(args.server, model, run=folder)
        try:
            evs, _ = t.collect(is_idle)
            st = last_state(evs)
            check('W320: the file as saved loads again, with the saved defaults',
                  st and dict(st['pars'])['a'] == 4 and dict(st['pars'])['b'] == 2.5, str(st and st['pars']))
        finally:
            t.close()
    finally:
        s.close()
        shutil.rmtree(folder, ignore_errors=True)


check_save_values()


# Nullclines, direction fields and flows as data (docs/protocol.md "The
# plot as data", docs/ui-v2.md T7), in plot coordinates.
def check_phase_data():
    proc5, run5, send5, collect5, _ = launch_server()


    def after(cmd):
        send5(**cmd)
        return collect5(is_idle, timeout=30 * SLOW)[0]

    of = lambda evs, name: [e for e in evs if e.get('ev') == name]

    def segments_ok(segs, v):
        """'' when segs [x1,y1,x2,y2,...] are whole segments inside the view (a little slack), else why"""
        if not segs or len(segs) % 4:
            return '%d values' % len(segs)
        dx, dy = (v['xhi'] - v['xlo']) * 0.01, (v['yhi'] - v['ylo']) * 0.01
        for k in range(0, len(segs), 2):
            x, y = segs[k], segs[k + 1]
            if not (v['xlo'] - dx <= x <= v['xhi'] + dx and v['ylo'] - dy <= y <= v['yhi'] + dy):
                return 'point %s outside the view %s' % ((x, y), v)
        return ''

    def grid_ok(grid, n, v):
        """'' when the arrows start on the n x n grid of the view, else why"""
        if len(grid) != 4 * n * n:
            return 'count %d arrows' % (len(grid) // 4)
        xs = sorted({round(grid[k], 6) for k in range(0, len(grid), 4)})
        ys = sorted({round(grid[k + 1], 6) for k in range(0, len(grid), 4)})
        if len(xs) != n or len(ys) != n:
            return '%d x %d distinct starts' % (len(xs), len(ys))
        if abs(xs[0] - v['xlo']) > 1e-4 * (v['xhi'] - v['xlo']) or abs(ys[0] - v['ylo']) > 1e-4 * (v['yhi'] - v['ylo']):
            return 'first start %s, view %s' % ((xs[0], ys[0]), v)
        return ''

    try:
        evs, _ = collect5(is_idle)
        hello5 = next((e for e in evs if e.get('ev') == 'hello'), {})
        check('hello lists the nullclines and dfield features',
              {'nullclines', 'dfield'} <= set(hello5.get('features', [])), str(hello5.get('features')))
        evs = after({'cmd': 'events.subscribe', 'events': ['nullclines', 'dfield']})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('data sends both at once, empty, for window 1',
              len(nc) == 1 and len(df) == 1 and nc[0]['win'] == 'graph_1' and nc[0]['x'] == [] and nc[0]['frozen'] == []
              and df[0]['grid'] == [] and df[0]['flows'] == [] and not of(evs, 'series'), str(evs)[:300])

        evs = after({'cmd': 'nullcline.compute'})
        nc, v = of(evs, 'nullclines'), last_state(evs)['view']
        n0 = nc[0] if nc else {}
        check('Nullcline/New: the event names V and W, colours #f06400 and #00e100 (XPPAUT\'s 2 and 7)',
              len(nc) == 1 and n0['xname'] == 'V' and n0['yname'] == 'W' and n0['xcolor'] == '#f06400' and n0['ycolor'] == '#00e100',
              str(nc)[:300])
        why = (segments_ok(n0.get('x', []), v) or segments_ok(n0.get('y', []), v)) if n0 else 'no event'
        check('... its x- and y-nullclines are whole segments inside the view', not why, why)
        evs = after({'cmd': 'window.redraw'})
        check('a redraw draws them again and sends nothing', not of(evs, 'nullclines') and not of(evs, 'dfield'))

        evs = after({'cmd': 'dirfield.compute', 'grid': 16, 'scaled': True})
        df, v = of(evs, 'dfield'), last_state(evs)['view']
        d0 = df[0] if df else {}
        check('Dir.field/Scaled: dfield has a 17 x 17 grid, scaled, in the curve\'s colour',
              len(df) == 1 and d0['n'] == 17 and d0['scaled'] == 1 and d0['color'] == 'foreground'
              and len(d0['grid']) == 4 * 289 and len(d0['speed']) == 289, str(df)[:300])
        why = grid_ok(d0.get('grid', []), 17, v) if d0 else 'no event'
        check('... its arrows start on the 17 x 17 grid of the view', not why, why)
        check('... unit directions', all(abs(d0['grid'][k + 2] ** 2 + d0['grid'][k + 3] ** 2 - 1) < 1e-5
                                         for k in range(0, len(d0.get('grid', [])), 4)) if d0 else False)

        evs = after({'cmd': 'nullcline.freeze'})
        nc = of(evs, 'nullclines')
        ncx = nc[0]['x'] if nc else []
        check('Nullcline/Freeze/Freeze: the frozen nullclines are in the event',
              len(nc) == 1 and len(nc[0]['frozen']) == 1 and nc[0]['frozen'][0]['x'] == nc[0]['x']
              and nc[0]['frozen'][0]['y'] == nc[0]['y'], str(nc)[:300])

        evs = after({'cmd': 'window.clear'})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('Erase clears them: both events empty',
              len(nc) == 1 and nc[0]['x'] == [] and nc[0]['frozen'] == [] and len(df) == 1 and df[0]['grid'] == [],
              str(evs)[:300])
        evs = after({'cmd': 'window.redraw'})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('a redraw draws the nullclines again (not the field Erase turned off)',
              len(nc) == 1 and len(nc[0]['x']) > 0 and len(nc[0]['frozen']) == 1 and not df, str(evs)[:300])

        evs = after({'cmd': 'dirfield.flow', 'grid': 5})
        df = of(evs, 'dfield')
        fl = df[0]['flows'] if df else []
        xs = fl[0]['x'] if fl else []
        starts = [0] + [i + 1 for i, a in enumerate(xs) if a is None]
        check('Dir.field/Flow: one curve of 72 trajectories (6 x 6, forward and back), finite points',
              len(fl) == 1 and fl[0]['color'] == 'foreground' and len(starts) == 72 and len(xs) == len(fl[0]['y'])
              and all(a is None or abs(a) < 1e6 for a in xs), '%d curves, %d trajectories, %d points'
              % (len(fl), len(starts), len(xs)))
        check('... each trajectory starts on the grid',
              bool(fl) and all(abs((xs[s] + 0.6) / 0.36 - round((xs[s] + 0.6) / 0.36)) < 1e-4 for s in starts),
              str([xs[s] for s in starts][:8]))

        evs = after({'cmd': 'events.subscribe', 'events': ['nullclines', 'dfield'], 'encoding': 'f32'})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        same = False
        if nc and df and fl:
            same = (nc[0].get('enc') == 'f32' and df[0].get('enc') == 'f32'
                    and same_floats(values({'data': nc[0]['x']}, 'f32'), values({'data': ncx}, None))
                    and same_floats(values({'data': df[0]['flows'][0]['x']}, 'f32'), values({'data': xs}, None)))
        check('enc f32: the same values as base64 float32', same, str(evs)[:200])
        after({'cmd': 'events.unsubscribe', 'events': ['nullclines', 'dfield']})
        evs = after({'cmd': 'events.subscribe', 'events': ['series']})
        check('not asked for: neither is sent', not of(evs, 'nullclines') and not of(evs, 'dfield'))
    finally:
        stop_server(proc5, run5, send5)


check_phase_data()


# "Use this view" (docs/ui-v2.md T9, GitHub issue #18, W60): the keys
# Window/Window (w w) and its form's four numbers set the active window's axes
# (graf_par.cpp user_window), so state.view, "plots" and a PostScript export
# all agree with them afterward. The `view` command that used to do it is gone
# (an unknown command).
def check_view():
    proc6, run6, send6, collect6, _ = launch_server()
    collect6(is_idle)  # startup hello/state/idle
    send6(cmd='events.subscribe', events=['plots'])
    collect6(is_idle)

    def view_keys(xlo, xhi, ylo, yhi):
        """Window/Window answered with the four numbers; returns (plots or
        None, message-error text or None, state.view) of the events up to idle"""
        send6(cmd='window.set_bounds',x_min=xlo,x_max=xhi,y_min=ylo,y_max=yhi)
        got,_=collect6(is_idle)
        pl = next((e for e in got if e.get('ev') == 'plots'), None)
        msg = next((e.get('error') for e in got if e.get('ev') in ('message', 'done') and 'error' in e), None)
        st = last_state(got)
        return pl, msg, st and st.get('view')

    def axes(v):
        return v and (v['xlo'], v['xhi'], v['ylo'], v['yhi'])

    try:
        pl, msg, view = view_keys(-100, 100, -100, 100)
        w = pl and next((x for x in pl['windows'] if x['win'] == 'graph_1'), None)
        check('Window/Window sets the axes of the window: plots reflects them',
              msg is None and axes(w) == (-100, 100, -100, 100), str(w))
        check('... and state.view matches, for the active window',
              view is not None and view['win'] == 'graph_1' and axes(view) == (-100, 100, -100, 100), str(view))

        # the command the page's "Use this view" button used to send is gone
        send6(cmd='view', window='graph_1', xlo=-1, xhi=1, ylo=-1, yhi=1)
        evs, _ = collect6(is_idle)
        msg5 = next((e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), None)
        check('the view command is gone: an unknown command', msg5 is not None and 'Unknown command' in msg5, str(msg5))
        check('... and leaves the axes unchanged', axes(last_state(evs).get('view')) == (-100, 100, -100, 100))

        # Graphic stuff/Postscript (key g, then its submenu's p): a menu,
        # then a form for the PS parameters (answered with its own
        # defaults), then a file to write; the written file's axes come
        # from the same MyGraph Window/Window set, so its tick labels
        # ("%g" of the boundary, Box_axis/draw_xtics/draw_ytics in
        # axes2.cpp) are the boundary values themselves, -100 and 100 (a
        # symmetric [-100,100] range makes make_tics() choose a 20-wide
        # tic, which lands exactly on the boundary).
        psname='t9view.pdf'
        send6(cmd='plot.export_pdf',file=psname,color=True,axes_font_size=12)
        collect6(is_idle)
        pspath = os.path.join(run6, psname)
        ps = ''
        if os.path.exists(pspath):
            with open(pspath, encoding='latin-1') as f:
                ps = f.read()
        check('the PDF file was written', ps.startswith('%PDF-1.4'))
        check("its axes use the new view: the boundary tick labels (-100, 100) appear in the file's text",
              '(-100) Tj' in ps and '(100) Tj' in ps, str(len(ps)))
    finally:
        stop_server(proc6, run6, send6)



# 3D plots turned by the client (docs/ui-v2.md T14, GitHub issue #18, W60):
# the key 3 (3d-params) and its form, with Theta and Phi answered with the
# angles web2 settled on after projecting the box itself and turning it
# locally, set the active window's angles and redraw it, so "plots" and
# state.view.theta/phi agree. The `view3d` command that used to do it is gone
# (an unknown command). lorenz.odex sets axes=3d and phi=60 (theta stays the
# default 45).
def check_view3d():
    proc7, run7, send7, collect7, _ = launch_server(ode='examples/ode/lorenz.odex')
    evs0, _ = collect7(is_idle, timeout=30 * SLOW)
    view0 = last_state(evs0) and last_state(evs0)['view']
    check('lorenz.odex opens a 3D window at its @ phi=60 (theta the default 45)',
          view0 is not None and view0['three'] == 1 and view0['theta'] == 45 and view0['phi'] == 60, str(view0))

    send7(cmd='events.subscribe', events=['plots'])
    collect7(is_idle)

    def turn(theta, phi):
        """key 3, its form answered with theta and phi (the rest as offered);
        returns (plots or None, message-error text or None, state.view)"""
        send7(cmd='plot.set_3d_view',theta=theta,phi=phi)
        got,_=collect7(is_idle)
        pl = next((e for e in got if e.get('ev') == 'plots'), None)
        msg = next((e.get('error') for e in got if e.get('ev') in ('message', 'done') and 'error' in e), None)
        st = last_state(got)
        return pl, msg, st and st.get('view')

    def angles(v):
        return v and (v['theta'], v['phi'])

    try:
        pl, msg, view = turn(10, -20)
        w = pl and next((x for x in pl['windows'] if x['win'] == 'graph_1'), None)
        check('3d-params sets the angles of the window: plots reflects them',
              msg is None and w is not None and (w['theta'], w['phi']) == (10, -20), str(w))
        check('... and state.view.theta/phi matches, for the active window',
              view is not None and view['win'] == 'graph_1' and angles(view) == (10, -20), str(view))

        # the command the page used to send is gone
        send7(cmd='view3d', window='graph_1', theta=0, phi=0)
        evs, _ = collect7(is_idle)
        msg2 = next((e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), None)
        check('the view3d command is gone: an unknown command', msg2 is not None and 'Unknown command' in msg2, str(msg2))
        check('... and leaves the angles unchanged', angles(last_state(evs)['view']) == (10, -20))
    finally:
        stop_server(proc7, run7, send7)



# Marks as data (docs/protocol.md "The plot as data", docs/ui-v2.md T8): the
# equilibria Sing pts marks, Text,etc's text, arrows and markers, and frozen
# curves, as the classic window shows them: a redraw draws the labels,
# objects and frozen curves again but not the equilibria, Erase clears them
# all, a deleted one goes at once.
def check_marks():
    proc6, run6, send6, collect6, _ = launch_server()
    of = lambda evs, name: [e for e in evs if e.get('ev') == name]

    def after(cmd):
        send6(**cmd)
        return collect6(is_idle, timeout=30 * SLOW)[0]

    def marks_of(evs):
        m = of(evs, 'marks')
        return m[-1] if len(m) == 1 else None

    def near(a, b, tol):
        return a is not None and b is not None and abs(a - b) <= tol

    try:
        evs, _ = collect6(is_idle)
        hello6 = next((e for e in evs if e.get('ev') == 'hello'), {})
        check('hello lists the marks feature', 'marks' in hello6.get('features', []), str(hello6.get('features')))
        evs = after({'cmd': 'events.subscribe', 'events': ['marks', 'plots', 'series']})
        m = marks_of(evs)
        check('data sends marks at once, empty, for window 1',
              m is not None and m['win'] == 'graph_1' and m['equilibria'] == [] and m['text'] == [] and m['arrows'] == []
              and m['markers'] == [] and m['frozen'] == [], str(of(evs, 'marks'))[:300])
        curve = of(evs, 'plots')[0]['windows'][0]['curves'][0]

        evs = after({'cmd': 'run.initial'})
        ser = whole_series(evs)
        v = last_state(evs)['view']
        px = (v['xhi'] - v['xlo']) / abs(v['right'] - v['left']) * 1.5
        py = (v['yhi'] - v['ylo']) / abs(v['bottom'] - v['top']) * 1.5
        check('an integration sends no marks', bool(ser) and not of(evs, 'marks'))
        cols = {c['col']: c['data'] for c in ser[-1]['columns']} if ser else {}

        typed = r'\1a\0-point \{2*3}'
        evs = after(dict(cmd='annotation.add_text', x=-.3, y=.5, text=typed, text_size=3, style='bold', color='#ffcd00'))
        m = marks_of(evs)
        t = m['text'] if m else []
        check('Text,etc/Text: a text mark with its text (\\{expr} filled in), size, style, colour and position',
              len(t) == 1 and t[0]['text'] == r'\1a\0-point 6' and t[0]['source'] == typed and t[0]['size'] == 3
              and t[0]['font'] == 0 and t[0]['style'] == 'bold' and t[0]['color'] == '#ffcd00'
              and near(t[0]['x'], -0.3, px) and near(t[0]['y'], 0.5, py), str(m)[:300])
        asks = of(evs, 'ask')
        check('W517: typed annotation has no navigation asks', not asks)
        check('W227: hello.text says what a text takes',
              hello6.get('text') == {'max_length': 1000, 'size_max': 4, 'default_size': 2} and hello6.get('text_styles') == ['normal', 'bold', 'italic', 'bold_italic'],
              str(hello6.get('text')))
        tid = t[0]['id'] if t else -1

        # W227, W513: the annotation command edits what is given, moves it, adds and deletes, by id
        def result_of(evs):
            return next((e.get('result', {}) for e in of(evs, 'done')), {})

        def ann(op, **fields):
            cmd = {'cmd': 'annotation.' + op, **fields}
            if 'id' in cmd: cmd['annotation_id'] = cmd.pop('id')
            if op in ('add_text', 'add_arrow', 'add_pointer', 'add_marker', 'list', 'delete_all'): cmd.setdefault('window', 'graph_1')
            evs = after(cmd)
            return evs, marks_of(evs), [e.get('error', '') for e in of(evs, 'done') if e.get('status') == 'error']
        _, m, errs = ann(op='edit', id=tid, color='#0000ff')
        t = m['text'] if m else []
        check('W227: annotation edit of a text with only a colour changes only the colour',
              len(t) == 1 and t[0]['color'] == '#0000ff' and t[0]['style'] == 'bold' and t[0]['size'] == 3 and t[0]['source'] == typed
              and not errs, str((m, errs))[:300])
        ann(op='edit', id=tid, text='moved', text_size=0, style='bold_italic', color='foreground')
        _, m, errs = ann(op='move', id=tid, new_x=0.25, new_y=0.75)
        t = m['text'] if m else []
        check('W227: annotation edit changes the text, size, style, colour and move its position',
              len(t) == 1 and t[0]['text'] == 'moved' and t[0]['source'] == 'moved' and t[0]['size'] == 0
              and t[0]['style'] == 'bold_italic' and t[0]['color'] == 'foreground' and t[0]['id'] == tid and near(t[0]['x'], 0.25, 1e-6)
              and near(t[0]['y'], 0.75, 1e-6) and not errs, str((m, errs))[:300])
        for what, fields, want in [
                ('a size above 4', dict(op='edit', id=tid, text_size=9), 'must be at most 4'),
                ('a style that is no name', dict(op='edit', id=tid, style='4'), 'Invalid choice'),
                ('a colour that is none', dict(op='edit', id=tid, color='banana'), 'must be a CSS colour name'),
                ('a colour that is a number', dict(op='edit', id=tid, color=3), 'must be text'),
                ('a size that is not whole', dict(op='edit', id=tid, text_size=1.5), 'a whole number'),
                ('an empty text', dict(op='edit', id=tid, text=''), 'not empty'),
                ('a text of 1001 characters', dict(op='edit', id=tid, text='x' * 1001), 'at most 1000'),
                ('a move to a position that is not a number', dict(op='move', id=tid, new_x='left', new_y=0), 'must be a finite number'),
                ('a move with no position', dict(op='move', id=tid), 'Missing argument "new_x"'),
                ('an edit of an id there is not', dict(op='edit', id=7777), 'no annotation with that id'),
                ('a delete of an id there is not', dict(op='delete', id=7777), 'no annotation with that id'),
                ('a move of an id there is not', dict(op='move', id=7777, new_x=0, new_y=0), 'no annotation with that id'),
                ('an id that is not a number', dict(op='delete', id='a'), 'must be a whole number'),
                ('an add with no position', dict(op='add_text', text='a'), 'Missing argument "x"'),
                ('an add with no text', dict(op='add_text', x=0, y=0), 'Missing argument "text"'),
                ('a window it has not', dict(op='list', window='graph_9'), 'Unknown window')]:
            evs, m, errs = ann(**fields)
            check('W227: annotation refuses %s, naming it, and changes nothing' % what,
                  m is None and len(errs) == 1 and want in errs[0], str((m, errs))[:300])
        evs, m, errs = ann(op='add_text', text=r'%a\{2*3}', x=0.5, y=-0.5, text_size=4, style='italic', color='#00e100c0')
        t = m['text'] if m else []
        added = next((x for x in t if x['id'] != tid), None)
        added_ev = result_of(evs)
        check('W227: annotation add_text: a new label with its look, a % text stored as its value, its id reported',
              len(t) == 2 and added and added['text'] == added['source'] and added['text'] == 'a6'
              and added['size'] == 4 and added['style'] == 'italic' and added['color'] == '#00e100c0' and not errs
              and added_ev.get('id') == added['id'] and added['id'] > tid, str((m, errs))[:300])
        _, m, errs = ann(op='delete', id=added['id'] if added else -1)
        check('W227: annotation delete removes that label only', m is not None and [x['id'] for x in m['text']] == [tid] and not errs,
              str((m, errs))[:300])
        ann(op='edit', id=tid, text=typed, text_size=3, style='bold', color='#ffcd00')
        _, m, errs = ann(op='move', id=tid, new_x=-0.3, new_y=0.5)
        check('W227: ... and the first label is as Text left it', m is not None and len(m['text']) == 1
              and m['text'][0]['text'] == r'\1a\0-point 6' and not errs, str((m, errs))[:300])

        evs = after(dict(cmd='annotation.add_pointer', tip_x=.1, tip_y=.2, tail_x=.6, tail_y=.8, size=.2, color='#c8c800'))
        m = marks_of(evs)
        a = m['arrows'] if m else []
        check('Text,etc/Pointer: an arrow mark from the first point to the second, its size and colour',
              len(a) == 1 and a[0]['kind'] == 'pointer' and a[0]['size'] == 0.2 and a[0]['color'] == '#c8c800'
              and near(a[0]['x1'], 0.1, px) and near(a[0]['y1'], 0.2, py) and near(a[0]['x2'], 0.6, px)
              and near(a[0]['y2'], 0.8, py) and len(m['text']) == 1, str(m)[:300])

        evs = after(dict(cmd='annotation.add_marker', x=.9, y=.1, shape='diamond', color='#00e100', size=2))
        m = marks_of(evs)
        k = m['markers'] if m else []
        check('Text,etc/Marker: a marker mark with its shape, size, colour and position',
              len(k) == 1 and k[0]['shape'] == 'diamond' and k[0]['size'] == 2 and k[0]['color'] == '#00e100'
              and near(k[0]['x'], 0.9, px) and near(k[0]['y'], 0.1, py), str(m)[:300])

        # W513: every annotation kind has an id, listed, edited, moved and deleted by it, in plot coordinates
        pid, kid = (a[0]['id'] if a else -1), (k[0]['id'] if k else -1)
        check('W513: the pointer and the marker have ids of their own, none the text\'s',
              len({tid, pid, kid}) == 3 and min(tid, pid, kid) >= 0, str((tid, pid, kid)))
        evs, _, errs = ann(op='list')
        ls = [result_of(evs)]
        check('W513: annotation list sends every text, arrow and marker of the window with its id',
              len(ls) == 1 and ls[0].get('window') == 'graph_1' and [x['id'] for x in ls[0]['text']] == [tid]
              and [x['id'] for x in ls[0]['arrows']] == [pid] and [x['id'] for x in ls[0]['markers']] == [kid]
              and ls[0]['arrows'][0]['kind'] == 'pointer' and ls[0]['markers'][0]['shape'] == 'diamond' and not errs, str(ls)[:300])
        _, m, errs = ann(op='move', id=pid, new_x=0.3, new_y=0.4)
        ar = m['arrows'][0] if m else {}
        check('W513: move of a pointer by id puts its tip there and the tail the same way off (data coordinates)',
              near(ar.get('x1'), 0.3, 1e-6) and near(ar.get('y1'), 0.4, 1e-6) and near(ar.get('x2'), 0.8, 2e-3)
              and near(ar.get('y2'), 1.0, 2e-3) and not errs, str((m, errs))[:300])
        _, m, errs = ann(op='edit', id=pid, size=0.5, color='#ff0000')
        ar = m['arrows'][0] if m else {}
        check('W513: edit of a pointer by id changes its size and colour',
              ar.get('size') == 0.5 and ar.get('color') == '#ff0000' and ar.get('id') == pid and not errs, str((m, errs))[:300])
        _, m, errs = ann(op='move', id=kid, new_x=0.6, new_y=0.2)
        _, m, errs = ann(op='edit', id=kid, shape='plus', size=3, color='#0000ff')
        mk = m['markers'][0] if m else {}
        check('W513: move and edit of a marker by id change its position, shape, size and colour',
              mk.get('shape') == 'plus' and mk.get('size') == 3 and mk.get('color') == '#0000ff' and mk.get('id') == kid
              and near(mk.get('x'), 0.6, 1e-6) and near(mk.get('y'), 0.2, 1e-6) and not errs, str((m, errs))[:300])
        for what, fields, want in [('a shape it has not', dict(op='edit', id=kid, shape='star'), 'Invalid choice'),
                                   ('a negative size', dict(op='edit', id=pid, size=-1), 'at least 0'),
                                   ('an arrow with the tip on the tail', dict(op='add_arrow', tip_x=1, tip_y=1, tail_x=1, tail_y=1), 'the same point'),
                                   ('an arrow with no tail', dict(op='add_pointer', tip_x=1, tip_y=1), 'Missing argument "tail_x"'),
                                   ('a marker with no position', dict(op='add_marker', shape='box'), 'Missing argument "x"')]:
            _, m, errs = ann(**fields)
            check('W513: annotation refuses %s' % what, m is None and len(errs) == 1 and want in errs[0], str((m, errs)))
        evs, m, errs = ann(op='add_arrow', tip_x=0.1, tip_y=0.1, tail_x=0.2, tail_y=0.3, color='#00c8c8')
        new_arrow = result_of(evs).get('id', -1)
        evs, m, errs = ann(op='add_marker', shape='circle', x=0.5, y=0.5)
        new_marker = result_of(evs).get('id', -1)
        check('W513: add_arrow and add_marker report ids no other annotation has; the arrow keeps its look',
              len({tid, pid, kid, new_arrow, new_marker}) == 5 and m is not None and len(m['arrows']) == 2 and len(m['markers']) == 2
              and next((x for x in m['arrows'] if x['id'] == new_arrow), {}).get('kind') == 'arrow'
              and next((x for x in m['arrows'] if x['id'] == new_arrow), {}).get('color') == '#00c8c8', str((m, errs))[:300])
        _, m, errs = ann(op='delete', id=new_marker)
        _, m, errs = ann(op='delete', id=new_arrow)
        _, m, errs = ann(op='delete', id=7777)
        check('W513: delete by id removes only that arrow or marker; the others keep their ids',
              m is None and not [e for e in errs if 'no annotation' not in e], str(errs))
        evs, m, _ = ann(op='list')
        ls = result_of(evs)
        check('... the pointer, marker and text are as they were, with the same ids',
              [x['id'] for x in ls['text']] == [tid] and [x['id'] for x in ls['arrows']] == [pid]
              and [x['id'] for x in ls['markers']] == [kid], str(ls)[:300])
        # no caps: more than 50 texts and 400 arrows and markers
        ids = []
        for i in range(60):
            evs, _, _ = ann(op='add_text', text='t%d' % i, x=i / 100, y=0.5)
            ids += [result_of(evs).get('id', -1)]
        for i in range(450):
            evs, _, _ = ann(op='add_marker', shape='box', x=i / 1000, y=0.1) if i % 2 else ann(op='add_arrow', tip_x=0, tip_y=0, tail_x=i / 1000 + 0.1, tail_y=0.3)
            ids += [result_of(evs).get('id', -1)]
        evs, _, errs = ann(op='list')
        ls = result_of(evs)
        check('W513: no cap of 50 texts or 400 arrows and markers: 61 texts and 452 arrows and markers are kept, ids all different',
              len(ls['text']) == 61 and len(ls['arrows']) + len(ls['markers']) == 452 and len(set(ids)) == 510 and not errs,
              str((len(ls['text']), len(ls['arrows']), len(ls['markers']), len(set(ids)))))
        for i in ids:
            send6(cmd='annotation.delete', annotation_id=i)
            collect6(is_idle, timeout=30 * SLOW)
        evs, _, _ = ann(op='list')
        ls = result_of(evs)
        check('... and deleting them leaves the first three', [len(ls[k]) for k in ('text', 'arrows', 'markers')] == [1, 1, 1], str(ls)[:200])

        evs = after(dict(cmd='plot.freeze_curve', color='#ffcd00', draw='line', dash='solid', width=1, marker='circle', marker_size=3, key='first run', name='frz1'))
        m = marks_of(evs)
        f = m['frozen'] if m else []
        check('Graphic stuff/Freeze: a frozen curve equal to the series at freeze time, its key and colour',
              len(f) == 1 and f[0]['key'] == 'first run' and f[0]['name'] == 'frz1' and f[0]['color'] == '#ffcd00'
              and f[0]['draw'] == 'line' and f[0]['dash'] == 'solid' and f[0]['width'] == 1 and values({'data': f[0]['x']}, None) == cols.get(curve['x']) and values({'data': f[0]['y']}, None) == cols.get(curve['y'])
              and len(f[0]['x']) > 2, str(m)[:300])

        got = after({'cmd': 'equilibrium.find'})
        eq = next(iter(of(got, 'equilibrium')), None)
        m = marks_of(got)
        q = m['equilibria'] if m else []
        vals = [val for _, val in eq['values']] if eq else []
        check("Sing pts: an equilibrium mark at the equilibrium's values of the plotted variables",
              eq is not None and len(q) == 1 and near(q[0]['x'], vals[curve['x'] - 1], 1e-12)
              and near(q[0]['y'], vals[curve['y'] - 1], 1e-12), '%s vs %s' % (q, vals))
        check('... its type and symbol as the stability says',
              len(q) == 1 and q[0]['symbol'] == {'stable': 'circle', 'unstable': 'box', 'saddle': 'triangle'}[q[0]['type']]
              and (q[0]['type'] == 'stable') == (eq['cplus'] + eq['rplus'] == 0)
              and (q[0]['type'] == 'saddle') == (eq['cplus'] + eq['rplus'] > 0 and eq['cminus'] + eq['rminus'] > 0),
              '%s, %s' % (q, eq and eq['type']))
        check('... the other marks unchanged', m is not None and len(m['text']) == 1 and len(m['frozen']) == 1)

        evs = after({'cmd': 'window.redraw'})
        m = marks_of(evs)
        check('a redraw draws text, objects and frozen curves again, not the equilibrium (as the classic window)',
              m is not None and m['equilibria'] == [] and len(m['text']) == 1 and len(m['arrows']) == 1
              and len(m['markers']) == 1 and len(m['frozen']) == 1, str(m)[:300])
        evs = after({'cmd': 'window.redraw'})
        check('... and a second one sends nothing', not of(evs, 'marks'))

        evs = after({'cmd': 'events.subscribe', 'events': ['marks'], 'encoding': 'f32'})
        m = marks_of(evs)
        check('enc f32: the frozen curve as base64 float32, the same values',
              m is not None and m.get('enc') == 'f32' and len(m['frozen']) == 1 and bool(f)
              and same_floats(values({'data': m['frozen'][0]['x']}, 'f32'), values({'data': f[0]['x']}, None)),
              str(m)[:200])
        after({'cmd': 'events.subscribe', 'events': ['marks']})

        evs = after(dict(cmd='plot.delete_all_frozen'))
        m = marks_of(evs)
        check('Freeze/Remove all: the frozen curve goes at once', m is not None and m['frozen'] == []
              and len(m['text']) == 1, str(m)[:300])

        evs = after({'cmd': 'window.clear'})
        m = marks_of(evs)
        check('Erase clears every mark',
              m is not None and not any(m[k] for k in ('equilibria', 'text', 'arrows', 'markers', 'frozen')),
              str(m)[:300])
        evs = after({'cmd': 'window.redraw'})
        m = marks_of(evs)
        check('... a redraw brings the text and objects back', m is not None and len(m['text']) == 1
              and len(m['arrows']) == 1 and len(m['markers']) == 1, str(m)[:300])
        evs = after(dict(cmd='annotation.delete_all'))
        m = marks_of(evs)
        check('Text,etc/Delete all deletes them', m is not None and m['text'] == [] and m['arrows'] == []
              and m['markers'] == [], str(m)[:300])
        after({'cmd': 'events.unsubscribe', 'events': ['marks']})
        evs = after({'cmd': 'events.subscribe', 'events': ['series']})
        evs += after({'cmd': 'equilibrium.find'})
        check('not asked for: no marks', not of(evs, 'marks'))
    finally:
        stop_server(proc6, run6, send6)


def check_ani_data():
    """The animation as data (docs/protocol.md "The animation as data",
    docs/ui-v2.md T13): tools/gui_test.ani (one of every command) on lecar.
    Each frame event's primitives are in unit coordinates of the dimension
    box, colours as #rrggbb or foreground; the player's step, seek and Go
    send the frames they show, Go at most 25 a second and always its last
    one."""
    proc6, run6, send6, collect6, _ = launch_server()
    shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gui_test.ani'), run6)
    of = lambda evs, name: [e for e in evs if e.get('ev') == name]
    frames = lambda evs: [e for e in evs if e.get('ev') == 'ani' and e.get('op') == 'frame']
    states = lambda evs: [e for e in evs if e.get('ev') == 'ani' and 'op' not in e]

    def answered(pending):
        got = []
        while True:
            evs, e = collect6(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] in ('idle','done'):
                return got
            send6(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))


    ani_commands = {'go': ('animation.play', {}), 'reset': ('animation.reset', {}), 'step': ('animation.step', {'n': 'rows'}),
                    'seek': ('animation.seek', {'pos': 'row'}), 'speed': ('animation.set_speed', {})}
    def ani(op, *answers, **kw):
        if op == 'file': send6(cmd='animation.load', file=answers[0]['file'])
        elif op == 'grab': send6(cmd='animation.grab_point')
        else:
            name, renamed = ani_commands[op]
            send6(cmd=name, **{renamed.get(k,k):v for k,v in kw.items()})
        return answered([])

    named = {'foreground', '#ff0000', '#f06400', '#ffa500', '#ffcd00', '#c8c800', '#c8eb4b', '#00e100', '#00c8c8', '#0000ff', '#a020f0'}

    def colours_ok(f):
        """'' when every primitive's colour is `foreground` or a #rrggbb, else why"""
        for p in f['prims']:
            c = p[4] if p[0] in ('dot', 'text') else p[5]
            if not (c == 'foreground' or (isinstance(c, str) and len(c) == 7 and c[0] == '#')):
                return 'colour %r of %s' % (c, p)
        return ''

    def in_unit(f):
        """None when every coordinate is in [0,1] but a line's (a line may leave the box), else what"""
        for p in f['prims']:
            if p[0] == 'line':
                continue
            pts = [(p[1], p[2])] + ([(p[3], p[4])] if p[0] == 'rect' else [])
            for u, v in pts:
                if not (0 <= u <= 1 and 0 <= v <= 1):
                    return '%s at %s' % (p, (u, v))
        return None

    try:
        evs, _ = collect6(is_idle)
        hello = of(evs, 'hello')
        check('ani data: hello lists the ani feature', bool(hello) and 'ani' in hello[0].get('features', []))
        send6(cmd='events.subscribe', events=['ani'])
        evs, _ = collect6(is_idle)
        check('ani data: no frame before one is drawn', not frames(evs))
        send6(cmd='run.initial')
        collect6(is_idle)
        send6(cmd='animation.open')
        evs = answered([])
        check('ani data: animation.open opens the animation window',
              any(e.get('open') == 1 for e in states(evs)) or any(e.get('ev') == 'window' and e.get('win') == 'animation' and e.get('op') == 'create' for e in evs))
        evs = ani('file', {'file': 'gui_test.ani'})
        fr, st = frames(evs), states(evs)
        check('ani data: loading an animation shows its first frame', len(fr) == 1 and fr[0]['pos'] == 0
              and fr[0]['rows'] == 601 and bool(st) and st[-1]['loaded'] == 1, str(evs)[-400:])
        if not fr:
            return
        f0 = fr[0]
        native_path = os.path.join(run6, 'gui_test.anix')
        check('W520: opening .ani writes its native companion once', os.path.isfile(native_path))
        native_events = ani('file', {'file': 'gui_test.anix'})
        native_frames = frames(native_events)
        check('W520: the native animation reproduces the imported frame data',
              bool(native_frames) and native_frames[-1]['prims'] == f0['prims'], str(native_events)[-300:])
        with open(os.path.join(run6, 'bad.anix'), 'w') as f:
            f.write('options { speed: 7 }\nline { x1: 0, y1: 0, x2: absent, y2: 1 }\n')
        refused = ani('file', {'file': 'bad.anix'})
        errors = [e for e in refused if e.get('error')]
        check('W520: a refused native animation names its line and preserves the frame',
              len(errors) == 1 and errors[0].get('file') == 'bad.anix' and errors[0].get('line') == 2
              and not frames(refused) and all(e['speed'] == 5 for e in states(refused)), str(refused)[-600:])
        check('ani data: the frame names its box, window, speed and time',
              f0['dim'] == [-0.6, -0.1, 0.4, 0.6] and (f0['w'], f0['h']) == (280, 350) and f0['speed'] == 5
              and f0['skip'] == 1 and f0['t'] == 0, str({k: v for k, v in f0.items() if k != 'prims'}))
        kinds = sorted({p[0] for p in f0['prims']})
        check('ani data: every kind of primitive (line, rect, circle, ellipse, dot, text)',
              kinds == ['circle', 'dot', 'ellipse', 'line', 'rect', 'text'], str(kinds))
        check('ani data: frame 0 has a primitive per drawing command of the .ani, colours as #rrggbb or foreground',
              len(f0['prims']) == 13 and not colours_ok(f0), '%d primitives %s' % (len(f0['prims']), colours_ok(f0)))
        check('ani data: its coordinates are in [0,1] (only a line may leave the box)',
              in_unit(f0) is None, str(in_unit(f0)))
        spectral = [p for p in f0['prims'] if p[0] == 'circle' and p[5] not in named]
        check('ani data: a colour of the colour map is #rrggbb (fcircle ...;w)', len(spectral) == 1, str(spectral))

        evs = ani('step', n=1)
        fr = frames(evs)
        check('ani data: step sends the next frame, moved on from frame 0', len(fr) == 1 and fr[0]['pos'] == 1
              and fr[0]['t'] > 0 and fr[0]['prims'] != f0['prims'] and not colours_ok(fr[0]), str(fr)[:300])
        evs = ani('seek', pos=300)
        fr = frames(evs)
        bad = in_unit(fr[-1]) if fr else 'no frame'
        check('ani data: seek sends the frame it lands on, in unit coordinates',
              bool(fr) and fr[-1]['pos'] == 300 and bad is None, str(bad))
        evs = ani('speed', ms=20)
        check('ani data: speed sets the delay between frames', bool(states(evs)) and states(evs)[-1]['speed'] == 20,
              str(states(evs)))
        evs = ani('go')
        fr = frames(evs)
        # rows 300 to 600, 20 ms apart (6 s): at most 25 frames a second is far fewer than the 300 rows shown
        check('ani data: Go sends frames as it plays, at most 25 a second, and its last one',
              3 <= len(fr) < 300 and fr[-1]['pos'] == 600 and all(a['pos'] < b['pos'] for a, b in zip(fr, fr[1:])),
              '%d frames, last %s' % (len(fr), fr and fr[-1]['pos']))
        check('ani data: the last frame of Go is whole: in unit coordinates, its colours, the comets grown',
              bool(fr) and in_unit(fr[-1]) is None and not colours_ok(fr[-1])
              and len(fr[-1]['prims']) > len(f0['prims']),
              str(fr and (in_unit(fr[-1]), colours_ok(fr[-1]), len(fr[-1]['prims']))))

        # W177: one sequence choice, then derived PPM names, even across 601 rows.
        ani('reset')
        ani('speed', ms=1)
        send6(cmd='animation.export_frames', ppm=True, file='sequence.ppm')
        saved, file_asks = [], 0
        while True:
            evs, ask = collect6(lambda e: is_ask(e) or is_idle(e), timeout=60 * SLOW)
            saved += [e for e in evs if e.get('ev') == 'saved' and e.get('saved')]
            if not ask or is_idle(ask): break
            if ask.get('kind') == 'pixels':
                send6(cmd='answer', id=ask['id'], **pixels(ask))
            elif ask.get('kind') == 'file':
                file_asks += 1
                send6(cmd='answer', id=ask['id'], ok=0)
            else:
                send6(cmd='answer', id=ask['id'], key='y')
        check('W177: 601 animation PPM frames need no further destination picker',
              file_asks == 0 and len(saved) == 601 and saved[0]['file'] == 'sequence.ppm'
              and saved[-1]['file'] == 'sequence-frame600.ppm', str((file_asks, len(saved), saved[-1:])))

        # grab: the grab point's cross (the last two black lines), hit in unit coordinates
        evs = ani('grab')
        fr = frames(evs)
        check('ani data: Grab shows the grab points', bool(fr) and states(evs)[-1]['grab'] == 1, str(states(evs)))
        cross = [p for p in (fr[-1] if fr else f0)['prims'] if p[0] == 'line' and p[5] == 'foreground'][-2:]
        if len(cross) == 2:
            cx, cy = (cross[1][1] + cross[1][3]) / 2, (cross[1][2] + cross[1][4]) / 2
            xlo, ylo, xhi, yhi = f0['dim']
            send6(cmd='animation.grab_point', x=xlo + cx * (xhi - xlo), y=ylo + cy * (yhi - ylo))
            evs = answered([])
            done = [e for e in evs if e.get('ev') == 'done']
            check('ani data: animation.grab_point at the point in the animation\'s coordinates grabs it',
                  bool(states(evs)) and states(evs)[-1]['grab'] == 0 and done and done[-1].get('result') == {'grabbed': True},
                  str(states(evs)) + str(done))
        else:
            check('ani data: animation.grab_point at the point in the animation\'s coordinates grabs it', False,
                  'no grab cross drawn')

        send6(cmd='events.unsubscribe', events=ALL_EVENTS)
        collect6(is_idle)
        evs = ani('step', n=1)
        check('ani data: not asked for, no frame is sent', not frames(evs) and bool(states(evs)), str(evs)[:200])
    finally:
        stop_server(proc6, run6, send6)

def check_fit_retained_runs():
    """Window/Fit takes the bounds of the live data and of every retained run
    in 2D and in 3D alike: a first run that grows far past the live one is
    inside the fitted box."""
    zeq = "\nz'=a*z\ninit z=1"
    for dims, extra, plot in ((2, '', ''), (3, zeq, ', xp: x, yp: y, zp: z, axes: 3, zmin: 0, zmax: 10')):
        d = tempfile.mkdtemp(prefix='xppfit')
        ode = os.path.join(d, 'fit.odex')
        with open(ode, 'w') as f:
            f.write("par a=2\nx'=a*x\ny'=a*y\ninit x=1,y=1" + extra + "\noptions { integration: { total_time: 1, dt: .1 }, "
                    "plot: { xlo: 0, xhi: 1, ylo: 0, yhi: 1" + plot + " } }\n")
        proc, run, snd, col, _ = launch_server(ode=ode)

        try:
            col(is_idle, timeout=60 * SLOW)
            snd(cmd='events.subscribe', events=['series', 'plots'])
            col(is_idle)
            snd(cmd='run.initial')
            col(is_idle)
            snd(cmd='value.set_parameters', values={'a': 0.1})
            col(is_idle)
            snd(cmd='run.initial')
            col(is_idle)
            snd(cmd='window.fit'); evs,_=col(is_idle)
            snd(cmd='events.subscribe', events=['series', 'plots'])
            evs, _ = col(is_idle)
            w = next((x for e in evs if e.get('ev') == 'plots' for x in e['windows'] if x['win'] == 'graph_1'), {})
            box = w.get('box', {})
            grown = 7.3  # the first run ends near e^2, far past the live run's 1.1
            check('Fit includes a retained run that extends past the live one (%dD)' % dims,
                  box.get('ymax', 0) >= grown and (dims == 2 or (box.get('xmax', 0) >= grown and box.get('zmax', 0) >= grown)), str(box))
        finally:
            stop_server(proc, run, snd)
            shutil.rmtree(d, ignore_errors=True)


check_view()
check_view3d()
check_fit_retained_runs()
check_marks()
check_ani_data()
# AUTO's info strip and stability circle as data (docs/protocol.md "The AUTO
# diagram as data", `autoinfo`), and a grab answered with a point of the
# diagram's data (`point`). lecar's steady-state branch from its "hopf" fixed
# point, then the periodic branch from the Hopf point, as
# examples/recordings/lecar_auto.recx does it.
def auto_scratch(p):
    """AUTO's private scratch directories of server process p (xpp::files::make_temp_dir)"""
    tmp = tempfile.gettempdir() if os.name == 'nt' else (os.environ.get('TMPDIR') or '/tmp')
    pre = 'xppautoX-%d-' % p.pid
    return [os.path.join(tmp, d) for d in os.listdir(tmp) if d.startswith(pre)]


NUM = r'([-+]?\d\.\d+E[-+]\d+)'


def printed_stability(p):
    """(branch, point) -> the eigenvalues or multipliers AUTO printed for it
    (its fort.9, kept as <model>.d in its scratch directory), the last printing"""
    out = {}
    for d in auto_scratch(p):
        for f in glob.glob(os.path.join(d, '*.d')):
            with open(f) as fh:
                for line in fh:
                    m = re.match(r'\s*(\d+)\s+(\d+)\s+(Eigenvalue|Multiplier)\s*(\d+)\s*' + NUM + r'\s*' + NUM + r'\s*$',
                                 line)
                    if m:
                        key = (int(m.group(1)), int(m.group(2)))
                        if int(m.group(4)) == (1 if m.group(3) == 'Eigenvalue' else 0):  # the first of a printing
                            out[key] = []
                        out.setdefault(key, []).append([float(m.group(5)), float(m.group(6))])
    return out


def strip_matches(info, diag):
    """None when autoinfo's point is the diagram data's point it names (branch, point, the
    parameter as the diagram's x), else why not"""
    if not 0 <= info['point'] < len(diag):
        return 'point %d of %d' % (info['point'], len(diag))
    p = diag[info['point']]
    if (abs(p[0]), p[1]) != (info['br'], info['pt']):
        return 'br/pt %s vs %s' % (p[:2], (info['br'], info['pt']))
    x = info['par'][0]['value']
    if abs(p[3] - x) > 1e-6 * max(1, abs(x)):
        return 'x %r vs %s %r' % (p[3], info['par'][0]['name'], x)
    return None


def close_pairs(a, b, tol):
    """two lists of [re, im] the same within tol (relative to 1 or more), in any order"""
    if a is None or b is None or len(a) != len(b):
        return False
    rest = list(b)
    for x in a:
        k = next((i for i, y in enumerate(rest) if abs(x[0] - y[0]) <= tol * max(1, abs(y[0]))
                  and abs(x[1] - y[1]) <= tol * max(1, abs(y[1]))), None)
        if k is None:
            return False
        rest.pop(k)
    return True


def diagram_view_name(evs, view):
    if isinstance(view, str):
        return view
    names = next((e['names'] for e in reversed(evs) if e.get('ev') == 'diagram' and e.get('op') == 'views'), ['diagram_1'])
    return names[view] if view < len(names) else 'diagram_%d' % (view + 1)


def rebuild_diagram(evs, pts, view=0):
    """view `view`'s diagram data after these events: (br, pt, ty, x, y, y2, from) per point"""
    for e in evs:
        if e.get('ev') != 'diagram' or e.get('view') != diagram_view_name(evs, view):
            continue
        if e['op'] == 'reset':
            del pts[e['keep']:]
        elif e['op'] == 'add':
            del pts[e['from']:]
            for r in e['runs']:
                for i, x in enumerate(r['x']):
                    pts.append((r['br'], r['pt'] + i, r['ty'], x, r['y'][i], (r.get('y2') or r['y'])[i],
                                r.get('from', 0) if i == 0 else 0))
    return pts


def lecar_to_auto(snd, col):
    """Apply hopf, find its equilibrium and start AUTO through registry commands."""
    result=[]
    for command in [dict(cmd='set.apply',name='hopf',scope='all'),
                    dict(cmd='equilibrium.find'),
                    dict(cmd='equilibrium.import_as_ic'),dict(cmd='auto.open'),dict(cmd='auto.run',kind='equilibrium')]:
        snd(**command)
        evs,_=col(is_idle,timeout=60*SLOW)
        result.extend(evs)
    return result


def infos(evs):
    return [e for e in evs if e.get('ev') == 'autoinfo']


# XPP_NO_THROTTLE (core/xpp_job.h): every throttled flush happens, so the
# autoinfo events are each state the strip passes through on any machine,
# not the few a 0.1 s rate limit happens to catch (W49: a redraw's passage
# of the circle over every point showed only on a slow runner)
NO_THROTTLE = {'XPP_NO_THROTTLE': '1'}


def check_autoinfo():
    pa, ra, snda, cola, _ = launch_server(NO_THROTTLE)
    pb, rb, sndb, colb, _ = launch_server(NO_THROTTLE)
    try:
        cola(is_idle)
        colb(is_idle)
        snda(cmd='events.subscribe', events=['autoinfo'])
        evs, _ = cola(is_idle)
        check('autoinfo: sent at once after data, empty before AUTO',
              [(e['info'], e['stab']) for e in infos(evs)] == [(None, None)], str(infos(evs)))
        evs = lecar_to_auto(snda, cola)
        diag_a = rebuild_diagram(evs, [])
        runs = [r for e in evs if e.get('ev') == 'diagram' and e.get('op') == 'add' for r in e['runs']]
        check('W118: every diagram run says whether it is stable and periodic, as its ty (1..4) is',
              runs and all(r.get('stable') == (r['ty'] in (1, 3)) and r.get('periodic') == (r['ty'] in (3, 4))
                           for r in runs), str([(r['ty'], r.get('stable'), r.get('periodic')) for r in runs])[:300])
        got = infos(evs)
        stab = got[-1]['stab'] if got else None
        last = diag_a[-1] if diag_a else None
        pr = printed_stability(pa).get(last[:2]) if last else None
        check("autoinfo: after a run, the circle is its last point's (e^eigenvalue), as AUTO printed it",
              stab is not None and got[-1]['info'] is None and stab['periodic'] == 0 and len(stab['circle']) == 2
              and close_pairs(stab['eig'], pr, 2e-5), '%s vs %s' % (got[-1:], pr))
        evs = lecar_to_auto(sndb, colb)
        diag_b = rebuild_diagram(evs, [])
        check('autoinfo: a client that did not ask gets none', not infos(evs))

        snda(cmd='auto.grab')
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        got = infos(evs)
        info = got[-1]['info'] if got else None
        check("autoinfo: Grab sends the strip of the point under the cursor (the first), the diagram data's point",
              info is not None and info['point'] == 0 and info['br'] == 1 and info['pt'] == 1
              and strip_matches(info, diag_a) is None, info and strip_matches(info, diag_a) or str(got))
        check('W118: the strip says whether its point is stable and periodic, as its type is',
              info is not None and info.get('stable') == (info['type'] in (1, 3))
              and info.get('periodic') == (info['type'] in (3, 4)), str(info))
        snda(cmd='answer', id=ask['id'], key='Tab')
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        got = infos(evs)
        info, stab = (got[-1]['info'], got[-1]['stab']) if got else (None, None)
        check('autoinfo: Tab to the Hopf point: its strip', info is not None and info['sym'] == 'hopf'
              and info['lab'] > 0 and strip_matches(info, diag_a) is None, info and strip_matches(info, diag_a) or str(got))
        hb = info
        printed = printed_stability(pa)
        pr = printed.get((1, hb['pt'])) if hb else None
        check("autoinfo: the Hopf point's eigenvalues are what AUTO printed for it, a pair on the imaginary axis",
              stab is not None and stab['periodic'] == 0 and len(stab['circle']) == 2 and close_pairs(stab['eig'], pr, 2e-5)
              and all(abs(e[0]) < 1e-3 and abs(e[1]) > 0.1 for e in stab['eig']), '%s vs %s' % (stab, pr))
        check('autoinfo: the circle is e^eigenvalue', stab is not None and all(
            abs(complex(*z) - cmath.exp(complex(*e))) < 1e-9 for z, e in zip(stab['circle'], stab['eig'])))
        # a point of the data by its index moves the cursor there
        snda(cmd='answer', id=ask['id'], point=5)
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        got = infos(evs)
        info, stab = (got[-1]['info'], got[-1]['stab']) if got else (None, None)
        p5 = diag_a[5] if len(diag_a) > 5 else None
        pr = printed.get((info['br'], info['pt'])) if info else None
        check('grab by point: the cursor goes to point 5 of the data, its strip and eigenvalues',
              info is not None and p5 is not None and info['point'] == 5 and (info['br'], info['pt']) == p5[:2]
              and strip_matches(info, diag_a) is None and stab is not None and close_pairs(stab['eig'], pr, 2e-5),
              '%s %s %s' % (info, p5, pr))
        snda(cmd='answer', id=ask['id'], point=100000, key='Return')
        evs, ask = cola(lambda e: e.get('ev') == 'ask' or is_idle(e))
        check('grab by point: a point the data do not have is ignored, and so is its key',
              ask is not None and ask.get('ev') == 'ask' and not infos(evs), str(evs)[:300])
        # take the Hopf point by its index, in one answer; the other server by its keys
        snda(cmd='answer', id=ask['id'], point=hb['point'], key='Return')
        evs, end = cola(is_idle)
        check('grab by point: point and Return take it in one answer', end is not None
              and not any(e.get('ev') == 'ask' for e in evs), str(evs)[:300])
        sndb(cmd='auto.grab')
        for k in ['Tab', 'Return']:
            evs, ask = colb(lambda e: e.get('ev') == 'ask')
            sndb(cmd='answer', id=ask['id'], key=k)
        colb(is_idle)
        run_evs = []
        for snd, col, diag in ((snda, cola, diag_a), (sndb, colb, diag_b)):
            # W338: keep the circle fixture short. W341 (#402) also preserves
            # the restored solution's multipliers at a failed-step MX endpoint.
            snd(cmd='auto.configure_numerics', **{'max_steps': 20})
            evs, _ = col(is_idle)
            check('W338: the short periodic circle fixture is configured',
                  not any(e.get('ev') in ('message', 'done') and 'error' in e for e in evs), str(evs)[-300:])
            snd(cmd='auto.run', kind='periodic', label=hb['lab'])
            evs, _ = col(is_idle, timeout=120 * SLOW)
            rebuild_diagram(evs, diag)
            run_evs = run_evs or evs
        per = [p for p in diag_a if p[0] == 2]
        check('grab by point, then Run: the periodic branch is the one grabbing by keys gives',
              len(per) == 20 and diag_a == diag_b, '%d periodic points; %d vs %d points' % (len(per), len(diag_a), len(diag_b)))
        check('diagram: the periodic branch says it started from the Hopf label (from)',
              per and per[0][6] == hb['lab'] and all(p[6] == 0 for p in per[1:]), str(per[:2]))
        got = infos(run_evs)
        stab = got[-1]['stab'] if got else None
        pr = printed_stability(pa).get(per[-1][:2]) if per else None
        check("autoinfo: after the periodic run, the circle holds its last point's Floquet multipliers, as AUTO printed them",
              stab is not None and stab['periodic'] == 1 and 'eig' not in stab and close_pairs(stab['circle'], pr, 2e-5)
              and any(abs(complex(*z) - 1) < 1e-3 for z in stab['circle']), '%s vs %s' % (stab, pr))
        snda(cmd='window.redraw')
        evs, _ = cola(is_idle)
        check('autoinfo: a redraw sends none (nothing it shows changed, nor on its way)', not infos(evs),
              '%d events, the first %s' % (len(infos(evs)), str(infos(evs)[:1])[:300]))
        # a redraw after a grab: the strip and the circle stay the grabbed point's (W49: the circle
        # used to move to the diagram's last point, under the grabbed point's info)
        snda(cmd='auto.grab')
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        snda(cmd='answer', id=ask['id'], point=5, key='Return')
        evs, _ = cola(is_idle)
        snda(cmd='window.redraw')
        evs, _ = cola(is_idle)
        sent = infos(evs)
        snda(cmd='events.subscribe', events=['autoinfo'])  # sends what it holds, changed or not
        evs, _ = cola(is_idle)
        got = infos(evs)
        info, stab = (got[-1]['info'], got[-1]['stab']) if got else (None, None)
        pr = printed.get((info['br'], info['pt'])) if info else None
        check("autoinfo: a redraw after a grab sends none, and the strip and circle stay the grabbed point's",
              not sent and info is not None and info['point'] == 5 and strip_matches(info, diag_a) is None
              and stab is not None and stab['periodic'] == 0 and close_pairs(stab['eig'], pr, 2e-5),
              '%d sent; %s %s vs %s' % (len(sent), info, stab, pr))
        snda(cmd='auto.pick', x=0.125, y=-0.25)
        evs, _ = cola(is_idle)
        hint = [e.get('auto') for e in evs if e.get('ev') in ('message', 'done') and 'auto' in e]
        check('auto point in data coordinates shows exactly those', hint and hint[-1] == 'x=0.125,y=-0.25', str(hint))
        snda(cmd='window.destroy', window='diagram_1')
        evs, _ = cola(is_idle)
        got = infos(evs)
        check('autoinfo: closing AUTO empties it', got and got[-1]['info'] is None and got[-1]['stab'] is None, str(got))
    finally:
        stop_server(pa, ra, snda)
        stop_server(pb, rb, sndb)


check_autoinfo()


# W151: a file dialog starts in the model's folder, never in AUTO's scratch
# folder: the ask's `dir` (the process's current folder) after an AUTO run
def check_dialog_folder_after_auto():
    p, r, snd, col, _ = launch_server(NO_THROTTLE)
    try:
        col(is_idle)
        want = os.path.realpath(r)

        def ask_dir(keys):
            snd(cmd='recording.open', play=False, file_picker=True)
            evs, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30*SLOW)
            directory = ask.get('dir') if ask and is_ask(ask) else None
            if directory is not None:
                snd(cmd='answer', id=ask['id'], ok=0)
                col(is_idle)
            return ask, directory
        ask, d = ask_dir(['f', 'y'])
        check('W151: a file ask before AUTO starts in the model folder',
              ask and d and os.path.realpath(d) == want, str(d))
        lecar_to_auto(snd, col)
        scratch = auto_scratch(p)
        ask, d = ask_dir(['f', 'y'])
        check('W151: a file ask after an AUTO run still starts in the model folder, not in AUTO scratch folder',
              ask and scratch and d and os.path.realpath(d) == want and
              not any(os.path.realpath(d).startswith(os.path.realpath(x)) for x in scratch), str(d) + ' ' + str(scratch))
        # a replay runs in a scratch folder of its own: its dialogs still start in the model's
        snd(cmd='recording.start')
        col(is_idle, timeout=30 * SLOW)
        snd(cmd='recording.add_note', note='note')
        col(is_idle, timeout=30 * SLOW)
        snd(cmd='recording.stop', file='dlg')
        col(is_idle, timeout=30 * SLOW)
        snd(cmd='recording.open', play=False, file='dlg.recx')
        evs, ask = col(lambda e: e.get('ev') == 'ask' or is_idle(e), timeout=30 * SLOW)
        if ask and ask.get('ev') == 'ask':
            snd(cmd='answer', id=ask['id'], key='d')
            col(is_idle, timeout=30 * SLOW)
        ask, d = ask_dir(['f', 'y'])
        scratch = auto_scratch(p)
        check('W151: a file ask in a replayed recording starts in the folder of the recording, not in the replay scratch folder',
              ask and scratch and d and os.path.realpath(d) == want and
              not any(os.path.realpath(d).startswith(os.path.realpath(x)) for x in scratch), str(d) + ' ' + str(scratch))
        # a later dialog of one kind starts where the last of its kind was answered
        sub = os.path.join(r, 'sub')
        os.mkdir(sub)
        snd(cmd='recording.open', play=False, file_picker=True)
        _, ask = col(lambda e: e.get('ev') == 'ask' and e.get('kind') == 'file', timeout=30 * SLOW)
        snd(cmd='answer', id=ask['id'], file=os.path.join(sub, 'x.recx'), replace=1)
        col(is_idle, timeout=30 * SLOW)
        ask, d = ask_dir(['f', 'y'])
        check('W151: a file dialog starts in the folder of the file of its kind last chosen',
              ask and d and os.path.realpath(d) == os.path.realpath(sub), str(d))
    finally:
        stop_server(p, r, snd)


check_dialog_folder_after_auto()


# AUTO's settings as data (docs/protocol.md "AUTO's settings as data", T22):
# the autosettings event holds what the AUTO forms show, and `auto` `set`
# writes the same fields, checked, all or nothing.
def settings_of(evs):
    got = [e for e in evs if e.get('ev') == 'autosettings']
    return got[-1] if got else None


NUM_KEYS = ['mesh_intervals', 'max_steps', 'output_every', 'collocation_points', 'step_size', 'min_step_size', 'max_step_size', 'parameter_min', 'parameter_max', 'norm_min', 'norm_max', 'parameter_tolerance', 'state_tolerance', 'special_point_tolerance',
            'mesh_adapt_every', 'max_branch_switches', 'switch_one_direction', 'output_detail', 'max_location_iterations', 'max_newton_iterations', 'full_newton_after', 'step_adapt_every', 'suppress_branch_points']


def check_autosettings():
    pa, ra, snda, cola, _ = launch_server()
    pb, rb, sndb, colb, _ = launch_server()
    try:
        cola(is_idle)
        colb(is_idle)

        def auto_errors(events):
            return [e['error'] for e in events if e.get('ev') in ('message', 'done') and 'error' in e]

        def auto_set(**c):
            """the old one-command set as the registry's commands (marks, then the Numerics and the axes), stopping at an error"""
            commands = []
            marks = c.get('marks')
            if isinstance(marks, list) and all(isinstance(m, list) and len(m) == 2 for m in marks):
                commands.append(dict(cmd='auto.configure_marks', parameters=[m[0] for m in marks], values=[m[1] for m in marks]))
            elif 'marks' in c:
                commands.append(dict(cmd='auto.configure_marks', marks=marks))
            numerics = dict(c.get('numerics', {}))
            if 'special_points' in c:
                numerics['special_points'] = c['special_points']
            if 'numerics' in c or 'special_points' in c:
                commands.append(dict(cmd='auto.configure_numerics', **numerics))
            if 'axes' in c:
                rename = {'plot': 'plot_type', 'var': 'variable', 'par1': 'parameter1', 'par2': 'parameter2', 'xmin': 'x_min', 'xmax': 'x_max', 'ymin': 'y_min', 'ymax': 'y_max'}
                commands.append(dict(cmd='auto.configure_plot', **{rename.get(k, k): v for k, v in c['axes'].items()}))
            if 'pars' in c:
                commands.append(dict(cmd='auto.configure_plot', pars=c['pars']))
            if 'threads' in c:
                commands.append(dict(cmd='auto.set_threads', **({'automatic': True} if c['threads'] == 'auto' else {'threads': c['threads']})))
            evs = []
            for command in commands:
                snda(**command)
                more, _ = cola(is_idle)
                evs += more
                if auto_errors(more):
                    break
            return evs, settings_of(evs), auto_errors(evs)

        snda(cmd='events.subscribe', events=['autosettings'])
        evs, _ = cola(is_idle)
        st = settings_of(evs)
        check('autosettings: sent at once after data, before AUTO is open', st is not None
              and sorted(st['numerics']) == sorted(NUM_KEYS) and 'pars' not in st
              and st['axes']['par1'] == 'iapp', str(st)[:300])
        rules = st.get('rules', {}) if st else {}
        check('W118: autosettings carries each Numerics rule with the message its refusal says, the pairs and the step',
              sorted(rules) == sorted(NUM_KEYS) and rules['collocation_points'].get('min') == 2 and rules['collocation_points'].get('max') == 7
              and isinstance(st['numerics']['suppress_branch_points'], bool) and rules['suppress_branch_points'].get('boolean') is True
              and rules['collocation_points'].get('integer') is True and rules['min_step_size'].get('positive') is True
              and rules['step_size'].get('nonzero') is True and rules['max_branch_switches'].get('min') == 0
              and rules['output_detail'].get('choices') == ['none', 'little', 'normal', 'start_jacobian', 'extensive', 'debugging']
              and rules['collocation_points'].get('message') == 'Collocation points must be a whole number from 2 to 7'
              and [(p['lo'], p['hi'], p['strict']) for p in st.get('pairs', [])]
              == [('min_step_size', 'max_step_size', False), ('parameter_min', 'parameter_max', True), ('norm_min', 'norm_max', True)]
              and st.get('step', {}).get('key') == 'step_size', str(st and (st.get('pairs'), st.get('step')))[:300])
        point_keys = ('bogdanov_takens', 'cusp', 'zero_hopf', 'generalized_hopf', 'resonance_1_1', 'resonance_1_2', 'resonance_1_3', 'resonance_1_4', 'fold_flip', 'fold_neimark_sacker', 'flip_neimark_sacker', 'double_neimark_sacker')
        check('W364: named detection booleans enabled by default', st.get('special_points') == dict.fromkeys(point_keys, True))
        for value in (False, True):
            events, current, errors = auto_set(special_points={'generalized_hopf': value})
            check('W364: registry applies GH '+str(value), not errors and current['special_points']['generalized_hopf'] is value)
        snda(cmd='events.subscribe', events=['hello', 'autosettings'])
        window_events, _ = cola(is_idle)
        window_hello = next((e for e in window_events if e.get('ev') == 'hello'), {})
        check('W339: AUTO has no Parameter form or pars field', 'pars' not in st
              and 'param' not in str(window_hello.get('windows', {}).get('auto', {})))
        evs, unchanged, errs = auto_set(pars=['iapp'])
        check('W339: removed pars command is rejected atomically', len(errs)==1 and 'pars' in errs[0] and unchanged is None)
        evs, all_axes, errs = auto_set(axes={'par1':'gk'})
        check('W339: Axes accepts a parameter beyond the old eight', not errs and all_axes and all_axes['axes']['par1']=='gk')
        auto_set(axes={'par1':'iapp'})
        check('autosettings: no Mark values at first', st['marks'] == [])
        evs, st2, errs = auto_set(numerics={'output_every': 40})
        check('autosettings: typed numerics emits current values', not errs and st2['numerics']['output_every'] == 40)

        # the set: every part, then read back through the forms
        p0, p1 = 'iapp', 'gca'
        evs, st3, errs = auto_set(numerics={'max_steps': 12, 'step_size': -0.01}, axes={'plot': 'norm', 'var': 'W', 'par1': p1},
                                  marks=[[p0, 0.25], ['T', 30]])
        check('auto set: numerics, pars, axes and marks set, the event says so', not errs and st3
              and st3['numerics']['max_steps'] == 12 and st3['numerics']['step_size'] == -0.01
              and st3['axes']['plot'] == 'norm' and st3['axes']['var'] == 'W' and st3['axes']['par1'] == p1
              and st3['marks'] == [[p0, 0.25], ['T', 30]], '%s %s' % (errs, st3))
        # W331: protocol and legacy forms agree beyond all previous mark capacities.
        many_marks = [['T', 100 + i] for i in range(128)]
        evs, many, errs = auto_set(marks=many_marks)
        check('W331: protocol sets 128 marks', not errs and many and many['marks'] == many_marks)
        for bad_marks in (many_marks[:-1] + [['unknown', 0]], 'bad', [['T', 1, 2]]):
            evs, changed, errs = auto_set(numerics={'max_steps': 99}, marks=bad_marks)
            check('W331: bad mark command rejects its numerics too', len(errs) == 1 and changed is None)
        snda(cmd='events.subscribe', events=['autosettings'])
        unchanged = settings_of(cola(is_idle)[0])
        check('W331: settings stayed intact after bad forms and sets',
              unchanged and unchanged['marks'] == many_marks and unchanged['numerics']['max_steps'] == 12)
        auto_set(marks=st3['marks'])

        # refused: bad values change nothing, and say why
        for bad, why in (({'numerics': {'suppress_branch_points': 1}}, 'must be boolean'), ({'numerics': {'suppress_branch_points': 'true'}}, 'must be boolean'),
                         ({'numerics': {'collocation_points': 9}}, 'Collocation points'), ({'numerics': {'mesh_intervals': 1.5}}, 'mesh_intervals'),
                         ({'numerics': {'max_steps': 30, 'min_step_size': 0}}, 'Min step size'), ({'numerics': {'parameter_min': 5, 'parameter_max': 1}}, 'Parameter min'),
                         ({'numerics': {'step_size': -0.9, 'max_step_size': 0.5}}, 'The size of Step size must be from Min step size to Max step size'),
                         ({'numerics': {'min_step_size': 0.5, 'max_step_size': 1, 'step_size': 0.1}}, 'The size of Step size must be from Min step size to Max step size'),
                         ({'numerics': {'max_steps': 'many'}}, 'max_steps'), ({'pars': ['nosuch']}, 'pars'),
                         ({'axes': {'par1': 'nosuch'}}, 'nosuch'),
                         ({'axes': {'var': 'nosuch'}}, 'nosuch'), ({'axes': {'plot': 'sideways'}}, 'sideways'), ({'axes': {'plot': 7}}, 'plot_type'),
                         ({'axes': {'xmin': 1, 'xmax': 0}}, 'Xmin'), ({'marks': [['W', 1]]}, 'W')):
            evs, st4, errs = auto_set(**bad)
            check('auto set %s is refused with an error naming %s, nothing changes' % (json.dumps(bad), why),
                  len(errs) == 1 and why in errs[0] and st4 is None, '%s %s' % (errs, st4))
        evs, boolean_set, errs = auto_set(numerics={'suppress_branch_points': True})
        check('W499: SuppBP accepts and publishes a boolean',
              not errs and boolean_set and boolean_set['numerics']['suppress_branch_points'] is True)
        auto_set(numerics={'suppress_branch_points': False})
        evs, st4, errs = auto_set(numerics={'collocation_points': 9})
        check('W118: a refused value says the message the event gave its rule',
              errs == ['collocation_points=9: ' + rules.get('collocation_points', {}).get('message', '?')], str(errs))

        # a set changes the next run: Nmax 12 against the default on the other server
        evs, st6, errs = auto_set(numerics={'max_steps': 12}, axes={'plot': 'maximum_and_minimum', 'var': 'V', 'par1': p0}, marks=[])
        evs_a = lecar_to_auto(snda, cola)
        evs_b = lecar_to_auto(sndb, colb)
        na, nb = len(rebuild_diagram(evs_a, [])), len(rebuild_diagram(evs_b, []))
        check('auto set: Nmax 12 makes the next run stop at 12 points (the default goes on to %d)' % nb,
              not errs and na == 12 and nb > 12, '%s: %d vs %d points' % (errs, na, nb))
        # W155: Reload keeps AUTO's settings without a settings file.
        before_reload = settings_of(auto_set(numerics={'max_steps': 41}, marks=many_marks)[0])
        snda(cmd='session.reload_model')
        reload_ask = cola(lambda e: e.get('ev') == 'ask')[1]
        snda(cmd='answer', id=reload_ask['id'], key='d')
        cola(is_idle)
        snda(cmd='events.subscribe', events=['autosettings'])
        after_reload = settings_of(cola(is_idle)[0])
        check('W155: Reload keeps AUTO numerics, parameters, axes and marks without a settings file',
              before_reload is not None and after_reload is not None
              and all(before_reload[k] == after_reload[k] for k in ('numerics', 'axes', 'marks')),
              str(after_reload))

    finally:
        stop_server(pa, ra, snda)
        stop_server(pb, rb, sndb)


check_autosettings()


# Why a branch ended (T23, docs/protocol.md "The AUTO diagram as data",
# autoinfo's "stop"): tools/models/auto_stop.odex's line of steady states run
# into each limit in turn, one fresh server each, and a Stop.
STOP_ODE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'auto_stop.odex')


def auto_stop_run(numerics, abort=False):
    """a steady-state run of auto_stop.odex with these Numerics: (the last autoinfo's stop, the run's points)"""
    p, r, snd, col, _ = launch_server(ode=STOP_ODE)
    try:
        col(is_idle)
        snd(cmd='events.subscribe', events=['autoinfo'])
        col(is_idle)
        snd(cmd='auto.open')
        col(is_idle)
        snd(cmd='auto.configure_numerics', **numerics)
        evs, _ = col(is_idle)
        errs = [e['error'] for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        if errs: return {'errors': errs}, 0
        snd(cmd='auto.run', kind='equilibrium')
        evs = []
        if abort:
            evs, _ = col(lambda e: e.get('ev') == 'diagram' and e.get('op') == 'add', timeout=30 * SLOW)
            snd(cmd='run.abort')
        more, _ = col(is_idle, timeout=60 * SLOW)
        evs += more
        got = infos(evs)
        return (got[-1].get('stop') if got else None), len(rebuild_diagram(evs, []))
    finally:
        stop_server(p, r, snd)


def check_auto_stop():
    wide = {'parameter_min': -0.5, 'parameter_max': 0.9, 'norm_min': -1, 'norm_max': 100, 'max_steps': 200}
    for numerics, why, text, abort in (
            ({**wide, 'parameter_max': 0.5}, 'parameter_max', 'parameter a reached parameter_max (0.5)', False),
            ({**wide, 'parameter_min': -0.3, 'step_size': -0.01}, 'parameter_min', 'parameter a reached parameter_min (-0.3)', False),
            ({**wide, 'norm_max': 0.3}, 'norm_max', 'the norm reached norm_max (0.3)', False),
            ({**wide, 'max_steps': 5}, 'max_steps', 'the branch reached max_steps (5)', False),
            ({**wide, 'parameter_max': 10}, 'noconv-min', 'no convergence even at the smallest step (min_step_size 0.001; max_newton_iterations 7, parameter_tolerance 0.0001, state_tolerance 0.0001)', False),
            ({**wide, 'parameter_max': 10, 'step_adapt_every': 0}, 'noconv-fixed', 'no convergence with a fixed step size (step_adapt_every 0; max_newton_iterations 7, parameter_tolerance 0.0001, state_tolerance 0.0001)', False),
            ({**wide, 'max_steps': 1000000, 'step_size': 1e-7, 'min_step_size': 1e-8, 'max_step_size': 1e-7}, 'user', 'by the user (Stop)', True)):
        stop, n = auto_stop_run(numerics, abort)
        ok = isinstance(stop, dict) and stop.get('why') == why and stop.get('text') == text and stop.get('br') == 1
        if ok and why in ('parameter_max', 'parameter_min', 'norm_max'):
            ok = abs(stop['limit'] - numerics[why]) < 1e-9 and (
                stop['value'] < stop['limit'] if why == 'parameter_min' else stop['value'] > stop['limit'])
        if ok and why == 'max_steps':
            ok = stop['pt'] == 5 and n == 5
        if ok and why == 'user':
            ok = 1 < stop['pt'] < 1000000
        check('autoinfo stop: %s ends the branch with "%s"' % (why, text), ok, '%s (%d points)' % (stop, n))


check_auto_stop()


def check_auto_points_limit():
    """W432 (#494): Nmax is a loop bound, not an allocation: 10^7 (XPPAUT models
    mean "no limit" by it) is accepted; the bound is where the diagram stores
    points (XPP_DIAGRAM_POINTS_LIMIT lowers it): the run stops with one error
    naming the limit and keeps the points before it."""
    p, r, snd, col, _ = launch_server(ode=STOP_ODE, extra_env={'XPP_DIAGRAM_POINTS_LIMIT': '7'})
    try:
        col(is_idle)
        snd(cmd='auto.open')
        col(is_idle)
        snd(cmd='auto.configure_numerics', **{'parameter_min': -0.5, 'parameter_max': 0.9, 'norm_min': -1, 'norm_max': 100, 'max_steps': 10000000})
        evs, _ = col(is_idle)
        check('W432: Nmax 10000000 is accepted', not [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e], str(evs[-3:]))
        snd(cmd='auto.run', kind='equilibrium')
        evs = []
        more, _ = col(is_idle, timeout=60 * SLOW)
        evs += more
        errs = [e for e in evs if 'error' in e]
        points = rebuild_diagram(evs, [])
        check('W432: a diagram at its point limit stops the run with one error naming the limit, its points kept',
              len(errs) == 1 and 'holds 7 points' in errs[0]['error'] and len(points) == 7,
              str(errs) + ' %d points' % len(points))
    finally:
        stop_server(p, r, snd)


check_auto_points_limit()


def check_auto_no_nan_par():
    """W35a (issue #73, QA SCI-001): a periodic-continuation script left the
    model's parameter at -nan after a run that never converged, which made
    the next "state" event invalid JSON. auto_stop.odex's 'noconv-min' run
    (above: Par Max far out of reach, so AUTO never converges even at its
    smallest step) is the same failure mode, small and fast. Every line
    from the server must still parse as strict JSON (a non-finite constant
    is rejected: json.loads' parse_constant, launch_server's reader), and
    the run's last "state" must show the continuation parameter 'a'
    finite, not left over from a diverged Newton step (core/auto_nox.cpp's
    auto_restore_finite_pars)."""
    wide = {'parameter_min': -0.5, 'parameter_max': 0.9, 'norm_min': -1, 'norm_max': 100, 'max_steps': 200}
    p, r, snd, col, evq = launch_server(ode=STOP_ODE)
    try:
        col(is_idle)
        snd(cmd='events.subscribe', events=['autoinfo'])
        col(is_idle)
        snd(cmd='auto.open')
        col(is_idle)
        snd(cmd='auto.configure_numerics', **{**wide, 'parameter_max': 10})
        col(is_idle)
        snd(cmd='auto.run', kind='equilibrium')
        evs, _ = col(is_idle, timeout=60*SLOW)
        bad = [e for e in evs if e.get('ev') == 'bad']
        states = [e for e in evs if e.get('ev') == 'state']
        pars = states[-1].get('pars', []) if states else []
        ok = not bad and bool(states) and all(math.isfinite(v) for _,v in pars)
        detail = str(bad or pars)
        check('AUTO: a run with no convergence parses strictly and leaves every par finite', ok, detail)
    finally:
        stop_server(p, r, snd)


check_auto_no_nan_par()


# Save data (W52, docs/protocol.md "The data browser"): the browser's
# write with `what`, `format` and `name` writes the data table or what the
# plot shows in each registered format; each file is read back here, the
# NPZ by parsing its zip and .npy headers (and by numpy.load when numpy is
# there), and the browser's load reads each back into the table.
def npz_arrays(path):
    """name -> (shape, float64 values) of an .npz, read without numpy"""
    import ast, zipfile
    out = {}
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            b = z.read(info)
            assert b[:6] == b'\x93NUMPY' and b[6] == 1
            hlen = struct.unpack('<H', b[8:10])[0]
            h = ast.literal_eval(b[10:10 + hlen].decode('latin1'))
            assert h['descr'] == '<f8' and not h['fortran_order'] and (10 + hlen) % 64 == 0
            n = len(b[10 + hlen:]) // 8
            out[info.filename[:-4]] = (h['shape'], struct.unpack('<%dd' % n, b[10 + hlen:]))
    return out


def check_data_formats():
    import gzip
    p, r, snd, col, _ = launch_server()
    f32 = lambda v: struct.unpack('<f', struct.pack('<f', float(v)))[0]
    read = lambda d, n: open(os.path.join(d, n), 'rb').read() if os.path.exists(os.path.join(d, n)) else b''

    def browser(cmd, **kw):
        """a browser command, a File exists? Overwrite ask answered yes"""
        snd(cmd=cmd, **kw)
        got = []
        while True:
            evs, e = col(lambda e: e.get('ev') == 'ask' or is_idle(e), timeout=20 * SLOW)
            got += evs
            if e is None or e.get('kind') != 'choice':
                return got, e
            snd(cmd='answer', id=e['id'], key='y')

    try:
        col(is_idle)
        live_run(snd, col)
        for fmt, name in [('dat', 'd.dat'), ('csv', 'd.csv'), ('csv.gz', 'd.csv.gz'), ('npz', 'd.npz')]:
            _, e = browser('browser.export', target='table', format=fmt, file=name)
            check('Save data writes the table as %s without asking' % fmt,
                  e is not None and is_idle(e) and os.path.exists(os.path.join(r, name)), str(e))
        silent = tempfile.mkdtemp(prefix='xppsilent')
        copy_model(args.ode, silent)
        subprocess.run([os.path.abspath(args.server), os.path.basename(args.ode), '--silent'], cwd=silent,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60 * SLOW)
        want = read(silent, 'output.dat').replace(b'\r', b'')
        shutil.rmtree(silent, ignore_errors=True)
        check("Save data as .dat is byte for byte the --silent run's output.dat",
              read(r, 'd.dat').replace(b'\r', b'') == want, '%d vs %d bytes' % (len(read(r, 'd.dat')), len(want)))
        rows = [[float(x) for x in l.split()] for l in want.decode().splitlines() if l.strip()]
        csv = read(r, 'd.csv').decode().splitlines() or ['']
        # W71: a run's own seed is a "# seed N" comment line ahead of the
        # header (every run gets one, stochastic or not: the run is what
        # is reproducible, whether or not this model itself uses it)
        seed_line = csv[0] if csv and csv[0].startswith('# seed ') else None
        if seed_line is not None:
            csv = csv[1:]
        check('Save data as CSV: a leading "# seed N" comment names the run',
              seed_line is not None and seed_line[7:].isdigit(), str(seed_line))
        header = csv[0].split(',')
        vals = [[float(x) for x in l.split(',')] for l in csv[1:]]
        check('Save data as CSV: a header of the column names, then every row',
              header[:3] == ['T', 'V', 'W'] and rows and len(header) == len(rows[0]) and len(vals) == len(rows) == 601,
              '%s, %d rows' % (header, len(vals)))
        check("CSV's values are the stored floats (output.dat's %.8g of them)",
              all('%.8g' % f32(a) == '%.8g' % b for ra, rb in zip(vals, rows) for a, b in zip(ra, rb)))
        check('Save data as CSV.gz is the CSV, gzipped', gzip.decompress(read(r, 'd.csv.gz')) == read(r, 'd.csv'))
        try:
            arrays = npz_arrays(os.path.join(r, 'd.npz'))
            seed_array = arrays.pop('seed', None)
            ok = list(arrays) == header and all(
                arrays[n][0] == (601,) and all(v == f32(x[j]) for v, x in zip(arrays[n][1], vals))
                for j, n in enumerate(header))
            ok = ok and seed_array is not None and seed_array[0] == (1,) and int(seed_array[1][0]) == int(seed_line[7:])
            detail = str({k: v[0] for k, v in arrays.items()})
        except Exception as ex:  # any parse failure is the check's failure
            ok, detail = False, repr(ex)
        check('Save data as NPZ: one float64 array per column, named after it, plus a "seed" one', ok, detail)
        try:
            import numpy
        except ImportError:
            numpy = None
            print('SKIP numpy.load reads the NPZ: numpy is not installed for %s' % sys.executable)
        if numpy is not None:
            z = numpy.load(os.path.join(r, 'd.npz'))
            check('numpy.load reads the NPZ', sorted(z.files) == sorted(header + ['seed']) and z['V'].dtype == numpy.float64
                  and list(z['V']) == [f32(x[1]) for x in vals], str(z.files))

        # what the plot shows: the curve V against W, one row per point
        browser('browser.export', target='plot_curves', format='csv', file='p.csv')
        pc = read(r, 'p.csv').decode().splitlines() or ['']
        if pc and pc[0].startswith('# seed '):
            pc = pc[1:]
        check('Save data writes what the plot shows: curve,x,y, a row per point',
              pc[0] == 'curve,x,y' and len(pc) == 602 and pc[1].split(',')[0] == '1'
              and [float(x) for x in pc[1].split(',')[1:]] == vals[0][1:3], str(pc[:2]))
        browser('browser.export', target='plot_curves', format='npz', file='p.npz')
        try:
            arrays = npz_arrays(os.path.join(r, 'p.npz'))
            arrays.pop('seed', None)
            ok, detail = list(arrays) == ['curve1'] and arrays['curve1'][0] == (601, 2), str(arrays.keys())
        except Exception as ex:
            ok, detail = False, repr(ex)
        check('what the plot shows as NPZ: one (points, 2) array per curve', ok, detail)

        evs, _ = browser('browser.export', target='plot_curves', format='csv.gz', file='asked.csv.gz')
        check('Save data exposes every format through its registry choices and writes gzip',
              gzip.decompress(read(r, 'asked.csv.gz')) == read(r, 'p.csv'))

        evs, _ = browser('browser.export', target='plot_curves', format='csv', file='export.csv')
        check('Plot export writes what the graph shows', read(r, 'export.csv') == read(r, 'p.csv'))

        # Load reads each format back into the table, picked by its extension
        for name in ['d.csv', 'd.csv.gz', 'd.npz', 'd.dat']:
            snd(cmd='browser.set_slice_start', row=300)
            col(is_idle)
            browser('browser.export', target='table', format='dat', file='cut.dat')  # rows 300.. only
            browser('browser.load', file='cut.dat')
            evs, _ = browser('browser.load', file=name)
            snd(cmd='browser.set_slice_start', row=0)
            col(is_idle)
            browser('browser.export', target='table', format='dat', file='back.dat')
            check('Load reads %s back into the table (rows and values)' % name,
                  read(r, 'back.dat').replace(b'\r', b'') == want and last_state(evs).get('rows') == 601,
                  str(last_state(evs).get('rows')))

        # W504: a hostile data file is refused whole, naming the file and why
        import zipfile
        def npy(shape, payload=b''):
            h = ("{'descr': '<f8', 'fortran_order': False, 'shape': %s, }" % shape).encode()
            h += b' ' * ((64 - (10 + len(h) + 1) % 64) % 64) + b'\n'
            return b'\x93NUMPY\x01\x00' + struct.pack('<H', len(h)) + h + payload
        with zipfile.ZipFile(os.path.join(r, 'evil_cols.npz'), 'w') as z:
            z.writestr('a.npy', npy('(0, 18446744073709551615)'))
        with zipfile.ZipFile(os.path.join(r, 'evil_dim.npz'), 'w') as z:
            z.writestr('a.npy', npy('(1, 99999999999999999999999)', b'\0' * 8))
        with zipfile.ZipFile(os.path.join(r, 'evil_seed.npz'), 'w') as z:
            z.writestr('seed.npy', npy('(1,)', struct.pack('<d', 1e300)))
        with gzip.open(os.path.join(r, 'bomb.csv.gz'), 'wb', compresslevel=1) as g:
            zeros = b'0' * (1 << 20)
            for _ in range(513):
                g.write(zeros)
        open(os.path.join(r, 'evil_seed.csv'), 'w').write('# seed 1e300\nx\n1\n')
        for name, why in [('evil_cols.npz', 'more than 4096 columns'), ('evil_dim.npz', 'too large to count'),
                          ('evil_seed.npz', 'seed.npy: its seed is not a whole number'),
                          ('bomb.csv.gz', 'expands to more than 536870912 bytes'),
                          ('evil_seed.csv', 'its seed is not a whole number')]:
            evs, _ = browser('browser.load', file=name)
            refused = [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
            check('Load refuses %s with its name and why' % name,
                  len(refused) == 1 and name in refused[0]['error'] and why in refused[0]['error'],
                  str([e['error'] for e in refused]))
    finally:
        stop_server(p, r, snd)


check_data_formats()


def check_save_owner():
    """W129: the permission belongs to the command; only commit delivers."""
    log = LogLines()
    p, r, snd, col, _ = launch_server(log=log)
    errors = lambda evs: [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
    results = lambda evs: [e for e in evs if e.get('ev') == 'saved']
    try:
        col(is_idle)
        snd(cmd='browser.export', target='table', format='csv')
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W129: nothing to save reports once before asking for a name',
              end is not None and is_idle(end) and len(errors(evs)) == 1
              and errors(evs)[0]['error'] == 'Nothing to save'
              and results(evs) == [{'ev': 'saved', 'saved': False, 'file': ''}], str(evs)[-400:])
        live_run(snd, col)
        snd(cmd='value.save_file', kind='parameters', file='same.par')
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W129: a new file saves without a question or a command decision',
              end is not None and is_idle(end)
              and results(evs) == [{'ev': 'saved', 'saved': True, 'file': 'same.par'}]
              and not errors(evs), str(evs)[-300:])
        before = open(os.path.join(r, 'same.par'), 'rb').read()
        snd(cmd='value.set_parameters', values={'iapp': 0.11})
        col(is_idle)
        snd(cmd='value.save_file', kind='parameters', file='same.par')
        _, ask = col(is_ask)
        check('W129: an existing target without a decision asks once',
              ask is not None and ask.get('question') == 'same.par exists. Replace it?'
              and ask.get('keys') == 'yn', str(ask))
        snd(cmd='answer', id=ask['id'], key='n')
        evs, _ = col(is_idle)
        check('W129: No preserves the file and emits saved false, the command saying it was declined',
              before == open(os.path.join(r, 'same.par'), 'rb').read()
              and results(evs) == [{'ev': 'saved', 'saved': False, 'file': 'same.par'}]
              and len(errors(evs)) == 1 and 'declined' in errors(evs)[0]['error'], str(evs)[-300:])
        snd(cmd='value.save_file', kind='parameters', file='same.par')
        _, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], key='y')
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W129: Yes replaces the existing file without a second question',
              end is not None and is_idle(end) and not errors(evs)
              and results(evs) == [{'ev': 'saved', 'saved': True, 'file': 'same.par'}]
              and before != open(os.path.join(r, 'same.par'), 'rb').read(), str(evs)[-300:])
        snd(cmd='value.save_file', kind='parameters', file='same.par', replace=1)
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W526: a registry command carries no replace decision: it asks, or is refused naming the argument',
              end is not None and is_idle(end) and any('Unknown argument "replace"' in e.get('error', '') for e in evs), str(evs)[-300:])
        os.mkdir(os.path.join(r, 'blocked.par'))
        marker = os.path.join(r, 'blocked.par', 'original')
        with open(marker, 'wb') as f: f.write(b'old bytes')
        # The temp can be created beside this directory, but rename cannot
        # replace it: a deterministic commit failure, including on Windows.
        for name in ['blocked.par']:
            snd(cmd='value.save_file', kind='parameters', file=name)
            evs, end = col(lambda e: is_idle(e) or is_ask(e))
            if end and is_ask(end):
                snd(cmd='answer', id=end['id'], key='y')
                evs, _ = col(is_idle)
            check('W129: failed %s reports one error and no successful save' % name,
                  len(errors(evs)) == 1
                  and results(evs) == [{'ev': 'saved', 'saved': False, 'file': name}]
                  and open(marker, 'rb').read() == b'old bytes', str(evs)[-400:])
        check('W129: failed saves leave no temporary files or duplicate Writer log',
              not glob.glob(os.path.join(r, '*.tmp-*'))
              and not any('writer' in line.lower() for line in log), str(log[-5:]))
    finally:
        stop_server(p, r, snd)
    d = tempfile.mkdtemp(prefix='w129script')
    try:
        copy_model(args.ode, d)
        commands = [{'cmd': 'value.save_file', 'kind': 'parameters', 'file': 'repeat.par'}]
        folders = [d, tempfile.mkdtemp(prefix='w129script')]
        copy_model(args.ode, folders[1])
        runs = [run_commands(args.server, args.ode, commands, folder) for folder in folders]
        shutil.rmtree(folders[1], ignore_errors=True)
        check('W129: the same script saves twice, no disk-dependent ask',
              all(v.returncode == 0 and not any(e.get('ev') == 'ask' for e in
                  [json.loads(line) for line in v.stdout.splitlines() if line.strip()])
                  and '"saved":true' in v.stdout for v in runs), str([v.returncode for v in runs]))
    finally:
        shutil.rmtree(d, ignore_errors=True)


check_save_owner()

def check_output_names():
    """W130: actual asks, Cancel, whole names and the model-folder boundary."""
    p, r, snd, col, _ = launch_server()
    try:
        evs, _ = col(is_idle)
        h = next(e for e in evs if e.get('ev') == 'hello')
        check('W130: hello supplies all page save names from the model base',
              h.get('output_names') == {'par': 'lecar.par', 'ic': 'lecar.ic',
                                       'csv': 'lecar.csv', 'curves': 'lecar-curves.csv'}, str(h.get('output_names')))
        def cancel_name(want):
            _, ask = col(lambda e: is_ask(e) or is_idle(e))
            while ask and ask.get('kind') == 'form':
                check('W130: settings forms contain no filename', 'file' not in ask.get('kinds', []), str(ask))
                snd(cmd='answer', id=ask['id'], values=ask['values'])
                _, ask = col(lambda e: is_ask(e) or is_idle(e))
            check('W130: file ask offers ' + want,
                  ask and ask.get('kind') == 'file' and ask.get('file') == want, str(ask))
            if ask and is_ask(ask): snd(cmd='answer', id=ask['id'], ok=0)
            evs, _ = col(is_idle)
            return evs
        for kind in ('par', 'ic'):
            snd(cmd='value.save_file', kind=VALUE_KIND[kind])
            cancel_name('lecar.' + kind)
        snd(cmd='session.save_as', file_picker=True, confirm=False)
        cancel_name('lecar.snapx')
        live_run(snd, col)
        # Every file-taking registry command can explicitly ask for the existing picker.
        for command in [dict(cmd='plot.export_pdf'), dict(cmd='plot.export_svg'),
                        dict(cmd='browser.tabulate_column', variable='V', x_low=0, x_high=30)]:
            snd(**command, file_picker=True)
            _, ask = col(lambda e: is_ask(e) or is_idle(e))
            check('W517: '+command['cmd']+' requests a write picker', ask and ask.get('kind') == 'file' and ask.get('mode') == 'write', str(ask))
            if ask and is_ask(ask): snd(cmd='answer', id=ask['id'], ok=0); col(is_idle)
        for _ in range(3): snd(cmd='kinescope.capture'); col(is_idle)
        before = sorted(os.listdir(r))
        snd(cmd='kinescope.export_frames', file_picker=True)
        _, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], ok=0)
        evs,_ = col(is_idle)
        check('W130: cancelled kinescope picker requests no pixels and writes nothing', sorted(os.listdir(r)) == before and not any(e.get('kind') == 'pixels' for e in evs))
        snd(cmd='kinescope.export_frames', file='lecar.gif')
        saved=[]
        while True:
            evs,e = col(lambda e: is_ask(e) or is_idle(e))
            saved += [x for x in evs if x.get('ev') == 'saved']
            if not e or not is_ask(e): break
            snd(cmd='answer', id=e['id'], **pixels(e))
        names=['lecar.gif','lecar-frame1.gif','lecar-frame2.gif']
        check('W130: three captured frames save through the registry', [x.get('file') for x in saved] == names and all(os.path.isfile(os.path.join(r,n)) for n in names), str(saved))
        # W177: a live JSON reply cannot grant native-picker authorization.
        victim = os.path.join(os.path.dirname(r), os.path.basename(r) + '-victim.par')
        with open(victim, 'wb') as f: f.write(b'keep victim')
        try:
            for path in (victim, os.path.relpath(victim, r)):
                snd(cmd='value.save_file', kind='parameters')
                _, ask = col(is_ask)
                snd(cmd='answer', id=ask['id'], file=path, replace=1)
                evs, _ = col(is_idle)
                check('W177: replace 1 cannot authorize an external live save: ' + path,
                      open(victim, 'rb').read() == b'keep victim'
                      and any(e.get('error') and e.get('file') and 'line' in e for e in evs)
                      and not any(e.get('saved') for e in evs), str(evs)[-400:])
        finally:
            os.unlink(victim)
        snd(cmd='nullcline.compute'); col(is_idle)
        snd(cmd='nullcline.freeze'); col(is_idle)
        with open(os.path.join(r, 'result.dat.1'), 'wb') as f: f.write(b'keep frozen')
        snd(cmd='nullcline.export', file='result.dat')
        evs, _ = col(is_idle)
        check('W522: exporting the current nullclines leaves unrelated derived names untouched',
              open(os.path.join(r, 'result.dat.1'), 'rb').read() == b'keep frozen')
        snd(cmd='plot.set_variables', how='array_plot', names=['V', 'W'])
        col(is_idle)
        row=next(c for c in h['commands'] if c['cmd'] == 'array_plot.export_pdf')
        check('W492: array PDF labels are registry text fields, with no render type',
              [a['name'] for a in row['arguments'] if a['type'] == 'text'] == ['top_label','side_label','bottom_label'] and not any(a['name'] == 'render' for a in row['arguments']))
        snd(cmd='window.destroy', window='array_plot_1')
        col(is_idle)
        # A long valid name is retained, while traversal/device names report an error.
        long = 'save-' + 'a' * 80 + '.par'
        snd(cmd='value.save_file', kind='parameters')
        _, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], file=long)
        evs, _ = col(is_idle)
        check('W130: a filename over 24 characters is written whole',
              os.path.isfile(os.path.join(r, long)) and any(e.get('file') == long and e.get('saved') for e in evs), str(evs)[-300:])
        for bad in ('../escape.par', 'CON.par', '.hidden.par'):
            snd(cmd='value.save_file', kind='parameters')
            _, ask = col(is_ask)
            snd(cmd='answer', id=ask['id'], file=bad)
            evs, _ = col(is_idle)
            check('W130: typed unsafe output name is a placed error: ' + bad,
                  any(e.get('error') and e.get('file') and 'line' in e for e in evs)
                  and not any(e.get('saved') for e in evs), str(evs)[-300:])
        snd(cmd='recording.start')
        col(is_idle)
        snd(cmd='value.set_parameters', values={'iapp': .05})
        col(is_idle)
        snd(cmd='recording.stop')
        cancel_name('lecar.recx')
    finally:
        stop_server(p, r, snd)

check_output_names()



def check_seed_per_run():
    """W71 "a seed per run": examples/ode/fhn_noise.odex (wiener n) run
    twice gives different noise, each run logging its own seed in state
    (docs/protocol.md's state.seed); setting the numerics seed back to a
    run's own (Numerics > stocHast > New seed) and Go reproduces that
    run's series byte for byte."""
    p, r, snd, col, _ = launch_server(ode='examples/ode/fhn_noise.odex')


    def after(**cmd):
        snd(**cmd)
        return col(is_idle, timeout=30 * SLOW)[0]

    full = whole_series
    try:
        col(is_idle)
        snd(cmd='events.subscribe', events=['series'])
        col(is_idle)

        evs1 = after(cmd='run.initial')
        st1, ser1 = last_state(evs1), full(evs1)
        evs2 = after(cmd='run.initial')
        st2, ser2 = last_state(evs2), full(evs2)
        seed1 = st1.get('seed') if st1 else None
        seed2 = st2.get('seed') if st2 else None
        data1 = ser1[-1]['columns'] if ser1 else None
        data2 = ser2[-1]['columns'] if ser2 else None
        check('state: each Go logs its own seed', isinstance(seed1, int) and isinstance(seed2, int)
              and seed1 != seed2, str((seed1, seed2)))
        check('two Go presses give different data', data1 is not None and data1 != data2, '')

        # Numerics > stocHast > New seed: set run 2's own seed for the next Go
        after(cmd='analysis.seed_random',seed=seed2)

        evs4 = after(cmd='run.initial')
        st4, ser4 = last_state(evs4), full(evs4)
        seed4 = st4.get('seed') if st4 else None
        data4 = ser4[-1]['columns'] if ser4 else None
        check("setting run 2's logged seed reproduces it as the next run's seed",
              seed4 == seed2, str((seed4, seed2)))
        check("...and its data byte for byte", data4 == data2, '')
    finally:
        stop_server(p, r, snd)


check_seed_per_run()

# A HOME the process cannot write to used to make AUTO exit(1) under the
# client when it opened fort.8 there; open_auto() now falls back to the
# model's directory. Drive a second server with such a HOME and check it survives.
bad_home = tempfile.mkdtemp(prefix='xppbadhome')
shutil.rmtree(bad_home)  # a path that is guaranteed not to exist
proc2, run2, send2, collect2, events2 = launch_server(extra_env={'HOME': bad_home})
collect2(is_idle)  # startup hello/state/idle
send2(cmd='auto.open')
collect2(lambda e: e.get('ev') == 'window' and e.get('win') == 'diagram_1')
collect2(is_idle)
send2(cmd='auto.run', kind='equilibrium')
evs, e = collect2(is_idle, timeout=20*SLOW)
st2 = [x for x in evs if x.get('ev') == 'state']
alive = proc2.poll() is None
check('bad-HOME server survives Run/Steady state (state then idle, still alive)',
      st2 and e is not None and alive, str(evs)[-300:])

if alive:
    # writing to a dead server's stdin would raise and end the script
    send2(cmd='session.quit')
    evs, ask = collect2(lambda e: e.get('ev') == 'ask')
    if ask:
        send2(cmd='answer', id=ask['id'], key='d')
    try:
        proc2.wait(timeout=5 * SLOW)
        check('bad-HOME server: File/Quit exits', True)
    except subprocess.TimeoutExpired:
        proc2.kill()
        check('bad-HOME server: File/Quit exits', False)
else:
    proc2.kill()
    check('bad-HOME server: File/Quit exits', False, 'server had already exited')
shutil.rmtree(run2, ignore_errors=True)


# W154: the converter gives the foreign non-ASCII identifier Iαpp an
# .odex identifier. Its UTF-8 spelling remains intact in the source comment;
# UTF-8 protocol file names are exercised above (W35b).
UTF8_ODE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'utf8name.odex')
alpha_name = 'i__pp_'
p3, r3, snd3, col3, _ = launch_server(ode=UTF8_ODE)
try:
    col3(is_idle)
    snd3(cmd='session.get_state')
    evs, st = col3(is_state)
    col3(is_idle)
    check('a converted non-ASCII parameter has its suggested .odex name in state', st is not None
          and dict(st['pars']).get(alpha_name) == 0.05, str(st and st['pars']))
    snd3(cmd='value.set_parameters', values={alpha_name: 0.2})
    snd3(cmd='session.get_state')
    evs, st = col3(is_state)
    col3(is_idle)
    check('set by the converted name changes it, and state retains that name', st is not None
          and dict(st['pars']).get(alpha_name) == 0.2, str(st and st['pars']))
finally:
    stop_server(p3, r3, snd3)

# W68, W95 (GitHub #116, #144, docs/protocol.md "Commands during a command"):
# while a computation runs, a command of the data or computation kind is
# refused (one log line, then, after the computation, an error message, state
# and idle of its own: never run), a view command is kept and runs after the
# computation, and what the computation acts on (browser with from, abort) is
# taken at once. The computation says it began (`computing`, before its
# progress). lecar with Total 1e7 (2e8 steps) does not end on its own before
# the Abort below, whatever the machine's speed; its first progress event
# says it is under way, and its stopped that it still was when the Abort
# came, after the lines under test.
def check_refused_during_run():
    log = LogLines()
    p, r, snd, col, _ = launch_server(log=log)
    is_ask = lambda e: e.get('ev') == 'ask'
    try:
        col(is_idle)
        snd(cmd='run.configure', total_time=1e7)
        col(is_idle)
        snd(cmd='run.initial')
        evs, prog = col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        check('W95: the integration says it computes (computing) before its progress',
              [e.get('ev') for e in evs].count('computing') == 1, str([e.get('ev') for e in evs]))
        snd(cmd='equilibrium.find')  # a computation
        snd(cmd='run.continue')  # Continue: a computation
        snd(cmd='window.set_display', window='graph_1', x_range=[0, 50])  # a view: kept for after the run
        snd(cmd='browser.page', **{'from_row': 0, 'count': 1})  # only reads: answered during the run
        evs, rows = col(lambda e: e.get('ev') == 'browser')
        during = [e for e in evs if e.get('ev') in ('ask', 'idle', 'message')]
        snd(cmd='run.abort')
        evs, e = col(is_idle, timeout=30 * SLOW)
        stopped = any(x.get('ev') == 'stopped' for x in evs)
        check('W68: during a run, a browser request is answered at once and the run goes on until the Abort (stopped)',
              prog is not None and rows is not None and not during and stopped, str([x.get('ev') for x in evs])[-200:])
        after = []
        for _ in range(3):  # the two refused lines, then the display, each with its own idle
            more, _ = col(is_idle)
            after.append(more)
        refused = [[x.get('error', '') for x in m if x.get('ev') in ('message', 'done') and x.get('error')] for m in after[:2]]
        check('W95: a computation or data command sent during a run is refused after it: an error, state and idle each',
              all(len(m) == 1 and 'Not while a computation runs' in m[0] for m in refused)
              and not any(is_ask(x) for m in after for x in m), str(refused))
        snd(cmd='events.subscribe', events=['plots'])
        evs, _ = col(is_idle)
        plots = [x for x in evs if x.get('ev') == 'plots']
        w1 = plots and next((w for w in plots[-1]['windows'] if w['win'] == 'graph_1'), None)
        check('W95: a view command sent during a run runs after it (display: the zoom shown)',
              not any(x.get('ev') == 'message' for x in after[2]) and w1 and w1.get('zoom', {}).get('x') == [0, 50],
              str(w1 and w1.get('zoom')))
        want = ['refused during a computation: equilibrium.find', 'refused during a computation: run.continue']
        log.wait_for(lambda lines: all(w in lines for w in want))
        check('W95: each refused line is logged once ("refused during a computation: equilibrium.find"), the view none',
              all(log.count(w) == 1 for w in want) and len([l for l in log if 'refused' in l]) == 2,
              str([l for l in log if 'refused' in l]))
        snd(cmd='run.configure', total_time=20)
        evs, done = col(is_idle)
        check('W517: after the run a setting command works again', done is not None and not any('error' in e for e in evs))
    finally:
        stop_server(p, r, snd)


check_refused_during_run()


# W389 (GitHub #451, docs/protocol.md "Commands during a command"): the
# input's queue is bounded. While a computation runs, normal lines wait for
# it; past MAX_PENDING_LINES the next is refused with a Busy `message`
# error naming its command and id, never queued, the reader never blocks, so an
# Abort sent after the flood is read and stops the run (stopped); the lines
# that were queued then each run (state and idle), and the session works.
MAX_PENDING_LINES = 1024  # core/xpp_inbox.h
def check_busy_flood():
    flood = MAX_PENDING_LINES + 50
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='run.configure', total_time=1e7)
        col(is_idle)
        snd(cmd='run.initial')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        for i in range(flood):  # a view command: kept for after the run
            snd(cmd='window.set_display', window='graph_1', x_range=[0, 50], id=i)
        evs, last = col(lambda e: e.get('ev') in ('message', 'done') and ('(id %d)' % (flood - 1)) in e.get('error', ''))
        busy = [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error', '').startswith('Busy:')]
        ids = [int(re.search(r'\(id (\d+)\)', e['error']).group(1)) for e in busy]
        check('W389: past the bound each line is refused with a Busy error naming its command and id, none before',
              last is not None and ids == list(range(MAX_PENDING_LINES, flood))
              and all('display' in e['error'] and e.get('source', '').startswith('{') for e in busy),
              str(ids[:3]) + ' ... %d refused' % len(ids))
        snd(cmd='run.abort')
        evs, e = col(is_idle, timeout=30 * SLOW)
        check('W389: an Abort sent after the flood is read and stops the run (stopped)',
              any(x.get('ev') == 'stopped' for x in evs), str([x.get('ev') for x in evs])[-200:])
        done = 0
        while done < MAX_PENDING_LINES:  # the queued lines each run: state and idle
            evs, e = col(is_idle, timeout=30 * SLOW)
            if e is None:
                break
            done += 1
        check('W389: the queue drains: every line it held ran with its own idle', done == MAX_PENDING_LINES, str(done))
        snd(cmd='session.get_state')
        evs, e = col(lambda x: x.get('ev') == 'state')
        snd(cmd='window.set_display', window='graph_1', x_range=[0, 40], id=flood)
        evs, e = col(is_idle)
        check('W389: afterwards the session takes commands again (no Busy)',
              e is not None and not any(str(x.get('error', '')).startswith('Busy:') for x in evs), str(evs)[-200:])
    finally:
        stop_server(p, r, snd)


check_busy_flood()


# W106 (GitHub #155, docs/protocol.md "Commands during a command"): a setting
# sent during a computation (a parameter, a numerics field) is taken at once
# and kept for after it: the run in progress keeps the values it started
# with (its rows are those of a run with the old values, stopped at the same
# row: a .recx --silent with the stop armed there, not a race), and each setting
# then applies as a command of its own (state and idle), before anything
# else: the state after shows it, and the next run uses it. A bad numerics
# value is an error message after the run, naming the field. The numerics
# as data: `data` `numerics` sends them, and `set` kind `num` sets one.
def check_method_refusal():
    model_dir = tempfile.mkdtemp(prefix='xppmethod')
    model = os.path.join(model_dir, 'odd.odex')
    with open(model, 'w') as f:
        f.write("x'=-x\ny'=x\nz'=y\noptions { integration: { method: euler } }\n")
    p, r, snd, col, _ = launch_server(ode=model)
    try:
        col(is_idle)
        snd(cmd='run.configure', method='symplectic')
        evs, _ = col(is_idle)
        errors = [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        snd(cmd='events.subscribe', events=['numerics'])
        fields = next((e['fields'] for e in col(is_idle)[0] if e.get('ev') == 'numerics'), [])
        method = next((f['value'] for f in fields if f['key'] == 'method'), None)
        check('W132: run.configure refuses odd Symplectic at the command and keeps Euler',
              method == 'euler' and len(errors) == 1 and 'even dimensions' in errors[0]['error']
              and errors[0].get('file') == 'command' and errors[0].get('line') == 1, str(errors))
        snd(cmd='run.configure', method='symplectic')
        evs, _ = col(is_idle)
        snd(cmd='events.subscribe', events=['numerics'])
        fields = next((e['fields'] for e in col(is_idle)[0] if e.get('ev') == 'numerics'), [])
        check('W132: values panel refuses odd Symplectic and keeps Euler',
              any('even dimensions' in e.get('error', '') for e in evs)
              and next((f['value'] for f in fields if f['key'] == 'method'), None) == 'euler', str(evs))
        m_field = next((f for f in fields if f['key'] == 'method'), {})
        unav = m_field.get('unavailable', {})
        check('W379: odd model has Symplectic and Volterra unavailable in numerics',
              unav.get('symplectic') == 'Symplectic is only for even dimensions'
              and unav.get('volterra') == 'Volterra only for integral eqns', str(unav))
    finally:
        p.stdin.close()
        p.wait(timeout=10 * SLOW)
        shutil.rmtree(r, ignore_errors=True)

    wiener_model = os.path.join(model_dir, 'w.odex')
    with open(wiener_model, 'w') as f:
        f.write("wiener w\nx'=w\noptions { integration: { method: euler } }\n")
    p, r, snd, col, _ = launch_server(ode=wiener_model)
    try:
        col(is_idle)
        snd(cmd='events.subscribe', events=['numerics'])
        fields = next((e['fields'] for e in col(is_idle)[0] if e.get('ev') == 'numerics'), [])
        mfield = next((f for f in fields if f['key'] == 'method'), {})
        unav = mfield.get('unavailable', {})
        reason = 'a model with Wiener noise requires a method that draws it each step'
        check('W379: Wiener model marks adaptive methods unavailable with reason',
              all(unav.get(m) == reason for m in ['gear', 'qualrk', 'stiff', 'cvode', 'dopri5', 'dopri8', 'rosenbrock'])
              and 'euler' not in unav and 'runge_kutta' not in unav, str(unav))
        snd(cmd='run.configure', method='dopri5')
        evs, _ = col(is_idle)
        errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        check('W379: set num method refuses DoPri5 on Wiener model',
              len(errs) == 1 and ('Method: ' + reason) in errs[0], str(errs))
    finally:
        p.stdin.close()
        p.wait(timeout=10 * SLOW)
        shutil.rmtree(r, ignore_errors=True)

    bad_wiener = os.path.join(model_dir, 'bad_w.odex')
    with open(bad_wiener, 'w') as f:
        f.write("wiener w\nx'=w\noptions { integration: { method: dopri5 } }\n")
    run_silent = subprocess.run([os.path.abspath(args.server), bad_wiener, '--silent'], capture_output=True, text=True, cwd=model_dir, timeout=60 * SLOW)
    check('W379: --silent refuses DoPri5 with Wiener noise at the line',
          run_silent.returncode == 1 and 'bad_w.odex:3:26: the option `method=dopri5`: ' + reason in run_silent.stderr, run_silent.stderr)

    bad_ode = os.path.join(model_dir, 'bad_w.ode')
    with open(bad_ode, 'w') as f:
        f.write("wiener w\nx'=w\n@ meth=dopri5\ndone\n")
    run_conv = subprocess.run([os.path.abspath(args.server), bad_ode, '--convert'], capture_output=True, text=True, cwd=model_dir, timeout=60 * SLOW)
    check('W379: --convert refuses DoPri5 with Wiener noise at the @ line',
          run_conv.returncode == 1 and 'bad_w.ode:3: the option `meth=dopri5`: ' + reason in run_conv.stderr, run_conv.stderr)
    shutil.rmtree(model_dir, ignore_errors=True)


check_method_refusal()


def check_jacobian_setting():
    """W260: the numerics' jacobian, difference (the default) or exact: the
    numerics event shows it with its choices, `set num` takes a name or a
    number, and a model with no exact Jacobian says why, refuses exact and
    fails to load when its own @ line asks for it, at that line."""
    model_dir = tempfile.mkdtemp(prefix='xppjacobian')

    def fields_of(col, snd):
        snd(cmd='events.subscribe', events=['numerics'])
        return next((e['fields'] for e in col(is_idle)[0] if e.get('ev') == 'numerics'), [])

    try:
        p, r, snd, col, _ = launch_server()
        try:
            col(is_idle)
            jac = next((f for f in fields_of(col, snd) if f['key'] == 'jacobian'), {})
            check('W260: the numerics event has jacobian, difference by default, its choices, available',
                  jac.get('value') == 'difference' and jac.get('choices') == ['difference', 'exact'] and 'unavailable' not in jac
                  and 'integer' not in jac, str(jac))
            snd(cmd='set', kind='num', name='jacobian', text='exact')
            evs, _ = col(is_idle)
            errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            jac = next((f for f in fields_of(col, snd) if f['key'] == 'jacobian'), {})
            check('W260: set num jacobian exact takes it', not errs and jac.get('value') == 'exact', '%s %s' % (errs, jac))
            snd(cmd='set', kind='num', name='jacobian', text='difference')
            col(is_idle)
            snd(cmd='set', kind='num', name='jacobian', text='finite')
            evs, _ = col(is_idle)
            errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            jac = next((f for f in fields_of(col, snd) if f['key'] == 'jacobian'), {})
            snd(cmd='set', kind='num', name='jacobian', text='1')
            evs, _ = col(is_idle)
            errs += [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            check('W260: set num jacobian by name; a name that is no choice, a number too, is refused, naming them',
                  errs == ['Numerics: Jacobian must be difference or exact'] * 2 and jac.get('value') == 'difference', '%s %s' % (errs, jac))
            # W274 (folded into W285): the setting is saved with the session and restored
            snd(cmd='set', kind='num', name='jacobian', text='exact')
            col(is_idle)
            snd(cmd='session.save_as', file='jac', data=False)
            col(is_idle)
            with zipfile.ZipFile(os.path.join(r, 'jac.snapx')) as z:
                check('W274: model.setx holds the Jacobian setting by its word', b'\n  jacobian: exact,\n' in z.read('model.setx'))
            snd(cmd='set', kind='num', name='jacobian', text='difference')
            col(is_idle)
            snd(cmd='session.open_model', file='jac.snapx')
            while True:
                _, e = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=20 * SLOW)
                if e is None or e['ev'] in ('idle','done'):
                    break
                snd(cmd='answer', id=e['id'], key='d')
            jac = next((f for f in fields_of(col, snd) if f['key'] == 'jacobian'), {})
            check('W274: opening the session restores exact', jac.get('value') == 'exact', str(jac))
        finally:
            p.stdin.close()
            p.wait(timeout=10 * SLOW)
            shutil.rmtree(r, ignore_errors=True)
        noisy = os.path.join(model_dir, 'noisy.odex')
        with open(noisy, 'w') as f:
            f.write("par a = 1\nx' = -a*x + normal(0, 1)\n")
        p, r, snd, col, _ = launch_server(ode=noisy)
        try:
            col(is_idle)
            jac = next((f for f in fields_of(col, snd) if f['key'] == 'jacobian'), {})
            check('W260: the exact Jacobian is not made until asked for: exact is offered, with no reason yet',
                  jac.get('value') == 'difference' and 'unavailable' not in jac, str(jac))
            why = 'noisy.odex:2:1: uses random numbers'
            snd(cmd='set', kind='num', name='jacobian', text='exact')
            evs, _ = col(is_idle)
            errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            check('W260: set num jacobian exact is refused there, with the reason',
                  errs == ['Numerics: Jacobian: exact is not available: ' + why], str(errs))
            jac = next((f for f in fields_of(col, snd) if f['key'] == 'jacobian'), {})
            check('W260: a model with random numbers has no exact Jacobian; the field then says why, at its line',
                  jac.get('value') == 'difference' and jac.get('unavailable', {}).get('exact') == why, str(jac))
        finally:
            p.stdin.close()
            p.wait(timeout=10 * SLOW)
            shutil.rmtree(r, ignore_errors=True)
        with open(noisy, 'a') as f:
            f.write('options { integration: { jacobian: exact } }\n')
        run = subprocess.run([os.path.abspath(args.server), 'noisy.odex', '--silent'], capture_output=True, text=True, cwd=model_dir,
                             timeout=60 * SLOW)
        check('W260: @ jacobian=exact in a model with none fails the load at the @ line, with the reason',
              run.returncode == 1 and 'noisy.odex:3:26: the option `jacobian=exact`: the model has no exact Jacobian: '
              'noisy.odex:2:1: uses random numbers' in run.stderr, run.stderr[-400:])
        smooth = os.path.join(model_dir, 'smooth.odex')
        with open(smooth, 'w') as f:
            f.write("par a = 1\nx' = -a*x\ninit x = 1\n")

        def load_log():
            out = subprocess.run([os.path.abspath(args.server), 'smooth.odex', '--silent', '--verbose'], capture_output=True, text=True,
                                 cwd=model_dir, timeout=60 * SLOW)
            return out.stderr if out.returncode == 0 else 'exit %d: %s' % (out.returncode, out.stderr[-300:])
        log = load_log()
        check('W260: a model loaded with the default setting has not compiled an exact Jacobian',
              'Compiled ' in log and 'Compiled the exact Jacobian' not in log, log[-300:])
        with open(smooth, 'a') as f:
            f.write('options { integration: { jacobian: exact } }\n')
        check('W260: its own @ jacobian=exact compiles it at load', 'Compiled the exact Jacobian' in load_log())
        # Reload keeps the setting; a model with no exact Jacobian says so and takes difference
        lines = []
        p, r, snd, col, _ = launch_server(ode=smooth, log=lines)
        try:
            col(is_idle)
            snd(cmd='set', kind='num', name='jacobian', text='exact')
            col(is_idle)
            with open(os.path.join(r, 'smooth.odex'), 'w') as f:
                f.write("par a = 1\nx' = -a*x + normal(0, 1)\n")
            snd(cmd='session.reload_model')
            while True:
                evs, e = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=20 * SLOW)
                if e is None or e['ev'] in ('idle','done'):
                    break
                snd(cmd='answer', id=e['id'], key='d')
            jac = next((f for f in fields_of(col, snd) if f['key'] == 'jacobian'), {})
            check('W260: Reload keeping exact for a model with none says so, naming the file and why, and sets difference',
                  jac.get('value') == 'difference' and any('Jacobian set to difference: smooth.odex has no exact Jacobian: '
                                                'smooth.odex:2:1: uses random numbers' in l for l in lines), '%s %s' % (jac, lines[-3:]))
        finally:
            p.stdin.close()
            p.wait(timeout=10 * SLOW)
            shutil.rmtree(r, ignore_errors=True)
    finally:
        shutil.rmtree(model_dir, ignore_errors=True)


check_jacobian_setting()


def check_jacobian_failure():
    """W260: an exact derivative that is not finite where the right-hand
    side is (sqrt at 0) stops the run with an error at the equation's line,
    naming the variable, the time and the state, and offering the action
    "Switch to differences and continue": its commands, sent as they come,
    finish the run with differences; a recording of it replays to the same
    end; under differences the run goes through at once."""
    model_dir = tempfile.mkdtemp(prefix='xppjacfail')
    try:
        model = os.path.join(model_dir, 'sqrt.odex')
        with open(model, 'w') as f:
            f.write("x' = sqrt(x) + 1\ninit x = 0\noptions { integration: { method: stiff, total_time: 4, jacobian: exact } }\n")
        p, r, snd, col, _ = launch_server(ode=model)
        try:
            col(is_idle)
            snd(cmd='recording.start')
            col(is_idle)
            snd(cmd='run.initial')
            evs, _ = col(is_idle)
            errs = [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            err = errs[-1] if errs else {}
            action = err.get('action') or {}
            check('W260: sqrt(x) at x=0 under the exact Jacobian stops the run: its line, the variable, time and state, '
                  'and what to do',
                  len(errs) == 1 and err.get('file') == 'sqrt.odex' and err.get('line') == 1
                  and err.get('error') == "the exact Jacobian's dX'/dX is inf at t=0 (X=0), where X' is 1: "
                                          "switch the Numerics' Jacobian to difference", str(errs))
            check('W260: the error offers Switch to differences and continue, as the commands that do it',
                  action.get('label') == 'Switch to differences and continue'
                  and action.get('commands') == [{'cmd': 'set', 'kind': 'num', 'name': 'jacobian', 'text': 'difference'},
                                                 {'cmd': 'run.continue', 'until': 4}], str(action))
            for c in action.get('commands', []):
                snd(**c)
                evs, _ = col(is_idle)
            st = next((e for e in reversed(evs) if is_state(e)), {})
            errs = [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            check('W260: sending them finishes the run with differences, to t=4',
                  not errs and st.get('time') == 4 and st.get('rows', 0) > 1, '%s %s' % (errs, st))
            snd(cmd='recording.stop', file='click.recx')
            col(is_idle)
            replay = replay_recording(args.server, model, os.path.join(r, 'click.recx'), r)
            replayed = [json.loads(l) for l in replay.stdout.splitlines() if l.strip()]
            end = next((e for e in reversed(replayed) if is_state(e)), {})
            # the replay meets the same failure (a step's error: exit 1, as any) and the click's commands after it
            check('W260: a recording made with the click replays to the same end (time, rows, values), '
                  'the failure on the way',
                  replay.returncode == 1 and "dX'/dX is inf at t=0" in replay.stderr
                  and all(end.get(k) == st.get(k) for k in ('time', 'rows', 'now')),
                  '%d %s / %s %s' % (replay.returncode, {k: end.get(k) for k in ('time', 'rows', 'now')},
                                     {k: st.get(k) for k in ('time', 'rows', 'now')}, replay.stderr[-300:]))
        finally:
            p.stdin.close()
            p.wait(timeout=10 * SLOW)
            shutil.rmtree(r, ignore_errors=True)
        with open(model, 'w') as f:
            f.write("x' = sqrt(x) + 1\ninit x = 0\noptions { integration: { method: stiff, total_time: 4 } }\n")
        run = subprocess.run([os.path.abspath(args.server), 'sqrt.odex', '--silent'], capture_output=True, text=True,
                             cwd=model_dir, timeout=60 * SLOW)
        check('W260: under differences the same run goes through', run.returncode == 0 and 'Jacobian' not in run.stderr,
              run.stderr[-300:])
    finally:
        shutil.rmtree(model_dir, ignore_errors=True)


check_jacobian_failure()


def check_auto_jacobian_failure():
    """W275: an exact derivative that is not finite during an AUTO run (the
    sqrt term is present below a = 0.5, on the branch x = 0, where its slope
    is infinite while the right-hand side is 0) stops the run as a Stop does:
    the last converged point is stored as an end point (EP) with a label.
    The error carries the action "Switch to differences and continue": the
    command that sets the Jacobian to difference, then the one that restarts
    the continuation from that label with the run's own settings; sent as
    they come they take the branch on past the failure to Par Min, and a
    recording of it replays to the same diagram."""
    model_dir = tempfile.mkdtemp(prefix='xppautjacfail')
    try:
        model = os.path.join(model_dir, 'rootfail.odex')
        with open(model, 'w') as f:
            f.write("par a=1.5\nx' = (a+1)*x + if(a<0.5)then(sqrt(x*x))else(0)\ninit x=0\noptions { integration: { jacobian: exact, total_time: 1 } }\n")
        p, r, snd, col, _ = launch_server(ode=model)
        try:
            col(is_idle)
            snd(cmd='events.subscribe', events=['autoinfo'])
            col(is_idle)
            snd(cmd='recording.start')
            col(is_idle)
            snd(cmd='auto.open')
            col(lambda e: e.get('ev') == 'window' and e.get('win') == 'diagram_1')
            col(is_idle)
            snd(cmd='auto.configure_numerics', step_size=-0.1, max_step_size=0.1, parameter_min=-0.5, parameter_max=2, max_steps=100)
            col(is_idle)
            snd(cmd='auto.configure_plot', plot_type='norm', variable='x', parameter1='a')
            col(is_idle)
            snd(cmd='auto.run', kind='equilibrium')
            evs, _ = col(is_idle, timeout=60 * SLOW)
            errs = [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            err = errs[-1] if errs else {}
            action = err.get('action') or {}
            cmds = action.get('commands') or [{}, {}]
            labs = [(lab, sym) for e in evs if e.get('ev') == 'diagram' and e.get('op') == 'add'
                    for run in e['runs'] for _i, lab, sym in run.get('lab', [])]
            info = [e['stop'] for e in evs if e.get('ev') == 'autoinfo' and e.get('stop')]
            check('W275: the exact Jacobian not finite in an AUTO run is one error, at the equation, naming it and the point',
                  len(errs) == 1 and err.get('file') == 'rootfail.odex' and err.get('line') == 2
                  and err.get('error', '').startswith('AUTO stopped: the exact Jacobian') and '(AUTO at a=' in err.get('error', ''),
                  str(errs))
            check('W275: the run stored its last converged point as an end point, with a label, and says why it stopped',
                  bool(labs) and labs[-1][1] == 'end' and labs[-1][0] > 0 and info and info[-1].get('why') == 'jacobian',
                  '%s %s' % (labs, info))
            check('W275: the error offers Switch to differences and continue: set the Jacobian to difference, then restart from the end point',
                  action.get('label') == 'Switch to differences and continue' and len(cmds) == 2
                  and cmds[0] == {'cmd': 'set', 'kind': 'num', 'name': 'jacobian', 'text': 'difference'}
                  and cmds[1] == {'cmd': 'auto.run', 'kind': 'equilibrium', 'label': labs[-1][0] if labs else None}, str(action))
            before = sum(len(run['x']) for e in evs if e.get('ev') == 'diagram' and e.get('op') == 'add' for run in e['runs'])
            went = []
            for c in cmds:
                snd(**c)
                more, _ = col(is_idle, timeout=60 * SLOW)
                went += more
            errs = [e for e in went if e.get('ev') in ('message', 'done') and 'error' in e]
            xs = [x for e in went if e.get('ev') == 'diagram' and e.get('op') == 'add' for run in e['runs'] for x in run['x']]
            info = [e['stop'] for e in went if e.get('ev') == 'autoinfo' and e.get('stop')]
            check('W275: sending them takes the branch on, past the failure (a below 0.5) to Par Min, with no error',
                  not errs and xs and min(xs) < 0.4 and info and info[-1].get('why') == 'parameter_min', '%s %s %s' % (errs, xs[-3:], info))
            branch_jacobians = lambda evs: sorted({(run['br'], run['jac']) for e in evs if e.get('ev') == 'diagram' and e.get('op') == 'add'
                                                   for run in e['runs']})
            first = branch_jacobians(evs)
            second = branch_jacobians(went)
            check('W278: each run of the diagram says how its Jacobian was formed: the run with the exact Jacobian exact, the restart after the switch difference (one branch, two runs)',
                  first == [(1, 'exact')] and second == [(1, 'difference')], '%s %s' % (first, second))
            snd(cmd='recording.stop', file='click.recx')
            col(is_idle)
            replay = replay_recording(args.server, model, os.path.join(r, 'click.recx'), r)
            replayed = [json.loads(l) for l in replay.stdout.splitlines() if l.strip()]
            points = sum(len(run['x']) for e in replayed if e.get('ev') == 'diagram' and e.get('op') == 'add' for run in e['runs'])
            check('W275: a recording made with the click replays to the same diagram (the run stopped where it did, the restart from its label)',
                  points == before + len(xs), '%d %d %s' % (points, before + len(xs), replay.stderr[-300:]))
            snd(cmd='session.save_as', file='jac')
            col(is_idle)
            snd(cmd='session.open_model', file='jac.snapx')
            reopened, ask = col(lambda e: is_idle(e) or is_ask(e), timeout=60 * SLOW)
            if is_ask(ask or {}):
                snd(cmd='answer', id=ask['id'], key='d')  # the question to save this session first: don't
                more, _ = col(is_idle, timeout=60 * SLOW)
                reopened += more
            check("W278: a saved and reopened session keeps each run's Jacobian",
                  branch_jacobians(reopened) == sorted(first + second), '%s %s' % (first + second, branch_jacobians(reopened)))
        finally:
            p.stdin.close()
            p.wait(timeout=10 * SLOW)
            shutil.rmtree(r, ignore_errors=True)
    finally:
        shutil.rmtree(model_dir, ignore_errors=True)


check_auto_jacobian_failure()


def check_settings_during_run():
    p, r, snd, col, _ = launch_server()
    is_prog = lambda e: e.get('ev') == 'progress'
    num = lambda evs: next((dict((f['key'], f['value']) for f in e['fields'])
                            for e in reversed(evs) if e.get('ev') == 'numerics'), None)
    iapp = lambda evs: next((dict(e['pars']).get('iapp') for e in reversed(evs) if is_state(e)), None)
    try:
        col(is_idle)
        snd(cmd='events.subscribe', events=['numerics'])
        evs, _ = col(is_idle)
        n0 = num(evs)
        fields = next((e['fields'] for e in evs if e.get('ev') == 'numerics'), [])
        meth = next((f for f in fields if f['key'] == 'method'), {})
        check('W106: data numerics sends the numerics: total, dt, method (by name, with its choices)',
              n0 is not None and n0.get('total_time') == 30 and n0.get('dt') == 0.05
              and meth.get('value') == 'runge_kutta'
              and next((f for f in fields if f['key'] == 'store_every'), {}).get('integer') is True, str(n0))
        snd(cmd='run.configure', total_time=1e7)
        evs, _ = col(is_idle)
        check('W106: set kind num sets a numerics field (total), the numerics event says so',
              (num(evs) or {}).get('total_time') == 1e7, str(num(evs)))
        snd(cmd='run.configure', dt=0)
        evs, _ = col(is_idle)
        errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        check('W106: a bad numerics value is an error naming the field, nothing changes',
              errs == ['Dt must be a number other than 0'] and num(evs) is None, str(errs))
        # W206: Store every N steps is `store_every`; its old name nout is refused, as any unknown key
        snd(cmd='set', kind='num', name='nout', value=1)
        evs, _ = col(is_idle)
        errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        check('W206: set num refuses the old key nout, naming it', len(errs) == 1 and 'nout' in errs[0], str(errs))
        snd(cmd='run.configure', store_every=1)
        evs, _ = col(is_idle)
        errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        check('W206: set num takes store_every', not errs, str(errs))
        # W116: what the value commands and set cannot take is the protocol's
        # error, the value left as it was (never a 0, never the ICs reset)
        snd(cmd='session.get_state')
        v_before = dict(next((e for e in reversed(col(is_idle)[0]) if is_state(e)), {'ics': []})['ics']).get('V')
        for bad, expect in (
                (dict(cmd='set', kind='par', name='iapp'), 'set no longer takes parameters: send value.set_parameters (a formula: value.set_by_formula)'),
                (dict(cmd='value.set_parameters', values={'iapp': '0.2'}), 'Argument "values.iapp" must be a finite number'),
                (dict(cmd='set', kind='parm', name='iapp', value=0.2), 'set takes kind delay, bc or num, not "parm"'),
                (dict(cmd='value.set_parameters', values={'nosuch': 0.2}), 'Argument "values": no parameter named "nosuch" in the model'),
                (dict(cmd='ic.set', values={'nosuch': 0.2}), 'Argument "values": no variable named "nosuch" in the model'),
                (dict(cmd='value.slide', name='nosuch', value=0.2), 'Argument "name": no parameter or variable named "nosuch" in the model'),
                (dict(cmd='value.slide', name='iapp'), 'Missing argument "value"'),
                (dict(cmd='set.restore_defaults', scope='pars'), 'Invalid choice "pars" for scope')):
            snd(cmd='ic.set', values={'v': 0.25})
            evs, _ = col(is_idle)
            iapp0 = iapp(evs)
            snd(**bad)
            evs, _ = col(is_idle)
            errs = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
            snd(cmd='session.get_state')
            st = next((e for e in reversed(col(is_idle)[0]) if is_state(e)), None)
            check('W116: %s is refused (%s), iapp and V as they were' % (json.dumps(bad), expect),
                  errs == [expect] and st and dict(st['pars']).get('iapp') == iapp0 and dict(st['ics']).get('V') == 0.25,
                  '%s; %s' % (errs, st and (dict(st['pars']).get('iapp'), dict(st['ics']).get('V'))))
        snd(cmd='ic.set', values={'v': v_before})  # the ICs the checks after expect
        col(is_idle)
        snd(cmd='run.configure', method='cvode')
        evs, _ = col(is_idle)
        m = num(evs)
        snd(cmd='run.configure', method='runge_kutta')
        evs, _ = col(is_idle)
        taken = num(evs)
        snd(cmd='run.configure', method='3')
        refused, _ = col(is_idle)
        errs = [e.get('error') for e in refused if e.get('ev') in ('message', 'done') and 'error' in e]
        check('W106: set num method takes its name (the key of the method table), never its number',
              m and m.get('method') == 'cvode' and (taken or {}).get('method') == 'runge_kutta'
              and len(errs) == 1 and '`3` is not a method' in errs[0] and num(refused) is None, str((m, taken, errs)))
        snd(cmd='run.initial')
        evs, prog = col(is_prog, timeout=30 * SLOW)
        snd(cmd='value.set_parameters', values={'iapp': 0.3})
        snd(cmd='run.configure', total_time=50)
        snd(cmd='run.configure', store_every=0)  # bad: its error comes after the run
        snd(cmd='session.get_state')
        snd(cmd='browser.page', **{'from_row': 0, 'count': 1})  # control, behind the sets: they were taken
        evs, _ = col(lambda e: e.get('ev') == 'browser')
        during = [e for e in evs if e.get('ev') in ('idle', 'message')]
        dones = [e for e in evs if e.get('ev') == 'done']  # the state's own, answered at once (session.get_state)
        check('W106: settings sent during a run get no idle or error while it runs; state still shows the '
              'values it runs with', prog is not None and not during and len(dones) == 1 and dones[0].get('status') == 'ok'
              and iapp(evs) == 0.05, str([e.get('ev') for e in evs]))
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        stopped = [e for e in evs if e.get('ev') == 'stopped']
        rows = next((e['rows'] for e in reversed(evs) if is_state(e)), 0)
        check('W106: the run went on with Total 1e7 until the Abort (stopped), the settings not applied yet',
              stopped and rows > 1 and iapp(evs) == 0.05 and num(evs) is None, str(stopped))
        after = []
        for _ in range(3):
            more, _ = col(is_idle)
            after.append(more)
        errs = [[e.get('error') for e in m if e.get('ev') in ('message', 'done') and e.get('error')] for m in after]
        check('W106: then each setting is a command of its own, in order: the bad one an error after the run',
              iapp(after[0]) == 0.3 and (num(after[1]) or {}).get('total_time') == 50
              and errs == [[], [], ['Argument "store_every" must be at least 1']], str(errs))
        snd(cmd='browser.page', **{'from_row': rows - 1, 'count': 1})
        evs, br = col(lambda e: e.get('ev') == 'browser')
        col(is_idle)
        live = br and br['data']
        code, out, err = run_replay([
            {'cmd': 'run.configure', 'total_time': 1e7},
            {'cmd': 'run.configure', 'method': 'runge_kutta'},
            {'cmd': 'run.initial'},
            {'cmd': 'run.abort', 'at': {'what': 'integrate', 'rows': rows, 't': 0}},
            {'cmd': 'browser.page', 'from_row': rows - 1, 'count': 1}])
        ref = next((e['data'] for e in map(json.loads, out.splitlines()) if e.get('ev') == 'browser'), None)
        check('W106: the rows the run stored are those of a run with the old values (the settings did not '
              'reach it)', code == 0 and live and live == ref, '%s vs %s (%s)' % (live, ref, err[-200:]))
        snd(cmd='run.initial')
        evs, _ = col(is_idle, timeout=60 * SLOW)
        rows2 = next((e['rows'] for e in reversed(evs) if is_state(e)), 0)
        snd(cmd='browser.page', **{'from_row': rows2 - 1, 'count': 1})
        evs, br = col(lambda e: e.get('ev') == 'browser')
        col(is_idle)
        code, out, err = run_replay([
            {'cmd': 'value.set_parameters', 'values': {'iapp': 0.3}},
            {'cmd': 'run.configure', 'total_time': 50},
            {'cmd': 'run.configure', 'method': 'runge_kutta'},
            {'cmd': 'run.initial'},
            {'cmd': 'browser.page', 'from_row': 1000, 'count': 1}])
        ref = next((e['data'] for e in map(json.loads, out.splitlines()) if e.get('ev') == 'browser'), None)
        check('W106: the next run uses them (Total 50: 1001 rows, iapp 0.3: the rows of a run set so)',
              rows2 == 1001 and br and br['data'] == ref, '%d rows, %s vs %s' % (rows2, br and br['data'], ref))
    finally:
        stop_server(p, r, snd)


check_settings_during_run()


# W95: a command sent right behind one that asks, before the client saw the
# question, is kept for after that command, never lost: its own idle comes
# (the key g opens a menu; the state behind it runs once the menu is answered)
def check_kept_behind_ask():
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='session.open_model')
        snd(cmd='session.get_source')
        evs, ask = col(lambda e: e.get('ev') == 'ask')
        snd(cmd='answer', id=ask['id'], ok=0)
        evs, _ = col(is_idle)
        evs2, _ = col(is_idle)
        check('W95: a command sent before a question it could not see runs after that command (equations, own idle)',
              any(x.get('ev') == 'equations' for x in evs2) and not any(x.get('ev') == 'equations' for x in evs),
              str([x.get('ev') for x in evs2]))
    finally:
        stop_server(p, r, snd)
check_kept_behind_ask()



def check_open_reload():
    """W61: File > Open model and Reload load in this process. Open asks
    (save first / don't save), then the page gets a new hello and the new
    model's state; Reload keeps the values by name and takes the file's
    edits; a model that cannot be loaded leaves the one before running,
    integrating to the same data."""
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        other = os.path.join(r, 'other.ode')
        with open(other, 'w') as f:
            f.write('par a=1\ninit x=0.5\nx\'=-a*x\n@ total=5, dt=0.05\ndone\n')
        with open(os.path.join(r, 'broken.ode'), 'w') as f:
            f.write('par a=1\nx\'=-a*y+\ndone\n')
        with open(os.path.join(r, 'badoption.ode'), 'w') as f:
            f.write('par a=1\nx\'=-a*x\n@ ync=12\ndone\n')

        def rows_of(name):
            """the stored data as written by the browser (csv), comments dropped"""
            snd(cmd='browser.export', target='table', format='csv', file=name)
            col(is_idle)
            with open(os.path.join(r, name)) as f:
                return [l.strip() for l in f if l.strip() and not l.startswith('#')]

        def integrate():
            snd(cmd='run.initial')
            col(is_idle)

        def open_model(file, key='d'):
            """open file, answering its question with key; the events until idle"""
            snd(cmd='session.open_model', file=file)
            got = []
            while True:
                evs, e = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=20 * SLOW)
                got += evs
                if e is None or e['ev'] in ('idle','done'):
                    return got
                if key:
                    snd(cmd='answer', id=e['id'], key=key)
                else:
                    snd(cmd='answer', id=e['id'], ok=0)

        # W135: Open replaces all definitions, including slots absent from the next model.
        for model, preset in [('lecar', '@ s1=iapp,slo1=-1,shi1=2\n'),
                              ('vanderpol', '@ s1=x,slo1=-3,shi1=3\n')]:
            with open(os.path.join('examples', 'ode', model + '.ode')) as f:
                source = f.read()
            filename = 'w135-' + model + '.ode'
            with open(os.path.join(r, filename), 'w') as f:
                f.write(preset + source)
            switched = last_state(open_model(filename))
            expected = 'iapp' if model == 'lecar' else 'x'
            check('W135: Open %s replaces the sliders with its @ presets' % model,
                  switched and [d['name'] for d in switched['sliders'] if d['name']] == [expected],
                  str(switched and switched.get('sliders')))
        with open(os.path.join(r, 'w317-sliders.odex'), 'w') as f:
            f.write("par a=1\nx'=0\ninit x=0\noptions { interface: { sliders: [" +
                    ', '.join('{ name: a, low: 0, high: %d }' % i for i in range(2, 7)) + " ] } }\n")
        native = last_state(open_model('w317-sliders.odex'))
        check('W317: a native preset list supplies all five sliders',
              native and len(native['sliders']) == 5 and [d['hi'] for d in native['sliders']] == list(range(2, 7)), str(native and native['sliders']))
        open_model('lecar.odex')
        integrate()
        first = rows_of('lecar1.csv')
        snd(cmd='auto.open')
        evs, _ = col(is_idle)

        def made(evs, op, win):
            return any(e.get('ev') == 'window' and e.get('op') == op and e.get('win') == win for e in evs)

        check('File/Auto opens window 101 before the open', made(evs, 'create', 'diagram_1'), '')
        evs = open_model('other.ode', key=None)
        asks = [e for e in evs if e.get('ev') == 'ask']
        check('open asks first, offering to save (choice s/d)',
              asks and asks[0]['kind'] == 'choice' and asks[0].get('keys') == 'sd', str(asks[:1]))
        check('open cancelled: no hello, lecar still loaded',
              not [e for e in evs if e.get('ev') == 'hello'] and not made(evs, 'destroy', 'diagram_1')
              and last_state(evs)['ics'][0][0] == 'V',
              str(last_state(evs))[:120])
        evs = open_model('other.ode')
        hello = next((e for e in evs if e.get('ev') == 'hello'), None)
        st = last_state(evs)
        check("open: the model before's AUTO window goes", made(evs, 'destroy', 'diagram_1'), '')
        check('open loads the model here: a new hello names it', hello is not None and hello.get('file') == 'other.odex',
              str(hello)[:120])
        check('open: the state is the new model\'s', st and st['pars'] == [['a', 1]] and st['ics'] == [['X', 0.5]],
              str(st)[:200])
        integrate()
        other1 = rows_of('other1.csv')
        check('the opened model integrates (its 101 rows and their heading)', len(other1) == 102, str(other1[:3]))

        # Reload after editing: a's value set in the session stays, the new b comes with its file value
        snd(cmd='value.set_parameters', values={'a': 3})
        col(is_idle)
        snd(cmd='ic.set', values={'X': 2})
        col(is_idle)
        with open(os.path.splitext(other)[0] + '.odex', 'w') as f:
            f.write('par a=1, b=7\ninit x=0.5\nx\'=-a*x+b*0\noptions { integration: { total_time: 5, dt: 0.05 } }\n')
        snd(cmd='session.reload_model')
        evs, ask = col(lambda e: e.get('ev') == 'ask' or is_idle(e))
        check('reload asks first, as Open model does (W59d): Save session s, Don\'t save d',
              ask and ask.get('kind') == 'choice' and ask.get('keys') == 'sd'
              and ask.get('choices') == ['Save session', "Don't save"] and 'Reload other.odex?' in ask.get('question', ''),
              str(ask))
        if ask and ask.get('ev') == 'ask':
            snd(cmd='answer', id=ask['id'], key='d')
            evs, _ = col(is_idle)
        st = last_state(evs)
        hello = next((e for e in evs if e.get('ev') == 'hello'), None)
        check('reload: a new hello, the file\'s own defaults', hello is not None
              and hello.get('defaults', {}).get('pars') == [1, 7], str(hello and hello.get('defaults')))
        check('reload keeps the values by name, the new parameter from the file',
              st and st['pars'] == [['a', 3], ['b', 7]] and st['ics'] == [['X', 2]], str(st and (st['pars'], st['ics'])))
        # the numerics too: nUmerics/Total 10 survives a reload (the file says 5)
        snd(cmd='run.configure', total_time=10)
        col(is_idle)
        snd(cmd='session.reload_model')
        _, ask = col(lambda e: e.get('ev') == 'ask')
        if ask:
            snd(cmd='answer', id=ask['id'], key='d')
        col(is_idle)
        integrate()
        check('reload keeps the numerics: Total 10 gives 201 rows', len(rows_of('other_t10.csv')) == 202, '')

        # a model that cannot be loaded: the one before stays, integrating to the same data
        snd(cmd='value.set_parameters', values={'a': 1})
        col(is_idle)
        snd(cmd='ic.set', values={'X': 0.5})
        col(is_idle)
        integrate()
        before = rows_of('other2.csv')
        evs = open_model('broken.ode')
        errs = [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        check('a broken model: an error, no hello',
              errs and errs[-1].get('file') == 'broken.ode' and not [e for e in evs if e.get('ev') == 'hello'],
              str(errs)[:200])
        check('a broken model: the model before is still loaded, its values untouched',
              last_state(evs)['pars'] == [['a', 1], ['b', 7]], str(last_state(evs))[:200])
        evs = open_model('badoption.ode')
        errs = [e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        check('a refused option value: an error naming the file, line and value, the model before kept (W119)',
              errs and errs[-1].get('file') == 'badoption.ode' and 'ync=12' in errs[-1]['error']
              and not [e for e in evs if e.get('ev') == 'hello']
              and last_state(evs)['pars'] == [['a', 1], ['b', 7]], str(errs)[:300])
        evs = open_model('missing.ode')
        check('a missing model: an error naming the file (line 0: it cannot be read), nothing asked',
              [e for e in evs if e.get('ev') in ('message', 'done') and e.get('file') == 'missing.ode' and e.get('line') == 0]
              and not [e for e in evs if e.get('ev') == 'ask'], str(evs)[:200])
        integrate()
        after = rows_of('other3.csv')
        check('after a failed open the model integrates to the same data', after == before and len(after) > 100,
              '%d vs %d rows' % (len(after), len(before)))

        # back to lecar: the same data as the first run, nothing of other.ode left
        evs = open_model(os.path.basename(args.ode))
        check('open back the first model: its hello', any(e.get('ev') == 'hello' for e in evs), '')
        integrate()
        check('the first model integrates to its first data again', rows_of('lecar2.csv') == first, '')
        check('the process is alive after it all', p.poll() is None, '')
    finally:
        stop_server(p, r, snd)


# W65: what the page displays is the core's (docs/protocol.md "Display state"):
# earlier runs until Erase (the `runs` event), the zoom shown and the earlier
# runs' toggle (`display`, `plots` zoom/runs), AUTO's hidden branches and zoom
# (`autoview`).
def check_display_state():
    p, r, snd, col, _ = launch_server(NO_THROTTLE)

    def after(**cmd):
        snd(**cmd)
        return col(is_idle, timeout=60 * SLOW)[0]


    runs_of = lambda evs: [e for e in evs if e.get('ev') == 'runs']
    added = lambda evs: [x for e in runs_of(evs) for x in e['add']]
    win1 = lambda evs: [w for e in evs if e.get('ev') == 'plots' for w in e['windows'] if w['win'] == 'graph_1']
    full = whole_series
    try:
        col(is_idle)
        evs = after(cmd='events.subscribe', events=['series', 'plots'])
        w = win1(evs)
        check('display: a window shows its own axes and its earlier runs at first',
              w and w[-1].get('zoom') == {'x': None, 'y': None} and w[-1].get('runs') == 1, str(w))
        check('display: no runs event before there is a run', not runs_of(evs))
        evs = after(cmd='run.initial')
        first = full(evs)[-1]
        check('display: the first run has nothing before it', not added(evs) and not any(e['erased'] for e in runs_of(evs)))
        evs = after(cmd='run.initial')
        cols1 = {c['col']: c['data'] for c in first['columns']}
        check('display: a second run keeps the first as an earlier run (one runs event: keep, the series the client'
              ' holds, not its data again)',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['keep'] == 1 and not added(evs)
              and all(e['erased'] == 0 and not e['clear'] and e['drop'] == 0 for e in runs_of(evs)), str(runs_of(evs))[:300])
        evs = after(cmd='events.subscribe', events=['series', 'plots'])
        got = added(evs)
        check('display: asking for the data again sends the earlier runs whole, once: the data of the first run',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['clear'] == 1 and len(got) == 1
              and got[0]['rows'] == first['rows'] and got[0]['curves'] == first['curves']
              and {c['col']: values(c, None) for c in got[0]['columns']} == cols1, str(runs_of(evs))[:200])
        evs = after(cmd='window.redraw')
        check('display: Redraw preserves the retained trajectory without clearing it',
              all(e['clear'] == 0 and e['erased'] == 0 for e in runs_of(evs)))
        evs = after(cmd='events.subscribe', events=['series', 'plots'])
        got = added(evs)
        check('display: retained values remain unchanged after Redraw',
              len(got) == 1 and {c['col']: values(c, None) for c in got[0]['columns']} == cols1)
        evs = after(cmd='window.clear')
        check('display: Erase forgets the earlier runs and hides the current one',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['clear'] == 1 and runs_of(evs)[0]['erased'] == 1
              and not added(evs), str(runs_of(evs)))
        evs = after(cmd='window.redraw')
        check('display: Redraw unhides the current run without another history clear',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['clear'] == 0 and runs_of(evs)[0]['erased'] == 0
              and not added(evs), str(runs_of(evs)))

        evs = after(cmd='window.set_display', window='graph_1', x_range=[0, 10], y_range=[-0.5, 0.5], show_earlier_runs=False)
        w = win1(evs)
        check('display: a zoom and the runs toggle are held by the window and sent in plots',
              w and w[-1]['zoom'] == {'x': [0, 10], 'y': [-0.5, 0.5]} and w[-1]['runs'] == 0, str(w))
        evs = after(cmd='window.set_display', window='graph_1', x_range=[])
        w = win1(evs)
        check('display: null returns one axis to the window own axes', w and w[-1]['zoom'] == {'x': None, 'y': [-0.5, 0.5]}, str(w))
        evs = after(cmd='window.set_display', window='graph_1', x_range=[5, 1])
        check('display: a range with low not below high is refused, the zoom unchanged',
              any((e.get('ev') == 'done' and e.get('status') == 'error') or (e.get('ev') in ('message', 'done') and 'error' in e) for e in evs) and not win1(evs), str(evs)[:200])
        evs = after(cmd='window.set_display', window='graph_7', x_range=[0, 1])
        check('display: an unknown window is refused',
              any(e.get('ev') == 'done' and e.get('status') == 'error' for e in evs), str(evs)[:200])
        evs = after(cmd='window.set_bounds',x_min=-1,x_max=1,y_min=-1,y_max=1)
        w = win1(evs)
        check('display: other axes of the window drop the zoom', w and w[-1]['zoom'] == {'x': None, 'y': None}, str(w))
    finally:
        stop_server(p, r, snd)

    p, r, snd, col, _ = launch_server(NO_THROTTLE)
    try:
        col(is_idle)
        snd(cmd='events.subscribe', events=['autoinfo'])
        evs, _ = col(is_idle)
        av = lambda evs: [e for e in evs if e.get('ev') == 'autoview']
        check('autoview: sent at once after data, nothing hidden, the diagram own axes',
              [(e['earlier'], e['show'], e['active'], e['views']) for e in av(evs)] == [(0, 0, 'diagram_1', [{'name':'diagram_1','zoom': {'x': None, 'y': None}}])],
              str(av(evs)))
        evs = lecar_to_auto(snd, col)
        n = len(rebuild_diagram(evs, []))
        check('autoview: a run hides nothing', not av(evs) or av(evs)[-1]['earlier'] == 0, str(av(evs)))
        snd(cmd='auto.hide_earlier')
        evs, _ = col(is_idle)
        snd(cmd='auto.redraw')
        evs2, _ = col(is_idle)
        got = av(evs) + av(evs2)
        check('autoview: Clear makes the points so far the earlier branches, hidden',
              got and got[-1]['earlier'] == n and got[-1]['show'] == 0 and n > 1, str(got) + ' n=%d' % n)
        snd(cmd='auto.set_display', show_earlier=True, x_range=[0.1, 0.2])
        evs, _ = col(is_idle)
        check('autoview: show and the zoom are set by auto display',
              av(evs) and av(evs)[-1] == dict(av(evs)[-1], earlier=n, show=1, views=[{'name':'diagram_1','zoom': {'x': [0.1, 0.2], 'y': None}}]),
              str(av(evs)))
        snd(cmd='auto.set_display', x_range=[3, 3])
        evs, _ = col(is_idle)
        check('autoview: a bad range is refused', any(e.get('ev') in ('message', 'done') and 'error' in e for e in evs), str(evs)[:200])
        snd(cmd='window.destroy', window='diagram_1')
        evs, _ = col(is_idle)
        check('autoview: closing AUTO resets it', av(evs) and av(evs)[-1]['earlier'] == 0
              and av(evs)[-1]['views'] == [{'name':'diagram_1','zoom': {'x': None, 'y': None}}], str(av(evs)))
    finally:
        stop_server(p, r, snd)


def check_silent_commands():
    """W56: --silent is a built-in script of the protocol's own commands.
    Each command it uses where the interface had none (browser.export of
    the output columns with replace, write_values_query's
    file, dfield write, equilibrium write) writes, through --server, the very
    file the --silent run writes; its .recx also plays silently with the
    same success/error result. --silent prints nothing on stdout."""
    def silent(ode, *flags):
        d = tempfile.mkdtemp(prefix='xppsilent')
        copy_model(ode, d)
        r = subprocess.run([os.path.abspath(args.server), os.path.basename(ode), '--silent'] + list(flags), cwd=d,
                           capture_output=True, text=True, timeout=60 * SLOW)
        return d, r

    def script(ode, lines, before=None):
        d = tempfile.mkdtemp(prefix='xppscript')
        copy_model(ode, d)
        if before:
            before(d)
        r = run_commands(args.server, ode, lines, d)
        path = make_recording(args.server, ode, lines, d)
        quiet = subprocess.run([os.path.abspath(args.server), path, '--silent', '--yes'], cwd=d,
            capture_output=True, text=True, encoding='utf-8', timeout=60 * SLOW)
        r.returncode = quiet.returncode
        r.stderr = quiet.stderr
        if quiet.stdout:
            raise AssertionError('silent recording emitted interface events')
        return d, r

    def read(d, n):
        path = os.path.join(d, n)
        return open(path, 'rb').read().replace(b'\r', b'') if os.path.exists(path) else None

    go = [{'cmd': 'run.initial'}]
    dirs = []
    try:
        s, r = silent(args.ode, '--qsets', '--qpars', '--qics', '--outfile', 'q.txt')
        dirs.append(s)
        query = read(s, 'q.txt') or b''
        check('--silent --qsets --qpars --qics writes the internal sets, the parameters and the initial conditions, each under its heading',
              r.returncode == 0 and b'#Internal sets query:' in query and b'#Parameters query:' in query
              and b'#Initial conditions query:' in query, r.stderr[-300:] + repr(query[:80]))

        s, r = silent(args.ode, '--dfdraw', '4', '--equil', '0')
        dirs.append(s)
        check('--silent writes nothing on stdout', r.returncode == 0 and r.stdout == '', r.stdout[:300])
        stale = lambda d: open(os.path.join(d, 'output.dat'), 'w').write('stale\n')
        c, rc = script(args.ode, go + [
            {'cmd': 'browser.export', 'target': 'output', 'format': 'dat', 'file': 'output.dat'},
            {'cmd': 'equilibrium.export', 'file': 'equil.dat'},
            {'cmd': 'dirfield.compute', 'grid': 16, 'scaled': False},
            {'cmd': 'dirfield.export', 'file': 'dirfields.dat'}])
        dirs.append(c)
        check('the script with --silent\'s commands runs through', rc.returncode == 0, rc.stderr[-300:])
        check('browser.export of the output columns: --silent\'s output.dat',
              read(c, 'output.dat') is not None and read(c, 'output.dat') == read(s, 'output.dat'))
        # --silent replaces the file that is there, unasked
        over, ro = silent(args.ode)
        dirs.append(over)
        stale(over)
        ro = subprocess.run([os.path.abspath(args.server), os.path.basename(args.ode), '--silent'], cwd=over,
                            capture_output=True, text=True, timeout=60 * SLOW)
        check('--silent writes its output.dat over the file there, unasked',
              ro.returncode == 0 and read(over, 'output.dat') == read(s, 'output.dat') and 'Replace it' not in ro.stderr, ro.stderr[-300:])
        check('equilibrium write: --silent --equil 0\'s equil.dat',
              read(c, 'equil.dat') is not None and read(c, 'equil.dat') == read(s, 'equil.dat'))
        df = read(c, 'dirfields.dat') or b''
        check('dfield write: the direction field shown, one arrow a line (17 x 17), in --silent --dfdraw 4\'s form',
              len(df.splitlines()) == 17 * 17 and df == read(s, 'dirfields.dat'),
              '%d lines' % len(df.splitlines()))
        bad, rb = script(args.ode, [{'cmd': 'dirfield.export', 'file': 'none.dat'}])
        dirs.append(bad)
        check('dfield write with no field shown is an error, and writes nothing',
              rb.returncode == 1 and rb.outcome == 1 and '"error"' in rb.stdout and read(bad, 'none.dat') is None, rb.stdout[-300:])

        dirs.append(tempfile.mkdtemp(prefix='xpppost'))
        post = os.path.join(dirs[-1], 'post.odex')
        with open(post, 'w') as f:
            f.write("x'=-x+sin(t)\ninit x=0.5\noptions { integration: { total_time: 50, dt: .05 }, analysis: { postprocess: 1, histcol: x, histlo: -1, histhi: 1, histbins: 20 } }\n")
        s, r = silent(post)
        dirs.append(s)
        c, rc = script(post, go + [{'cmd': 'analysis.histogram'},
                                   {'cmd': 'browser.export', 'target': 'output', 'format': 'dat',
                                    'file': 'output.dat'}])
        dirs.append(c)
        hist = read(c, 'output.dat') or b''
        check('analysis.histogram: the model\'s @ postprocess histogram (20 bins), as --silent writes it',
              len(hist.splitlines()) == 21 and hist == read(s, 'output.dat'), '%d lines' % len(hist.splitlines()))
    finally:
        for d in dirs:
            shutil.rmtree(d, ignore_errors=True)


check_silent_commands()

def check_export_elements():
    """W346: run requests populate the session before any export, in every mode."""
    import xml.etree.ElementTree as ET
    from unittest.mock import patch
    from collections import Counter

    model = 'tools/models/export-elements.ode'
    binary = os.path.abspath(args.server)
    go = [{'cmd': 'run.initial'}]

    def elements(data, fmt):
        """the picture's items as (kind, colour, text): a path or a dot (a marker) in its colour, a text in its colour"""
        text = data.decode('utf-8')
        if fmt == 'svg':
            result = []
            for node in ET.fromstring(text).iter():
                tag = node.tag.rsplit('}', 1)[-1]
                if tag == 'polyline':
                    result.append(('path', node.get('stroke'), ''))
                elif tag == 'circle':
                    result.append(('dot', node.get('fill'), ''))
                elif tag == 'text':
                    result.append(('text', node.get('fill'), ''.join(node.itertext()).strip()))
            return result
        hexed = lambda r, g, b: '#%02x%02x%02x' % tuple(round(float(v) * 255) for v in (r, g, b))
        result, stroke, fill, curved = [], '#000000', '#000000', False
        for line in text.split('stream\n', 1)[1].splitlines():
            m = re.fullmatch(r'(\S+) (\S+) (\S+) (RG|rg)', line)
            if m:
                if m.group(4) == 'RG':
                    stroke = hexed(*m.group(1, 2, 3))
                else:
                    fill = hexed(*m.group(1, 2, 3))
            elif line.endswith(' c'):
                curved = True
            elif line == 'S':
                result.append(('path', stroke, ''))
            elif line == 'f':
                result.append(('dot' if curved else 'fill', fill, ''))
                curved = False
            elif line.endswith(' Tj'):
                result.append(('text', fill, line[line.index('('):]))
        return result

    with tempfile.TemporaryDirectory(prefix='xppexport') as root, patch.dict(os.environ, HOME=root):
        server = Server(binary, model, run=root)
        try:
            server.collect(is_idle)
            server.send(cmd='events.subscribe', events=['nullclines', 'dfield'])
            server.collect(is_idle)
            server.send(**go[0]); events, _ = server.collect(is_idle)
            nc = next((e for e in events if e.get('ev') == 'nullclines' and e.get('x') and e.get('y')), None)
            df = next((e for e in events if e.get('ev') == 'dfield' and e.get('grid')), None)
            check('W346: a run publishes requested nullclines and field without a separate command',
                  nc is not None and df is not None, str(events[-3:])[:300])
            expected_files = {}
            picture_values = []
            for fmt in ('pdf', 'svg'):
                name = 'server.' + fmt
                def form(e):
                    picture_values[:] = e['values']
                    return {'ok': 1, 'values': e['values']}
                server.send(cmd='plot.export_'+fmt, file=name)
                server.answer_asks(is_idle, {
                    'choice': lambda e: {'key': 'p' if fmt == 'pdf' else 'v'},
                    'form': form, 'file': lambda e: {'file': name}})
                with open(os.path.join(root, name), 'rb') as f:
                    expected_files[fmt] = elements(f.read(), fmt)
        finally:
            server.close()
        for fmt in ('pdf', 'svg'):
            expected = expected_files[fmt]
            result = subprocess.run([binary, os.path.basename(model), '--silent', '--yes', '--mkplot', '--plotfmt', fmt],
                                    cwd=root, capture_output=True, text=True, timeout=60 * SLOW)
            # batch_plot_name uses the loaded (converted) model's basename.
            pictures = glob.glob(os.path.join(root, 'export-elements*.' + fmt))
            actual = elements(open(pictures[0], 'rb').read(), fmt) if pictures else []
            check('W346: silent/server ' + fmt + ' element lists agree',
                  result.returncode == 0 and bool(actual) and actual == expected,
                  str((len(expected), len(actual), next(((a, b) for a, b in zip(expected, actual) if a != b), None)))
                  + result.stderr[-300:])
            counts = Counter((kind, colour) for kind, colour, _ in actual)
            check('W346: ' + fmt + ' holds both nullclines (XPPAUT\'s colours 2 and 7) and label text',
                  counts['path', '#f06400'] > 0 and counts['path', '#00e100'] > 0
                  and any(kind == 'text' for kind, _, _ in actual), str(counts))
            # each arrow is a path with a dot at its head
            check('W346: ' + fmt + ' holds all 25 requested field arrows', counts['dot', '#000000'] == 25, str(counts))
        annotations = go + [dict(cmd='annotation.add_text',window='graph_1',x=.2,y=.4,text='W346label'),
            dict(cmd='plot.freeze_curve',color='#ffcd00',draw='line',dash='solid',width=1,marker='circle',marker_size=3,key='W346frozen',name='frz1')]
        for fmt in ('pdf','svg'): annotations.append(dict(cmd='plot.export_'+fmt,file='annotated.'+fmt))
        result = run_commands(binary, model, annotations, root)
        annotated = {}
        for fmt in ('pdf', 'svg'):
            with open(os.path.join(root, 'annotated.' + fmt), 'rb') as f:
                annotated[fmt] = elements(f.read(), fmt)
        recording = make_recording(binary, model, annotations, root)
        result = subprocess.run([binary, recording, '--silent', '--yes'], cwd=root,
                                capture_output=True, text=True, timeout=60 * SLOW)
        for fmt in ('pdf', 'svg'):
            with open(os.path.join(root, 'annotated.' + fmt), 'rb') as f:
                replayed = elements(f.read(), fmt)
            check('W346: silent/server ' + fmt + ' preserve a label and frozen curve',
                  result.returncode == 0 and replayed == annotated[fmt]
                  and any('W346label' in text for _, _, text in replayed)
                  and len(replayed) > len(expected_files[fmt]), result.stderr[-300:])
        # ncdraw=2 and dfdraw=4 use the ordinary writer in both modes.
        server = Server(binary, model, run=root, extra_args=('--ncdraw', '2', '--dfdraw', '4'))
        try:  # a registry run carries no replace permission: the files it replaces are answered
            server.collect(is_idle)
            server.send(**go[0])
            server.answer_asks(is_idle, {'choice': lambda e: {'key': 'y'}})
        finally:
            server.close()
        with open(os.path.join(root, 'nullclines.dat'), 'rb') as f:
            dump = f.read()
        with open(os.path.join(root, 'dirfields.dat'), 'rb') as f:
            field_dump = f.read()
        result = subprocess.run([binary, os.path.basename(model), '--silent', '--yes', '--ncdraw', '2', '--dfdraw', '4'],
                                cwd=root, capture_output=True, text=True, timeout=60 * SLOW)
        check('W346: ncdraw=2 dumps the same nullclines in server and silent',
              result.returncode == 0 and dump and dump.splitlines()[0].split()[0] in (b'1', b'2') and len(dump.splitlines()[0].split()) == 5
              and dump == open(os.path.join(root, 'nullclines.dat'), 'rb').read(), result.stderr[-300:])
        check('W346: dfdraw=4 dumps the same 25 arrows in server and silent',
              len(field_dump.splitlines()) == 25
              and field_dump == open(os.path.join(root, 'dirfields.dat'), 'rb').read())


check_export_elements()


def check_outcomes_once():
    """W133: results, file places, film failures, and every exit route."""
    d = tempfile.mkdtemp(prefix='xppoutcomes')
    try:
        with open(os.path.join(d, 'linear.odex'), 'w') as f:
            f.write("par a=1\nx'=a\ny'=a\ninit x=0,y=0\naux type=1\naux branch=1\naux f2=0\noptions { integration: { dt: .1, total_time: 1 } }\n")
        with open(os.path.join(d, 'fit.dat'), 'w') as f:
            f.write(''.join('%d %d\n' % (t, 2*t) for t in range(6)))
        def run(lines, *flags):
            if any((c.get('cmd') == 'run.abort' and 'at' in c) or c.get('cmd') == 'kinescope.capture' for c in lines):
                path = make_recording(args.server, os.path.join(d, 'linear.odex'), lines, d)
                with open(path, encoding='utf-8') as f:
                    run.places = [(i, l.rstrip()) for i, l in enumerate(f, 1) if l.startswith('{')]
                r = replay_recording(args.server, os.path.join(d, 'linear.odex'), path, d,
                    pixel_answers=[c for c in lines if 'rgb' in c])
            else:
                r = run_commands(args.server, os.path.join(d, 'linear.odex'), lines, d, flags)
                if not any('rgb' in c for c in lines):
                    path = make_recording(args.server, os.path.join(d, 'linear.odex'), lines, d)
                    quiet = subprocess.run([os.path.abspath(args.server), path, '--silent', '--yes', *flags],
                        cwd=d, capture_output=True, text=True, encoding='utf-8', timeout=60 * SLOW)
                    r.returncode = quiet.returncode
                    r.quiet_stdout = quiet.stdout
            return r, [json.loads(line) for line in r.stdout.splitlines() if line.startswith('{')]
        def fit(name):
            return [dict(cmd='analysis.fit_curve',variables=['x'],parameters=['a'],tolerance=1e-6,points=6,columns=2,data_columns=[2],epsilon=1e-5,max_iterations=50,file=name)]
        r, evs = run(fit('fit.dat'))
        results = [e.get('bottom', '').strip() for e in evs if e.get('ev') in ('message', 'done')]
        errors = [e for e in evs if e.get('error')]
        check('a converged script fit exits 0 and sends exactly one info outcome, no error dialog event',
              r.returncode == 0 and results.count('Success!') == 1 and not errors, str((r.returncode, results, errors, r.stderr[-300:])))
        saved_csv = [
            {'cmd': 'value.set_parameters', 'values': {'a': 2}},
            {'cmd': 'run.initial'},
            {'cmd': 'browser.export', 'target': 'table', 'format': 'csv', 'file': 'saved.csv'},
            {'cmd': 'value.set_parameters', 'values': {'a': 1}}]
        r, evs = run(saved_csv + fit('saved.csv'))
        results = [e.get('bottom', '').strip() for e in evs if e.get('ev') in ('message', 'done')]
        errors = [e for e in evs if e.get('error')]
        states = [e for e in evs if e.get('ev') == 'state']
        check('curve fit reads a CSV written by Save data and recovers the parameter',
              r.returncode == 0 and results.count('Success!') == 1 and not errors
              and states and abs(dict(states[-1]['pars'])['a'] - 2) < 1e-4,
              str((r.returncode, results, errors, states[-1].get('pars') if states else None)))
        STOP_MOVIE_ROWS = 15  # after the first 11-row frame, inside the second integration
        # Arm Stop while answering the first captured frame; the second
        # integration stops at row 15, after a real frame reached the temp file.
        range_movie = [
            {'cmd': 'run.initial'},
            {'cmd': 'plot.set_variables', 'how': 'array_plot', 'names': ['x', 'y']},
            dict(cmd='array_plot.range',still=False,tag=False,file='movie.gif',variable='a',steps=2,start=1,end=3,reset_storage=False,use_old_ic=True,cycle_color=False,movie=False,write_each_run_to_file=False),
            {'cmd': 'answer', **pixels(None)},
            {'cmd': 'run.abort', 'at': {'what': 'integrate', 'rows': STOP_MOVIE_ROWS, 't': 0}}]
        movie_path = os.path.join(d, 'movie.gif')
        for old in [None, b'previous complete movie']:
            if old is not None:
                with open(movie_path, 'wb') as f:
                    f.write(old)
            r, evs = run(range_movie)
            saved = [e for e in evs if e.get('ev') == 'saved']
            got = open(movie_path, 'rb').read() if os.path.exists(movie_path) else None
            check('stopped range movie preserves %s and reports no successful save' % ('the old file' if old else 'an absent file'),
                  r.returncode == 0 and any(e.get('ev') == 'stopped' for e in evs)
                  and len(saved) == 1 and saved[0].get('saved') is False and got == old
                  and not glob.glob(os.path.join(d, '*.tmp-*')),
                  str((r.returncode, saved, got, r.stderr[-300:])))
        r, evs = run(range_movie[:-1] + [{'cmd': 'answer', **pixels(None)}] * 2, '--yes')
        saved = [e for e in evs if e.get('ev') == 'saved']
        got = open(movie_path, 'rb').read() if os.path.exists(movie_path) else b''
        check('completed range movie replaces the old file with a complete GIF',
              r.returncode == 0 and len(saved) == 1 and saved[0].get('saved') is True
              and got.startswith(b'GIF') and got.endswith(b';')
              and not glob.glob(os.path.join(d, '*.tmp-*')),
              str((r.returncode, saved, len(got), r.stderr[-300:])))
        r, evs = run(fit('missing.dat') + [{'cmd': 'session.quit'}, {'cmd': 'answer', 'key': 'd'}])
        errors = [e for e in evs if e.get('error')]
        check('an unreadable fit file has its Place; the asking quit also exits 1, without bye',
              r.returncode == 1 and len(errors) == 1 and errors[0].get('file') == 'missing.dat'
              and errors[0].get('line') == 0 and r.quiet_stdout == '', str(errors))
        r, evs = run([dict(cmd='animation.load',file='missing.ani')])
        errors = [e for e in evs if e.get('error')]
        check('an unreadable animation names its file at line 0 once',
              r.returncode == 1 and len(errors) == 1 and errors[0].get('file') == 'missing.ani'
              and errors[0].get('line') == 0, str(errors))
        # W500: a bad number in an animation file is a load error at its file and line (no value is applied)
        with open(os.path.join(d, 'bad.ani'), 'w') as f:
            f.write('line 0;0;1;1;$RED;thick_bad\n')
        r, evs = run([dict(cmd='animation.load',file='bad.ani')])
        errors = [e for e in evs if e.get('error')]
        check('an animation line with a bad number names its file, line and the value',
              r.returncode == 1 and len(errors) == 1 and errors[0].get('file') == 'bad.ani'
              and errors[0].get('line') == 1 and 'thick_bad' in errors[0].get('error', ''), str(errors))
        # Kinescope capture has a fixed capacity; its last attempted frame fails once.
        capture = [dict(cmd='kinescope.capture')]
        r, evs = run(capture * 251)
        errors = [e for e in evs if e.get('error')]
        frames = [e for e in evs if e.get('ev') == 'film' and e.get('op') == 'capture']
        check('the full kinescope returns one error at its command, retaining 250 frames',
              r.returncode == 1 and len(frames) == 250 and len(errors) == 1
              and errors[0].get('file', '').endswith('run.recx')
              and (errors[0].get('line'), errors[0].get('source')) == run.places[-1], str(errors))
        r, evs = run([{'cmd': 'file', 'op': 'get', 'name': '../outside.dat'}])
        check('a file command refusal also counts towards exit status without a second event',
              r.returncode == 1 and len([e for e in evs if e.get('error')]) == 1, str(evs[-3:]))
        # A directory at the output path makes the write fail on every platform.
        os.mkdir(os.path.join(d, 'output.dat'))
        r = subprocess.run([os.path.abspath(args.server), 'linear.odex', '--silent'], cwd=d,
                           capture_output=True, text=True, timeout=60 * SLOW)
        check('a failed silent output write exits 1 and keeps stdout empty',
              r.returncode == 1 and r.stdout == '' and 'output.dat' in r.stderr, str((r.returncode, r.stderr[-300:])))
        os.rmdir(os.path.join(d, 'output.dat'))
        # Windows directory readonly attributes do not deny writes; use its ACL.
        if os.name == 'nt':
            denied = subprocess.run(['icacls', d, '/deny', '*S-1-1-0:(W)'], capture_output=True)
            try:
                r = subprocess.run([os.path.abspath(args.server), 'linear.odex', '--silent'], cwd=d,
                                   capture_output=True, text=True, timeout=60 * SLOW)
                check('silent in a read-only folder exits 1 for output.dat',
                      denied.returncode == 0 and r.returncode == 1 and 'output.dat' in r.stderr,
                      str((denied.returncode, r.returncode, r.stderr[-300:])))
            finally:
                subprocess.run(['icacls', d, '/remove:d', '*S-1-1-0'], capture_output=True, check=True)
        elif os.geteuid() != 0:
            os.chmod(d, 0o555)
            try:
                r = subprocess.run([os.path.abspath(args.server), 'linear.odex', '--silent'], cwd=d,
                                   capture_output=True, text=True, timeout=60 * SLOW)
                check('silent in a read-only folder exits 1 for output.dat',
                      r.returncode == 1 and 'output.dat' in r.stderr, str((r.returncode, r.stderr[-300:])))
            finally:
                os.chmod(d, 0o755)
    finally:
        shutil.rmtree(d, ignore_errors=True)

check_outcomes_once()



def check_dae_fold():
    """W127: a DAE run stops at a fold instead of stepping over it.
    dae_ex3.odex (w'=v, 0=v(1-v^2)-w, v(0)=1) reaches the fold v=1/sqrt(3) at
    t*=0.450694, past which its branch has no solution (docs/w126-dae-check.md):
    the run keeps the rows to t=0.45 (dt .05: 10 rows) and says once, as an
    error, that there is no solution past the last time solved. dae.odex, a
    DAE with no fold, runs to its end (total 20, dt .05: 401 rows)."""
    GO = [{'cmd': 'run.initial'}]
    SING = [{'cmd': 'equilibrium.find'}]

    def run(ode, lines=GO):
        code, out, err = run_replay(lines, ode=ode)
        evs = [json.loads(l) for l in out.splitlines() if l.strip()]
        errs = [e['error'] for e in evs if e.get('ev') in ('message', 'done') and 'error' in e]
        return code, errs, (last_state(evs) or {}).get('rows'), err

    code, errs, rows, err = run('examples/ode/dae_ex3.odex')
    # .recx --silent exits 1 after an error message (docs/protocol.md "Playing a recording without an interface")
    check('dae_ex3 stops at the fold: the rows to t=0.45, then one error',
          code == 1 and rows == 10 and len(errs) == 1, 'exit %d, rows %s, %s %s' % (code, rows, errs, err[-200:]))
    check('... saying there is no solution past t=0.45, a fold',
          len(errs) == 1 and errs[0].startswith('No solution of the algebraic equations past t=0.45:')
          and 'fold' in errs[0], str(errs))
    code, out, err = run_replay(GO, ode='examples/ode/dae_ex3.odex')
    fold = next((json.loads(l) for l in out.splitlines() if 'No solution of the algebraic' in l), {})
    check('... at the 0= line, the place of every error (W140b)',
          fold.get('file') == 'dae_ex3.odex' and fold.get('line') == 5 and fold.get('source') == '0 = v_*(1-v_*v_)-w',
          str(fold))
    code, errs, rows, err = run('examples/ode/dae.odex')
    check('dae.odex, a DAE without a fold, runs to its end with no error',
          code == 0 and rows == 401 and not errs, 'exit %d, rows %s, %s %s' % (code, rows, errs, err[-200:]))

    # The branch is the run's alone (DaeRun): Sing pts after it solves the
    # algebraic equations without one, as before W127, both after the run
    # that stopped at the fold and after one that ended on the branch
    # (total .3, before the fold). dae_ex3's equilibrium, w=0 and v=0, is on
    # the middle branch, which Newton from the run's v (the outer branch)
    # does not reach, before W127 or since: "No equilibrium from W = 0",
    # and no DAE error of the search's own.
    code, errs, rows, err = run('examples/ode/dae_ex3.odex', GO + SING)
    check('Sing pts right after the run stopped at the fold: as before W127',
          rows == 10 and len(errs) == 2 and 'fold' in errs[0] and errs[1].startswith('No equilibrium from W = 0: Newton'), str(errs))
    short_dir = tempfile.mkdtemp(prefix='xppdae')
    try:
        short = os.path.join(short_dir, 'dae_ex3.odex')
        with open('examples/ode/dae_ex3.odex') as f:
            text = f.read().replace('integration: {', 'integration: { total_time: .3,', 1)
        with open(short, 'w') as f:
            f.write(text)
        code, errs, rows, err = run(short, GO + SING)
        check('Sing pts after a run that ended on its branch: as before W127',
              rows == 7 and len(errs) == 1 and errs[0].startswith('No equilibrium from W = 0: Newton'), 'rows %s, %s' % (rows, errs))
    finally:
        shutil.rmtree(short_dir, ignore_errors=True)

check_dae_fold()


def check_session_random():
    """W123: a session file holds the random generator's state (random.txt):
    Go on a stochastic model, save, open in a new server and Continue gives
    the rows Continue gives without the save; a file without the member or
    with one that is no generator state is refused naming it; a recording's
    snapshot holds it, and its replay writes the same rows"""
    import zipfile
    ode = 'tools/models/stoch_rng.odex'
    is_ask = lambda e: e.get('ev') == 'ask'

    def mk():
        p, r, snd, col, _ = launch_server(ode=ode)
        col(is_idle)

        def run(**cmd):
            snd(**cmd)
            evs = []
            while True:
                got, e = col(lambda e: is_ask(e) or is_idle(e), timeout=60 * SLOW)
                evs += got
                if e is None or is_idle(e):
                    return evs
                snd(cmd='answer', id=e['id'], key='d' if e.get('keys') == 'sd' else 'g')

        return p, r, snd, col, run

    def write(run, name):
        run(cmd='browser.export', target='output', format='dat', file=name)

    def read(r, name):
        path = os.path.join(r, name)
        return open(path, 'rb').read() if os.path.exists(path) else None

    keep = tempfile.mkdtemp(prefix='xpprandom')
    p, r, snd, col, run = mk()
    try:
        # a recording of a Go and a Continue after an earlier Go: its snapshot
        # holds the generator and the next Go's seed
        run(cmd='run.initial')
        run(cmd='recording.start')
        run(cmd='run.initial')
        run(cmd='run.continue')
        write(run, 'rec.dat')
        run(cmd='recording.stop', file='rr')
        rec = read(r, 'rec.dat')
        text = open(os.path.join(r, 'rr.recx'), encoding='utf-8').read()
        shutil.copy(os.path.join(r, 'rr.recx'), keep)
        snap = recx_snapshot(text) or {}
        check('random: a recording snapshot holds random.txt', 'random.txt' in snap, str(sorted(snap)))
        run(cmd='run.initial')
        run(cmd='session.save_as', file='r1')
        run(cmd='run.continue')
        write(run, 'a.dat')
        a = read(r, 'a.dat')
        z = zipfile.ZipFile(os.path.join(r, 'r1.snapx'))
        check('random: a session file lists random.txt', 'random.txt' in z.namelist(), str(z.namelist()))
        members = {n: z.read(n) for n in z.namelist()}
        shutil.copy(os.path.join(r, 'r1.snapx'), keep)
    finally:
        stop_server(p, r, snd)

    p, r, snd, col, run = mk()
    try:
        shutil.copy(os.path.join(keep, 'r1.snapx'), r)
        run(cmd='session.open_model', file='r1.snapx')
        run(cmd='run.continue')
        write(run, 'b.dat')
        b = read(r, 'b.dat')
        check('random: save, open, Continue gives the rows of Continue without the save',
              a is not None and a == b, '%s vs %s bytes' % (len(a or b''), len(b or b'')))
        for name, change, expect in (('norandom', lambda m: m.pop('random.txt'), 'its random.txt is missing'),
                                     ('noseed', lambda m: m.__setitem__('random.txt', m['random.txt'].split(b'\n', 1)[1]),
                                      ('noseed.snapx/random.txt:1:', 'not "seed <number>"')),
                                     ('badrandom', lambda m: m.__setitem__('random.txt', b'seed 5\nwiener 0\nnot a state'),
                                      ('badrandom.snapx/random.txt:3:', 'not a random generator'))):
            damaged = dict(members)
            change(damaged)
            with zipfile.ZipFile(os.path.join(r, name + '.snapx'), 'w') as out:
                for n, bts in damaged.items():
                    out.writestr(n, bts)
            evs = run(cmd='session.open_model', file=name + '.snapx')
            msgs = ' '.join(placed(e) for e in evs if e.get('ev') in ('message', 'done'))
            check('random: %s.snapx is refused, the error names the member' % name,
                  all(x in msgs for x in (expect if isinstance(expect, tuple) else (expect,))), msgs[:300])
    finally:
        stop_server(p, r, snd)

    p, r, snd, col, run = mk()
    try:
        shutil.copy(os.path.join(keep, 'rr.recx'), r)
        snd(cmd='recording.open', play=False, file='rr.recx')
        evs, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='d')
            col(is_idle, timeout=30 * SLOW)
        run(cmd='recording.set_speed', speed=8)
        snd(cmd='recording.play')
        got, _ = col(lambda e: e.get('ev') == 'state' and (e.get('player') or {}).get('step') == 3
                     and e['player']['running'] == -1, timeout=60 * SLOW)
        got += col(is_idle, timeout=10 * SLOW)[0]
        snd(cmd='file', op='get', name='rec.dat')
        _, ev = col(lambda e: e.get('ev') == 'file')
        col(is_idle)
        again = base64.b64decode(ev['data']) if ev and ev.get('ok') else None
        check('random: the recording replays the rows it wrote', rec is not None and again == rec,
              '%s vs %s bytes' % (len(rec or b''), len(again or b'')))
    finally:
        stop_server(p, r, snd)
        shutil.rmtree(keep, ignore_errors=True)


def check_save_session():
    """W209: Save session (File item savesession, Ctrl+S) writes the session
    file the Session remembers with no ask, and asks for a path when it has
    none; Save session as (savesessionas, Ctrl+Shift+S) always asks, and asks
    before replacing a file; state.changed follows the values (a parameter, an
    initial condition, a numerics setting, a run's data) and a save or a load
    clears it"""
    p, r, snd, col, _ = launch_server()

    def run(*answers, **cmd):
        """cmd, its asks answered in order (a dict is the answer; then cancel), up to idle; the events"""
        snd(**cmd)
        got, n = [], 0
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=60 * SLOW)
            got.extend(evs)
            if e is None or e['ev'] in ('idle','done'):
                return got
            a = answers[n] if n < len(answers) else {'ok': 0}
            n += 1
            snd(cmd='answer', id=e['id'], **a)

    state = lambda evs: next((e for e in reversed(evs) if e.get('ev') == 'state'), {})
    asks = lambda evs: [e for e in evs if e.get('ev') == 'ask']
    saved = lambda evs: [e for e in evs if e.get('ev') == 'saved']
    snapx = lambda name: os.path.join(r, name)
    try:
        col(is_idle)
        evs = run(cmd='events.subscribe', events=['series'])
        check('W209: a loaded model is unchanged and has no session file',
              state(evs).get('changed') is False and 'session' not in state(evs), str(state(evs).get('changed')))
        evs = run({'file': 'sv1'}, cmd='session.save')
        a = asks(evs)
        check('W209: the first Save session asks for a path (the file picker), as Save as does',
              len(a) == 1 and a[0].get('kind') == 'file' and a[0].get('wild') == '*.snapx', str(a))
        check('W209: it writes sv1.snapx and the Session remembers it, unchanged',
              os.path.exists(snapx('sv1.snapx')) and state(evs).get('session') == {'file': 'sv1.snapx'}
              and state(evs).get('changed') is False, str(state(evs)))
        evs = run(cmd='value.set_parameters', values={'phi': 0.4})
        check('W209: a parameter edit sets changed', state(evs).get('changed') is True, str(state(evs).get('changed')))
        before = open(snapx('sv1.snapx'), 'rb').read()
        evs = run(cmd='session.save')
        after = open(snapx('sv1.snapx'), 'rb').read()
        check('W209: the second Save session asks nothing, replaces sv1.snapx and clears changed',
              not asks(evs) and saved(evs) and saved(evs)[-1].get('saved') is True and before != after
              and state(evs).get('changed') is False and state(evs).get('session') == {'file': 'sv1.snapx'}, str(asks(evs))[:200])
        for what, command in (('an initial condition', dict(cmd='ic.set', values={'V': -0.3})),
                              ('the time span', dict(cmd='run.configure', total_time=77))):
            evs = run(**command)
            check('W209: %s sets changed' % what, state(evs).get('changed') is True, str(state(evs).get('changed')) + str(evs[-4:])[:200])
            evs = run(cmd='session.save')
            check('W209: Save session after %s clears it' % what, state(evs).get('changed') is False)
        evs = run(cmd='run.initial')
        check('W209: a run changes the data, so changed', state(evs).get('changed') is True, str(state(evs).get('changed')))
        evs = run({'file': 'sv1'}, {'key': 'n'}, cmd='session.save_as', file_picker=True, confirm=False)
        a = asks(evs)
        check('W209: Save session as asks for a path, then before replacing the file that is there',
              len(a) == 2 and a[0].get('kind') == 'file' and a[1].get('kind') == 'choice', str(a)[:300])
        check('W209: declining the replacement saves nothing and keeps changed',
              saved(evs) and saved(evs)[-1].get('saved') is False and state(evs).get('changed') is True, str(saved(evs)))
        evs = run({'file': 'sv2'}, cmd='session.save_as', file_picker=True, confirm=False)
        check('W209: Save session as a new name writes it, remembers it and clears changed',
              os.path.exists(snapx('sv2.snapx')) and state(evs).get('session') == {'file': 'sv2.snapx'}
              and state(evs).get('changed') is False, str(state(evs)))
        evs = run(cmd='value.set_parameters', values={'phi': 0.5})
        evs = run(cmd='session.save')
        check('W209: Save session now goes to sv2.snapx, asking nothing',
              not asks(evs) and state(evs).get('session') == {'file': 'sv2.snapx'} and state(evs).get('changed') is False, str(asks(evs))[:200])
        # W218: Reload keeps the session file; Save a copy leaves file and changed alone
        evs = run({'key': 'd'}, cmd='session.reload_model')
        check('W218: Reload keeps the session file, and its dropped data read as changed',
              state(evs).get('session') == {'file': 'sv2.snapx'} and state(evs).get('changed') is True, str(state(evs).get('session')) + str(state(evs).get('changed')))
        evs = run(cmd='session.save')
        check('W218: Save session after Reload asks nothing and writes sv2.snapx',
              not asks(evs) and saved(evs) and saved(evs)[-1].get('saved') is True and state(evs).get('changed') is False
              and state(evs).get('session') == {'file': 'sv2.snapx'}, str(asks(evs))[:200])
        evs = run(cmd='value.set_parameters', values={'phi': 0.7})
        evs = run({'file': 'cp1'}, cmd='session.save_copy', file_picker=True, confirm=False)
        a = asks(evs)
        check('W218: Save a copy asks for a path (the file picker), writes it, and leaves the file and changed as they were',
              len(a) == 1 and a[0].get('kind') == 'file' and a[0].get('wild') == '*.snapx' and os.path.exists(snapx('cp1.snapx'))
              and state(evs).get('session') == {'file': 'sv2.snapx'} and state(evs).get('changed') is True, str(a)[:200] + str(state(evs).get('session')))
        copy_bytes = open(snapx('cp1.snapx'), 'rb').read()
        evs = run({'file': 'cp1'}, {'key': 'n'}, cmd='session.save_copy', file_picker=True, confirm=False)
        a = asks(evs)
        check('W218: Save a copy of a file that exists asks before replacing it; declined, nothing changes',
              len(a) == 2 and a[1].get('kind') == 'choice' and saved(evs) and saved(evs)[-1].get('saved') is False
              and open(snapx('cp1.snapx'), 'rb').read() == copy_bytes and state(evs).get('changed') is True
              and state(evs).get('session') == {'file': 'sv2.snapx'}, str(a)[:300])
        evs = run(cmd='session.save')
        check('W218: Save session after a copy still writes sv2.snapx, not the copy',
              not asks(evs) and state(evs).get('session') == {'file': 'sv2.snapx'} and state(evs).get('changed') is False
              and open(snapx('cp1.snapx'), 'rb').read() == copy_bytes, str(asks(evs))[:200])
        evs = run(cmd='value.set_parameters', values={'phi': 0.6})
        evs = run({'key': 'd'}, cmd='session.open', file='sv1')
        check('W209: opening a session file clears changed and names it',
              state(evs).get('changed') is False and state(evs).get('session', {}).get('file', '').endswith('sv1.snapx'),
              str(state(evs).get('session')) + str(state(evs).get('changed')))
        # W210: undo and redo of the value edits (a Reload starts the stacks empty)
        evs = run({'key': 'd'}, cmd='session.reload_model')
        pars = lambda evs: {k: v for k, v in state(evs)['pars']}
        ics = lambda evs: {k: v for k, v in state(evs)['ics']}
        errs = lambda evs: [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
        undo = lambda: run(cmd='value.undo')
        redo = lambda: run(cmd='value.redo')
        edit = lambda v: run(cmd='value.set_parameters', values={'phi': v})
        phi0 = pars(evs)['phi']
        check('W210: a loaded model has nothing to undo or redo',
              state(evs).get('can_undo') is False and state(evs).get('can_redo') is False, str(state(evs).get('can_undo')))
        evs = undo()
        check('W210: Undo with nothing to undo is an error naming the command, and changes nothing',
              len(errs(evs)) == 1 and 'undo' in str(errs(evs)[0]) and pars(evs)['phi'] == phi0, str(errs(evs))[:200])
        edit(0.1)
        evs = edit(0.2)
        check('W210: an edit can be undone', state(evs).get('can_undo') is True and state(evs).get('can_redo') is False)
        evs = undo()
        check('W210: Undo takes back the last edit, in one step', pars(evs)['phi'] == 0.1 and state(evs).get('can_redo') is True, str(pars(evs)['phi']))
        evs = redo()
        check('W210: Redo puts it back', pars(evs)['phi'] == 0.2 and state(evs).get('can_redo') is False, str(pars(evs)['phi']))
        undo()
        evs = undo()
        check('W210: Undo twice reaches the first values, and the stack is then empty',
              pars(evs)['phi'] == phi0 and state(evs).get('can_undo') is False and state(evs).get('can_redo') is True, str(pars(evs)['phi']))
        evs = edit(0.3)
        check('W210: a new edit forgets what could be redone', state(evs).get('can_redo') is False and state(evs).get('can_undo') is True)
        evs = redo()
        check('W210: Redo with nothing to redo is an error', len(errs(evs)) == 1 and 'redo' in str(errs(evs)[0]) and pars(evs)['phi'] == 0.3)
        evs = run(cmd='value.set_parameters', values={'phi': 0.4})
        evs = run(cmd='run.configure', total_time=123)
        evs = undo()
        check('W210: run.configure is an undo step of its own: the parameter edit before it stays', pars(evs)['phi'] == 0.4, str(pars(evs)['phi']))
        evs = undo()
        check('W210: ... and the parameter edit is the step after it', pars(evs)['phi'] == 0.3, str(pars(evs)['phi']))
        evs = run(cmd='set.restore_defaults', scope='parameters')
        check('W210: Reset all (default) goes on the stack too', pars(evs)['phi'] != 0.3)
        evs = undo()
        check('W210: ... and Undo brings the edited values back', pars(evs)['phi'] == 0.3)
        # Run from last state pushes the initial conditions it overwrites
        run(cmd='run.initial')
        before = ics(run(cmd='session.get_state'))
        evs = run(cmd='run.last')
        after = ics(evs)
        check('W210: Run from last state copies the last state into the initial conditions', after != before)
        evs = undo()
        check('W210: Undo after Run from last state restores the old initial conditions', ics(evs) == before, str(ics(evs)) + str(before))
        # the stack is bounded
        for i in range(MAX_VALUE_UNDO + 5):
            evs = edit(0.5 + i * 1e-3)
        n = 0
        while state(evs).get('can_undo') and n < MAX_VALUE_UNDO + 10:
            evs = undo()
            n += 1
        check('W210: the undo stack keeps the last %d edits' % MAX_VALUE_UNDO, n == MAX_VALUE_UNDO, str(n))
        # a slider drag is one edit: its steps share one snapshot
        evs = edit(0.11)
        before = pars(evs)['phi']
        for v in (0.12, 0.13, 0.14):
            evs = run(cmd='value.slide', name='phi', value=v)
        check('W210: a slider drag moves the value', pars(evs)['phi'] == 0.14)
        evs = undo()
        check('W210: one Undo takes back the whole drag', pars(evs)['phi'] == before, str(pars(evs)['phi']))
    finally:
        stop_server(p, r, snd)



def check_plot_limits():
    """W376: the protocol and --silent share dynamically sized plot state."""
    import zipfile
    with tempfile.TemporaryDirectory(prefix='w376-model-') as source:
        model = os.path.join(source, 'limits.odex')
        with open(model, 'w', encoding='utf-8') as f:
            f.write(''.join("x%d'=-x%d\n" % (i, i) for i in range(13)))
            f.write(''.join('init x%d=0\n' % i for i in range(13)))
            f.write('options { integration: { total_time: 1, dt: 0.1 } }\n')
        s = Server(args.server, model)
        def command(menus=(), form=None, **cmd):
            keys = iter(menus)
            s.send(**cmd)
            replies = {
                'choice': lambda e: {'key': next(keys, '\x1b')},
                'form': lambda e: {'ok': 1, 'values': form} if form is not None else {'ok': 0},
            }
            if cmd.get('cmd') in ('session.open_model', 'session.open'):
                replies['choice'] = lambda e: {'key': 'd'}  # Discard the scratch session being replaced.
            events, end = s.answer_asks(is_idle, replies)
            check('W376: plot command finishes without error', end is not None and is_idle(end)
                  and not any(e.get('error') for e in events), str(events[-2:])[:200])
            return events
        try:
            s.collect(is_idle)
            command(cmd='events.subscribe', events=['plots', 'series', 'marks'])
            events = command(cmd='plot.set_variables', how='time_series', names=['x%d' % i for i in range(13)])
            graph = next((e for e in reversed(events) if e.get('ev') == 'plots'), {})
            check('W376: xvst retains all thirteen selected states',
                  len(graph.get('windows', [{}])[0].get('curves', [])) == 13, str(graph)[:200])
            command(cmd='run.initial')
            for i in range(32):
                command(cmd='plot.freeze_curve', color='#ffcd00', draw='line', dash='solid', width=1, marker='circle', marker_size=3, key='frozen%d'%i, name='f%d'%i)
            for i in range(26):
                command(cmd='window.create')
            command(cmd='events.subscribe', events=['plots', 'series', 'marks'])
            command(cmd='session.save_as', file='limits', data=True)
            snap = os.path.join(s.run, 'limits.snapx')
            check('W376: large session archive is written', os.path.exists(snap))
            if not os.path.exists(snap):
                return
            with zipfile.ZipFile(snap) as z:
                saved = {k: z.read(k) for k in ('windows.setx', 'marks.setx', 'frozen.npz')}
            for win in ('missing graph', 1.5, -1, 2147483647):
                s.send(cmd='window.select', window=win)
                invalid, _ = s.collect(is_idle)
                errors = [e for e in invalid if e.get('error')]
                check('W376: missing/fractional plot ID is a placed error',
                      len(errors) == 1 and errors[0].get('file') == 'command' and errors[0].get('line') == 1, str(errors))
            # Opening the session uses the same transactional member readers as snapshots.
            command(cmd='session.open_model', file='limits.snapx')
            all_data = command(cmd='events.subscribe', events=['plots', 'series', 'marks'])
            plots = next((e for e in reversed(all_data) if e.get('ev') == 'plots'), {})
            frozen = next((e.get('frozen', []) for e in all_data if e.get('ev') == 'marks' and e.get('win') == 'graph_1'), [])
            check('W376: archive reload restores 27 windows with 13 curves each',
                  len(plots.get('windows', [])) == 27 and all(len(w.get('curves', [])) == 13 for w in plots['windows']))
            check('W376: archive reload restores every frozen curve and its data',
                  len(frozen) == 32 and frozen[-1].get('name') == 'f31' and len(frozen[-1].get('x', [])) == 11,
                  str(frozen[-1:])[:150])
            command(cmd='session.save_as', file='again', data=True)
            with zipfile.ZipFile(os.path.join(s.run, 'again.snapx')) as z:
                check('W376: plot and frozen archive members round-trip exactly',
                      all(z.read(k) == v for k, v in saved.items()))
            # --silent restores sessions through a recording snapshot, not a .snapx argument.
            command(cmd='recording.start')
            command(cmd='run.initial')
            command(cmd='session.save_as', file='silent', data=True)
            command(cmd='recording.stop', file='limits.recx')
            os.remove(os.path.join(s.run, 'silent.snapx'))  # Require the replay's output, not the original save.
            replay = replay_recording(args.server, model, os.path.join(s.run, 'limits.recx'), s.run)
            check('W376: --silent loads and runs the recording snapshot with large plot state',
                  replay.returncode == 0 and os.path.exists(os.path.join(s.run, 'silent.snapx')),
                  str((replay.returncode, replay.stderr[-200:])))
            with zipfile.ZipFile(os.path.join(s.run, 'silent.snapx')) as z:
                check('W376: recording replay preserves every window, curve and frozen curve',
                      all(z.read(k) == v for k, v in saved.items()))
        finally:
            s.close()

def check_session_file():
    """W57, W103: Save session writes one name.snapx (a zip of the files
    listed in core/snapx.h, the model's own included); a new server that
    opens it shows the same state, plots, marks, AUTO view and diagram, and
    AUTO continues from the restored diagram; with the .ode on the disk
    edited, the saved model is the one loaded; a session file without its
    model, or a zip opened as a model, is refused and nothing changes; an
    added browser column comes back computed (W145)"""
    import zipfile
    allev = []
    last = lambda name, **m: next((e for e in reversed(allev) if e.get('ev') == name
                                    and all(e.get(k) == v for k, v in m.items())), None)
    no_t = lambda e: {k: v for k, v in e.items() if k != '_t'} if e else e

    def answered(snd, col, answers, **cmd):
        """cmd, its asks answered in order (a str is a key, a dict the answer; then cancel), up to idle"""
        snd(**cmd)
        n = 0
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=60 * SLOW)
            allev.extend(evs)
            if e is None or e['ev'] in ('idle','done'):
                return evs
            a = answers[n] if n < len(answers) else {'ok': 0}
            n += 1
            snd(cmd='answer', id=e['id'], **({'key': a} if isinstance(a, str) else a))

    SUBSCRIBE = dict(cmd='events.subscribe', events=['plots', 'marks', 'autoinfo', 'nullclines'])
    p, r, snd, col, _ = launch_server()
    keep = tempfile.mkdtemp(prefix='xppsnapx')
    try:
        col(is_idle)
        answered(snd, col, (), **SUBSCRIBE)
        allev.extend(lecar_to_auto(snd, col))
        answered(snd, col, (), cmd='value.set_parameters', values={'phi': 0.4})
        answered(snd, col, (), cmd='ic.set', values={'V': -0.3})
        answered(snd, col, (), cmd='run.initial')
        answered(snd, col, (), cmd='window.set_display', window='graph_1', x_range=[0, 50], y_range=[-0.5, 0.3], show_earlier_runs=False)
        answered(snd, col, (), cmd='auto.set_display', x_range=[0.05, 0.3])
        # W50: a second view of the diagram, the norm, with a zoom of its own; the first active again
        answered(snd, col, (), cmd='auto.new_view')
        answered(snd, col, (), cmd='auto.configure_plot', **{'view': 'diagram_2', 'plot_type': 'norm', 'fit': True})
        answered(snd, col, (), cmd='auto.set_display', view='diagram_2', y_range=[0, 0.5])
        answered(snd, col, (), cmd='window.select', window='diagram_1')
        answered(snd, col, (), cmd='annotation.add_text', window='graph_1', x=10, y=.1, text='here', text_size=2, style='italic', color='#ffcd00')
        answered(snd, col, (), cmd='plot.freeze_curve', window='graph_1', color='#ffcd00', draw='line', dash='dashed', width=2, marker='square', marker_size=4, key='first', name='frz1')
        answered(snd, col, (), cmd='window.create')
        # W145: an added browser column is saved with the session and comes back computed
        answered(snd, col, (), cmd='browser.add_column', name='VW', formula='v*w')
        browser1 = next((no_t(e) for e in reversed(answered(snd, col, (), cmd='browser.page', **{'from_row': 0, 'count': 2}))
                         if e.get('ev') == 'browser'), None)
        check('W145: the added column VW is in the browser before the save',
              browser1 is not None and browser1.get('cols', [])[-1:] == ['VW'], str(browser1)[:200])
        # W135: definitions and computed/frozen nullclines are Session members.
        answered(snd, col, (), cmd='value.configure_slider', slot=0, name='iapp', low=-1, high=2, step=0.01)
        answered(snd, col, (), cmd='value.configure_slider', slot=2, name='V', low=-2, high=2, step=0.02)
        answered(snd, col, (), cmd='value.configure_slider', slot=3, name='phi', low=0, high=1, step=0.001)
        answered(snd, col, (), cmd='nullcline.compute')
        answered(snd, col, (), cmd='nullcline.freeze')
        answered(snd, col, (), cmd='session.save_as', file='s1')
        st1 = no_t(last('state'))
        check('session save: state.session names the .snapx', st1 and st1.get('session') == {'file': 's1.snapx'},
              str(st1 and st1.get('session')))
        saved = {k: no_t(last(k)) for k in ('plots', 'autoview', 'nullclines')}
        check('W135: the saved nullclines contain a current curve and one frozen pair',
              saved['nullclines'] and saved['nullclines'].get('x') and len(saved['nullclines'].get('frozen', [])) == 1,
              str(saved['nullclines'] and [len(saved['nullclines'].get('x', [])), len(saved['nullclines'].get('frozen', []))]))
        marks1 = [no_t(last('marks', win=w)) for w in ('graph_1', 'graph_2')]
        diagram1 = rebuild_diagram(allev, [])
        view1 = rebuild_diagram(allev, [], 1)
        snap = os.path.join(r, 's1.snapx')
        names = zipfile.ZipFile(snap).namelist() if os.path.exists(snap) else []
        check('session save: s1.snapx is a zip of the files listed, the model in it',
              sorted(names) == sorted(['session.setx', 'model/lecar.odex', 'model/ml.anix', 'model.setx', 'auto/settings.txt', 'auto/diagram.csv',
                        'auto/solutions.s', 'auto/views.txt', 'windows.setx', 'marks.setx', 'frozen.npz', 'data.npz', 'random.txt', 'sliders.setx', 'nullclines.setx']),
              str(names))
        if names:
            z = zipfile.ZipFile(snap)
            check('session save: model.setx holds the settings of lecar, session.setx names it',
                  b'\nnumerics: {\n' in z.read('model.setx') and b'\n  total_time: ' in z.read('model.setx')
                  and b'\n  model: "lecar.odex",\n' in z.read('session.setx'), str(z.read('session.setx')[:200]))
            with open(os.path.join(r, 'lecar.odex'), 'rb') as f:
                check('session save: model/lecar.odex is the model, byte for byte', z.read('model/lecar.odex') == f.read())
            vt = z.read('auto/views.txt').decode().splitlines()
            check('session save: auto/views.txt holds both views, the first active',
                  len(vt) == 3 and vt[0].startswith('view "diagram_1" shown maximum_and_minimum V iapp ') and vt[1].startswith('view "diagram_2" shown norm V iapp ')
                  and vt[1].endswith(' - 0:0.5') and vt[2] == 'active "diagram_1"', str(vt))
            shutil.copy(snap, keep)
        answered(snd, col, (), cmd='session.save_as', file='s2.snapx', data=False)
        s2 = os.path.join(r, 's2.snapx')
        check('session save with data false leaves data.npz out',
              os.path.exists(s2) and 'data.npz' not in zipfile.ZipFile(s2).namelist(), '')
    finally:
        stop_server(p, r, snd)

    def open_snapx(ode_text=None, name='s1.snapx'):
        """a new server in a new folder with s1.snapx beside its lecar.odex (ode_text: an edited one)"""
        p, r, snd, col, _ = launch_server()
        col(is_idle)
        shutil.copy(os.path.join(keep, 's1.snapx'), os.path.join(r, name))
        if ode_text is not None:
            with open(os.path.join(r, 'lecar.odex'), 'w') as f:
                f.write(ode_text)
            answered(snd, col, ('d',), cmd='session.reload_model')
        answered(snd, col, (), **SUBSCRIBE)
        del allev[:]
        answered(snd, col, ('d',), cmd='session.open_model', file=name)
        answered(snd, col, (), **SUBSCRIBE)
        return p, r, snd, col

    def restored_as_saved(tag):
        st2 = no_t(last('state'))
        drop = lambda s: {k: v for k, v in (s or {}).items() if k not in ('session', 'can_undo', 'can_redo')}  # the undo stacks are not saved
        check('%s: the state is the saved one' % tag, st2 and drop(st1) == drop(st2),
              str([k for k in drop(st1) if drop(st1).get(k) != drop(st2).get(k)]))
        check('%s: state.session names the file opened' % tag,
              st2 and st2.get('session', {}).get('file', '').endswith('s1.snapx'), str(st2 and st2.get('session')))
        for k in saved:
            # Nullclines are per plot window: subscription order can differ after a load.
            restored = no_t(last(k, **({'win': saved[k]['win']} if k == 'nullclines' else {})))
            check('%s: the %s event is the saved one' % (tag, k), saved[k] == restored,
                  str([field for field in (saved[k] or {}) if saved[k].get(field) != (restored or {}).get(field)]))
        diagram2 = rebuild_diagram(allev, [])
        # the diagram keeps every point at full precision (W92): the same points exactly
        check("%s: the diagram is the saved one, exactly" % tag,
              diagram1 and diagram1 == diagram2,
              '%d vs %d points, first difference %s' % (len(diagram1), len(diagram2),
                                                          next(((a, b) for a, b in zip(diagram1, diagram2) if a != b), None)))
        view2 = rebuild_diagram(allev, [], 1)
        check('%s: the second view is the saved one, its points exactly' % tag,
              view1 and view1 == view2 and last('autoview') and len(last('autoview')['views']) == 2,
              '%d vs %d points' % (len(view1), len(view2)))
        return diagram2

    # the same model open (the same bytes): the session it replaces may be
    # saved first, as File > Open asks (W103 review), then the session loads
    p, r, snd, col = open_snapx()
    try:
        asks = [e for e in allev if e.get('ev') == 'ask']
        check('open session of the model open: asks once whether to save this session first',
              len(asks) == 1 and asks[0].get('kind') == 'choice' and asks[0].get('keys') == 'sd', str(asks[:2]))
        diagram2 = restored_as_saved('open session')
        browser2 = next((no_t(e) for e in reversed(answered(snd, col, (), cmd='browser.page', **{'from_row': 0, 'count': 2}))
                         if e.get('ev') == 'browser'), None)
        check('W145: open session: the added column VW is back, computed as before the save',
              browser1 is not None and browser2 == browser1, '%s\n    %s' % (str(browser1)[:200], str(browser2)[:200]))
        m2 = [no_t(last('marks', win=w)) for w in ('graph_1', 'graph_2')]
        strip = lambda m: m and dict(m, equilibria=[])
        check('open session: the labels and frozen curves are the saved ones (not Sing pts\' symbols)',
              [strip(m) for m in marks1] == [strip(m) for m in m2], str(m2)[:300])
        answered(snd, col, (), cmd='auto.grab', type='hopf', index=1)
        evs = answered(snd, col, (), cmd='auto.run', kind='periodic', type='hopf', index=1)
        more = rebuild_diagram(allev, [])
        check('open session: a grab and a periodic run continue the restored diagram',
              len(more) > len(diagram2) and more[:len(diagram2)] == diagram2 and p.poll() is None,
              '%d points after %d' % (len(more), len(diagram2)))
    finally:
        stop_server(p, r, snd)

    # the .ode on the disk edited (phi renamed) and loaded: the session's own
    # saved model is loaded, not the disk's, and everything is as saved
    with open(args.ode) as f:
        text = f.read()
    p, r, snd, col = open_snapx(re.sub(r'\bphi\b', 'phi2', text))
    try:
        hello = last('hello')
        check('open session with the .ode edited: the saved model loads, the title names it',
              hello is not None and 'lecar.odex (saved in s1.snapx)' in hello.get('title', ''), str(hello and hello.get('title')))
        restored_as_saved('open session with the .ode edited')
        with open(os.path.join(r, 'lecar.odex')) as f:
            check('open session with the .ode edited: the .ode on the disk is left as it was, nothing written beside it',
                  'phi2' in f.read() and sorted(os.listdir(r)) == sorted(staged_model_names(args.ode) + ['s1.snapx']), str(os.listdir(r)))
        # a zip opened as a model: refused, its bytes never shown, nothing changes
        shutil.copy(os.path.join(r, 's1.snapx'), os.path.join(r, 'zip.ode'))
        del allev[:]
        answered(snd, col, ('d',), cmd='session.open_model', file='zip.ode')
        msgs = ' '.join(str(e.get('bottom', '')) + str(e.get('error', '')) for e in allev if e.get('ev') in ('message', 'done'))
        check('a zip opened as a model is refused, its bytes never shown',
              'model text' in msgs and 'PK' not in msgs and not last('hello'), msgs[:300])
        # a session file without its model: refused, nothing changes
        z = zipfile.ZipFile(os.path.join(r, 's1.snapx'))
        with zipfile.ZipFile(os.path.join(r, 'nomodel.snapx'), 'w') as out:
            for n in z.namelist():
                if not n.startswith('model/'):
                    out.writestr(n, z.read(n))
        del allev[:]
        answered(snd, col, ('d',), cmd='session.open_model', file='nomodel.snapx')
        msgs = ' '.join(str(e.get('bottom', '')) + str(e.get('error', '')) for e in allev if e.get('ev') in ('message', 'done'))
        check('a session file without its model is refused, an error says the model is missing',
              'model is missing' in msgs and not last('hello') and not any(e.get('ev') == 'ask' for e in allev), msgs[:300])
        # W50: a session file without the views of its diagram: an error names the member (no older files)
        with zipfile.ZipFile(os.path.join(r, 'noviews.snapx'), 'w') as out:
            for n in z.namelist():
                if n != 'auto/views.txt':
                    out.writestr(n, z.read(n))
        del allev[:]
        answered(snd, col, ('d',), cmd='session.open_model', file='noviews.snapx')
        msgs = ' '.join(str(e.get('error', '')) for e in allev if e.get('ev') in ('message', 'done'))
        check('a session file without the views of its diagram: an error names auto/views.txt, the session before stays',
              'its auto/views.txt is missing' in msgs and not last('hello'), msgs[:300])
        # W116: a member missing, cut short or with a line that does not
        # read fails the open before its model is kept: the error names the
        # member and the line, and the session before stays (no hello)
        members = {n: z.read(n) for n in z.namelist()}

        def text_lines(member, k, line):
            """member's text with its line k (from 1) set to line"""
            rows = members[member].decode().split('\n')
            rows[k - 1] = line
            return '\n'.join(rows).encode()

        win_text = members['windows.setx'].decode()
        win_rows = win_text.split('\n')
        set_rows = members['model.setx'].decode().split('\n')
        random_rows = members['random.txt'].decode().split('\n')
        # the line (from 1) of a member's rows that starts with key, the last one with last
        line_starting = lambda rows, key, last=False: (next(k + 1 for k in reversed(range(len(rows))) if rows[k].lstrip().startswith(key))
                                                       if last else next(k + 1 for k in range(len(rows)) if rows[k].lstrip().startswith(key)))
        # the line of model.setx's last value (the BVP range's high end)
        set_high = line_starting(set_rows, 'high:', True)
        set_store_every, set_dt = line_starting(set_rows, 'store_every:'), line_starting(set_rows, 'dt:')
        set_total = line_starting(set_rows, 'total_time:')
        win_curve_x = line_starting(win_rows, 'x:')
        # windows.setx ends with its added columns
        win_added = line_starting(win_rows, 'added_columns:')
        generator = random_rows[-1].split(' ')
        manifest_lines = len(members['session.setx'].decode().rstrip('\n').split('\n'))

        def with_line(member, k, line):
            """member's text with its line k (from 1) set to line"""
            return text_lines(member, k, line)

        def label_marks(label_line=None, **over):
            """marks.setx with one label (its fields over the defaults); the label's
            lines are 5 (window) to 12 (text)"""
            f = dict(id='0', window='"graph_1"', x='0.5', y='0.5', size='2', font='0', style='normal', color='"foreground"', text='"hi"')
            f.update(over)
            rows = ['next_annotation_id: 1,', 'labels: [', '  {'] + ['    %s: %s,' % (k, f[k]) for k in ('id', 'window', 'x', 'y', 'size', 'font', 'style', 'color', 'text')]
            rows += ['  },', '],', 'arrows_and_markers: [],', 'freeze_each_run: 0,', 'frozen_curves: [],', '']
            return '\n'.join(rows).encode()

        def mark_marks(**over):
            """marks.setx with one arrow or marker; its lines are 6 (window) to 13"""
            f = dict(id='0', window='"graph_1"', type='0', color='"foreground"', size='1', x_start='0', y_start='0', x_end='1', y_end='1')
            f.update(over)
            rows = ['next_annotation_id: 1,', 'labels: [],', 'arrows_and_markers: [', '  {'] + ['    %s: %s,' % (k, f[k]) for k in
                                                                       ('id', 'window', 'type', 'color', 'size', 'x_start', 'y_start', 'x_end', 'y_end')]
            rows += ['  },', '],', 'freeze_each_run: 0,', 'frozen_curves: [],', '']
            return '\n'.join(rows).encode()

        def added_columns(items):
            """windows.setx with its added columns replaced by items (name, formula)"""
            rows = ['added_columns: [']
            for name, formula in items:
                rows += ['  {', '    name: "%s",' % name, '    formula: "%s",' % formula, '  },']
            rows += ['],', '']
            return ('\n'.join(win_rows[:win_added - 1]) + '\n' + '\n'.join(rows)).encode()

        def nullclines(x_segments):
            return ('current: { x_axis: 1, y_axis: 2, x_segments: [%s], y_segments: [] },\nfrozen: [],\n' % x_segments).encode()

        damages = [
            ('badslider', lambda m: m.__setitem__('sliders.setx', text_lines('sliders.setx', 3, '    name: nonsense,')),
             ('badslider.snapx/sliders.setx:3:', 'nonsense')),
            ('badsliderrange', lambda m: m.__setitem__('sliders.setx', text_lines('sliders.setx', 5, '    high: -2,')),
             ('badsliderrange.snapx/sliders.setx:5:', 'slider range -1 -2')),
            ('nannullcline', lambda m: m.__setitem__('nullclines.setx', nullclines('nan, 0, 0, 0')),
             ('nannullcline.snapx/nullclines.setx:1:', 'nan')),
            ('badnullcline', lambda m: m.__setitem__('nullclines.setx', nullclines('0, 0, 0, 0, 0')),
             ('badnullcline.snapx/nullclines.setx:1:', 'four for each segment')),

            ('nowindows', lambda m: m.pop('windows.setx'), 'its windows.setx is missing'),
            ('cutwindows', lambda m: m.__setitem__('windows.setx', '\n'.join(win_rows[:10]).encode() + b'\n'),
             ('cutwindows.snapx/windows.setx:', 'missing `}`')),
            ('badset', lambda m: m.__setitem__('model.setx', text_lines('model.setx', set_total, '  total_time: x,')),
             ('badset.snapx/model.setx:%d:' % set_total, 'found `x`')),
            ('badname', lambda m: m.__setitem__('windows.setx', text_lines('windows.setx', win_curve_x, '      x: nosuch,')),
             ('badname.snapx/windows.setx:%d:' % win_curve_x, 'the model has no variable "nosuch"')),
            # W125: a bad value on a member's last line: refused at it, nothing applied
            ('lastmarks', lambda m: m.__setitem__('marks.setx', m['marks.setx'].rstrip(b'\n') + b'\nmore: 1,\n'),
             ('lastmarks.snapx/marks.setx:%d:' % (len(members['marks.setx'].decode().rstrip('\n').split('\n')) + 1), '`more` is not a setting')),
            ('lastset', lambda m: m.__setitem__('model.setx', text_lines('model.setx', set_high, '  high: 1e999,')),
             ('lastset.snapx/model.setx:%d:' % set_high, '`1e999` is not a number')),
            ('latermanifest', lambda m: m.__setitem__('session.setx', m['session.setx'] + b'later: 1,\n'),
             ('latermanifest.snapx/session.setx:%d:' % (manifest_lines + 1), '`later` is not a setting')),
            # W145: every value checked by the rule that checks it anywhere else, before anything is applied
            ('zerostore_every', lambda m: m.__setitem__('model.setx', text_lines('model.setx', set_store_every, '  store_every: 0,')),
             ('zerostore_every.snapx/model.setx:%d:' % set_store_every, 'Store every N steps must be a whole number of at least 1')),
            ('zerodt', lambda m: m.__setitem__('model.setx', text_lines('model.setx', set_dt, '  dt: 0,')),
             ('zerodt.snapx/model.setx:%d:' % set_dt, 'Dt must be a number other than 0')),
            # W285: a session of the older layout (XPPAUT's set lines) is refused as any text that is no settings is
            ('oldset', lambda m: m.__setitem__('model.setx', b'## Set file for lecar.odex\n4   Number of equations and auxiliaries\n'),
             ('oldset.snapx/model.setx:2:1:', 'expected a key')),
            ('manycolumns', lambda m: m.__setitem__('windows.setx', added_columns([('X%d' % k, 'v*w') for k in range(5001)])),
             ('manycolumns.snapx/windows.setx:%d:' % win_added, 'entries: at most')),
            ('badformula', lambda m: m.__setitem__('windows.setx', added_columns([('VW', 'unknown_symbol+')])),
             ('badformula.snapx/windows.setx:%d:' % (win_added + 3), 'the formula of the added column VW does not compile')),
            ('spareflag', lambda m: m.__setitem__('random.txt', text_lines('random.txt', len(random_rows),
                                                                           ' '.join(generator[:-2] + ['9', generator[-1]]))),
             ('spareflag.snapx/random.txt:%d:' % len(random_rows), "not a random generator's state")),
            ('aftergenerator', lambda m: m.__setitem__('random.txt', m['random.txt'] + b'\nnot_generator_state\n'),
             ('aftergenerator.snapx/random.txt:%d:' % (len(random_rows) + 1), '"not_generator_state" after the end')),
            # W227: a label's every value is checked at its line (window, x, y, size, font, style, colour, text)
            ('labelsize', lambda m: m.__setitem__('marks.setx', label_marks(size='9')),
             ('labelsize.snapx/marks.setx:8:', '9 is not a text size (0 to 4)')),
            ('labelfont', lambda m: m.__setitem__('marks.setx', label_marks(font='5')),
             ('labelfont.snapx/marks.setx:9:', '5 is not a text font (0 to 1)')),
            ('labelstyle', lambda m: m.__setitem__('marks.setx', label_marks(style='oblique')),
             ('labelstyle.snapx/marks.setx:10:', '`style`: `oblique` is not one of normal, bold, italic, bold_italic')),
            ('labelcolor', lambda m: m.__setitem__('marks.setx', label_marks(color='"99"')),
             ('labelcolor.snapx/marks.setx:11:', '`99` is not a colour')),
            ('labelempty', lambda m: m.__setitem__('marks.setx', label_marks(text='""')),
             ('labelempty.snapx/marks.setx:12:', 'a text is not empty')),
            ('labellong', lambda m: m.__setitem__('marks.setx', label_marks(text='"' + 'x' * 1001 + '"')),
             ('labellong.snapx/marks.setx:12:', 'at most 1000')),
            # W513: ids are checked at their line: below next_annotation_id, each above the one before, none shared by two kinds
            ('labelid', lambda m: m.__setitem__('marks.setx', label_marks(id='1')),
             ('labelid.snapx/marks.setx:4:', '1 is not an id below next_annotation_id 1')),
            ('labelnegid', lambda m: m.__setitem__('marks.setx', label_marks(id='-1')),
             ('labelnegid.snapx/marks.setx:4:', '-1 is not an id below next_annotation_id 1')),
            ('markid', lambda m: m.__setitem__('marks.setx', mark_marks(id='5')),
             ('markid.snapx/marks.setx:5:', '5 is not an id below next_annotation_id 1')),
            ('sharedid', lambda m: m.__setitem__('marks.setx', mark_marks().replace(b'labels: [],', label_marks().split(b'arrows_and_markers')[0].split(b'\n', 1)[1].rstrip(b'\n') + b'')),
             ('sharedid.snapx/marks.setx:', 'belongs to a text and an arrow, pointer or marker')),
            ('labelnan', lambda m: m.__setitem__('marks.setx', label_marks(x='nan')),
             ('labelnan.snapx/marks.setx:6:', 'is not a finite number')),
            ('marktype', lambda m: m.__setitem__('marks.setx', mark_marks(type='999')),
             ('marktype.snapx/marks.setx:7:', "999 is not an object's type (0 to 7)")),
            ('markcolor', lambda m: m.__setitem__('marks.setx', mark_marks(color='"999"')),
             ('markcolor.snapx/marks.setx:8:', '`999` is not a colour')),
            ('twonames', lambda m: m.__setitem__('session.setx', m['session.setx'].replace(b'session: {', b'session: { model: "lecar.odex",', 1)),
             ('twonames.snapx/session.setx:3:', 'is given twice')),
        ]
        if 'data.npz' in members:
            damages.append(('nodata', lambda m: m.pop('data.npz'), 'its data.npz is missing'))
        for name, change, expect in damages:
            damaged = dict(members)
            change(damaged)
            with zipfile.ZipFile(os.path.join(r, name + '.snapx'), 'w') as out:
                for n, b in damaged.items():
                    out.writestr(n, b)
            del allev[:]
            answered(snd, col, ('d',), cmd='session.open_model', file=name + '.snapx')
            msgs = ' '.join(placed(e) for e in allev if e.get('ev') in ('message', 'done'))
            check('W116: %s.snapx is refused, the error says where (%s), the session before stays' % (name, expect),
                  all(x in msgs for x in (expect if isinstance(expect, tuple) else (expect,))) and not last('hello'), msgs[:300])
        snd(cmd='session.get_state')
        st2 = no_t(next((e for e in reversed(col(is_idle)[0]) if is_state(e)), None))
        check('W116: after the refused opens the session before stays: its state, s1.snapx its session',
              st2 and st2.get('pars') == st1.get('pars') and st2.get('sliders') == st1.get('sliders') and st2.get('rows') == st1.get('rows')
              and st2.get('session', {}).get('file', '').endswith('s1.snapx'), str(st2 and st2.get('session')))
    finally:
        stop_server(p, r, snd)
        shutil.rmtree(keep, ignore_errors=True)


# W50: any number of views of the one AUTO diagram (docs/protocol.md "Views
# of the diagram"): each with its own axes and zoom, every one holding the
# same points in the same order; a new view, a closed one and the active one
# over `auto` `view` and the Axes menu; a run's points reach every view, a
# grab goes by the active view's data.
def check_auto_views():
    p, r, snd, col, _ = launch_server(NO_THROTTLE)
    allev = []

    def after(**cmd):
        snd(**cmd)
        evs = col(is_idle, timeout=60 * SLOW)[0]
        allev.extend(evs)
        return evs

    av = lambda evs: [e for e in evs if e.get('ev') == 'autoview']
    dg = lambda evs, view=None: [e for e in evs if e.get('ev') == 'diagram' and (view is None or e.get('view') == diagram_view_name(allev, view))]
    views_n = lambda evs: [e['n'] for e in dg(evs) if e['op'] == 'views']
    last_axes = lambda view: next((e for e in reversed(dg(allev, view)) if e['op'] in ('axes', 'reset')), None)
    same_points = lambda a, b: len(a) == len(b) and [x[:3] for x in a] == [x[:3] for x in b]
    try:
        col(is_idle)
        after(cmd='events.subscribe', events=['autoinfo'])
        allev.extend(lecar_to_auto(snd, col))
        v0 = rebuild_diagram(allev, [])
        evs = after(cmd='auto.new_view')
        check('views: a new view says there are two, then sends the diagram at the same axes, view 0 unchanged',
              views_n(evs) == [2] and len(v0) > 1 and rebuild_diagram(allev, [], 1) == v0
              and not [e for e in dg(evs, 0) if e['op'] != 'axes'], str([(e.get('op'), e.get('view')) for e in dg(evs)]))
        check('views: the new view is the active one, with a zoom of its own',
              av(evs) and av(evs)[-1]['active'] == 'diagram_2' and len(av(evs)[-1]['views']) == 2, str(av(evs)))

        evs = after(cmd='auto.configure_plot', **{'view': 'diagram_2', 'plot_type': 'norm', 'fit': True})
        a0, a1 = last_axes(0), last_axes(1)
        check("views: a view's axes are its own: view 1 plots the norm, view 0 still hI-lo",
              a1 and a1['plot'] == 'norm' and a1['ylabel'] == 'Norm' and a0 and a0['plot'] == 'maximum_and_minimum'
              and not [e for e in dg(evs, 0) if e['op'] != 'axes'], '%s | %s' % (a0, a1))
        v1 = rebuild_diagram(allev, [], 1)
        check('views: every view holds the same points in the same order (branch, point, kind), at its own axes',
              same_points(v0, v1) and rebuild_diagram(allev, [], 0) == v0 and v1 != v0, '%d vs %d' % (len(v0), len(v1)))

        evs = after(cmd='auto.new_view')
        a2 = last_axes(2)
        check("views: the Axes menu's new view makes a third, the active one, with the active one's axes (the norm)",
              views_n(evs) == [3] and a2 and a2['plot'] == 'norm' and av(evs) and av(evs)[-1]['active'] == 'diagram_3'
              and rebuild_diagram(allev, [], 2) == v1, '%s %s' % (views_n(evs), a2))

        evs = after(cmd='auto.set_display', view='diagram_1', x_range=[0.1, 0.2])
        check('views: auto display with a view zooms that view alone',
              av(evs) and [v['zoom']['x'] for v in av(evs)[-1]['views']] == [[0.1, 0.2], None, None], str(av(evs)))

        after(cmd='window.select', window='diagram_1')
        after(cmd='auto.grab', type='hopf', index=1)
        after(cmd='auto.run', kind='periodic', type='hopf', index=1)
        lists = [rebuild_diagram(allev, [], v) for v in range(3)]
        check("views: a run's points reach every view, the same points in each",
              len(lists[0]) > len(v0) and same_points(lists[0], lists[1]) and same_points(lists[0], lists[2])
              and lists[0][:len(v0)] == v0, str([len(x) for x in lists]))

        after(cmd='window.select', window='diagram_2')
        snd(cmd='auto.grab')
        evs, ask = col(lambda e: e.get('ev') == 'ask', timeout=60 * SLOW)
        at = next((i for i, x in enumerate(lists[1]) if x[3] is not None and x[4] is not None and i > 3), 4)
        snd(cmd='answer', id=ask['id'], point=at)
        evs, ask = col(lambda e: e.get('ev') == 'ask', timeout=60 * SLOW)
        info = next((e['info'] for e in reversed(evs) if e.get('ev') == 'autoinfo' and e.get('info')), None)
        check('views: a grab in another view goes to the point of its data (the same index in every view)',
              info is not None and info['point'] == at and (info['br'], info['pt']) == lists[1][at][:2]
              and (info['br'], info['pt']) == lists[0][at][:2], '%s %s' % (info, lists[1][at] if at < len(lists[1]) else None))
        snd(cmd='answer', id=ask['id'], key='Escape')
        col(is_idle, timeout=60 * SLOW)

        evs = after(cmd='window.destroy', window='diagram_1')
        check('views: closing view 0 leaves two, the others move down, the active one stays active (now view 0)',
              views_n(evs) == [2] and av(evs) and av(evs)[-1]['active'] == 'diagram_2' and len(av(evs)[-1]['views']) == 2
              and rebuild_diagram(allev, [], 0) == lists[1] and rebuild_diagram(allev, [], 1) == lists[2],
              '%s %s' % (views_n(evs), av(evs)))
        evs = after(cmd='window.destroy', window='diagram_8')
        check('views: closing a view there is not is refused', any(e.get('ev') == 'done' and e.get('status') == 'error' for e in evs), '')
        after(cmd='window.destroy', window='diagram_3')
        evs = after(cmd='window.destroy', window='diagram_2')
        check('views: closing the last diagram closes AUTO', any(e.get('ev') == 'window' and e.get('op') == 'destroy' for e in evs), str(evs)[:200])
        check('the process is alive after it all', p.poll() is None, '')
    finally:
        stop_server(p, r, snd)


def check_auto_subscriptions():
    """W335: AUTO's subscriptions are the protocol client's: subscribe,
    unsubscribe and subscribe again send, stop and resend autosettings,
    autoinfo and autoview (a reload and a grab after it: autocheck's
    subscriptions section)."""
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        # 1. Subscribe to autoinfo, autosettings
        snd(cmd='events.subscribe', events=['autoinfo', 'autosettings'])
        evs, _ = col(is_idle)
        st = [e for e in evs if e.get('ev') == 'autosettings']
        info = [e for e in evs if e.get('ev') == 'autoinfo']
        av = [e for e in evs if e.get('ev') == 'autoview']
        check('auto subscriptions: subscribe sends autosettings, autoinfo and autoview',
              len(st) > 0 and len(info) > 0 and len(av) > 0, str([len(st), len(info), len(av)]))

        # Unsubscribe
        snd(cmd='events.unsubscribe', events=ALL_EVENTS)
        evs, _ = col(is_idle)
        unsub_evs = [e for e in evs if e.get('ev') in ('autoinfo', 'autosettings', 'autoview')]
        check('auto subscriptions: unsubscribe stops auto events', len(unsub_evs) == 0, str(unsub_evs))

        # Subscribe again
        snd(cmd='events.subscribe', events=['autoinfo', 'autosettings'])
        evs, _ = col(is_idle)
        st2 = [e for e in evs if e.get('ev') == 'autosettings']
        info2 = [e for e in evs if e.get('ev') == 'autoinfo']
        av2 = [e for e in evs if e.get('ev') == 'autoview']
        check('auto subscriptions: subscribe again resends events',
              len(st2) > 0 and len(info2) > 0 and len(av2) > 0, str([len(st2), len(info2), len(av2)]))

    finally:
        stop_server(p, r, snd)


check_auto_subscriptions()



def check_model_bcs():
    """W99: the state event's `bcs` are the model's own boundary conditions;
    a model with no b/bndry lines sends none (the core's default 0 ones are not
    the model's), one with them sends its own."""
    for ode, want in (('examples/ode/amari.odex', 0), ('examples/ode/dumbbvp.odex', 2), ('examples/ode/vdp.odex', 2)):
        p, r, snd, col, _ = launch_server(ode=ode)
        snd(cmd='session.get_state')
        evs, e = col(lambda e: e.get('ev') in ('idle','done'))
        st = [x for x in evs if x.get('ev') == 'state']
        n = len(st[-1].get('bcs', [])) if st else -1
        check('bcs: %s sends %d boundary conditions' % (os.path.basename(ode), want), n == want, str(n))
        if want:
            check('bcs: %s formulas are its own' % os.path.basename(ode),
                  all(len(b) == 2 and b[1] not in ('', '0') for b in st[-1]['bcs']), str(st[-1]['bcs']))
        stop_server(p, r, snd)


check_model_bcs()



def check_save_recording():
    """W129: key saves replay explicit decisions and recorded choice answers."""
    p, r, snd, col, _ = launch_server()
    def run(**cmd):
        snd(**cmd)
        return col(is_idle, timeout=30 * SLOW)[0]
    try:
        col(is_idle)
        with open(os.path.join(r, 'browser-no.par'), 'wb') as f: f.write(b'unchanged')
        run(cmd='recording.start')
        for name in ('first.snapx', 'second.snapx'):
            run(cmd='session.save_as', file=name)
        snd(cmd='session.save_as', file_picker=True, confirm=False)
        _, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], file='dialog-save.snapx')
        col(is_idle)
        fresh = run(cmd='value.save_file', kind='parameters', file='browser-new.par')
        snd(cmd='value.save_file', kind='parameters', file='browser-no.par')
        _, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], key='n')
        declined, _ = col(is_idle)
        run(cmd='recording.stop', file='key-save.recx')
        path = os.path.join(r, 'key-save.recx')
        _, _, steps, written, got = read_recx(open(path, encoding='utf-8').read())
        saves = [step for step,_ in steps if step.get('cmd',{}).get('cmd') == 'session.save_as']
        check('W517: saves record registry commands and the picker answer', len(saves) == 3 and written == got
              and saves[-1].get('answers', [])[:1] == [{'file':'dialog-save.snapx'}], str(saves))
        check('W534: a core refused save is not recorded',
              any(e.get('saved') for e in fresh) and any(e.get('ev') == 'saved' and not e.get('saved') for e in declined)
              and all(step.get('cmd', {}).get('file') != 'browser-no.par' for step,_ in steps), str(steps))
        snd(cmd='recording.open', play=False, file=path)
        evs, end = col(lambda e: is_ask(e) or is_idle(e))
        if end and is_ask(end):
            snd(cmd='answer', id=end['id'], key='d')
            col(is_idle)
        run(cmd='recording.set_speed', speed=8)
        prepared = run(cmd='session.save_as', file='dialog-save.snapx', confirm=True)
        check('W129: the replay folder already contains the native dialog destination',
              any(e.get('ev') == 'saved' and e.get('saved') and e.get('file') == 'dialog-save.snapx' for e in prepared), str(prepared)[-200:])
        snd(cmd='value.save_file', kind='parameters', file='browser-new.par')
        evs, end = col(lambda e: is_ask(e) or is_idle(e))
        if end and is_ask(end):
            snd(cmd='answer', id=end['id'], key='y')
            col(is_idle)
        snd(cmd='recording.play')
        evs, end = col(lambda e: e.get('ev') == 'state'
                        and (e.get('player') or {}).get('step') == len(steps)
                        and e['player']['running'] == -1, timeout=120 * SLOW)
        check('W129: Play reuses explicit/dialog decisions when the destination now exists',
              end is not None and [e['saved'] for e in evs if e.get('ev') == 'saved'] == [True, True, True, True]
              and not any(e.get('ev') in ('message', 'done') and e.get('error') for e in evs), str(evs)[-500:])
        col(is_idle)
        listing = run(cmd='file', op='list')
        names = {f['name'] for e in listing if e.get('ev') == 'file' for f in e.get('files', [])}
        check('W534: replay omits a refused save even when its target no longer exists',
              not any(e.get('ev') == 'saved' and e.get('file') == 'browser-no.par' for e in evs)
              and 'browser-no.par' not in names and 'browser-new.par' in names
              and not any(e.get('ev') == 'ask' and e.get('kind') == 'choice'
                          and e.get('question', '').endswith(' exists. Replace it?') for e in evs), str(names))
    finally:
        stop_server(p, r, snd)


check_save_recording()


def check_quit():
    """W59d: every way of leaving asks one question: File/Quit (F Q) and the
    quit that asks ({"cmd": "session.quit"}: the desktop window's File >
    Quit and close box): Save session (s), Don't save (d), Cancel; s saves
    the session (and a recording in progress) then exits, d exits, a cancel
    keeps the session; a quit during a computation stops it, then asks. The
    plain quit (scripts, --server's clients) exits at once, asking nothing.
    W110: hello carries the question, which the page asks itself during a
    run; its Save session is {"cmd": "session.quit", "save": true} (the run stops, the
    session is saved, the exit), its Don't save the plain quit."""
    is_ask = lambda e: e.get('ev') == 'ask'
    question = 'Quit xppautX? Save this session first?'

    def asked(snd, col, **cmd):
        snd(**cmd)
        return col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)

    def ended(p, col):
        """the events up to the exit, and whether it said bye and exited 0"""
        evs, bye = col(lambda e: e.get('ev') == 'bye', timeout=30 * SLOW)
        try:
            p.wait(timeout=10 * SLOW)
        except subprocess.TimeoutExpired:
            p.kill()
            return evs, False
        return evs, bye is not None and p.returncode == 0

    def done(p, r, snd):
        if p.poll() is None:
            stop_server(p, r, snd)
        shutil.rmtree(r, ignore_errors=True)

    # F Q asks; Esc (a cancel) keeps the session
    p, r, snd, col, _ = launch_server()
    try:
        evs, _ = col(is_idle)
        hello = next((e for e in evs if e.get('ev') == 'hello'), {})
        _, ask = asked(snd, col, cmd='session.quit')
        check('File/Quit asks one question: Save session (s), Don\'t save (d), Cancel',
              ask and is_ask(ask) and ask.get('kind') == 'choice' and ask.get('question') == question
              and ask.get('choices') == ['Save session', "Don't save"] and ask.get('keys') == 'sd', str(ask))
        quit = hello.get('quit') or {}
        check('hello carries File/Quit\'s question as the core asks it (W110: the page asks the same during a run)',
              ask and quit.get('question') == ask.get('question') and quit.get('choices') == ask.get('choices')
              and quit.get('keys') == ask.get('keys')
              and quit.get('recording') == 'Quit xppautX? Save this session, and the recording in progress, first?',
              str(quit))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        snd(cmd='session.get_state')
        evs, _ = col(is_idle)
        check('quit: Cancel (Esc) keeps xppautX running', last_state(evs) is not None and p.poll() is None, '')
        # a question open when the window's quit comes is cancelled; the quit then asks its own
        _, ask = asked(snd, col, cmd='session.open_model')
        snd(cmd='session.quit')
        evs, ask2 = col(is_ask, timeout=30 * SLOW)
        check('quit with ask while a menu is open: the menu is cancelled (its idle), then the question of the quit',
              ask and is_ask(ask) and ask.get('kind') == 'file' and any(is_idle(e) for e in evs)
              and ask2 and ask2.get('question') == question, str(ask2))
        if ask2:
            snd(cmd='answer', id=ask2['id'], ok=0)
        col(is_idle)
        # the window's quit asks the same; d exits, saving nothing
        _, ask = asked(snd, col, cmd='session.quit')
        check('quit with ask (the window\'s Quit and close box) asks the same question',
              ask and is_ask(ask) and ask.get('question') == question and ask.get('keys') == 'sd', str(ask))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='d')
        _, ok = ended(p, col)
        check('quit: Don\'t save (d) exits (bye, status 0), writing nothing', ok and sorted(os.listdir(r)) == staged_model_names(args.ode),
              str(os.listdir(r)))
    finally:
        done(p, r, snd)

    # s saves the session, and the recording in progress with it, then exits
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='recording.start')
        col(is_idle)
        snd(cmd='run.initial')
        col(is_idle, timeout=30 * SLOW)
        _, ask = asked(snd, col, cmd='session.quit')
        check('quit while recording: the question says the recording is saved with the session',
              ask and is_ask(ask) and ask.get('question') == 'Quit xppautX? Save this session, and the recording in progress, first?',
              str(ask))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='s')
        _, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        check('quit: Save session asks the session file\'s name, as Save session does',
              ask and is_ask(ask) and ask.get('kind') == 'file' and ask.get('wild') == '*.snapx', str(ask))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], file='left')
        _, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        check('quit: then the recording\'s name, as its stop asks', ask and is_ask(ask) and ask.get('wild') == '*.recx', str(ask))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], file='left')
        _, ok = ended(p, col)
        snap, rec = os.path.join(r, 'left.snapx'), os.path.join(r, 'left.recx')
        text = open(rec, encoding='utf-8').read() if os.path.exists(rec) else ''
        _, _, steps, written, got = read_recx(text)
        check('quit: Save session (s) saves the session and the recording, then exits',
              ok and os.path.exists(snap) and 'session.setx' in zipfile.ZipFile(snap).namelist()
              and [x.get('cmd') for x, _ in steps] == [{'cmd': 'run.initial'}] and written == got, str(sorted(os.listdir(r))))
    finally:
        done(p, r, snd)

    # a quit that asks during a computation stops it, then asks
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='run.configure', total_time=1e7)
        col(is_idle)
        snd(cmd='run.initial')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        snd(cmd='session.quit')
        evs, ask = col(is_ask, timeout=30 * SLOW)
        kinds = [e.get('ev') for e in evs if e.get('ev') in ('stopped', 'done', 'ask')]
        check('quit during a run: the run stops first (stopped, done), then the question',
              kinds[:3] == ['stopped', 'done', 'ask'] and ask and ask.get('question') == question, str(kinds))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        # the plain quit: at once, nothing asked
        snd(cmd='session.quit', save=False)
        try:
            p.wait(timeout=10 * SLOW)
        except subprocess.TimeoutExpired:
            pass
        evs, _ = col(lambda e: False, timeout=1)
        check('the plain quit (scripts, --server) exits at once, asking nothing',
              p.poll() == 0 and not any(is_ask(e) for e in evs), str(p.poll()))
    finally:
        done(p, r, snd)

    def long_run(snd, col):
        """an integration that goes on for minutes, going"""
        snd(cmd='run.configure', total_time=1e7)
        col(is_idle)
        snd(cmd='run.initial')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)

    # W110: the page asked during a run and was answered Save session: the
    # run stops, the session is saved (its file asked), then the exit
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        long_run(snd, col)
        snd(cmd='session.quit', save=True)
        evs, ask = col(is_ask, timeout=30 * SLOW)
        kinds = [e.get('ev') for e in evs if e.get('ev') in ('stopped', 'idle', 'done', 'ask')]
        check('quit with save (the page\'s Save session) during a run: the run stops, then the session file is asked, '
              'no question before it', kinds[:3] in (['stopped', 'idle', 'ask'], ['stopped', 'done', 'ask']) and ask and ask.get('kind') == 'file'
              and ask.get('wild') == '*.snapx', str(kinds) + ' ' + str(ask))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], file='kept')
        _, ok = ended(p, col)
        snap = os.path.join(r, 'kept.snapx')
        check('quit with save: the session saved, then the exit (bye, status 0)',
              ok and os.path.exists(snap) and 'session.setx' in zipfile.ZipFile(snap).namelist(), str(sorted(os.listdir(r))))
    finally:
        done(p, r, snd)

    # a cancelled save keeps the session (its run already stopped); the
    # page's Don't save is the plain quit, which ends a run at once
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='session.quit', save=True)
        _, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        snd(cmd='session.get_state')
        evs, _ = col(is_idle)
        check('quit with save: a cancelled save keeps xppautX running', ask and is_ask(ask) and ask.get('kind') == 'file'
              and last_state(evs) is not None and p.poll() is None, str(ask))
        long_run(snd, col)
        snd(cmd='session.quit', save=False)
        try:
            p.wait(timeout=10 * SLOW)
        except subprocess.TimeoutExpired:
            pass
        check('the plain quit during a run (the page\'s Don\'t save) exits at once, asking nothing, saving nothing',
              p.poll() == 0 and sorted(os.listdir(r)) == staged_model_names(args.ode), str(p.poll()) + ' ' + str(os.listdir(r)))
    finally:
        done(p, r, snd)


def recx_snapshot(text):
    """the session a .recx begins from (its @snapshot section, W59d): the
    .snapx's members {name: bytes}, or None when it has none"""
    lines = text.split('\n')
    if '@snapshot' not in lines:
        return None
    i = lines.index('@snapshot') + 1
    digits = ''.join(lines[i:lines.index('@end', i)])
    z = zipfile.ZipFile(io.BytesIO(base64.b64decode(digits)))
    return {n: z.read(n) for n in z.namelist()}


def check_recording():
    """W59a: File/recorD and the record command write name.recx: the model
    embedded, one step per command idle to idle (a menu's key, a dialog's
    answers, a cancel, an Abort with where it stopped, a view), a note above
    its step, and a fingerprint of the files and steps, not the notes"""
    p, r, snd, col, _ = launch_server()
    is_ask = lambda e: e.get('ev') == 'ask'

    def run(**cmd):
        snd(**cmd)
        evs, _ = col(is_idle, timeout=30 * SLOW)
        return evs

    def asked(**cmd):
        snd(**cmd)
        return col(is_ask)[1]

    try:
        col(is_idle)
        st = last_state(run(cmd='recording.start'))
        check('record: start, and state says recording, no step yet',
              st and st.get('recording') == {'steps': 0, 'note': ''}, str(st and st.get('recording')))
        st = last_state(run(cmd='recording.add_note', note='First run.\nThe cell settles.'))
        check('record: a note waits for the next step (state.recording.note)',
              st and st['recording'] == {'steps': 0, 'note': 'First run.\nThe cell settles.'}, str(st and st.get('recording')))
        run(cmd='run.initial')
        run(cmd='window.clear')
        run(cmd='run.configure', total_time=1e7)
        snd(cmd='run.initial')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        stopped = [e['at'] for e in evs if e.get('ev') == 'stopped']
        run(cmd='window.set_display', window='graph_1', x_range=[0, 50])
        st = last_state(run(cmd='value.set_parameters', values={'iapp': 0.1}))
        check('record: state counts the steps taken', st and st['recording']['steps'] == 6, str(st and st.get('recording')))
        evs = run(cmd='recording.stop', file='mine')
        path = os.path.join(r, 'mine.recx')
        check('record: stop writes name.recx and the recording ends',
              os.path.exists(path) and 'recording' not in (last_state(evs) or {'recording': 1}), str(os.listdir(r)))
        text = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
        header, files, steps, written, got = read_recx(text)
        ode = os.path.basename(args.ode)
        check('record: the header names the format, program, model and date',
              header[0] == 'recording: {' and header[1] == '  version: 4,' and header[2].startswith('  program: "xppautX ')
              and header[3] == '  model: "%s",' % ode and re.match(r'  recorded: "\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ",$', header[4]),
              str(header))
        want = open(os.path.join(r, ode), encoding='utf-8').read().splitlines()
        check('record: the model is embedded whole, and nothing else was read',
              list(files) == [ode, 'ml.anix'] and files[ode] == want, str(list(files)))
        s = [x for x, _ in steps]
        check('record: run.initial is one step, its label from its menu path, its note above it',
              s and s[0] == {'step': 'Run → Run from initial', 'cmd': {'cmd': 'run.initial'}} and steps[0][1] == 'First run.\nThe cell settles.',
              str(steps[:1]))
        check('record: a cancelled menu is a step (keys g, Escape)',
              len(s) > 1 and s[1] == {'step': 'Clear plot', 'cmd': {'cmd':'window.clear'}, 'view': True} and steps[1][1] == '', str(s[1:2]))
        check('record: run.configure is a step with its arguments',
              s[2:3] == [{'step': 'Run → Settings', 'cmd': {'cmd': 'run.configure', 'total_time': 1e7}}], str(s[2:3]))
        check('record: an Abort belongs to its step, where it stopped as the stopped event said',
              len(s) > 3 and s[3].get('cmd') == {'cmd': 'run.initial'} and stopped and s[3].get('abort') == stopped[0]
              and stopped[0]['what'] == 'integrate', str(s[3:4]) + str(stopped))
        check('record: a zoom is a view step with its command; a set a step of its own',
              s[4:6] == [{'step': 'Zoom window graph_1', 'cmd': {'cmd': 'window.set_display', 'window': 'graph_1', 'x_range': [0, 50]}, 'view': True},
                         {'step': 'Set iapp = 0.1', 'cmd': {'cmd': 'value.set_parameters', 'values': {'iapp': 0.1}}}]
              and len(s) == 6, str(s[4:]))
        check('record: the fingerprint is the files\' and steps\' SHA-256', written == got and len(got) == 64, '%s %s' % (written, got))
        edited = text.replace('# First run.', '# The first run.')
        _, _, _, w2, g2 = read_recx(edited)
        check('record: editing a note keeps the fingerprint', edited != text and w2 == g2, '')
        edited = text.replace('window.clear', 'window.redraw')
        _, _, _, w2, g2 = read_recx(edited)
        check('record: editing a step changes it', edited != text and w2 != g2, '')

        # File/recorD starts and stops; the File menu opened to stop is no step
        run(cmd='value.save_file', kind='parameters', file='saved.par')
        run(cmd='recording.start')
        run(cmd='value.load_file', kind='parameters', file='saved.par')
        run(cmd='window.clear')
        snd(cmd='recording.stop')
        evs, ask = col(lambda e: is_ask(e) or is_idle(e))
        check('File/recorD again asks the name, as the other File saves (*.recx, the model\'s name)',
              ask and ask.get('kind') == 'file' and ask.get('wild') == '*.recx' and ask.get('file') == 'lecar.recx', str(ask))
        if ask and ask.get('kind') == 'file':
            run(cmd='answer', id=ask['id'], file='menu')
        _, files, steps, written, got = read_recx(open(os.path.join(r, 'menu.recx'), encoding='utf-8').read()
                                                  if os.path.exists(os.path.join(r, 'menu.recx')) else '')
        check('File/recorD: the steps between (the file a step read named by its section), and not the File menu opened to stop',
              [x for x, _ in steps] == [{'step': 'File → Values → Load file', 'cmd': {'cmd': 'value.load_file', 'kind': 'parameters', 'file': 'saved.par'},
                                         'files': [2]},
                                        {'step': 'Clear plot', 'cmd': {'cmd': 'window.clear'}, 'view': True}] and written == got, str(steps))
        par = open(os.path.join(r, 'saved.par'), encoding='utf-8').read().splitlines()
        check('record: a file the session read while recording is embedded after the model',
              list(files) == [ode, 'ml.anix', 'saved.par'] and files['saved.par'] == par, str(list(files)))
        evs = run(cmd='recording.add_note', note='x')
        check('record: a note while not recording is an error',
              any(e.get('ev') in ('message', 'done') and 'Not recording' in e.get('error', '') for e in evs), '')
    finally:
        stop_server(p, r, snd)


def check_player():
    """W59b: a recording played back: its model from the file, each step
    run as recorded (a menu's key, a dialog's answer, a setting, an Abort at
    its row), a press event before each input, the same data as the
    session that was recorded; a note saved keeps the fingerprint, and a
    changed step warns and still plays"""
    p, r, snd, col, _ = launch_server()
    is_ask = lambda e: e.get('ev') == 'ask'

    def run(**cmd):
        snd(**cmd)
        evs, _ = col(is_idle, timeout=30 * SLOW)
        return evs

    def asked(**cmd):
        snd(**cmd)
        return col(is_ask)[1]

    def write(name):
        run(cmd='browser.export', target='output', format='dat', file=name)

    def open_player(name):
        """play open, answering the question about this model's session"""
        snd(cmd='recording.open', play=False, file=name)
        evs, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='d')
            evs, _ = col(is_idle, timeout=30 * SLOW)
        return next((e for e in evs if e.get('ev') == 'player'), None), evs

    def play_to_end(n):
        """play start, at 8x, until the player's last step ended: the events"""
        run(cmd='recording.set_speed', speed=8)
        snd(cmd='recording.play')
        got = []
        deadline = time.time() + 120 * SLOW
        while time.time() < deadline:
            evs, st = col(lambda e: e.get('ev') == 'state' and (e.get('player') or {}).get('step') == n
                          and e['player']['running'] == -1, timeout=60 * SLOW)
            got += evs
            if st:
                break
        got += col(is_idle, timeout=10 * SLOW)[0]
        return got

    def get(name):
        snd(cmd='file', op='get', name=name)
        evs, ev = col(lambda e: e.get('ev') == 'file')
        col(is_idle)
        return base64.b64decode(ev['data']) if ev and ev.get('ok') else None

    try:
        col(is_idle)
        # W59d: the recording begins partway through the session, after a
        # parameter changed and a run: the replay starts from that state
        run(cmd='value.set_parameters', values={'phi': 0.06})
        run(cmd='run.initial')
        run(cmd='recording.start')
        run(cmd='recording.add_note', note='The cell fires once and settles.')
        run(cmd='run.initial')
        write('run1.dat')
        run(cmd='value.set_parameters', values={'iapp': 0.08})
        run(cmd='run.initial')
        write('run2.dat')
        run(cmd='run.configure', total_time=1e7)
        snd(cmd='run.initial')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        stopped = [e['at'] for e in evs if e.get('ev') == 'stopped']
        write('run3.dat')
        run(cmd='recording.stop', file='play')
        path = os.path.join(r, 'play.recx')
        text = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
        _, _, steps, _, _ = read_recx(text)
        orig = {n: open(os.path.join(r, n), 'rb').read() for n in ('run1.dat', 'run2.dat', 'run3.dat')
                if os.path.exists(os.path.join(r, n))}
        check('player: the session recorded (8 steps, an abort at its row)',
              len(steps) == 8 and len(orig) == 3 and stopped and stopped[0]['what'] == 'integrate',
              '%d steps, %s' % (len(steps), stopped))
        snap = recx_snapshot(text) or {}
        check('record: the recording begins with the session as it was (@snapshot: a .snapx without the data table)',
              'session.setx' in snap and 'model/lecar.odex' in snap and 'model.setx' in snap and 'data.npz' not in snap,
              str(sorted(snap)))
        # no fallback: a recording without its snapshot is refused
        lines = text.split('\n')
        at = lines.index('@snapshot') if '@snapshot' in lines else 0
        cut = '\n'.join(lines[:at] + lines[lines.index('@end', at) + 1:]) if at else text
        open(os.path.join(r, 'nosnap.recx'), 'w', encoding='utf-8').write(cut)
        evs = run(cmd='recording.open', play=False, file=os.path.join(r, 'nosnap.recx'))
        check('play open: a recording without its @snapshot is an error, nothing asked',
              any(e.get('ev') in ('message', 'done') and 'no @snapshot section' in e.get('error', '') for e in evs)
              and not any(is_ask(e) for e in evs), str([e for e in evs if e.get('ev') in ('message', 'done')]))

        pl, evs = open_player(path)
        st = last_state(evs)
        check('play open: the player event with the steps and their notes, the file intact',
              pl and pl.get('intact') is True and len(pl.get('steps', [])) == 8
              and pl['steps'][0].get('note') == 'The cell fires once and settles.'
              and pl['steps'][0].get('cmd') == {'cmd': 'run.initial'}, str(pl)[:300])
        check('play open: state.player at step 0, paused, 1x',
              st and st.get('player') == {'step': 0, 'running': -1, 'playing': False, 'speed': 1, 'fast': False,
                                          'intact': True}, str(st and st.get('player')))
        os.remove(os.path.join(r, 'run1.dat'))  # nothing may read the disk's copies
        evs = play_to_end(8)
        presses = [(e['step'], e['what'], e['index']) for e in evs if e.get('ev') == 'press']
        check('play: a press before every input (the commands)',
              presses[:2] == [(0, 'cmd', 0), (1, 'cmd', 0)] and (2, 'cmd', 0) in presses and (5, 'cmd', 0) in presses
              and all(what == 'cmd' for _, what, _ in presses), str(presses[:12]))
        # W150: 1x is slower (core/json_player.cpp PACE_SLOWDOWN 1.5): the first press of step 0 (700 ms + the note's
        # reading time, 35 ms a character, at most 3 s), played at 8x
        note0 = 'The cell fires once and settles.'
        want0 = (700 + min(3000, 35 * len(note0))) * 1.5 / 8
        ms0 = next((e['ms'] for e in evs if e.get('ev') == 'press' and e['step'] == 0 and e['index'] == 0), None)
        check('play: the first press of a step is paced 1.5 times the old pace (%.1f ms at 8x)' % want0,
              ms0 is not None and abs(ms0 - want0) <= 2, str(ms0))
        again = [e['at'] for e in evs if e.get('ev') == 'stopped']
        check('play: the recorded Abort stops the replayed run at its row', again == stopped, '%s %s' % (again, stopped))
        errors = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
        check('play: no error on the way', not errors, str(errors))
        for n in ('run1.dat', 'run2.dat', 'run3.dat'):
            check('play: the replay, from the session\'s state at Record (phi 0.06), gives the same data (%s)' % n,
                  get(n) == orig.get(n) and orig.get(n), 'differs' if n in orig else 'missing')
        check('play: the replay wrote in its own folder, not beside the recording',
              not os.path.exists(os.path.join(r, 'run1.dat')), str(sorted(os.listdir(r))))

        # a note saved keeps the fingerprint
        evs = run(cmd='recording.set_step_note', step=3, note='Now the drive current is raised.')
        pl = next((e for e in evs if e.get('ev') == 'player'), None)
        _, _, steps2, w2, g2 = read_recx(open(path, encoding='utf-8').read())
        check('play note: written above its step in the file, which stays intact',
              pl and pl['steps'][3].get('note') == 'Now the drive current is raised.' and pl.get('intact') is True
              and steps2[3][1] == 'Now the drive current is raised.' and w2 == g2, str(steps2[3:4]))

        # a changed step: a warning (intact false) and it still plays
        changed = text.replace('"iapp": 0.08', '"iapp": 0.09')
        open(os.path.join(r, 'changed.recx'), 'w', encoding='utf-8').write(changed)
        pl, evs = open_player(os.path.join(r, 'changed.recx'))
        check('play open: a changed step makes the file not intact', pl and pl.get('intact') is False and changed != text,
              str(pl and pl.get('intact')))
        evs = play_to_end(8)
        check('play: a changed recording still plays to its end',
              (last_state(evs) or {}).get('player', {}).get('step') == 8, '')
        check('play: its changed step ran as it now reads (other data)', get('run2.dat') not in (None, orig.get('run2.dat')), '')

        # play from a step: the steps before run with no pace or press
        snd(cmd='recording.seek', step=6, play=False)
        evs, _ = col(lambda e: e.get('ev') == 'state' and (e.get('player') or {}).get('step') == 6
                     and e['player']['running'] == -1 and not e['player']['fast'], timeout=120 * SLOW)
        check('play from: the steps before it run with no pace, then it waits',
              _ is not None and all(e['ms'] == 0 for e in evs if e.get('ev') == 'press'), '')
        col(is_idle, timeout=10 * SLOW)

        # W59c: a recording on the command line starts in the player, no question asked
        done = subprocess.run([os.path.abspath(args.server), '--server', path], cwd=r, input='',
                              capture_output=True, text=True, encoding='utf-8', timeout=60 * SLOW)
        evs = [json.loads(l) for l in done.stdout.splitlines() if l.strip().startswith('{')]
        pl = next((e for e in evs if e.get('ev') == 'player'), None)
        check('command line: xppautX name.recx opens the recording in the player, asking nothing',
              pl is not None and len(pl.get('steps', [])) == 8 and not any(e.get('ev') == 'ask' for e in evs),
              done.stderr[-300:])

        # a recording that is not one
        open(os.path.join(r, 'bad.recx'), 'w').write('hello\n')
        evs = run(cmd='recording.open', play=False, file=os.path.join(r, 'bad.recx'))
        check('play open: a file that is not a recording is an error',
              any(e.get('ev') in ('message', 'done') and 'not a recording' in e.get('error', '') for e in evs), '')

        # W125: a recording whose last step does not read is refused at its line, before
        # anything is played or loaded: the player as it was
        rows = text.split('\n')
        k = max(i for i, l in enumerate(rows) if l.startswith('{'))
        rows[k] = 'not json'
        open(os.path.join(r, 'laststep.recx'), 'w', encoding='utf-8').write('\n'.join(rows))
        before = (last_state(run(cmd='session.get_state')) or {}).get('player')
        evs = run(cmd='recording.open', play=False, file=os.path.join(r, 'laststep.recx'))
        m = next((e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')), {})
        check('W125: a recording whose last step does not read is refused at that line, the line as written',
              os.path.basename(m.get('file', '')) == 'laststep.recx' and m.get('line') == k + 1
              and m.get('source') == 'not json' and 'cannot be played' in m.get('error', ''), str(m))
        check('W125: after the refused recording the player is as it was',
              before is not None and (last_state(run(cmd='session.get_state')) or {}).get('player') == before, str(before))
    finally:
        stop_server(p, r, snd)


def check_ani_grab_job():
    # An armed row stop makes grab integration/cancellation independent of speed.
    grab_rows = 400  # strictly inside lecar's default 601 stored rows
    with tempfile.TemporaryDirectory(prefix='xppgrab') as r:
        copy_model(args.ode, r)
        for name, runnow in [('grab.ani', 1), ('view.ani', 0)]:
            with open(os.path.join(r, name), 'w') as f:
                f.write(chr(10).join(['dimension -1;-1;1;1', 'speed 0', 'transient',
                                       'circle 0;0;0.1;$RED;1', 'grab 0;0;0.2',
                                       '{w=w}', '{runnow=%d}' % runnow, 'end', '']))
        lines = [
            {'cmd': 'run.initial'},
            {'cmd':'animation.open'},
            dict(cmd='animation.load',file='grab.ani'),
            {'cmd': 'recording.start'},
            {'cmd':'animation.grab_point'},
            {'cmd': 'animation.grab_point', 'x': 0.0, 'y': 0.0},
            {'cmd': 'run.abort', 'at': {'what': 'integrate', 'rows': grab_rows, 't': 0}},
            {'cmd': 'animation.pause'},
            dict(cmd='animation.load',file='view.ani'),
            {'cmd':'animation.grab_point'},
            {'cmd': 'animation.grab_point', 'x': 0.0, 'y': 0.0},
            {'cmd': 'recording.stop', 'file': 'grab'},
        ]
        path = make_recording(args.server, args.ode, lines, r)
        result = replay_recording(args.server, args.ode, path, r)
        evs = [json.loads(l) for l in result.stdout.splitlines() if l.strip()]
        stopped = [e for e in evs if e.get('ev') == 'stopped']
        check('W136: animator grab computes as a job and stops at its armed row',
              result.returncode == 0 and sum(e.get('ev') == 'computing' for e in evs) == 2
              and len(stopped) == 1 and stopped[0]['at'].get('rows') == grab_rows, result.stderr[-300:])
        path = os.path.join(r, 'grab.recx')
        text = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
        _, _, steps, _, _ = read_recx(text)
        grabs = [x for x, _ in steps if x.get('cmd', {}).get('cmd') == 'animation.grab_point' and 'x' in x['cmd']]
        check('W136: a grab that integrates is a computing recording step, stopped at its armed row',
              len(grabs) == 2 and not any(x.get('view') for x in grabs) and grabs[0].get('abort', {}).get('rows') == grab_rows, str(grabs))
        check('W136: pause while idle is recorded as a view without an error',
              any(x.get('cmd', {}).get('cmd') == 'animation.pause' and x.get('view') for x, _ in steps)
              and not any(e.get('error') for e in evs if e.get('ev') == 'message'), '')


check_ani_grab_job()





def check_parser_group():
    """W312/W313/W315/W324 share the core load in server and silent modes."""
    with tempfile.TemporaryDirectory(prefix='xppparser') as source:
        binary = os.path.abspath(args.server)

        def run(file, mode, extra=()):
            return subprocess.run([binary, file, mode] + list(extra), cwd=source,
                                  input='{"cmd": "session.quit", "save": False}\n' if mode == '--server' else '',
                                  capture_output=True, text=True, encoding='utf-8', timeout=30 * SLOW)

        def write(file, text):
            with open(os.path.join(source, file), 'w', encoding='utf-8') as f:
                f.write(text)

        empty_message = 'it has no equations: it is a file to import into a model, not a model to run'
        for text in ('# empty\n', 'par gain=2\n', 'par gain=2\naux z=gain\n'):
            write('values.odex', text)
            for mode in ('--server', '--silent'):
                p = run('values.odex', mode)
                check('W324: %s refuses a file without equations (%r)' % (mode, text),
                      p.returncode == 1 and 'values.odex: ' + empty_message in p.stderr, p.stderr)
                if mode == '--server':
                    evs = [json.loads(l) for l in p.stdout.splitlines() if l.strip()]
                    errs = [e for e in evs if e.get('ev') == 'error']
                    check('W324: one core error, file named, no hello',
                          len(errs) == 1 and errs[0].get('file') == 'values.odex'
                          and errs[0].get('error') == empty_message
                          and not any(e.get('ev') == 'hello' for e in evs), str(evs))

        # W317: the same strict option reader in check, server and silent modes.
        presets = ', '.join('{ name: x, low: -1, high: %d }' % i for i in range(1, 6))
        write('presets.odex', "x'=0\ninit x=0\noptions { integration: { total_time: .02, dt: .01 }, interface: { sliders: [" + presets + " ] } }\n")
        for mode in ('--check', '--server', '--silent'):
            p = run('presets.odex', mode)
            check('W317: %s accepts all five native presets' % mode, p.returncode == 0, p.stderr)
            for group in ('numerics', 'auto', 'sliders', 'range', 'histogram', 'batch', 'log', 'buttons'):
                write('oldgroup.odex', "x'=0\ninit x=0\noptions { " + group + ": {} }\n")
                p = run('oldgroup.odex', mode)
                if mode == '--check':
                    diagnostics = json.loads(p.stdout)['diagnostics']
                    rejected = p.returncode == 2 and any(d['file'] == 'oldgroup.odex' and d['line'] == 3 and d['col'] == 11
                                                        and group in d['message'] for d in diagnostics)
                else:
                    rejected = p.returncode != 0 and 'oldgroup.odex:3:11:' in p.stderr and group in p.stderr
                check('W317: %s refuses old group %s at its source line' % (mode, group), rejected, p.stderr + p.stdout)
        # W494: a value outside its range, or two that break an order, is refused at its line in every mode
        for body, line, words in (
                ('integration: { total_time: .02, dt: .01, min_step: 1, max_step: 0.5 }', 2, 'min_step (1) must be at most max_step (0.5)'),
                ('plot: { xlo: 3, xhi: 1 }', 2, 'xlo (3) must be below xhi (1)'),
                ('analysis: { histlo: 2, histhi: 2 }', 2, 'histlo (2) must be below histhi (2)'),
                ('analysis: { histbins: 1 }', 2, 'must be a whole number of at least 2'),
                ('plot: { nplot: 9 }', 2, 'exceeds the allocation maximum'),
                ('plot: { dfgrid: 1 }', 2, 'must be a whole number of at least 2'),
                ('integration: { torus_period: 0 }', 2, 'must be a number above 0'),
                ('sweep: { rangestep: 0 }', 2, 'must be a whole number of at least 1')):
            write('badrange.odex', "x'=0\ninit x=0\noptions { " + body + " }\n")
            for mode in ('--check', '--silent'):
                p = run('badrange.odex', mode)
                check('W494: %s refuses %s (%s)' % (mode, body, words),
                      p.returncode != 0 and words in p.stdout + p.stderr and 'badrange.odex' in p.stdout + p.stderr,
                      p.stderr + p.stdout)
        options = 'options { integration: { total_time: .02, dt: .01 } }\n'
        write('missing.odex', "x'=0\ny'=0\ninit x=2\n" + options)
        write('array.odex', "x[j]'=0 for j in 1..4\ninit x[2]=1\n" + options)
        write('stepped.odex', "x[j]'=0 for j in 1..5 by 2\n" + options)
        for mode in ('--server', '--silent'):
            p = run('missing.odex', mode)
            check('W313: %s warns once naming only the missing equation' % mode,
                  p.returncode == 0 and p.stderr.count('states without init') == 1
                  and 'missing.odex:2:1: y' in p.stderr and 'missing.odex:1:1: x' not in p.stderr, p.stderr)
            p = run('array.odex', mode)
            check('W313: %s names a partial array once with its missing ranges' % mode,
                  p.returncode == 0 and p.stderr.count('array.odex:1:1: x[1, 3..4]') == 1, p.stderr)
            p = run('stepped.odex', mode)
            check('W313: %s names a stepped array once' % mode,
                  p.returncode == 0 and 'stepped.odex:1:1: x[1..5 by 2]' in p.stderr, p.stderr)
            p = run('missing.odex', mode, ['--runnow'])
            check('W312: %s refuses --runnow as an unknown flag' % mode,
                  p.returncode != 0 and '--runnow' in p.stderr and 'no such option --runnow' in p.stderr, p.stderr)
            write('runnow.odex', "x'=0\noptions { output: { runnow: 1 } }\n")
            p = run('runnow.odex', mode)
            check('W312: %s refuses runnow at its file and line' % mode,
                  p.returncode == 1 and 'runnow.odex:2:' in p.stderr and '`runnow` is not an option' in p.stderr, p.stderr)
            write('prefix.odex', 'import "values.odex" as c1\n')
            p = run('prefix.odex', mode)
            check('W315: %s rejects the prefix at the import line' % mode,
                  p.returncode == 1 and 'prefix.odex:1:22:' in p.stderr and '`as`' in p.stderr, p.stderr)

        write('zero.ode', "x'=0\ny'=0\n@ runnow=1,total=.02,dt=.01\ndone\n")
        p = subprocess.run([binary, 'zero.ode', '--convert'], cwd=source,
                           capture_output=True, text=True, encoding='utf-8', timeout=30 * SLOW)
        check('W312: conversion drops runnow with its diagnostic',
              p.returncode == 0 and '[warning: runnow-removed]' in p.stderr, p.stderr)
        for mode in ('--server', '--silent'):
            p = run('zero.odex', mode)
            check('W313: %s loads the converted zero initial conditions quietly' % mode,
                  p.returncode == 0 and not p.stderr.strip(), p.stderr)


check_parser_group()

def check_player_ani():
    """W59b: Escape during the animation's Go, a key the running job reads
    itself, is recorded with the frame it came at, and the replay stops the
    Go at that frame; the .ani the session read is played from the
    recording"""
    p, r, snd, col, _ = launch_server()
    shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gui_test.ani'), r)
    frames = lambda evs: [e['pos'] for e in evs if e.get('ev') == 'ani' and e.get('op') == 'frame']

    def run(**cmd):
        snd(**cmd)
        got = []
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] in ('idle','done'):
                return got
            snd(cmd='answer', id=e['id'], ok=0)

    try:
        col(is_idle)
        run(cmd='events.subscribe', events=['ani'])
        run(cmd='recording.start')
        run(cmd='run.initial')
        run(cmd='animation.open')
        run(cmd='animation.load', file='gui_test.ani')
        run(cmd='animation.set_speed', ms=20)
        snd(cmd='animation.play')
        went, _ = col(lambda e: e.get('ev') == 'ani' and e.get('op') == 'frame' and e['pos'] >= 40, timeout=30 * SLOW)
        snd(cmd='key', key='Escape')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        escape_last = frames(went + evs)[-1:]
        run(cmd='animation.reset')
        snd(cmd='animation.play')
        went, _ = col(lambda e: e.get('ev') == 'ani' and e.get('op') == 'frame' and e['pos'] >= 40, timeout=30 * SLOW)
        snd(cmd='animation.set_speed', ms=10)
        snd(cmd='animation.pause')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        last = frames(went + evs)[-1:]
        for _ in range(2):  # each steering command's own done, after the Go's end
            col(lambda e: e.get('ev') == 'done', timeout=30 * SLOW)
        run(cmd='recording.stop', file='ani')
        text = open(os.path.join(r, 'ani.recx'), encoding='utf-8').read() if os.path.exists(os.path.join(r, 'ani.recx')) else ''
        _, files, steps, _, _ = read_recx(text)
        check('player: Escape during the animation\'s Go is recorded with its frame; the .ani is embedded',
              escape_last and 0 < escape_last[0] < 600 and 'gui_test.ani' in files
              and any(x.get('during', [{}])[0].get('at', {}).get('what') == 'ani' for x, _ in steps),
              '%s %s' % (last, [x for x, _ in steps][-2:]))
        controls = [d for x, _ in steps for d in x.get('during', []) if 'cmd' in d]
        check('W136: speed and pause during Go retain their commands and frames',
              [d['cmd'].get('cmd') for d in controls] == ['animation.set_speed', 'animation.pause']
              and all(d['at'].get('what') == 'ani' for d in controls), str(controls))
        os.remove(os.path.join(r, 'gui_test.ani'))  # the replay reads the recording's copy
        snd(cmd='recording.open', play=False, file=os.path.join(r, 'ani.recx'))
        evs, ask = col(lambda e: e.get('ev') in ('ask', 'idle','done'), timeout=30 * SLOW)
        if ask and ask.get('ev') == 'ask':
            snd(cmd='answer', id=ask['id'], key='d')
            col(is_idle, timeout=30 * SLOW)
        run(cmd='events.subscribe', events=['ani'])
        run(cmd='recording.set_speed', speed=8)
        snd(cmd='recording.play')
        evs, _ = col(lambda e: e.get('ev') == 'state' and (e.get('player') or {}).get('step') == len(steps)
                     and e['player']['running'] == -1, timeout=120 * SLOW)
        errors = [e.get('error') for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
        check('W136: replayed pause stops Go at its recorded frame, after its speed change',
              last and frames(evs)[-1:] == last and not errors
              and any(e.get('ev') == 'ani' and e.get('speed') == 10 for e in evs),
              '%s vs %s %s' % (frames(evs)[-1:], last, errors))
        col(is_idle, timeout=10 * SLOW)
    finally:
        stop_server(p, r, snd)


def check_ode_open():
    """W154: the shared open commits only the converted model and asks."""
    p, r, snd, col, _ = launch_server()
    try:
        initial, _ = col(is_idle)
        original = last_state(initial)
        foreign = os.path.join(r, 'foreign.ode')
        converted = os.path.join(r, 'foreign.odex')

        def write_foreign(value):
            with open(foreign, 'w') as f:
                f.write("par and=%d\nx'=and-x\ninit x=0\n@ total=1,dt=.1\ndone\n" % value)

        def begin():
            snd(cmd='session.open_model', file='foreign.ode')
            _, leave = col(is_ask)
            snd(cmd='answer', id=leave['id'], key='d')
            return col(lambda e: e.get('ev') in ('ask', 'idle','done'))

        write_foreign(1)
        _, rename = begin()
        check('ode open: reserved names ask string with the suggested default',
              rename and rename.get('kind') == 'string' and rename.get('value') == 'and_', str(rename))
        snd(cmd='answer', id=rename['id'], ok=False)
        col(is_idle)
        snd(cmd='window.redraw')
        kept, _ = col(is_idle)
        check('ode open: cancelling the name keeps the model and writes nothing',
              not os.path.exists(converted) and last_state(kept)['pars'] == original['pars'])

        _, rename = begin()
        snd(cmd='answer', id=rename['id'], value=rename['value'])
        opened, _ = col(is_idle)
        check('ode open: saves and opens odex, with the manual link',
              os.path.exists(converted) and any(e.get('ev') == 'hello' and 'foreign.odex' in e.get('title', '') for e in opened)
              and any(e.get('help') == {'chapter': '02-ode-files', 'anchor': 'odex'} for e in opened), str(opened)[:300])
        with open(converted, 'rb') as f:
            before = f.read()
        write_foreign(2)

        for key in (None, 'n', 'y'):
            _, rename = begin()
            snd(cmd='answer', id=rename['id'], value=rename['value'])
            _, choice = col(is_ask)
            check('ode open: existing different text uses the overwrite ask (%s)' % key,
                  choice and choice.get('question') == 'File Exists! Overwrite?'
                  and choice.get('choices') == ['Replace', 'Open existing .odex'] and choice.get('keys') == 'yn', str(choice))
            snd(cmd='answer', id=choice['id'], **({'ok': False} if key is None else {'key': key}))
            col(is_idle)
            with open(converted, 'rb') as f:
                after = f.read()
            check('ode open: %s leaves/writes the selected file' % ('Cancel' if key is None else key),
                  (after == before) if key != 'y' else (after != before))

        _, rename = begin()
        snd(cmd='answer', id=rename['id'], value=rename['value'])
        _, settled = col(lambda e: e.get('ev') in ('ask', 'idle','done'))
        check('ode open: identical conversion opens without an overwrite ask', settled and settled.get('ev') in ('idle', 'done'))
        write_foreign(3)
        snd(cmd='session.reload_model')
        _, leave = col(is_ask)
        snd(cmd='answer', id=leave['id'], key='d')
        reloaded, _ = col(is_idle)
        check('ode open: Reload uses odex after the foreign source changes',
              any(e.get('ev') == 'hello' and 'foreign.odex' in e.get('title', '') for e in reloaded)
              and any(e.get('ev') == 'state' and ['and_', 2] in e.get('pars', []) for e in reloaded))
        with open(converted, 'rb') as f:
            before = f.read()
        silent = subprocess.run([os.path.abspath(args.server), 'foreign.ode', '--silent'], cwd=r, input='and_\n',
                                capture_output=True, text=True, timeout=30 * SLOW)
        with open(converted, 'rb') as f:
            after = f.read()
        check('ode open: silent with no answer refuses different existing text, names both files and writes nothing',
              silent.returncode == 1 and 'foreign.ode' in silent.stderr and 'foreign.odex' in silent.stderr and before == after)
        os.remove(converted)
        silent = subprocess.run([os.path.abspath(args.server), 'foreign.ode', '--silent', '-y'], cwd=r, input='',
                                capture_output=True, text=True, timeout=30 * SLOW)
        check('ode open: silent -y takes suggestions and prints the saved path',
              silent.returncode == 0 and 'Converted foreign.ode and saved as foreign.odex' in silent.stderr and os.path.exists(converted))
        # A directory at the destination exercises a write failure on Windows
        # too, even for a user who can bypass Unix permission bits.
        with open(os.path.join(r, 'blocked.ode'), 'w') as f:
            f.write("x'=-x\ndone\n")
        os.mkdir(os.path.join(r, 'blocked.odex'))
        snd(cmd='session.open_model', file='blocked.ode')
        _, leave = col(is_ask)
        snd(cmd='answer', id=leave['id'], key='d')
        failed, _ = col(is_idle)
        check('ode open: an unwritable destination is named, no model replacement',
              any(e.get('file') == 'blocked.odex' and e.get('error') for e in failed)
              and not any(e.get('ev') == 'hello' for e in failed)
              and last_state(failed)['pars'] == [['and_', 2]], str(failed[-5:])[:300])
        # At command-line startup the rename must be answerable before hello.
        p2, r2, snd2, col2, _ = launch_server(ode=foreign)
        try:
            _, rename = col2(is_ask)
            check('ode startup: reserved names ask string before hello',
                  rename and rename.get('kind') == 'string' and rename.get('value') == 'and_', str(rename))
            if rename:
                snd2(cmd='answer', id=rename['id'], value=rename['value'])
            startup, _ = col2(is_idle)
            check('ode startup: accepting saves and opens the converted model',
                  os.path.isfile(os.path.join(r2, 'foreign.odex'))
                  and any(e.get('ev') == 'hello' and e.get('file') == 'foreign.odex' for e in startup)
                  and last_state(startup)['pars'] == [['and_', 3]], str(startup[-5:])[:300])
        finally:
            stop_server(p2, r2, snd2)
    finally:
        stop_server(p, r, snd)


check_ode_open()
check_quit()
check_recording()
check_player()
check_player_ani()

check_open_reload()
check_display_state()
check_auto_views()
check_plot_limits()
check_session_file()
check_save_session()
check_session_random()

send(cmd='session.quit')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
if ask:
    send(cmd='answer', id=ask['id'], key='d')
try:
    proc.wait(timeout=5 * SLOW)
    check('File/Quit exits', True)
except subprocess.TimeoutExpired:
    proc.kill()
    check('File/Quit exits', False)

shutil.rmtree(run, ignore_errors=True)

def check_terminal_playback():
    """W144 review: outputs survive, and only the terminal authorizes replacement."""
    binary = os.path.abspath(args.server)
    with tempfile.TemporaryDirectory(prefix='xpprec-input') as source, \
            tempfile.TemporaryDirectory(prefix='xpprec-output') as destination:
        data = b'recorded output\n'
        lines = [{'cmd':'file', 'op':'put', 'name':'saved.dat', 'data':base64.b64encode(data).decode()},
                 {'cmd':'file', 'op':'put', 'name':'after.dat', 'data':'YWZ0ZXI='}]
        path = make_recording(binary, args.ode, lines, source)
        def play(answer='', automatic=False):
            return subprocess.run([binary, path, '--silent'] + (['--yes'] if automatic else []),
                cwd=destination, input=answer, capture_output=True, text=True, encoding='utf-8', timeout=30 * SLOW)
        saved = os.path.join(destination, 'saved.dat')
        after = os.path.join(destination, 'after.dat')
        result = play()
        check('W144: silent recording output survives in the launch folder with its recorded content',
              result.returncode == 0 and result.stdout == '' and open(saved, 'rb').read() == data
              and not os.path.exists(os.path.join(source, 'saved.dat')), result.stderr)
        os.remove(after)
        with open(saved, 'wb') as f: f.write(b'keep')
        result = play()
        check('W144: closed stdin refuses replacement once at the step and plays nothing after it',
              result.returncode == 1 and open(saved, 'rb').read() == b'keep' and not os.path.exists(after)
              and 'saved.dat exists. Replace it? [no] (60 s)' in result.stderr
              and 'saved.dat already exists; replace not confirmed (--yes replaces)' in result.stderr
              and result.stderr.count(path + ':') == 1, result.stderr)
        result = play('y\n')
        check('W144: a confirmation line on piped stdin replaces the output and resumes playback',
              result.returncode == 0 and open(saved, 'rb').read() == data and os.path.exists(after), result.stderr)
        with open(saved, 'wb') as f: f.write(b'keep again')
        result = play(automatic=True)
        check('W144: --yes replaces existing outputs without a terminal question',
              result.returncode == 0 and open(saved, 'rb').read() == data and '(60 s)' not in result.stderr
              and 'saved.dat exists. Replace it? -> yes (answered automatically)' in result.stderr, result.stderr)
        with open(path, encoding='utf-8') as f: template = f.read()
        for name in ['../escaped.par', 'sub/escaped.par', os.path.join(destination, 'escaped.par'), 'bad..par']:
            steps = [{'cmd':{'cmd': 'value.save_file', 'kind': 'parameters', 'file':name}},
                     {'cmd':{'cmd':'file', 'op':'put', 'name':'after.dat', 'data':'YWZ0ZXI='}}]
            with open(path, 'w', encoding='utf-8', newline='\n') as f: f.write(recording_text(template, steps))
            if os.path.exists(after): os.remove(after)
            before = sorted(os.listdir(destination))
            result = play(automatic=True)
            check('W144: recorded path %r is refused at its step without writing anything' % name,
                  result.returncode == 1 and result.stderr.count(path + ':') == 1
                  and sorted(os.listdir(destination)) == before and open(saved, 'rb').read() == data, result.stderr)
        for form in [{'keys': ['i', 'g']}, {'cmd': {'cmd': 'run.initial'}, 'keys': ['Ctrl+R']}, {'cmd': {'cmd': 'run.initial'}, 'win': 'auto'}]:
            with open(path, 'w', encoding='utf-8', newline='\n') as f: f.write(recording_text(template, [form]))
            result = play(automatic=True)
            member = next(m for m in ('keys', 'win') if m in form)
            check('W536: a recorded step with %s is refused with its file, line and value' % member,
                  result.returncode == 1 and result.stderr.count(path + ':') >= 1 and '`%s`' % member in result.stderr
                  and 'a step is a command' in result.stderr, result.stderr)
        foreign = os.path.join(source, 'reserved.ode')
        with open(foreign, 'w') as f: f.write("par and=1\nx'=and\ndone\n")
        converted = foreign[:-4] + '.odex'
        result = subprocess.run([binary, '--convert', foreign], input='', capture_output=True,
                                text=True, encoding='utf-8', timeout=30 * SLOW)
        check('W144: --convert still refuses a reserved name when terminal input ends',
              result.returncode == 1 and not os.path.exists(converted), result.stderr)
        result = subprocess.run([binary, '--convert', foreign], input='and_\n', capture_output=True,
                                text=True, encoding='utf-8', timeout=30 * SLOW)
        check('W144: --convert uses the shared terminal question with piped input',
              result.returncode == 0 and os.path.exists(converted) and 'par and_ = 1' in open(converted).read()
              and '(60 s)' in result.stderr, result.stderr)
        model = os.path.join(source, 'other.odex')
        with open(args.ode, 'rb') as f: embedded = f.read().replace(b'  animation: \"ml.anix\",\n', b'')
        steps = [{'cmd':{'cmd': 'session.open_model', 'file': model}, 'answers':[{'key':'d'}]},
                 {'cmd':{'cmd':'file', 'op':'put', 'name':'next.dat', 'data':base64.b64encode(data).decode()}}]
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(recording_text(template, steps, [('other.odex', embedded), ('.xpprc', b'')]))
        result = play()
        check('W144: opening an embedded model keeps later outputs in the launch folder',
              result.returncode == 0 and os.path.exists(os.path.join(destination, 'next.dat'))
              and open(os.path.join(destination, 'next.dat'), 'rb').read() == data
              and not os.path.exists(model) and not os.path.exists(os.path.join(source, 'next.dat')), result.stderr)
        check("W288: a recording's recorded answer in --silent is printed with what it answers",
              "the recording's choice -> " in result.stderr and '(answered automatically)' in result.stderr, result.stderr)

check_terminal_playback()

def check_conversion_modes():
    """W288: a .ode's conversion in every mode that is not the page (check_ode_open
    is --server, web2check's files section the page): the answer comes from the
    terminal, from nobody (refused, naming the file) or from --yes (printed)."""
    binary = os.path.abspath(args.server)
    asked = '`and` is a reserved word in .odex; its new name'
    printed = asked + ' -> and_ (answered automatically)'
    replaced = 'foreign.odex exists; replace it -> yes (answered automatically)'
    differs = 'File Exists! Overwrite? -> Replace (answered automatically)'
    with tempfile.TemporaryDirectory(prefix='xppconvert') as d:
        ode = os.path.join(d, 'foreign.ode')
        odex = os.path.join(d, 'foreign.odex')
        with open(ode, 'w') as f: f.write("par and=1\nx'=and\ninit x=0\n@ total=1,dt=.1\ndone\n")
        def run(flags, answer=''):
            return subprocess.run([binary, *flags], cwd=d, input=answer, capture_output=True, text=True,
                                  encoding='utf-8', timeout=30 * SLOW)
        def forget():
            for n in ('foreign.odex', 'output.dat'):
                if os.path.exists(os.path.join(d, n)): os.remove(os.path.join(d, n))
        result = run(['foreign.ode', '--silent'], 'my_and\n')
        check('W288: --silent asks the conversion question on the terminal; the answer is used',
              result.returncode == 0 and asked in result.stderr and '(60 s)' in result.stderr
              and 'par my_and = 1' in open(odex).read(), result.stderr)
        forget()
        result = run(['foreign.ode', '--silent'])
        check('W288: --silent with no answer refuses, names the file and the name, writes nothing',
              result.returncode == 1 and 'foreign.ode' in result.stderr and 'and' in result.stderr
              and not os.path.exists(odex), result.stderr)
        result = run(['foreign.ode', '--silent', '--yes'])
        check('W288: --silent --yes takes the suggestion and prints question and answer',
              result.returncode == 0 and printed in result.stderr and 'par and_ = 1' in open(odex).read()
              and '(60 s)' not in result.stderr, result.stderr)
        with open(odex, 'a') as f: f.write('# changed\n')
        result = run(['foreign.ode', '--silent', '-y'])
        check('W288: --silent -y replacing a different .odex prints the replacement',
              result.returncode == 0 and differs in result.stderr and '# changed' not in open(odex).read(), result.stderr)
        forget()
        result = run(['--convert', 'foreign.ode'], 'my_and\n')
        check('W288: --convert asks on the terminal and uses the answer',
              result.returncode == 0 and asked in result.stderr and 'par my_and = 1' in open(odex).read(), result.stderr)
        result = run(['--convert', 'foreign.ode', '--yes'])
        check('W288: --convert --yes prints the suggestion taken and the replacement of the existing file',
              result.returncode == 0 and printed in result.stderr and replaced in result.stderr
              and 'par and_ = 1' in open(odex).read(), result.stderr)
        forget()
        result = run(['--convert', 'foreign.ode'])
        check('W288: --convert with no answer refuses and writes nothing',
              result.returncode == 1 and not os.path.exists(odex), result.stderr)

check_conversion_modes()

def check_question_policy():
    """W304 (#363): compare persisted data across the five answering policies.
    --check validates only; recording playback and --convert's write never run there.
    Interactive playback writes its private scratch, so cannot ask about a launch-folder output.
    """
    binary = os.path.abspath(args.server)
    modes = ('server', 'stdin', 'yes', 'eof', 'check', 'server yes')
    before_checks = check.checks
    with tempfile.TemporaryDirectory(prefix='xppquestions') as temp:
        root = os.path.realpath(temp)
        for case in ('name', 'conversion replacement', 'outside path', 'recording replacement'):
            for mode in modes:
                folder = os.path.join(root, case.replace(' ', '_') + '_' + mode)
                os.makedirs(folder)
                env = {'XPP_CONFIG_DIR': os.path.join(folder, 'config')}
                model = os.path.join(folder, 'model.ode')
                converted = model[:-4] + '.odex'
                text = "par %s=1\nx'=%s-x\ninit x=0\n@ total=1,dt=.1\ndone\n" % (('and', 'and') if case == 'name' else ('a', 'a'))
                with open(model, 'w') as f: f.write(text)
                keep = b"par a = 7\nx' = a-x\n"
                if case == 'conversion replacement':
                    with open(converted, 'wb') as f: f.write(keep)
                target = model
                if case == 'outside path':
                    table = os.path.join(root, 'outside.tab').replace(os.sep, '/')
                    with open(table, 'w') as f: f.write('5\n0\n4\n0\n1\n4\n9\n16\n')
                    target = os.path.join(folder, 'model.odex')
                    with open(target, 'w') as f: f.write('table w "' + table + '"\n' + "x'=w(1)-x\noptions { integration: { total_time: 1 } }\n")
                saved = os.path.join(folder, 'saved.dat')
                if case == 'recording replacement':
                    os.remove(model)  # Recordings accept native model text only; do not embed the foreign fixture.
                    target = make_recording(binary, args.ode,
                        [{'cmd':'file', 'op':'put', 'name':'saved.dat', 'data':base64.b64encode(b'new').decode()}], folder)
                    with open(saved, 'wb') as f: f.write(b'keep')
                if mode in ('server', 'server yes'):
                    server = Server(binary, target, run=folder, env=env, extra_args=['--yes'] if mode == 'server yes' else [])
                    try:
                        events, event = server.collect(lambda e: e.get('ev') in ('ask', 'idle','done'))
                        expected_kind = 'string' if case == 'name' else 'choice'
                        valid_ask = bool(event) and ((event.get('ev') in ('idle', 'done')) if mode == 'server yes' or case == 'recording replacement' else (event.get('ev') == 'ask' and event.get('kind') == expected_kind))
                        if case != 'recording replacement' and mode == 'server' and valid_ask:
                            server.send(cmd='answer', id=event['id'], **({'value':'and_'} if case == 'name' else {'key':'o' if case == 'outside path' else 'y'}))
                            later, _ = server.collect(is_idle)
                            events += later
                        if case == 'recording replacement':
                            server.send(cmd='recording.play')
                            _, finished = server.collect(lambda e: e.get('ev') == 'state' and e.get('player', {}).get('step') == 1)
                            valid_ask = bool(finished) and file_bytes(server, 'saved.dat') == b'new'
                        ok = valid_ask and (open(saved, 'rb').read() == b'keep' if case == 'recording replacement' else any(e.get('ev') == 'state' for e in events))
                        detail = str(events)[-500:]
                    finally:
                        server.close()
                    if mode == 'server yes' and case != 'recording replacement':
                        ok = ok and '(answered automatically)' in ''.join(server.log) and not any(e.get('ev') == 'ask' for e in events)
                else:
                    flags = ['--check'] if mode == 'check' else ['--silent'] + (['--yes'] if mode == 'yes' else [])
                    answer = ('and_\n' if case == 'name' else 'o\n' if case == 'outside path' else 'y\n') if mode in ('stdin', 'check') else ''
                    result = subprocess.run([binary, target, *flags], cwd=folder, env=dict(os.environ, **env), input=answer,
                                            capture_output=True, text=True, encoding='utf-8', timeout=90 * SLOW)
                    # --check takes a reserved name's suggestion and warns (docs/odex.md "Checking a .ode")
                    expected = 0 if mode in ('stdin', 'yes') or (mode == 'check' and case == 'conversion replacement') else 1 if mode == 'check' and case == 'name' else 2 if mode == 'check' else 1
                    ok = result.returncode == expected
                    if mode == 'check':
                        ok = ok and '(60 s)' not in result.stderr and '(answered automatically)' not in result.stderr and not os.path.exists(os.path.join(folder, 'output.dat'))
                        if case != 'recording replacement':
                            diagnostic = json.loads(result.stdout)
                            ok = ok and isinstance(diagnostic.get('diagnostics'), list)
                        if case == 'name':
                            ok = ok and not os.path.exists(converted) and any(
                                d['id'] == 'reserved-word' and 'and is and_' in d['message'] for d in diagnostic['diagnostics'])
                    elif mode == 'yes':
                        ok = ok and '(answered automatically)' in result.stderr and '(60 s)' not in result.stderr
                    else:
                        ok = ok and '(60 s)' in result.stderr
                    detail = result.stderr + result.stdout[:500]
                if case == 'name' and mode in ('server', 'server yes', 'stdin', 'yes'):
                    ok = ok and 'par and_ = 1' in open(converted).read()
                if case == 'conversion replacement':
                    changed = open(converted, 'rb').read() != keep
                    ok = ok and changed == (mode in ('server', 'server yes', 'stdin', 'yes'))
                if case == 'recording replacement':
                    ok = ok and open(saved, 'rb').read() == (b'new' if mode in ('stdin', 'yes') else b'keep')
                if case == 'outside path':
                    prefs = os.path.join(env['XPP_CONFIG_DIR'], 'preferences.setx')
                    ok = ok and (not os.path.exists(prefs) or table not in open(prefs, encoding='utf-8').read().replace('\\', '/'))
                check('W304: %s / %s' % (case, mode), ok, detail)
        # The converter's write-only replacement has terminal policies; there is no server command for it.
        for mode, flags, answer in [('stdin', [], 'y\n'), ('yes', ['--yes'], ''), ('eof', [], '')]:
            model = os.path.join(root, 'convert.ode')
            converted = model[:-4] + '.odex'
            with open(model, 'w') as f: f.write("x'=-x\ndone\n")
            with open(converted, 'wb') as f: f.write(b'keep')
            result = subprocess.run([binary, '--convert', model, *flags], input=answer, capture_output=True,
                                    text=True, encoding='utf-8', timeout=90 * SLOW)
            check('W304: converter replacement / ' + mode,
                  result.returncode == (1 if mode == 'eof' else 0) and (open(converted, 'rb').read() != b'keep') == (mode != 'eof')
                  and ('(answered automatically)' in result.stderr if mode == 'yes' else '(60 s)' in result.stderr), result.stderr)
    print('W304 question matrix: %d checks (4 questions x 5 modes + 4 server --yes + 3 converter replacements)' % (check.checks - before_checks))

check_question_policy()

def check_keymap():
    """W211, docs/protocol.md "Keymap": keymap.json is one file in the config folder; hello carries its
    effective keys; `keymap` get / set / reset read, replace (checked whole, then written) and remove it;
    a bad file is an error with its place and the defaults shown as marked, never applied in part; a
    second server reads what the first wrote."""
    folder = os.path.join(CONFIG_ROOT, 'nested', 'xppautX')     # the first start or save makes it
    path = os.path.join(folder, 'keymap.json')
    env = {'XPP_CONFIG_DIR': folder}
    same = lambda a, b: os.path.normcase(os.path.abspath(a)) == os.path.normcase(os.path.abspath(b))
    keymap_events = lambda evs: [e for e in evs if e.get('ev') == 'keymap']
    errors = lambda evs: [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
    by_id = lambda km: {c['id']: c for c in km['commands']}
    p, r, snd, col, _ = launch_server(extra_env=env)
    try:
        evs, _ = col(is_idle)
        km = next(e for e in evs if e.get('ev') == 'hello')['keymap']
        cmds = by_id(km)
        check("W211: hello gives the default keymap when there is no file: ok, preset default, nothing pinned, the table's keys, all source default",
              km['ok'] is True and 'preset' not in km and km['pinned'] == [] and same(km['path'], path)
              and cmds['session.open_model']['keys'] == ['Ctrl+O'] and cmds['run.initial']['keys'] == ['Ctrl+R']
              and all(c['source'] == 'default' for c in cmds.values()) and 'file' not in cmds, str(km)[:300])
        check('W211: hello gives the reserved keys and the limits the editor keeps to',
              'Ctrl+W' in km['reserved'] and 'F12' in km['reserved'] and km['limits'] == {'keys': 8, 'key_bytes': 64},
              str(km['limits']))
        # W232: the start made the folder for preferences.setx (the model it was started with); reading the keymap made no file
        check('W211: reading the keymap makes no keymap.json', not os.path.exists(path))
        snd(cmd='keymap.get')
        evs, _ = col(is_idle)
        check('W211: get answers a keymap event with the same effective keymap, then state and idle',
              [e.get('op') for e in keymap_events(evs)] == ['get'] and keymap_events(evs)[0]['keymap'] == km
              and not errors(evs), str(evs)[-300:])

        good = {'pinned': ['session.reload_model', 'tool.calculator'],
                'bindings': {'session.reload_model': ['Ctrl+B', 'Ctrl+K'], 'session.save': [], 'run.continue': ['Alt+I']}}
        snd(cmd='keymap.set', **good)
        evs, _ = col(is_idle)
        ev = keymap_events(evs)
        cmds = by_id(ev[0]['keymap']) if ev else {}
        check('W211: set checks the map, makes the folder, writes the file and answers the effective keymap',
              len(ev) == 1 and ev[0]['op'] == 'set' and ev[0]['keymap']['ok'] is True and not errors(evs)
              and 'preset' not in ev[0]['keymap'] and ev[0]['keymap']['pinned'] == ['session.reload_model', 'tool.calculator']
              and cmds['session.reload_model'] == {'id': 'session.reload_model', 'keys': ['Ctrl+B', 'Ctrl+K'], 'source': 'user'}
              and cmds['session.save'] == {'id': 'session.save', 'keys': [], 'source': 'user'}
              and cmds['session.open_model']['source'] == 'default' and os.path.isfile(path), str(evs)[-300:])
        written = open(path, 'rb').read()
        check('W211: the file holds only the differences, one binding to a line, and no other file is left beside it',
              b'"session.reload_model": ["Ctrl+B", "Ctrl+K"]' in written and b'session.open_model' not in written
              and sorted(os.listdir(folder)) == ['keymap.json', 'preferences.setx'], repr(written[:200]))

        for what, bad, line, value in [
                ('a reserved key', {'bindings': {'session.reload_model': ['Ctrl+W']}}, 1, 'Ctrl+W'),
                ('a key two commands would share', {'bindings': {'tool.calculator': ['Ctrl+O']}}, 1, 'session.open_model'),
                ('a sequence of keys', {'bindings': {'tool.calculator': ['F S']}}, 1, 'F S'),
                ('an unknown command', {'pinned': ['nosuch']}, 1, 'nosuch'),
                ('a removed command pinned', {'pinned': ['retired_help']}, 1, 'retired_help'),
                ('a removed command bound', {'bindings': {'tutorial': ['Ctrl+T']}}, 1, 'tutorial'),
                ('a command that cannot be pinned', {'pinned': ['quit']}, 1, 'quit'),
                ('a malformed key', {'bindings': {'session.reload_model': ['ctrl+r']}}, 1, 'ctrl+r'),
                ]:
            snd(cmd='keymap.set', **bad)
            evs, _ = col(is_idle)
            err = errors(evs)
            check('W211: set refuses %s at its place with its value, and neither answers nor writes' % what,
                  len(err) == 1 and err[0]['file'] == 'the keymap sent' and err[0]['line'] >= 1 and value in err[0]['error']
                  and not keymap_events(evs) and open(path, 'rb').read() == written, str(evs)[-300:])
        snd(cmd='keymap.set', colour=1)
        evs, _ = col(is_idle)
        check('W211: set refuses a setting the keymap does not have, shown once', len(errors(evs)) == 1 and 'colour' in errors(evs)[0]['error'] and not keymap_events(evs), str(evs)[-200:])
        snd(cmd='keymap.set', bindings=[1])
        evs, _ = col(is_idle)
        check('W211: set needs an object of bindings, shown once', len(errors(evs)) == 1 and not keymap_events(evs), str(evs)[-200:])
        snd(cmd='keymap.frob')
        evs, _ = col(is_idle)
        check('W211: an unknown op is an error', len(errors(evs)) == 1 and not keymap_events(evs), str(evs)[-200:])
    finally:
        stop_server(p, r, snd)

    # a second server (the browser beside the window) reads what the first wrote
    p, r, snd, col, _ = launch_server(extra_env=env)
    try:
        evs, _ = col(is_idle)
        km = next(e for e in evs if e.get('ev') == 'hello')['keymap']
        check('W211: another server reads the same file: window and browser never disagree',
              km['ok'] is True and by_id(km)['session.reload_model']['keys'] == ['Ctrl+B', 'Ctrl+K'] and km['pinned'] == ['session.reload_model', 'tool.calculator'], str(km)[:200])

        # a bad file, written by hand: shown as an error at its line, the defaults marked, nothing applied
        with open(path, 'w', newline='\n') as f:
            f.write('{\n  "pinned": ["session.reload_model"],\n  "bindings": {"session.reload_model": ["Ctrl+Q"]}\n}\n')
        snd(cmd='keymap.get')
        evs, _ = col(is_idle)
        ev = keymap_events(evs)
        km = ev[0]['keymap'] if ev else {}
        check('W211: a bad file is an error with its file, line and value; the keymap is the defaults, marked, nothing applied; no message event',
              len(ev) == 1 and km.get('ok') is False and same(km['file'], path) and km['line'] == 3 and 'Ctrl+Q' in km['error']
              and 'Ctrl+Q' in km['source'] and km['pinned'] == [] and 'preset' not in km
              and by_id(km)['session.reload_model']['keys'] == [] and not errors(evs), str(evs)[-400:])
        for what, text, line, value in [('a preset', '{\n  "preset": "xppaut"\n}\n', 2, 'preset'),
                                        ('a chord', '{\n  "bindings": {\n    "tool.calculator": ["F S"]\n  }\n}\n', 3, 'F S')]:
            with open(path, 'w', newline='\n') as f:
                f.write(text)
            snd(cmd='keymap.get')
            evs, _ = col(is_idle)
            ev = keymap_events(evs)
            km = ev[0]['keymap'] if ev else {}
            check('W536: a file with %s is refused with its file, line and value, nothing applied' % what,
                  len(ev) == 1 and km.get('ok') is False and same(km['file'], path) and km['line'] == line and value in km['error']
                  and by_id(km)['tool.calculator']['keys'] == [], str(evs)[-400:])
        snd(cmd='keymap.set', pinned=['tool.calculator'])
        evs, _ = col(is_idle)
        ev = keymap_events(evs)
        check('W211: set replaces a bad file whole', len(ev) == 1 and ev[0]['keymap']['ok'] is True
              and ev[0]['keymap']['pinned'] == ['tool.calculator'], str(evs)[-300:])

        # a file too large is refused as it is read
        with open(path, 'w') as f:
            f.write(' ' * 70000 + '{}')
        snd(cmd='keymap.get')
        evs, _ = col(is_idle)
        ev = keymap_events(evs)
        check('W211: a file over the limit is an error naming the file', len(ev) == 1 and ev[0]['keymap']['ok'] is False
              and same(ev[0]['keymap']['file'], path), str(evs)[-300:])

        snd(cmd='keymap.reset')
        evs, _ = col(is_idle)
        ev = keymap_events(evs)
        check('W211: reset removes the file and answers the defaults',
              len(ev) == 1 and ev[0]['op'] == 'reset' and ev[0]['keymap']['ok'] is True and ev[0]['keymap']['pinned'] == []
              and not os.path.exists(path), str(evs)[-300:])
        snd(cmd='keymap.reset')
        evs, _ = col(is_idle)
        check('W211: reset with no file is fine', len(keymap_events(evs)) == 1 and not errors(evs), str(evs)[-200:])
    finally:
        stop_server(p, r, snd)

check_keymap()

def recent_text(models):
    """W287: a preferences.setx holding these recent models (a quoted string is JSON's)"""
    return 'recent_models: [%s],\n' % ', '.join(json.dumps(m) for m in models)

def recent_models(path):
    """W287: the recent_models of a preferences.setx the program wrote"""
    m = re.search(r'recent_models: (\[.*?\]),\n', open(path, encoding='utf-8').read(), re.S)
    return json.loads(m.group(1)) if m else []

def check_start_screen():
    """W232, docs/protocol.md "Start screen": a program started with no file has a session with no model.
    hello carries `start` (the recent models, newest first, a missing file listed as missing) and no
    state; Open model asks at once, the same ask File > Open model makes (the filter takes .ode, .odex,
    .snapx and .recx); Cancel leaves the start screen (no error, no exit); a command that needs a model is
    refused; Open model of a file, from the screen, is a normal open (new hello with no `start`, the model
    in preferences.setx); a bad preferences.setx is an error with its file and line, no entries listed, and is not
    overwritten by an open; a hostile file is refused whole."""
    folder = os.path.join(CONFIG_ROOT, 'start', 'xppautX')
    path = os.path.join(folder, 'preferences.setx')
    env = {'XPP_CONFIG_DIR': folder}
    errors = lambda evs: [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
    model = os.path.abspath(args.ode)
    same = lambda a, b: os.path.normcase(os.path.abspath(a)) == os.path.normcase(os.path.abspath(b))

    def start_up():
        p, r, snd, col, _ = launch_server(extra_env=env, no_model=True)
        evs, ask = col(lambda e: e.get('ev') == 'ask')
        return p, r, snd, col, evs, ask

    p, r, snd, col, evs, ask = start_up()
    try:
        hello = next((e for e in evs if e.get('ev') == 'hello'), {})
        start = hello.get('start', {})
        check('W232: with no file hello carries the start screen: no recent model, the limit, the file, and no state',
              start.get('recent') == [] and start.get('limit') == 10 and same(start.get('path', ''), path)
              and 'error' not in start and not [e for e in evs if e.get('ev') in ('state', 'window')] and hello.get('file') == '', str(evs)[:300])
        check('W232: Open model asks at once, as File > Open model does: one dialog, .ode .odex .snapx .recx',
              ask and ask['kind'] == 'file' and ask['title'] == 'Open model' and ask['mode'] == 'read'
              and ask['wild'].split() == ['*.ode', '*.odex', '*.snapx', '*.recx', '*.anix'], str(ask))
        snd(cmd='answer', id=ask['id'], ok=0)
        evs, _ = col(is_idle)
        check('W232: Cancel at the start leaves the start screen: no error, no exit', not errors(evs) and p.poll() is None, str(evs)[-300:])
        for cmd in ({'cmd': 'session.get_state'}, {'cmd': 'run.initial'}):
            snd(**cmd)
            evs, _ = col(is_idle)
            err = errors(evs)
            check('W232: %s needs a model: refused once, ending in idle' % cmd['cmd'],
                  len(err) == 1 and 'No model is open' in err[0]['error'] and not [e for e in evs if e.get('ev') == 'state'], str(evs)[-300:])
        snd(cmd='events.subscribe', events=['series'])
        evs, _ = col(is_idle)
        check('W232: data is accepted with nothing to send', not errors(evs), str(evs)[-200:])
        snd(cmd='session.open_model')
        evs, ask = col(lambda e: e.get('ev') == 'ask')
        check('W232: File > Open model by its id is the same ask', ask and ask['title'] == 'Open model', str(evs)[-200:])
        snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        check('W232: opening nothing wrote nothing', not os.path.exists(path), '')
        # a model opened from the screen: no question about saving, a new hello with no start
        snd(cmd='session.open_model', file=model)
        evs, _ = col(is_idle)
        hello = next((e for e in evs if e.get('ev') == 'hello'), {})
        check('W232: Open model of a file loads it: a hello with no start screen, its state, no question, no error',
              hello and 'start' not in hello and last_state(evs) and not [e for e in evs if e.get('ev') == 'ask'] and not errors(evs), str(evs)[-300:])
        check('W232: the opened model is the first of recent_models in preferences.setx, an absolute path',
              same(recent_models(path)[0], model), '')
    finally:
        stop_server(p, r, snd)

    # a second start lists it; a file that was removed is listed as missing, not dropped
    gone = os.path.join(CONFIG_ROOT, 'gone.odex')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(recent_text([model, gone]))
    p, r, snd, col, evs, ask = start_up()
    try:
        recent = next(e for e in evs if e.get('ev') == 'hello')['start']['recent']
        check('W232: the next start lists the models, a missing file as missing and kept',
              len(recent) == 2 and same(recent[0]['path'], model) and not recent[0]['missing']
              and same(recent[1]['path'], gone) and recent[1]['missing'], str(recent))
        snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        snd(cmd='session.open_model', file=gone)
        evs, _ = col(is_idle)
        err = errors(evs)
        check('W232: opening a missing recent model is an ordinary open error naming it, the start screen stays',
              len(err) == 1 and 'gone.odex' in err[0]['error'] + err[0].get('file', '') and p.poll() is None, str(evs)[-300:])
        check('W232: and it is not made the newest', same(recent_models(path)[0], model), '')
    finally:
        stop_server(p, r, snd)

    # a bad preferences.setx: an error at its line, nothing listed, never overwritten
    bad = 'recent_models: [%s,\n  "relative.odex"],\n' % json.dumps(model)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(bad)
    p, r, snd, col, evs, ask = start_up()
    try:
        start = next(e for e in evs if e.get('ev') == 'hello')['start']
        check('W232: a bad preferences.setx is an error with its file, line and value, and nothing is listed',
              start['recent'] == [] and same(start['file'], path) and start['line'] == 2
              and 'relative.odex' in start['error'] and 'relative.odex' in start['source'], str(start))
        snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        snd(cmd='session.open_model', file=model)
        evs, _ = col(is_idle)
        err = errors(evs)
        check('W232: opening still works, the bad file shown once and left as it was',
              len(err) == 1 and 'relative.odex' in err[0]['error'] and any(e.get('ev') == 'hello' and 'start' not in e for e in evs)
              and open(path, encoding='utf-8', newline='').read() == bad, str(evs)[-300:])
    finally:
        stop_server(p, r, snd)

    # a hostile file: too long, and too many lines
    for what, text in (('a file over the limit', 'x' * 300000), ('more entries than are kept', recent_text([model + str(i) for i in range(11)]))):
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        p, r, snd, col, evs, ask = start_up()
        try:
            start = next(e for e in evs if e.get('ev') == 'hello')['start']
            check('W232: %s is refused whole' % what, start['recent'] == [] and start.get('error'), str(start)[:200])
        finally:
            stop_server(p, r, snd)

    # --silent has nothing to open: an error and exit 1, as before
    run_dir = tempfile.mkdtemp(prefix='xppnofile')
    try:
        q = subprocess.run([os.path.abspath(args.server), '--silent'], cwd=run_dir, capture_output=True, text=True, timeout=30 * SLOW)
        check('W232: --silent with no file is an error and exit 1', q.returncode == 1 and 'no model file was named' in q.stderr, repr(q.stderr[-200:]))
    finally:
        shutil.rmtree(run_dir, ignore_errors=True)

check_start_screen()

def check_start_examples():
    """W233, docs/protocol.md "Start screen": the bundled examples are the .odex files of examples/ beside
    the executable, listed in hello.start.examples (none, with the reason, when there is no such folder); the
    page opens one by its name only: a path, `..` or a name that is not listed is refused, and an example
    is opened as a copy in the config folder's examples/, asking before it replaces one that is there."""
    root = tempfile.mkdtemp(prefix='xppexamples')
    config = os.path.join(root, 'config')
    env = {'XPP_CONFIG_DIR': config}
    same = lambda a, b: os.path.normcase(os.path.abspath(a)) == os.path.normcase(os.path.abspath(b))
    errors = lambda evs: [e for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]
    try:
        # no examples/ beside the executable: none, and the reason is there to be shown
        p, r, snd, col, _ = launch_server(extra_env=env, no_model=True)
        try:
            evs, _ = col(lambda e: e.get('ev') == 'ask')
            ex = next(e for e in evs if e.get('ev') == 'hello')['start'].get('examples', {})
            check('W233: with no examples folder beside the program, none are listed and the reason is given',
                  ex.get('names') == [] and ex.get('reason') and ex.get('folder', '').endswith('examples'), str(ex))
        finally:
            stop_server(p, r, snd)

        # a copy of the program with examples/ beside it
        bin_dir = os.path.join(root, 'bin')
        os.makedirs(os.path.join(bin_dir, 'examples', 'sub'))
        exe = os.path.join(bin_dir, os.path.basename(args.server))
        shutil.copy(args.server, exe)
        copy_model(args.ode, os.path.join(bin_dir, 'examples', 'lecar.odex'))
        copy_model(args.ode, os.path.join(bin_dir, 'examples', 'sub', 'deep.odex'))
        with open(os.path.join(bin_dir, 'examples', 'notes.txt'), 'w') as f:
            f.write('not a model\n')
        copied = os.path.join(config, 'examples', 'lecar.odex')
        p, r, snd, col, _ = launch_server(extra_env=env, no_model=True, server=exe)
        try:
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            ex = next(e for e in evs if e.get('ev') == 'hello')['start'].get('examples', {})
            check('W233: the .odex files of examples/ beside the program are listed, sorted, no folder or other file',
                  ex.get('names') == ['lecar.odex'] and 'reason' not in ex, str(ex))
            snd(cmd='answer', id=ask['id'], ok=0)
            col(is_idle)
            for name in ('../lecar.odex', '..', 'sub/deep.odex', os.path.join(bin_dir, 'examples', 'lecar.odex'), 'notes.txt', 'nothere.odex', ''):
                snd(cmd='session.open_model', example=name)
                evs, _ = col(is_idle)
                err = errors(evs)
                check('W233: the example %r is refused (not listed), nothing opened or written' % name,
                      len(err) == 1 and 'not one of the bundled examples' in err[0]['error'] and not os.path.exists(config)
                      and not any(e.get('ev') == 'hello' for e in evs), str(evs)[-300:])
            snd(cmd='session.open_model', example='lecar.odex')
            evs, _ = col(is_idle)
            hello = next((e for e in evs if e.get('ev') == 'hello'), {})
            with open(args.ode, 'rb') as a, open(copied, 'rb') as b:
                same_bytes = a.read() == b.read()
            check('W233: opening an example copies it into the config folder and opens the copy: a hello with no start, a state, no error, no question',
                  hello and 'start' not in hello and last_state(evs) and not errors(evs) and not [e for e in evs if e.get('ev') == 'ask']
                  and same_bytes and same(recent_models(os.path.join(config, 'preferences.setx'))[0], copied),
                  str(evs)[-300:])
            check('W233: and says where the copy is', any(same(e['bottom'].split(' is copied to ')[1].split(';')[0], copied) for e in evs if e.get('ev') in ('message', 'done') and ' is copied to ' in e.get('bottom', '')), str(evs)[-300:])
            # a changed copy is asked about before it is replaced; No keeps it as it is
            with open(copied, 'a') as f:
                f.write('# mine\n')
            changed = open(copied, 'rb').read()
            snd(cmd='session.open_model', example='lecar.odex')
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            check('W233: a copy that is there is asked about: Replace?', ask and ask.get('question', '').endswith(' exists. Replace it?'), str(ask))
            snd(cmd='answer', id=ask['id'], key='n')
            evs, _ = col(is_idle)
            check('W233: No keeps the copy as it was and opens nothing', open(copied, 'rb').read() == changed and not [e for e in evs if e.get('ev') == 'ask']
                  and not any(e.get('ev') == 'hello' for e in evs), str(evs)[-300:])
            snd(cmd='session.open_model', example='lecar.odex')
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            snd(cmd='answer', id=ask['id'], key='y')
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            check('W233: Yes replaces the copy with the example, then Open model asks its usual question about this session',
                  open(copied, 'rb').read() != changed and ask and ask.get('kind') == 'choice', str(ask))
            snd(cmd='answer', id=ask['id'], key='d')
            evs, _ = col(is_idle)
            check('W233: and the example opens', any(e.get('ev') == 'hello' for e in evs) and not errors(evs), str(evs)[-300:])
        finally:
            stop_server(p, r, snd)
    finally:
        shutil.rmtree(root, ignore_errors=True)


check_start_examples()

def check_auto_threads():
    """W267, docs/protocol.md "AUTO's settings as data": the thread count is a program preference: the
    autosettings event carries it ("auto" or the count, what automatic gives, the most), `auto` `set`
    `threads` saves it in preferences.setx (in a config folder of its own) and a server started later reads
    it; a count outside 1 to threads_max, or a word that is not "auto", is refused whole, nothing saved."""
    root = tempfile.mkdtemp(prefix='xppthreads')
    config = os.path.join(root, 'config')
    env = {'XPP_CONFIG_DIR': config}
    prefs = os.path.join(config, 'preferences.setx')
    errors = lambda evs: [e['error'] for e in evs if e.get('ev') in ('message', 'done') and e.get('error')]

    def threads_of(snd, col):
        snd(cmd='events.subscribe', events=['autosettings'])
        return settings_of(col(is_idle)[0])

    def text():
        with open(prefs) as f:
            return f.read()
    try:
        p, r, snd, col, _ = launch_server(extra_env=env)
        try:
            col(is_idle)
            st = threads_of(snd, col)
            most = st['threads_max']
            check('W267: the event gives threads "auto" at first, what automatic gives (1 to 4) and the most (the logical processors)',
                  st['threads'] == 'auto' and 1 <= st['threads_auto'] <= min(4, most) and most >= 1, str(st)[-200:])
            snd(cmd='auto.set_threads', threads=most)
            evs, _ = col(is_idle)
            st = settings_of(evs)
            check('W267: set threads (the most) shows in the event and is saved in preferences.setx',
                  not errors(evs) and st and st['threads'] == most and 'auto_threads: %d' % most in text(), str(evs)[-300:])
            before = text()
            for bad in (0, most + 1, -1, 1.5, 'many', 'Auto', [2]):
                snd(cmd='auto.set_threads', threads=bad)
                evs, _ = col(is_idle)
                err = errors(evs)
                check('W267: set threads %r is refused naming the rule, the file is as it was' % (bad,),
                      len(err) == 1 and 'threads' in err[0] and text() == before, str(evs)[-300:])
            check('W267: and the shown count is the one before', threads_of(snd, col)['threads'] == most)
            snd(cmd='auto.set_threads', threads=1)
            check('W267: set threads 1', col(is_idle) and 'auto_threads: 1' in text())
        finally:
            stop_server(p, r, snd)
        # a later start reads it, and "auto" puts the word back
        p, r, snd, col, _ = launch_server(extra_env=env)
        try:
            col(is_idle)
            check('W267: a later server starts with the saved count', threads_of(snd, col)['threads'] == 1)
            snd(cmd='auto.set_threads', automatic=True)
            evs, _ = col(is_idle)
            check('W267: set threads "auto" is saved as the word and shown', not errors(evs) and 'auto_threads: auto' in text()
                  and settings_of(evs)['threads'] == 'auto', str(evs)[-300:])
        finally:
            stop_server(p, r, snd)
        # a file with a bad count is an error at its line on start and is never overwritten
        bad_text = 'recent_models: [],\nauto_threads: 0,\n'
        with open(prefs, 'w') as f:
            f.write(bad_text)
        p, r, snd, col, _ = launch_server(extra_env=env)
        try:
            evs, _ = col(is_idle)
            snd(cmd='auto.set_threads', threads=2)
            evs2, _ = col(is_idle)
            err = errors(evs2)
            check('W267: a bad count in the file is refused with its file and line, the set is refused with it and the file is left as it is',
                  len(err) == 1 and 'preferences.setx:2' in err[0] and text() == bad_text, str(err) + str(evs)[-200:])
        finally:
            stop_server(p, r, snd)
    finally:
        shutil.rmtree(root, ignore_errors=True)


def check_outside_paths():
    """W286, docs/protocol.md "Paths outside the model's folder": a path a model names outside its folder is
    a question (`ask` kind `choice`, keys `aor`): Allow always is kept in the preferences for this model file and
    this exact path, Allow once is for this load, Refuse (and --silent with no answer) stops the load at the
    model's line. --yes allows once, printed."""
    binary = os.path.abspath(args.server)
    # its real path: macOS's temp folder is under /var, a link to /private/var, and a link on the way is refused
    root = os.path.realpath(tempfile.mkdtemp(prefix='xppoutside'))
    config = os.path.join(root, 'config')
    env = {'XPP_CONFIG_DIR': config}
    prefs = os.path.join(config, 'preferences.setx')
    table = os.path.join(root, 'w.tab').replace(os.sep, '/')
    table2 = os.path.join(root, 'w2.tab').replace(os.sep, '/')
    for t in (table, table2):
        with open(t, 'w') as f:
            f.write(chr(10).join(['5', '0', '4', '0', '1', '4', '9', '16', '']))
    proj = os.path.join(root, 'proj')
    os.makedirs(proj)
    def model_with(path, name):
        full = os.path.join(proj, name)
        with open(full, 'w') as f:
            f.write(chr(10).join(['table w "' + path + '"', 'par a=1.5', "x'=w(a)-x", 'init x=0',
                                  'options { integration: { total_time: 4 } }', '']))
        return full
    # the preferences keep the resolved path, on Windows with backslashes (written \\): read them as / to
    # compare with the path as the model names it
    kept = lambda: open(prefs, encoding='utf-8').read().replace('\\\\', '/') if os.path.exists(prefs) else ''
    is_hello = lambda e: e.get('ev') == 'hello'
    try:
        model = model_with(table, 'tab.odex')
        def start(path):
            return Server(args.server, path, env=env, run=proj, verbose=args.v)
        failed = lambda e: e.get('ev') == 'error' or (e.get('ev') in ('message', 'done') and e.get('error'))
        # no answer: the question, then the load stops at the model's line
        s = start(model)
        try:
            evs, ask = s.collect(lambda e: e.get('ev') == 'ask')
            check('W286: a table outside the model folder asks one choice: Allow always / Allow once / Refuse, naming the model, its line and the path',
                  ask and ask.get('kind') == 'choice' and ask.get('choices') == ['Allow always', 'Allow once', 'Refuse']
                  and ask.get('keys') == 'aor' and table in ask.get('question', '') and 'line 1 of' in ask.get('question', '')
                  and 'tab.odex' in ask.get('question', ''), str(ask))
            s.send(cmd='answer', id=ask['id'], key='r')
            evs, _ = s.collect(failed)
            errs = [e for e in evs if failed(e)]
            check('W286: Refuse stops the load with the refusal at the table line, nothing kept',
                  errs and 'refused' in str(errs[-1]) and 'tab.odex' in str(errs[-1]) and not any(e.get('ev') == 'state' for e in evs)
                  and table not in kept(), str(evs)[-400:])
        finally:
            s.close()
        # Allow once: loads, keeps nothing, asks again next time
        s = start(model)
        try:
            evs, ask = s.collect(lambda e: e.get('ev') == 'ask')
            s.send(cmd='answer', id=ask['id'], key='o')
            evs, _ = s.collect(is_idle)
            check('W286: Allow once loads the model and keeps nothing', any(e.get('ev') == 'state' for e in evs) and not [e for e in evs if failed(e)]
                  and table not in kept(), str(evs)[-300:])
        finally:
            s.close()
        # Allow always: loads, kept for this model file and this path
        s = start(model)
        try:
            evs, ask = s.collect(lambda e: e.get('ev') == 'ask')
            check('W286: Allow once kept nothing, so the same model asks again', ask and ask.get('kind') == 'choice', str(ask))
            s.send(cmd='answer', id=ask['id'], key='a')
            evs, _ = s.collect(is_idle)
            check('W286: Allow always loads the model and keeps the model file and the path in the preferences',
                  any(e.get('ev') == 'state' for e in evs) and table in kept(), str(evs)[-300:] + kept())
        finally:
            s.close()
        # the remembered pair is not asked again
        s = start(model)
        try:
            evs, _ = s.collect(is_idle)
            check('W286: a remembered approval is not asked again', not any(e.get('ev') == 'ask' for e in evs) and any(e.get('ev') == 'state' for e in evs), str(evs)[-300:])
        finally:
            s.close()
        # a changed path asks again
        model2 = model_with(table2, 'tab.odex')
        s = start(model2)
        try:
            evs, ask = s.collect(lambda e: e.get('ev') == 'ask')
            check('W286: a changed path asks again', ask and ask.get('kind') == 'choice' and table2 in ask.get('question', ''), str(ask))
            s.send(cmd='answer', id=ask['id'], key='r')
            s.collect(failed)
        finally:
            s.close()
        # --silent: the terminal's question; no answer refuses at the model's line, --yes allows once and says so
        def silent(flags, answer=''):
            return subprocess.run([binary, 'tab.odex', '--silent'] + flags, cwd=proj, input=answer,
                                  capture_output=True, text=True, encoding='utf-8', timeout=60 * SLOW, env=dict(os.environ, **env))
        with open(prefs, 'w', encoding='utf-8') as f:
            f.write('')
        result = silent([])
        check('W286: --silent with no answer refuses and quits with the error at the model\'s line',
              result.returncode == 1 and '(60 s)' in result.stderr and 'tab.odex:1' in result.stderr and 'refused' in result.stderr, result.stderr)
        result = silent([], 'o\n')
        check('W286: --silent takes the answer from the terminal', result.returncode == 0 and '(60 s)' in result.stderr, result.stderr)
        result = silent(['--yes'])
        check('W286: --silent --yes allows once and prints the question and what it took',
              result.returncode == 0 and '(60 s)' not in result.stderr and ' -> Allow once (answered automatically)' in result.stderr
              and table2 not in kept(), result.stderr)

        # W297: conversion approvals survive into the native load; no second ask.
        outside = os.path.join(root, 'output.dat').replace(os.sep, '/')
        ode = os.path.join(proj, 'once.ode')
        with open(ode, 'w') as f:
            f.write("@ output=" + outside + "\nx'=-x\ninit x=1\n@ total=1\ndone\n")
        for key in ('o', 'a'):
            s = start(ode)
            try:
                evs, ask = s.collect(lambda e: e.get('ev') == 'ask')
                check('W297: .ode outside output asks approval before loading (' + key + ')',
                      ask and outside in ask.get('question', ''), str(ask))
                s.send(cmd='answer', id=ask['id'], key=key)
                evs, end = s.collect(lambda e: is_idle(e) or e.get('ev') == 'ask' or failed(e))
                check('W297: .ode outside output is asked exactly once (' + key + ')',
                      end and is_idle(end) and not any(e.get('ev') == 'ask' or failed(e) for e in evs), str(evs)[-400:])
            finally:
                s.close()
        for path in (ode, ode.replace('.ode', '.odex')):
            s = start(path)
            try:
                evs, end = s.collect(lambda e: is_idle(e) or e.get('ev') == 'ask' or failed(e))
                check('W297: Allow always applies to ' + os.path.basename(path),
                      end and is_idle(end) and not any(e.get('ev') == 'ask' for e in evs), str(evs)[-400:])
            finally:
                s.close()
        with open(prefs, 'w', encoding='utf-8') as f:
            f.write('')
        result = subprocess.run([binary, ode, '--silent', '--yes'], cwd=proj, input='',
                                capture_output=True, text=True, timeout=60 * SLOW, env=dict(os.environ, **env))
        check('W297: --silent --yes .ode prints exactly one outside approval',
              result.returncode == 0 and result.stderr.count(' -> Allow once (answered automatically)') == 1,
              result.stderr)

        # A table must actually be read by the converter, then read again by the native load.
        tableode = os.path.join(proj, 'once-table.ode')
        with open(tableode, 'w') as f:
            f.write('table w ' + table + "\nx'=w(1)-x\ninit x=0\n@ total=1\ndone\n")
        s = start(tableode)
        try:
            evs, ask = s.collect(lambda e: e.get('ev') == 'ask')
            check('W297: converter asks before reading an outside table', ask and table in ask.get('question', ''), str(ask))
            s.send(cmd='answer', id=ask['id'], key='o')
            evs, end = s.collect(lambda e: is_idle(e) or e.get('ev') == 'ask' or failed(e))
            check('W297: outside table approval transfers to native load',
                  end and is_idle(end) and not any(e.get('ev') == 'ask' or failed(e) for e in evs), str(evs)[-400:])
        finally:
            s.close()

        # Swap the final log name while its approval is pending, after resolve_path.
        log = os.path.join(root, 'swapped.log').replace(os.sep, '/')
        target = os.path.join(root, 'target.txt')
        with open(target, 'w') as f:
            f.write('keep target')
        logmodel = os.path.join(proj, 'log.odex')
        with open(logmodel, 'w') as f:
            f.write("x'=-x\ninit x=1\noptions { output: { logfile: \"" + log + "\" } }\n")
        s = start(logmodel)
        try:
            evs, ask = s.collect(lambda e: e.get('ev') == 'ask')
            check('W297: log approval precedes stream open', ask and log in ask.get('question', ''), str(ask))
            try:
                os.symlink(target, log)
            except OSError as error:
                if os.name != 'nt':
                    raise
                print('SKIP W297 Windows log symlink: ' + str(error))
                s.send(cmd='answer', id=ask['id'], key='r')
                s.collect(failed)
            else:
                s.send(cmd='answer', id=ask['id'], key='o')
                evs, error = s.collect(failed)
                check('W297: swapped log link is refused with file and source line',
                      error and 'swapped.log' in str(error) and 'log.odex' in str(error)
                      and error.get('line') == 3 and 'logfile:' in error.get('source', '')
                      and ('refused' in str(error) or 'symbolic link' in str(error)), str(evs)[-600:])
                check('W297: swapped log target is untouched', open(target).read() == 'keep target')
                result = subprocess.run([binary, 'tab.odex', '--silent', '--logfile', log], cwd=proj, input='',
                                        capture_output=True, text=True, timeout=60 * SLOW, env=dict(os.environ, **env))
                check('W297: command-line logfile refuses link and names file',
                      result.returncode != 0 and 'swapped.log' in result.stderr and open(target).read() == 'keep target', result.stderr)
        finally:
            s.close()

    finally:
        shutil.rmtree(root, ignore_errors=True)


check_auto_threads()
check_outside_paths()

def check_create_folders():
    """W303: all writers use the same parent-folder decision and OS denial."""
    binary = os.path.abspath(args.server)
    before = check.checks
    with tempfile.TemporaryDirectory(prefix='xpp-create-') as temporary:
        root = os.path.realpath(temporary)
        model = os.path.join(root, 'model.odex')
        copy_model(args.ode, model)
        env = {'XPP_CONFIG_DIR': os.path.join(root, 'config')}
        for option in ('--outfile', '--logfile'):
            for mode, flags, answer in [('yes', ['--yes'], ''), ('terminal', [], 'y\n'), ('eof', [], '')]:
                folder = os.path.join(root, option[2:] + '-' + mode)
                destination = os.path.join(folder, 'result.dat')
                result = subprocess.run([binary, model, '--silent', option, destination, *flags],
                                        cwd=root, env=dict(os.environ, **env), input=answer,
                                        capture_output=True, text=True, encoding='utf-8', timeout=90 * SLOW)
                transcript = result.stderr
                if option == '--logfile' and os.path.isfile(destination):
                    with open(destination, encoding='utf-8') as log: transcript += log.read()
                expected = mode in ('yes', 'terminal')
                check('W303: %s / %s' % (option, mode),
                      os.path.isfile(destination) == expected and os.path.isdir(folder) == expected
                      and (result.returncode == 0 if mode != 'eof' else result.returncode != 0)
                      and ('Create the folder' not in result.stderr if mode == 'check' else 'Create the folder' in transcript)
                      and ('administrator rights' not in result.stderr), result.stderr)
        for mode, flags, answer in [('yes', ['--yes'], ''), ('terminal', [], 'y\n'), ('eof', [], ''), ('check', ['--check', '--yes'], '')]:
            output = 'model-output-' + mode + '/data.dat'
            origin = os.path.join(root, 'output.odex')
            with open(origin, 'w', encoding='utf-8') as f:
                f.write("x' = -x\noptions { output: { output: \"%s\" }, integration: { total_time: 1 } }\n" % output)
            command = [binary, '--check', origin, '--yes'] if mode == 'check' else [binary, origin, '--silent', *flags]
            result = subprocess.run(command, cwd=root, env=dict(os.environ, **env),
                                    input=answer, capture_output=True, text=True, encoding='utf-8', timeout=90 * SLOW)
            check('W303: model output retains its source / ' + mode,
                  os.path.isfile(os.path.join(root, output)) == (mode in ('yes', 'terminal'))
                  and ('output.odex:2' in result.stderr if mode == 'eof' else result.returncode == 0)
                  and ('Create the folder' not in result.stderr if mode == 'check' else result.stderr.count('Create the folder') == 1), result.stderr)
        inspection = os.path.join(root, 'inspect-log.odex')
        with open(inspection, 'w', encoding='utf-8') as f:
            f.write("x' = -x\noptions { output: { logfile: \"inspect-missing/log.txt\" } }\n")
        result = subprocess.run([binary, '--check', inspection, '--yes'], cwd=root, env=dict(os.environ, **env),
                                input='', capture_output=True, text=True, encoding='utf-8', timeout=90 * SLOW)
        check('W303: inspection neither asks nor creates a logfile folder', result.returncode == 0
              and 'Create the folder' not in result.stderr and not os.path.exists(os.path.join(root, 'inspect-missing')), result.stderr)
        shared = os.path.join(root, 'shared.odex')
        with open(shared, 'w', encoding='utf-8') as f:
            f.write("x' = -x\noptions { output: { logfile: \"shared/run.log\", output: \"shared/data.dat\" }, integration: { total_time: 1 } }\n")
        result = subprocess.run([binary, shared, '--silent', '--yes'], cwd=root, env=dict(os.environ, **env),
                                input='', capture_output=True, text=True, encoding='utf-8', timeout=90 * SLOW)
        with open(os.path.join(root, 'shared', 'run.log'), encoding='utf-8') as log: transcript = result.stderr + log.read()
        check('W303: a folder created for a log is not asked again for data', result.returncode == 0
              and transcript.count('Create the folder') == 1
              and os.path.isfile(os.path.join(root, 'shared', 'run.log'))
              and os.path.isfile(os.path.join(root, 'shared', 'data.dat')), result.stderr)
        # A model's logfile is prepared at its source line. An outside path
        # must have its own earlier approval; two independent questions.
        project = os.path.join(root, 'project')
        os.mkdir(project)
        source = os.path.join(project, 'log.odex')
        for answer in ('y', 'n'):
            destination = '../outside-' + answer + '/run.log'
            with open(source, 'w', encoding='utf-8') as f:
                f.write("x' = -x\noptions { output: { logfile: \"%s\" } }\n" % destination)
            server = Server(binary, source, env=env, run=project)
            try:
                _, outside = server.collect(lambda e: e.get('ev') in ('ask', 'error'))
                check('W303: outside approval precedes folder creation / ' + answer,
                      outside and outside.get('keys') == 'aor', str(outside))
                if outside and outside.get('ev') == 'ask':
                    server.send(cmd='answer', id=outside['id'], key='o')
                _, ask = server.collect(lambda e: e.get('ev') in ('ask', 'error'))
                check('W303: server asks to create the folder / ' + answer,
                      ask and 'Create the folder' in ask.get('question', '') and ask.get('keys') == 'yn', str(ask))
                if ask and ask.get('ev') == 'ask':
                    server.send(cmd='answer', id=ask['id'], key=answer)
                events, _ = server.collect(lambda e: e.get('ev') in ('idle', 'error'))
                errors = [e for e in events if e.get('ev') == 'error']
                check('W303: server creation answer / ' + answer,
                      os.path.isfile(os.path.join(root, 'outside-' + answer, 'run.log')) == (answer == 'y')
                      and (not errors if answer == 'y' else any(e.get('line') == 2 and 'folder' in e.get('error', '') for e in errors)), str(events[-3:]))
            finally:
                server.close()
        denied = os.path.join(root, 'denied')
        os.mkdir(denied)
        sid = None
        try:
            if os.name == 'nt':
                sid = 'S-1-1-0'  # Everyone: the ACL convention used by the silent output checks.
                subprocess.run(['icacls', denied, '/deny', '*' + sid + ':(OI)(CI)(W)'], check=True, capture_output=True)
            else:
                os.chmod(denied, 0o500)
            for nested in (False, True):
                destination = os.path.join(denied, 'child', 'result.dat') if nested else os.path.join(denied, 'result.dat')
                result = subprocess.run([binary, model, '--silent', '--yes', '--outfile', destination], cwd=root,
                                        env=dict(os.environ, **env), input='', capture_output=True, text=True,
                                        encoding='utf-8', timeout=90 * SLOW)
                check('W303: OS permission denial / ' + ('create' if nested else 'write'),
                      result.returncode != 0 and 'Permission denied:' in result.stderr
                      and 'administrator rights' in result.stderr and 'choose another place' in result.stderr
                      and 'denied' in result.stderr and not os.path.exists(destination), result.stderr)
        finally:
            if sid:
                subprocess.run(['icacls', denied, '/remove:d', '*' + sid], check=True, capture_output=True)
            else:
                os.chmod(denied, 0o700)
    print('W303 folder matrix: %d checks' % (check.checks - before))


check_create_folders()

def check_range_files():
    """W349: range-owned files, persisted settings and foreign option import."""
    with tempfile.TemporaryDirectory(prefix='w349') as folder:
        model = os.path.join(folder, 'range.odex')
        with open(model, 'w') as f:
            f.write("par a=1\nx'=a\ninit x=0\noptions { integration: { dt: 0.1, total_time: 0.2 }, "
                    "sweep: { rangeover: a, rangestep: 2, rangelow: 1, rangehigh: 3, "
                    "rangereset: true, rangeoldic: true, rangefiles: true } }\n")
        with tempfile.TemporaryDirectory(prefix='w349run') as run:
            s = Server(args.server, model, run=run)
            try:
                s.collect(is_idle)
                s.send(cmd='session.save_as', file='before', data=False)
                s.collect(is_idle)
                with zipfile.ZipFile(os.path.join(run, 'before.snapx')) as z:
                    settings = z.read('model.setx').decode()
                check('W349: session saves rangefiles by name', 'rangefiles: true' in settings, settings[-600:])
                for enabled, reset, expected in (('Y', 'Y', 3), ('N', 'Y', 0), ('Y', 'N', 0)):
                    s.send(cmd='run.range', variable='a', steps=2, start=1, end=3, reset_storage=reset == 'Y', use_old_ic=True,
                           cycle_color=False, movie=False, write_each_run_to_file=enabled == 'Y')
                    s.collect(is_idle)
                    paths = sorted(glob.glob(os.path.join(run, 'range.dat.*')))
                    rows = [[list(map(float, line.split())) for line in open(path)] for path in paths]
                    check('W349: server reset=%s files=%s writes %d runs' % (reset, enabled, expected),
                          len(paths) == expected and (not expected or all(len(r) == 3 and
                          abs(r[-1][0] - .2) < 1e-6 and abs(r[-1][1] - .2 * (i + 1)) < 1e-6
                          for i, r in enumerate(rows))), str(rows))
                    for path in paths:
                        os.remove(path)
                s.send(cmd='run.double_range', variable1='a', start1=1, end1=3, variable2='x', start2=0, end2=2, steps1=1, steps2=1,
                       reset_storage=True, use_old_ic=True, cycle_color=False, movie=False, vary='independently', write_each_run_to_file=True)
                s.collect(is_idle)
                paths = sorted(glob.glob(os.path.join(run, 'range.dat.*')))
                rows = [[list(map(float, line.split())) for line in open(path)] for path in paths]
                check('W349: double range gives every run a separate file', len(rows) == 4
                      and all(len(r) == 3 and abs(r[-1][1] - expected) < 1e-6
                              for r, expected in zip(rows, [.2, .6, 2.2, 2.6])), str(rows))
                for path in paths:
                    os.remove(path)
                s.send(cmd='session.open_model', file='before.snapx')
                s.answer_asks(is_idle, {'choice': lambda e: {'key': 'd'}})
                s.send(cmd='run.range')
                s.collect(is_idle)
                paths = sorted(glob.glob(os.path.join(run, 'range.dat.*')))
                check('W349: session load restores rangefiles: a range with no arguments writes its runs', len(paths) == 3, str(paths))
                for path in paths:
                    os.remove(path)
                s.send(cmd='set.save_defaults', scope='all')
                s.answer_asks(is_idle, {'choice': lambda e: {'key': 'y'}})
                saved = open(os.path.join(run, 'range.odex')).read()
                check('W349: saving .odex writes the named range setting', 'rangefiles: true' in saved, saved)
                s.send(cmd='session.open_model', file='range.odex')
                s.answer_asks(is_idle, {'choice': lambda e: {'key': 'd'}})
                s.send(cmd='session.save_as', file='odex-roundtrip', data=False)
                s.collect(is_idle)
                with zipfile.ZipFile(os.path.join(run, 'odex-roundtrip.snapx')) as z:
                    text = z.read('model.setx').decode()
                check('W349: saved .odex reloads rangefiles', 'rangefiles: true' in text, text[-600:])
            finally:
                s.close()
        for enabled in ('Y', 'N'):
            with tempfile.TemporaryDirectory(prefix='w349rec') as run:
                lines = [dict(cmd='run.range', variable='a', steps=2, start=1, end=3, reset_storage=True, use_old_ic=True,
                              cycle_color=False, movie=False, write_each_run_to_file=enabled == 'Y')]
                rec = make_recording(args.server, model, lines, run)
                result = subprocess.run([os.path.abspath(args.server), rec, '--silent', '--yes'],
                                        cwd=run, capture_output=True, text=True, timeout=60 * SLOW)
                paths = sorted(glob.glob(os.path.join(run, 'range.dat.*')))
                rows = [[list(map(float, line.split())) for line in open(path)] for path in paths]
                check('W349: silent recording files=' + enabled,
                      result.returncode == 0 and len(paths) == (3 if enabled == 'Y' else 0)
                      and all(len(r) == 3 and abs(r[-1][0] - .2) < 1e-6
                              and abs(r[-1][1] - .2 * (i + 1)) < 1e-6 for i, r in enumerate(rows)),
                      result.stderr + str(rows))
        foreign = os.path.join(folder, 'legacy.ode')
        with open(foreign, 'w') as f:
            f.write("par a=1\nx'=a\n@ range=1,rangeover=a,rangestep=2,rangelow=1,rangehigh=3,total=.2,dt=.1\ndone\n")
        imported = subprocess.run([os.path.abspath(args.server), '--convert', foreign], cwd=folder,
                                  capture_output=True, text=True, timeout=60 * SLOW)
        converted = os.path.join(folder, 'legacy.odex')
        text = open(converted).read() if os.path.isfile(converted) else ''
        check('W349: .ode range=1 imports as sweep rangefiles: true',
              imported.returncode == 0 and 'rangefiles: true' in text and 'range: 1' not in text, imported.stderr + text)
        result = subprocess.run([os.path.abspath(args.server), converted, '--silent', '--outfile', 'out.dat'],
                                cwd=folder, capture_output=True, text=True, timeout=60 * SLOW)
        paths = sorted(glob.glob(os.path.join(folder, 'out.dat.*')))
        rows = [[list(map(float, line.split())) for line in open(path)] for path in paths]
        check('W349: built-in silent range honours its output filename and each run',
              result.returncode == 0 and len(rows) == 3 and all(len(r) == 3
              and abs(r[-1][1] - .2 * (i + 1)) < 1e-6 for i, r in enumerate(rows)), result.stderr + str(rows))
        for group, key, value in (('sweep', 'rangefiles', '1'), ('sweep', 'rangefiles', 'maybe'), ('output', 'range', '1')):
            bad = os.path.join(folder, 'bad.odex')
            with open(bad, 'w') as f:
                f.write("x'=1\noptions { %s: { %s: %s } }\n" % (group, key, value))
            result = subprocess.run([os.path.abspath(args.server), bad, '--silent'], cwd=folder,
                                    capture_output=True, text=True, timeout=60 * SLOW)
            check('W349: own .odex refuses %s: %s' % (key, value),
                  result.returncode != 0 and 'bad.odex:2' in result.stderr, result.stderr)
        # Reading and saving the converted model exercises the named option round trip.
        with tempfile.TemporaryDirectory(prefix='w349load') as run:
            s = Server(args.server, converted, run=run)
            try:
                s.collect(is_idle)
                s.send(cmd='session.save_as', file='roundtrip', data=False)
                s.collect(is_idle)
                with zipfile.ZipFile(os.path.join(run, 'roundtrip.snapx')) as z:
                    text = z.read('model.setx').decode()
                check('W349: converted .odex loads the named setting', 'rangefiles: true' in text, text[-600:])
            finally:
                s.close()


check_range_files()

def check_storage_full():
    with tempfile.TemporaryDirectory(prefix='w347') as folder:
        model = os.path.join(folder, 'full.odex')
        with open(model, 'w') as f:
            f.write("x'=1\noptions { integration: { total_time: 1, dt: .1, storage_rows: 4 } }\n")
        # W48 removed XPP_MEM_FAIL_AT. The storage owner's narrow hook tests
        # this recoverable failure without failing unrelated allocations.
        s = Server(args.server, model, env={'XPP_STORAGE_FAIL_GROW': '1'})
        try:
            s.collect(is_idle)
            s.send(cmd='events.subscribe', events=['series'])
            s.collect(is_idle)
            s.send(cmd='run.initial')
            evs, _ = s.collect(is_idle)
            errors = [e for e in evs if e.get('error')]
            state = next((e for e in reversed(evs) if e.get('ev') == 'state'), {})
            series = whole_series(evs)
            cols = {c['col']: c['data'] for c in series[-1]['columns']} if series else {}
            check('W347: failed storage growth stops with one placed error and no overwrite question',
                  len(errors) == 1 and errors[0].get('line', 0) > 0
                  and 'Cannot allocate sufficient storage' in errors[0].get('error', '')
                  and not any(e.get('kind') == 'choice' for e in evs), str(errors))
            check('W347: failed growth preserves every stored row in time order',
                  state.get('rows') == 4 and len(cols.get(0, [])) == 4
                  and all(abs(t - i * .1) < 1e-6 for i, t in enumerate(cols.get(0, [])))
                  and all(abs(x - i * .1) < 1e-6 for i, x in enumerate(cols.get(1, []))), str(cols))
            s.send(cmd='run.configure', total_time=-2)
            evs, _ = s.collect(is_idle)
            check('W347: protocol rejects negative Total with its command place',
                  any(e.get('error') and e.get('line', 0) > 0 for e in evs), str(evs[-3:]))
        finally:
            s.close()
        env = dict(os.environ, XPP_STORAGE_FAIL_GROW='1')
        result = subprocess.run([os.path.abspath(args.server), model, '--silent'], env=env,
                                cwd=folder, capture_output=True, text=True, timeout=30 * SLOW)
        check('W347: silent failed growth reports the same error once',
              result.stderr.count('Cannot allocate sufficient storage') == 1, result.stderr)
        foreign = os.path.join(folder, 'negative.ode')
        with open(foreign, 'w') as f:
            f.write("x'=1\n@ total=-.2,dt=.1\ndone\n")
        result = subprocess.run([os.path.abspath(args.server), '--convert', foreign], cwd=folder,
                                capture_output=True, text=True, timeout=30 * SLOW)
        converted = os.path.join(folder, 'negative.odex')
        text = open(converted).read() if os.path.isfile(converted) else ''
        check('W347: negative .ode Total converts to its absolute finite duration',
              result.returncode == 0 and 'total_time: 0.2' in text, result.stderr + text)
        result = subprocess.run([os.path.abspath(args.server), converted, '--silent'], cwd=folder,
                                capture_output=True, text=True, timeout=30 * SLOW)
        output = os.path.join(folder, 'output.dat')
        rows = [list(map(float, line.split())) for line in open(output)] if os.path.isfile(output) else []
        check('W347: imported negative Total stores data and ends at its absolute duration',
              result.returncode == 0 and len(rows) == 3 and abs(rows[-1][0] - .2) < 1e-6
              and abs(rows[-1][1] - .2) < 1e-6, result.stderr + str(rows))
        os.remove(output)
        zero = os.path.join(folder, 'zero.odex')
        with open(zero, 'w') as f:
            f.write("x'=1\ninit x=0\noptions { integration: { total_time: 0 } }\n")
        result = subprocess.run([os.path.abspath(args.server), zero, '--silent'], cwd=folder,
                                capture_output=True, text=True, timeout=30 * SLOW)
        rows = [line.split() for line in open(output)] if os.path.isfile(output) else []
        check('W347: Total=0 loads and stores only the starting point',
              result.returncode == 0 and len(rows) == 1, result.stderr + str(rows))
        for total in (-2, -0.5):
            bad = os.path.join(folder, 'bad.odex')
            with open(bad, 'w') as f:
                f.write("x'=1\noptions { integration: { total_time: %s } }\n" % total)
            result = subprocess.run([os.path.abspath(args.server), bad, '--silent'], cwd=folder,
                                    capture_output=True, text=True, timeout=30 * SLOW)
            check('W347: own .odex rejects Total=%s at its line' % total,
                  result.returncode != 0 and 'bad.odex:2' in result.stderr, result.stderr)


check_storage_full()


def check_stop_checks():
    """W388: long computations (fixed-step chunks, flow, range nullclines,
    range equilibria, pairwise histograms, and fitting) reach cancellation
    checkpoints, stopping cleanly on abort, and silent replays honour armed stops."""
    # Each run below would take hours without its Stop check, so only the
    # check can end it within the wait's safety limit (no race on speed).
    W388_GRID = 400        # flow: 2 x 401^2 trajectories
    W388_STEPS = 1000000   # range of nullclines / equilibria: points
    W388_ROWS = 400000     # stored rows (at dt 0.05) for the pairwise histogram
    def stopped_in(evs):
        return any(e.get('ev') == 'stopped' for e in evs)
    p, r, snd, col, _ = launch_server()
    try:
        initial, _ = col(is_idle)
        registry = next(e for e in initial if e.get('ev') == 'hello')['commands']
        null_steps = int(next(a['max'] for c in registry if c['cmd'] == 'nullcline.range_freeze'
                              for a in c['arguments'] if a['name'] == 'steps'))
        max_mesh = int(next(a['max'] for c in registry if c['cmd'] == 'nullcline.set_mesh'
                            for a in c['arguments'] if a['name'] == 'mesh'))
        snd(cmd='events.subscribe', events=['numerics'])
        numerics, _ = col(is_idle)
        old_mesh = int(next(f['value'] for e in numerics if e.get('ev') == 'numerics'
                            for f in e['fields'] if f['key'] == 'nullcline_mesh'))
        # 1. Fixed-step chunk with large store_every (core/odesol2.cpp)
        snd(cmd='run.configure', store_every=50000000)
        col(is_idle)
        snd(cmd='run.configure', total_time=10000000)
        col(is_idle)
        snd(cmd='run.configure', method='euler')
        col(is_idle)
        snd(cmd='run.initial')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        stopped = any(e.get('ev') == 'stopped' for e in evs)
        check('W388: fixed-step chunk with large store_every stops on abort', stopped, str([e.get('ev') for e in evs]))

        # Restore standard settings
        snd(cmd='run.configure', store_every=1)
        col(is_idle)
        snd(cmd='run.configure', total_time=20)
        col(is_idle)

        # 2. Flow grid cancellation (core/nullcline.cpp)
        snd(cmd='dirfield.flow', grid=W388_GRID)
        _, started = col(lambda e: e.get('ev') == 'computing' or is_idle(e))
        flow_started = started is not None and started.get('ev') == 'computing'
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        check('W388: direction field flow loop stops on abort', flow_started and stopped_in(evs), str([e.get('ev') for e in evs][-8:]))

        # 3. Range nullclines cancellation (core/nullcline.cpp)
        # A maximal advertised mesh makes this range hours of work without Stop, independently of machine speed.
        snd(cmd='nullcline.set_mesh', mesh=max_mesh)
        col(is_idle)
        snd(cmd='nullcline.range_freeze', parameter='iapp', steps=null_steps, start=0, end=1)
        _, started = col(lambda e: e.get('ev') == 'computing' or is_idle(e))
        null_started = started is not None and started.get('ev') == 'computing'
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        check('W388: range nullclines sweep stops on abort', null_started and stopped_in(evs), str([e.get('ev') for e in evs][-8:]))
        snd(cmd='nullcline.set_mesh', mesh=old_mesh)
        col(is_idle)

        # 4. Range equilibria sweep cancellation (core/integrate.cpp)
        snd(cmd='equilibrium.range', parameter='iapp', steps=W388_STEPS, start=-0.5, end=0.5)
        _, started = col(lambda e: e.get('ev') == 'computing' or is_idle(e))
        eq_started = started is not None and started.get('ev') == 'computing'
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        check('W388: range equilibria sweep stops on abort', eq_started and stopped_in(evs), str([e.get('ev') for e in evs][-8:]))

        # 5. Pairwise differences histogram cancellation (core/histogram.cpp):
        #    W388_ROWS^2 differences
        snd(cmd='run.configure', total_time=W388_ROWS * 0.05)
        col(is_idle)
        snd(cmd='run.initial')
        col(is_idle)
        snd(cmd='analysis.autocorrelation', column='V', bins=10, low=0, high=10)
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        check('W388: pairwise histogram O(N^2) loop stops on abort', stopped_in(evs), str([e.get('ev') for e in evs][-8:]))
        snd(cmd='run.configure', total_time=20)
        col(is_idle)


        # 6. Fitting cancellation (core/do_fit.cpp)
        fit_path = os.path.join(r, 'fit_data.dat')
        with open(fit_path, 'w') as f:
            for i in range(20):
                f.write(f'{i * 0.5} {-0.5 + i * 0.05}\n')
        snd(cmd='analysis.fit_curve', variables=['V'], parameters=['iapp'], tolerance=1e-9, points=20, columns=2, data_columns=[2], epsilon=1e-4, max_iterations=1000, file='fit_data.dat')
        snd(cmd='run.abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        msgs = [e.get('bottom') for e in evs if e.get('ev') in ('message', 'done') and 'bottom' in e]
        check('W388: fitting Levenberg-Marquardt loop stops on abort', ' Stopped ' in msgs, str(msgs))
    finally:
        stop_server(p, r, snd)

    # 7. --silent: recx replay with armed abort on large store_every
    code, out, err = run_replay([
        {'cmd': 'run.configure', 'store_every': 1000000},
        {'cmd': 'run.configure', 'method': 'euler'},
        {'cmd': 'run.configure', 'total_time': 10000000},
        {'cmd': 'run.initial'},
        {'cmd': 'run.abort', 'at': {'what': 'integrate', 'rows': 2, 't': 0}},
    ])
    check('W388: --silent plays armed stop on fixed-step integration', code == 0, 'exit %d, %s' % (code, err[-300:]))


check_stop_checks()


# W521: the run, ic and bvp resources on the registry: each command's good call with its result,
# a refused argument with its message, and the old forms refused as unknown
def check_run_registry():
    p, r, snd, col, _ = launch_server()

    def call(**cmd):
        snd(**cmd)
        evs, _ = col(is_idle, timeout=60 * SLOW)
        done = next((e for e in reversed(evs) if e.get('ev') == 'done'), {})
        return done, evs

    def ok(what, cmd, want):
        done, evs = call(**cmd)
        got = done.get('result', {})
        check('W521: %s' % what, done.get('status') == 'ok' and all(got.get(k) == v for k, v in want.items() if v is not None)
              and all(k in got for k in want), str(done)[:300])
        return got

    def refused(what, cmd, words):
        done, evs = call(**cmd)
        check('W521: %s is refused (%s)' % (what, words), done.get('status') == 'error' and words in done.get('error', ''),
              str(done)[:300])

    try:
        col(is_idle)
        got = ok('run.initial runs and says rows and time', {'cmd': 'run.initial'}, {'status': 'completed', 'steps': 601, 'time': None})
        ok('ic.from_data_row makes a stored row the initial values', {'cmd': 'ic.from_data_row', 'row': 2}, {'row': 2, 'values': None})
        call(cmd='run.configure', total_time=0.5)
        ok('run.backward runs backward', {'cmd': 'run.backward'}, {'status': 'completed', 'steps': 11, 'time': None})
        call(cmd='run.configure', total_time=30)
        ok('run.last runs from the last state', {'cmd': 'run.last'}, {'status': 'completed', 'steps': 601, 'time': None})
        ok('run.old runs from the initial values as they stand', {'cmd': 'run.old'}, {'status': 'completed', 'steps': 601, 'time': None})
        ok('run.shift continues the clock', {'cmd': 'run.shift'}, {'status': 'completed', 'steps': 601, 'time': None})
        ok('run.configure sets the numerics together and says their values',
           {'cmd': 'run.configure', 'total_time': 10, 'dt': 0.05, 'method': 'runge_kutta', 'store_every': 1, 'stop_at_limit': True},
           {'settings': {'total_time': 10, 'dt': 0.05, 'method': 'runge_kutta', 'store_every': 1, 'stop_at_limit': True}})
        ok('run.continue extends by a duration', {'cmd': 'run.continue', 'extra': 1}, {'status': 'completed', 'end_time': None})
        ok('run.continue with no value continues for another Total', {'cmd': 'run.continue'}, {'status': 'completed', 'end_time': None})
        ok('run.steady settles or reaches its limit',
           {'cmd': 'run.steady', 'decimals': 6, 'hold': 1, 'maximum': 100}, {'status': None, 'decimals': 6, 'time': None})
        ok('run.range with no arguments takes the session\'s range', {'cmd': 'run.range', 'variable': 'iapp', 'start': 0, 'end': 0.1, 'steps': 2},
           {'runs_completed': 3, 'total_steps': None})
        ok('run.double_range runs the grid',
           {'cmd': 'run.double_range', 'variable1': 'iapp', 'start1': 0, 'end1': 0.1, 'variable2': 'phi', 'start2': 0.3, 'end2': 0.4,
            'steps1': 1, 'steps2': 1, 'vary': 'independently'}, {'grid_points': 4, 'total_steps': None})
        ok('ic.set sets some initial values', {'cmd': 'ic.set', 'values': {'v': -0.1}}, {'applied': None})
        ok('ic.formula sets an initial value from a formula', {'cmd': 'ic.formula', 'variable': 'w', 'formula': '2*0.25'},
           {'variable': 'w', 'formula': '2*0.25'})
        ok('ic.array sets states from a formula of t', {'cmd': 'ic.array', 'name': 'v', 'size': 2, 'formula': 't+1'}, {'name': 'v', 'size': 2})
        ok('ic.at runs from a point of the graph', {'cmd': 'ic.at', 'coordinates': {'v': 0.1, 'w': 0.2}}, {'applied': None})
        ok('ic.mouse_stream runs from each point', {'cmd': 'ic.mouse_stream', 'x': [0.1, 0.2], 'y': [0.1, 0.2]}, {'trajectories_run': 2})
        refused('run.dae_guess of a model with no algebraic variable', {'cmd': 'run.dae_guess'}, 'no algebraic variables')
        refused('run.shoot with no shooting data', {'cmd': 'run.shoot', 'manifold_index': 1}, 'No shooting data')
        refused('run.continue with both extra and until', {'cmd': 'run.continue', 'extra': 1, 'until': 50}, 'At most one')
        refused('run.continue with a text duration', {'cmd': 'run.continue', 'extra': '1'}, 'extra')
        refused('run.steady without its hold', {'cmd': 'run.steady', 'decimals': 6, 'maximum': 10}, 'hold')
        refused('run.steady with 16 decimals', {'cmd': 'run.steady', 'decimals': 16, 'hold': 1, 'maximum': 10}, 'decimals')
        refused('run.range over a name the model does not have', {'cmd': 'run.range', 'variable': 'nosuch'}, 'nosuch')
        refused('run.range with no steps', {'cmd': 'run.range', 'steps': 0}, 'steps')
        refused('run.double_range with an unknown way of varying', {'cmd': 'run.double_range', 'vary': 'sideways'}, 'sideways')
        refused('run.configure with nothing to set', {'cmd': 'run.configure'}, 'at least one')
        refused('run.configure with a total time that is not a number', {'cmd': 'run.configure', 'total_time': 'long'}, 'total_time')
        refused('run.configure with an unknown method', {'cmd': 'run.configure', 'method': 'nosuch'}, 'nosuch')
        refused('run.configure with a minimum step above the maximum', {'cmd': 'run.configure', 'min_step': 1e9}, 'max_step')
        refused('ic.set with a name that is no variable', {'cmd': 'ic.set', 'values': {'nosuch': 1}}, 'nosuch')
        refused('ic.set with no values', {'cmd': 'ic.set', 'values': {}}, 'at least one')
        refused('ic.formula that does not parse', {'cmd': 'ic.formula', 'variable': 'v', 'formula': '2*('}, '2*(')
        refused('ic.array past the last state', {'cmd': 'ic.array', 'name': 'w', 'size': 5, 'formula': '1'}, 'do not fit')
        refused('ic.from_data_row past the data', {'cmd': 'ic.from_data_row', 'row': 100000}, 'past the data')
        refused('ic.load of a file that is not there', {'cmd': 'ic.load', 'file': 'nosuch.ic'}, 'nosuch.ic')
        refused('ic.load of a path', {'cmd': 'ic.load', 'file': '../x.ic'}, 'file')
        refused('ic.at with a state the graph does not plot', {'cmd': 'ic.at', 'coordinates': {'v': 0.1, 'iapp': 1}}, 'iapp')
        refused('ic.at with one coordinate', {'cmd': 'ic.at', 'coordinates': {'v': 0.1}}, 'Give both')
        refused('ic.mouse_stream with unequal lists', {'cmd': 'ic.mouse_stream', 'x': [1, 2], 'y': [1]}, 'one pair')
        refused('ic.at in a window that is not open', {'cmd': 'ic.at', 'coordinates': {'v': 0, 'w': 0}, 'window': 'nowhere'}, 'nowhere')
        with open(os.path.join(r, 'start.ic'), 'w') as f:
            f.write('-0.2\n0.1\n')
        got = ok('ic.load reads the file of the model folder', {'cmd': 'ic.load', 'file': 'start.ic'}, {'file': 'start.ic', 'applied': None})
        check('W521: ic.load applied the values', got.get('applied', {}).get('V') == -0.2 and got['applied'].get('W') == 0.1, str(got))
        done, _ = call(cmd='bvp.configure', max_iterates=7, tolerance=1e-7, epsilon=1e-5)
        check('W521: bvp.configure sets the shooting numerics together',
              done.get('result', {}).get('settings') == {'bvp_max_iterates': 7, 'bvp_tolerance': 1e-7, 'bvp_epsilon': 1e-5}, str(done))
        refused('bvp.configure with no iterations', {'cmd': 'bvp.configure', 'max_iterates': 0}, 'max_iterates')
        refused('bvp.solve without show', {'cmd': 'bvp.solve'}, 'show')
        refused('bvp.solve_periodic with a name that is no parameter', {'cmd': 'bvp.solve_periodic', 'frequency_parameter': 'v', 'section_variable': 'v', 'section': 0, 'show': False}, 'parameter')
        refused('bvp.solve_range with an unknown side', {'cmd': 'bvp.solve_range', 'side': 'middle'}, 'middle')
        for old in ({'cmd': 'continue'}, {'cmd': 'steady', 'decimals': 3, 'hold': 1, 'maximum': 5}, {'cmd': 'abort'}):
            snd(**old)
            evs, _ = col(is_idle)
            err = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
            check('W521: the old %s is refused as unknown' % old['cmd'], 'Unknown command' in err.get('error', ''), str(evs)[-200:])
        evs = []
        for old in ({'cmd': 'set', 'kind': 'ic', 'name': 'v', 'value': 0}, {'cmd': 'set', 'kind': 'num', 'name': 'total_time', 'value': 5}):
            done, evs = call(**old)
            err = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
            check('W521: set no longer takes %s' % old['kind'], 'no longer takes' in err.get('error', ''), str(evs)[-200:])
        snd(cmd='keymap.reset'); col(is_idle)
        snd(cmd='key', key='Ctrl+R'); evs,_ = col(is_idle)
        check('W517: Ctrl+R runs run.initial directly', any(e.get('ev') == 'state' and e.get('rows',0)>1 for e in evs) and not any(is_ask(e) for e in evs))
    finally:
        stop_server(p, r, snd)


check_run_registry()

# W525: the analysis and table resources on the registry: each command's good call with its result, a refused
# argument with its message, and the old forms refused as unknown
def check_analysis_registry():
    def helpers(snd, col):
        def call(**cmd):
            snd(**cmd)
            evs, _ = col(is_idle, timeout=60 * SLOW)
            done = next((e for e in reversed(evs) if e.get('ev') == 'done'), {})
            return done, evs

        def ok(what, cmd, want):
            done, evs = call(**cmd)
            got = done.get('result', {})
            check('W525: %s' % what, done.get('status') == 'ok' and all(got.get(k) == v for k, v in want.items() if v is not None)
                  and all(k in got for k in want), str(done)[:300])
            return got

        def refused(what, cmd, words):
            done, evs = call(**cmd)
            check('W525: %s is refused (%s)' % (what, words), done.get('status') == 'error' and (words in done.get('error', '') or words in done.get('file', '')),
                  str(done)[:300])

        return call, ok, refused

    def session(model=None):
        p, r, snd, col, _ = launch_server(ode=model)
        col(is_idle)
        return (p, r, snd, col) + helpers(snd, col)

    p, r, snd, col, call, ok, refused = session()

    def browser_rows():
        snd(cmd='browser.page', **{'from_row': 0, 'count': 12, 'first_column': 1, 'columns': 2})
        _, rows = col(lambda e: e.get('ev') == 'browser')
        col(is_idle)
        return (rows or {}).get('data')

    try:
        ok('analysis.seed_random seeds the next run', {'cmd': 'analysis.seed_random', 'seed': 42}, {'seed': 42})
        refused('analysis.seed_random with a negative seed', {'cmd': 'analysis.seed_random', 'seed': -1}, 'seed')
        refused('analysis.seed_random with no seed', {'cmd': 'analysis.seed_random'}, 'seed')
        refused('analysis.load_stochastic_mean before any stochastic run', {'cmd': 'analysis.load_stochastic_mean'}, 'No stochastic runs')
        refused('analysis.load_stochastic_variance before any stochastic run', {'cmd': 'analysis.load_stochastic_variance'}, 'No stochastic runs')
        refused('analysis.histogram with no stored data', {'cmd': 'analysis.histogram', 'column': 'v', 'bins': 10, 'low': -1, 'high': 1}, 'nothing to analyse')
        ok('run.initial stores the orbit the analyses read', {'cmd': 'run.initial'}, {'status': 'completed', 'steps': 601, 'time': None})
        got = ok('analysis.statistics gives the mean, variance and standard deviation', {'cmd': 'analysis.statistics', 'column': 'v'},
                 {'mean': None, 'variance': None, 'std_dev': None})
        check('W525: the standard deviation is the variance\'s root', abs(got['std_dev'] ** 2 - got['variance']) < 1e-9 * max(1, got['variance']), str(got))
        ok('analysis.histogram counts every row in its bins', {'cmd': 'analysis.histogram', 'column': 'v', 'bins': 10, 'low': -100, 'high': 100, 'condition': ''},
           {'bins': 10, 'total_samples': 601})
        ok('analysis.histogram with a condition counts the rows where it holds', {'cmd': 'analysis.histogram', 'condition': 't<1'}, {'bins': 10})
        ok('analysis.reload_histogram shows it again', {'cmd': 'analysis.reload_histogram'}, {'reloaded': True})
        ok('analysis.load_stochastic_run puts the run\'s data back', {'cmd': 'analysis.load_stochastic_run'}, {'loaded': True})
        ok('run.initial again', {'cmd': 'run.initial'}, {'status': 'completed'})
        refused('analysis.histogram with one bin', {'cmd': 'analysis.histogram', 'column': 'v', 'bins': 1}, 'bins')
        refused('analysis.histogram with its low above its high', {'cmd': 'analysis.histogram', 'column': 'v', 'bins': 10, 'low': 1, 'high': -1}, 'Low')
        refused('analysis.histogram of a column the model does not have', {'cmd': 'analysis.histogram', 'column': 'nosuch'}, 'nosuch')
        refused('analysis.histogram with a condition that does not parse', {'cmd': 'analysis.histogram', 'bins': 10, 'low': -1, 'high': 1, 'condition': '2*('}, 'condition')
        ok('analysis.histogram_2d counts the pairs', {'cmd': 'analysis.histogram_2d', 'column1': 'v', 'column2': 'w', 'bins1': 5, 'bins2': 4,
                                                      'low1': -100, 'high1': 100, 'low2': -100, 'high2': 100}, {'bins_x': 5, 'bins_y': 4})
        refused('analysis.histogram_2d with more bins than the data table has rows',
                {'cmd': 'analysis.histogram_2d', 'column1': 'v', 'column2': 'w', 'bins1': 4096, 'bins2': 4096, 'low1': -1, 'high1': 1, 'low2': -1, 'high2': 1}, 'room for')
        call(cmd='analysis.load_stochastic_run')
        ok('analysis.fourier_series gives the harmonics', {'cmd': 'analysis.fourier_series', 'column': 'v'}, {'harmonics': 299})
        call(cmd='analysis.load_stochastic_run')
        ok('analysis.power_spectrum gives the frequencies', {'cmd': 'analysis.power_spectrum', 'column': 'v'}, {'frequencies': 299})
        call(cmd='analysis.load_stochastic_run')
        ok('analysis.autocorrelation gives its bins', {'cmd': 'analysis.autocorrelation', 'column': 'v', 'bins': 20, 'low': -5, 'high': 5}, {'lags_computed': 21})
        call(cmd='analysis.load_stochastic_run')
        ok('analysis.cross_correlation directly', {'cmd': 'analysis.cross_correlation', 'column1': 'v', 'column2': 'w', 'lags': 10, 'method': 'direct'},
           {'lags_computed': None})
        call(cmd='analysis.load_stochastic_run')
        ok('analysis.cross_correlation by the FFT', {'cmd': 'analysis.cross_correlation', 'column1': 'v', 'column2': 'w', 'lags': 10, 'method': 'fft'},
           {'lags_computed': None})
        refused('analysis.cross_correlation with more lags than half the data allows', {'cmd': 'analysis.cross_correlation', 'lags': 4000}, 'lags')
        refused('analysis.cross_correlation by a method that is not one', {'cmd': 'analysis.cross_correlation', 'method': 'slow'}, 'slow')
        call(cmd='analysis.load_stochastic_run')
        ok('analysis.spectral_density of one column', {'cmd': 'analysis.spectral_density', 'type': 'psd_x', 'column': 'v', 'window_length': 64, 'window': 'hann'},
           {'points': 32})
        call(cmd='analysis.load_stochastic_run')
        ok('analysis.spectral_density of two (their coherence)', {'cmd': 'analysis.spectral_density', 'type': 'coherence_xy', 'column': 'v', 'column2': 'w',
                                                                  'window_length': 64, 'window': 'square'}, {'points': 32})
        refused('analysis.spectral_density with a window of one point', {'cmd': 'analysis.spectral_density', 'window_length': 1}, 'window_length')
        refused('analysis.spectral_density with a window longer than the data', {'cmd': 'analysis.spectral_density', 'window_length': 100000}, 'No window of')
        refused('analysis.spectral_density of a type that is not one', {'cmd': 'analysis.spectral_density', 'type': 'psd_z'}, 'psd_z')
        refused('analysis.spectral_density with a window that is not one', {'cmd': 'analysis.spectral_density', 'window': 'hat'}, 'hat')
        call(cmd='analysis.load_stochastic_run')
        got = ok('analysis.lyapunov_exponent of the stored orbit', {'cmd': 'analysis.lyapunov_exponent'}, {'max_lyapunov_exponent': None, 'runs_completed': 1})
        got = ok('analysis.lyapunov_exponent over a range gives the runs of the sweep',
                 {'cmd': 'analysis.lyapunov_exponent', 'range_over_parameters': True, 'variable': 'iapp', 'start': 0, 'end': 0.1, 'steps': 2},
                 {'max_lyapunov_exponent': None, 'runs_completed': 3})
        refused('analysis.lyapunov_exponent with the sweep\'s settings and no range', {'cmd': 'analysis.lyapunov_exponent', 'steps': 3}, 'range_over_parameters')
        refused('analysis.lyapunov_exponent over a range of a name the model does not have',
                {'cmd': 'analysis.lyapunov_exponent', 'range_over_parameters': True, 'variable': 'nosuch'}, 'nosuch')
        ok('analysis.poincare_map sets a section', {'cmd': 'analysis.poincare_map', 'type': 'section', 'variable': 'v', 'section': 0.1, 'direction': 'positive',
                                                    'stop_on_section': False}, {'poincare_configured': True})
        ok('analysis.poincare_map of the maxima and minima', {'cmd': 'analysis.poincare_map', 'type': 'max_min'}, {'poincare_configured': True})
        ok('analysis.poincare_map none', {'cmd': 'analysis.poincare_map', 'type': 'none'}, {'poincare_configured': False})
        refused('analysis.poincare_map of a type that is not one', {'cmd': 'analysis.poincare_map', 'type': 'max'}, 'max')
        refused('analysis.poincare_map with no type', {'cmd': 'analysis.poincare_map'}, 'type')
        refused('analysis.poincare_map with a direction that is not one', {'cmd': 'analysis.poincare_map', 'type': 'section', 'direction': 'up'}, 'up')
        refused('analysis.poincare_map of a variable the model does not have', {'cmd': 'analysis.poincare_map', 'type': 'section', 'variable': 'nosuch'}, 'nosuch')
        ok('analysis.phase_space_torus folds all variables', {'cmd': 'analysis.phase_space_torus', 'mode': 'all', 'period': 6.28}, {'torus_variables': None})
        got = ok('analysis.phase_space_torus folds the chosen variables', {'cmd': 'analysis.phase_space_torus', 'mode': 'choose', 'variables': ['v']}, {'torus_variables': None})
        check('W525: only the chosen variable is folded', len(got.get('torus_variables', [])) == 1, str(got))
        ok('analysis.phase_space_torus none', {'cmd': 'analysis.phase_space_torus', 'mode': 'none'}, {'torus_variables': []})
        refused('analysis.phase_space_torus with a period of 0', {'cmd': 'analysis.phase_space_torus', 'mode': 'all', 'period': 0}, 'period')
        refused('analysis.phase_space_torus with a mode that is not one', {'cmd': 'analysis.phase_space_torus', 'mode': 'some'}, 'some')
        refused('analysis.phase_space_torus choosing a name that is no variable', {'cmd': 'analysis.phase_space_torus', 'mode': 'choose', 'variables': ['iapp']}, 'iapp')
        ok('analysis.color_coding by velocity, its range from the stored data', {'cmd': 'analysis.color_coding', 'mode': 'velocity'}, {'color_coding': 'velocity'})
        ok('analysis.color_coding by a column between limits', {'cmd': 'analysis.color_coding', 'mode': 'variable', 'variable': 'w', 'low': 0, 'high': 1},
           {'color_coding': 'variable'})
        ok('analysis.color_coding by a column, its range from the stored data', {'cmd': 'analysis.color_coding', 'mode': 'variable', 'variable': 'w'},
           {'color_coding': 'variable'})
        ok('analysis.color_coding none', {'cmd': 'analysis.color_coding', 'mode': 'none'}, {'color_coding': 'none'})
        refused('analysis.color_coding with one limit', {'cmd': 'analysis.color_coding', 'mode': 'velocity', 'low': 0}, 'low')
        refused('analysis.color_coding with its limits in the wrong order', {'cmd': 'analysis.color_coding', 'mode': 'variable', 'low': 1, 'high': 0}, 'empty')
        refused('analysis.color_coding by a negative low velocity', {'cmd': 'analysis.color_coding', 'mode': 'velocity', 'low': -1, 'high': 1}, 'below 0')
        refused('analysis.color_coding by a mode that is not one', {'cmd': 'analysis.color_coding', 'mode': 'rainbow'}, 'rainbow')
        refused('analysis.color_coding in a window that is not open', {'cmd': 'analysis.color_coding', 'mode': 'none', 'window': 'nowhere'}, 'nowhere')
        with open(os.path.join(r, 'fit.dat'), 'w') as f:
            for i in range(20):
                f.write(f'{i * 0.5} {-0.5 + i * 0.05}\n')
        got = ok('analysis.fit_curve says if it converged and the parameters it found',
                 {'cmd': 'analysis.fit_curve', 'variables': ['v'], 'parameters': ['iapp'], 'data_columns': [2], 'columns': 2, 'points': 20, 'tolerance': 1e-9,
                  'epsilon': 1e-4, 'max_iterations': 3, 'file': 'fit.dat'}, {'converged': None, 'parameters': None})
        check('W525: the fit names its parameter', list(got.get('parameters', {})) == ['iapp'] and isinstance(got['converged'], bool), str(got))
        refused('analysis.fit_curve of a file that is not there', {'cmd': 'analysis.fit_curve', 'variables': ['v'], 'parameters': ['iapp'], 'file': 'nosuch.dat'}, 'nosuch.dat')
        refused('analysis.fit_curve of a path', {'cmd': 'analysis.fit_curve', 'variables': ['v'], 'parameters': ['iapp'], 'file': '../fit.dat'}, 'file')
        refused('analysis.fit_curve with a variable the model does not have', {'cmd': 'analysis.fit_curve', 'variables': ['nosuch'], 'parameters': ['iapp']}, 'nosuch')
        refused('analysis.fit_curve with no variables', {'cmd': 'analysis.fit_curve', 'parameters': ['iapp']}, 'variables')
        refused('analysis.fit_curve with a data column below 2', {'cmd': 'analysis.fit_curve', 'variables': ['v'], 'parameters': ['iapp'], 'data_columns': [1]}, 'data_columns')
        refused('analysis.fit_curve with more columns than the file has', {'cmd': 'analysis.fit_curve', 'variables': ['v'], 'parameters': ['iapp'], 'data_columns': [2],
                                                                           'columns': 5, 'points': 20, 'file': 'fit.dat'}, 'needs')
        refused('analysis.fit_curve with more points than the file has', {'cmd': 'analysis.fit_curve', 'variables': ['v'], 'parameters': ['iapp'], 'data_columns': [2],
                                                                          'columns': 2, 'points': 900000, 'file': 'fit.dat'}, 'needs')
        refused('analysis.fit_curve with a tolerance of 0', {'cmd': 'analysis.fit_curve', 'variables': ['v'], 'parameters': ['iapp'], 'tolerance': 0}, 'tolerance')
        refused('analysis.adjoint_load before the adjoint is computed', {'cmd': 'analysis.adjoint_load'}, 'No adjoint')
        # many runs: the mean and variance of the runs are kept for the data browser
        call(cmd='analysis.load_stochastic_run')
        call(cmd='run.range', variable='iapp', start=0, end=0.1, steps=2)
        ok('analysis.stochastic_integrate runs the trials', {'cmd': 'analysis.stochastic_integrate', 'trials': 4}, {'trials_completed': 4})
        ok('analysis.load_stochastic_mean shows the mean of the runs', {'cmd': 'analysis.load_stochastic_mean'}, {'loaded': True})
        ok('analysis.load_stochastic_variance shows their variance', {'cmd': 'analysis.load_stochastic_variance'}, {'loaded': True})
        refused('analysis.stochastic_integrate with one trial', {'cmd': 'analysis.stochastic_integrate', 'trials': 1}, 'trials')
        # the old forms are gone
        for old, words in (({'cmd': 'browser', 'op': 'postprocess'}, 'Unknown command browser'), ({'cmd': 'browser', 'op': 'postprocess', 'kind': 'hist'}, 'Unknown command browser'),
                           ({'cmd': 'set', 'kind': 'options', 'name': 'seed', 'value': 3}, 'set takes kind')):
            snd(**old)
            evs, _ = col(is_idle)
            err = next((e for e in evs if e.get('ev') in ('message', 'done') and 'error' in e), {})
            check('W525: the old form %s is refused' % json.dumps(old), words in err.get('error', ''), str(evs)[-200:])
    finally:
        stop_server(p, r, snd)

    # the adjoint of a limit cycle (borrowed.ode: a stable cycle of period 2*pi) and its H function
    p, r, snd, col, call, ok, refused = session(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'borrowed.ode'))
    try:
        refused('analysis.adjoint_compute with no orbit', {'cmd': 'analysis.adjoint_compute'}, 'Compute a periodic orbit first')
        refused('analysis.adjoint_interaction before the adjoint', {'cmd': 'analysis.adjoint_interaction'}, 'Must compute adjoint first')
        refused('analysis.adjoint_load before the adjoint', {'cmd': 'analysis.adjoint_load'}, 'No adjoint')
        refused('analysis.adjoint_load_interaction before the H function', {'cmd': 'analysis.adjoint_load_interaction'}, 'No H function')
        ok('the orbit', {'cmd': 'run.initial'}, {'status': 'completed'})
        ok('analysis.adjoint_configure sets the adjoint\'s iterations and tolerance', {'cmd': 'analysis.adjoint_configure', 'max_iterates': 50, 'tolerance': 1e-5},
           {'updated': True})
        refused('analysis.adjoint_configure with a tolerance of 0', {'cmd': 'analysis.adjoint_configure', 'tolerance': 0}, 'tolerance')
        refused('analysis.adjoint_configure with no iterations', {'cmd': 'analysis.adjoint_configure', 'max_iterates': 0}, 'max_iterates')
        ok('analysis.adjoint_compute', {'cmd': 'analysis.adjoint_compute'}, {'computed': True})
        ok('analysis.adjoint_load shows it', {'cmd': 'analysis.adjoint_load'}, {'loaded': True})
        ok('analysis.adjoint_interaction with the coupling of one equation', {'cmd': 'analysis.adjoint_interaction', 'couplings': {'x': 'x-y'}}, {'computed': True})
        ok('analysis.adjoint_load_interaction shows the H function', {'cmd': 'analysis.adjoint_load_interaction'}, {'loaded': True})
        ok('analysis.adjoint_load_orbit shows the orbit', {'cmd': 'analysis.adjoint_load_orbit'}, {'loaded': True})
        refused('analysis.adjoint_interaction with a coupling that does not parse', {'cmd': 'analysis.adjoint_interaction', 'couplings': {'y': '2*('}}, 'couplings')
        refused('analysis.adjoint_interaction for a name that is no differential equation', {'cmd': 'analysis.adjoint_interaction', 'couplings': {'r': '1'}}, 'r')
        ok('analysis.adjoint_range is off at first', {'cmd': 'analysis.adjoint_range'}, {'adjoint_range': False})
        ok('analysis.adjoint_range on', {'cmd': 'analysis.adjoint_range', 'enabled': True}, {'adjoint_range': True})
        ok('analysis.adjoint_range off', {'cmd': 'analysis.adjoint_range', 'enabled': False}, {'adjoint_range': False})
        refused('analysis.adjoint_range with a number for its boolean', {'cmd': 'analysis.adjoint_range', 'enabled': 1}, 'enabled')
        got = ok('analysis.histogram of the model\'s own settings (@ histbins, histcol, histlo, histhi)', {'cmd': 'analysis.histogram'}, {'bins': 20, 'total_samples': None})
        check('W525: that histogram counted the orbit', got.get('total_samples', 0) > 100, str(got))
    finally:
        stop_server(p, r, snd)

    # the lookup tables: one of a formula, one of a file
    folder = tempfile.mkdtemp(prefix='xpptables')
    try:
        with open(os.path.join(folder, 'ft.tab'), 'w') as f:
            f.write('5\n0\n1\n0.5\n0.6\n0.7\n0.8\n0.9\n')
        with open(os.path.join(folder, 'ft2.tab'), 'w') as f:
            f.write('3\n0\n1\n1\n2\n3\n')
        model = os.path.join(folder, 'tables.ode')
        with open(model, 'w') as f:
            f.write('table ft "ft.tab"\ntable fn % 5 0 4 3*t\nx\'=-x+ft(t)+fn(t)\ninit x=0\n@ total=2\ndone\n')
        s = Server(args.server, model, run=folder)
        snd, col = s.send, s.collect
        call, ok, refused = helpers(snd, col)
        try:
            col(is_idle)
            ok('table.edit changes a formula table\'s formula, points and range', {'cmd': 'table.edit', 'name': 'fn', 'formula': '2*t', 'x_min': 0, 'x_max': 8, 'points': 9,
                                                                                   'auto_evaluate': False}, {'table': 'FN', 'points_evaluated': 9})
            ok('table.edit leaves out what stays', {'cmd': 'table.edit', 'name': 'FN', 'points': 17}, {'table': 'FN', 'points_evaluated': 17})
            ok('table.edit of a file table reads its file again', {'cmd': 'table.edit', 'name': 'ft'}, {'table': 'FT', 'points_evaluated': 5})
            ok('table.edit of a file table reads another file', {'cmd': 'table.edit', 'name': 'ft', 'file': 'ft2.tab'}, {'table': 'FT', 'points_evaluated': 3})
            got = ok('table.view_in_browser shows the points', {'cmd': 'table.view_in_browser', 'name': 'fn'}, {'table': 'FN', 'shown_in_browser': True})
            refused('table.edit of a name that is no table', {'cmd': 'table.edit', 'name': 'x'}, 'no table named')
            refused('table.edit of a name the model does not have', {'cmd': 'table.edit', 'name': 'nosuch'}, 'nosuch')
            refused('table.edit of a formula table with a file', {'cmd': 'table.edit', 'name': 'fn', 'file': 'ft.tab'}, 'not a file')
            refused('table.edit of a file table with a formula', {'cmd': 'table.edit', 'name': 'ft', 'formula': 't'}, 'read from a file')
            refused('table.edit of a file table with a file that is not there', {'cmd': 'table.edit', 'name': 'ft', 'file': 'nosuch.tab'}, 'nosuch.tab')
            refused('table.edit with a formula that does not parse', {'cmd': 'table.edit', 'name': 'fn', 'formula': '2*('}, 'formula')
            refused('table.edit with one point', {'cmd': 'table.edit', 'name': 'fn', 'points': 1}, 'points')
            refused('table.edit with its range the wrong way round', {'cmd': 'table.edit', 'name': 'fn', 'x_min': 5, 'x_max': 1}, 'low end')
            refused('table.edit with a table of a million and one points', {'cmd': 'table.edit', 'name': 'fn', 'points': 1000001}, 'points')
            refused('table.edit with no name', {'cmd': 'table.edit', 'points': 5}, 'name')
            refused('table.view_in_browser of a name that is no table', {'cmd': 'table.view_in_browser', 'name': 'x'}, 'no table named')
        finally:
            s.close()
    finally:
        shutil.rmtree(folder, ignore_errors=True)


check_analysis_registry()


# W521: the numerics with no key of the main table (the Volterra points, the CVODE band, the delay's guesses and grid)
# are run.configure's arguments, named as the .odex options name them, each only where it applies
def check_numerics_extras():
    def session(model):
        p, r, snd, col, _ = launch_server(ode=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'examples', 'ode', model))

        def call(**cmd):
            snd(**cmd)
            evs, _ = col(is_idle, timeout=60 * SLOW)
            return next((e for e in reversed(evs) if e.get('ev') == 'done'), {})
        col(is_idle)
        return p, r, snd, call

    p, r, snd, call = session('delay.odex')
    try:
        done = call(cmd='run.configure', delay_real_guess=1.5, delay_imaginary_guess=3, delay_grid=40)
        check('W521: run.configure sets the delay guesses and grid and says them',
              done.get('status') == 'ok' and done['result']['settings'] == {'delay_real_guess': 1.5, 'delay_imaginary_guess': 3, 'delay_grid': 40}, str(done))
        done = call(cmd='run.configure', delay_grid=0)
        check('W521: a delay grid of 0 is refused naming the argument', done.get('status') == 'error' and 'delay_grid' in done.get('error', ''), str(done))
        done = call(cmd='run.configure', bandlo=2)
        check('W521: bandlo refused with a method that cannot be banded, nothing set',
              done.get('status') == 'error' and 'bandlo' in done.get('error', ''), str(done))
        done = call(cmd='run.configure', method='cvode', total_time=7, bandlo=2, bandup=3)
        check('W521: with CVODE the band is set together with the method',
              done.get('status') == 'ok' and done['result']['settings'].get('bandlo') == 2 and done['result']['settings'].get('bandup') == 3
              and done['result']['settings'].get('total_time') == 7, str(done))
        done = call(cmd='run.configure', method='euler', vmaxpts=100)
        check('W521: a refused extra takes the whole call back (the method stays CVode)',
              done.get('status') == 'error' and 'vmaxpts' in done.get('error', ''), str(done))
    finally:
        stop_server(p, r, snd)

    p, r, snd, call = session('voltex1.odex')
    try:
        done = call(cmd='run.configure', vmaxpts=5000, autoeval=False)
        check('W521: run.configure sets the Volterra points and auto-evaluation',
              done.get('status') == 'ok' and done['result']['settings'] == {'vmaxpts': 5000, 'autoeval': False}, str(done))
        done = call(cmd='run.configure', delay_grid=10)
        check('W521: a delay grid is refused for a model with no delays',
              done.get('status') == 'error' and 'delay_grid' in done.get('error', ''), str(done))
    finally:
        stop_server(p, r, snd)


check_numerics_extras()


def check_recursive_user_functions():
    """W387: a user function that recurses without end stops the run with its file and line,
    preserving already-stored rows, while terminating recursion (e.g. test.odex) runs cleanly."""
    with tempfile.TemporaryDirectory(prefix='w387') as folder:
        # 1. Direct recursion probe: f(x)=f(x)+1, x1'=-f(x1)
        probe = os.path.join(folder, 'probe.ode')
        with open(probe, 'w') as f:
            f.write("f(x)=f(x)+1\nx1'=-f(x1)\ndone\n")
        s = Server(args.server, probe)
        try:
            s.collect(is_idle)
            s.send(cmd='run.initial')
            evs, _ = s.collect(is_idle)
            errors = [e for e in evs if e.get('error')]
            check('W387: direct recursion in server mode stops with one placed error',
                  len(errors) == 1 and errors[0].get('line') == 3
                  and 'probe' in errors[0].get('file', '')
                  and errors[0].get('source') == 'fun f(x) = f(x)+1'
                  and "user function 'f' nests deeper than" in errors[0].get('error', '').lower(),
                  str(errors))
        finally:
            s.close()

        res_silent = subprocess.run([os.path.abspath(args.server), probe, '--silent'],
                                    cwd=folder, capture_output=True, text=True, timeout=30 * SLOW)
        check('W387: direct recursion in --silent mode exits 1 with placed error and no segfault',
              res_silent.returncode == 1 and 'probe.odex:3' in res_silent.stderr
              and "user function 'f' nests deeper than" in res_silent.stderr.lower(),
              f'rc={res_silent.returncode}, stderr={res_silent.stderr}')

        # 2. Mutual recursion: f(x)=g(x), g(x)=f(x), x1'=-f(x1)
        mutual = os.path.join(folder, 'mutual.ode')
        with open(mutual, 'w') as f:
            f.write("f(x)=g(x)\ng(x)=f(x)\nx1'=-f(x1)\ndone\n")
        s = Server(args.server, mutual)
        try:
            s.collect(is_idle)
            s.send(cmd='run.initial')
            evs, _ = s.collect(is_idle)
            errors = [e for e in evs if e.get('error')]
            check('W387: mutual recursion in server mode stops with one placed error',
                  len(errors) == 1 and 'mutual' in errors[0].get('file', '')
                  and errors[0].get('line') in (3, 4)
                  and "nests deeper than" in errors[0].get('error', '').lower(),
                  str(errors))
        finally:
            s.close()

        res_mutual = subprocess.run([os.path.abspath(args.server), mutual, '--silent'],
                                    cwd=folder, capture_output=True, text=True, timeout=30 * SLOW)
        check('W387: mutual recursion in --silent mode exits 1 with placed error',
              res_mutual.returncode == 1 and 'mutual.odex:' in res_mutual.stderr
              and "nests deeper than" in res_mutual.stderr.lower(),
              f'rc={res_mutual.returncode}, stderr={res_mutual.stderr}')

        # 3. Preserves stored rows before recursion error:
        # f(x, s) recurses only when s > 0.5
        partial = os.path.join(folder, 'partial.odex')
        with open(partial, 'w') as f:
            f.write("fun f(x, s) = if s>0.5 then f(x, s)+1 else x\nx' = -f(x, t)\noptions { integration: { total_time: 2, dt: 0.1 } }\n")
        s = Server(args.server, partial)
        try:
            s.collect(is_idle)
            s.send(cmd='run.initial')
            evs, _ = s.collect(is_idle)
            errors = [e for e in evs if e.get('error')]
            state = next((e for e in reversed(evs) if e.get('ev') == 'state'), {})
            check('W387: recursion stops run and preserves stored rows',
                  len(errors) == 1 and 'nests deeper than' in errors[0].get('error', '').lower()
                  and state.get('rows', 0) >= 5, f'errors={errors}, rows={state.get("rows")}')
        finally:
            s.close()

        # 4. What no caller takes is shown when the command ends (ui_json.cpp):
        # the direction field's grid stops at the first call nested too deep
        field = os.path.join(folder, 'field.odex')
        with open(field, 'w') as f:
            f.write("fun f(x) = f(x)+1\nx' = -f(x)\ny' = x\n"
                    "options { plot: { xp: x, yp: y, xlo: -1, xhi: 1, ylo: -1, yhi: 1 } }\n")
        s = Server(args.server, field)
        try:
            s.collect(is_idle)
            s.send(cmd='dirfield.compute', grid=10)
            evs, _ = s.collect(is_idle)
            errors = [e for e in evs if e.get('error')]
            check('W387: a direction field nested too deep shows one placed error when the command ends',
                  len(errors) == 1 and errors[0].get('line') == 1
                  and 'nests deeper than' in errors[0].get('error', ''), str(errors))
        finally:
            s.close()

        # 5. Terminating recursion: examples/ode/test.odex (factorial) loads and runs
        test_odex = os.path.abspath('examples/ode/test.odex')
        s = Server(args.server, test_odex)
        try:
            s.collect(is_idle)
            s.send(cmd='run.initial')
            evs, _ = s.collect(is_idle)
            errors = [e for e in evs if e.get('error')]
            state = next((e for e in reversed(evs) if e.get('ev') == 'state'), {})
            check('W387: test.odex terminating recursion runs without error',
                  len(errors) == 0 and state.get('rows', 0) > 0, f'errors={errors}, rows={state.get("rows")}')
        finally:
            s.close()

        res_test = subprocess.run([os.path.abspath(args.server), test_odex, '--silent'],
                                  cwd=folder, capture_output=True, text=True, timeout=30 * SLOW)
        check('W387: test.odex runs cleanly in --silent',
              res_test.returncode == 0, f'rc={res_test.returncode}, stderr={res_test.stderr}')


check_recursive_user_functions()
check_equilibrium_settings()
check_run_registry()
check_stops()
check_auto_stops()
check_auto_global_ends()
check_auto_starts()
check_auto_two_parameter_starts()
check_auto_periodic_start_kind()
check_auto_registry()
check_fit_stops()
check_event_count_note()
check_auto_restart_file_stops()
check_auto_multiplier_note()
check_auto_singular_pivot()

print('server checks: %d checks, %s' % (check.checks, 'all passed' if check.failures == 0 else '%d failed' % check.failures))
sys.exit(1 if check.failures else 0)
