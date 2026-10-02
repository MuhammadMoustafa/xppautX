"""A small client for xppautX --server, shared by the protocol checks.

    s = Server('./xppautX', 'examples/ode/lecar.odex')
    s.collect(is_idle)
    s.send(cmd='key', key='i')
    evs, ask = s.collect(is_ask)

Each Server runs in its own scratch directory (the model is copied there),
reads events on a thread and hands them out with collect().
"""
import base64, copy, json, os, queue, shutil, struct, subprocess, tempfile, threading, time

# XPP_CHECK_SLOW=F multiplies every wait by F, for a server under a slow
# tool (tools/valgrindcheck.sh: memcheck runs it some 30 times slower)
SLOW = float(os.environ.get('XPP_CHECK_SLOW', '1'))


def _reject_non_finite(text):
    """json.loads' parse_constant: NaN/Infinity/-Infinity are not valid
    JSON (W35a, core/json_number.h); a strict parse must fail on them
    instead of silently making a Python float, so a "-nan" that leaks
    past the core's writer fails a check instead of round-tripping."""
    raise ValueError('non-finite JSON constant: %s' % text)


def is_idle(e):
    return e.get('ev') == 'idle'


def is_ask(e):
    return e.get('ev') == 'ask'


def placed(e):
    """An error event's text as the core renders an error (xpp::Error::text,
    docs/protocol.md "Errors"): "file:line:col: error", leaving out what is
    not known; "" for an event with no error."""
    if not e.get('error'):
        return ''
    t, line, col = e.get('file') or '', e.get('line') or 0, e.get('col') or 0
    if line > 0:
        t += (':' if t else 'line ') + str(line)
        if col > 0:
            t += ':' + str(col)
    return (t + ': ' if t else '') + e['error']


def is_state(e):
    return e.get('ev') == 'state'


def drain_stderr(proc, sink=None):
    """Reads a server's stderr on a thread, each line to sink(line) when
    given. With XPP_CHECK_STDERR (a directory; tools/asancheck.sh sets it)
    every line also goes to DIR/server-PID.log: gcc's UBSan, built beside
    ASan, ignores log_path and reports only on stderr, which asancheck
    reads back from there."""
    keep = os.environ.get('XPP_CHECK_STDERR')

    def run():
        out = open(os.path.join(keep, 'server-%d.log' % proc.pid), 'a', errors='replace') if keep else None
        for line in proc.stderr:
            if out:
                out.write(line)
                out.flush()
            if sink:
                sink(line)
        if out:
            out.close()

    threading.Thread(target=run, daemon=True).start()


def series_values(col, enc):
    """a series column's data as Python floats, float32-rounded (NaN for
    null), whatever the encoding"""
    d = col['data']
    if enc == 'f32':
        raw = base64.b64decode(d)
        return list(struct.unpack('<%df' % (len(raw) // 4), raw))
    return [float('nan') if v is None else struct.unpack('<f', struct.pack('<f', v))[0] for v in d]


def decoded_series(ev):
    """a full series event with its columns' data as floats (series_values), no `enc`"""
    h = dict(ev, columns=[dict(c, data=series_values(c, ev.get('enc'))) for c in ev['columns']])
    h.pop('enc', None)
    return h


class SeriesMirror:
    """What a client holds of each plot window's series (docs/protocol.md
    "The plot as data"): the last full `series` and the appends after it. A
    live run ends with an `end` instead of the whole series again; feed()
    gives that event the whole series as `held`, with a full series' fields
    (its columns' data as floats, no `enc`), so a check reads a run's series
    the same way whichever way it came (whole_series)."""

    def __init__(self):
        self.held = {}

    def feed(self, ev):
        if ev.get('ev') != 'series':
            return
        op, win = ev.get('op'), ev.get('win')
        if op is None:
            self.held[win] = decoded_series(ev)
            return
        h = self.held.get(win)
        if h is None:
            return
        if op == 'append':
            have = {c['col']: c for c in h['columns']}
            for c in ev['columns']:
                if c['col'] in have:
                    have[c['col']]['data'] = have[c['col']]['data'][:ev['from']] + series_values(c, ev.get('enc'))
            h['rows'] = ev['rows']
        elif op == 'end' and h['rows'] == ev['rows']:
            h['version'] = ev['version']
            ev['held'] = copy.deepcopy(h)


def whole_series(evs, win=None):
    """the whole series that arrived in evs, in order: each full `series`
    event, and each live run's end as the series the client then holds
    (SeriesMirror), all with their data as floats (decoded_series); only
    window win's when given"""
    return [e['held'] if 'held' in e else decoded_series(e) for e in evs if e.get('ev') == 'series' and e.get('op') in (None, 'end')
            and (win is None or e.get('win') == win)]


class Server:
    def __init__(self, binary, ode, env=None, verbose=False, stdin=subprocess.PIPE):
        self.verbose = verbose
        self.run = tempfile.mkdtemp(prefix='xppserver')
        shutil.copy(ode, self.run)
        full = None
        if env is not None:
            full = dict(os.environ)
            full.update(env)
        self.proc = subprocess.Popen([os.path.abspath(binary), '--server', os.path.basename(ode)], cwd=self.run,
                                     stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, bufsize=1, env=full)
        self.events = queue.Queue()
        self.series = SeriesMirror()
        self.sent_at = {}
        threading.Thread(target=self._read, daemon=True).start()
        drain_stderr(self.proc)

    def auto_dirs(self):
        """AUTO's scratch directories of this process (xpp::files::make_temp_dir)"""
        tmp = tempfile.gettempdir() if os.name == 'nt' else (os.environ.get('TMPDIR') or '/tmp')
        pre = 'xppautoX-%d-' % self.proc.pid
        return [os.path.join(tmp, d) for d in os.listdir(tmp) if d.startswith(pre)]

    def _read(self):
        for line in self.proc.stdout:
            try:
                ev = json.loads(line, parse_constant=_reject_non_finite)
            except ValueError:
                ev = {'ev': 'bad', 'line': line[:300]}
            ev['_t'] = time.monotonic()
            self.series.feed(ev)
            self.events.put(ev)
        self.events.put({'ev': 'eof', '_t': time.monotonic()})

    def send(self, **cmd):
        if self.verbose:
            print('  >', json.dumps(cmd))
        self.proc.stdin.write(json.dumps(cmd) + '\n')
        self.proc.stdin.flush()
        return time.monotonic()

    def collect(self, until, timeout=10 * SLOW):
        """events up to and including the first for which until(ev) is true;
        (events, None) on timeout or end of output"""
        got = []
        while True:
            try:
                ev = self.events.get(timeout=timeout)
            except queue.Empty:
                return got, None
            if self.verbose:
                s = json.dumps(ev)
                print('  <', s[:160] + ('...' if len(s) > 160 else ''))
            got.append(ev)
            if until(ev):
                return got, ev
            if ev.get('ev') == 'eof':
                return got, None

    def answer_asks(self, until, replies, timeout=20 * SLOW):
        """collect up to until(ev), answering asks whose kind is in replies
        (kind -> function of the ask returning the answer's fields)"""
        got = []
        while True:
            evs, e = self.collect(lambda e: is_ask(e) or until(e), timeout)
            got += evs
            if e is None or not is_ask(e) or e['kind'] not in replies:
                return got, e
            self.send(cmd='answer', id=e['id'], **replies[e['kind']](e))

    def alive(self):
        return self.proc.poll() is None

    def close(self):
        if self.alive():
            try:
                self.proc.stdin.close()
                self.proc.wait(timeout=5 * SLOW)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.kill()
        shutil.rmtree(self.run, ignore_errors=True)
