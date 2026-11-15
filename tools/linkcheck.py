#!/usr/bin/env python3
"""tools/linkcheck.py (W746, #825): the levels of tools/depgraph.py checked at the link.

depgraph reads include lines; a function declared in a lower module's header and defined in a
higher module's .cpp passes it, though the lower code then depends on the higher one when linked.
This reads the objects tools/deadcode.sh builds (build/deadcode): for each core object, its
undefined symbols (`nm`) against the symbols every core object defines. A reference from module A
to a symbol some other module's object defines (strongly: weak, inline and template definitions
are each object's own copy) is a link edge A -> B, and the rule is depgraph's (its MODULES, LEVELS
and INDEPENDENT, imported, never copied): an edge goes to the same module or a lower level, never
into an independent partner. Symbols no core object defines (the C++ library, third_party, the
system) are ignored. Edges against the rule are counted per module pair; ALLOWLIST below names
each pair's count and the card that removes it, exactly like depgraph's. Linux only (GNU nm).

  uv run --locked python tools/linkcheck.py [--builddir DIR]   the pairs and counts
  uv run --locked python tools/linkcheck.py --check [--verbose]  fail on a new pair, a raised or a
                                                lowered count, a stale entry; --verbose names each
                                                symbol and the file that refers to it
"""
import os
import platform
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depgraph  # noqa: E402

BUILDDIR = 'build/deadcode'   # the objects deadcode.sh builds, which sourcecheck.sh runs this on
STRONG = set('TDBRCSGI')      # nm's global, non-weak definitions (T text, D data, B bss, R rodata, C common, ...)

# (from, to) -> (link edges against the levels now, the card that removes them); exact
ALLOWLIST = {
    ('analysis', 'export'): (5, 'W737'),
    ('analysis', 'session'): (18, 'W732'),
    ('animation', 'application'): (1, 'W747'),
    ('animation', 'session files'): (1, 'W737'),
    ('autoX', 'commands'): (1, 'W737'),
    ('autoX', 'export'): (1, 'W737'),
    ('autoX', 'session'): (2, 'W732'),
    ('autoX', 'session files'): (2, 'W737'),
    ('commands', 'application'): (6, 'W737'),
    ('expressions', 'application'): (1, 'W747'),
    ('expressions', 'integrators'): (6, 'W743'),
    ('expressions', 'model and loading'): (18, 'W743'),
    ('expressions', 'session'): (2, 'W732'),
    ('integrators', 'analysis'): (31, 'W745'),
    ('integrators', 'animation'): (1, 'W737'),
    ('integrators', 'application'): (3, 'W747'),
    ('integrators', 'autoX'): (2, 'W745'),
    ('integrators', 'session'): (15, 'W732'),
    ('kernel', 'platform'): (1, 'W747'),
    ('model and loading', 'analysis'): (13, 'W744'),
    ('model and loading', 'application'): (18, 'W747'),
    ('model and loading', 'autoX'): (5, 'W744'),
    ('model and loading', 'commands'): (6, 'W737'),
    ('model and loading', 'export'): (2, 'W737'),
    ('model and loading', 'integrators'): (33, 'W744'),
    ('model and loading', 'session'): (25, 'W732'),
    ('model and loading', 'session files'): (5, 'W737'),
    ('plot', 'export'): (28, 'W737'),
    ('plot', 'session files'): (2, 'W737'),
    ('protocol front end', 'application'): (10, 'W737'),
    ('protocol front end', 'platform'): (7, 'W737'),
    ('session', 'application'): (2, 'W737'),
    ('session', 'plot'): (2, 'W747'),
    ('ui seam', 'platform'): (1, 'W737'),
}


def read_nm(text):
    """(defined strongly, defined weakly, undefined) symbol sets of one object, from `nm -g -P` text."""
    strong, weak, undef = set(), set(), set()
    for line in text.splitlines():
        f = line.split()
        if len(f) < 2:
            continue
        name, kind = f[0], f[1]
        if kind == 'U':
            undef.add(name)
        elif kind in 'WVwv':
            weak.add(name)
        elif kind in STRONG:
            strong.add(name)
    return strong, weak, undef


def link_edges(objects, modules):
    """{(from, to): [file:symbol]} against the levels, from {core source: nm text}: the same
    machinery as depgraph's include check, a link edge in place of an include line."""
    syms = {src: read_nm(text) for src, text in objects.items()}
    definer = {}
    for src, (strong, _, _) in syms.items():
        for name in strong:
            definer.setdefault(name, set()).add(src)
    module_of = {src: depgraph.place(src, modules) for src in objects}
    edges = []
    for src, (strong, weak, undef) in syms.items():
        for name in sorted(undef - strong - weak):
            for d in sorted(definer.get(name, ())):
                if module_of[d] != module_of[src]:
                    edges.append((src, d, name))
    return depgraph.level_violations({'edges': edges, 'module_of': module_of}, modules)


def object_sources(builddir):
    """{core source: its object's path} for the objects the Makefile builds in builddir, all of which must exist."""
    out = subprocess.run(['make', '-s', f'BUILDDIR={builddir}', 'print-objects'], capture_output=True, text=True, cwd=depgraph.ROOT)
    if out.returncode:
        raise SystemExit('linkcheck: cannot list the objects of ' + builddir)
    objs = out.stdout.split()
    missing = [o for o in objs if not os.path.exists(os.path.join(depgraph.ROOT, o))]
    if missing:
        raise SystemExit(f'linkcheck: {builddir} is missing objects: ' + ' '.join(missing)
                         + f'\nlinkcheck: build them first (make objects BUILDDIR={builddir})')
    srcs = {}
    for o in objs:
        src = 'core/' + os.path.splitext(os.path.basename(o))[0] + '.cpp'   # third_party's objects have none
        if os.path.exists(os.path.join(depgraph.ROOT, src)):
            srcs[src] = o
    return srcs


def read_objects(builddir):
    objects = {}
    for src, o in sorted(object_sources(builddir).items()):
        out = subprocess.run(['nm', '-g', '-P', o], capture_output=True, text=True, cwd=depgraph.ROOT)
        if out.returncode:
            raise SystemExit(f'linkcheck: nm failed on {o}: {out.stderr.strip()}')
        objects[src] = out.stdout
    return objects


def demangle(names):
    out = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True).stdout.split('\n')
    return dict(zip(names, out))


def main(argv):
    args, check, verbose, builddir = argv[1:], False, False, BUILDDIR
    while args:
        a = args.pop(0)
        if a == '--check':
            check = True
        elif a == '--verbose':
            verbose = True
        elif a == '--builddir' and args:
            builddir = args.pop(0)
        else:
            raise SystemExit('usage: tools/linkcheck.py [--check] [--verbose] [--builddir DIR]')
    if platform.system() != 'Linux':
        print('linkcheck: Linux only (GNU nm); skipped')
        return 0
    bad = link_edges(read_objects(builddir), depgraph.MODULES)
    names = demangle(sorted({e.split(':', 1)[1] for lines in bad.values() for e in lines})) if verbose or not check else {}

    def show(line):
        src, sym = line.split(':', 1)
        return f'{src}: {names.get(sym, sym)}'
    total = sum(len(v) for v in bad.values())
    if not check:
        for (a, b), lines in sorted(bad.items()):
            print(f'{a} -> {b}: {len(lines)}')
            for line in lines:
                print('  ' + show(line))
        print(f'--- link edges against the levels: {total} in {len(bad)} module pairs')
        return 0
    problems = depgraph.level_problems(bad, ALLOWLIST, 'link edge(s)')
    for p in problems:
        print('linkcheck: ' + p)
    if verbose:
        for (a, b), lines in sorted(bad.items()):
            print(f'linkcheck: {a} -> {b}:')
            for line in lines:
                print('  ' + show(line))
    if problems:
        print('linkcheck FAILED: a lower module needs a higher one at the link; invert the dependency (the inner module owns an '
              'interface the outer one implements), or lower or add the ALLOWLIST line with its card')
        return 1
    print(f'linkcheck ok: {total} link edges against the levels in {len(bad)} module pairs, all in ALLOWLIST')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
