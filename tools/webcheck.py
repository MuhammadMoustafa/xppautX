#!/usr/bin/env python3
"""Check xppautX in browser mode: the page is served, events stream, commands work, and
the token protects the event and command URLs.

usage: tools/webcheck.py [--bin ./xppautX] [--ode examples/ode/lecar.ode]
"""
import argparse, hashlib, http.client, json, os, queue, re, shutil, socket, subprocess, sys, tempfile, threading

ap = argparse.ArgumentParser()
ap.add_argument('--bin', default='./xppautX')
ap.add_argument('--ode', default='examples/ode/lecar.ode')
args = ap.parse_args()

run = tempfile.mkdtemp(prefix='xppweb')
shutil.copy(args.ode, run)
proc = subprocess.Popen([os.path.abspath(args.bin), '--no-open', '--port', '0', '--verbose', os.path.basename(args.ode)],
                        cwd=run, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
# --verbose: core/xpp_log.h is quiet by default now, and this script's "what
# xppaut printed reaches the page" check below wants the startup banner/
# parser-stats chatter that used to always print, to exercise the log ->
# page pipeline (xpp_http.cpp log thread -> the "log" event -> web/xpp-client.js).
failures = 0


def check(name, ok, detail=''):
    global failures
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
check('serves the page', status == 200 and 'xpp-client.js' in body)
status, body = get('/xpp-client.js')
check('serves the client script', status == 200 and 'XppClient' in body)
status, body = get('/v2/?t=' + token)
check('serves the new front end at /v2/', status == 200 and 'app.js' in body)
status, body = get('/v2/app.js')
check('serves its script', status == 200 and 'series' in body)
c = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
c.request('GET', '/v2/inter.woff2')
r = c.getresponse()
check('serves its font', r.status == 200 and r.getheader('Content-Type') == 'font/woff2' and len(r.read()) > 10000)
c.request('GET', '/v2/inter-greek.woff2')
r = c.getresponse()
check('and the font\'s Greek subset (T8: symbol-font labels as Greek text)',
      r.status == 200 and r.getheader('Content-Type') == 'font/woff2' and len(r.read()) > 10000)
check('refuses events without the token', get('/events?t=wrong')[0] == 403)
check('refuses commands without the token', post({'cmd': 'state'}, 'wrong') == 403)

events = queue.Queue()
nodraw = queue.Queue()  # a page that draws from data (web2) asks for no drawing ops


def stream(q, extra=''):
    try:
        c = http.client.HTTPConnection('127.0.0.1', port, timeout=60)
        c.request('GET', '/events?t=' + token + extra)
        r = c.getresponse()
        for raw in r:
            line = raw.decode('utf-8').strip()
            if line.startswith('data: '):
                q.put(json.loads(line[6:]))
    except (OSError, http.client.HTTPException):
        pass  # the program exited


threading.Thread(target=stream, args=(events,), daemon=True).start()
threading.Thread(target=stream, args=(nodraw, '&draw=0'), daemon=True).start()


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
check('a new page gets hello, palette, window and state', all(k in kinds for k in ('hello', 'palette', 'window', 'state')),
      str(kinds[:8]))
check('what xppaut printed reaches the page', any(e['ev'] == 'log' for e in evs))
post({'cmd': 'key', 'key': 'i'})
_, ask = collect(lambda e: e['ev'] == 'ask')
check('a key opens a menu', ask is not None and ask['kind'] == 'menu', str(ask))
if ask:
    post({'cmd': 'answer', 'id': ask['id'], 'key': 'g'})
    evs, _ = collect(lambda e: e['ev'] == 'idle', 30)
    st = [e for e in evs if e['ev'] == 'state']
    check('integrating from the page', st and st[-1]['rows'] == 601, str(st[-1:])[:120])
    check('the page gets the drawing', any(e['ev'] == 'draw' for e in evs))
    got = []
    while True:
        try:
            got.append(nodraw.get(timeout=15))
        except queue.Empty:
            break
        if got[-1]['ev'] == 'state' and got[-1].get('rows') == 601:
            break
    check('a stream opened with draw=0 gets the events but no drawing',
          any(e['ev'] == 'hello' for e in got) and any(e['ev'] == 'state' and e.get('rows') == 601 for e in got)
          and not any(e['ev'] == 'draw' for e in got), str(sorted(set(e['ev'] for e in got))))

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

post({'cmd': 'key', 'key': 'f'})
post({'cmd': 'key', 'key': 'q'})
_, ask = collect(lambda e: e['ev'] == 'ask')
if ask:
    post({'cmd': 'answer', 'id': ask['id'], 'key': 'y'})
_, ex = collect(lambda e: e['ev'] == 'exit')
check('File/Quit ends with an exit event', ex is not None and ex['code'] == 0, str(ex))
try:
    proc.wait(timeout=10)
    check('the program exits', True)
except subprocess.TimeoutExpired:
    proc.kill()
    check('the program exits', False)
shutil.rmtree(run, ignore_errors=True)
print('web checks: %s' % ('all passed' if failures == 0 else '%d failed' % failures))
sys.exit(1 if failures else 0)
