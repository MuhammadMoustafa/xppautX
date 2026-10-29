# usage: uv run --locked python autocg.py DESCRIPTION [--cg] [--threads N]
# Runs one tests/auto description through tools/autocompare.py's compute (the same server session as the
# AUTO baseline check); with --cg the server runs under callgrind (out/auto_DESCRIPTION.cg).
import os, stat, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, '../../..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import autocompare as ac
name = sys.argv[1]; cg = '--cg' in sys.argv
threads = int(sys.argv[sys.argv.index('--threads') + 1]) if '--threads' in sys.argv else 1
binary = os.environ.get('BIN') or os.path.join(ROOT, 'xppautX')
if cg:
    out = os.environ.get('CGOUT') or os.path.join(HERE, 'out', 'auto_%s.cg' % name)
    w = tempfile.NamedTemporaryFile('w', suffix='.sh', delete=False)
    w.write('#!/bin/bash\nexec valgrind --tool=callgrind %s --callgrind-out-file=%s.$$ %s "$@"\n' % (os.environ.get("CGOPT", ""), out, binary)); w.close()
    os.chmod(w.name, 0o755); binary = w.name
d = ac.read_description(ac.BASE / (name + '.json'))
if os.environ.get('NMAX'):  # e.g. mu26 control: NMAX=15 shortens the 'cycles' run
    for r in d['runs']:
        if r['name'] == 'cycles': r['auto']['nmax'] = int(os.environ['NMAX'])
t = time.monotonic(); ac.compute(binary, d, threads)
print(name, 'threads', threads, 'seconds %.2f' % (time.monotonic() - t))
if cg: os.unlink(binary)
