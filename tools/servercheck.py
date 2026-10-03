#!/usr/bin/env python3
"""Drive xppautX --server through the JSON protocol and check what comes back.

usage: tools/servercheck.py [--server ./xppautX] [--ode examples/ode/lecar.odex] [-v]

Plays a fixed session (integrate, change a parameter, answer a menu, a
string prompt and a form, find an equilibrium, open a second plot window)
and prints PASS/FAIL per step. No display needed; runs in a few seconds.
"""
import argparse, base64, cmath, glob, hashlib, io, json, math, os, re, shutil, struct, subprocess, sys, tempfile, threading, time, queue, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xppclient import SeriesMirror, drain_stderr, is_ask, placed, whole_series, save_permission

ap = argparse.ArgumentParser()
ap.add_argument('--server', default='./xppautX')
ap.add_argument('--ode', default='examples/ode/lecar.odex')
ap.add_argument('-v', action='store_true')
args = ap.parse_args()
# XPP_CHECK_SLOW=F multiplies every wait by F (tools/xppclient.py)
SLOW = float(os.environ.get('XPP_CHECK_SLOW', '1'))

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
    global failures
    run_dir = tempfile.mkdtemp(prefix='xppquiet')
    try:
        shutil.copy(args.ode, run_dir)
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


def launch_server(extra_env=None, ode=None, log=None, extra_args=None):
    """Start one xppautX --server instance in its own scratch directory and
    return (proc, run_dir, send, collect, events) -- send/collect work just
    like the module-level ones below but are bound to this instance, so a
    second, differently-configured server (e.g. a bad HOME, another model)
    can be driven the same way without disturbing the main session. With
    `log` (a list), the server's stderr lines are appended to it."""
    ode = ode or args.ode
    run_dir = tempfile.mkdtemp(prefix='xppserver')
    shutil.copy(ode, run_dir)
    env = None
    if extra_env is not None:
        env = dict(os.environ)
        env.update(extra_env)
    # encoding='utf-8' explicitly (docs/protocol.md: the protocol is UTF-8):
    # text=True alone decodes with locale.getpreferredencoding(), which on a
    # Windows box without the UTF-8 system locale is the ANSI code page
    # (cp1252 here), mojibake-ing a non-ASCII name on this side even though
    # the wire bytes and xppautX itself (card W35b) are correct UTF-8.
    proc = subprocess.Popen([os.path.abspath(args.server), '--server', os.path.basename(ode)] + (extra_args or []), cwd=run_dir,
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


proc, run, send, collect, events = launch_server()
failures = 0


def check(name, ok, detail=''):
    global failures
    print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else '  ' + detail))
    failures += 0 if ok else 1


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
              and e.get('source') == '@ total=abc' and e.get('error') == '@ total=abc: not a number', str(e))
        check('load error: the log names the file and line, once (W140)',
              p.stderr.count('opts.inc:1: @ total=abc: not a number') == 1
              and p.stderr.count('not a number') == 1, repr(p.stderr[-300:]))

        with open(os.path.join(bad_dir, 'bad.odex'), 'w') as f:
            f.write("par a = 1\nx' = -x + * a\n")
        p, evs = load('bad.odex')
        e = next((v for v in evs if v.get('ev') == 'error'), {})
        check('load error: an .odex problem has its line and column',
              p.returncode == 1 and e.get('file') == 'bad.odex' and e.get('line') == 2 and e.get('col', 0) > 0
              and e.get('source') == "x' = -x + * a" and e.get('error'), str(e))

        # a value an option refuses (W119): the load stops at the option's line
        for name, text, src in (('badopt.ode', "par a=1\nx'=-x\n@ total=5\n@ ync=12\ndone\n", '@ ync=12'),
                                ('badnum.ode', "par a=1\nx'=-x\n@ total=5\n@ dt=2*3\ndone\n", '@ dt=2*3'),
                                ('badopt.odex', "par a = 1\nx' = -x\n@ total = 5\n@ ync = 12\n", '@ ync = 12')):
            with open(os.path.join(bad_dir, name), 'w') as f:
                f.write(text)
            p, evs = load(name)
            e = next((v for v in evs if v.get('ev') == 'error'), {})
            check('load error: a value an option refuses stops the load at its line (%s)' % name,
                  p.returncode == 1 and e.get('file') == name and e.get('line') == 4
                  and src.replace(' ', '').split('@')[1] in e.get('error', '').replace(' ', '')
                  and e.get('source') == src, str(e))
        for method, reason in (('symplectic', 'even dimensions'), ('rubbish', 'Unknown method')):
            with open(os.path.join(bad_dir, 'badmethod.odex'), 'w') as f:
                f.write("x' = -x\ny' = x\nz' = y\n@ meth=" + method + "\n")
            p, evs = load('badmethod.odex')
            e = next((v for v in evs if v.get('ev') == 'error'), {})
            check('W132: @ meth=%s fails the whole load at its line' % method,
                  p.returncode == 1 and e.get('file') == 'badmethod.odex' and e.get('line') == 4
                  and e.get('source') == '@ meth=' + method and reason in e.get('error', '')
                  and not any(v.get('ev') == 'hello' for v in evs), str(e))
    finally:
        shutil.rmtree(bad_dir, ignore_errors=True)


check_load_error()


def is_state(e):
    return e.get('ev') == 'state'


def is_idle(e):
    return e.get('ev') == 'idle'


def last_state(evs):
    st = [e for e in evs if e.get('ev') == 'state']
    return st[-1] if st else None


evs, _ = collect(is_idle)
st = last_state(evs)
hello = next((e for e in evs if e.get('ev') == 'hello'), None)
check('hello', hello is not None and len(hello['menus']['main']) == 20)
check('hello says protocol 3, and no draw ops or palette follow (removed in 2)',
      hello is not None and hello.get('protocol') == 3 and not any(e.get('ev') in ('draw', 'palette') for e in evs),
      str(hello and hello.get('protocol')))
check('hello carries the About text: author, email, issues URL, version',
      hello is not None and all(t in hello.get('about', '') for t in
          ('Author: Muhammad Ahmad', 'muhammadmoustafa22@gmail.com',
           'https://github.com/MuhammadMoustafa/xppautX/issues', 'Protocol 3', 'xppautX ')),
      str(hello and hello.get('about')))
check('state after hello', st is not None and any(p[0] == 'iapp' for p in st['pars']), str(st))
check('window 1 is created', any(e.get('ev') == 'window' and e.get('op') == 'create' and e.get('win') == 1
                                 for e in evs))
send(cmd='size', win=1, w=800, h=600)
evs, _ = collect(is_idle)
check('size is no command any more (removed in protocol 2): an unknown-command error, then state and idle',
      [e.get('ev') for e in evs] == ['message', 'state', 'idle'] and 'Unknown command' in evs[0].get('error', ''),
      str(evs)[:200])
check('hello lists the series feature', 'series' in hello.get('features', []), str(hello.get('features')))

# W95: every action's kind, one letter (c control, v view, d data, x
# computation): each item of the three main-window menus and of the windows'
# key layers (core/menus.cpp), and every command that is not a key (the
# command table core/ui_json.cpp dispatches from); the documented commands
# (docs/protocol.md's table) are each in it
def check_hello_kinds():
    menus = hello.get('menus', {})
    bad = [w for w in ('main', 'file', 'num')
           if len(menus.get(w + '_kinds', '')) != len(menus.get(w + '_keys', ''))
           or len(menus.get(w + '_kinds', '')) != len(menus.get(w, []))
           or set(menus.get(w + '_kinds', '')) - set('cvsdx')]
    check('W95: hello gives each item of the main, File and Numerics menus a kind', not bad, str(bad))
    wins = hello.get('windows', {})
    badw = [w for w, l in wins.items()
            if not (len(l.get('kinds', '')) == len(l.get('keys', '')) == len(l.get('ids', [])) == len(l.get('items', [])))
            or set(l.get('kinds', '')) - set('cvsdx')]
    check('W95: hello gives the key layers of auto, browser, ani, aplot and equilibrium, a kind and a name per key',
          sorted(wins) == ['ani', 'aplot', 'auto', 'browser', 'equilibrium'] and not badw, str(sorted(wins)) + str(badw))
    cmds = hello.get('commands', [])
    kinds = {(c['cmd'], c.get('op')): c['kind'] for c in cmds}
    doc = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'protocol.md'),
               encoding='utf-8').read()
    section = doc[doc.index('## Commands (client to server)'):doc.index('## Window keys')]
    documented = set(re.findall(r'^\| `(\w+)` \|', section, re.M)) - {'key'}
    missing = sorted(d for d in documented if not any(c == d for c, _ in kinds))
    check('W95: hello gives every documented command but key a kind', not missing and cmds
          and all(k in 'cvsdx' for k in kinds.values()), str(missing))
    want = {('display', None): 'v', ('set', None): 's', ('values', None): 's', ('values', 'write'): 'd',
            ('abort', None): 'c', ('browser', 'write'): 'd', ('browser', None): 'v', ('auto', 'set'): 's',
            ('auto', 'grab'): 'd', ('userbut', None): 'x'}
    check('W95, W106: kinds as decided: display a view, set/values/auto set settings, Save data and Grab data, '
          'abort control, a user button a computation',
          all(kinds.get(k) == v for k, v in want.items()), str({k: kinds.get(k) for k in want}))
    num = dict(zip(menus.get('num_keys', ''), menus.get('num_kinds', '')))
    check('W106: the Numerics items that ask a value are settings (Total, Dt, Method, dElay), Parameters too',
          all(num.get(k) == 's' for k in 'tdme') and num.get('') == 'v'
          and dict(zip(menus.get('main_keys', ''), menus.get('main_kinds', ''))).get('p') == 's', str(num))
    main = dict(zip(menus.get('main_keys', ''), menus.get('main_kinds', '')))
    check('W95: Initialconds and Sing pts compute, Window/zoom is a view, File/Save info is data, File/Quit control',
          main.get('i') == 'x' and main.get('s') == 'x' and main.get('w') == 'v'
          and dict(zip(menus.get('file_keys', ''), menus.get('file_kinds', ''))).get('s') == 'd'
          and dict(zip(menus.get('file_keys', ''), menus.get('file_kinds', ''))).get('q') == 'c'
          and 'w' not in menus.get('file_keys', ''), str(main))


check_hello_kinds()


# W118: what core and page both know comes in hello, written once in the core
def check_hello_shared():
    menus = hello.get('menus', {})
    bad = [w for w in ('main', 'file', 'num')
           if len(menus.get(w + '_ids', [])) != len(menus.get(w + '_keys', ''))
           or len(set(menus.get(w + '_ids', []))) != len(menus.get(w + '_ids', []))]
    check('W118: hello names the menus by state\'s menu number and gives each item an id, one per key',
          menus.get('names') == ['main', 'file', 'num'] and not bad, str(menus.get('names')) + str(bad))
    ids = dict(zip(menus.get('main_ids', []), menus.get('main_keys', '')))
    fids = dict(zip(menus.get('file_ids', []), menus.get('file_keys', '')))
    check('W118: the ids name the keys (Initialconds i, Makewindow m, File f; File/Import XPPAUT set r)',
          ids.get('initialconds') == 'i' and ids.get('makewindow') == 'm' and ids.get('file') == 'f'
          and ids.get('numerics') == 'u' and fids.get('importset') == 'r' and 'writeset' not in fids, str(ids))
    check('W118: hello gives the limits (64 MB uploads, 2000 rows and 500 columns a browser block) and the window ids',
          hello.get('limits') == {'upload': 64 << 20, 'browser_rows': 2000, 'browser_cols': 500}
          and hello.get('window_ids') == {'plots': 21, 'auto': 101, 'ani': 104, 'aplot': 105},
          str(hello.get('limits')) + str(hello.get('window_ids')))
    steps = {(c['cmd'], c.get('op')): c.get('step') for c in hello.get('commands', [])}
    check('W118: every command says whether it is a step: a key, set and a write are, state, data, an answer, '
          'the browser\'s paging and the recording\'s own are not',
          all(isinstance(v, bool) for v in steps.values()) and steps.get(('key', None)) is True
          and steps.get(('set', None)) is True and steps.get(('browser', 'write')) is True
          and not any(steps.get(k) for k in (('state', None), ('data', None), ('answer', None), ('browser', None),
                                             ('record', None), ('play', None))), str(steps))


check_hello_shared()
# W118: an error about a file the command could not read names the file (the page offers to add it)
send(cmd='key', key='f')
collect(is_idle)
send(cmd='key', key='r')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
if ask is not None and ask.get('kind') == 'file':
    send(cmd='answer', id=ask['id'], file='w118-gone.set')
    evs, _ = collect(is_idle)
else:
    evs = []
err = next((e for e in evs if e.get('ev') == 'message' and 'error' in e), None)
check('W118: File/Import XPPAUT set of a file that is not there: the error names the file',
      err is not None and err.get('file') == 'w118-gone.set', str(err))
send(cmd='data', events=['series'])
evs, _ = collect(is_idle)
ser = [e for e in evs if e.get('ev') == 'series']
check('asking for the series data sends the plot at once', len(ser) == 1 and ser[0]['win'] == 1 and ser[0]['rows'] == 0
      and len(ser[0]['curves']) == 1, str(ser)[:300])
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('Initialconds opens a menu', ask is not None and ask['kind'] == 'menu' and 'g' in ask['keys'], str(ask))
send(cmd='answer', id=ask['id'], key='g')
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
    shutil.copy(ode, silent)
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
send(cmd='redraw')
evs, _ = collect(is_idle)
check('a redraw of unchanged data sends no series', not any(e.get('ev') == 'series' for e in evs))
send(cmd='key', key='x')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
if ask:
    send(cmd='answer', id=ask['id'], ok=1, value='V')
evs, _ = collect(is_idle)
ser = [e for e in evs if e.get('ev') == 'series']
check('Xi vs t sends the series of V against T', len(ser) == 1 and ser[0]['curves'][0]['x'] == 0
      and ser[0]['curves'][0]['y'] == 1 and [c['name'] for c in ser[0]['columns']] == ['T', 'V'], str(ser)[:300])
send(cmd='data', events=[])
collect(is_idle)
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30 * SLOW)
check('an empty data list stops the series', not any(e.get('ev') == 'series' for e in evs))

send(cmd='set', kind='par', name='iapp', value=0.1)
send(cmd='state')
evs, st = collect(is_state)
collect(is_idle)
check('set parameter', st is not None and dict(st['pars'])['iapp'] == 0.1, str(st and st['pars']))

collect(is_idle)  # the idle of the state command above
send(cmd='set', kind='par', name='iapp', text='%0.1*2')
evs, _ = collect(is_idle)
st = last_state(evs)
check('set parameter by %formula', st is not None and abs(dict(st['pars'])['iapp'] - 0.2) < 1e-12, str(st and st['pars']))
send(cmd='default', kind='par')
evs, _ = collect(is_idle)
st = last_state(evs)
check('default parameters', st is not None and dict(st['pars'])['iapp'] == 0.05, str(st and st['pars']))

# W66: the values panel's Save/Load of XPP's own .par/.ic file, through the
# core (docs/protocol.md "values") -- omitting `name` asks for one, like
# any other Save/Load; given, it skips the ask and writes into the
# model's folder at once
send(cmd='values', op='write', kind='par')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('values write with no name asks for a file', ask is not None and ask['kind'] == 'file'
      and ask.get('mode') == 'write', str(ask))
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
collect(is_idle)

send(cmd='values', op='write', kind='par', name='t_values.par')
collect(is_idle)
with open(os.path.join(run, 't_values.par')) as f:
    par_text = f.read()
first_line = par_text.split('\n', 1)[0]
check('values write par: XPP\'s format, a count then "value  name" lines',
      first_line.endswith('   Number params') and first_line.split()[0].isdigit()
      and '0.05  iapp\n' in par_text, par_text[:80])

send(cmd='values', op='write', kind='ic', name='t_values.ic')
collect(is_idle)
with open(os.path.join(run, 't_values.ic')) as f:
    ic_lines = [l.strip() for l in f if l.strip()]
check('values write ic: the values alone, one per line, in the model order',
      ic_lines == ['-0.144', '0.03'], str(ic_lines))

# change both, away from what was just written
send(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'value': 0.09}, {'kind': 'ic', 'name': 'V', 'value': -0.4}])
evs, _ = collect(is_idle)
st = last_state(evs)
check('values round trip: the values changed before reading back',
      st is not None and dict(st['pars'])['iapp'] == 0.09 and dict(st['ics'])['V'] == -0.4,
      str(st and (st['pars'], st['ics'])))

# read them back through the existing load path (io_parameter_file/io_ic_file, READEM)
send(cmd='values', op='read', kind='par', name='t_values.par')
evs, _ = collect(is_idle)
st = last_state(evs)
check('values read par restores the written value exactly',
      st is not None and dict(st['pars'])['iapp'] == 0.05, str(st and st['pars']))

send(cmd='values', op='read', kind='ic', name='t_values.ic')
evs, _ = collect(is_idle)
st = last_state(evs)
check('values read ic restores the written values exactly',
      st is not None and dict(st['ics'])['V'] == -0.144 and dict(st['ics'])['W'] == 0.03,
      str(st and st['ics']))

send(cmd='values', op='write', kind='xyz', name='t_values.bad')
evs, msg = collect(lambda e: e.get('ev') == 'message' and 'error' in e)
check('values with a kind other than par/ic is refused', msg is not None, str(msg))
collect(is_idle)

send(cmd='browser', **{'from': 10, 'count': 3, 'col': 2, 'ncol': 1})
evs, br = collect(lambda e: e.get('ev') == 'browser')
collect(is_idle)
check('browser sends the rows asked for', br is not None and br['rows'] == 601 and br['from'] == 10
      and len(br['data']) == 3 and len(br['data'][0]) == 2 and br['cols'][:3] == ['T', 'V', 'W'], str(br)[:200])
send(cmd='key', win='browser', key='g', row=10)
evs, _ = collect(is_idle)
st = last_state(evs)
check('browser Get sets the initial conditions from a row',
      st is not None and abs(dict(st['ics'])['W'] - br['data'][0][1]) < 1e-6, str(st and st['ics']))
send(cmd='browser', **{'from': 0, 'count': 0})
collect(is_idle)

# W77: the data browser's Add column is the Session's, not the Model's
# (docs/roadmap.md W77, #125): it must not grow neq/nvar or add a
# variable, so it stays invisible to everything that reads the model --
# but docs/manual/07-data-browser.md promises it stays computed "as
# though ... another auxiliary variable", so it is recomputed (not
# dropped) after every run, and find_variable resolves it by name so it
# can be plotted.
def fetch_browser(nrows):
    send(cmd='browser', **{'from': 0, 'count': nrows, 'col': 1, 'ncol': 500})
    evs, br = collect(lambda e: e.get('ev') == 'browser')
    collect(is_idle)
    return br


def near(a, b, tol=1e-5):
    """elementwise, for a value recomputed in double precision from JSON
    text against a value the core computed in float32: same_floats wants
    bit-identical floats, appropriate for the same stored value read two
    ways, not for redoing its arithmetic"""
    return len(a) == len(b) and all(abs(x - y) < tol for x, y in zip(a, b))


send(cmd='key', win='browser', key='a')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('addcol asks for the column name', ask is not None and ask['kind'] == 'string' and ask.get('name') == 'Name',
      str(ask))
send(cmd='answer', id=ask['id'], ok=1, value='VW')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('addcol then asks for the formula', ask is not None and ask['kind'] == 'string', str(ask))
send(cmd='answer', id=ask['id'], ok=1, value='v*w')
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
send(cmd='browser', op='write', what='table', format='csv', name='added_col.csv')
collect(is_idle)
with open(os.path.join(run, 'added_col.csv')) as f:
    csv_lines = [l.strip() for l in f if l.strip() and not l.startswith('#')]
check('the saved CSV has a VW column, one more than the model columns',
      csv_lines[0].split(',') == orig_cols + ['VW']
      and len(csv_lines[1].split(',')) == len(orig_cols) + 1, str(csv_lines[:2]))

# plot it by name (Xi vs t: graf_par.cpp find_variable resolves it)
send(cmd='data', events=['series'])
collect(is_idle)
send(cmd='key', key='x')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], ok=1, value='VW')
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
send(cmd='set', kind='par', name='iapp', value=0.2)
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
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
send(cmd='data', events=[])
collect(is_idle)

send(cmd='data', events=['series'])
collect(is_idle)
send(cmd='slide', name='iapp', value=0.07, rerun=1)
evs, _ = collect(is_idle, timeout=30 * SLOW)
st = last_state(evs)
ser = whole_series(evs)
check('slide sets the parameter, no run any more (the rerun flag is gone, W69)',
      st is not None and dict(st['pars'])['iapp'] == 0.07 and not ser,
      str(st and st['pars']) + str(ser)[:200])
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30 * SLOW)
ser = whole_series(evs)
check('a later Go runs with the slid value (a new series of 601 rows)',
      ser and ser[-1]['rows'] == 601, str(ser)[:200])
send(cmd='data', events=[])
collect(is_idle)
send(cmd='equations')
evs, eqs = collect(lambda e: e.get('ev') == 'equations')
collect(is_idle)
check('equations', eqs is not None and eqs['lines'][0].startswith('dV/dT='), str(eqs))
send(cmd='key', key='f')
send(cmd='key', key='p')
evs, src = collect(lambda e: e.get('ev') == 'source')
collect(is_idle)
acts = [i for i, c in enumerate(src['comments']) if c[1]] if src else []
check('source lists the comments with actions', len(acts) >= 5, str(src and src['comments'][:3]))
if acts:
    send(cmd='action', index=acts[1])  # {gk=0}
    evs, _ = collect(is_idle)
    st = last_state(evs)
    check('a comment action sets its parameters', st is not None and dict(st['pars'])['gk'] == 0, str(st and st['pars']))
    send(cmd='set', kind='par', name='gk', value=2)
    send(cmd='set', kind='par', name='iapp', value=0.1)
    collect(is_idle)
    collect(is_idle)

send(cmd='key', key='f')
send(cmd='key', key='h')
evs, help_ev = collect(lambda e: e.get('ev') == 'help')
collect(is_idle)
check('File/Help opens the manual at the File menu chapter',
      help_ev is not None and help_ev.get('chapter') == '05-commands' and help_ev.get('anchor') == 'file',
      str(help_ev))

send(cmd='key', key='f')
send(cmd='key', key='s')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('File/Save info asks for a file name', ask is not None and ask['kind'] in ('file', 'string'), str(ask))
check('the file ask lists its folder', ask is not None and ask.get('dir') and 'files' in ask
      and 'lecar.odex' not in ask['files'], str(ask)[:200])
if ask:
    if ask['kind'] == 'file':
        send(cmd='answer', id=ask['id'], ok=1, file='info.txt')
    else:
        send(cmd='answer', id=ask['id'], ok=1, value='info.txt')
    collect(is_idle)
    check('info file written', os.path.exists(os.path.join(run, 'info.txt')))
    check('a file ask for writing says so (mode write)', ask.get('mode') == 'write', str(ask)[:200])
send(cmd='key', key='f')
send(cmd='key', key='r')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('File/Import XPPAUT set asks for a file to read (mode read)', ask is not None and ask['kind'] == 'file'
      and ask.get('mode') == 'read' and ask.get('wild') == '*.set', str(ask)[:200])
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
collect(is_idle)

# W88: a file ask answered with a full path outside the model's folder (what
# the desktop window's own dialog answers) is read and written right there,
# the path whole, however long (Windows' fopen stops at MAX_PATH, so shorter
# there)
elsewhere = tempfile.mkdtemp(prefix='xppelsewhere')
far = os.path.join(elsewhere, 'd' * (40 if os.name == 'nt' else 200))
os.makedirs(far)
far_set = os.path.join(far, 'w88 far.set')
send(cmd='set', kind='par', name='iapp', value=0.33)
collect(is_idle)
send(cmd='session', op='save', name='w88', data=False)
collect(is_idle)
# the .set an XPPAUT writes is the session's model.set and its equations after it
with zipfile.ZipFile(os.path.join(run, 'w88.snapx')) as z:
    with open(far_set, 'wb') as f:
        f.write(z.read('model.set') + b'RHS etc ...\n')
send(cmd='set', kind='par', name='iapp', value=0.77)
collect(is_idle)
send(cmd='key', key='f')
send(cmd='key', key='r')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
if ask:
    send(cmd='answer', id=ask['id'], ok=1, file=far_set)
collect(is_idle)
send(cmd='state')
evs, st = collect(is_state)
collect(is_idle)
check('File/Import XPPAUT set from a full path elsewhere reads it there, the path whole (the parameter it holds is back)',
      ask is not None and ask.get('mode') == 'read' and st is not None and dict(st['pars'])['iapp'] == 0.33,
      str(st and dict(st['pars']).get('iapp')))
shutil.rmtree(elsewhere, ignore_errors=True)

send(cmd='key', key='f')
send(cmd='key', key='t')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check("T32: File/Transpose's Column 1 is a name (hello.lists[0]), the rest integers",
      ask is not None and ask['kind'] == 'form'
      and ask.get('kinds') == ['name:0', 'integer', 'integer', 'integer', 'integer', 'integer'],
      str(ask and ask.get('kinds')))
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
collect(is_idle)


# The model's folder for a client that cannot reach it (docs/protocol.md
# "Files"): the `file` command puts, gets and lists files there, base names
# only, as the /files endpoints of browser mode do (tools/webcheck.py).
def file_cmd(**kw):
    send(cmd='file', **kw)
    evs, ev = collect(lambda e: e.get('ev') == 'file')
    collect(is_idle)
    return ev or {}


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

send(cmd='key', key='u')  # nUmerics menu
send(cmd='key', key='t')  # total
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('nUmerics/Total asks for a number', ask is not None and ask['kind'] == 'string', str(ask))
check("T31: new_float's ask says its field takes a number or %formula (kinds formula)",
      ask is not None and ask.get('kinds') == ['formula'], str(ask))
if ask:
    send(cmd='answer', id=ask['id'], ok=1, value='40')
    collect(is_idle)
send(cmd='key', key='Escape')
collect(is_idle)

send(cmd='key', key='v')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='2')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('Viewaxes/2D opens a form', ask is not None and ask['kind'] == 'form' and 'Xmax' in ask['names'][4], str(ask))
check("T31: the form's fields carry their kinds: the axes' pickers text, the ranges numbers, the labels text",
      ask is not None and ask.get('kinds') == ['text', 'text', 'number', 'number', 'number', 'number', 'text', 'text'],
      str(ask and ask.get('kinds')))
if ask:
    vals = list(ask['values'])
    vals[4] = '40'
    send(cmd='answer', id=ask['id'], ok=1, values=vals)
    collect(is_idle)

# T31: an integer field (Initialconds/Range's Steps) and a name (Xi vs t: T or a variable, hello.lists[0])
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='r')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
kinds = ask.get('kinds') if ask else None
check('T31: Range Integrate says Steps is an integer and Start and End numbers',
      ask is not None and ask['kind'] == 'form' and kinds is not None and len(kinds) == len(ask['names'])
      and kinds[1:4] == ['integer', 'number', 'number'], str(ask))
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
    collect(is_idle)
send(cmd='key', key='x')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('T31: Xi vs t asks for a name from hello.lists[0] (kinds name:0)',
      ask is not None and ask['kind'] == 'string' and ask.get('kinds') == ['name:0'], str(ask))
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
    collect(is_idle)

send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30 * SLOW)
st = last_state(evs)
check('total 40 gives 801 rows', st is not None and st['rows'] == 801, str(st and st['rows']))
check('a run that is not cancelled sends no stopped', not any(e.get('ev') == 'stopped' for e in evs))

# An Abort right behind the answer that starts the run: the run stops, and
# says where (docs/protocol.md "stopped") before its state and idle. Armed
# by row count ahead of time (xpp::job::stop_at_rows, the same mechanism
# --script's own abort replay uses: docs/protocol.md "Scripts"), in a
# one-shot --script subprocess of its own, not raced against the clock in
# the middle of this session: how many rows are kept is known in advance,
# not discovered by luck.
def run_script(lines, ode=None):
    """xppautX --script of `lines` (dicts, one command each) over `ode` (a
    fresh scratch copy); returns (exit code, stdout text, stderr text)"""
    ode = ode or args.ode
    run_dir = tempfile.mkdtemp(prefix='xppscript')
    try:
        shutil.copy(ode, run_dir)
        script_path = os.path.join(run_dir, 'script.jsonl')
        with open(script_path, 'w') as f:
            f.write(''.join(json.dumps(save_permission(c)) + chr(10) for c in lines))
        r = subprocess.run([os.path.abspath(args.server), '--script', script_path, os.path.basename(ode)],
                            cwd=run_dir, capture_output=True, text=True, timeout=60 * SLOW)
        return r.returncode, r.stdout, r.stderr
    finally:
        shutil.rmtree(run_dir, ignore_errors=True)

STOP_ROWS = 400  # well inside total 40's 801 rows
code, out, err = run_script([
    {'cmd': 'key', 'key': 'u'}, {'cmd': 'key', 'key': 't'}, {'cmd': 'answer', 'value': '40'},
    {'cmd': 'key', 'key': 'Escape'},
    {'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'},
    {'cmd': 'abort', 'at': {'what': 'integrate', 'rows': STOP_ROWS, 't': 0}},
    {'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'},
])
check('xppautX --script plays the armed interruption', code == 0, 'exit %d, %s' % (code, err[-300:]))
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
    elif e.get('ev') == 'idle':
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

send(cmd='key', key='s')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
eq = None
for _ in range(6):
    evs, e = collect(lambda e: e.get('ev') in ('ask', 'equilibrium'), timeout=10 * SLOW)
    if e is None:
        break
    if e['ev'] == 'equilibrium':
        eq = e
    else:
        send(cmd='answer', id=e['id'], key='n')
    if eq:
        break
check('Sing pts/Go reports an equilibrium', eq is not None and eq['type'] in ('STABLE', 'UNSTABLE', 'NEUTRAL'), str(eq))
ev = (eq or {}).get('eigenvalues') or []
check('with its eigenvalues, one [re, im] per variable, as its counts say',
      len(ev) == len(eq['values']) and sum(1 for r, i in ev if r < 0) == eq['cminus'] + eq['rminus'], str(ev))
collect(is_idle)

send(cmd='key', key='m')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='c')
evs, _ = collect(lambda e: e.get('ev') == 'window' and e['op'] == 'create', timeout=5 * SLOW)
check('Makewindow/Create opens window 2', any(e.get('ev') == 'window' and e.get('win') == 2 for e in evs))
collect(is_idle)


def answer_asks(until, replies, timeout=20 * SLOW):
    """collect up to until(ev), answering asks whose kind is in replies"""
    got = []
    while True:
        evs, e = collect(lambda e: e.get('ev') == 'ask' or until(e), timeout)
        got += evs
        if e is None or e.get('ev') != 'ask' or e['kind'] not in replies:
            return got, e
        send(cmd='answer', id=e['id'], **replies[e['kind']](e))


pixels = lambda e: {'w': 2, 'h': 1, 'rgb': base64.b64encode(bytes([255, 0, 0, 0, 0, 255])).decode()}
menu = lambda key: (lambda e: {'key': key})
for _ in range(2):
    send(cmd='key', key='k')
    evs, _ = answer_asks(lambda e: e.get('ev') == 'film', {'menu': menu('c')})
    collect(is_idle)
check('Kinescope/Capture sends film captures', evs and evs[-1].get('count') == 2, str(evs[-1:]))
send(cmd='key', key='k')
evs, e = answer_asks(is_idle, {'menu': menu('s'), 'string': lambda e: {'value': 'kin'}, 'pixels': pixels})
check('Kinescope/Save asks for the pixels and writes GIFs',
      all(os.path.exists(os.path.join(run, 'kin_%d.gif' % i)) for i in range(2)), str(os.listdir(run)))
send(cmd='key', key='v')
evs, _ = answer_asks(is_idle, {'menu': menu('t')})
check('the animation window opens', any(e.get('ev') == 'window' and e.get('win') == 104
      and e.get('op') == 'create' for e in evs), str([e for e in evs if e.get('ev') == 'window']))
send(cmd='ani', op='close')
evs, _ = collect(is_idle)
check('the animation window closes', any(e.get('ev') == 'window' and e.get('op') == 'destroy' for e in evs))

send(cmd='plotvars', how=2, names=['V', 'W'])
evs, _ = collect(is_idle)
ap = [e for e in evs if e.get('ev') == 'aplot']
check('the IC arry button opens an array plot', ap and ap[-1]['nx'] == 2 and len(ap[-1]['cells']) == 2 * ap[-1]['ny'],
      str(ap[-1:])[:200])

# docs/ui-v2.md T12: `values`, the cells' numbers before XPP maps them to a
# colour, equal the browser's own numbers for the same rows and columns.
# optimize_aplot (core/graf_par.cpp) always starts at row 0 with ColSkip 1 and
# picks RowSkip so `ny` rows span the run (ndown = min(201, nrows), nskip =
# nrows // ndown, both integer division as the core computes them); the
# columns are V, W (plotvars picked them, in that order).
send(cmd='state')
evs, st = collect(is_state)
collect(is_idle)
nrows = st['rows']
send(cmd='browser', **{'from': 0, 'count': nrows, 'col': 1, 'ncol': 500})
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
send(cmd='data', events=[], enc='f32')
collect(is_idle)
send(cmd='key', win='aplot', key='d')
evs, _ = collect(is_idle)
ap2 = [e for e in evs if e.get('ev') == 'aplot']
check('aplot values as base64 float32 when the client asked for it',
      ap2 and ap2[-1].get('enc') == 'f32' and isinstance(ap2[-1]['values'], str)
      and same_floats(values({'data': ap2[-1]['values']}, 'f32'), got), str(ap2[-1:])[:200])
send(cmd='data', events=[])
collect(is_idle)

send(cmd='key', win='aplot', key='r')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check("T32: array plot Range saving's Basename is a file, Still/Tag integers",
      ask is not None and ask['kind'] == 'form'
      and ask.get('kinds') == ['file', 'integer', 'integer'], str(ask and ask.get('kinds')))
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
collect(is_idle)

send(cmd='aplot', op='close')
collect(is_idle)
send(cmd='click', win=1)
evs, _ = collect(is_idle)
send(cmd='state')
evs, st = collect(is_state)
collect(is_idle)
view = st and st.get('view')
check('state has the view of the active window', view is not None and view['right'] > view['left'], str(view))
send(cmd='key', key='w')
evs, _ = answer_asks(lambda e: e.get('ev') == 'ask' and e['kind'] == 'drag', {'menu': menu('s')})
drag = evs[-1] if evs else None
for d in [{'what': 'down', 'x': 300, 'y': 200}, {'what': 'move', 'x': 360, 'y': 200}, {'what': 'up', 'x': 360, 'y': 200}]:
    send(cmd='answer', id=drag['id'], **d)
    evs, drag = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=drag['id'], ok=0)
evs, _ = collect(is_idle)
st = last_state(evs)
check('Window/Scroll drags the view', st is not None and view is not None and st['view']['xlo'] < view['xlo'],
      str(st and st['view']))

send(cmd='key', key='f')
send(cmd='key', key='a')
evs, _ = collect(lambda e: e.get('ev') == 'window' and e.get('win') == 101)
check('File/Auto opens the AUTO window', any(e.get('ev') == 'window' and e.get('win') == 101 for e in evs))
collect(is_idle)
send(cmd='key', win='auto', key='d')
evs, _ = collect(is_idle)
axes = [e for e in evs if e.get('ev') == 'diagram' and e['op'] in ('axes', 'reset')]
check('the AUTO diagram sends its axes as data', axes and axes[-1]['wid'] > 0 and axes[-1]['xlabel'], str(axes))

# Run, Grab a labelled point, Run again: the second run restarts from the
# label in fort.3 (findlb/readlb). On Windows the backward fseek that located
# the label line was undefined on a text stream and corrupted the heap.
# W96: AUTO's pop-up menus live in menus.cpp (menu_auto_*); every one seen in
# this session is compared with what the page was sent before they moved
# there. The protocol name of each is "auto", the default item goes as "def".
AUTO_MENUS = {
    'Start': (['Steady state', 'Periodic', 'Bdry Value', 'Homoclinic', 'hEteroclinic'], 'spbhe'),
    'Plot Type': (['Hi', 'Norm', 'hI-lo', 'Period', 'Two par', '(Z)oom in', 'Zoom (O)ut', 'last 1 par',
                   'last 2 par', 'Fit', 'fRequency', 'Average', 'Default', 'Scroll', 'new (V)iew'], 'hniptzo12fradsv'),
    'Mark values: how many?': (list('0123456789'), '0123456789'),
    'File': (['Import orbit', 'Save diagram', 'Load diagram', 'Postscript', 'SVG', 'Reset diagram', 'Clear grab',
              'Write pts', 'All info', 'init Data', 'Toggle redraw', 'auto raNge', 'sElect 2par pt', 'draw laBled',
              'lOad branch', 'eXport CSV', 'Import diagram'], 'islpvrcwadtneboxf'),
    'Torus': (['Two Param', 'Fixed period', 'Extend'], 'tfe'),
    'Per. Doub.': (['Doubling', 'Two Param', 'Fixed period', 'Extend'], 'dtfe'),
    'Periodic ': (['Extend', 'Fixed Period'], 'ef'),
    'Hopf Pt': (['Periodic', 'Extend', 'New Point', 'Two Param'], 'pent'),
    'Branch Pt': (['Switch', 'Extend', 'New Point', 'Two Param'], 'sent'),
}
auto_menus_seen = []

def note_auto_menu(ask):
    if ask and ask.get('ev') == 'ask' and ask.get('kind') == 'menu' and ask.get('name') == 'auto':
        auto_menus_seen.append(ask)

send(cmd='key', win='auto', key='r')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
note_auto_menu(ask)
check('Auto/Run opens the start menu', ask is not None and ask['kind'] == 'menu' and 's' in ask['keys'], str(ask))
if ask:
    send(cmd='answer', id=ask['id'], key='s')
    evs, _ = collect(is_idle, timeout=30 * SLOW)
    adds = [e for e in evs if e.get('ev') == 'diagram' and e['op'] == 'add']
    check('Auto/Run sends the points it draws as diagram data',
          adds and adds[0]['from'] == 0 and sum(len(r['x']) for e in adds for r in e['runs']) > 0, str(adds)[:200])
    send(cmd='key', win='auto', key='g')
    evs, ask = collect(lambda e: e.get('ev') == 'ask')
    check('Auto/Grab asks for a point', ask is not None and ask['kind'] == 'grab', str(ask))
    for k in ['Tab', 'Tab', 'Return']:
        if ask is None:
            break
        send(cmd='answer', id=ask['id'], key=k)
        evs, ask = collect(lambda e: e.get('ev') == 'ask' or is_idle(e))
        if k == 'Return' and ask is not None and ask.get('ev') == 'ask':
            send(cmd='answer', id=ask['id'], ok=0)  # a confirmation is not expected; cancel it
            collect(is_idle)
    send(cmd='key', win='auto', key='r')
    evs, e = collect(lambda e: e.get('ev') == 'ask' or is_idle(e), timeout=30 * SLOW)
    if e is not None and e.get('ev') == 'ask':
        note_auto_menu(e)
        send(cmd='answer', id=e['id'], ok=0)
        evs, e = collect(is_idle, timeout=30 * SLOW)
    check('Auto/Run after a Grab restarts from the label and the server survives',
          e is not None and proc.poll() is None, 'exit code %s' % proc.poll())

    # W64 (issue #112): "auto" "grab" by label, no ask (docs/protocol.md
    # "Grab by label"). autocheck.py's grab section is the fuller check (a
    # periodic run from a label matches the same run from an interactive
    # grab); here just that it grabs with no ask and an unknown label is
    # refused, changing nothing.
    labs = sorted({(i, lab) for e in adds for r in e['runs'] for i, lab, _sym in r.get('lab', [])})
    target_label = labs[1][1] if len(labs) > 1 else None
    if target_label is not None:
        send(cmd='auto', op='grab', label=target_label)
        evs, _ = collect(is_idle)
        check('grab by label asks nothing', not any(e.get('ev') == 'ask' for e in evs), str([e.get('ev') for e in evs]))
        check('the server survives a grab by label', proc.poll() is None, 'exit code %s' % proc.poll())
    send(cmd='auto', op='grab', label=999999)
    evs, _ = collect(is_idle)
    errs = [e.get('error') for e in evs if e.get('ev') == 'message' and 'error' in e]
    check('grab by an unknown label is refused (a message error)', bool(errs), str(errs))
    check('an unknown label changes nothing (no diagram event, no ask)',
          not any(e.get('ev') in ('diagram', 'ask') for e in evs), str([e.get('ev') for e in evs]))

    # T21: an Axes change draws the diagram again in the new quantities (no
    # reDraw: a data client shows what the core sent), File/Reset diagram
    # empties it at once, and Usr period's prompts say what it does
    def auto_dialog(op, *answers):
        send(cmd='key', win='auto', key={'axes': 'a', 'usr': 'u', 'file': 'f'}[op])
        evs = []
        for a in answers:
            ev, ask = collect(lambda e: e.get('ev') == 'ask' or is_idle(e), timeout=30 * SLOW)
            evs += ev
            if ask is None or ask.get('ev') != 'ask':
                return evs, ask
            note_auto_menu(ask)
            send(cmd='answer', id=ask['id'], **(a(ask) if callable(a) else a))
        ev, _ = collect(is_idle, timeout=30 * SLOW)
        return evs + ev, None

    evs, _ = auto_dialog('axes', {'key': 'n'}, lambda ask: {'ok': 1, 'values': ask['values']})
    dg = [e for e in evs if e.get('ev') == 'diagram']
    resets = [i for i, e in enumerate(dg) if e['op'] == 'reset']
    added = sum(len(r['x']) for e in dg[resets[-1]:] if e['op'] == 'add' for r in e['runs']) if resets else 0
    check('Axes/Norm, OK: the diagram comes again in norms (reset, then every point) without a reDraw',
          resets and dg[resets[-1]]['plot'] == 1 and dg[resets[-1]]['keep'] == 0 and added > 0,
          str([(e['op'], e.get('keep'), e.get('from'), added) for e in dg])[:300])
    evs, ask = auto_dialog('usr', lambda ask: {'ok': 0})
    titles = [e.get('title') for e in evs if e.get('ev') == 'ask']
    check('Usr period asks "Mark values: how many?"', titles == ['Mark values: how many?'], str(titles))
    evs, _ = auto_dialog('file', {'key': 'r'}, {'key': 'y'})
    dg = [e for e in evs if e.get('ev') == 'diagram']
    check('File/Reset diagram empties the diagram at once (reset 0, nothing added)',
          dg and dg[-1]['op'] == 'reset' and dg[-1]['keep'] == 0 and not any(e['op'] == 'add' for e in dg),
          str([(e['op'], e.get('keep')) for e in dg]))

    titles_seen = {m['title'] for m in auto_menus_seen}
    check('W96: AUTO menus seen: Start, Plot Type, Mark values, File and the grabbed one',
          {'Start', 'Plot Type', 'Mark values: how many?', 'File'} <= titles_seen, str(sorted(titles_seen)))
    for m in auto_menus_seen:
        want = AUTO_MENUS.get(m['title'])
        check('W96: AUTO menu %r is as menus.cpp defines it' % m['title'],
              want is not None and m['items'] == want[0] and m['keys'] == want[1]
              and len(m.get('hints', [])) == len(want[0]), str(m)[:300])


# Live plotting (docs/protocol.md "The plot as data"): while an integration
# runs, a subscribed client gets the rows as they are stored, in "append"
# series events, then the full series. tools/models/live.odex stores 20 001
# rows in about a second, long enough for several appends.
LIVE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'live.odex')


def live_run(send, collect, key='i', answer=None):
    """integrate (Initialconds/Go, or Continue) and return the series events of the command"""
    send(cmd='key', key=key)
    evs, ask = collect(lambda e: e.get('ev') == 'ask', timeout=20 * SLOW)
    if ask:
        send(cmd='answer', id=ask['id'], **(answer or {'key': 'g'}))
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
        send3(cmd='data', events=['series'])
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
        ser = live_run(send3, collect3, 'c', {'value': '2000'})
        more = check_appends('Continue', ser, 20001)
        check('Continue: the rows before it are kept', more is not None and final is not None
              and all(same_floats(more[c][:20001], final[c]) for c in final))

        send3(cmd='data', events=['series'], enc='f32')
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

        send3(cmd='data', events=[])
        collect3(is_idle)
        ser = live_run(send3, collect3)
        check('a client not subscribed gets no series, appended or full', ser == [], str(ser)[:200])
    finally:
        send3(cmd='quit')
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
    snd(cmd='state')
    evs, st = col(is_state)
    col(is_idle)
    view = st['view']
    snd(cmd='key', key='w')
    evs, ask = col(lambda e: e.get('ev') == 'ask')
    snd(cmd='answer', id=ask['id'], key='z')
    evs, ask = col(lambda e: e.get('ev') == 'ask')
    ok = ask is not None and ask['kind'] == 'rubber'
    if ok:
        snd(cmd='answer', id=ask['id'], **answer_of(view))
    evs, _ = col(is_idle)
    st = last_state(evs)
    return ok, view, st and st['view'], (p, r, snd, col)


def pixel_to_data(v, i, j):
    return (v['xlo'] + (v['xhi'] - v['xlo']) * (i - v['left']) / (v['right'] - v['left']),
            v['ylo'] + (v['yhi'] - v['ylo']) * (j - v['bottom']) / (v['top'] - v['bottom']))


def stop_server(p, r, snd):
    snd(cmd='quit')
    try:
        p.wait(timeout=5 * SLOW)
    except subprocess.TimeoutExpired:
        p.kill()
    shutil.rmtree(r, ignore_errors=True)


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
        # Initialconds/Mouse picked in data coordinates: the ICs are that point
        if by_data:
            v = by_data
            xd, yd = v['xlo'] + 0.3 * (v['xhi'] - v['xlo']), v['ylo'] + 0.6 * (v['yhi'] - v['ylo'])
            snd(cmd='key', key='i')
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            snd(cmd='answer', id=ask['id'], key='m')
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            if ask is not None and ask['kind'] == 'mouse':
                snd(cmd='answer', id=ask['id'], xd=xd, yd=yd)
            evs, _ = col(is_idle, timeout=30 * SLOW)
            st = last_state(evs)
            ics = dict(st['ics']) if st else {}
            pxm = abs(v['xhi'] - v['xlo']) / (v['right'] - v['left'])
            pym = abs(v['yhi'] - v['ylo']) / (v['bottom'] - v['top'])
            check('Initialconds/Mouse answered in data coordinates starts from that point, within a pixel',
                  ask is not None and ask['kind'] == 'mouse' and abs(ics.get('V', 1e9) - xd) <= pxm
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

    def command(key, *answers):
        """a key and the answers to the asks it opens; the events up to its idle"""
        send4(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = collect4(lambda e: e.get('ev') in ('ask', 'idle'), timeout=120 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            send4(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

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
        evs = after({'cmd': 'data', 'events': ['series', 'plots']})
        pl = plots(evs)
        w1 = pl[0]['windows'][0] if pl and pl[0]['windows'] else {}
        check('plots at once: window 1, active, its title, axes and curves',
              len(pl) == 1 and pl[0]['active'] == 1 and wins(pl[0]) == [1] and w1.get('title') == 'W vs V'
              and w1.get('three') == 0 and w1.get('xlo') == -0.6 and w1.get('yhi') == 1.2
              and w1.get('curves') and w1['curves'][0]['x'] == 1 and 'theta' in w1 and 'zmax' in w1.get('box', {}),
              str(pl)[:400])
        check('plots comes before the series', evs.index(pl[0]) < [e.get('ev') for e in evs].index('series') if pl and full(evs) else False)
        command('i', {'key': 'g'})
        evs = command('m', {'key': 'c'})
        pl, ser = plots(evs), full(evs)
        check('Makewindow/Create: plots has windows 1 and 2, 2 active',
              len(pl) == 1 and wins(pl[0]) == [1, 2] and pl[0]['active'] == 2, str(pl)[:300])
        check('the new window gets its own series, with the data', [e['win'] for e in ser] == [2]
              and ser[0]['rows'] == 20001, str([(e['win'], e['rows']) for e in ser]))
        evs = command('x', {'value': 'V'})
        pl, ser = plots(evs), full(evs)
        cur = {w['win']: (w['curves'][0]['x'], w['curves'][0]['y']) for w in pl[0]['windows']} if pl else {}
        check('Xi vs t in window 2: its curves change, window 1 keeps its own',
              cur == {1: (1, 2), 2: (0, 1)} and [w['title'] for w in pl[0]['windows']] == ['W vs V', 'V vs T'],
              str(pl)[:400])
        check('... and only window 2 gets a new series', [e['win'] for e in ser] == [2]
              and [c['name'] for c in ser[0]['columns']] == ['T', 'V'], str([(e['win'], e['rows']) for e in ser]))
        evs = after({'cmd': 'click', 'win': 1})
        pl = plots(evs)
        check('selecting window 1 (click) makes it active in plots, and sends no series',
              len(pl) == 1 and pl[0]['active'] == 1 and not full(evs), str(pl)[:200])
        evs = command('i', {'key': 'g'})
        ser = [e for e in evs if e.get('ev') == 'series']
        apps = [e for e in ser if e.get('op') == 'append']
        check('a run appends to the active window only', len(apps) >= 3 and all(e['win'] == 1 for e in apps),
              str([(e['win'], e.get('op')) for e in ser])[:300])
        check('... and ends with the whole series of each window, the active one first (its end, then the full series of window 2)',
              [e['win'] for e in full(evs)] == [1, 2] and all(e['rows'] == 20001 for e in full(evs)),
              str([(e['win'], e['rows']) for e in full(evs)]))
        after({'cmd': 'click', 'win': 2})
        evs = command('m', {'key': 'd'})
        pl = plots(evs)
        check('Makewindow/Destroy of window 2: plots has window 1 only, active',
              len(pl) == 1 and wins(pl[0]) == [1] and pl[0]['active'] == 1 and not full(evs), str(pl)[:300])
        command('m', {'key': 'c'})
        evs = command('m', {'key': 'c'})
        check('two more windows', plots(evs) and wins(plots(evs)[0]) == [1, 2, 3], str(plots(evs))[:200])
        evs = command('m', {'key': 'k'}, {'key': 'y'})
        pl = plots(evs)
        check('Makewindow/Kill all leaves window 1', len(pl) == 1 and wins(pl[0]) == [1] and pl[0]['active'] == 1,
              str(pl)[:300])
        evs = command('m', {'key': 'c'})
        check('a window made again gets its series again', [e['win'] for e in full(evs)] == [2],
              str([(e['win'], e['rows']) for e in full(evs)]))
        evs = after({'cmd': 'redraw'})
        check('a redraw sends neither plots nor series', not plots(evs) and not full(evs))
        evs = after({'cmd': 'data', 'events': ['series']})
        check('without plots in the list: series only', not plots(evs) and [e['win'] for e in full(evs)] == [2, 1],
              str([(e['ev'], e.get('win')) for e in evs if e.get('ev') in ('plots', 'series')]))
    finally:
        stop_server(proc4, run4, send4)


check_plot_windows()


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
        snd(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'value': 0.3}])
        col(is_idle)
        snd(cmd='key', key='f')
        col(is_idle)
        snd(cmd='key', key='g')
        evs, ask = col(lambda e: e.get('ev') == 'ask')
        check('Param set menu is asked', ask is not None and ask['kind'] == 'menu', str(ask))
        if ask:
            snd(cmd='answer', id=ask['id'], ok=0)
        evs, _ = col(is_idle)
        check('Param set menu cancelled: no error message',
              not [e for e in evs if e.get('ev') == 'message' and 'error' in e], str(evs)[:200])
        snd(cmd='values', op='write', kind='par', name='w101.par')
        evs, _ = col(is_idle)
        with open(os.path.join(r, 'w101.par')) as f:
            txt = f.read()
        check('values write par with an edited value: file written, no error',
              '0.3  iapp' in txt and not [e for e in evs if e.get('ev') == 'message' and 'error' in e],
              txt[:80] + str(evs)[:150])
    finally:
        stop_server(p, r, snd)


check_param_set_cancel()


def check_values_protocol():
    pv, rv, sndv, colv, _ = launch_server()

    def after(**cmd):
        sndv(**cmd)
        return colv(is_idle, timeout=30 * SLOW)[0]

    def keys(key, *answers):
        sndv(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = colv(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            sndv(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

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
        after(cmd='data', events=['series'])
        evs = keys('i', {'key': 'g'})
        st, ser = last_state(evs), full(evs)
        cols = {c['col']: c['data'] for c in ser[-1]['columns']} if ser else {}
        check('a run: the full series carries the data version',
              ser and isinstance(ser[-1].get('version'), int), str(ser and {k: v for k, v in ser[-1].items() if k != 'columns'})[:200])
        check('state: now is where the run ended (the last stored row)',
              st and len(st.get('now', [])) == len(st['ics']) and ser
              and all(abs(st['now'][i] - cols[i + 1][-1]) < 1e-6 for i in range(len(st['ics'])) if i + 1 in cols),
              str(st and st.get('now')))
        v0 = ser[-1]['version'] if ser else None

        # W69: `set`/`slide`/`default` never run anything themselves any more
        # (the `rerun` flag is gone, and ignored if a client still sends it):
        # the values panel holds edits pending and sends them in one `set`
        # right before the next command that computes.
        evs = after(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'text': '0.1'},
                                       {'kind': 'ic', 'name': 'v', 'value': -0.2}], rerun=1)
        st = last_state(evs)
        check('set values[]: both set, no run (rerun is ignored, W69)',
              st and abs(par(st, 'iapp') - 0.1) < 1e-12 and abs(ic(st, 'v') + 0.2) < 1e-12 and not full(evs),
              str(st and st['pars'][:3]))
        check('a rerun-free set sends no erase event', not pics(evs), str(pics(evs)))
        evs = keys('i', {'key': 'g'})
        st, ser = last_state(evs), full(evs)
        v_col = ser[-1]['columns'][[c['col'] for c in ser[-1]['columns']].index(1)]['data'] if ser else []
        check('a later Go runs with the values the set left (no rerun needed)',
              st and abs(par(st, 'iapp') - 0.1) < 1e-12 and len(ser) == 1
              and ser[0]['rows'] == 601 and v_col and abs(v_col[0] - f32(-0.2)) < 1e-9 and ser[0]['version'] != v0,
              str(st and st['pars'][:3]) + str([(s['rows'], s.get('version')) for s in ser]))
        evs = after(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'text': '%nosuch+'}], rerun=1)
        check('set values[] with a bad formula: an error, and no run',
              any(e.get('ev') == 'message' and 'error' in e for e in evs) and not full(evs), str(evs)[:300])
        # W131: one number rule (all of the text), a set applies all or nothing,
        # its error names the field, and default is the core's own command
        errs = lambda evs: [e for e in evs if e.get('ev') == 'message' and 'error' in e]
        st1 = last_state(after(cmd='state'))
        evs = after(cmd='set', kind='par', name='iapp', text='abc')
        st = last_state(evs)
        check('set par with text that is not a number: refused naming the field, nothing changes',
              len(errs(evs)) == 1 and errs(evs)[0].get('field') == 'par:iapp' and 'abc' in errs(evs)[0]['error']
              and st and par(st, 'iapp') == par(st1, 'iapp'), str(errs(evs)) + str(st and st['pars'][:3]))
        evs = after(cmd='set', kind='par', name='iapp', text='1O0')
        check('set par 1O0 (a letter O): refused, nothing changes', len(errs(evs)) == 1
              and par(last_state(evs), 'iapp') == par(st1, 'iapp'), str(errs(evs)))
        evs = after(cmd='slide', name='iapp', value='5x')
        check('slide with a value that is not all a number: refused, nothing changes', len(errs(evs)) == 1
              and par(last_state(evs), 'iapp') == par(st1, 'iapp'), str(errs(evs)))
        evs = after(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'text': '0.31'},
                                       {'kind': 'ic', 'name': 'v', 'text': '%bad('},
                                       {'kind': 'par', 'name': 'gca', 'text': '1.1'}])
        st = last_state(evs)
        check('a set of three with the second bad: one error naming ic v, nothing applied',
              len(errs(evs)) == 1 and errs(evs)[0].get('field') == 'ic:v' and st
              and par(st, 'iapp') == par(st1, 'iapp') and par(st, 'gca') == par(st1, 'gca') and ic(st, 'v') == ic(st1, 'v'),
              str(errs(evs)) + str(st and st['pars'][:3]))
        evs = after(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'text': '0.31'},
                                       {'kind': 'num', 'name': 'total', 'text': '1O0'}])
        check('a set with a bad numerics value applies none of it, its error names num:total',
              len(errs(evs)) == 1 and errs(evs)[0].get('field') == 'num:total'
              and par(last_state(evs), 'iapp') == par(st1, 'iapp'), str(errs(evs)))
        keys('u')
        evs = keys('t', {'value': '1O0'})
        check('an answer to a number ask that is not a number is refused, naming the question',
              len(errs(evs)) == 1 and 'Total' in errs(evs)[0]['error'] and '1O0' in errs(evs)[0]['error'], str(errs(evs)))
        keys('Escape')  # back to the main menu
        evs = after(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'text': ' 0.31 '},
                                       {'kind': 'ic', 'name': 'v', 'text': '-0.2'}])
        st = last_state(evs)
        check('a good set of two (blanks around a number allowed) applies both, no error', not errs(evs) and st
              and abs(par(st, 'iapp') - 0.31) < 1e-12 and abs(ic(st, 'v') + 0.2) < 1e-12, str(errs(evs)) + str(st and st['pars'][:3]))
        evs = after(cmd='default', kind='par')
        st = last_state(evs)
        check('default par after edits: every parameter back to the model file value, no error',
              not errs(evs) and st and [v for _, v in st['pars']] == d['pars'], str(errs(evs)) + str(st and st['pars'][:3]))
        after(cmd='set', kind='ic', name='v', value=-0.2)
        before = last_state(after(cmd='state'))
        evs = after(cmd='set', kind='ic', **{'from': 'last'})
        st = last_state(evs)
        check('set from last is gone (W60: Initialconds/Last is the key): the ICs stay, no run',
              st and st['ics'] == before['ics'] and not full(evs),
              str(st and st['ics']) + ' vs ' + str(before and before.get('ics')))
        after(cmd='set', kind='ic', name='v', value=-0.3)
        prev_now = last_state(after(cmd='state'))['now']
        evs = keys('i', {'key': 'l'})
        states = [i for i, e in enumerate(evs) if is_state(e)]
        sers = [i for i, e in enumerate(evs) if e.get('ev') == 'series']
        first = evs[states[0]] if states else None
        check('Initialconds/Last: a state at the start of the run already has the ICs from where the last one ended',
              first and sers and states[0] < sers[-1] and [v for _, v in first['ics']] == prev_now,
              str(first and first['ics']) + ' vs ' + str(prev_now))
        evs = after(cmd='default', kind='par', rerun=1)
        st = last_state(evs)
        check('default with rerun: the model values, no run (rerun is ignored, W69)',
              st and par(st, 'iapp') == d['pars'][[k.lower() for k, _ in st['pars']].index('iapp')] and not full(evs),
              str(st and st['pars'][:3]))
        evs = keys('i', {'key': 'g'})
        check('a later Go runs with the default values the default command left',
              last_state(evs) and par(last_state(evs), 'iapp') == d['pars'][[k.lower() for k, _ in st['pars']].index('iapp')]
              and len(full(evs)) == 1, str(last_state(evs) and last_state(evs)['pars'][:3]))

        evs = after(cmd='slide', name='iapp', value=0.07)
        check('a slide sets the parameter, no run (the rerun flag is gone, W69)',
              not pics(evs) and not full(evs) and abs(par(last_state(evs), 'iapp') - 0.07) < 1e-12, str(pics(evs)))
        evs = keys('i', {'key': 'g'})
        check('a later Go runs with the slid value, no erase event', not pics(evs) and len(full(evs)) == 1, str(pics(evs)))
        evs = keys('e')
        check('Erase: one erase event for the active window, no series', pics(evs) == [('erase', 1)] and not full(evs),
              str(pics(evs)))
        evs = keys('r')
        check('Redraw: one redraw event for the active window, no series', pics(evs) == [('redraw', 1)] and not full(evs),
              str(pics(evs)))
        evs = after(cmd='redraw')
        check('the redraw command (a reconnect) sends no erase or redraw event', not pics(evs), str(pics(evs)))
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

        sndm(cmd='values', op='write', kind='ic', name='kepler_markov.ic')
        colm(is_idle)
        with open(os.path.join(rm, 'kepler_markov.ic')) as f:
            lines = [l.strip() for l in f if l.strip()]
        check('values write ic on a markov model: exactly `node` lines (2), z excluded',
              lines == ['0.0001', '0.0001'], str(lines))

        sndm(cmd='set', kind='ic', name='x1', value=0.5)
        colm(is_idle)
        sndm(cmd='values', op='read', kind='ic', name='kepler_markov.ic')
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
        snde(cmd='values', op='read', kind='par', name='w140bad.par')
        evs, _ = cole(is_idle, timeout=30 * SLOW)
        m = next((e for e in evs if e.get('ev') == 'message' and 'error' in e), {})
        check('an error event: a values file at its line names the file, the line and the line as written (W140)',
              os.path.basename(m.get('file', '')) == 'w140bad.par' and m.get('line') == 1 and m.get('col') == 0
              and m.get('source') == '3   Number params' and 'parameters' in m.get('error', ''), str(m))
        snde(cmd='values', op='read', kind='par', name='w140gone.par')
        evs, _ = cole(is_idle, timeout=30 * SLOW)
        m = next((e for e in evs if e.get('ev') == 'message' and 'error' in e), {})
        check('an error event: a file that cannot be read is named with line 0 (W140)',
              os.path.basename(m.get('file', '')) == 'w140gone.par' and m.get('line') == 0
              and m.get('source') == '' and m.get('error'), str(m))
        snde(cmd='nosuchcommand')
        evs, _ = cole(is_idle, timeout=30 * SLOW)
        m = next((e for e in evs if e.get('ev') == 'message' and 'error' in e), {})
        check('an error event: an error with no place has every field, empty (W140)',
              m.get('file') == '' and m.get('line') == 0 and m.get('col') == 0 and m.get('source') == ''
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

        write('nan.odex', "# NaN at the first step\npar a=1\nx'=sqrt(x-2*a)\ninit x=1\n")
        code, out, err = run_script([{'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'},
                                     {'cmd': 'nosuchcommand'}], ode=os.path.join(d, 'nan.odex'))
        errs = [json.loads(l) for l in out.splitlines() if '"error"' in l and '"message"' in l]
        nan = next((m for m in errs if 'NaN' in m.get('error', '')), {})
        check('an error event: a run-time NaN is named at the equation that makes it (W140b)',
              nan.get('file') == 'nan.odex' and nan.get('line') == 3 and nan.get('source') == "x'=sqrt(x-2*a)"
              and nan.get('error', '').startswith('X is NaN at t'), str(errs))
        step = next((m for m in errs if 'nosuchcommand' in m.get('error', '')), {})
        check("an error event: a script's step is named at the script's file and line (W140b)",
              step.get('file', '').endswith('script.jsonl') and step.get('line') == 3
              and step.get('source') == '{"cmd": "nosuchcommand"}', str(step))
    finally:
        shutil.rmtree(d, ignore_errors=True)


check_error_places_of_each_kind()


def check_load_all_or_nothing():
    """W125: loading one of our files is all or nothing. A set file (File >
    Import XPPAUT set), a parameter file and an initial-conditions file (`values`
    `read`) whose last value is bad, and an internal set (File > Get par
    set) whose last item is, are refused at that line, naming the file and
    the line as written, and the session is exactly as it was (its state,
    and the set file it writes)."""
    model = os.path.join(tempfile.mkdtemp(prefix='w125m'), 'w125.odex')
    with open(model, 'w') as f:
        f.write("x'=-a*x+b\ny'=x-y\npar a=1,b=2\ninit x=1,y=0\nset good = a=3,b=4\nset bad = a=5,x=7,b=oops\n")
    p, r, snd, col, _ = launch_server(ode=model)

    def answered(answers, **cmd):
        """cmd, its asks answered in order (a str is a key, a dict the answer; then cancel), up to idle"""
        snd(**cmd)
        got, n = [], 0
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            a = answers[n] if n < len(answers) else {'ok': 0}
            n += 1
            snd(cmd='answer', id=e['id'], **({'key': a} if isinstance(a, str) else a))

    def state():
        st = last_state(answered((), cmd='state'))
        return {k: v for k, v in st.items() if k not in ('_t', 'session')} if st else st

    def errors(evs):
        return [e for e in evs if e.get('ev') == 'message' and e.get('error')]

    written = []

    def set_file():
        """the set file the session saves now (its model.set), without
        its first line (the time); CR LF read as LF, since Windows writes
        it in text mode"""
        written.append('now%d' % len(written))
        answered((), cmd='session', op='save', name=written[-1], data=False)
        with zipfile.ZipFile(os.path.join(r, written[-1] + '.snapx')) as z:
            return z.read('model.set').decode().replace('\r\n', '\n').split('\n', 1)[1]

    def read_set(name):
        """File > Import XPPAUT set of name"""
        answered((), cmd='key', key='f')
        return answered(({'file': name},), cmd='key', key='r')

    try:
        col(is_idle)
        answered((), cmd='set', values=[{'kind': 'par', 'name': 'a', 'value': 1.5}, {'kind': 'ic', 'name': 'x', 'value': 0.25}])
        st0, set0 = state(), set_file()
        rows = set0.split('\n')
        high = max(k for k in range(len(rows)) if rows[k].endswith('BVP range high')) + 2  # its line in the file
        lines = ('## Set file\n' + set0 + 'RHS etc ...\n').split('\n')
        lines[high - 1] = '1e999  BVP range high'
        with open(os.path.join(r, 'bad.set'), 'w') as f:
            f.write('\n'.join(lines))
        par = '2   Number params\n9  a\noops  b\n'
        with open(os.path.join(r, 'bad.par'), 'w') as f:
            f.write(par)
        with open(os.path.join(r, 'bad.ic'), 'w') as f:
            f.write('9\nnan?\n')
        for what, cmd, answers, file, line, source in (
                ('an XPPAUT set file', None, 'bad.set', 'bad.set', high, '1e999  BVP range high'),
                ('a parameter file', dict(cmd='values', op='read', kind='par', name='bad.par'), (), 'bad.par', 3, 'oops  b'),
                ('an initial-conditions file', dict(cmd='values', op='read', kind='ic', name='bad.ic'), (), 'bad.ic', 2, 'nan?'),
                ('an internal set', dict(cmd='values', op='internset', name='bad'), (), 'w125.odex', 6, None)):
            errs = errors(read_set(answers) if cmd is None else answered(answers, **cmd))
            e = errs[0] if len(errs) == 1 else {}
            check('W125: %s whose last value is bad is refused at that line (%s:%d)' % (what, file, line),
                  os.path.basename(e.get('file', '')) == file and e.get('line') == line
                  and (source is None or e.get('source') == source)
                  and (source is not None or 'b=oops: not a number' in e.get('error', '')), str(errs))
            st, set1 = state(), set_file()
            check('W125: after %s refused, the session is as it was (its state, its set file)' % what,
                  st == st0 and set1 == set0, str([(k, st0.get(k), st.get(k)) for k in st0 if st.get(k) != st0.get(k)])[:300])
        # W153: same counts, another model's names; valid earlier changes
        # must not apply, and no session may be created for a refused file.
        own = ('## Set file\n' + set0 + 'RHS etc ...\n').split('\n')
        other = own.copy()
        named_line = next(k for k, row in enumerate(other) if row.endswith('  b'))
        other[named_line - 1] = '9  a'
        other[named_line] = '9  another_models_parameter'
        with open(os.path.join(r, 'other.set'), 'w') as f:
            f.write('\n'.join(other))
        evs = read_set('other.set')
        errs = errors(evs)
        e = errs[0] if len(errs) == 1 else {}
        check('W153: another model with equal counts refused with file, first wrong line and source',
              os.path.basename(e.get('file', '')) == 'other.set' and e.get('line') == named_line + 1
              and e.get('source') == other[named_line] and 'another_models_parameter' in e.get('error', ''), str(errs))
        check('W153: refused names apply nothing and write no session',
              state() == st0 and set_file() == set0 and not os.path.exists(os.path.join(r, 'other.snapx')))
        answered((), cmd='set', values=[{'kind': 'par', 'name': 'a', 'value': 8}, {'kind': 'ic', 'name': 'x', 'value': 7}])
        with open(os.path.join(r, 'own.set'), 'w') as f:
            f.write('\n'.join(own))
        evs = read_set('own.set')
        messages = [e for e in evs if e.get('ev') == 'message']
        answered_state = last_state(answered((), cmd='state'))
        check('W153: own set applies and its adjacent session becomes open, with one message',
              not errors(evs) and state() == st0 and os.path.exists(os.path.join(r, 'own.snapx'))
              and os.path.basename(answered_state.get('session', {}).get('file', '')) == 'own.snapx'
              and len(messages) == 1 and 'own.snapx' in messages[0].get('bottom', ''), str(messages))
        with zipfile.ZipFile(os.path.join(r, 'own.snapx')) as z:
            check('W153: converted session saves the imported values through the session writer',
                  z.read('model.set').decode().replace('\r\n', '\n').split('\n', 1)[1] == set0)
        # Saving failure after a valid import: values stay applied, one error.
        os.mkdir(os.path.join(r, 'blocked.snapx'))
        with open(os.path.join(r, 'blocked.set'), 'w') as f:
            f.write('\n'.join(own))
        answered((), cmd='set', kind='par', name='a', value=8)
        errs = errors(read_set('blocked.set'))
        check('W153: failed conversion save says so once and keeps valid imported values',
              len(errs) == 1 and 'saving session' in errs[0].get('error', '')
              and 'remain applied' in errs[0].get('error', '') and state() == st0, str(errs))
        errs = errors(answered((), cmd='values', op='internset', name='good'))
        st = state()
        check('W125: a good internal set still applies whole', not errs and st['pars'] != st0['pars'], str(errs))
    finally:
        stop_server(p, r, snd)


check_load_all_or_nothing()


# Nullclines, direction fields and flows as data (docs/protocol.md "The
# plot as data", docs/ui-v2.md T7), in plot coordinates.
def check_phase_data():
    proc5, run5, send5, collect5, _ = launch_server()

    def command(key, *answers):
        send5(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = collect5(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            send5(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

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
        evs = after({'cmd': 'data', 'events': ['nullclines', 'dfield']})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('data sends both at once, empty, for window 1',
              len(nc) == 1 and len(df) == 1 and nc[0]['win'] == 1 and nc[0]['x'] == [] and nc[0]['frozen'] == []
              and df[0]['grid'] == [] and df[0]['flows'] == [] and not of(evs, 'series'), str(evs)[:300])

        evs = command('n', {'key': 'n'})
        nc, v = of(evs, 'nullclines'), last_state(evs)['view']
        n0 = nc[0] if nc else {}
        check('Nullcline/New: the event names V and W, colours 2 and 7',
              len(nc) == 1 and n0['xname'] == 'V' and n0['yname'] == 'W' and n0['xcolor'] == 2 and n0['ycolor'] == 7,
              str(nc)[:300])
        why = (segments_ok(n0.get('x', []), v) or segments_ok(n0.get('y', []), v)) if n0 else 'no event'
        check('... its x- and y-nullclines are whole segments inside the view', not why, why)
        evs = after({'cmd': 'redraw'})
        check('a redraw draws them again and sends nothing', not of(evs, 'nullclines') and not of(evs, 'dfield'))

        evs = command('d', {'key': 's'}, {'value': '16'})
        df, v = of(evs, 'dfield'), last_state(evs)['view']
        d0 = df[0] if df else {}
        check('Dir.field/Scaled: dfield has a 17 x 17 grid, scaled, in the curve\'s colour',
              len(df) == 1 and d0['n'] == 17 and d0['scaled'] == 1 and d0['color'] == 0
              and len(d0['grid']) == 4 * 289 and len(d0['speed']) == 289, str(df)[:300])
        why = grid_ok(d0.get('grid', []), 17, v) if d0 else 'no event'
        check('... its arrows start on the 17 x 17 grid of the view', not why, why)
        check('... unit directions', all(abs(d0['grid'][k + 2] ** 2 + d0['grid'][k + 3] ** 2 - 1) < 1e-5
                                         for k in range(0, len(d0.get('grid', [])), 4)) if d0 else False)

        evs = command('n', {'key': 'f'}, {'key': 'f'})
        nc = of(evs, 'nullclines')
        ncx = nc[0]['x'] if nc else []
        check('Nullcline/Freeze/Freeze: the frozen nullclines are in the event',
              len(nc) == 1 and len(nc[0]['frozen']) == 1 and nc[0]['frozen'][0]['x'] == nc[0]['x']
              and nc[0]['frozen'][0]['y'] == nc[0]['y'], str(nc)[:300])

        evs = command('e')
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('Erase clears them: both events empty',
              len(nc) == 1 and nc[0]['x'] == [] and nc[0]['frozen'] == [] and len(df) == 1 and df[0]['grid'] == [],
              str(evs)[:300])
        evs = after({'cmd': 'redraw'})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('a redraw draws the nullclines again (not the field Erase turned off)',
              len(nc) == 1 and len(nc[0]['x']) > 0 and len(nc[0]['frozen']) == 1 and not df, str(evs)[:300])

        evs = command('d', {'key': 'f'}, {'value': '5'})
        df = of(evs, 'dfield')
        fl = df[0]['flows'] if df else []
        xs = fl[0]['x'] if fl else []
        starts = [0] + [i + 1 for i, a in enumerate(xs) if a is None]
        check('Dir.field/Flow: one curve of 72 trajectories (6 x 6, forward and back), finite points',
              len(fl) == 1 and fl[0]['color'] == 0 and len(starts) == 72 and len(xs) == len(fl[0]['y'])
              and all(a is None or abs(a) < 1e6 for a in xs), '%d curves, %d trajectories, %d points'
              % (len(fl), len(starts), len(xs)))
        check('... each trajectory starts on the grid',
              bool(fl) and all(abs((xs[s] + 0.6) / 0.36 - round((xs[s] + 0.6) / 0.36)) < 1e-4 for s in starts),
              str([xs[s] for s in starts][:8]))

        evs = after({'cmd': 'data', 'events': ['nullclines', 'dfield'], 'enc': 'f32'})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        same = False
        if nc and df and fl:
            same = (nc[0].get('enc') == 'f32' and df[0].get('enc') == 'f32'
                    and same_floats(values({'data': nc[0]['x']}, 'f32'), values({'data': ncx}, None))
                    and same_floats(values({'data': df[0]['flows'][0]['x']}, 'f32'), values({'data': xs}, None)))
        check('enc f32: the same values as base64 float32', same, str(evs)[:200])
        evs = after({'cmd': 'data', 'events': ['series']})
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
    send6(cmd='data', events=['plots'])
    collect6(is_idle)

    def view_keys(xlo, xhi, ylo, yhi):
        """Window/Window answered with the four numbers; returns (plots or
        None, message-error text or None, state.view) of the events up to idle"""
        send6(cmd='key', key='w')
        got = []
        answers = [{'key': 'w'}, {'ok': 1, 'values': [str(v) for v in (xlo, xhi, ylo, yhi)]}]
        while True:
            evs, e = collect6(lambda e: e.get('ev') in ('ask', 'idle'), timeout=30 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                break
            send6(cmd='answer', id=e['id'], **(answers.pop(0) if answers else {'ok': 0}))
        pl = next((e for e in got if e.get('ev') == 'plots'), None)
        msg = next((e.get('error') for e in got if e.get('ev') == 'message' and 'error' in e), None)
        st = last_state(got)
        return pl, msg, st and st.get('view')

    def axes(v):
        return v and (v['xlo'], v['xhi'], v['ylo'], v['yhi'])

    try:
        pl, msg, view = view_keys(-100, 100, -100, 100)
        w = pl and next((x for x in pl['windows'] if x['win'] == 1), None)
        check('Window/Window sets the axes of the window: plots reflects them',
              msg is None and axes(w) == (-100, 100, -100, 100), str(w))
        check('... and state.view matches, for the active window',
              view is not None and view['win'] == 1 and axes(view) == (-100, 100, -100, 100), str(view))

        # the command the page's "Use this view" button used to send is gone
        send6(cmd='view', win=1, xlo=-1, xhi=1, ylo=-1, yhi=1)
        evs, _ = collect6(is_idle)
        msg5 = next((e.get('error') for e in evs if e.get('ev') == 'message' and 'error' in e), None)
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
        send6(cmd='key', key='g')
        evs, ask = collect6(lambda e: e.get('ev') == 'ask')
        check('Graphic stuff opens its submenu (curves)',
              ask is not None and ask['kind'] == 'menu' and 'p' in ask.get('keys', ''), str(ask))
        if ask:
            send6(cmd='answer', id=ask['id'], key='p')
        evs, ask2 = collect6(lambda e: e.get('ev') == 'ask')
        check('Postscript asks for its PS parameters first',
              ask2 is not None and ask2['kind'] == 'form', str(ask2))
        if ask2:
            send6(cmd='answer', id=ask2['id'], ok=1, values=ask2['values'])
        evs, ask3 = collect6(lambda e: e.get('ev') == 'ask')
        check('... then a file to write it to (mode write, *.ps)',
              ask3 is not None and ask3['kind'] == 'file' and ask3.get('mode') == 'write'
              and ask3.get('wild') == '*.ps', str(ask3))
        psname = 't9view.ps'
        if ask3:
            send6(cmd='answer', id=ask3['id'], ok=1, file=psname)
        collect6(is_idle)
        pspath = os.path.join(run6, psname)
        ps = ''
        if os.path.exists(pspath):
            with open(pspath, encoding='latin-1') as f:
                ps = f.read()
        check('the PostScript file was written', ps != '')
        check("its axes use the new view: the boundary tick labels (-100, 100) appear in the file's text",
              '(-100) ' in ps and '(100) ' in ps, str(len(ps)))
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
    # Startup runnow has its own computing/idle cycle. Wait for that
    # condition, so the load's idle cannot be mistaken for the run's.
    collect7(lambda e: e.get('ev') == 'computing', timeout=30 * SLOW)
    evs0, _ = collect7(is_idle, timeout=30 * SLOW)
    view0 = last_state(evs0) and last_state(evs0)['view']
    check('lorenz.odex opens a 3D window at its @ phi=60 (theta the default 45)',
          view0 is not None and view0['three'] == 1 and view0['theta'] == 45 and view0['phi'] == 60, str(view0))

    send7(cmd='data', events=['plots'])
    collect7(is_idle)

    def turn(theta, phi):
        """key 3, its form answered with theta and phi (the rest as offered);
        returns (plots or None, message-error text or None, state.view)"""
        send7(cmd='key', key='3')
        got = []
        while True:
            evs, e = collect7(lambda e: e.get('ev') in ('ask', 'idle'), timeout=30 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                break
            vals = list(e.get('values', []))
            names = e.get('names', [])
            if e['kind'] == 'form' and 'Theta' in names and 'Phi' in names:
                vals[names.index('Theta')] = str(theta)
                vals[names.index('Phi')] = str(phi)
            send7(cmd='answer', id=e['id'], ok=1, values=vals)
        pl = next((e for e in got if e.get('ev') == 'plots'), None)
        msg = next((e.get('error') for e in got if e.get('ev') == 'message' and 'error' in e), None)
        st = last_state(got)
        return pl, msg, st and st.get('view')

    def angles(v):
        return v and (v['theta'], v['phi'])

    try:
        pl, msg, view = turn(10, -20)
        w = pl and next((x for x in pl['windows'] if x['win'] == 1), None)
        check('3d-params sets the angles of the window: plots reflects them',
              msg is None and w is not None and (w['theta'], w['phi']) == (10, -20), str(w))
        check('... and state.view.theta/phi matches, for the active window',
              view is not None and view['win'] == 1 and angles(view) == (10, -20), str(view))

        # the command the page used to send is gone
        send7(cmd='view3d', win=1, theta=0, phi=0)
        evs, _ = collect7(is_idle)
        msg2 = next((e.get('error') for e in evs if e.get('ev') == 'message' and 'error' in e), None)
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

    def command(key, answer):
        """a key, its asks answered by answer(ask, n) (n counts them), up to its idle"""
        send6(cmd='key', key=key)
        got, n = [], 0
        while True:
            evs, e = collect6(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            reply = answer(e, n) or {'ok': 0}
            n += 1
            send6(cmd='answer', id=e['id'], **reply)

    def keys(*replies):
        """answers in order: a str is a menu key, a dict the answer itself; then cancel"""
        def answer(e, n):
            if n >= len(replies):
                return None
            return {'key': replies[n]} if isinstance(replies[n], str) else replies[n]
        return answer

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
        evs = after({'cmd': 'data', 'events': ['marks', 'plots', 'series']})
        m = marks_of(evs)
        check('data sends marks at once, empty, for window 1',
              m is not None and m['win'] == 1 and m['equilibria'] == [] and m['text'] == [] and m['arrows'] == []
              and m['markers'] == [] and m['frozen'] == [], str(of(evs, 'marks'))[:300])
        curve = of(evs, 'plots')[0]['windows'][0]['curves'][0]

        evs = command('i', keys('g'))
        ser = whole_series(evs)
        v = last_state(evs)['view']
        px = (v['xhi'] - v['xlo']) / abs(v['right'] - v['left']) * 1.5
        py = (v['yhi'] - v['ylo']) / abs(v['bottom'] - v['top']) * 1.5
        check('an integration sends no marks', bool(ser) and not of(evs, 'marks'))
        cols = {c['col']: c['data'] for c in ser[-1]['columns']} if ser else {}

        typed = r'\1a\0-point \{2*3}'
        evs = command('t', keys('t', {'value': typed}, {'value': '3'}, {'xd': -0.3, 'yd': 0.5}))
        m = marks_of(evs)
        t = m['text'] if m else []
        check('Text,etc/Text: a text mark with its text (\\{expr} filled in), size and position',
              len(t) == 1 and t[0]['text'] == r'\1a\0-point 6' and t[0]['size'] == 3 and t[0]['font'] == 0
              and near(t[0]['x'], -0.3, px) and near(t[0]['y'], 0.5, py), str(m)[:300])

        evs = command('t', keys('p', {'value': '0.2'}, {'value': '5'},
                                {'xd': 0.1, 'yd': 0.2, 'xd2': 0.6, 'yd2': 0.8}))
        m = marks_of(evs)
        a = m['arrows'] if m else []
        check('Text,etc/Pointer: an arrow mark from the first point to the second, its size and colour',
              len(a) == 1 and a[0]['kind'] == 'pointer' and a[0]['size'] == 0.2 and a[0]['color'] == 5
              and near(a[0]['x1'], 0.1, px) and near(a[0]['y1'], 0.2, py) and near(a[0]['x2'], 0.6, px)
              and near(a[0]['y2'], 0.8, py) and len(m['text']) == 1, str(m)[:300])

        evs = command('t', keys('m', {'values': ['3', '7', '2']}, {'xd': 0.9, 'yd': 0.1}))
        m = marks_of(evs)
        k = m['markers'] if m else []
        check('Text,etc/Marker: a marker mark with its shape, size, colour and position',
              len(k) == 1 and k[0]['shape'] == 'diamond' and k[0]['size'] == 2 and k[0]['color'] == 7
              and near(k[0]['x'], 0.9, px) and near(k[0]['y'], 0.1, py), str(m)[:300])

        evs = command('g', keys('f', 'f', {'values': ['4', 'first run', 'frz1']}))
        m = marks_of(evs)
        f = m['frozen'] if m else []
        check('Graphic stuff/Freeze: a frozen curve equal to the series at freeze time, its key and colour',
              len(f) == 1 and f[0]['key'] == 'first run' and f[0]['name'] == 'frz1' and f[0]['color'] == 4
              and f[0]['line'] == 1 and values({'data': f[0]['x']}, None) == cols.get(curve['x']) and values({'data': f[0]['y']}, None) == cols.get(curve['y'])
              and len(f[0]['x']) > 2, str(m)[:300])

        got = command('s', lambda e, n: {'key': 'g' if n == 0 else 'n'})
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

        evs = after({'cmd': 'redraw'})
        m = marks_of(evs)
        check('a redraw draws text, objects and frozen curves again, not the equilibrium (as the classic window)',
              m is not None and m['equilibria'] == [] and len(m['text']) == 1 and len(m['arrows']) == 1
              and len(m['markers']) == 1 and len(m['frozen']) == 1, str(m)[:300])
        evs = after({'cmd': 'redraw'})
        check('... and a second one sends nothing', not of(evs, 'marks'))

        evs = after({'cmd': 'data', 'events': ['marks'], 'enc': 'f32'})
        m = marks_of(evs)
        check('enc f32: the frozen curve as base64 float32, the same values',
              m is not None and m.get('enc') == 'f32' and len(m['frozen']) == 1 and bool(f)
              and same_floats(values({'data': m['frozen'][0]['x']}, 'f32'), values({'data': f[0]['x']}, None)),
              str(m)[:200])
        after({'cmd': 'data', 'events': ['marks']})

        evs = command('g', keys('f', 'r'))
        m = marks_of(evs)
        check('Freeze/Remove all: the frozen curve goes at once', m is not None and m['frozen'] == []
              and len(m['text']) == 1, str(m)[:300])

        evs = command('e', keys())
        m = marks_of(evs)
        check('Erase clears every mark',
              m is not None and not any(m[k] for k in ('equilibria', 'text', 'arrows', 'markers', 'frozen')),
              str(m)[:300])
        evs = after({'cmd': 'redraw'})
        m = marks_of(evs)
        check('... a redraw brings the text and objects back', m is not None and len(m['text']) == 1
              and len(m['arrows']) == 1 and len(m['markers']) == 1, str(m)[:300])
        evs = command('t', keys('d'))
        m = marks_of(evs)
        check('Text,etc/Delete all deletes them', m is not None and m['text'] == [] and m['arrows'] == []
              and m['markers'] == [], str(m)[:300])
        evs = after({'cmd': 'data', 'events': ['series']})
        evs += command('s', lambda e, n: {'key': 'g' if n == 0 else 'n'})
        check('not asked for: no marks', not of(evs, 'marks'))
    finally:
        stop_server(proc6, run6, send6)


def check_ani_data():
    """The animation as data (docs/protocol.md "The animation as data",
    docs/ui-v2.md T13): tools/gui_test.ani (one of every command) on lecar.
    Each frame event's primitives are in unit coordinates of the dimension
    box, colours as XPP indices or #rrggbb; the player's step, seek and Go
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
            evs, e = collect6(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            send6(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    def command(key, *answers):
        send6(cmd='key', key=key)
        return answered(list(answers))

    ani_keys = {'file': 'f', 'go': 'g', 'reset': 'r', 'skip': 's', 'mpeg': 'm', 'fly': 'o', 'grab': 'a'}

    def ani(op, *answers, **kw):
        """the animation window's key (menu_ani_window) when it has one, else its ani op"""
        if op in ani_keys:
            send6(cmd='key', win='ani', key=ani_keys[op])
        else:
            send6(cmd='ani', op=op, **kw)
        return answered(list(answers))

    def colours_ok(f):
        """'' when every primitive's colour is an XPP index (0..10) or a colour map's #rrggbb, else why"""
        for p in f['prims']:
            c = p[4] if p[0] in ('dot', 'text') else p[5]
            if not ((isinstance(c, int) and 0 <= c <= 10) or (isinstance(c, str) and len(c) == 7 and c[0] == '#')):
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
        send6(cmd='data', events=['ani'])
        evs, _ = collect6(is_idle)
        check('ani data: no frame before one is drawn', not frames(evs))
        command('i', {'key': 'g'})
        evs = command('v', {'key': 't'})
        check('ani data: Viewaxes/Toon opens the animation window',
              any(e.get('ev') == 'window' and e.get('win') == 104 and e.get('op') == 'create' for e in evs))
        evs = ani('file', {'file': 'gui_test.ani'})
        fr, st = frames(evs), states(evs)
        check('ani data: loading an animation shows its first frame', len(fr) == 1 and fr[0]['pos'] == 0
              and fr[0]['rows'] == 601 and bool(st) and st[-1]['loaded'] == 1, str(evs)[-400:])
        if not fr:
            return
        f0 = fr[0]
        check('ani data: the frame names its box, window, speed and time',
              f0['dim'] == [-0.6, -0.1, 0.4, 0.6] and (f0['w'], f0['h']) == (280, 350) and f0['speed'] == 5
              and f0['skip'] == 1 and f0['t'] == 0, str({k: v for k, v in f0.items() if k != 'prims'}))
        kinds = sorted({p[0] for p in f0['prims']})
        check('ani data: every kind of primitive (line, rect, circle, ellipse, dot, text)',
              kinds == ['circle', 'dot', 'ellipse', 'line', 'rect', 'text'], str(kinds))
        check('ani data: frame 0 has a primitive per drawing command of the .ani, colours as indices or #rrggbb',
              len(f0['prims']) == 13 and not colours_ok(f0), '%d primitives %s' % (len(f0['prims']), colours_ok(f0)))
        check('ani data: its coordinates are in [0,1] (only a line may leave the box)',
              in_unit(f0) is None, str(in_unit(f0)))
        spectral = [p for p in f0['prims'] if p[0] == 'circle' and isinstance(p[5], str)]
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

        # grab: the grab point's cross (the last two black lines), hit in unit coordinates
        evs = ani('grab')
        fr = frames(evs)
        check('ani data: Grab shows the grab points', bool(fr) and states(evs)[-1]['grab'] == 1, str(states(evs)))
        cross = [p for p in (fr[-1] if fr else f0)['prims'] if p[0] == 'line' and p[5] == 0][-2:]
        if len(cross) == 2:
            cx, cy = (cross[1][1] + cross[1][3]) / 2, (cross[1][2] + cross[1][4]) / 2
            ani('mouse', what='down', u=cx, v=cy)
            evs = ani('mouse', what='up', u=cx, v=cy)
            check('ani data: a press and release at the point in unit coordinates (u, v) grabs it',
                  bool(states(evs)) and states(evs)[-1]['grab'] == 0, str(states(evs)))
        else:
            check('ani data: a press and release at the point in unit coordinates (u, v) grabs it', False,
                  'no grab cross drawn')

        send6(cmd='data', events=[])
        collect6(is_idle)
        evs = ani('step', n=1)
        check('ani data: not asked for, no frame is sent', not frames(evs) and bool(states(evs)), str(evs)[:200])
    finally:
        stop_server(proc6, run6, send6)

check_view()
check_view3d()
check_marks()
check_ani_data()
# AUTO's info strip and stability circle as data (docs/protocol.md "The AUTO
# diagram as data", `autoinfo`), and a grab answered with a point of the
# diagram's data (`point`). lecar's steady-state branch from its "hopf" fixed
# point, then the periodic branch from the Hopf point, as
# examples/scripts/lecar_auto.jsonl does it.
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


def rebuild_diagram(evs, pts, view=0):
    """view `view`'s diagram data after these events: (br, pt, ty, x, y, y2, from) per point"""
    for e in evs:
        if e.get('ev') != 'diagram' or e.get('view') != view:
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
    """the "hopf" set, its fixed point as the IC, File/Auto, Run a steady state: the events"""
    def step(**c):
        snd(**c)
        return col(lambda e: is_idle(e) or e.get('ev') == 'ask', timeout=30 * SLOW)
    for c in ({'cmd': 'key', 'key': 'f'}, {'cmd': 'key', 'key': 'g'}, {'cmd': 'answer', 'key': 'd'},
              {'cmd': 'key', 'key': 's'}, {'cmd': 'answer', 'key': 'g'}, {'cmd': 'answer', 'key': 'n'},
              {'cmd': 'key', 'win': 'equilibrium', 'key': 'i'}, {'cmd': 'key', 'key': 'f'}, {'cmd': 'key', 'key': 'a'}, {'cmd': 'key', 'win': 'auto', 'key': 'r'}):
        step(**c)
    snd(cmd='answer', key='s')
    evs, _ = col(is_idle, timeout=60 * SLOW)
    return evs


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
        snda(cmd='data', events=['autoinfo'])
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

        snda(cmd='key', win='auto', key='g')
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
        check('autoinfo: Tab to the Hopf point: its strip', info is not None and info['sym'] == 'HB'
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
        sndb(cmd='key', win='auto', key='g')
        for k in ['Tab', 'Return']:
            evs, ask = colb(lambda e: e.get('ev') == 'ask')
            sndb(cmd='answer', id=ask['id'], key=k)
        colb(is_idle)
        run_evs = []
        for snd, col, diag in ((snda, cola, diag_a), (sndb, colb, diag_b)):
            snd(cmd='key', win='auto', key='r')
            evs, ask = col(lambda e: e.get('ev') == 'ask', timeout=30 * SLOW)
            snd(cmd='answer', id=ask['id'], key='p')
            evs, _ = col(is_idle, timeout=120 * SLOW)
            rebuild_diagram(evs, diag)
            run_evs = run_evs or evs
        per = [p for p in diag_a if p[0] == 2]
        check('grab by point, then Run: the periodic branch is the one grabbing by keys gives',
              len(per) > 10 and diag_a == diag_b, '%d periodic points; %d vs %d points' % (len(per), len(diag_a), len(diag_b)))
        check('diagram: the periodic branch says it started from the Hopf label (from)',
              per and per[0][6] == hb['lab'] and all(p[6] == 0 for p in per[1:]), str(per[:2]))
        got = infos(run_evs)
        stab = got[-1]['stab'] if got else None
        pr = printed_stability(pa).get(per[-1][:2]) if per else None
        check("autoinfo: after the periodic run, the circle holds its last point's Floquet multipliers, as AUTO printed them",
              stab is not None and stab['periodic'] == 1 and 'eig' not in stab and close_pairs(stab['circle'], pr, 2e-5)
              and any(abs(complex(*z) - 1) < 1e-3 for z in stab['circle']), '%s vs %s' % (stab, pr))
        snda(cmd='redraw')
        evs, _ = cola(is_idle)
        check('autoinfo: a redraw sends none (nothing it shows changed, nor on its way)', not infos(evs),
              '%d events, the first %s' % (len(infos(evs)), str(infos(evs)[:1])[:300]))
        # a redraw after a grab: the strip and the circle stay the grabbed point's (W49: the circle
        # used to move to the diagram's last point, under the grabbed point's info)
        snda(cmd='key', win='auto', key='g')
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        snda(cmd='answer', id=ask['id'], point=5, key='Return')
        evs, _ = cola(is_idle)
        snda(cmd='redraw')
        evs, _ = cola(is_idle)
        sent = infos(evs)
        snda(cmd='data', events=['autoinfo'])  # sends what it holds, changed or not
        evs, _ = cola(is_idle)
        got = infos(evs)
        info, stab = (got[-1]['info'], got[-1]['stab']) if got else (None, None)
        pr = printed.get((info['br'], info['pt'])) if info else None
        check("autoinfo: a redraw after a grab sends none, and the strip and circle stay the grabbed point's",
              not sent and info is not None and info['point'] == 5 and strip_matches(info, diag_a) is None
              and stab is not None and stab['periodic'] == 0 and close_pairs(stab['eig'], pr, 2e-5),
              '%d sent; %s %s vs %s' % (len(sent), info, stab, pr))
        snda(cmd='auto', op='point', xd=0.125, yd=-0.25)
        evs, _ = cola(is_idle)
        hint = [e.get('auto') for e in evs if e.get('ev') == 'message' and 'auto' in e]
        check('auto point in data coordinates shows exactly those', hint and hint[-1] == 'x=0.125,y=-0.25', str(hint))
        snda(cmd='auto', op='close')
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
            for k in keys:
                snd(cmd='key', key=k)
            evs, ask = col(lambda e: e.get('ev') == 'ask' and e.get('kind') == 'file', timeout=30 * SLOW)
            d = None
            if ask:
                d = ask.get('dir')
                snd(cmd='answer', id=ask['id'], file='')
                col(is_idle, timeout=30 * SLOW)
            return ask, d
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
        snd(cmd='record', op='start')
        col(is_idle, timeout=30 * SLOW)
        snd(cmd='record', op='note', text='note')
        col(is_idle, timeout=30 * SLOW)
        snd(cmd='record', op='stop', name='dlg')
        col(is_idle, timeout=30 * SLOW)
        snd(cmd='play', op='open', file='dlg.recx')
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
        snd(cmd='key', key='f')
        snd(cmd='key', key='y')
        _, ask = col(lambda e: e.get('ev') == 'ask' and e.get('kind') == 'file', timeout=30 * SLOW)
        snd(cmd='answer', id=ask['id'], file=os.path.join(sub, 'x.recx'))
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


NUM_KEYS = ['ntst', 'nmx', 'npr', 'ncol', 'ds', 'dsmin', 'dsmax', 'rl0', 'rl1', 'a0', 'a1', 'epsl', 'epsu', 'epss',
            'iad', 'mxbf', 'iid', 'itmx', 'itnw', 'nwtn', 'iads', 'suppbp']


def form_text(v):
    """a value as the core's forms print it: %d for whole numbers, else %g"""
    return '%d' % v if isinstance(v, int) else '%g' % v


def check_autosettings():
    pa, ra, snda, cola, _ = launch_server()
    pb, rb, sndb, colb, _ = launch_server()
    try:
        cola(is_idle)
        colb(is_idle)

        def ask_of(**c):
            snda(**c)
            return cola(lambda e: e.get('ev') == 'ask' or is_idle(e))[1]

        def cancel(ask):
            snda(cmd='answer', id=ask['id'], ok=0)
            return cola(is_idle)[0]

        def auto_set(**c):
            snda(cmd='auto', op='set', **c)
            evs, _ = cola(is_idle)
            return evs, settings_of(evs), [e['error'] for e in evs if e.get('ev') == 'message' and 'error' in e]

        snda(cmd='data', events=['autosettings'])
        evs, _ = cola(is_idle)
        st = settings_of(evs)
        check('autosettings: sent at once after data, before AUTO is open', st is not None
              and sorted(st['numerics']) == sorted(NUM_KEYS) and len(st['pars']) == 8
              and st['axes']['par1'] == st['pars'][0], str(st)[:300])
        rules = st.get('rules', {}) if st else {}
        check('W118: autosettings carries each Numerics rule with the message its refusal says, the pairs and the step',
              sorted(rules) == sorted(NUM_KEYS) and rules['ncol'].get('min') == 2 and rules['ncol'].get('max') == 7
              and rules['ncol'].get('integer') is True and rules['dsmin'].get('positive') is True
              and rules['ds'].get('nonzero') is True and 'min' not in rules['mxbf']
              and rules['ncol'].get('message') == 'Ncol must be a whole number from 2 to 7'
              and [(p['lo'], p['hi'], p['strict']) for p in st.get('pairs', [])]
              == [('dsmin', 'dsmax', False), ('rl0', 'rl1', True), ('a0', 'a1', True)]
              and st.get('step', {}).get('key') == 'ds', str(st and (st.get('pairs'), st.get('step')))[:300])
        ask = ask_of(cmd='key', win='auto', key='n')
        cancel(ask)
        want = [form_text(st['numerics'][k]) for k in NUM_KEYS]
        check("autosettings: the numerics are the Numerics form's values, field for field",
              ask and ask['kind'] == 'form' and ask['values'] == want, '%s vs %s' % (ask and ask['values'], want))
        ask = ask_of(cmd='key', win='auto', key='p')
        cancel(ask)
        check("autosettings: pars are the Parameter form's names", ask and ask['values'] == st['pars'],
              '%s vs %s' % (ask and ask['values'], st['pars']))
        plot_key = {0: 'h', 1: 'n', 2: 'i', 3: 'p', 4: 't', 10: 'r', 11: 'a'}[st['axes']['plot']]
        menu_ask = ask_of(cmd='key', win='auto', key='a')
        snda(cmd='answer', id=menu_ask['id'], key=plot_key)
        ask = cola(lambda e: e.get('ev') == 'ask')[1]
        cancel(ask)
        a = st['axes']
        want = [a['var'], a['par1'], a['par2']] + [form_text(a[k]) for k in ('xmin', 'ymin', 'xmax', 'ymax')]
        check("autosettings: axes are the Axes menu's plot type and the AutoPlot form's values",
              ask and ask['kind'] == 'form' and ask['values'] == want, '%s vs %s' % (ask and ask['values'], want))
        ask = ask_of(cmd='key', win='auto', key='u')
        snda(cmd='answer', id=ask['id'], key='0')
        cola(is_idle)
        check('autosettings: no Mark values at first, as the form says', st['marks'] == [], str(st['marks']))

        # a form's OK shows in the event too
        ask = ask_of(cmd='key', win='auto', key='n')
        vals = list(ask['values'])
        vals[NUM_KEYS.index('npr')] = '40'
        snda(cmd='answer', id=ask['id'], ok=1, values=vals)
        evs, _ = cola(is_idle)
        st2 = settings_of(evs)
        check('autosettings: a Numerics form answered: the event follows (NPr 40)', st2 and st2['numerics']['npr'] == 40,
              str(st2)[:200])

        # the set: every part, then read back through the forms
        p0, p1 = st['pars'][0], st['pars'][1]
        evs, st3, errs = auto_set(numerics={'nmx': 12, 'ds': -0.01}, pars=[p1, p0], axes={'plot': 1, 'var': 'W', 'par1': p1},
                                  marks=[[p0, 0.25], ['T', 30]])
        check('auto set: numerics, pars, axes and marks set, the event says so', not errs and st3
              and st3['numerics']['nmx'] == 12 and st3['numerics']['ds'] == -0.01 and st3['pars'][:2] == [p1, p0]
              and st3['axes']['plot'] == 1 and st3['axes']['var'] == 'W' and st3['axes']['par1'] == p1
              and st3['marks'] == [[p0, 0.25], ['T', 30]], '%s %s' % (errs, st3))
        ask = ask_of(cmd='key', win='auto', key='n')
        cancel(ask)
        check('auto set: the Numerics form shows it', ask and ask['values'][1] == '12' and ask['values'][4] == '-0.01',
              str(ask and ask['values']))
        ask = ask_of(cmd='key', win='auto', key='u')
        snda(cmd='answer', id=ask['id'], key='2')
        ask2 = cola(lambda e: e.get('ev') == 'ask')[1]
        cancel(ask2)
        check('auto set: the Mark values form shows it', ask2 and ask2['values'][:2] == ['%s=0.25' % p0, 'T=30'],
              str(ask2 and ask2['values']))

        # refused: bad values change nothing, and say why
        for bad, why in (({'numerics': {'ncol': 9}}, 'Ncol'), ({'numerics': {'ntst': 1.5}}, 'Ntst'),
                         ({'numerics': {'nmx': 30, 'dsmin': 0}}, 'Dsmin'), ({'numerics': {'rl0': 5, 'rl1': 1}}, 'Par Min'),
                         ({'numerics': {'ds': -0.9, 'dsmax': 0.5}}, 'Ds must be from Dsmin to Dsmax'),
                         ({'numerics': {'dsmin': 0.5, 'dsmax': 1, 'ds': 0.1}}, 'Ds must be from Dsmin to Dsmax'),
                         ({'numerics': {'nmx': 'many'}}, 'Nmax'), ({'pars': ['nosuch']}, 'nosuch'),
                         ({'axes': {'var': 'nosuch'}}, 'nosuch'), ({'axes': {'plot': 7}}, 'plot'),
                         ({'axes': {'xmin': 1, 'xmax': 0}}, 'Xmin'), ({'marks': [['W', 1]]}, 'W')):
            evs, st4, errs = auto_set(**bad)
            check('auto set %s is refused with an error naming %s, nothing changes' % (json.dumps(bad), why),
                  len(errs) == 1 and why in errs[0] and st4 is None, '%s %s' % (errs, st4))
        evs, st4, errs = auto_set(numerics={'ncol': 9})
        check('W118: a refused value says the message the event gave its rule',
              errs == ['AUTO settings: ' + rules.get('ncol', {}).get('message', '?')], str(errs))

        # a set sent while a question is open is kept for after the command, not dropped: a
        # command of its own then (W106), with its own idle
        ask = ask_of(cmd='key', win='auto', key='n')
        snda(cmd='auto', op='set', numerics={'nmx': 77})
        evs = cancel(ask)
        st5a = settings_of(evs)
        st5 = settings_of(cola(is_idle)[0])
        check('auto set during a question: applied after the command, with its own idle',
              st5a is None and st5 and st5['numerics']['nmx'] == 77, str(st5)[:200])

        # a set changes the next run: Nmax 12 against the default on the other server
        evs, st6, errs = auto_set(numerics={'nmx': 12}, pars=[p0, p1], axes={'plot': 2, 'var': 'V', 'par1': p0}, marks=[])
        evs_a = lecar_to_auto(snda, cola)
        evs_b = lecar_to_auto(sndb, colb)
        na, nb = len(rebuild_diagram(evs_a, [])), len(rebuild_diagram(evs_b, []))
        check('auto set: Nmax 12 makes the next run stop at 12 points (the default goes on to %d)' % nb,
              not errs and na == 12 and nb > 12, '%s: %d vs %d points' % (errs, na, nb))
        # W155: Reload keeps AUTO's settings without a settings file.
        before_reload = settings_of(auto_set(numerics={'nmx': 41})[0])
        snda(cmd='reload')
        reload_ask = cola(lambda e: e.get('ev') == 'ask')[1]
        snda(cmd='answer', id=reload_ask['id'], key='d')
        cola(is_idle)
        snda(cmd='data', events=['autosettings'])
        after_reload = settings_of(cola(is_idle)[0])
        check('W155: Reload keeps AUTO numerics, parameters, axes and marks without a settings file',
              before_reload is not None and after_reload is not None
              and all(before_reload[k] == after_reload[k] for k in ('numerics', 'pars', 'axes', 'marks')),
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
        snd(cmd='data', events=['autoinfo'])
        col(is_idle)
        for k in 'fa':
            snd(cmd='key', key=k)
            col(lambda e: is_idle(e) or e.get('ev') == 'ask')
        snd(cmd='auto', op='set', numerics=numerics)
        evs, _ = col(is_idle)
        errs = [e['error'] for e in evs if e.get('ev') == 'message' and 'error' in e]
        snd(cmd='key', win='auto', key='r')
        evs, ask = col(lambda e: e.get('ev') == 'ask' or is_idle(e))
        if errs or not ask or ask.get('ev') != 'ask':
            return {'errors': errs, 'ask': ask}, 0
        snd(cmd='answer', id=ask['id'], key='s')
        evs = []
        if abort:
            evs, _ = col(lambda e: e.get('ev') == 'diagram' and e.get('op') == 'add', timeout=30 * SLOW)
            snd(cmd='abort')
        more, _ = col(is_idle, timeout=60 * SLOW)
        evs += more
        got = infos(evs)
        return (got[-1].get('stop') if got else None), len(rebuild_diagram(evs, []))
    finally:
        stop_server(p, r, snd)


def check_auto_stop():
    wide = {'rl0': -1, 'rl1': 0.9, 'a0': -1, 'a1': 100, 'nmx': 200}
    for numerics, why, text, abort in (
            ({**wide, 'rl1': 0.5}, 'parmax', 'parameter a reached Par Max (0.5)', False),
            ({**wide, 'rl0': -0.3, 'ds': -0.01}, 'parmin', 'parameter a reached Par Min (-0.3)', False),
            ({**wide, 'a1': 0.3}, 'normmax', 'the norm reached Norm Max (0.3)', False),
            ({**wide, 'nmx': 5}, 'npts', 'the branch reached Max points (NMX 5)', False),
            ({**wide, 'rl1': 10}, 'noconv-min', 'no convergence even at the smallest step (Dsmin 0.001)', False),
            ({**wide, 'rl1': 10, 'iads': 0}, 'noconv-fixed', 'no convergence with a fixed step size (IADS 0)', False),
            ({**wide, 'nmx': 1000000, 'ds': 1e-7, 'dsmin': 1e-8, 'dsmax': 1e-7}, 'user', 'by the user (Stop)', True)):
        stop, n = auto_stop_run(numerics, abort)
        ok = isinstance(stop, dict) and stop.get('why') == why and stop.get('text') == text and stop.get('br') == 1
        if ok and why in ('parmax', 'parmin', 'normmax'):
            ok = abs(stop['limit'] - numerics[{'parmax': 'rl1', 'parmin': 'rl0', 'normmax': 'a1'}[why]]) < 1e-9 and (
                stop['value'] < stop['limit'] if why == 'parmin' else stop['value'] > stop['limit'])
        if ok and why == 'npts':
            ok = stop['pt'] == 5 and n == 5
        if ok and why == 'user':
            ok = 1 < stop['pt'] < 1000000
        check('autoinfo stop: %s ends the branch with "%s"' % (why, text), ok, '%s (%d points)' % (stop, n))
    # a run that starts where the limit already is: Par Min below the start
    stop, _ = auto_stop_run({**wide, 'rl0': 0.2, 'rl1': 0.9})
    check('autoinfo stop: a start beyond Par Min ends at its first point',
          isinstance(stop, dict) and stop.get('why') == 'parmin' and stop.get('pt') == 1, str(stop))


check_auto_stop()


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
    wide = {'rl0': -1, 'rl1': 0.9, 'a0': -1, 'a1': 100, 'nmx': 200}
    p, r, snd, col, evq = launch_server(ode=STOP_ODE)
    try:
        col(is_idle)
        snd(cmd='data', events=['autoinfo'])
        col(is_idle)
        for k in 'fa':
            snd(cmd='key', key=k)
            col(lambda e: is_idle(e) or e.get('ev') == 'ask')
        snd(cmd='auto', op='set', numerics={**wide, 'rl1': 10})
        col(is_idle)
        snd(cmd='key', win='auto', key='r')
        _, ask = col(lambda e: e.get('ev') == 'ask' or is_idle(e))
        ok = isinstance(ask, dict) and ask.get('ev') == 'ask'
        states = []
        if ok:
            snd(cmd='answer', id=ask['id'], key='s')
            evs, _ = col(is_idle, timeout=60 * SLOW)
            bad = [e for e in evs if e.get('ev') == 'bad']
            states = [e for e in evs if e.get('ev') == 'state']
            pars = states[-1].get('pars', []) if states else []
            finite = all(math.isfinite(v) for _, v in pars)
            ok = not bad and bool(states) and finite
            detail = str(bad) if bad else str(pars)
        else:
            detail = str(ask)
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

    def browser(**kw):
        """a browser command, a File exists? Overwrite ask answered yes"""
        snd(cmd='browser', **kw)
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
            _, e = browser(op='write', what='table', format=fmt, name=name)
            check('Save data writes the table as %s without asking' % fmt,
                  e is not None and is_idle(e) and os.path.exists(os.path.join(r, name)), str(e))
        silent = tempfile.mkdtemp(prefix='xppsilent')
        shutil.copy(args.ode, silent)
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
        browser(op='write', what='plot', format='csv', name='p.csv')
        pc = read(r, 'p.csv').decode().splitlines() or ['']
        if pc and pc[0].startswith('# seed '):
            pc = pc[1:]
        check('Save data writes what the plot shows: curve,x,y, a row per point',
              pc[0] == 'curve,x,y' and len(pc) == 602 and pc[1].split(',')[0] == '1'
              and [float(x) for x in pc[1].split(',')[1:]] == vals[0][1:3], str(pc[:2]))
        browser(op='write', what='plot', format='npz', name='p.npz')
        try:
            arrays = npz_arrays(os.path.join(r, 'p.npz'))
            arrays.pop('seed', None)
            ok, detail = list(arrays) == ['curve1'] and arrays['curve1'][0] == (601, 2), str(arrays.keys())
        except Exception as ex:
            ok, detail = False, repr(ex)
        check('what the plot shows as NPZ: one (points, 2) array per curve', ok, detail)

        # the dialog: what, then the format from the registry, then the file
        snd(cmd='browser', op='write')
        asks = []
        replies = {'menu': lambda e: {'key': 'p' if e.get('name') == 'save_what' else 'g'},
                   'file': lambda e: {'file': 'asked.csv.gz'}}
        while True:
            _, e = col(lambda e: e.get('ev') == 'ask' or is_idle(e), timeout=20 * SLOW)
            if e is None or is_idle(e):
                break
            asks.append(e)
            snd(cmd='answer', id=e['id'], **(replies[e['kind']](e) if e['kind'] in replies else {'ok': 0}))
        fmt_menu = [a for a in asks if a.get('name') == 'save_format']
        check('Save data asks what, then the format (every registered one), then the file',
              [a.get('name', a['kind']) for a in asks] == ['save_what', 'save_format', 'file']
              and fmt_menu and len(fmt_menu[0]['items']) == 4 and fmt_menu[0]['keys'] == 'dcgn'
              and asks[-1].get('wild') == '*.csv.gz'
              and gzip.decompress(read(r, 'asked.csv.gz')) == read(r, 'p.csv'),
              str([(a['kind'], a.get('name'), a.get('wild')) for a in asks]))

        # Graphic stuff > exp(O)rt is the same Save data of what the plot shows
        snd(cmd='key', key='g')
        asks = []
        while True:
            _, e = col(lambda e: e.get('ev') == 'ask' or is_idle(e), timeout=20 * SLOW)
            if e is None or is_idle(e):
                break
            asks.append(e)
            reply = ({'key': 'o'} if e.get('name') == 'curves' else
                     {'menu': {'key': 'c'}, 'file': {'file': 'export.csv'}}.get(e['kind'], {'ok': 0}))
            snd(cmd='answer', id=e['id'], **reply)
        check('Graphic stuff/exp(O)rt asks the format and the file and writes what the plot shows',
              [a.get('name', a['kind']) for a in asks] == ['curves', 'save_format', 'file']
              and read(r, 'export.csv') == read(r, 'p.csv'), str([(a['kind'], a.get('name')) for a in asks]))

        # Load reads each format back into the table, picked by its extension
        for name in ['d.csv', 'd.csv.gz', 'd.npz', 'd.dat']:
            snd(cmd='browser', op='first', row=300)
            col(is_idle)
            browser(op='write', what='table', format='dat', name='cut.dat')  # rows 300.. only
            browser(op='load', name='cut.dat')
            evs, _ = browser(op='load', name=name)
            snd(cmd='browser', op='first', row=0)
            col(is_idle)
            browser(op='write', what='table', format='dat', name='back.dat')
            check('Load reads %s back into the table (rows and values)' % name,
                  read(r, 'back.dat').replace(b'\r', b'') == want and last_state(evs).get('rows') == 601,
                  str(last_state(evs).get('rows')))
    finally:
        stop_server(p, r, snd)


check_data_formats()


def check_save_owner():
    """W129: the permission belongs to the command; only commit delivers."""
    log = []
    p, r, snd, col, _ = launch_server(log=log)
    errors = lambda evs: [e for e in evs if e.get('ev') == 'message' and e.get('error')]
    results = lambda evs: [e for e in evs if e.get('ev') == 'saved']
    try:
        col(is_idle)
        snd(cmd='browser', op='write', what='table', format='csv', replace=0)
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W129: nothing to save reports once before asking for a name',
              end is not None and is_idle(end) and len(errors(evs)) == 1
              and errors(evs)[0]['error'] == 'Nothing to save'
              and results(evs) == [{'ev': 'saved', 'saved': False, 'file': ''}], str(evs)[-400:])
        live_run(snd, col)
        snd(cmd='values', op='write', kind='par', name='same.par', replace=0)
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W129: a new file saves without a question or a command decision',
              end is not None and is_idle(end)
              and results(evs) == [{'ev': 'saved', 'saved': True, 'file': 'same.par'}]
              and not errors(evs), str(evs)[-300:])
        before = open(os.path.join(r, 'same.par'), 'rb').read()
        snd(cmd='set', kind='par', name='iapp', value=0.11)
        col(is_idle)
        snd(cmd='values', op='write', kind='par', name='same.par', replace=0)
        _, ask = col(is_ask)
        check('W129: an existing target without a decision asks once',
              ask is not None and ask.get('question') == 'same.par exists. Replace it?'
              and ask.get('keys') == 'yn', str(ask))
        snd(cmd='answer', id=ask['id'], key='n')
        evs, _ = col(is_idle)
        check('W129: No preserves the file and emits saved false, no error',
              before == open(os.path.join(r, 'same.par'), 'rb').read()
              and results(evs) == [{'ev': 'saved', 'saved': False, 'file': 'same.par'}]
              and not errors(evs), str(evs)[-300:])
        snd(cmd='values', op='write', kind='par', name='same.par', replace=0)
        _, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], key='y')
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W129: Yes replaces the existing file without a second question',
              end is not None and is_idle(end) and not errors(evs)
              and results(evs) == [{'ev': 'saved', 'saved': True, 'file': 'same.par'}]
              and before != open(os.path.join(r, 'same.par'), 'rb').read(), str(evs)[-300:])
        before = open(os.path.join(r, 'same.par'), 'rb').read()
        snd(cmd='values', op='write', kind='par', name='same.par', replace=1)
        evs, end = col(lambda e: is_idle(e) or is_ask(e))
        check('W129: replace 1 overwrites an existing target without a question',
              end is not None and is_idle(end) and not errors(evs)
              and results(evs) == [{'ev': 'saved', 'saved': True, 'file': 'same.par'}], str(end))
        before = open(os.path.join(r, 'same.par'), 'rb').read()
        snd(cmd='values', op='write', kind='par', name='same.par', replace=-1)
        evs, end = col(lambda e: is_ask(e) or is_idle(e))
        check('W129: a command can carry No without a question',
              end is not None and is_idle(end) and not errors(evs)
              and results(evs) == [{'ev': 'saved', 'saved': False, 'file': 'same.par'}]
              and before == open(os.path.join(r, 'same.par'), 'rb').read(), str(end))
        os.mkdir(os.path.join(r, 'blocked.par'))
        marker = os.path.join(r, 'blocked.par', 'original')
        with open(marker, 'wb') as f: f.write(b'old bytes')
        # The temp can be created beside this directory, but rename cannot
        # replace it: a deterministic commit failure, including on Windows.
        for name in ['blocked.par', 'absent/fail.par']:
            snd(cmd='values', op='write', kind='par', name=name, replace=1)
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
        shutil.copy(args.ode, d)
        commands = [{'cmd': 'values', 'op': 'write', 'kind': 'par', 'name': 'repeat.par', 'replace': 1}]
        script = os.path.join(d, 'save.jsonl')
        with open(script, 'w') as f: f.write('\n'.join(json.dumps(c) for c in commands))
        runs = [subprocess.run([os.path.abspath(args.server), '--script', script, os.path.basename(args.ode)],
                              cwd=d, capture_output=True, text=True, encoding='utf-8', timeout=30 * SLOW)
                for _ in range(2)]
        check('W129: the same script saves twice with its explicit answer, no disk-dependent ask',
              all(v.returncode == 0 and not any(e.get('ev') == 'ask' for e in
                  [json.loads(line) for line in v.stdout.splitlines() if line.strip()])
                  and '"saved":true' in v.stdout for v in runs), str([v.returncode for v in runs]))
    finally:
        shutil.rmtree(d, ignore_errors=True)


check_save_owner()


def check_seed_per_run():
    """W71 "a seed per run": examples/ode/fhn_noise.odex (wiener n) run
    twice gives different noise, each run logging its own seed in state
    (docs/protocol.md's state.seed); setting the numerics seed back to a
    run's own (Numerics > stocHast > New seed) and Go reproduces that
    run's series byte for byte."""
    p, r, snd, col, _ = launch_server(ode='examples/ode/fhn_noise.odex')

    def keys(key, *answers):
        snd(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=30 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            snd(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    full = whole_series
    try:
        col(is_idle)
        snd(cmd='data', events=['series'])
        col(is_idle)

        evs1 = keys('i', {'key': 'g'})
        st1, ser1 = last_state(evs1), full(evs1)
        evs2 = keys('i', {'key': 'g'})
        st2, ser2 = last_state(evs2), full(evs2)
        seed1 = st1.get('seed') if st1 else None
        seed2 = st2.get('seed') if st2 else None
        data1 = ser1[-1]['columns'] if ser1 else None
        data2 = ser2[-1]['columns'] if ser2 else None
        check('state: each Go logs its own seed', isinstance(seed1, int) and isinstance(seed2, int)
              and seed1 != seed2, str((seed1, seed2)))
        check('two Go presses give different data', data1 is not None and data1 != data2, '')

        # Numerics > stocHast > New seed: set run 2's own seed for the next Go
        keys('u')                                       # main menu: nUmerics
        keys('h', {'key': 'n'}, {'value': str(seed2)})  # stocHast: New seed
        keys('Escape')                                   # back to the main menu

        evs4 = keys('i', {'key': 'g'})
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
send2(cmd='key', key='f')
send2(cmd='key', key='a')
collect2(lambda e: e.get('ev') == 'window' and e.get('win') == 101)
collect2(is_idle)
send2(cmd='key', win='auto', key='r')
evs, ask = collect2(lambda e: e.get('ev') == 'ask')
check('bad-HOME server: Auto/Run opens the Start menu',
      ask is not None and ask['kind'] == 'menu' and 's' in ask['keys'], str(ask))
if ask:
    send2(cmd='answer', id=ask['id'], key='s')
evs, e = collect2(is_idle, timeout=20 * SLOW)
st2 = [x for x in evs if x.get('ev') == 'state']
alive = proc2.poll() is None
check('bad-HOME server survives Run/Steady state (state then idle, still alive)',
      ask is not None and st2 and e is not None and alive, str(evs)[-300:])

if alive:
    # writing to a dead server's stdin would raise and end the script
    send2(cmd='key', key='f')
    send2(cmd='key', key='q')
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
    snd3(cmd='state')
    evs, st = col3(is_state)
    col3(is_idle)
    check('a converted non-ASCII parameter has its suggested .odex name in state', st is not None
          and dict(st['pars']).get(alpha_name) == 0.05, str(st and st['pars']))
    snd3(cmd='set', kind='par', name=alpha_name, value=0.2)
    snd3(cmd='state')
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
    log = []
    p, r, snd, col, _ = launch_server(log=log)
    is_ask = lambda e: e.get('ev') == 'ask'
    try:
        col(is_idle)
        snd(cmd='key', key='u')
        col(is_idle)
        snd(cmd='key', key='t')
        evs, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], ok=1, value='1e7')
        col(is_idle)
        snd(cmd='key', key='Escape')
        col(is_idle)
        snd(cmd='key', key='i')
        evs, ask = col(is_ask)
        check('W95: a menu opened is no computation (no computing event yet)',
              not any(e.get('ev') == 'computing' for e in evs), str([e.get('ev') for e in evs]))
        snd(cmd='answer', id=ask['id'], key='g')
        evs, prog = col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        check('W95: the integration says it computes (computing) before its progress',
              [e.get('ev') for e in evs].count('computing') == 1, str([e.get('ev') for e in evs]))
        snd(cmd='key', key='s')  # Sing pts: a computation
        snd(cmd='key', key='c')  # Continue: a computation
        snd(cmd='display', win=1, x=[0, 50])  # a view: kept for after the run
        snd(cmd='browser', **{'from': 0, 'count': 1})  # only reads: answered during the run
        evs, rows = col(lambda e: e.get('ev') == 'browser')
        during = [e for e in evs if e.get('ev') in ('ask', 'idle', 'message')]
        snd(cmd='abort')
        evs, e = col(is_idle, timeout=30 * SLOW)
        stopped = any(x.get('ev') == 'stopped' for x in evs)
        check('W68: during a run, a browser request is answered at once and the run goes on until the Abort (stopped)',
              prog is not None and rows is not None and not during and stopped, str([x.get('ev') for x in evs])[-200:])
        after = []
        for _ in range(3):  # the two refused lines, then the display, each with its own idle
            more, _ = col(is_idle)
            after.append(more)
        refused = [[x.get('error', '') for x in m if x.get('ev') == 'message'] for m in after[:2]]
        check('W95: a computation or data command sent during a run is refused after it: an error, state and idle each',
              all(len(m) == 1 and 'Not while a computation runs' in m[0] for m in refused)
              and not any(is_ask(x) for m in after for x in m), str(refused))
        snd(cmd='data', events=['plots'])
        evs, _ = col(is_idle)
        plots = [x for x in evs if x.get('ev') == 'plots']
        w1 = plots and next((w for w in plots[-1]['windows'] if w['win'] == 1), None)
        check('W95: a view command sent during a run runs after it (display: the zoom shown)',
              not any(x.get('ev') == 'message' for x in after[2]) and w1 and w1.get('zoom', {}).get('x') == [0, 50],
              str(w1 and w1.get('zoom')))
        want = ['refused during a computation: key s', 'refused during a computation: key c']
        deadline = time.monotonic() + 5 * SLOW
        while not all(w in log for w in want) and time.monotonic() < deadline:
            time.sleep(0.05)
        check('W95: each refused line is logged once ("refused during a computation: key s"), the view none',
              all(log.count(w) == 1 for w in want) and len([l for l in log if 'refused' in l]) == 2,
              str([l for l in log if 'refused' in l]))
        snd(cmd='key', key='u')
        col(is_idle)
        snd(cmd='key', key='t')
        evs, ask = col(lambda e: e.get('ev') in ('ask', 'idle'))
        check('W68: after the run a key works again (nUmerics/Total asks)', ask is not None and ask['ev'] == 'ask',
              str(ask))
        if ask and ask['ev'] == 'ask':
            snd(cmd='answer', id=ask['id'], ok=0)
            col(is_idle)
    finally:
        stop_server(p, r, snd)


check_refused_during_run()


# W106 (GitHub #155, docs/protocol.md "Commands during a command"): a setting
# sent during a computation (a parameter, a numerics field) is taken at once
# and kept for after it: the run in progress keeps the values it started
# with (its rows are those of a run with the old values, stopped at the same
# row: a --script with the stop armed there, not a race), and each setting
# then applies as a command of its own (state and idle), before anything
# else: the state after shows it, and the next run uses it. A bad numerics
# value is an error message after the run, naming the field. The numerics
# as data: `data` `numerics` sends them, and `set` kind `num` sets one.
def check_method_refusal():
    model_dir = tempfile.mkdtemp(prefix='xppmethod')
    model = os.path.join(model_dir, 'odd.odex')
    with open(model, 'w') as f:
        f.write("x'=-x\ny'=x\nz'=y\n@ meth=euler\n")
    p, r, snd, col, _ = launch_server(ode=model)
    try:
        col(is_idle)
        snd(cmd='key', key='u')
        col(is_idle)
        snd(cmd='key', key='m')
        _, ask = col(lambda e: e.get('ev') == 'ask')
        snd(cmd='answer', id=ask['id'], key='y')
        evs, _ = col(is_idle)
        errors = [e for e in evs if e.get('ev') == 'message' and 'error' in e]
        snd(cmd='data', events=['numerics'])
        fields = next((e['fields'] for e in col(is_idle)[0] if e.get('ev') == 'numerics'), [])
        method = next((f['value'] for f in fields if f['key'] == 'method'), None)
        check('W132: menu refuses odd Symplectic at the command and keeps Euler',
              method == 1 and len(errors) == 1 and 'even dimensions' in errors[0]['error']
              and errors[0].get('file') == '' and errors[0].get('line') == 0, str(errors))
        snd(cmd='set', kind='num', name='method', text='symplectic')
        evs, _ = col(is_idle)
        snd(cmd='data', events=['numerics'])
        fields = next((e['fields'] for e in col(is_idle)[0] if e.get('ev') == 'numerics'), [])
        check('W132: values panel refuses odd Symplectic and keeps Euler',
              any('even dimensions' in e.get('error', '') for e in evs)
              and next((f['value'] for f in fields if f['key'] == 'method'), None) == 1, str(evs))
    finally:
        p.stdin.close()
        p.wait(timeout=10 * SLOW)
        shutil.rmtree(r, ignore_errors=True)
        shutil.rmtree(model_dir, ignore_errors=True)


check_method_refusal()


def check_settings_during_run():
    p, r, snd, col, _ = launch_server()
    is_prog = lambda e: e.get('ev') == 'progress'
    num = lambda evs: next((dict((f['key'], f['value']) for f in e['fields'])
                            for e in reversed(evs) if e.get('ev') == 'numerics'), None)
    iapp = lambda evs: next((dict(e['pars']).get('iapp') for e in reversed(evs) if is_state(e)), None)
    try:
        col(is_idle)
        snd(cmd='data', events=['numerics'])
        evs, _ = col(is_idle)
        n0 = num(evs)
        fields = next((e['fields'] for e in evs if e.get('ev') == 'numerics'), [])
        meth = next((f for f in fields if f['key'] == 'method'), {})
        check('W106: data numerics sends the numerics: total, dt, method (by number, with its choices)',
              n0 is not None and n0.get('total') == 30 and n0.get('dt') == 0.05
              and meth.get('choices', [None] * 4)[meth.get('value', 0)] == 'Runge-Kutta'
              and next((f for f in fields if f['key'] == 'nout'), {}).get('integer') is True, str(n0))
        snd(cmd='set', kind='num', name='total', value=1e7)
        evs, _ = col(is_idle)
        check('W106: set kind num sets a numerics field (total), the numerics event says so',
              (num(evs) or {}).get('total') == 1e7, str(num(evs)))
        snd(cmd='set', kind='num', name='dt', text='0')
        evs, _ = col(is_idle)
        errs = [e.get('error') for e in evs if e.get('ev') == 'message' and 'error' in e]
        check('W106: a bad numerics value is an error naming the field, nothing changes',
              errs == ['Numerics: Dt must be a number other than 0'] and num(evs) is None, str(errs))
        # W116: what set, slide and default cannot take is the protocol's
        # error, the value left as it was (never a 0, never the ICs reset)
        snd(cmd='state')
        v_before = dict(next((e for e in reversed(col(is_idle)[0]) if is_state(e)), {'ics': []})['ics']).get('V')
        for bad, expect in (
                (dict(cmd='set', kind='par', name='iapp'), 'set par iapp: its value is not a number (or its text missing)'),
                (dict(cmd='set', kind='par', name='iapp', value='0.2'),
                 'set par iapp: its value is not a number (or its text missing)'),
                (dict(cmd='set', kind='parm', name='iapp', value=0.2), 'set takes kind par, ic, delay, bc or num, not "parm"'),
                (dict(cmd='set', kind='par', name='nosuch', value=0.2), 'set: the model has no par nosuch'),
                (dict(cmd='set', kind='ic', index=7, value=0.2), 'set ic: its index is not one of 0 to 1'),
                (dict(cmd='slide', name='nosuch', value=0.2), 'slide: the model has no parameter or variable nosuch'),
                (dict(cmd='slide', name='iapp'), 'slide iapp: its value is not a number'),
                (dict(cmd='default', kind='pars'), 'default takes kind par or ic, not "pars"')):
            snd(cmd='set', kind='ic', name='v', value=0.25)
            evs, _ = col(is_idle)
            iapp0 = iapp(evs)
            snd(**bad)
            evs, _ = col(is_idle)
            errs = [e.get('error') for e in evs if e.get('ev') == 'message' and 'error' in e]
            snd(cmd='state')
            st = next((e for e in reversed(col(is_idle)[0]) if is_state(e)), None)
            check('W116: %s is refused (%s), iapp and V as they were' % (json.dumps(bad), expect),
                  errs == [expect] and st and dict(st['pars']).get('iapp') == iapp0 and dict(st['ics']).get('V') == 0.25,
                  '%s; %s' % (errs, st and (dict(st['pars']).get('iapp'), dict(st['ics']).get('V'))))
        snd(cmd='set', kind='ic', name='v', value=v_before)  # the ICs the checks after expect
        col(is_idle)
        snd(cmd='set', kind='num', name='method', text='cvode')
        evs, _ = col(is_idle)
        m = num(evs)
        snd(cmd='set', kind='num', name='method', value=3)
        evs, _ = col(is_idle)
        check('W106: set num method takes a name (any case) or a number',
              m and m.get('method') == 10 and (num(evs) or {}).get('method') == 3, str(m))
        snd(cmd='key', key='i')
        evs, ask = col(lambda e: e.get('ev') == 'ask')
        snd(cmd='answer', id=ask['id'], key='g')
        evs, prog = col(is_prog, timeout=30 * SLOW)
        snd(cmd='set', kind='par', name='iapp', value=0.3)
        snd(cmd='set', kind='num', name='total', value=50)
        snd(cmd='set', kind='num', name='nout', value=0)  # bad: its error comes after the run
        snd(cmd='state')
        snd(cmd='browser', **{'from': 0, 'count': 1})  # control, behind the sets: they were taken
        evs, _ = col(lambda e: e.get('ev') == 'browser')
        during = [e for e in evs if e.get('ev') in ('idle', 'message')]
        check('W106: settings sent during a run get no idle or error while it runs; state still shows the '
              'values it runs with', prog is not None and not during and iapp(evs) == 0.05,
              str([e.get('ev') for e in evs]))
        snd(cmd='abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        stopped = [e for e in evs if e.get('ev') == 'stopped']
        rows = next((e['rows'] for e in reversed(evs) if is_state(e)), 0)
        check('W106: the run went on with Total 1e7 until the Abort (stopped), the settings not applied yet',
              stopped and rows > 1 and iapp(evs) == 0.05 and num(evs) is None, str(stopped))
        after = []
        for _ in range(3):
            more, _ = col(is_idle)
            after.append(more)
        errs = [[e.get('error') for e in m if e.get('ev') == 'message'] for m in after]
        check('W106: then each setting is a command of its own, in order: the bad one an error after the run',
              iapp(after[0]) == 0.3 and (num(after[1]) or {}).get('total') == 50
              and errs == [[], [], ['Numerics: nOutput must be a whole number of at least 1']], str(errs))
        snd(cmd='browser', **{'from': rows - 1, 'count': 1})
        evs, br = col(lambda e: e.get('ev') == 'browser')
        col(is_idle)
        live = br and br['data']
        code, out, err = run_script([
            {'cmd': 'set', 'kind': 'num', 'name': 'total', 'value': 1e7},
            {'cmd': 'set', 'kind': 'num', 'name': 'method', 'value': 3},
            {'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'},
            {'cmd': 'abort', 'at': {'what': 'integrate', 'rows': rows, 't': 0}},
            {'cmd': 'browser', 'from': rows - 1, 'count': 1}])
        ref = next((e['data'] for e in map(json.loads, out.splitlines()) if e.get('ev') == 'browser'), None)
        check('W106: the rows the run stored are those of a run with the old values (the settings did not '
              'reach it)', code == 0 and live and live == ref, '%s vs %s (%s)' % (live, ref, err[-200:]))
        snd(cmd='key', key='i')
        evs, ask = col(lambda e: e.get('ev') == 'ask')
        snd(cmd='answer', id=ask['id'], key='g')
        evs, _ = col(is_idle, timeout=60 * SLOW)
        rows2 = next((e['rows'] for e in reversed(evs) if is_state(e)), 0)
        snd(cmd='browser', **{'from': rows2 - 1, 'count': 1})
        evs, br = col(lambda e: e.get('ev') == 'browser')
        col(is_idle)
        code, out, err = run_script([
            {'cmd': 'set', 'kind': 'par', 'name': 'iapp', 'value': 0.3},
            {'cmd': 'set', 'kind': 'num', 'name': 'total', 'value': 50},
            {'cmd': 'set', 'kind': 'num', 'name': 'method', 'value': 3},
            {'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'},
            {'cmd': 'browser', 'from': 1000, 'count': 1}])
        ref = next((e['data'] for e in map(json.loads, out.splitlines()) if e.get('ev') == 'browser'), None)
        check('W106: the next run uses them (Total 50: 1001 rows, iapp 0.3: the rows of a run set so)',
              rows2 == 1001 and br and br['data'] == ref, '%d rows, %s vs %s' % (rows2, br and br['data'], ref))
    finally:
        stop_server(p, r, snd)


check_settings_during_run()


# W95: a command sent right behind one that asks, before the client saw the
# question, is kept for after that command, never lost: its own idle comes
# (the key i opens a menu; the state behind it runs once the menu is answered)
def check_kept_behind_ask():
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='key', key='i')
        snd(cmd='equations')
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

def check_copy_set():
    """W67: File/cOpy set line asks the set's name, shows the line, and sends it
    (the copy event); pasted into the .odex, Get par set reproduces the state"""
    p, r, snd, col, _ = launch_server()
    p2 = r2 = None
    try:
        col(is_idle)
        snd(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'value': 0.1234567891234567},
                               {'kind': 'ic', 'name': 'V', 'value': -0.7000000000000001}])
        evs, _ = col(is_idle)
        st = last_state(evs)

        def copy_with(name):
            snd(cmd='key', key='f')
            col(is_idle)
            snd(cmd='key', key='o')
            got, prompt = [], None
            while True:
                evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=20 * SLOW)
                got += evs
                if e is None or e['ev'] == 'idle':
                    return got, prompt
                if e['kind'] == 'string':
                    prompt = e
                    snd(cmd='answer', id=e['id'], value=name)
                else:
                    prompt = prompt or e
                    snd(cmd='answer', id=e['id'], key='c')

        evs, first = copy_with('set1')
        check('cOpy set line: the name is asked, pre-filled with set1',
              first and first['kind'] == 'string' and first.get('value') == 'set1', str(first))
        copies = [e for e in evs if e.get('ev') == 'copy']
        line = copies[0]['text'] if copies else ''
        check('cOpy set line: the copy event carries a set line with every parameter and IC',
              len(copies) == 1 and line.startswith('set set1 {') and line.endswith('}')
              and line.count('=') == len(st['pars']) + len(st['ics']) and 'iapp=0.1234567891234567' in line, line[:200])
        for name in ('bad name', 'set'):
            evs, _ = copy_with(name)
            check('cOpy set line: refuses %r (no copy)' % name, not [e for e in evs if e.get('ev') == 'copy'], '')
        odelines = open(os.path.join(r, os.path.basename(args.ode)), encoding='utf-8').read().splitlines()
        # The copied foreign-format line is checked above; our model uses
        # the .odex set statement, without braces or a done terminator.
        odelines.append(line.replace(' {', ' = ', 1).removesuffix('}'))
        with open(os.path.join(r, 'withset.odex'), 'w', encoding='utf-8') as f:
            f.write('\n'.join(odelines) + '\n')
        p2, r2, snd2, col2, _ = launch_server(ode=os.path.join(r, 'withset.odex'))
        evs2, _ = col2(is_idle)
        st0 = last_state(evs2)
        check('set line loads: values differ before Get par set', st0 and st0['pars'] != st['pars'], '')
        snd2(cmd='key', key='f')
        col2(is_idle)
        snd2(cmd='key', key='g')
        evs2, ask = col2(lambda e: e.get('ev') == 'ask')
        check('Get par set lists the copied set', ask is not None and ask['kind'] == 'menu', str(ask))
        if ask:
            snd2(cmd='answer', id=ask['id'], key=next(i[0] for i in ask['items'] if i.endswith(': set1')))
        evs2, _ = col2(is_idle)
        st2 = last_state(evs2)
        check('Get par set with the copied name reproduces the parameters and ICs exactly',
              st2 and st2['pars'] == st['pars'] and st2['ics'] == st['ics'], str((st2 or {}).get('pars'))[:200])
    finally:
        stop_server(p, r, snd)
        if p2:
            stop_server(p2, r2, snd2)


check_copy_set()


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
            snd(cmd='browser', op='write', what='table', format='csv', name=name)
            col(is_idle)
            with open(os.path.join(r, name)) as f:
                return [l.strip() for l in f if l.strip() and not l.startswith('#')]

        def integrate():
            snd(cmd='key', key='i')
            _, ask = col(lambda e: e.get('ev') == 'ask')
            if ask:
                snd(cmd='answer', id=ask['id'], key='g')
            col(is_idle)

        def open_model(file, key='d'):
            """open file, answering its question with key; the events until idle"""
            snd(cmd='open', file=file)
            got = []
            while True:
                evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=20 * SLOW)
                got += evs
                if e is None or e['ev'] == 'idle':
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
        open_model('lecar.odex')
        integrate()
        first = rows_of('lecar1.csv')
        snd(cmd='key', key='f')
        col(is_idle)
        snd(cmd='key', key='a')
        evs, _ = col(is_idle)

        def made(evs, op, win):
            return any(e.get('ev') == 'window' and e.get('op') == op and e.get('win') == win for e in evs)

        check('File/Auto opens window 101 before the open', made(evs, 'create', 101), '')
        evs = open_model('other.ode', key=None)
        asks = [e for e in evs if e.get('ev') == 'ask']
        check('open asks first, offering to save (choice s/d)',
              asks and asks[0]['kind'] == 'choice' and asks[0].get('keys') == 'sd', str(asks[:1]))
        check('open cancelled: no hello, lecar still loaded',
              not [e for e in evs if e.get('ev') == 'hello'] and not made(evs, 'destroy', 101)
              and last_state(evs)['ics'][0][0] == 'V',
              str(last_state(evs))[:120])
        evs = open_model('other.ode')
        hello = next((e for e in evs if e.get('ev') == 'hello'), None)
        st = last_state(evs)
        check("open: the model before's AUTO window goes", made(evs, 'destroy', 101), '')
        check('open loads the model here: a new hello names it', hello is not None and hello.get('file') == 'other.odex',
              str(hello)[:120])
        check('open: the state is the new model\'s', st and st['pars'] == [['a', 1]] and st['ics'] == [['X', 0.5]],
              str(st)[:200])
        integrate()
        other1 = rows_of('other1.csv')
        check('the opened model integrates (its 101 rows and their heading)', len(other1) == 102, str(other1[:3]))

        # Reload after editing: a's value set in the session stays, the new b comes with its file value
        snd(cmd='set', values=[{'kind': 'par', 'name': 'a', 'value': 3}, {'kind': 'ic', 'name': 'X', 'value': 2}])
        col(is_idle)
        with open(os.path.splitext(other)[0] + '.odex', 'w') as f:
            f.write('par a=1, b=7\ninit x=0.5\nx\'=-a*x+b*0\n@ total=5, dt=0.05\n')
        snd(cmd='reload')
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
        snd(cmd='key', key='u')
        snd(cmd='key', key='t')
        _, ask = col(lambda e: e.get('ev') == 'ask')
        if ask:
            snd(cmd='answer', id=ask['id'], ok=1, value='10')
            col(is_idle)
        snd(cmd='key', key='Escape')
        col(is_idle)
        snd(cmd='reload')
        _, ask = col(lambda e: e.get('ev') == 'ask')
        if ask:
            snd(cmd='answer', id=ask['id'], key='d')
        col(is_idle)
        integrate()
        check('reload keeps the numerics: Total 10 gives 201 rows', len(rows_of('other_t10.csv')) == 202, '')

        # a model that cannot be loaded: the one before stays, integrating to the same data
        snd(cmd='set', values=[{'kind': 'par', 'name': 'a', 'value': 1}, {'kind': 'ic', 'name': 'X', 'value': 0.5}])
        col(is_idle)
        integrate()
        before = rows_of('other2.csv')
        evs = open_model('broken.ode')
        errs = [e for e in evs if e.get('ev') == 'message' and 'error' in e]
        check('a broken model: an error, no hello',
              errs and errs[-1].get('file') == 'broken.ode' and not [e for e in evs if e.get('ev') == 'hello'],
              str(errs)[:200])
        check('a broken model: the model before is still loaded, its values untouched',
              last_state(evs)['pars'] == [['a', 1], ['b', 7]], str(last_state(evs))[:200])
        evs = open_model('badoption.ode')
        errs = [e for e in evs if e.get('ev') == 'message' and 'error' in e]
        check('a refused option value: an error naming the file, line and value, the model before kept (W119)',
              errs and errs[-1].get('file') == 'badoption.ode' and 'ync=12' in errs[-1]['error']
              and not [e for e in evs if e.get('ev') == 'hello']
              and last_state(evs)['pars'] == [['a', 1], ['b', 7]], str(errs)[:300])
        evs = open_model('missing.ode')
        check('a missing model: an error naming the file (line 0: it cannot be read), nothing asked',
              [e for e in evs if e.get('ev') == 'message' and e.get('file') == 'missing.ode' and e.get('line') == 0]
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

    def keys(key, *answers):
        snd(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            snd(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    runs_of = lambda evs: [e for e in evs if e.get('ev') == 'runs']
    added = lambda evs: [x for e in runs_of(evs) for x in e['add']]
    win1 = lambda evs: [w for e in evs if e.get('ev') == 'plots' for w in e['windows'] if w['win'] == 1]
    full = whole_series
    try:
        col(is_idle)
        evs = after(cmd='data', events=['series', 'plots'])
        w = win1(evs)
        check('display: a window shows its own axes and its earlier runs at first',
              w and w[-1].get('zoom') == {'x': None, 'y': None} and w[-1].get('runs') == 1, str(w))
        check('display: no runs event before there is a run', not runs_of(evs))
        evs = keys('i', {'key': 'g'})
        first = full(evs)[-1]
        check('display: the first run has nothing before it', not added(evs) and not any(e['erased'] for e in runs_of(evs)))
        evs = keys('i', {'key': 'g'})
        cols1 = {c['col']: c['data'] for c in first['columns']}
        check('display: a second run keeps the first as an earlier run (one runs event: keep, the series the client'
              ' holds, not its data again)',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['keep'] == 1 and not added(evs)
              and all(e['erased'] == 0 and not e['clear'] and e['drop'] == 0 for e in runs_of(evs)), str(runs_of(evs))[:300])
        evs = after(cmd='data', events=['series', 'plots'])
        got = added(evs)
        check('display: asking for the data again sends the earlier runs whole, once: the data of the first run',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['clear'] == 1 and len(got) == 1
              and got[0]['rows'] == first['rows'] and got[0]['curves'] == first['curves']
              and {c['col']: values(c, None) for c in got[0]['columns']} == cols1, str(runs_of(evs))[:200])
        evs = keys('e')
        check('display: Erase forgets the earlier runs and hides the current one',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['clear'] == 1 and runs_of(evs)[0]['erased'] == 1
              and not added(evs), str(runs_of(evs)))
        evs = keys('r')
        check('display: Redraw shows the current run again, without earlier ones',
              len(runs_of(evs)) == 1 and runs_of(evs)[0]['clear'] == 1 and runs_of(evs)[0]['erased'] == 0
              and not added(evs), str(runs_of(evs)))

        evs = after(cmd='display', win=1, x=[0, 10], y=[-0.5, 0.5], runs=False)
        w = win1(evs)
        check('display: a zoom and the runs toggle are held by the window and sent in plots',
              w and w[-1]['zoom'] == {'x': [0, 10], 'y': [-0.5, 0.5]} and w[-1]['runs'] == 0, str(w))
        evs = after(cmd='display', win=1, x=None)
        w = win1(evs)
        check('display: null returns one axis to the window own axes', w and w[-1]['zoom'] == {'x': None, 'y': [-0.5, 0.5]}, str(w))
        evs = after(cmd='display', win=1, x=[5, 1])
        check('display: a range with low not below high is refused, the zoom unchanged',
              any(e.get('ev') == 'message' and 'error' in e for e in evs) and not win1(evs), str(evs)[:200])
        evs = after(cmd='display', win=7, x=[0, 1])
        check('display: an unknown window is refused',
              any(e.get('ev') == 'message' and 'error' in e for e in evs), str(evs)[:200])
        evs = keys('w', {'key': 'w'}, {'ok': 1, 'values': ['-1', '1', '-1', '1']})
        w = win1(evs)
        check('display: other axes of the window drop the zoom', w and w[-1]['zoom'] == {'x': None, 'y': None}, str(w))
    finally:
        stop_server(p, r, snd)

    p, r, snd, col, _ = launch_server(NO_THROTTLE)
    try:
        col(is_idle)
        snd(cmd='data', events=['autoinfo'])
        evs, _ = col(is_idle)
        av = lambda evs: [e for e in evs if e.get('ev') == 'autoview']
        check('autoview: sent at once after data, nothing hidden, the diagram own axes',
              [(e['earlier'], e['show'], e['active'], e['views']) for e in av(evs)] == [(0, 0, 0, [{'zoom': {'x': None, 'y': None}}])],
              str(av(evs)))
        evs = lecar_to_auto(snd, col)
        n = len(rebuild_diagram(evs, []))
        check('autoview: a run hides nothing', not av(evs) or av(evs)[-1]['earlier'] == 0, str(av(evs)))
        snd(cmd='key', win='auto', key='c')
        evs, _ = col(is_idle)
        snd(cmd='key', win='auto', key='d')
        evs2, _ = col(is_idle)
        got = av(evs) + av(evs2)
        check('autoview: Clear makes the points so far the earlier branches, hidden',
              got and got[-1]['earlier'] == n and got[-1]['show'] == 0 and n > 1, str(got) + ' n=%d' % n)
        snd(cmd='auto', op='display', show=True, x=[0.1, 0.2], y=None)
        evs, _ = col(is_idle)
        check('autoview: show and the zoom are set by auto display',
              av(evs) and av(evs)[-1] == dict(av(evs)[-1], earlier=n, show=1, views=[{'zoom': {'x': [0.1, 0.2], 'y': None}}]),
              str(av(evs)))
        snd(cmd='auto', op='display', x=[3, 3])
        evs, _ = col(is_idle)
        check('autoview: a bad range is refused', any(e.get('ev') == 'message' and 'error' in e for e in evs), str(evs)[:200])
        snd(cmd='auto', op='close')
        evs, _ = col(is_idle)
        check('autoview: closing AUTO resets it', av(evs) and av(evs)[-1]['earlier'] == 0
              and av(evs)[-1]['views'] == [{'zoom': {'x': None, 'y': None}}], str(av(evs)))
    finally:
        stop_server(p, r, snd)


def check_silent_commands():
    """W56: --silent is a built-in script of the protocol's own commands.
    Each command it uses where the interface had none (browser write of
    the output columns with replace, browser postprocess, values query,
    dfield write, equilibrium write) writes, from a --script, the very
    file the --silent run writes; --silent prints nothing on stdout."""
    def silent(ode, *flags):
        d = tempfile.mkdtemp(prefix='xppsilent')
        shutil.copy(ode, d)
        r = subprocess.run([os.path.abspath(args.server), os.path.basename(ode), '--silent'] + list(flags), cwd=d,
                           capture_output=True, text=True, timeout=60 * SLOW)
        return d, r

    def script(ode, lines, before=None):
        d = tempfile.mkdtemp(prefix='xppscript')
        shutil.copy(ode, d)
        if before:
            before(d)
        with open(os.path.join(d, 'script.jsonl'), 'w') as f:
            f.write(''.join(json.dumps(save_permission(c)) + chr(10) for c in lines))
        r = subprocess.run([os.path.abspath(args.server), '--script', 'script.jsonl', os.path.basename(ode)],
                           cwd=d, capture_output=True, text=True, timeout=60 * SLOW)
        return d, r

    def read(d, n):
        path = os.path.join(d, n)
        return open(path, 'rb').read().replace(b'\r', b'') if os.path.exists(path) else None

    go = [{'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'}]
    dirs = []
    try:
        s, r = silent(args.ode, '--qsets', '--qpars', '--qics', '--outfile', 'q.txt')
        dirs.append(s)
        q, rq = script(args.ode, [{'cmd': 'values', 'op': 'query', 'name': 'q.txt', 'sets': 1, 'pars': 1, 'ics': 1}])
        dirs.append(q)
        check('values query writes what --silent --qsets --qpars --qics writes', rq.returncode == 0 and
              read(q, 'q.txt') is not None and read(q, 'q.txt') == read(s, 'q.txt'), rq.stderr[-300:])

        s, r = silent(args.ode, '--dfdraw', '4', '--equil', '0')
        dirs.append(s)
        check('--silent writes nothing on stdout', r.returncode == 0 and r.stdout == '', r.stdout[:300])
        stale = lambda d: open(os.path.join(d, 'output.dat'), 'w').write('stale\n')
        c, rc = script(args.ode, go + [
            {'cmd': 'browser', 'op': 'write', 'what': 'output', 'format': 'dat', 'name': 'output.dat', 'replace': 1},
            {'cmd': 'equilibrium', 'op': 'write', 'name': 'equil.dat'},
            {'cmd': 'key', 'key': 'd'}, {'cmd': 'answer', 'key': 'd'}, {'cmd': 'answer', 'value': '16'},
            {'cmd': 'dfield', 'op': 'write', 'name': 'dirfields.dat'}], before=stale)
        dirs.append(c)
        check('the script with --silent\'s commands runs through', rc.returncode == 0, rc.stderr[-300:])
        check('browser write of the output, replace: --silent\'s output.dat, over the file there, unasked',
              read(c, 'output.dat') is not None and read(c, 'output.dat') == read(s, 'output.dat'))
        check('equilibrium write: --silent --equil 0\'s equil.dat',
              read(c, 'equil.dat') is not None and read(c, 'equil.dat') == read(s, 'equil.dat'))
        df = read(c, 'dirfields.dat') or b''
        check('dfield write: the direction field shown, one arrow a line (17 x 17), in --silent --dfdraw 4\'s form',
              len(df.splitlines()) == 17 * 17 and df == read(s, 'dirfields.dat'),
              '%d lines' % len(df.splitlines()))
        bad, rb = script(args.ode, [{'cmd': 'dfield', 'op': 'write', 'name': 'none.dat'}])
        dirs.append(bad)
        check('dfield write with no field shown is an error, and writes nothing',
              rb.returncode == 1 and '"error"' in rb.stdout and read(bad, 'none.dat') is None, rb.stdout[-300:])

        dirs.append(tempfile.mkdtemp(prefix='xpppost'))
        post = os.path.join(dirs[-1], 'post.odex')
        with open(post, 'w') as f:
            f.write("x'=-x+sin(t)\ninit x=0.5\n@ total=50,dt=.05,postprocess=1,histcol=x,histlo=-1,histhi=1,histbins=20\n")
        s, r = silent(post)
        dirs.append(s)
        c, rc = script(post, go + [{'cmd': 'browser', 'op': 'postprocess'},
                                   {'cmd': 'browser', 'op': 'write', 'what': 'output', 'format': 'dat',
                                    'name': 'output.dat'}])
        dirs.append(c)
        hist = read(c, 'output.dat') or b''
        check('browser postprocess: the model\'s @ postprocess (a 20-bin histogram), as --silent writes it',
              len(hist.splitlines()) == 21 and hist == read(s, 'output.dat'), '%d lines' % len(hist.splitlines()))
    finally:
        for d in dirs:
            shutil.rmtree(d, ignore_errors=True)


check_silent_commands()

def check_outcomes_once():
    """W133: results, file places, film failures, and every exit route."""
    d = tempfile.mkdtemp(prefix='xppoutcomes')
    try:
        with open(os.path.join(d, 'linear.odex'), 'w') as f:
            f.write("par a=1\nx'=a\ninit x=0\n@ dt=.1,total=1\n")
        with open(os.path.join(d, 'fit.dat'), 'w') as f:
            f.write(''.join('%d %d\n' % (t, 2*t) for t in range(6)))
        def run(lines, *flags):
            with open(os.path.join(d, 'script.jsonl'), 'w') as f:
                f.write(''.join(json.dumps(c) + '\n' for c in lines))
            r = subprocess.run([os.path.abspath(args.server), '--script', 'script.jsonl', 'linear.odex', *flags],
                               cwd=d, capture_output=True, text=True, timeout=60 * SLOW)
            return r, [json.loads(line) for line in r.stdout.splitlines() if line.startswith('{')]
        def fit(name):
            return [{'cmd': 'key', 'key': 'u'}, {'cmd': 'key', 'key': 'h'},
                    {'cmd': 'answer', 'key': 'i'},
                    {'cmd': 'answer', 'values': [name, 'x', 'a', '1e-6', '6', '2', '2', '', '1e-5', '50']}]
        r, evs = run(fit('fit.dat'))
        results = [e.get('bottom', '').strip() for e in evs if e.get('ev') == 'message']
        errors = [e for e in evs if e.get('error')]
        check('a converged script fit exits 0 and sends exactly one info outcome, no error dialog event',
              r.returncode == 0 and results.count('Success!') == 1 and not errors, str((r.returncode, results, errors, r.stderr[-300:])))
        r, evs = run(fit('missing.dat') + [{'cmd': 'quit', 'ask': True}, {'cmd': 'answer', 'key': 'd'}])
        errors = [e for e in evs if e.get('error')]
        check('an unreadable fit file has its Place; the asking quit also exits 1, without bye',
              r.returncode == 1 and len(errors) == 1 and errors[0].get('file') == 'missing.dat'
              and errors[0].get('line') == 0 and not any(e.get('ev') == 'bye' for e in evs), str(errors))
        r, evs = run([{'cmd': 'key', 'win': 'ani', 'key': 'f'}, {'cmd': 'answer', 'file': 'missing.ani'}])
        errors = [e for e in evs if e.get('error')]
        check('an unreadable animation names its file at line 0 once',
              r.returncode == 1 and len(errors) == 1 and errors[0].get('file') == 'missing.ani'
              and errors[0].get('line') == 0, str(errors))
        # Kinescope capture has a fixed capacity; its last attempted frame fails once.
        capture = [{'cmd': 'key', 'key': 'k'}, {'cmd': 'answer', 'key': 'c'}]
        r, evs = run(capture * 251)
        errors = [e for e in evs if e.get('error')]
        frames = [e for e in evs if e.get('ev') == 'film' and e.get('op') == 'capture']
        check('the full kinescope returns one error at its command, retaining 250 frames',
              r.returncode == 1 and len(frames) == 250 and len(errors) == 1
              and errors[0].get('file') == 'script.jsonl' and errors[0].get('line') == 502, str(errors))
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
    GO = [{'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'}]
    SING = [{'cmd': 'key', 'key': 's'}, {'cmd': 'answer', 'key': 'g'}]

    def run(ode, lines=GO):
        code, out, err = run_script(lines, ode=ode)
        evs = [json.loads(l) for l in out.splitlines() if l.strip()]
        errs = [e['error'] for e in evs if e.get('ev') == 'message' and 'error' in e]
        return code, errs, (last_state(evs) or {}).get('rows'), err

    code, errs, rows, err = run('examples/ode/dae_ex3.odex')
    # --script exits 1 after an error message (docs/protocol.md "Scripts")
    check('dae_ex3 stops at the fold: the rows to t=0.45, then one error',
          code == 1 and rows == 10 and len(errs) == 1, 'exit %d, rows %s, %s %s' % (code, rows, errs, err[-200:]))
    check('... saying there is no solution past t=0.45, a fold',
          len(errs) == 1 and errs[0].startswith('No solution of the algebraic equations past t=0.45:')
          and 'fold' in errs[0], str(errs))
    code, out, err = run_script(GO, ode='examples/ode/dae_ex3.odex')
    fold = next((json.loads(l) for l in out.splitlines() if 'No solution of the algebraic' in l), {})
    check('... at the 0= line, the place of every error (W140b)',
          fold.get('file') == 'dae_ex3.odex' and fold.get('line') == 4 and fold.get('source') == '0 = v_*(1-v_*v_)-w',
          str(fold))
    code, errs, rows, err = run('examples/ode/dae.odex')
    check('dae.odex, a DAE without a fold, runs to its end with no error',
          code == 0 and rows == 401 and not errs, 'exit %d, rows %s, %s %s' % (code, rows, errs, err[-200:]))

    # The branch is the run's alone (DaeRun): Sing pts after it solves the
    # algebraic equations without one, as before W127, both after the run
    # that stopped at the fold and after one that ended on the branch
    # (total .3, before the fold). dae_ex3's equilibrium, w=0 and v=0, is on
    # the middle branch, which Newton from the run's v (the outer branch)
    # does not reach, before W127 or since: "Could not converge to root",
    # and no DAE error of the search's own.
    code, errs, rows, err = run('examples/ode/dae_ex3.odex', GO + SING)
    check('Sing pts right after the run stopped at the fold: as before W127',
          rows == 10 and len(errs) == 2 and 'fold' in errs[0] and errs[1] == 'Could not converge to root', str(errs))
    short_dir = tempfile.mkdtemp(prefix='xppdae')
    try:
        short = os.path.join(short_dir, 'dae_ex3.odex')
        with open('examples/ode/dae_ex3.odex') as f:
            text = f.read() + '\n@ total=.3\n'
        with open(short, 'w') as f:
            f.write(text)
        code, errs, rows, err = run(short, GO + SING)
        check('Sing pts after a run that ended on its branch: as before W127',
              rows == 7 and errs == ['Could not converge to root'], 'rows %s, %s' % (rows, errs))
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
        run(cmd='browser', op='write', what='output', format='dat', name=name, replace=1)

    def read(r, name):
        path = os.path.join(r, name)
        return open(path, 'rb').read() if os.path.exists(path) else None

    keep = tempfile.mkdtemp(prefix='xpprandom')
    p, r, snd, col, run = mk()
    try:
        # a recording of a Go and a Continue after an earlier Go: its snapshot
        # holds the generator and the next Go's seed
        run(cmd='key', key='i')
        run(cmd='record', op='start')
        run(cmd='key', key='i')
        run(cmd='key', key='c')
        write(run, 'rec.dat')
        run(cmd='record', op='stop', name='rr')
        rec = read(r, 'rec.dat')
        text = open(os.path.join(r, 'rr.recx'), encoding='utf-8').read()
        shutil.copy(os.path.join(r, 'rr.recx'), keep)
        snap = recx_snapshot(text) or {}
        check('random: a recording snapshot holds random.txt', 'random.txt' in snap, str(sorted(snap)))
        run(cmd='key', key='i')
        run(cmd='session', op='save', name='r1')
        run(cmd='key', key='c')
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
        run(cmd='open', file='r1.snapx')
        run(cmd='key', key='c')
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
            evs = run(cmd='open', file=name + '.snapx')
            msgs = ' '.join(placed(e) for e in evs if e.get('ev') == 'message')
            check('random: %s.snapx is refused, the error names the member' % name,
                  all(x in msgs for x in (expect if isinstance(expect, tuple) else (expect,))), msgs[:300])
    finally:
        stop_server(p, r, snd)

    p, r, snd, col, run = mk()
    try:
        shutil.copy(os.path.join(keep, 'rr.recx'), r)
        snd(cmd='play', op='open', file='rr.recx')
        evs, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='d')
            col(is_idle, timeout=30 * SLOW)
        run(cmd='play', op='speed', speed=8)
        snd(cmd='play', op='start')
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
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            allev.extend(evs)
            if e is None or e['ev'] == 'idle':
                return evs
            a = answers[n] if n < len(answers) else {'ok': 0}
            n += 1
            snd(cmd='answer', id=e['id'], **({'key': a} if isinstance(a, str) else a))

    def key(snd, col, k, *answers, win=None):
        return answered(snd, col, answers, cmd='key', key=k, **({'win': win} if win else {}))

    SUBSCRIBE = dict(cmd='data', events=['plots', 'marks', 'autoinfo', 'nullclines'])
    p, r, snd, col, _ = launch_server()
    keep = tempfile.mkdtemp(prefix='xppsnapx')
    try:
        col(is_idle)
        answered(snd, col, (), **SUBSCRIBE)
        allev.extend(lecar_to_auto(snd, col))
        answered(snd, col, (), cmd='set', values=[{'kind': 'par', 'name': 'phi', 'value': 0.4},
                                                  {'kind': 'ic', 'name': 'V', 'value': -0.3}])
        key(snd, col, 'i', 'g')
        answered(snd, col, (), cmd='display', win=1, x=[0, 50], y=[-0.5, 0.3], runs=False)
        answered(snd, col, (), cmd='auto', op='display', x=[0.05, 0.3], y=None)
        # W50: a second view of the diagram, the norm, with a zoom of its own; the first active again
        answered(snd, col, (), cmd='auto', op='view', new=1)
        answered(snd, col, (), cmd='auto', op='set', axes={'view': 1, 'plot': 1, 'fit': True})
        answered(snd, col, (), cmd='auto', op='display', view=1, x=None, y=[0, 0.5])
        answered(snd, col, (), cmd='auto', op='view', active=0)
        key(snd, col, 't', 't', {'value': 'here'}, {'value': '2'}, {'xd': 10, 'yd': 0.1})
        key(snd, col, 'g', 'f', 'f', {'values': ['4', 'first', 'frz1']})
        key(snd, col, 'm', 'c')
        # W145: an added browser column is saved with the session and comes back computed
        key(snd, col, 'a', {'ok': 1, 'value': 'VW'}, {'ok': 1, 'value': 'v*w'}, win='browser')
        browser1 = next((no_t(e) for e in reversed(answered(snd, col, (), cmd='browser', **{'from': 0, 'count': 2}))
                         if e.get('ev') == 'browser'), None)
        check('W145: the added column VW is in the browser before the save',
              browser1 is not None and browser1.get('cols', [])[-1:] == ['VW'], str(browser1)[:200])
        # W135: definitions and computed/frozen nullclines are Session members.
        answered(snd, col, (), cmd='slider', slot=0, name='iapp', lo=-1, hi=2, step=0.01)
        answered(snd, col, (), cmd='slider', slot=2, name='V', lo=-2, hi=2, step=0.02)
        answered(snd, col, (), cmd='slider', slot=3, name='phi', lo=0, hi=1, step=0.001)
        key(snd, col, 'n', 'n')
        key(snd, col, 'n', 'f', 'f')
        answered(snd, col, (), cmd='session', op='save', name='s1')
        st1 = no_t(last('state'))
        check('session save: state.session names the .snapx', st1 and st1.get('session') == {'file': 's1.snapx'},
              str(st1 and st1.get('session')))
        saved = {k: no_t(last(k)) for k in ('plots', 'autoview', 'nullclines')}
        check('W135: the saved nullclines contain a current curve and one frozen pair',
              saved['nullclines'] and saved['nullclines'].get('x') and len(saved['nullclines'].get('frozen', [])) == 1,
              str(saved['nullclines'] and [len(saved['nullclines'].get('x', [])), len(saved['nullclines'].get('frozen', []))]))
        marks1 = [no_t(last('marks', win=w)) for w in (1, 2)]
        diagram1 = rebuild_diagram(allev, [])
        view1 = rebuild_diagram(allev, [], 1)
        snap = os.path.join(r, 's1.snapx')
        names = zipfile.ZipFile(snap).namelist() if os.path.exists(snap) else []
        check('session save: s1.snapx is a zip of the files listed, the model in it',
              sorted(names) == sorted(['session.txt', 'model/lecar.odex', 'model.set', 'auto/settings.txt', 'auto/diagram.csv',
                        'auto/solutions.s', 'auto/views.txt', 'windows.set', 'marks.set', 'frozen.npz', 'data.npz', 'random.txt', 'sliders.set', 'nullclines.set']),
              str(names))
        if names:
            z = zipfile.ZipFile(snap)
            check('session save: model.set is a set file of lecar, session.txt names it',
                  z.read('model.set').startswith(b'## Set file for lecar.odex')
                  and b'\nname lecar.odex\n' in z.read('session.txt'), str(z.read('session.txt')[:200]))
            with open(os.path.join(r, 'lecar.odex'), 'rb') as f:
                check('session save: model/lecar.odex is the model, byte for byte', z.read('model/lecar.odex') == f.read())
            vt = z.read('auto/views.txt').decode().splitlines()
            check('session save: auto/views.txt holds both views, the first active',
                  len(vt) == 3 and vt[0].startswith('view 2 V iapp ') and vt[1].startswith('view 1 V iapp ')
                  and vt[1].endswith(' - 0:0.5') and vt[2] == 'active 0', str(vt))
            shutil.copy(snap, keep)
        answered(snd, col, (), cmd='session', op='save', name='s2.snapx', data=False)
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
            answered(snd, col, ('d',), cmd='reload')
        answered(snd, col, (), **SUBSCRIBE)
        del allev[:]
        answered(snd, col, ('d',), cmd='open', file=name)
        answered(snd, col, (), **SUBSCRIBE)
        return p, r, snd, col

    def restored_as_saved(tag):
        st2 = no_t(last('state'))
        drop = lambda s: {k: v for k, v in (s or {}).items() if k != 'session'}
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
        browser2 = next((no_t(e) for e in reversed(answered(snd, col, (), cmd='browser', **{'from': 0, 'count': 2}))
                         if e.get('ev') == 'browser'), None)
        check('W145: open session: the added column VW is back, computed as before the save',
              browser1 is not None and browser2 == browser1, '%s\n    %s' % (str(browser1)[:200], str(browser2)[:200]))
        m2 = [no_t(last('marks', win=w)) for w in (1, 2)]
        strip = lambda m: m and dict(m, equilibria=[])
        check('open session: the labels and frozen curves are the saved ones (not Sing pts\' symbols)',
              [strip(m) for m in marks1] == [strip(m) for m in m2], str(m2)[:300])
        answered(snd, col, (), cmd='auto', op='grab', type='HB', index=1)
        evs = key(snd, col, 'r', 'p', win='auto')
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
                  'phi2' in f.read() and sorted(os.listdir(r)) == ['lecar.odex', 's1.snapx'], str(os.listdir(r)))
        # a zip opened as a model: refused, its bytes never shown, nothing changes
        shutil.copy(os.path.join(r, 's1.snapx'), os.path.join(r, 'zip.ode'))
        del allev[:]
        answered(snd, col, ('d',), cmd='open', file='zip.ode')
        msgs = ' '.join(str(e.get('bottom', '')) + str(e.get('error', '')) for e in allev if e.get('ev') == 'message')
        check('a zip opened as a model is refused, its bytes never shown',
              'model text' in msgs and 'PK' not in msgs and not last('hello'), msgs[:300])
        # a session file without its model: refused, nothing changes
        z = zipfile.ZipFile(os.path.join(r, 's1.snapx'))
        with zipfile.ZipFile(os.path.join(r, 'nomodel.snapx'), 'w') as out:
            for n in z.namelist():
                if not n.startswith('model/'):
                    out.writestr(n, z.read(n))
        del allev[:]
        answered(snd, col, ('d',), cmd='open', file='nomodel.snapx')
        msgs = ' '.join(str(e.get('bottom', '')) + str(e.get('error', '')) for e in allev if e.get('ev') == 'message')
        check('a session file without its model is refused, an error says the model is missing',
              'model is missing' in msgs and not last('hello') and not any(e.get('ev') == 'ask' for e in allev), msgs[:300])
        # W50: a session file without the views of its diagram: an error names the member (no older files)
        with zipfile.ZipFile(os.path.join(r, 'noviews.snapx'), 'w') as out:
            for n in z.namelist():
                if n != 'auto/views.txt':
                    out.writestr(n, z.read(n))
        del allev[:]
        answered(snd, col, ('d',), cmd='open', file='noviews.snapx')
        msgs = ' '.join(str(e.get('error', '')) for e in allev if e.get('ev') == 'message')
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

        win_rows = members['windows.set'].decode().split('\n')
        set_rows = members['model.set'].decode().split('\n')
        random_rows = members['random.txt'].decode().split('\n')
        # the line (from 1) of a member's rows that ends with label
        line_of = lambda rows, label: next(k + 1 for k in reversed(range(len(rows))) if rows[k].rstrip('\r').endswith(label))
        # the line of model.set's last value (the BVP range's high end)
        set_high = line_of(set_rows, 'BVP range high')
        set_nout, set_dt = line_of(set_rows, ' nout'), line_of(set_rows, 'DeltaT')
        # windows.set's added columns: their count, then VW's name and formula
        win_added = line_of(win_rows, 'added columns')
        generator = random_rows[-1].split(' ')
        manifest_lines = len(members['session.txt'].decode().rstrip('\n').split('\n'))
        damages = [
            ('badslider', lambda m: m.__setitem__('sliders.set', text_lines('sliders.set', 3, 'nonsense low')),
             ('badslider.snapx/sliders.set:3:', 'nonsense')),
            ('badsliderrange', lambda m: m.__setitem__('sliders.set', text_lines('sliders.set', 4, '-2 high')),
             ('badsliderrange.snapx/sliders.set:4:', 'slider range -1 -2')),
            ('nannullcline', lambda m: m.__setitem__('nullclines.set', text_lines('nullclines.set', 4, 'nan coordinate')),
             ('nannullcline.snapx/nullclines.set:4:', 'nan')),
            ('badnullcline', lambda m: m.__setitem__('nullclines.set', text_lines('nullclines.set', 3, '10001 segments')),
             ('badnullcline.snapx/nullclines.set:3:', '10001')),

            ('nowindows', lambda m: m.pop('windows.set'), 'its windows.set is missing'),
            ('cutwindows', lambda m: m.__setitem__('windows.set', '\n'.join(win_rows[:10]).encode() + b'\n'),
             ('cutwindows.snapx/windows.set:11:', 'the file ends here')),
            ('badset', lambda m: m.__setitem__('model.set', text_lines('model.set', 5, 'x' + set_rows[4][1:])),
             ('badset.snapx/model.set:5:', '"x')),
            ('badname', lambda m: m.__setitem__('windows.set', text_lines('windows.set', 5, 'nosuch')),
             ('badname.snapx/windows.set:5:', 'the model has no variable "nosuch"')),
            # W125: a bad value on a member's last line: refused at it, nothing applied
            ('lastmarks', lambda m: m.__setitem__('marks.set', m['marks.set'].rstrip(b'\n') + b'\nmore\n'),
             ('lastmarks.snapx/marks.set:%d:' % (len(members['marks.set'].decode().rstrip('\n').split('\n')) + 1), '"more" after the end')),
            ('lastset', lambda m: m.__setitem__('model.set', text_lines('model.set', set_high, '1e999  BVP range high')),
             ('lastset.snapx/model.set:%d:' % set_high, '"1e999  BVP range high" is not a number')),
            ('latermanifest', lambda m: m.__setitem__('session.txt', m['session.txt'] + b'later 1\n'),
             ('latermanifest.snapx/session.txt:%d:' % (manifest_lines + 1), 'not a line it has')),
            # W145: every value checked by the rule that checks it anywhere else, before anything is applied
            ('zeronout', lambda m: m.__setitem__('model.set', text_lines('model.set', set_nout, '0   nout')),
             ('zeronout.snapx/model.set:%d:' % set_nout, 'nOutput must be a whole number of at least 1')),
            ('zerodt', lambda m: m.__setitem__('model.set', text_lines('model.set', set_dt, '0  DeltaT')),
             ('zerodt.snapx/model.set:%d:' % set_dt, 'Dt must be a number other than 0')),
            ('oldset', lambda m: m.__setitem__('model.set', m['model.set'].rstrip(b'\n') + b'\nRHS etc ...\ndV/dT=0\n'),
             ('oldset.snapx/model.set:%d:' % (len(members['model.set'].decode().rstrip('\n').split('\n')) + 1),
              '"RHS etc ..." after the end')),
            ('manycolumns', lambda m: m.__setitem__('windows.set', text_lines('windows.set', win_added, '5001 added columns')
                                                    + b''.join(b'X%d\nv*w\n' % k for k in range(5000))),
             ('manycolumns.snapx/windows.set:%d:' % win_added, '5001 added columns: the model has room for 0 to')),
            ('badformula', lambda m: m.__setitem__('windows.set', text_lines('windows.set', win_added + 2, 'unknown_symbol+')),
             ('badformula.snapx/windows.set:%d:' % (win_added + 2), 'the formula of the added column VW does not compile')),
            ('spareflag', lambda m: m.__setitem__('random.txt', text_lines('random.txt', len(random_rows),
                                                                           ' '.join(generator[:-2] + ['9', generator[-1]]))),
             ('spareflag.snapx/random.txt:%d:' % len(random_rows), "not a random generator's state")),
            ('aftergenerator', lambda m: m.__setitem__('random.txt', m['random.txt'] + b'\nnot_generator_state\n'),
             ('aftergenerator.snapx/random.txt:%d:' % (len(random_rows) + 1), '"not_generator_state" after the end')),
            ('marktype', lambda m: m.__setitem__('marks.set', b'0\n1\n0\n999\n0\n1\n0\n0\n1\n1\n0\n0\n'),
             ('marktype.snapx/marks.set:4:', "999 is not an object's type (0 to 7)")),
            ('markcolor', lambda m: m.__setitem__('marks.set', b'0\n1\n0\n2\n999\n1\n0\n0\n1\n1\n0\n0\n'),
             ('markcolor.snapx/marks.set:5:', '999 is not a colour (0 to 10)')),
            ('twonames', lambda m: m.__setitem__('session.txt', m['session.txt'] + b'name lecar.odex\n'),
             ('twonames.snapx/session.txt:%d:' % (manifest_lines + 1), 'name a second time')),
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
            answered(snd, col, ('d',), cmd='open', file=name + '.snapx')
            msgs = ' '.join(placed(e) for e in allev if e.get('ev') == 'message')
            check('W116: %s.snapx is refused, the error says where (%s), the session before stays' % (name, expect),
                  all(x in msgs for x in (expect if isinstance(expect, tuple) else (expect,))) and not last('hello'), msgs[:300])
        snd(cmd='state')
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

    def menu(k, *answers, win='auto'):
        """key k of window win, its asks answered in order, up to idle"""
        snd(cmd='key', win=win, key=k)
        got, pending = [], list(answers)
        while True:
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            allev.extend(evs)
            if e is None or e['ev'] == 'idle':
                return got
            snd(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    av = lambda evs: [e for e in evs if e.get('ev') == 'autoview']
    dg = lambda evs, view=None: [e for e in evs if e.get('ev') == 'diagram' and (view is None or e.get('view') == view)]
    views_n = lambda evs: [e['n'] for e in dg(evs) if e['op'] == 'views']
    last_axes = lambda view: next((e for e in reversed(dg(allev, view)) if e['op'] in ('axes', 'reset')), None)
    same_points = lambda a, b: len(a) == len(b) and [x[:3] for x in a] == [x[:3] for x in b]
    try:
        col(is_idle)
        after(cmd='data', events=['autoinfo'])
        allev.extend(lecar_to_auto(snd, col))
        v0 = rebuild_diagram(allev, [])
        evs = after(cmd='auto', op='view', new=1)
        check('views: a new view says there are two, then sends the diagram at the same axes, view 0 unchanged',
              views_n(evs) == [2] and len(v0) > 1 and rebuild_diagram(allev, [], 1) == v0
              and not [e for e in dg(evs, 0) if e['op'] != 'axes'], str([(e.get('op'), e.get('view')) for e in dg(evs)]))
        check('views: the new view is the active one, with a zoom of its own',
              av(evs) and av(evs)[-1]['active'] == 1 and len(av(evs)[-1]['views']) == 2, str(av(evs)))

        evs = after(cmd='auto', op='set', axes={'view': 1, 'plot': 1, 'fit': True})
        a0, a1 = last_axes(0), last_axes(1)
        check("views: a view's axes are its own: view 1 plots the norm, view 0 still hI-lo",
              a1 and a1['plot'] == 1 and a1['ylabel'] == 'Norm' and a0 and a0['plot'] == 2
              and not [e for e in dg(evs, 0) if e['op'] != 'axes'], '%s | %s' % (a0, a1))
        v1 = rebuild_diagram(allev, [], 1)
        check('views: every view holds the same points in the same order (branch, point, kind), at its own axes',
              same_points(v0, v1) and rebuild_diagram(allev, [], 0) == v0 and v1 != v0, '%d vs %d' % (len(v0), len(v1)))

        evs = menu('a', {'key': 'v'})
        a2 = last_axes(2)
        check("views: the Axes menu's new view makes a third, the active one, with the active one's axes (the norm)",
              views_n(evs) == [3] and a2 and a2['plot'] == 1 and av(evs) and av(evs)[-1]['active'] == 2
              and rebuild_diagram(allev, [], 2) == v1, '%s %s' % (views_n(evs), a2))

        evs = after(cmd='auto', op='display', view=0, x=[0.1, 0.2], y=None)
        check('views: auto display with a view zooms that view alone',
              av(evs) and [v['zoom']['x'] for v in av(evs)[-1]['views']] == [[0.1, 0.2], None, None], str(av(evs)))

        after(cmd='auto', op='view', active=0)
        after(cmd='auto', op='grab', type='HB', index=1)
        menu('r', {'key': 'p'})
        lists = [rebuild_diagram(allev, [], v) for v in range(3)]
        check("views: a run's points reach every view, the same points in each",
              len(lists[0]) > len(v0) and same_points(lists[0], lists[1]) and same_points(lists[0], lists[2])
              and lists[0][:len(v0)] == v0, str([len(x) for x in lists]))

        after(cmd='auto', op='view', active=1)
        snd(cmd='key', win='auto', key='g')
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

        evs = after(cmd='auto', op='view', close=0)
        check('views: closing view 0 leaves two, the others move down, the active one stays active (now view 0)',
              views_n(evs) == [2] and av(evs) and av(evs)[-1]['active'] == 0 and len(av(evs)[-1]['views']) == 2
              and rebuild_diagram(allev, [], 0) == lists[1] and rebuild_diagram(allev, [], 1) == lists[2],
              '%s %s' % (views_n(evs), av(evs)))
        evs = after(cmd='auto', op='view', close=7)
        check('views: closing a view there is not is refused', any(e.get('ev') == 'message' and 'error' in e for e in evs), '')
        after(cmd='auto', op='view', close=1)
        evs = after(cmd='auto', op='view', close=0)
        check('views: the last view cannot be closed', any(e.get('ev') == 'message' and 'last view' in e.get('error', '')
                                                         for e in evs), str(evs)[:200])
        check('the process is alive after it all', p.poll() is None, '')
    finally:
        stop_server(p, r, snd)



def check_model_bcs():
    """W99: the state event's `bcs` are the model's own boundary conditions;
    a model with no b/bndry lines sends none (the core's default 0 ones are not
    the model's), one with them sends its own."""
    for ode, want in (('examples/ode/amari.odex', 0), ('examples/ode/dumbbvp.odex', 2), ('examples/ode/vdp.odex', 2)):
        p, r, snd, col, _ = launch_server(ode=ode)
        snd(cmd='state')
        evs, e = col(lambda e: e.get('ev') == 'idle')
        st = [x for x in evs if x.get('ev') == 'state']
        n = len(st[-1].get('bcs', [])) if st else -1
        check('bcs: %s sends %d boundary conditions' % (os.path.basename(ode), want), n == want, str(n))
        if want:
            check('bcs: %s formulas are its own' % os.path.basename(ode),
                  all(len(b) == 2 and b[1] not in ('', '0') for b in st[-1]['bcs']), str(st[-1]['bcs']))
        stop_server(p, r, snd)


check_model_bcs()


def read_recx(text):
    """a .recx's parts (docs/protocol.md "Recordings"): the header lines, the
    embedded files {name: lines}, the steps [(step, note)], the fingerprint
    written and the one its files and steps give"""
    lines = text.split('\n')
    header, files, steps, hashed, note = lines[:4], {}, [], [], []
    name, body, in_steps, written = None, None, False, None
    for l in lines[4:]:
        if body is not None:
            hashed.append(l)
            if l == '@end':
                if name is not None:
                    files[name] = body
                body = None
            else:
                body.append(l[1:] if l.startswith('@@') else l)
        elif l.startswith('@file ') or l.startswith('@binary '):
            name, body = l.split(' ', 1)[1], []
            hashed.append(l)
        elif l == '@snapshot':  # the session it began from (W59d): recx_snapshot reads it
            name, body = None, []
            hashed.append(l)
        elif l == '@steps':
            in_steps = True
        elif l.startswith('fingerprint: '):
            written = l[len('fingerprint: '):]
        elif in_steps and l.startswith('#'):
            note.append(l[2:] if l.startswith('# ') else l[1:])
        elif in_steps and l.strip():
            hashed.append(l)
            steps.append((json.loads(l), '\n'.join(note)))
            note = []
    got = hashlib.sha256(''.join(h + '\n' for h in hashed).encode('utf-8')).hexdigest()
    return header, files, steps, written, got


def check_save_recording():
    """W129: key saves replay explicit decisions and recorded choice answers."""
    p, r, snd, col, _ = launch_server()
    def run(**cmd):
        snd(**cmd)
        return col(is_idle, timeout=30 * SLOW)[0]
    try:
        col(is_idle)
        with open(os.path.join(r, 'browser-no.par'), 'wb') as f: f.write(b'unchanged')
        run(cmd='record', op='start')
        for policy in [1, -1, 0]:
            run(cmd='key', key='f')
            snd(cmd='key', key='v', replace=policy)
            _, ask = col(is_ask)
            snd(cmd='answer', id=ask['id'], file='dialog-save.snapx' if policy == 0 else 'key-save.snapx',
                **({'replace': 1} if policy == 0 else {}))
            evs, end = col(lambda e: is_idle(e) or is_ask(e))
            check('W129: key save with explicit command/dialog decision asks no core question',
                  end is not None and is_idle(end), str(end))
        fresh = run(cmd='values', op='write', kind='par', name='browser-new.par', replace=0)
        snd(cmd='values', op='write', kind='par', name='browser-no.par', replace=0)
        _, ask = col(is_ask)
        snd(cmd='answer', id=ask['id'], key='n')
        declined, _ = col(is_idle)
        run(cmd='record', op='stop', name='key-save.recx')
        path = os.path.join(r, 'key-save.recx')
        _, _, steps, written, got = read_recx(open(path, encoding='utf-8').read())
        saves = [steps[i][0] for i in [1, 3, 5]]
        check('W129: recorded key saves retain explicit Yes and No in the command',
              [v.get('cmd', {}).get('replace') for v in saves[:2]] == [1, -1]
              and written == got, str(saves))
        check('W129: the OS dialog file answer is recorded unchanged with replace 1',
              saves[2].get('keys') == ['v'] and saves[2].get('answers') == [{'file': 'dialog-save.snapx', 'replace': 1}], str(saves[2]))
        check('W129: core new-file and No decisions are recorded as owner answers',
              [steps[i][0].get('answers') for i in [6, 7]] == [[{'save_replace': 1}], [{'save_replace': -1}]]
              and not steps[7][0].get('keys')
              and any(e.get('saved') for e in fresh)
              and any(e.get('ev') == 'saved' and not e.get('saved') for e in declined), str(steps[6:]))
        snd(cmd='play', op='open', file=path)
        evs, end = col(lambda e: is_ask(e) or is_idle(e))
        if end and is_ask(end):
            snd(cmd='answer', id=end['id'], key='d')
            col(is_idle)
        run(cmd='play', op='speed', speed=8)
        prepared = run(cmd='session', op='save', name='dialog-save.snapx', replace=1)
        check('W129: the replay folder already contains the native dialog destination',
              any(e.get('ev') == 'saved' and e.get('saved') and e.get('file') == 'dialog-save.snapx' for e in prepared), str(prepared)[-200:])
        run(cmd='values', op='write', kind='par', name='browser-new.par', replace=1)
        snd(cmd='play', op='start')
        evs, end = col(lambda e: e.get('ev') == 'state'
                        and (e.get('player') or {}).get('step') == 8
                        and e['player']['running'] == -1, timeout=120 * SLOW)
        check('W129: Play reuses explicit/dialog decisions when the destination now exists',
              end is not None and [e['saved'] for e in evs if e.get('ev') == 'saved'] == [True, False, True, True, False]
              and not any(e.get('ev') == 'message' and e.get('error') for e in evs), str(evs)[-500:])
        col(is_idle)
        listing = run(cmd='file', op='list')
        names = {f['name'] for e in listing if e.get('ev') == 'file' for f in e.get('files', [])}
        check('W129: replay preserves a core No even when its target no longer exists',
              any(e.get('ev') == 'saved' and e.get('file') == 'browser-no.par' and not e.get('saved') for e in evs)
              and 'browser-no.par' not in names and 'browser-new.par' in names
              and not any(e.get('ev') == 'ask' and e.get('kind') == 'choice'
                          and e.get('question', '').endswith(' exists. Replace it?') for e in evs), str(names))
    finally:
        stop_server(p, r, snd)


check_save_recording()


def check_quit():
    """W59d: every way of leaving asks one question: File/Quit (F Q) and the
    quit that asks ({"cmd":"quit","ask":true}: the desktop window's File >
    Quit and close box): Save session (s), Don't save (d), Cancel; s saves
    the session (and a recording in progress) then exits, d exits, a cancel
    keeps the session; a quit during a computation stops it, then asks. The
    plain quit (scripts, --server's clients) exits at once, asking nothing.
    W110: hello carries the question, which the page asks itself during a
    run; its Save session is {"cmd":"quit","save":true} (the run stops, the
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
        snd(cmd='key', key='f')
        col(is_idle)
        _, ask = asked(snd, col, cmd='key', key='q')
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
        snd(cmd='state')
        evs, _ = col(is_idle)
        check('quit: Cancel (Esc) keeps xppautX running', last_state(evs) is not None and p.poll() is None, '')
        # a question open when the window's quit comes is cancelled; the quit then asks its own
        _, ask = asked(snd, col, cmd='key', key='i')
        snd(cmd='quit', ask=True)
        evs, ask2 = col(is_ask, timeout=30 * SLOW)
        check('quit with ask while a menu is open: the menu is cancelled (its idle), then the question of the quit',
              ask and is_ask(ask) and ask.get('kind') == 'menu' and any(is_idle(e) for e in evs)
              and ask2 and ask2.get('question') == question, str(ask2))
        if ask2:
            snd(cmd='answer', id=ask2['id'], ok=0)
        col(is_idle)
        # the window's quit asks the same; d exits, saving nothing
        _, ask = asked(snd, col, cmd='quit', ask=True)
        check('quit with ask (the window\'s Quit and close box) asks the same question',
              ask and is_ask(ask) and ask.get('question') == question and ask.get('keys') == 'sd', str(ask))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='d')
        _, ok = ended(p, col)
        check('quit: Don\'t save (d) exits (bye, status 0), writing nothing', ok and os.listdir(r) == [os.path.basename(args.ode)],
              str(os.listdir(r)))
    finally:
        done(p, r, snd)

    # s saves the session, and the recording in progress with it, then exits
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='record', op='start')
        col(is_idle)
        _, ask = asked(snd, col, cmd='key', key='i')
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='g')
            col(is_idle, timeout=30 * SLOW)
        _, ask = asked(snd, col, cmd='quit', ask=True)
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
              ok and os.path.exists(snap) and 'session.txt' in zipfile.ZipFile(snap).namelist()
              and [x.get('keys') for x, _ in steps] == [['i', 'g']] and written == got, str(sorted(os.listdir(r))))
    finally:
        done(p, r, snd)

    # a quit that asks during a computation stops it, then asks
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='key', key='u')
        col(is_idle)
        _, ask = asked(snd, col, cmd='key', key='t')
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], value='1e7')
            col(is_idle)
        snd(cmd='key', key='Escape')
        col(is_idle)
        _, ask = asked(snd, col, cmd='key', key='i')
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='g')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        snd(cmd='quit', ask=True)
        evs, ask = col(is_ask, timeout=30 * SLOW)
        kinds = [e.get('ev') for e in evs if e.get('ev') in ('stopped', 'idle', 'ask')]
        check('quit during a run: the run stops first (stopped, idle), then the question',
              kinds[:3] == ['stopped', 'idle', 'ask'] and ask and ask.get('question') == question, str(kinds))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        # the plain quit: at once, nothing asked
        snd(cmd='quit')
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
        snd(cmd='key', key='u')
        col(is_idle)
        _, ask = asked(snd, col, cmd='key', key='t')
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], value='1e7')
            col(is_idle)
        snd(cmd='key', key='Escape')
        col(is_idle)
        _, ask = asked(snd, col, cmd='key', key='i')
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='g')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)

    # W110: the page asked during a run and was answered Save session: the
    # run stops, the session is saved (its file asked), then the exit
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        long_run(snd, col)
        snd(cmd='quit', save=True)
        evs, ask = col(is_ask, timeout=30 * SLOW)
        kinds = [e.get('ev') for e in evs if e.get('ev') in ('stopped', 'idle', 'ask')]
        check('quit with save (the page\'s Save session) during a run: the run stops, then the session file is asked, '
              'no question before it', kinds[:3] == ['stopped', 'idle', 'ask'] and ask and ask.get('kind') == 'file'
              and ask.get('wild') == '*.snapx', str(kinds) + ' ' + str(ask))
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], file='kept')
        _, ok = ended(p, col)
        snap = os.path.join(r, 'kept.snapx')
        check('quit with save: the session saved, then the exit (bye, status 0)',
              ok and os.path.exists(snap) and 'session.txt' in zipfile.ZipFile(snap).namelist(), str(sorted(os.listdir(r))))
    finally:
        done(p, r, snd)

    # a cancelled save keeps the session (its run already stopped); the
    # page's Don't save is the plain quit, which ends a run at once
    p, r, snd, col, _ = launch_server()
    try:
        col(is_idle)
        snd(cmd='quit', save=True)
        _, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], ok=0)
        col(is_idle)
        snd(cmd='state')
        evs, _ = col(is_idle)
        check('quit with save: a cancelled save keeps xppautX running', ask and is_ask(ask) and ask.get('kind') == 'file'
              and last_state(evs) is not None and p.poll() is None, str(ask))
        long_run(snd, col)
        snd(cmd='quit')
        try:
            p.wait(timeout=10 * SLOW)
        except subprocess.TimeoutExpired:
            pass
        check('the plain quit during a run (the page\'s Don\'t save) exits at once, asking nothing, saving nothing',
              p.poll() == 0 and os.listdir(r) == [os.path.basename(args.ode)], str(p.poll()) + ' ' + str(os.listdir(r)))
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
        st = last_state(run(cmd='record', op='start'))
        check('record: start, and state says recording, no step yet',
              st and st.get('recording') == {'steps': 0, 'note': ''}, str(st and st.get('recording')))
        st = last_state(run(cmd='record', op='note', text='First run.\nThe cell settles.'))
        check('record: a note waits for the next step (state.recording.note)',
              st and st['recording'] == {'steps': 0, 'note': 'First run.\nThe cell settles.'}, str(st and st.get('recording')))
        ask = asked(cmd='key', key='i')
        run(cmd='answer', id=ask['id'], key='g')
        ask = asked(cmd='key', key='i')
        run(cmd='answer', id=ask['id'], ok=0)
        run(cmd='key', key='u')
        ask = asked(cmd='key', key='t')
        run(cmd='answer', id=ask['id'], value='1e7')
        run(cmd='key', key='Escape')
        ask = asked(cmd='key', key='i')
        snd(cmd='answer', id=ask['id'], key='g')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        snd(cmd='abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        stopped = [e['at'] for e in evs if e.get('ev') == 'stopped']
        run(cmd='display', win=1, x=[0, 50])
        st = last_state(run(cmd='set', kind='par', name='iapp', value=0.1))
        check('record: state counts the steps taken', st and st['recording']['steps'] == 8, str(st and st.get('recording')))
        evs = run(cmd='record', op='stop', name='mine')
        path = os.path.join(r, 'mine.recx')
        check('record: stop writes name.recx and the recording ends',
              os.path.exists(path) and 'recording' not in (last_state(evs) or {'recording': 1}), str(os.listdir(r)))
        text = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
        header, files, steps, written, got = read_recx(text)
        ode = os.path.basename(args.ode)
        check('record: the header names the format, program, model and date',
              header[0] == 'xppautx-recording 1' and header[1].startswith('program: xppautX ')
              and header[2] == 'model: ' + ode and re.match(r'recorded: \d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$', header[3]),
              str(header))
        want = open(os.path.join(r, ode), encoding='utf-8').read().splitlines()
        check('record: the model is embedded whole, and nothing else was read',
              list(files) == [ode] and files[ode] == want, str(list(files)))
        s = [x for x, _ in steps]
        check('record: I G is one step, its keys and label from the menus, its note above it',
              s and s[0] == {'step': 'Initialconds → Go', 'keys': ['i', 'g']} and steps[0][1] == 'First run.\nThe cell settles.',
              str(steps[:1]))
        check('record: a cancelled menu is a step (keys i, Escape)',
              len(s) > 1 and s[1] == {'step': 'Initialconds', 'keys': ['i', 'Escape']} and steps[1][1] == '', str(s[1:2]))
        check('record: a dialog\'s answer is in its step (nUmerics, Total 1e7, Esc)',
              s[2:5] == [{'step': 'nUmerics', 'keys': ['u'], 'view': True},
                         {'step': 'nUmerics → Total', 'keys': ['t'], 'answers': ['1e7']},
                         {'step': 'nUmerics → [Esc]-exit', 'keys': ['Escape'], 'view': True}], str(s[2:5]))
        check('record: an Abort belongs to its step, where it stopped as the stopped event said',
              len(s) > 5 and s[5].get('keys') == ['i', 'g'] and stopped and s[5].get('abort') == stopped[0]
              and stopped[0]['what'] == 'integrate', str(s[5:6]) + str(stopped))
        check('record: a zoom is a view step with its command; a set a step of its own',
              s[6:8] == [{'step': 'Zoom window 1', 'cmd': {'cmd': 'display', 'win': 1, 'x': [0, 50]}, 'view': True},
                         {'step': 'Set iapp = 0.1', 'cmd': {'cmd': 'set', 'kind': 'par', 'name': 'iapp', 'value': 0.1}}]
              and len(s) == 8, str(s[6:]))
        check('record: the fingerprint is the files\' and steps\' SHA-256', written == got and len(got) == 64, '%s %s' % (written, got))
        edited = text.replace('# First run.', '# The first run.')
        _, _, _, w2, g2 = read_recx(edited)
        check('record: editing a note keeps the fingerprint', edited != text and w2 == g2, '')
        edited = text.replace('"keys":["i","Escape"]', '"keys":["i","c"]')
        _, _, _, w2, g2 = read_recx(edited)
        check('record: editing a step changes it', edited != text and w2 != g2, '')

        # File/recorD starts and stops; the File menu opened to stop is no step
        run(cmd='values', op='write', kind='par', name='saved.par')
        run(cmd='key', key='f')
        run(cmd='key', key='d')
        run(cmd='values', op='read', kind='par', name='saved.par')
        run(cmd='key', key='e')
        run(cmd='key', key='f')
        snd(cmd='key', key='d')
        evs, ask = col(lambda e: is_ask(e) or is_idle(e))
        check('File/recorD again asks the name, as the other File saves (*.recx, the model\'s name)',
              ask and ask.get('kind') == 'file' and ask.get('wild') == '*.recx' and ask.get('file') == 'lecar.recx', str(ask))
        if ask and ask.get('kind') == 'file':
            run(cmd='answer', id=ask['id'], file='menu')
        _, files, steps, written, got = read_recx(open(os.path.join(r, 'menu.recx'), encoding='utf-8').read()
                                                  if os.path.exists(os.path.join(r, 'menu.recx')) else '')
        check('File/recorD: the steps between (the file a step read named by its section), and not the File menu opened to stop',
              [x for x, _ in steps] == [{'step': 'Values read', 'cmd': {'cmd': 'values', 'op': 'read', 'kind': 'par', 'name': 'saved.par'},
                                         'files': [1]},
                                        {'step': 'Erase', 'keys': ['e'], 'view': True}] and written == got, str(steps))
        par = open(os.path.join(r, 'saved.par'), encoding='utf-8').read().splitlines()
        check('record: a file the session read while recording is embedded after the model',
              list(files) == [ode, 'saved.par'] and files['saved.par'] == par, str(list(files)))
        evs = run(cmd='record', op='note', text='x')
        check('record: a note while not recording is an error',
              any(e.get('ev') == 'message' and 'Not recording' in e.get('error', '') for e in evs), '')
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
        run(cmd='browser', op='write', what='output', format='dat', name=name, replace=1)

    def open_player(name):
        """play open, answering the question about this model's session"""
        snd(cmd='play', op='open', file=name)
        evs, ask = col(lambda e: is_ask(e) or is_idle(e), timeout=30 * SLOW)
        if ask and is_ask(ask):
            snd(cmd='answer', id=ask['id'], key='d')
            evs, _ = col(is_idle, timeout=30 * SLOW)
        return next((e for e in evs if e.get('ev') == 'player'), None), evs

    def play_to_end(n):
        """play start, at 8x, until the player's last step ended: the events"""
        run(cmd='play', op='speed', speed=8)
        snd(cmd='play', op='start')
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
        run(cmd='set', kind='par', name='phi', value=0.06)
        ask = asked(cmd='key', key='i')
        run(cmd='answer', id=ask['id'], key='g')
        run(cmd='record', op='start')
        run(cmd='record', op='note', text='The cell fires once and settles.')
        ask = asked(cmd='key', key='i')
        run(cmd='answer', id=ask['id'], key='g')
        write('run1.dat')
        run(cmd='set', kind='par', name='iapp', value=0.08)
        ask = asked(cmd='key', key='i')
        run(cmd='answer', id=ask['id'], key='g')
        write('run2.dat')
        run(cmd='key', key='u')
        ask = asked(cmd='key', key='t')
        run(cmd='answer', id=ask['id'], value='1e7')
        run(cmd='key', key='Escape')
        ask = asked(cmd='key', key='i')
        snd(cmd='answer', id=ask['id'], key='g')
        col(lambda e: e.get('ev') == 'progress', timeout=30 * SLOW)
        snd(cmd='abort')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        stopped = [e['at'] for e in evs if e.get('ev') == 'stopped']
        write('run3.dat')
        run(cmd='record', op='stop', name='play')
        path = os.path.join(r, 'play.recx')
        text = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
        _, _, steps, _, _ = read_recx(text)
        orig = {n: open(os.path.join(r, n), 'rb').read() for n in ('run1.dat', 'run2.dat', 'run3.dat')
                if os.path.exists(os.path.join(r, n))}
        check('player: the session recorded (10 steps, an abort at its row)',
              len(steps) == 10 and len(orig) == 3 and stopped and stopped[0]['what'] == 'integrate',
              '%d steps, %s' % (len(steps), stopped))
        snap = recx_snapshot(text) or {}
        check('record: the recording begins with the session as it was (@snapshot: a .snapx without the data table)',
              'session.txt' in snap and 'model/lecar.odex' in snap and 'model.set' in snap and 'data.npz' not in snap,
              str(sorted(snap)))
        # no fallback: a recording without its snapshot is refused
        lines = text.split('\n')
        at = lines.index('@snapshot') if '@snapshot' in lines else 0
        cut = '\n'.join(lines[:at] + lines[lines.index('@end', at) + 1:]) if at else text
        open(os.path.join(r, 'nosnap.recx'), 'w', encoding='utf-8').write(cut)
        evs = run(cmd='play', op='open', file=os.path.join(r, 'nosnap.recx'))
        check('play open: a recording without its @snapshot is an error, nothing asked',
              any(e.get('ev') == 'message' and 'no @snapshot section' in e.get('error', '') for e in evs)
              and not any(is_ask(e) for e in evs), str([e for e in evs if e.get('ev') == 'message']))

        pl, evs = open_player(path)
        st = last_state(evs)
        check('play open: the player event with the steps and their notes, the file intact',
              pl and pl.get('intact') is True and len(pl.get('steps', [])) == 10
              and pl['steps'][0].get('note') == 'The cell fires once and settles.'
              and pl['steps'][0].get('keys') == ['i', 'g'], str(pl)[:300])
        check('play open: state.player at step 0, paused, 1x',
              st and st.get('player') == {'step': 0, 'running': -1, 'playing': False, 'speed': 1, 'fast': False,
                                          'intact': True}, str(st and st.get('player')))
        os.remove(os.path.join(r, 'run1.dat'))  # nothing may read the disk's copies
        evs = play_to_end(10)
        presses = [(e['step'], e['what'], e['index']) for e in evs if e.get('ev') == 'press']
        first = [e.get('ev') for e in evs if e.get('ev') in ('press', 'ask')][:3]
        check('play: a press before every input (key i, the menu asks, key g; the answers; the commands)',
              presses[:2] == [(0, 'key', 0), (0, 'key', 1)] and first == ['press', 'ask', 'press']
              and (2, 'cmd', 0) in presses and (6, 'answer', 0) in presses, str(presses[:12]))
        # W150: 1x is slower (core/json_player.cpp PACE_SLOWDOWN 1.5): the first press of step 0 (700 ms + the note's
        # reading time, 35 ms a character, at most 3 s), played at 8x
        note0 = 'The cell fires once and settles.'
        want0 = (700 + min(3000, 35 * len(note0))) * 1.5 / 8
        ms0 = next((e['ms'] for e in evs if e.get('ev') == 'press' and e['step'] == 0 and e['index'] == 0), None)
        check('play: the first press of a step is paced 1.5 times the old pace (%.1f ms at 8x)' % want0,
              ms0 is not None and abs(ms0 - want0) <= 2, str(ms0))
        again = [e['at'] for e in evs if e.get('ev') == 'stopped']
        check('play: the recorded Abort stops the replayed run at its row', again == stopped, '%s %s' % (again, stopped))
        errors = [e.get('error') for e in evs if e.get('ev') == 'message' and e.get('error')]
        check('play: no error on the way', not errors, str(errors))
        for n in ('run1.dat', 'run2.dat', 'run3.dat'):
            check('play: the replay, from the session\'s state at Record (phi 0.06), gives the same data (%s)' % n,
                  get(n) == orig.get(n) and orig.get(n), 'differs' if n in orig else 'missing')
        check('play: the replay wrote in its own folder, not beside the recording',
              not os.path.exists(os.path.join(r, 'run1.dat')), str(sorted(os.listdir(r))))

        # a note saved keeps the fingerprint
        evs = run(cmd='play', op='note', step=3, text='Now the drive current is raised.')
        pl = next((e for e in evs if e.get('ev') == 'player'), None)
        _, _, steps2, w2, g2 = read_recx(open(path, encoding='utf-8').read())
        check('play note: written above its step in the file, which stays intact',
              pl and pl['steps'][3].get('note') == 'Now the drive current is raised.' and pl.get('intact') is True
              and steps2[3][1] == 'Now the drive current is raised.' and w2 == g2, str(steps2[3:4]))

        # a changed step: a warning (intact false) and it still plays
        changed = text.replace('"value": 0.08', '"value": 0.09')
        open(os.path.join(r, 'changed.recx'), 'w', encoding='utf-8').write(changed)
        pl, evs = open_player(os.path.join(r, 'changed.recx'))
        check('play open: a changed step makes the file not intact', pl and pl.get('intact') is False and changed != text,
              str(pl and pl.get('intact')))
        evs = play_to_end(10)
        check('play: a changed recording still plays to its end',
              (last_state(evs) or {}).get('player', {}).get('step') == 10, '')
        check('play: its changed step ran as it now reads (other data)', get('run2.dat') not in (None, orig.get('run2.dat')), '')

        # play from a step: the steps before run with no pace or press
        snd(cmd='play', op='from', step=8, play=0)
        evs, _ = col(lambda e: e.get('ev') == 'state' and (e.get('player') or {}).get('step') == 8
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
              pl is not None and len(pl.get('steps', [])) == 10 and not any(e.get('ev') == 'ask' for e in evs),
              done.stderr[-300:])

        # a recording that is not one
        open(os.path.join(r, 'bad.recx'), 'w').write('hello\n')
        evs = run(cmd='play', op='open', file=os.path.join(r, 'bad.recx'))
        check('play open: a file that is not a recording is an error',
              any(e.get('ev') == 'message' and 'not a recording' in e.get('error', '') for e in evs), '')

        # W125: a recording whose last step does not read is refused at its line, before
        # anything is played or loaded: the player as it was
        rows = text.split('\n')
        k = max(i for i, l in enumerate(rows) if l.startswith('{'))
        rows[k] = 'not json'
        open(os.path.join(r, 'laststep.recx'), 'w', encoding='utf-8').write('\n'.join(rows))
        before = (last_state(run(cmd='state')) or {}).get('player')
        evs = run(cmd='play', op='open', file=os.path.join(r, 'laststep.recx'))
        m = next((e for e in evs if e.get('ev') == 'message' and e.get('error')), {})
        check('W125: a recording whose last step does not read is refused at that line, the line as written',
              os.path.basename(m.get('file', '')) == 'laststep.recx' and m.get('line') == k + 1
              and m.get('source') == 'not json' and 'cannot be played' in m.get('error', ''), str(m))
        check('W125: after the refused recording the player is as it was',
              before is not None and (last_state(run(cmd='state')) or {}).get('player') == before, str(before))
    finally:
        stop_server(p, r, snd)


def check_ani_grab_job():
    # An armed row stop makes grab integration/cancellation independent of speed.
    grab_rows = 400  # strictly inside lecar's default 601 stored rows
    with tempfile.TemporaryDirectory(prefix='xppgrab') as r:
        shutil.copy(args.ode, r)
        for name, runnow in [('grab.ani', 1), ('view.ani', 0)]:
            with open(os.path.join(r, name), 'w') as f:
                f.write(chr(10).join(['dimension -1;-1;1;1', 'speed 0', 'transient',
                                       'circle 0;0;0.1;$RED;1', 'grab 0;0;0.2',
                                       '{w=w}', '{runnow=%d}' % runnow, 'end', '']))
        lines = [
            {'cmd': 'key', 'key': 'i'}, {'cmd': 'answer', 'key': 'g'},
            {'cmd': 'key', 'key': 'v'}, {'cmd': 'answer', 'key': 't'},
            {'cmd': 'key', 'win': 'ani', 'key': 'f'}, {'cmd': 'answer', 'file': 'grab.ani'},
            {'cmd': 'record', 'op': 'start'},
            {'cmd': 'key', 'win': 'ani', 'key': 'a'},
            {'cmd': 'ani', 'op': 'mouse', 'what': 'down', 'u': 0.5, 'v': 0.5},
            {'cmd': 'ani', 'op': 'mouse', 'what': 'up', 'u': 0.5, 'v': 0.5},
            {'cmd': 'abort', 'at': {'what': 'integrate', 'rows': grab_rows, 't': 0}},
            {'cmd': 'ani', 'op': 'pause'},
            {'cmd': 'key', 'win': 'ani', 'key': 'f'}, {'cmd': 'answer', 'file': 'view.ani'},
            {'cmd': 'key', 'win': 'ani', 'key': 'a'},
            {'cmd': 'ani', 'op': 'mouse', 'what': 'down', 'u': 0.5, 'v': 0.5},
            {'cmd': 'ani', 'op': 'mouse', 'what': 'up', 'u': 0.5, 'v': 0.5},
            {'cmd': 'record', 'op': 'stop', 'name': 'grab'},
        ]
        script = os.path.join(r, 'grab.jsonl')
        with open(script, 'w') as f:
            f.write(''.join(json.dumps(save_permission(c)) + chr(10) for c in lines))
        result = subprocess.run([os.path.abspath(args.server), '--script', script, os.path.basename(args.ode)],
                                cwd=r, capture_output=True, text=True, encoding='utf-8', timeout=60 * SLOW)
        evs = [json.loads(l) for l in result.stdout.splitlines() if l.strip()]
        stopped = [e for e in evs if e.get('ev') == 'stopped']
        check('W136: animator grab computes as a job and stops at its armed row',
              result.returncode == 0 and sum(e.get('ev') == 'computing' for e in evs) == 2
              and len(stopped) == 1 and stopped[0]['at'].get('rows') == grab_rows, result.stderr[-300:])
        path = os.path.join(r, 'grab.recx')
        text = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
        _, _, steps, _, _ = read_recx(text)
        mouse = [x for x, _ in steps if x.get('cmd', {}).get('op') == 'mouse']
        check('W136: grab down is a view; integrating release is a computing recording step',
              len(mouse) == 4 and mouse[0].get('view') is True and not mouse[1].get('view')
              and all(x.get('view') for x in mouse[2:])
              and mouse[1].get('abort', {}).get('rows') == grab_rows, str(mouse))
        check('W136: pause while idle is recorded as a view without an error',
              any(x.get('cmd', {}).get('op') == 'pause' and x.get('view') for x, _ in steps)
              and not any(e.get('error') for e in evs if e.get('ev') == 'message'), '')


check_ani_grab_job()


def check_runnow_job():
    # A deliberately heavy, effectively unbounded run: Abort follows the
    # computing condition, never a delay or a race to beat completion.
    with tempfile.TemporaryDirectory(prefix='xpprunnow') as source:
        model = os.path.join(source, 'runnow.odex')
        with open('tools/models/heavy.odex', encoding='utf-8') as f:
            heavy = f.read().replace('total=20', 'total=1e7')
        for option in ('model', 'cli'):
            with open(model, 'w', encoding='utf-8') as f:
                f.write(heavy + (chr(10) + '@ runnow=1' + chr(10) if option == 'model' else ''))
            p, r, snd, col, _ = launch_server(ode=model, extra_args=['--runnow'] if option == 'cli' else [])
            try:
                _, comp = col(lambda e: e.get('ev') == 'computing', timeout=30 * SLOW)
                check('W136: server runnow (%s) sends computing' % option, comp is not None)
                snd(cmd='abort')
                evs, idle = col(is_idle, timeout=30 * SLOW)
                stopped = next((e for e in evs if e.get('ev') == 'stopped'), None)
                check('W136: Abort stops startup runnow (%s) as an integration job' % option,
                      idle and stopped and stopped['at']['what'] == 'integrate', str(stopped))
            finally:
                stop_server(p, r, snd)


check_runnow_job()


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
            evs, e = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60 * SLOW)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            snd(cmd='answer', id=e['id'], ok=0)

    try:
        col(is_idle)
        run(cmd='data', events=['ani'])
        run(cmd='record', op='start')
        snd(cmd='key', key='i')
        ask = col(lambda e: e.get('ev') == 'ask')[1]
        run(cmd='answer', id=ask['id'], key='g')
        snd(cmd='key', key='v')
        ask = col(lambda e: e.get('ev') == 'ask')[1]
        run(cmd='answer', id=ask['id'], key='t')
        snd(cmd='key', win='ani', key='f')
        ask = col(lambda e: e.get('ev') == 'ask')[1]
        run(cmd='answer', id=ask['id'], file='gui_test.ani')
        run(cmd='ani', op='speed', ms=20)
        snd(cmd='key', win='ani', key='g')
        went, _ = col(lambda e: e.get('ev') == 'ani' and e.get('op') == 'frame' and e['pos'] >= 40, timeout=30 * SLOW)
        snd(cmd='key', key='Escape')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        escape_last = frames(went + evs)[-1:]
        run(cmd='key', win='ani', key='r')
        snd(cmd='key', win='ani', key='g')
        went, _ = col(lambda e: e.get('ev') == 'ani' and e.get('op') == 'frame' and e['pos'] >= 40, timeout=30 * SLOW)
        snd(cmd='ani', op='speed', ms=10)
        snd(cmd='ani', op='pause')
        evs, _ = col(is_idle, timeout=30 * SLOW)
        last = frames(went + evs)[-1:]
        run(cmd='record', op='stop', name='ani')
        text = open(os.path.join(r, 'ani.recx'), encoding='utf-8').read() if os.path.exists(os.path.join(r, 'ani.recx')) else ''
        _, files, steps, _, _ = read_recx(text)
        check('player: Escape during the animation\'s Go is recorded with its frame; the .ani is embedded',
              escape_last and 0 < escape_last[0] < 600 and 'gui_test.ani' in files
              and any(x.get('during', [{}])[0].get('at', {}).get('what') == 'ani' for x, _ in steps),
              '%s %s' % (last, [x for x, _ in steps][-2:]))
        controls = [d for x, _ in steps for d in x.get('during', []) if 'cmd' in d]
        check('W136: speed and pause during Go retain their commands and frames',
              [d['cmd'].get('op') for d in controls] == ['speed', 'pause']
              and all(d['at'].get('what') == 'ani' for d in controls), str(controls))
        os.remove(os.path.join(r, 'gui_test.ani'))  # the replay reads the recording's copy
        snd(cmd='play', op='open', file=os.path.join(r, 'ani.recx'))
        evs, ask = col(lambda e: e.get('ev') in ('ask', 'idle'), timeout=30 * SLOW)
        if ask and ask.get('ev') == 'ask':
            snd(cmd='answer', id=ask['id'], key='d')
            col(is_idle, timeout=30 * SLOW)
        run(cmd='data', events=['ani'])
        run(cmd='play', op='speed', speed=8)
        snd(cmd='play', op='start')
        evs, _ = col(lambda e: e.get('ev') == 'state' and (e.get('player') or {}).get('step') == len(steps)
                     and e['player']['running'] == -1, timeout=120 * SLOW)
        errors = [e.get('error') for e in evs if e.get('ev') == 'message' and e.get('error')]
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
            snd(cmd='open', file='foreign.ode')
            _, leave = col(is_ask)
            snd(cmd='answer', id=leave['id'], key='d')
            return col(lambda e: e.get('ev') in ('ask', 'idle'))

        write_foreign(1)
        _, rename = begin()
        check('ode open: reserved names ask string with the suggested default',
              rename and rename.get('kind') == 'string' and rename.get('value') == 'and_', str(rename))
        snd(cmd='answer', id=rename['id'], ok=False)
        col(is_idle)
        snd(cmd='redraw')
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
        _, settled = col(lambda e: e.get('ev') in ('ask', 'idle'))
        check('ode open: identical conversion opens without an overwrite ask', settled and settled.get('ev') == 'idle')
        write_foreign(3)
        snd(cmd='reload')
        _, leave = col(is_ask)
        snd(cmd='answer', id=leave['id'], key='d')
        reloaded, _ = col(is_idle)
        check('ode open: Reload uses odex after the foreign source changes',
              any(e.get('ev') == 'hello' and 'foreign.odex' in e.get('title', '') for e in reloaded)
              and any(e.get('ev') == 'state' and ['and_', 2] in e.get('pars', []) for e in reloaded))
        with open(converted, 'rb') as f:
            before = f.read()
        silent = subprocess.run([os.path.abspath(args.server), 'foreign.ode', '--silent'], cwd=r,
                                capture_output=True, text=True, timeout=30 * SLOW)
        with open(converted, 'rb') as f:
            after = f.read()
        check('ode open: silent refuses different existing text, names both files and writes nothing',
              silent.returncode == 1 and 'foreign.ode' in silent.stderr and 'foreign.odex' in silent.stderr and before == after)
        os.remove(converted)
        silent = subprocess.run([os.path.abspath(args.server), 'foreign.ode', '--silent'], cwd=r,
                                capture_output=True, text=True, timeout=30 * SLOW)
        check('ode open: silent takes suggestions and prints the saved path',
              silent.returncode == 0 and 'Converted foreign.ode and saved as foreign.odex' in silent.stderr and os.path.exists(converted))
        # A directory at the destination exercises a write failure on Windows
        # too, even for a user who can bypass Unix permission bits.
        with open(os.path.join(r, 'blocked.ode'), 'w') as f:
            f.write("x'=-x\ndone\n")
        os.mkdir(os.path.join(r, 'blocked.odex'))
        snd(cmd='open', file='blocked.ode')
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
check_session_file()
check_session_random()

send(cmd='key', key='f')
send(cmd='key', key='q')
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
print('server checks: %s' % ('all passed' if failures == 0 else '%d failed' % failures))
sys.exit(1 if failures else 0)
