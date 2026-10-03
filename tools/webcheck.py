#!/usr/bin/env python3
"""Check xppautX in browser mode: the page is served, events stream, commands work, and
the token protects the event and command URLs.

usage: tools/webcheck.py [--bin ./xppautX] [--ode examples/ode/lecar.odex]
"""
import argparse, atexit, hashlib, http.client, json, os, queue, re, shutil, socket, subprocess, sys, tempfile, threading, time

LEAVE_LOWER_BOUND_SECONDS = 3.5  # exceed core LEAVE_MS (2000 ms) to prove a live stream prevents exit
ALONE_LOWER_BOUND_SECONDS = 14  # exceed core ALONE_SECONDS (10 s), HEARTBEAT_MS (2 s), TICK_MS and time_t rounding

ap = argparse.ArgumentParser()
ap.add_argument('--bin', default='./xppautX')
ap.add_argument('--ode', default='examples/ode/lecar.odex')
args = ap.parse_args()

run = tempfile.mkdtemp(prefix='xppweb')
shutil.copy(args.ode, run)
# W121a: an external-options line with no assignments changes no values.
# --debug logs the line read from disk, exercising UTF-8 independently of
# the platform's command-line encoding.
log_marker = 'café 😀'
with open(os.path.join(run, 'log-text.set'), 'w', encoding='utf-8') as f:
    f.write('# ' + log_marker + '\n')
proc = subprocess.Popen([os.path.abspath(args.bin), '--browser', '--no-open', '--port', '0', '--debug', os.path.basename(args.ode), '--readset', 'log-text.set'],
                        cwd=run, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)


def stop_test_server():
    if proc.poll() is None:
        proc.kill()
    proc.wait()


atexit.register(stop_test_server)
# --debug: core/xpp_log.h is quiet by default now, and this script's "what
# xppaut printed reaches the page" check below wants the startup banner/
# parser-stats chatter that used to always print, to exercise the log ->
# page pipeline (xpp_http.cpp log thread -> the "log" event -> the page's log).
failures = 0
checks = 0


def check(name, ok, detail=''):
    global failures, checks
    checks += 1
    print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else '  ' + detail))
    failures += 0 if ok else 1


m = None
for _ in range(50):
    line = proc.stdout.readline()
    m = re.search(r'http://127\.0\.0\.1:(\d+)/\?t=(\w+)', line)
    if m:
        break
check('prints the address with a token', m is not None)
if not m:
    proc.kill()
    sys.exit(1)
port, token = int(m.group(1)), m.group(2)
threading.Thread(target=lambda: [None for _ in proc.stdout], daemon=True).start()


def get(path):
    c = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
    c.request('GET', path)
    r = c.getresponse()
    return r.status, r.read().decode('utf-8', 'replace')


def post(obj, tok=token):
    c = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
    c.request('POST', '/cmd?t=' + tok, body=json.dumps(obj))
    return c.getresponse().status


status, body = get('/?t=' + token)
check('serves the front end at /', status == 200 and 'app.js' in body)
status, body = get('/app.js')
check('serves its script', status == 200 and 'series' in body)
c = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
c.request('GET', '/inter.woff2')
r = c.getresponse()
check('serves its font', r.status == 200 and r.getheader('Content-Type') == 'font/woff2' and len(r.read()) > 10000)
c.request('GET', '/inter-greek.woff2')
r = c.getresponse()
check('and the font\'s Greek subset (T8: symbol-font labels as Greek text)',
      r.status == 200 and r.getheader('Content-Type') == 'font/woff2' and len(r.read()) > 10000)
for old in ('/v1/', '/v2/'):  # the classic page's path (removed at T18) and web2's before T17
    c = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
    c.request('GET', old + '?t=' + token)
    r = c.getresponse()
    r.read()
    check('redirects the old ' + old + ' path to /', r.status == 302 and r.getheader('Location', '').startswith('/?'))
check('serves no classic page script any more', get('/v1/xpp-client.js')[0] in (302, 404) and get('/xpp-client.js')[0] == 404)
check('refuses events without the token', get('/events?t=wrong')[0] == 403)
check('refuses commands without the token', post({'cmd': 'state'}, 'wrong') == 403)

# W161: 100 Continue acknowledges admission while the body stays open.
# Read the owner's constants so changing a limit cannot silently weaken this gate.
with open(os.path.join(os.path.dirname(__file__), '..', 'core', 'xpp_http.cpp')) as source:
    http_source = source.read()


def request_limit(name):
    return int(re.search(r'constexpr int ' + name + r' = (\d+);', http_source).group(1))


# W164: keep the head incomplete while making progress well within RECV_SECONDS.
# Only closure is a pass condition; the longer wait is a safety timeout, not a speed gate.
head_wait = (request_limit('HEAD_SECONDS') + request_limit('RECV_SECONDS')) * float(os.environ.get('XPP_CHECK_SLOW', '1'))
with socket.create_connection(('127.0.0.1', port), timeout=head_wait) as trickle:
    trickle.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\nX-Trickle: ')
    end = time.monotonic() + head_wait
    closed = False
    while time.monotonic() < end:
        trickle.settimeout(min(1, max(0.001, end - time.monotonic())))
        try:
            if trickle.recv(1) == b'':
                closed = True
                break
        except socket.timeout:
            try:
                trickle.sendall(b'x')
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                closed = True
                break
        except (ConnectionResetError, ConnectionAbortedError):
            closed = True
            break
    check('a trickling head is closed before it is complete', closed)
check('a complete head still serves the page after the head deadline check', get('/')[0] == 200)


def held_request(method, path, length):
    s = socket.create_connection(('127.0.0.1', port), timeout=20)
    s.sendall(('%s %s HTTP/1.1\r\nHost: localhost\r\nContent-Length: %d\r\n'
               'Expect: 100-continue\r\n\r\n' % (method, path, length)).encode())
    return s, s.makefile('rb')


def response_head(reader):
    line = reader.readline()
    match = re.match(rb'HTTP/1.1 (\d+)', line)
    headers = {}
    while line and line != b'\r\n':
        line = reader.readline()
        if b':' in line:
            key, value = line.split(b':', 1)
            headers[key.lower()] = value.strip()
    status = int(match.group(1)) if match else None
    body = reader.read(int(headers.get(b'content-length', b'0')))
    return status, body


def limit_check(label, constant, method, path, finish, expected):
    held = []
    try:
        admitted = []
        for i in range(request_limit(constant)):
            s, reader = held_request(method, path(i), 1)
            held.append((s, reader))
            admitted.append(response_head(reader)[0])
        check(label + ' admits its full limit with bodies held open', all(st == 100 for st in admitted), str(admitted))
        if method == 'PUT':
            s, reader = held_request(method, '/files/unauthorized.bin?t=wrong', 1)
            try:
                check('upload token check precedes saturated upload limit', response_head(reader)[0] == 403)
            finally:
                reader.close()
                s.close()
        rejected = []
        for i in range(2):
            s, reader = held_request(method, path(len(held) + i), 1)
            try:
                st, body = response_head(reader)
                rejected.append(st == 503 and b'limit reached' in body and reader.read(1) == b'')
            finally:
                reader.close()
                s.close()
        check(label + ' refuses both extra requests immediately with 503 and closes', all(rejected))
        finished = []
        for s, reader in held:
            finish(s)
            finished.append(response_head(reader)[0])
        check(label + ' admitted requests finish', all(st == expected for st in finished), str(finished))
    finally:
        for s, reader in held:
            reader.close()
            s.close()


limit_check('connection limit', 'MAX_CONNECTION_THREADS', 'POST', lambda i: '/cmd?t=' + token,
            lambda s: s.shutdown(socket.SHUT_WR), 400)
check('connection slots are released after incomplete bodies', post({'cmd': 'state'}) == 204)
limit_check('upload limit', 'MAX_UPLOADS', 'PUT', lambda i: '/files/limit-%d.bin?t=%s' % (i, token),
            lambda s: s.sendall(b'x'), 200)
s, reader = held_request('PUT', '/files/limit-reused.bin?t=' + token, 1)
try:
    admitted = response_head(reader)[0]
    if admitted == 100:
        s.sendall(b'x')
    check('upload slots are released after success', admitted == 100 and response_head(reader)[0] == 200)
finally:
    reader.close()
    s.close()
os.remove(os.path.join(run, 'limit-reused.bin'))
stored = []
for i in range(request_limit('MAX_UPLOADS')):
    with open(os.path.join(run, 'limit-%d.bin' % i), 'rb') as uploaded:
        stored.append(uploaded.read() == b'x')
    os.remove(os.path.join(run, 'limit-%d.bin' % i))
check('all held uploads store their completed bodies', all(stored))

events = queue.Queue()


def stream(q):
    try:
        c = http.client.HTTPConnection('127.0.0.1', port, timeout=60)
        c.request('GET', '/events?t=' + token)
        r = c.getresponse()
        for raw in r:
            line = raw.decode('utf-8').strip()
            if line.startswith('data: '):
                q.put(json.loads(line[6:]))
    except (OSError, http.client.HTTPException):
        pass  # the program exited


threading.Thread(target=stream, args=(events,), daemon=True).start()


def collect(until, timeout=15):
    got = []
    while True:
        try:
            e = events.get(timeout=timeout)
        except queue.Empty:
            return got, None
        got.append(e)
        if until(e):
            return got, e


evs, _ = collect(lambda e: e['ev'] == 'idle')
kinds = [e['ev'] for e in evs]
check('a new page gets hello, window and state', all(k in kinds for k in ('hello', 'window', 'state')),
      str(kinds[:8]))
check('and no drawing ops or palette (protocol 2)', not any(k in ('draw', 'palette') for k in kinds), str(kinds[:8]))
check('hello says protocol 3', any(e['ev'] == 'hello' and e.get('protocol') == 3 for e in evs))
check('what xppaut printed reaches the page', any(e['ev'] == 'log' for e in evs))
# The log pipe can deliver after idle: wait for the matching event.
log_seen = any(e['ev'] == 'log' and log_marker in e.get('text', '') for e in evs)
if not log_seen:
    _, log_event = collect(lambda e: e['ev'] == 'log' and log_marker in e.get('text', ''))
    log_seen = log_event is not None
check('UTF-8 text reaches the page log stream as written', log_seen)
post({'cmd': 'key', 'key': 'i'})
_, ask = collect(lambda e: e['ev'] == 'ask')
check('a key opens a menu', ask is not None and ask['kind'] == 'menu', str(ask))
if ask:
    post({'cmd': 'answer', 'id': ask['id'], 'key': 'g'})
    evs, _ = collect(lambda e: e['ev'] == 'idle', 30)
    st = [e for e in evs if e['ev'] == 'state']
    check('integrating from the page', st and st[-1]['rows'] == 601, str(st[-1:])[:120])
    check('the stream carries no drawing ops', not any(e['ev'] == 'draw' for e in evs))

# ---- the model's folder over HTTP (docs/protocol.md "Files", docs/ui-v2.md T5)


def req(method, path, body=None, headers=None):
    c = http.client.HTTPConnection('127.0.0.1', port, timeout=20)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    return r.status, r.read()


def raw(data, read=True):
    """bytes straight onto a socket (a request http.client would not send); the status"""
    s = socket.create_connection(('127.0.0.1', port), timeout=20)
    s.sendall(data)
    s.shutdown(socket.SHUT_WR)
    got = b''
    while read:
        chunk = s.recv(4096)
        if not chunk:
            break
        got += chunk
    s.close()
    m = re.match(rb'HTTP/1\.1 (\d+)', got)
    return int(m.group(1)) if m else None


def folder():
    return sorted(os.listdir(run))


tok = '?t=' + token
before = folder()
blob = bytes(range(256)) * 64 + os.urandom(4096) + b'\r\n\x00\x1a end'
st, body = req('PUT', '/files/bytes.bin' + tok, blob)
put = json.loads(body) if st == 200 else {}
check('PUT /files/NAME stores the body', st == 200 and put.get('size') == len(blob)
      and put.get('sha256') == hashlib.sha256(blob).hexdigest(), '%s %s' % (st, body[:200]))
st, body = req('GET', '/files/bytes.bin' + tok)
check('GET /files/NAME gives the same bytes back (binary included)', st == 200 and body == blob, '%s %d' % (st, len(body)))
st, body = req('GET', '/files' + tok)
listing = json.loads(body).get('files', []) if st == 200 else []
entry = [f for f in listing if f['name'] == 'bytes.bin']
check('GET /files lists name, size, mtime and SHA-256',
      entry and entry[0]['size'] == len(blob) and entry[0]['sha256'] == hashlib.sha256(blob).hexdigest()
      and entry[0]['mtime'] > 0 and any(f['name'] == os.path.basename(args.ode) for f in listing), str(listing)[:300])
st, _ = req('PUT', '/files/bytes.bin' + tok, b'replaced')
st2, body = req('GET', '/files/bytes.bin' + tok)
check('PUT replaces a file', st == 200 and body == b'replaced', '%s %s' % (st, body[:40]))
check('the listing leaves out what it cannot serve', not any(f['name'].startswith('.') for f in listing))

st_list = [req('GET', '/files')[0], req('GET', '/files?t=wrong')[0], req('GET', '/files/bytes.bin?t=' + token[:-1])[0],
           req('PUT', '/files/notoken.bin', b'x')[0], req('PUT', '/files/notoken.bin?t=' + token + 'x', b'x')[0],
           req('PUT', '/files/notoken.bin?x=1&t=', b'x')[0]]
check('the file endpoints refuse a missing or wrong token (403)', st_list == [403] * 6, str(st_list))
check('a refused token writes nothing', 'notoken.bin' not in folder(), str(folder()))

# where ../x would land: the temporary folder is shared, so only that name is watched
outside = os.path.join(os.path.dirname(run), 'x')
stat_of = lambda p: (os.stat(p).st_mtime_ns, os.stat(p).st_size) if os.path.exists(p) else None
above = stat_of(outside)
bad_names = ['../x', '..%2Fx', '..%2fx', '%2E%2E%2Fx', 'a/b', 'a\\b', 'a%5Cb', '..', '.', '.hidden', '%2Ehidden',
             'C%3Ax', 'a%00b', 'a%0Ab', 'a%', 'a%2', 'x.set.', 'CON', 'nul.txt', 'a|b', 'n' * 256]
bad = {}
for n in bad_names:
    for method in ('PUT', 'GET'):
        st, _ = req(method, '/files/' + n + tok, b'evil' if method == 'PUT' else None)
        if st != 400:
            bad['%s %s' % (method, n)] = st
check('traversal, separators, dot names, device names and long names are refused (400)', not bad, str(bad))
check('... and leave no file, here or in the folder above', folder() == sorted(before + ['bytes.bin'])
      and stat_of(outside) == above, str(folder()))

st, _ = req('PUT', '/files/big.bin' + tok, headers={'Content-Length': str(64 * 1024 * 1024 + 1)})
check('an upload over 64 MB is refused before its body (413)', st == 413, str(st))
st = raw(('PUT /files/cut.bin%s HTTP/1.1\r\nHost: x\r\nContent-Length: 100000\r\n\r\n' % tok).encode() + b'y' * 1000)
check('an upload cut short is refused (400)', st == 400, str(st))
st = raw(('PUT /files/nolen.bin%s HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n'
          % tok).encode())
check('an upload without a Content-Length is refused (411)', st == 411, str(st))
st = raw(('PUT /files/badlen.bin%s HTTP/1.1\r\nHost: x\r\nContent-Length: -1\r\n\r\n' % tok).encode())
check('a bad Content-Length is refused (400)', st == 400, str(st))
check('refused and cut uploads leave nothing, no temporary file either', folder() == sorted(before + ['bytes.bin']),
      str(folder()))

os.mkdir(os.path.join(run, 'sub'))
st, _ = req('GET', '/files/sub' + tok)
st2, _ = req('PUT', '/files/sub' + tok, b'x')
check('a folder is not a file (403)', st == 403 and st2 == 403 and os.path.isdir(os.path.join(run, 'sub')), '%s %s' % (st, st2))
os.rmdir(os.path.join(run, 'sub'))
secret = tempfile.mkdtemp(prefix='xppsecret')
with open(os.path.join(secret, 'secret.txt'), 'wb') as f:
    f.write(b'secret')
try:
    os.symlink(os.path.join(secret, 'secret.txt'), os.path.join(run, 'link.txt'))
    linked = True
except (OSError, NotImplementedError):
    linked = False  # Windows without the symlink privilege
if linked:
    st, body = req('GET', '/files/link.txt' + tok)
    st2, _ = req('PUT', '/files/link.txt' + tok, b'overwritten')
    with open(os.path.join(secret, 'secret.txt'), 'rb') as f:
        kept = f.read()
    st3, body3 = req('GET', '/files' + tok)
    check('a symbolic link is neither read nor written through (403)', st == 403 and st2 == 403 and kept == b'secret'
          and os.path.islink(os.path.join(run, 'link.txt')) and b'link.txt' not in body3, '%s %s %r' % (st, st2, kept))
    os.remove(os.path.join(run, 'link.txt'))
shutil.rmtree(secret, ignore_errors=True)

# ---- a connection that sends nothing, or a stalled upload, holds up no command:
# one thread used to answer every request, and a browser's preconnect held it
# (and an Abort behind it) for 30 s while an AUTO run went on (T25)
idle = socket.create_connection(('127.0.0.1', port), timeout=20)
stall = socket.create_connection(('127.0.0.1', port), timeout=20)
stall.sendall(('PUT /files/stall.txt%s HTTP/1.1\r\nHost: x\r\nContent-Length: 1000\r\n\r\nhalf' % tok).encode())
# The menu reply below acknowledges acceptance behind both connected sockets.
t = time.monotonic()
try:
    post({'cmd': 'key', 'key': 'i'})
    _, ask = collect(lambda e: e['ev'] == 'ask', 10)
except OSError:  # the POST timed out
    ask = None
took = time.monotonic() - t
check('an idle connection (a preconnect) and a stalled upload hold up no command: its menu within 1 s',
      ask is not None and took < 1, '%.2f s' % took)
if ask:
    post({'cmd': 'answer', 'id': ask['id'], 'key': 'Escape'})
    collect(lambda e: e['ev'] == 'idle')
idle.close()
stall.close()

# ---- W124: a page numbers its commands (p=, n=) and sends one again when its
# POST failed with no answer; the server takes each number once
def post_numbered(obj, page, n):
    c = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
    c.request('POST', '/cmd?t=%s&p=%s&n=%s' % (token, page, n), body=json.dumps(obj))
    return c.getresponse().status


collect(lambda e: False, 0.5)  # what is still coming from the commands above
sts = [post_numbered({'cmd': 'redraw'}, 'w124page', n) for n in (1, 1, 2, 1)]
sts.append(post_numbered({'cmd': 'redraw'}, 'w124other', 1))  # another page counts on its own
evs, _ = collect(lambda e: False, 3)
idles = sum(1 for e in evs if e['ev'] == 'idle')
check('W124: a numbered command sent again is answered (204) but taken once; another page counts on its own',
      sts == [204] * 5 and idles == 3, '%s, %d idle' % (sts, idles))
bad = [post_numbered({'cmd': 'redraw'}, p, n) for p, n in (('w124page', 'x'), ('w124page', '0'), ('bad%20id', 3), ('', 3))]
c = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
c.request('POST', '/cmd?t=%s&p=w124page' % token, body=json.dumps({'cmd': 'redraw'}))
bad.append(c.getresponse().status)
evs, _ = collect(lambda e: False, 1)
check('W124: a bad or half command number is refused (400) and nothing runs',
      bad == [400] * 5 and not any(e['ev'] == 'idle' for e in evs), '%s %s' % (bad, [e['ev'] for e in evs][:6]))

post({'cmd': 'key', 'key': 'f'})
post({'cmd': 'key', 'key': 'q'})
_, ask = collect(lambda e: e['ev'] == 'ask')
if ask:
    post({'cmd': 'answer', 'id': ask['id'], 'key': 'd'})
_, ex = collect(lambda e: e['ev'] == 'exit')
check('File/Quit ends with an exit event', ex is not None and ex['code'] == 0, str(ex))
try:
    proc.wait(timeout=10)
    check('the program exits', True)
except subprocess.TimeoutExpired:
    proc.kill()
    check('the program exits', False)
shutil.rmtree(run, ignore_errors=True)

# ---- W108: a page that says it is leaving ends the program soon; a reload inside the wait keeps it.
# Pass/fail never depends on speed: the exit must come after the leave and before a generous safety
# timeout; the reload case passes when the same process still answers after the wait has passed.


lrun = tempfile.mkdtemp(prefix='xppleave')
shutil.copy(args.ode, lrun)


def leave_session(ode=None):
    """a fresh --browser process with one event stream open; (proc, port, token, stream's connection)"""
    ode = ode or os.path.basename(args.ode)
    p = subprocess.Popen([os.path.abspath(args.bin), '--browser', '--no-open', '--port', '0', ode],
                         cwd=lrun, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    mm = None
    for _ in range(50):
        mm = re.search(r'http://127\.0\.0\.1:(\d+)/\?t=(\w+)', p.stdout.readline())
        if mm:
            break
    if not mm:
        p.kill()
        return None
    threading.Thread(target=lambda: [None for _ in p.stdout], daemon=True).start()
    pt, tk = int(mm.group(1)), mm.group(2)
    c = http.client.HTTPConnection('127.0.0.1', pt, timeout=30)
    c.request('GET', '/events?t=' + tk)
    c.sk = c.sock  # keep the stream's socket before getresponse detaches it
    c.stream = c.getresponse()  # kept: a response nobody holds is garbage collected and closes its socket
    c.stream.readline()  # the first event: the page is connected
    return p, pt, tk, c


def close_stream(c):
    """the page's stream goes away: its response holds the socket, so both close"""
    c.stream.close()
    c.close()


def leave_post(pt, tk):
    c = http.client.HTTPConnection('127.0.0.1', pt, timeout=10)
    c.request('POST', '/leave?t=' + tk, body='')
    r = c.getresponse()
    r.read()
    return r.status


def answers(pt, tk):
    try:
        c = http.client.HTTPConnection('127.0.0.1', pt, timeout=10)
        c.request('POST', '/cmd?t=' + tk, body=json.dumps({'cmd': 'state'}))
        return c.getresponse().status == 204
    except OSError:
        return False


sess = leave_session()
check('W108: a second process starts for the leave check', sess is not None)
if sess:
    p, pt, tk, c = sess
    check('W108: /leave needs the token', leave_post(pt, 'wrong') == 403)
    check('W108: a wrong-token leave leaves the program answering commands', answers(pt, tk))
    close_stream(c)  # the page's pagehide: the stream closes, then the beacon
    t0 = time.time()
    check('W108: the leave beacon is accepted', leave_post(pt, tk) == 204)
    try:
        p.wait(timeout=8)  # generous safety timeout; the wait itself is about 2 s
        print('perf: leave-exit %.2f s' % (time.time() - t0))
        check('W108: the program ends after the page says it is leaving', p.returncode == 0, str(p.returncode))
    except subprocess.TimeoutExpired:
        p.kill()
        check('W108: the program ends after the page says it is leaving', False, 'still running after 8 s')

sess = leave_session()
if sess:
    p, pt, tk, c = sess
    # The reloaded page's stream is up before the beacon: a stream open when the wait ends keeps the
    # session however long it took to connect, so the outcome does not depend on speed (W115). Root
    # cause of the old flake: the check dropped its response object, Python closed the socket at once,
    # and the server's two back-to-back heartbeats could both still succeed on the dead stream.
    c2 = http.client.HTTPConnection('127.0.0.1', pt, timeout=30)
    c2.request('GET', '/events?t=' + tk)
    c2.stream = c2.getresponse()
    c2.stream.readline()
    close_stream(c)  # the old page's stream closes, then its beacon
    check('W108: a reload leave beacon is accepted', leave_post(pt, tk) == 204)
    time.sleep(LEAVE_LOWER_BOUND_SECONDS)  # lower bound: core LEAVE_MS (2000 ms), reload keeps the stream open
    check('W108: the same process still answers after a reload inside the wait', p.poll() is None and answers(pt, tk))
    c2.close()
    p.kill()
    p.wait()
# ---- W112: a plain quit is a normal end (bye, then exit code 0), even during a run; a process that
# only serves a stopped model's log (no bye) keeps serving while a page is open and ends once none is left.
# Generous safety timeouts only; the times print as perf: lines.

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'heavy.odex')) as f:
    heavy = f.read().replace('total=20', 'total=1e7')
with open(os.path.join(lrun, 'longrun.odex'), 'w') as f:
    f.write(heavy)
shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'malformed_unbalanced.ode'), lrun)


def read_events(c, until, timeout=30):
    """events from an open stream's response until until(e) is true; (events, matching or None)"""
    evs = []
    end = time.time() + timeout
    c.sk.settimeout(timeout)
    try:
        while time.time() < end:
            line = c.resp.readline()
            if not line:
                break
            if line.startswith(b'data: '):
                e = json.loads(line[6:])
                evs.append(e)
                if until(e):
                    return evs, e
    except OSError:
        pass
    return evs, None


def post_cmd(pt, tk, obj):
    c = http.client.HTTPConnection('127.0.0.1', pt, timeout=10)
    c.request('POST', '/cmd?t=' + tk, body=json.dumps(obj))
    c.getresponse().read()


# W136: startup runnow uses the same job and HTTP events as an explicit Go.
with open(os.path.join(lrun, 'runnow.odex'), 'w') as f:
    f.write(heavy + chr(10) + '@ runnow=1' + chr(10))
sess = leave_session('runnow.odex')
check('W136: a browser runnow process starts', sess is not None)
if sess:
    p, pt, tk, c = sess
    try:
        c.resp = c.stream
        _, comp = read_events(c, lambda e: e['ev'] == 'computing')
        check('W136: browser startup runnow shows computing', comp is not None)
        post_cmd(pt, tk, {'cmd': 'abort'})
        evs, idle = read_events(c, lambda e: e['ev'] == 'idle')
        stopped = next((e for e in evs if e['ev'] == 'stopped'), None)
        check('W136: browser Abort stops startup runnow',
              idle is not None and stopped is not None and stopped['at']['what'] == 'integrate', str(stopped))
    finally:
        c.close()
        p.kill()
        p.wait()

sess = leave_session('longrun.odex')
check('W112: a process starts for the quit-during-a-run check', sess is not None)
if sess:
    p, pt, tk, c = sess
    close_stream(c)
    c = http.client.HTTPConnection('127.0.0.1', pt, timeout=30)
    c.request('GET', '/events?t=' + tk)
    c.sk = c.sock
    c.resp = c.getresponse()
    read_events(c, lambda e: e['ev'] == 'idle')
    post_cmd(pt, tk, {'cmd': 'key', 'key': 'i'})
    _, ask = read_events(c, lambda e: e['ev'] == 'ask', 30)
    check('W112: i asks for the initial conditions', ask is not None)
    post_cmd(pt, tk, {'cmd': 'answer', 'id': ask['id'] if ask else 0, 'key': 'g'})
    _, comp = read_events(c, lambda e: e['ev'] == 'computing', 30)
    check('W112: the run starts (computing)', comp is not None)
    t0 = time.time()
    post_cmd(pt, tk, {'cmd': 'quit'})
    evs, ex = read_events(c, lambda e: e['ev'] == 'exit', 30)
    names = [e['ev'] for e in evs]
    # the run was still going when the quit came: the quit stopped it
    stopped = next((e for e in evs if e['ev'] == 'stopped'), None)
    check('W112: the quit stops the run (stopped, integrate)',
          stopped is not None and stopped['at']['what'] == 'integrate', str(names))
    check('W112: a plain quit during a run says bye, then exit code 0',
          ex is not None and ex['code'] == 0 and 'bye' in names, str(names[-4:]) + str(ex))
    try:
        p.wait(timeout=15)
        print('perf: quit-exit %.2f s' % (time.time() - t0))
        check('W112: ... and the process ends with code 0', p.returncode == 0, str(p.returncode))
    except subprocess.TimeoutExpired:
        p.kill()
        check('W112: ... and the process ends with code 0', False, 'still running after 15 s')
    c.close()

# ---- W134: one route into the model's folder. An upload (PUT /files/NAME) during a run is refused
# like the protocol's `file` put (a data command): nothing lands under a computation, whichever way
# it came; once the run has ended the same upload goes through.
with open(os.path.join(lrun, 'guard.par'), 'wb') as f:
    f.write(b'old')


def put_at(pt, tk, name, body):
    c = http.client.HTTPConnection('127.0.0.1', pt, timeout=20)
    c.request('PUT', '/files/' + name + '?t=' + tk, body=body)
    r = c.getresponse()
    return r.status, r.read().decode('utf-8', 'replace')


def guard():
    with open(os.path.join(lrun, 'guard.par'), 'rb') as f:
        return f.read()


sess = leave_session('longrun.odex')
check('W134: a process starts for the upload-during-a-run check', sess is not None)
if sess:
    p, pt, tk, c = sess
    close_stream(c)
    c = http.client.HTTPConnection('127.0.0.1', pt, timeout=30)
    c.request('GET', '/events?t=' + tk)
    c.sk = c.sock
    c.resp = c.getresponse()
    read_events(c, lambda e: e['ev'] == 'idle')
    post_cmd(pt, tk, {'cmd': 'key', 'key': 'i'})
    _, ask = read_events(c, lambda e: e['ev'] == 'ask', 30)
    post_cmd(pt, tk, {'cmd': 'answer', 'id': ask['id'] if ask else 0, 'key': 'g'})
    _, comp = read_events(c, lambda e: e['ev'] == 'computing', 30)
    st, body = put_at(pt, tk, 'guard.par', b'new')
    check('W134: PUT /files/NAME during a run is refused (409, "Not while a computation runs")',
          comp is not None and st == 409 and 'Not while a computation runs' in body, '%s %s %s' % (comp, st, body))
    post_cmd(pt, tk, {'cmd': 'file', 'op': 'put', 'name': 'guard.par', 'data': 'bmV3'})  # "new"
    post_cmd(pt, tk, {'cmd': 'abort'})
    evs, _ = read_events(c, lambda e: e['ev'] == 'idle', 30)  # the run's idle, stopped
    evs, _ = read_events(c, lambda e: e['ev'] == 'idle', 30)  # the refused file put's
    refused = [e.get('error', '') for e in evs if e['ev'] == 'message']
    check('W134: ... like the file command sent during it (an error after the run, no file event)',
          len(refused) == 1 and 'Not while a computation runs' in refused[0]
          and not any(e['ev'] == 'file' for e in evs), str([e['ev'] for e in evs]) + str(refused))
    check('W134: ... and neither wrote the file', guard() == b'old', str(guard()))
    st, body = put_at(pt, tk, 'guard.par', b'new')
    check('W134: after the run the same upload lands', st == 200 and guard() == b'new', '%s %s' % (st, body))
    c.resp.close()
    c.close()
    p.kill()
    p.wait()

sess = leave_session('malformed_unbalanced.ode')
check('W112: a process with a model that does not load starts', sess is not None)
if sess:
    p, pt, tk, c = sess
    close_stream(c)
    c = http.client.HTTPConnection('127.0.0.1', pt, timeout=30)
    c.request('GET', '/events?t=' + tk)
    c.sk = c.sock
    c.resp = c.getresponse()
    evs, ex = read_events(c, lambda e: e['ev'] == 'exit', 30)
    check('W112: it reports exit code 1, with no bye', ex is not None and ex['code'] == 1
          and not any(e['ev'] == 'bye' for e in evs), str([e['ev'] for e in evs]))
    time.sleep(ALONE_LOWER_BOUND_SECONDS)  # lower bound: core ALONE_SECONDS (10 s), stopped page keeps its stream open
    check('W112: it keeps serving while the page is open', p.poll() is None and answers(pt, tk))
    c.resp.close()  # the response holds the socket open otherwise
    c.close()
    t0 = time.time()
    check('W112: the leave beacon is accepted', leave_post(pt, tk) == 204)
    try:
        p.wait(timeout=30)
        print('perf: stopped-leave-exit %.2f s' % (time.time() - t0))
        check('W112: it ends once the page has left', True)
    except subprocess.TimeoutExpired:
        p.kill()
        check('W112: it ends once the page has left', False, 'still running after 30 s')
shutil.rmtree(lrun, ignore_errors=True)

# ---- T27: the Windows exe with no standard error handle ------------------------------
# xppautX.exe is a GUI-subsystem program: started from Explorer, a shortcut,
# Start-Process or a terminal it has no standard error, and its C library
# gives stderr no descriptor at all; what the core prints (AUTO's table in
# the AUTO window's Output) must still reach the page. Only stdout is a pipe
# here, to read the address.
if os.name == 'nt':
    import _winapi, msvcrt
    run = tempfile.mkdtemp(prefix='xppweb')
    shutil.copy(args.ode, run)
# W121a: an external-options line with no assignments changes no values.
# --debug logs the line read from disk, exercising UTF-8 independently of
# the platform's command-line encoding.
log_marker = 'café 😀'
with open(os.path.join(run, 'log-text.set'), 'w', encoding='utf-8') as f:
    f.write('# ' + log_marker + '\n')
    rfd, wfd = os.pipe()
    whandle = msvcrt.get_osfhandle(wfd)
    os.set_handle_inheritable(whandle, True)
    si = subprocess.STARTUPINFO(dwFlags=subprocess.STARTF_USESTDHANDLES, hStdOutput=whandle)  # stdin, stderr: none
    exe = os.path.abspath(args.bin)
    hproc, hthread, _, _ = _winapi.CreateProcess(
        exe, subprocess.list2cmdline([exe, '--browser', '--no-open', '--port', '0', '--debug', os.path.basename(args.ode), '--readset', 'log-text.set']),
        None, None, True, 0, None, run, si)
    _winapi.CloseHandle(hthread)
    os.close(wfd)
    out = os.fdopen(rfd, 'r')
    m = re.search(r'http://127\.0\.0\.1:(\d+)/\?t=(\w+)', out.readline())
    check('T27: with no standard error, it prints the address', m is not None)
    if m:
        port, token = int(m.group(1)), m.group(2)
        threading.Thread(target=lambda: [None for _ in out], daemon=True).start()
        events = queue.Queue()
        threading.Thread(target=stream, args=(events,), daemon=True).start()
        evs, _ = collect(lambda e: e['ev'] == 'idle')
        check('T27: ... and what xppaut printed still reaches the page', any(e['ev'] == 'log' for e in evs),
              str([e['ev'] for e in evs][:8]))
        post({'cmd': 'key', 'key': 'f'}, token)
        post({'cmd': 'key', 'key': 'q'}, token)
        _, ask = collect(lambda e: e['ev'] == 'ask')
        if ask:
            post({'cmd': 'answer', 'id': ask['id'], 'key': 'd'}, token)
    if _winapi.WaitForSingleObject(hproc, 10000) != 0:
        _winapi.TerminateProcess(hproc, 1)
        check('T27: ... and it exits', False)
    _winapi.CloseHandle(hproc)
    shutil.rmtree(run, ignore_errors=True)
print('web checks: %d passed, %d failed' % (checks - failures, failures))
sys.exit(1 if failures else 0)
