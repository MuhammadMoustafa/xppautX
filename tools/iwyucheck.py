#!/usr/bin/env python3
"""tools/iwyucheck.py (W729, #807): include-what-you-use over core/ and tests/.

Builds a compile database from the Makefile's own commands (`make -Bn`: the
Makefile stays the one source of flags), runs IWYU through iwyu_tool on every
core/*.cpp and tests/*.cpp with tools/iwyu.imp, -j4 (tools/jobs.sh's JOBS), and
writes into build/iwyu/ (git-ignored): compile_commands.json, one advice file
per source (<name>.txt: the IWYU block of the source and of every header it
checked, as IWYU printed them) and summary.txt: per module (tools/depgraph.py's
MODULES) the includes IWYU would remove, add and turn into forward declarations,
the files with most removals, and each header's reach (how many core/*.cpp
include it, directly or not) before and after the advice, by tools/depgraph.py's
own include graph. Advice only: nothing in the source changes. The same
command line every time; a header IWYU checks in several translation units
keeps the advice of the first one in file order.

  uv run --locked python tools/iwyucheck.py [--jobs N] [--only STEM,...] [--db-only] [--reuse]
"""
import argparse
import concurrent.futures
import json
import os
import re
import shlex
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depgraph  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'build', 'iwyu')
MAPPING = os.path.join(ROOT, 'tools', 'iwyu.imp')
TOP = 15            # files listed with the most removals
WIDEST = 12         # headers listed with their reach, the widest
NAMED = ('session.h', 'model.h', 'struct.h', 'expr_program.h', 'colormap.h', 'xpp_io.h')   # and these
# IWYU's own switches: our mapping, no <bits/...> suggestions of its own, the
# long include lines kept on one line, and every header of core/ checked too
# (not only the one paired with a .cpp) so the headers' own includes are advised
IWYU_ARGS = ['-Xiwyu', '--mapping_file=' + MAPPING, '-Xiwyu', '--max_line_length=120',
             '-Xiwyu', '--check_also=core/*.h',
             '-w']

BLOCK = re.compile(r'^(\S.*?) (should add these lines|should remove these lines|has correct #includes/fwd-decls)', re.M)
REMOVE = re.compile(r'^- #include (?P<inc>[<"][^>"]+[>"])\s*// lines (?P<a>\d+)-(?P<b>\d+)', re.M)
ADD_INC = re.compile(r'^#include (?P<inc>[<"][^>"]+[>"])\s*(//\s*for (?P<why>.*))?', re.M)
# what IWYU asks for where the source never names it: a braced list, a defaulted
# projection, a generated comparison, libstdc++'s own internals. Not advice anyone
# would apply; left out of every count (the advice files keep it as printed)
IMPLICIT = {'initializer_list', 'identity', 'less', 'operator<', 'operator>', 'operator<=', 'operator>=',
            'operator==', 'operator!=', 'operator<=>'}
ADD_FWD = re.compile(r'^(namespace .*\{.*\}|(class|struct|enum class|union) \w+;)', re.M)


def database():
    """compile_commands.json from `make -Bn`: every core and tests object's compile line, as the Makefile prints it."""
    tests = sorted(os.path.splitext(f)[0] for f in os.listdir(os.path.join(ROOT, 'tests'))
                   if f.startswith('test_') and f.endswith('.cpp'))
    targets = ['objects'] + [f'build/obj/tests/{t}.o' for t in tests]
    dry = subprocess.run(['make', '-Bn', *targets], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    entries = []
    for line in dry.split('\n'):
        if ' -c ' not in line or not line.startswith(('g++ ', 'ccache g++ ')):
            continue
        args = shlex.split(line)
        if args[0] == 'ccache':
            args = args[1:]
        out, i = [], 0
        while i < len(args):
            if args[i] == '-MF' or args[i] == '-o':
                i += 2
            elif args[i] in ('-MMD', '-MP'):
                i += 1
            else:
                out.append(args[i])
                i += 1
        src = out[out.index('-c') + 1]
        if os.path.isfile(os.path.join(ROOT, src)) and src.startswith(('core/', 'tests/')) and src.endswith('.cpp'):
            entries.append({'directory': ROOT, 'arguments': out, 'file': src})
    entries.sort(key=lambda e: e['file'])
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'compile_commands.json'), 'w') as f:
        json.dump(entries, f, indent=1)
    return entries


def run_one(src, jobs_note=None):
    r = subprocess.run(['iwyu_tool', '-p', OUT, '-j1', src, '--', *IWYU_ARGS], cwd=ROOT,
                       capture_output=True, text=True)
    r.stdout = ''.join(l for l in r.stdout.splitlines(True) if 'no private include name for @headername' not in l)
    if 'USAGE:' in r.stdout + r.stderr:
        raise SystemExit(f'iwyucheck: IWYU refused its arguments for {src}:\n' + (r.stdout + r.stderr)[:400])
    return src, r.stdout + r.stderr


def implicit(why):
    """True when every name an added include is for is one nothing in the source spells (IMPLICIT, or a libstdc++ `_Name`)."""
    names = [w.strip() for w in (why or '').split(',') if w.strip()]
    return bool(names) and all(n in IMPLICIT or n.startswith('_') for n in names)


def blocks(text):
    """{file: (adds, removes)}: the advice of each file in one IWYU run's output.
    adds: [(kind 'inc'|'fwd', text)], removes: [(include text, first line, last line)]."""
    out = {}
    marks = list(BLOCK.finditer(text))
    for k, m in enumerate(marks):
        body = text[m.end():marks[k + 1].start() if k + 1 < len(marks) else len(text)]
        body = body.split('\nThe full include-list')[0]
        f = m.group(1)
        if f.startswith('(') or not f.startswith(('core/', 'tests/')):
            continue
        adds, removes = out.setdefault(f, ([], []))
        if m.group(2).startswith('should add'):
            adds += [('inc', x.group('inc')) for x in ADD_INC.finditer(body) if not implicit(x.group('why'))]
            adds += [('fwd', x.group(1)) for x in ADD_FWD.finditer(body)]
        elif m.group(2).startswith('should remove'):
            removes += [(x.group('inc'), int(x.group('a')), int(x.group('b'))) for x in REMOVE.finditer(body)]
    return out


def reach_of(files, cpps, succ):
    """header -> how many core/*.cpp include it, directly or not."""
    headers = {f for f in files if f.startswith('core/') and f.endswith('.h')}
    reach = {h: 0 for h in headers}
    for c in cpps:
        for h in depgraph.closure(c, succ) & headers:
            reach[h] += 1
    return reach


def summarize(advice, raw_names):
    g = depgraph.analyze(ROOT)
    files, edges = g['files'], g['edges']
    cpps = [f for f in files if f.startswith('core/') and f.endswith('.cpp')]
    succ = {}
    for s, d, n in edges:
        succ.setdefault(s, set()).add((d, n))
    plain = {s: {d for d, _ in v} for s, v in succ.items()}
    # the advice as edges: a removed quoted include drops the edge of that line, an added one gives a new edge
    after_rm = {s: {d for d, n in v if not any(a <= n <= b for _, a, b in advice.get(s, ([], []))[1])}
                for s, v in succ.items()}
    after_all = {s: set(v) for s, v in after_rm.items()}
    for f, (adds, _) in advice.items():
        for kind, inc in adds:
            if kind == 'inc' and inc.startswith('"'):
                r = depgraph.resolve(ROOT, f, inc[1:-1])
                if r:
                    after_all.setdefault(f, set()).add(depgraph.node_of(r))
    reach0 = reach_of(files, cpps, plain)
    reach1 = reach_of(files, cpps, after_rm)
    reach2 = reach_of(files, cpps, after_all)
    lines = []
    tot = [0, 0, 0, 0]
    per = {}
    for f, (adds, removes) in sorted(advice.items()):
        m = g['module_of'].get(f)
        row = per.setdefault(m, [0, 0, 0, 0, 0])
        row[0] += 1
        row[1] += len(removes)
        row[2] += sum(1 for k, _ in adds if k == 'inc')
        row[3] += sum(1 for k, _ in adds if k == 'fwd')
        row[4] += sum(1 for _, a, _b in removes if f.endswith('.h'))
    lines.append(f'{"module":<22}{"files":>6}{"remove":>8}{"add inc":>9}{"add fwd":>9}{"rm in .h":>10}')
    for m in sorted(per, key=lambda m: (-per[m][1], str(m))):
        r = per[m]
        lines.append(f'{m or "?":<22}{r[0]:>6}{r[1]:>8}{r[2]:>9}{r[3]:>9}{r[4]:>10}')
        for i in range(4):
            tot[i] += r[i]
    lines.append(f'{"total":<22}{tot[0]:>6}{tot[1]:>8}{tot[2]:>9}{tot[3]:>9}')
    rms = [i for _, (_, r) in advice.items() for i, _, _ in r]
    lines.append(f'removals: {sum(1 for i in rms if i.startswith(chr(34)))} of our headers, '
                 f'{sum(1 for i in rms if i.startswith("<") and i.endswith(".h>"))} of C headers (<stdio.h>), '
                 f'{sum(1 for i in rms if i.startswith("<") and not i.endswith(".h>"))} of C++ library headers')
    lines.append('')
    lines.append(f'the {TOP} files with most removals (file: remove / add include / add forward declaration)')
    rank = sorted(advice, key=lambda f: (-len(advice[f][1]), f))[:TOP]
    for f in rank:
        a, r = advice[f]
        lines.append(f'  {f}: {len(r)} / {sum(1 for k, _ in a if k == "inc")} / {sum(1 for k, _ in a if k == "fwd")}')
    lines.append('')
    lines.append(f'reach of the {WIDEST} widest headers (of {len(cpps)} core/*.cpp): now, after the removals, after removals and additions')
    wide = sorted(reach0, key=lambda h: (-reach0[h], h))
    for h in wide[:WIDEST] + [h for h in wide[WIDEST:] if h[5:] in NAMED]:
        lines.append(f'  {h}: {reach0[h]} -> {reach1[h]} -> {reach2[h]}')
    tot0, tot1, tot2 = sum(reach0.values()), sum(reach1.values()), sum(reach2.values())
    lines.append(f'  sum over all {len(reach0)} headers: {tot0} -> {tot1} -> {tot2}')
    return '\n'.join(lines) + '\n'


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--jobs', type=int, default=int(os.environ.get('JOBS', '4')))
    ap.add_argument('--only', help='comma-separated file stems, for a quick look')
    ap.add_argument('--db-only', action='store_true', help='write the compile database and stop')
    args = ap.parse_args(argv)
    entries = database()
    if args.db_only:
        print(f'iwyucheck: {len(entries)} commands in {os.path.relpath(OUT, ROOT)}/compile_commands.json')
        return 0
    srcs = [e['file'] for e in entries]
    if args.only:
        want = set(args.only.split(','))
        srcs = [s for s in srcs if depgraph.stem(s) in want]
    for f in os.listdir(OUT):
        if f.endswith('.txt'):
            os.remove(os.path.join(OUT, f))
    advice = {}
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = sorted(pool.map(run_one, srcs))
    for src, text in results:
        name = src.replace('/', '__') + '.txt'
        with open(os.path.join(OUT, name), 'w') as f:
            f.write(text)
        for f, (adds, removes) in blocks(text).items():
            advice.setdefault(f, (adds, removes))
    summary = summarize(advice, None)
    with open(os.path.join(OUT, 'summary.txt'), 'w') as f:
        f.write(summary)
    sys.stdout.write(summary)
    print(f'iwyucheck: {len(srcs)} sources, advice in {os.path.relpath(OUT, ROOT)}/')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
