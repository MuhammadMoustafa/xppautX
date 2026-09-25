"""A small client for xppautX --server, shared by the protocol checks.

    s = Server('./xppautX', 'examples/ode/lecar.ode')
    s.collect(is_idle)
    s.send(cmd='key', key='i')
    evs, ask = s.collect(is_ask)

Each Server runs in its own scratch directory (the model is copied there),
reads events on a thread and hands them out with collect().
"""
import json, os, queue, shutil, subprocess, tempfile, threading, time

# XPP_CHECK_SLOW=F multiplies every wait by F, for a server under a slow
# tool (tools/valgrindcheck.sh: memcheck runs it some 30 times slower)
SLOW = float(os.environ.get('XPP_CHECK_SLOW', '1'))


def is_idle(e):
    return e.get('ev') == 'idle'


def is_ask(e):
    return e.get('ev') == 'ask'


def is_state(e):
    return e.get('ev') == 'state'


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
        self.sent_at = {}
        threading.Thread(target=self._read, daemon=True).start()
        threading.Thread(target=lambda: [None for _ in self.proc.stderr], daemon=True).start()

    def auto_dirs(self):
        """AUTO's scratch directories of this process (xpp_make_temp_dir)"""
        tmp = tempfile.gettempdir() if os.name == 'nt' else (os.environ.get('TMPDIR') or '/tmp')
        pre = 'xppautoX-%d-' % self.proc.pid
        return [os.path.join(tmp, d) for d in os.listdir(tmp) if d.startswith(pre)]

    def _read(self):
        for line in self.proc.stdout:
            try:
                ev = json.loads(line)
            except ValueError:
                ev = {'ev': 'bad', 'line': line[:300]}
            ev['_t'] = time.monotonic()
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
