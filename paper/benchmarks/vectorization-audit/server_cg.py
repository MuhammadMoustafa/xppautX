# usage: uv run --locked python server_cg.py MODEL [--cg]   (MODEL path)
# One run through --server with the page's data subscription (series, plots), as web2 does: the cost of the
# data events. With --cg the server runs under callgrind (out/server_<model>.cg.<pid>).
import os, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, '../../..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from xppclient import Server, is_done
from autocompare import ensure_completed
model = os.path.abspath(sys.argv[1]); cg = '--cg' in sys.argv
binary = os.environ.get('BIN') or os.path.join(ROOT, 'xppautX')
if cg:
    out = os.path.join(HERE, 'out', 'server_%s.cg' % os.path.basename(model).split('.')[0])
    w = tempfile.NamedTemporaryFile('w', suffix='.sh', delete=False)
    w.write('#!/bin/bash\nexec valgrind --tool=callgrind --callgrind-out-file=%s.$$ %s "$@"\n' % (out, binary)); w.close()
    os.chmod(w.name, 0o755); binary = w.name
s = Server(binary, model)
try:
    evs, end = s.collect(is_done, timeout=600)
    ensure_completed(evs, end)
    s.send(cmd='events.subscribe', events=['plot.series', 'plot.shown'])
    evs, end = s.collect(is_done, timeout=600)
    ensure_completed(evs, end)
    t = time.monotonic()
    s.send(cmd='run.initial')
    evs, end = s.collect(is_done, timeout=3600)
    ensure_completed(evs, end)
    if not any(e.get('ev') == 'plot.series' for e in evs):
        raise RuntimeError('run.initial completed without plot.series')
    print('events', len(evs), 'series events', sum(1 for e in evs if e.get('ev') == 'plot.series'), 'seconds %.2f' % (time.monotonic() - t))
finally:
    s.close()
    if cg:
        os.unlink(binary)
