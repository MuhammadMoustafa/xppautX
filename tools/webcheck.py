#!/usr/bin/env python3
"""Check xppautX in browser mode: the page is served, events stream, commands work, and
the token protects the event and command URLs.

usage: tools/webcheck.py [--bin ./xppautX] [--ode examples/ode/lecar.ode]
"""
import argparse, http.client, json, os, queue, re, shutil, subprocess, sys, tempfile, threading

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
# page pipeline (xpp_http.c log thread -> the "log" event -> web/xpp-client.js).
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
check('refuses events without the token', get('/events?t=wrong')[0] == 403)
check('refuses commands without the token', post({'cmd': 'state'}, 'wrong') == 403)

events = queue.Queue()


def stream():
    try:
        c = http.client.HTTPConnection('127.0.0.1', port, timeout=60)
        c.request('GET', '/events?t=' + token)
        r = c.getresponse()
        for raw in r:
            line = raw.decode('utf-8').strip()
            if line.startswith('data: '):
                events.put(json.loads(line[6:]))
    except (OSError, http.client.HTTPException):
        pass  # the program exited


threading.Thread(target=stream, daemon=True).start()


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
