#!/usr/bin/env python3
"""tools/depgraph.py (W725, #803): the include graph of core/, as docs/deps.html.

Reads every `#include "..."` of core/*.h, core/*.cpp and tests/*.cpp (comments
and `#if 0` blocks skipped), resolves it to a file (third_party/ to one
external node per library), and computes: the file graph, a module for each
core file (MODULES below: a file the table does not place fails the script),
how many core/*.cpp reach each header through includes, include cycles among
headers (`#pragma once` hides them from the compiler) and among modules, and
module edges weighted by include lines. The page is one self-contained file,
byte-stable (everything sorted, no timestamps).

  uv run --locked python tools/depgraph.py            write docs/deps.html
  uv run --locked python tools/depgraph.py --check    fail when it is stale
"""
import fnmatch
import html
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cstrip  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGE = 'docs/deps.html'
TESTS = 'tests'

# module name -> (why these files belong together, stem patterns). A stem is a
# file name without its extension; the first module whose pattern matches wins.
MODULES = {
    'foundation': ('the owners of memory, text, files, logging, errors, math and the cancel token', [
        'xpp_io', 'xpp_files*', 'xpp_log', 'xpp_mem', 'xpp_error', 'xpp_util', 'xpp_math*', 'xpp_types',
        'xpp_cpu', 'xpp_sha256', 'xpp_zip', 'xpp_job', 'xpp_globals', 'f2c_helpers', 'format_version']),
    'ui seam': ('the XppUi table core code calls the front end through, and the lists of choices it shows', [
        'xpp_ui', 'menus', 'typed_values']),
    'expressions': ('the formula compiler, its evaluators, native code and derived quantities', [
        'expr', 'expr_*', 'xpp_mir', 'getvar', 'derived', 'form_ode', 'tabular']),
    'model and loading': ('a Model, the readers of .ode and .odex, and its options and value sets', [
        'model', 'model_*', 'load_eqn', 'odex', 'odex_*', 'ode_read', 'lunch-new', 'set_load', 'newpars',
        'flags', 'simplenet', 'options_panels']),
    'session': ('the Session, its stored data, display state and undo', [
        'session', 'storage', 'display_state', 'value_undo']),
    'session files': ('the session file, recordings and the settings they carry', [
        'recx', 'snapx', 'xpp_session', 'xpp_session_*', 'session_settings']),
    'commands': ('the command registry, its commands, keys and a model\'s buttons', [
        'command_*', 'commands_*', 'command', 'userbut', 'xpp_keymap', 'mykeydef']),
    'integrators': ('the solvers, CVODE, the right-hand side, delays and the Markov variables', [
        'integrate', 'odesol2', 'stiff', 'gear', 'cv2', 'cvode', 'cvband', 'cvdense', 'llnl*', 'solver',
        'dormpri', 'jacobian', 'volterra', 'volterra2', 'delay_handle', 'my_rhs', 'dae_fun', 'numerics',
        'numerics_settings', 'band', 'dense', 'vector', 'markov']),
    'analysis': ('what is computed from a run: sweeps, fits, equilibria, shooting, spectra, the data table', [
        'histogram', 'adj2', 'browse', 'browse_data', 'data_event', 'do_fit', 'equilibria', 'pp_shoot',
        'sweep_runs', 'del_stab']),
    'plot': ('the plot windows and what they show: curves, nullclines, marks, axes, colors', [
        'plot_data', 'graf_par', 'many_pops', 'graphics', 'nullcline', 'phase_data', 'marks_data', 'grobs',
        'axes2', 'colormap', 'line_style', 'text_style', 'arrayplot', 'struct', 'torus']),
    'export': ('pictures, movies and data files written out', [
        'picture', 'my_pdf', 'my_svg', 'scrngif', 'image_format', 'csv_export', 'data_formats', 'array_print',
        'kinescope']),
    'animation': ('the animator and its .ani files', ['ani*', 'ani_data', 'aniparse']),
    'autoX': ('xppautX\'s continuation: its numerics, runs, diagram and settings', [
        'autox_*', 'xAutox', 'setubv*', 'gogoautox', 'diagram*', 'conpar2']),
    'protocol front end': ('the JSON protocol: input, commands, events, windows and state', [
        'json_*', 'ui_json*', 'event_*', 'series_enc', 'xpp_inbox']),
    'application': ('the program\'s start, its preferences, recent models, updates and About', [
        'xppautx_main', 'xpp_batch', 'comline', 'model_switch', 'open_model', 'xpp_about', 'xpp_prefs',
        'xpp_recent', 'xpp_examples', 'xpp_update', 'xpp_install']),
    'platform': ('the system: HTTP, the window, WebView, Win32, starting a process, the page assets', [
        'xpp_http', 'xpp_https', 'xpp_window*', 'xpp_webview', 'xpp_win32', 'xpp_process', 'xpp_assets']),
}
TESTS_MODULE = 'tests'

INCLUDE = re.compile(r'\s*#\s*include\s*"([^"]+)"')
DIRECTIVE = re.compile(r'\s*#\s*(\w+)\s*(.*)')


def includes(text):
    """[(line, name)] of the quoted includes that are live: not in a comment or an `#if 0` block."""
    out, stack = [], []   # stack: for each open #if, whether it is an `#if 0`
    dead = 0
    for n, line in enumerate(cstrip.strip_comments(text).split('\n'), 1):
        m = DIRECTIVE.match(line)
        if not m:
            continue
        word, rest = m.groups()
        if word in ('if', 'ifdef', 'ifndef'):
            zero = word == 'if' and rest.strip() == '0'
            stack.append(zero)
            dead += zero
        elif word == 'else' and stack and stack[-1]:
            stack[-1] = False
            dead -= 1
        elif word == 'endif' and stack:
            dead -= stack.pop()
        elif word == 'include' and not dead:
            m = INCLUDE.match(line)
            if m:
                out.append((n, m.group(1)))
    return out


def resolve(root, src, name):
    """The file an include names: next to the includer, in core/ (-Icore), or in third_party/ (a library's own dir)."""
    third = os.path.join(root, 'third_party')
    libs = sorted(os.listdir(third)) if os.path.isdir(third) else []
    cands = [os.path.join(os.path.dirname(src), name), os.path.join('core', name), os.path.join('third_party', name)]
    for lib in libs:
        cands += [os.path.join('third_party', lib, name), os.path.join('third_party', lib, 'include', name)]
    for c in cands:
        c = os.path.normpath(c)
        if os.path.isfile(os.path.join(root, c)):
            return c
    return None


def stem(path):
    return os.path.splitext(os.path.basename(path))[0]


def place(path, modules):
    """The module of a core file, or None."""
    for mod, (_, pats) in modules.items():
        if any(fnmatch.fnmatchcase(stem(path), p) for p in pats):
            return mod
    return None


def node_of(path):
    """A file's graph node: itself, `lib:<name>` for a file of third_party/, `lib:<dir>` for any other folder (tools/)."""
    parts = path.split('/')
    if parts[0] == 'third_party':
        return 'lib:' + parts[1]
    return path if parts[0] in ('core', TESTS) else 'lib:' + parts[0]


def scc(nodes, succ):
    """Strongly connected components with more than one node (Tarjan, iterative), each sorted."""
    index, low, on, stack, out = {}, {}, set(), [], []
    for root in nodes:
        if root in index:
            continue
        work = [(root, iter(succ.get(root, ())))]
        index[root] = low[root] = len(index)
        stack.append(root)
        on.add(root)
        while work:
            v, it = work[-1]
            for w in it:
                if w not in index:
                    index[w] = low[w] = len(index)
                    stack.append(w)
                    on.add(w)
                    work.append((w, iter(succ.get(w, ()))))
                    break
                if w in on:
                    low[v] = min(low[v], index[w])
            else:
                work.pop()
                if work:
                    low[work[-1][0]] = min(low[work[-1][0]], low[v])
                if low[v] == index[v]:
                    comp = []
                    while True:
                        w = stack.pop()
                        on.discard(w)
                        comp.append(w)
                        if w == v:
                            break
                    if len(comp) > 1:
                        out.append(sorted(comp))
    return sorted(out)


def closure(start, succ):
    seen, todo = set(), [start]
    while todo:
        for w in succ.get(todo.pop(), ()):
            if w not in seen:
                seen.add(w)
                todo.append(w)
    return seen


def analyze(root, modules=None):
    """The whole analysis of the tree at root, as plain sorted data."""
    modules = MODULES if modules is None else modules
    files = []
    for d in ('core', TESTS):
        base = os.path.join(root, d)
        files += [d + '/' + f for f in sorted(os.listdir(base)) if f.endswith(('.h', '.cpp'))] if os.path.isdir(base) else []
    module_of, unplaced = {}, []
    for f in files:
        if f.startswith(TESTS + '/'):
            module_of[f] = TESTS_MODULE
        else:
            m = place(f, modules)
            if m is None:
                unplaced.append(f)
            module_of[f] = m
    if unplaced:
        raise SystemExit('depgraph: core files no module of tools/depgraph.py places: ' + ', '.join(unplaced))
    edges = []   # (src, dst node, line, resolved path)
    unresolved = []
    for f in files:
        with open(os.path.join(root, f), encoding='utf-8', errors='replace') as fh:
            text = fh.read()
        for line, name in includes(text):
            r = resolve(root, f, name)
            if r is None:
                unresolved.append(f'{f}:{line}: {name}')
            else:
                edges.append((f, node_of(r), line))
    if unresolved:
        raise SystemExit('depgraph: includes that name no file: ' + ', '.join(sorted(unresolved)))
    succ = {}
    for s, d, _ in edges:
        succ.setdefault(s, set()).add(d)
    core = [f for f in files if f.startswith('core/')]
    headers = [f for f in core if f.endswith('.h')]
    cpps = [f for f in core if f.endswith('.cpp')]
    reach = {h: 0 for h in headers}
    for c in cpps:
        for h in closure(c, succ) & set(headers):
            reach[h] += 1
    header_succ = {h: sorted(w for w in succ.get(h, ()) if w in reach) for h in headers}
    header_cycles = []
    for comp in scc(headers, header_succ):
        members = set(comp)
        header_cycles.append({'files': comp, 'lines': sorted(
            f'{s}:{n}' for s, d, n in edges if s in members and d in members)})
    # module edges: weight = include lines from one module's files into another's
    weight, lines = {}, {}
    for s, d, n in edges:
        if d.startswith('lib:'):
            continue
        a, b = module_of[s], module_of[d]
        if a != b:
            weight[(a, b)] = weight.get((a, b), 0) + 1
            lines.setdefault((a, b), []).append(f'{s}:{n}')
    names = sorted(set(module_of.values()))
    mod_succ = {}
    for (a, b) in weight:
        mod_succ.setdefault(a, set()).add(b)
    pairs = [{'a': a, 'b': b, 'ab': sorted(lines[(a, b)]), 'ba': sorted(lines[(b, a)])}
             for a in names for b in names if a < b and (a, b) in weight and (b, a) in weight]
    libs = {}
    for s, d, _ in edges:
        if d.startswith('lib:'):
            libs.setdefault(module_of[s], set()).add(d[4:])
    return {
        'files': files, 'module_of': module_of, 'edges': edges, 'reach': reach, 'cpps': len(cpps),
        'modules': names, 'weight': weight, 'header_cycles': header_cycles, 'module_pairs': pairs,
        'module_cycles': scc(names, {a: sorted(b) for a, b in mod_succ.items()}),
        'libs': {m: sorted(v) for m, v in libs.items()},
    }


# ---- the page ----------------------------------------------------------------

NODE_W, NODE_H, NODE_GAP, ARC_STEP, ARC_MAX = 210, 30, 22, 9, 130
WIDTH_PAD = 40   # the diagram's drawing width is ARC_MAX + node + ARC_MAX; the page scales it


def order_modules(names, weight):
    """A linear order of the modules with as little weight as possible running against it
    (Eades-Lin-Smyth: sources first, sinks last, else the biggest out minus in)."""
    live, front, back = set(names), [], []
    out_w = lambda n: sum(weight.get((n, m), 0) for m in live if m != n)
    in_w = lambda n: sum(weight.get((m, n), 0) for m in live if m != n)
    while live:
        pick = None
        for n in sorted(live):
            if out_w(n) == 0:
                back.append(n)
                pick = n
                break
            if in_w(n) == 0:
                front.append(n)
                pick = n
                break
        if pick is None:
            pick = max(sorted(live), key=lambda n: out_w(n) - in_w(n))
            front.append(pick)
        live.discard(pick)
    return front + back[::-1]


def page_data(r, modules=None):
    modules = MODULES if modules is None else modules
    libs = sorted({d for _, d, _ in r['edges'] if d.startswith('lib:')})
    nodes = r['files'] + libs
    index = {n: i for i, n in enumerate(nodes)}
    order = order_modules(r['modules'], r['weight'])
    pos = {m: i for i, m in enumerate(order)}
    mods = []
    for m in order:
        deps = sorted({b for (a, b) in r['weight'] if a == m})
        users = sorted({a for (a, b) in r['weight'] if b == m})
        mods.append({
            'name': m, 'why': modules[m][0] if m in modules else 'the unit tests',
            'files': [f for f in r['files'] if r['module_of'][f] == m],
            'uses': deps, 'usedby': users,
            'two_way': sorted(set(deps) & set(users)),
            'lines_out': sum(w for (a, b), w in r['weight'].items() if a == m),
            'lines_in': sum(w for (a, b), w in r['weight'].items() if b == m),
            'libs': r['libs'].get(m, []),
            'y': 20 + pos[m] * (NODE_H + NODE_GAP),
        })
    return {
        'nodes': nodes,
        'files': [[index[f], order.index(r['module_of'][f]),
                   r['reach'].get(f, -1), sorted([index[d], n] for s, d, n in r['edges'] if s == f)]
                  for f in r['files']],
        'cpps': r['cpps'],
        'modules': mods,
        'edges': [[pos[a], pos[b], w, int(pos[a] > pos[b])] for (a, b), w in sorted(r['weight'].items())],
        'hcycles': r['header_cycles'],
        'pairs': [[p['a'], p['b'], p['ab'], p['ba']] for p in r['module_pairs']],
        'mcycles': r['module_cycles'],
        'geom': {'w': NODE_W, 'h': NODE_H, 'step': ARC_STEP, 'max': ARC_MAX},
    }


def findings(r, data):
    """The top section's plain findings, as HTML."""
    esc = html.escape
    hc, mc, pairs = r['header_cycles'], r['module_cycles'], r['module_pairs']
    out = []
    if hc:
        out.append(f'<p class="warn"><b>Header cycles: {len(hc)}.</b> ' + '; '.join(
            f'{len(c["files"])} headers ({esc(", ".join(c["files"][:4]))}{", ..." if len(c["files"]) > 4 else ""})'
            for c in hc) + '.</p>')
    else:
        out.append('<p class="ok"><b>Header cycles: none.</b> No core header reaches itself through includes.</p>')
    n = len(r['modules']) - 1
    if mc:
        big = max(mc, key=len)
        out.append(f'<p class="warn"><b>Module cycles: {len(mc)}.</b> {len(big)} of {n} modules form one cycle '
                   f'(each reaches every other through includes); {len(pairs)} of {n * (n - 1) // 2} module pairs '
                   'include each other both ways.</p>')
    else:
        out.append('<p class="ok"><b>Module cycles: none.</b></p>')
    widest = sorted(r['reach'].items(), key=lambda kv: (-kv[1], kv[0]))[:10]
    out.append(f'<p><b>The 10 widest headers</b> (core/*.cpp that reach them, of {r["cpps"]}):</p><ol>' + ''.join(
        f'<li><code>{esc(h)}</code> {c} <span class="muted">({esc(r["module_of"][h])})</span></li>' for h, c in widest) + '</ol>')
    two = sorted(((len(m['two_way']), m['name']) for m in data['modules'] if m['name'] != TESTS_MODULE),
                 key=lambda t: (-t[0], t[1]))[:5]
    out.append('<p><b>Modules with the most two-way dependencies:</b> ' + ', '.join(
        f'{esc(name)} ({c} of {n - 1})' for c, name in two) + '.</p>')
    return '\n'.join(out)


def render(root):
    r = analyze(root)
    data = page_data(r)
    lines = ['{']
    keys = sorted(data)
    for i, k in enumerate(keys):
        sep = ',' if i < len(keys) - 1 else ''
        if isinstance(data[k], list):
            body = '[\n' + ',\n'.join(json.dumps(x, sort_keys=True, separators=(',', ':')) for x in data[k]) + '\n]'
        else:
            body = json.dumps(data[k], sort_keys=True, separators=(',', ':'))
        lines.append(f'{json.dumps(k)}:{body}{sep}')
    lines.append('}')
    blob = '\n'.join(lines).replace('</', '<\\/')
    return TEMPLATE.replace('@@FINDINGS@@', findings(r, data)).replace('@@DATA@@', blob)


def main(argv):
    check = argv[1:] == ['--check']
    if argv[1:] and not check:
        raise SystemExit('usage: tools/depgraph.py [--check]')
    want = render(ROOT)
    path = os.path.join(ROOT, PAGE)
    if check:
        have = open(path, encoding='utf-8', newline='').read() if os.path.exists(path) else ''
        if have != want:
            a, b = have.split('\n'), want.split('\n')
            n = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
            print(f'depgraph: {PAGE} is stale: first difference at line {n + 1}'
                  f' (now: {b[n][:100] if n < len(b) else "<end>"}); regenerate with'
                  ' `uv run --locked python tools/depgraph.py` and commit it', file=sys.stderr)
            return 1
        return 0
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(want)
    return 0


TEMPLATE = r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>core's include graph</title>
<style>
:root{color-scheme:light dark;--bg:#fafafa;--fg:#1d2127;--muted:#5d6672;--card:#fff;--line:#d5dae0;--accent:#1b61c7;--warn:#b3380f;--warn-bg:#fdeee8;--ok:#1a7f45;--ok-bg:#e8f6ee;--edge:#6b7a8c;--back:#c94a1c}
@media (prefers-color-scheme:dark){:root{--bg:#14171b;--fg:#e4e7eb;--muted:#9aa4b0;--card:#1c2026;--line:#323941;--accent:#6aa5ff;--warn:#ff9a72;--warn-bg:#3a2118;--ok:#5fcf8f;--ok-bg:#16301f;--edge:#8796a8;--back:#ff8456}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,sans-serif}
main{max-width:1180px;margin:0 auto;padding:16px}
h1{font-size:1.4rem;margin:.2rem 0}h2{font-size:1.1rem;margin:1.6rem 0 .5rem}
code,.mono{font:13px ui-monospace,monospace;overflow-wrap:anywhere}
.muted{color:var(--muted)}
section{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:4px 14px 12px;margin:14px 0}
.warn{color:var(--warn);background:var(--warn-bg);border-radius:6px;padding:6px 10px}
.ok{color:var(--ok);background:var(--ok-bg);border-radius:6px;padding:6px 10px}
.cols{display:grid;gap:14px;grid-template-columns:minmax(0,1fr)}
@media (min-width:48rem){.cols{grid-template-columns:minmax(0,5fr) minmax(0,4fr)}}
svg{width:100%;height:auto;display:block}
svg text{font:15px system-ui,sans-serif;fill:var(--fg)}
.node rect{fill:var(--card);stroke:var(--accent);stroke-width:1.5}
.node{cursor:pointer}.node.sel rect{fill:var(--accent)}.node.sel text{fill:var(--bg)}
.edge{fill:none;stroke:var(--edge);opacity:.35}.edge.back{stroke:var(--back);opacity:.6}
.edge.hi{opacity:1}.dim .edge:not(.hi){opacity:.05}
input[type=search],input[type=text]{width:100%;padding:8px;font:inherit;border:1px solid var(--line);border-radius:6px;background:var(--bg);color:var(--fg)}
table{border-collapse:collapse;width:100%}
th,td{text-align:left;padding:4px 6px;border-bottom:1px solid var(--line);vertical-align:top}
th{cursor:pointer;white-space:nowrap}th.n{text-align:right}td.n{text-align:right}
@media (max-width:47.99rem){.opt{display:none}th,td{padding:4px 3px}}
ul{margin:.3rem 0;padding-left:1.2rem}li{overflow-wrap:anywhere}
details{margin:.3rem 0}summary{cursor:pointer}
a.f{color:var(--accent);cursor:pointer;text-decoration:underline}
label{display:block;margin:.4rem 0}
</style>
</head>
<body>
<main>
<h1>core's include graph</h1>
<p class="muted">Generated by <code>tools/depgraph.py</code> from the <code>#include "..."</code> lines of core/ and tests/; regenerate it with <code>uv run --locked python tools/depgraph.py</code> whenever a change touches includes (<code>--check</code> fails when it is stale).</p>
<section id="findings">
<h2>What it shows</h2>
@@FINDINGS@@
<div id="cycles"></div>
</section>
<div class="cols">
<section><h2>Modules</h2>
<p class="muted">Each arc is the include lines from one module's files into another's, drawn right when it follows the order (users above, owners below), left in the warning color when it runs against it. Click a module to see only its arcs.</p>
<label>Hide arcs lighter than <b id="minv">3</b> include lines <input id="min" type="range" min="1" max="30" value="3"></label>
<svg id="graph" role="img" aria-label="module dependency graph"></svg></section>
<section><h2>A file</h2>
<input id="q" type="search" list="files" placeholder="a file, e.g. session.h" autocomplete="off" aria-label="file">
<datalist id="files"></datalist>
<div id="file" class="muted">Type a file name.</div></section>
</div>
<section><h2>Headers by reach</h2>
<p class="muted">Reach: how many of the core/*.cpp include the header, directly or through other headers. Click a column to sort.</p>
<table id="reach"><thead><tr><th data-k="0">Header</th><th data-k="1" class="n">Reach</th><th data-k="2" class="n opt">Includes</th><th data-k="3" class="n opt">Included by</th></tr></thead><tbody></tbody></table>
</section>
<section><h2>Per module</h2><table id="mods"><thead><tr><th>Module</th><th class="n">Uses</th><th class="n">Used by</th><th class="n opt">Two-way</th><th class="n opt">Lines out</th><th class="n opt">Lines in</th></tr></thead><tbody></tbody></table></section>
</main>
<script id="data" type="application/json">
@@DATA@@
</script>
<script>
(function(){
var D=JSON.parse(document.getElementById('data').textContent);
var N=D.nodes,M=D.modules,NS='http://www.w3.org/2000/svg';
function $(i){return document.getElementById(i)}
function el(t,a,p,x){var e=document.createElementNS(NS,t);for(var k in a)e.setAttribute(k,a[k]);if(x!==undefined)e.textContent=x;if(p)p.appendChild(e);return e}
function h(t,a,p,x){var e=document.createElement(t);for(var k in a)e.setAttribute(k,a[k]);if(x!==undefined)e.textContent=x;if(p)p.appendChild(e);return e}
var succ=N.map(function(){return[]}),pred=N.map(function(){return[]}),info={};
D.files.forEach(function(f){info[f[0]]=f;f[3].forEach(function(e){succ[f[0]].push([e[0],e[1]]);pred[e[0]].push([f[0],e[1]])})});
function closure(s,i){var seen={},todo=[i],c=0;while(todo.length){var v=todo.pop();s[v].forEach(function(e){if(!seen[e[0]]){seen[e[0]]=1;todo.push(e[0]);c++}})}return seen}
function isCore(i){return N[i].indexOf('core/')==0}
function count(set){var c=0;for(var k in set)if(isCore(+k))c++;return c}
// cycles
(function(){var c=$('cycles');
 if(D.hcycles.length){D.hcycles.forEach(function(x){var d=h('details',{'class':'warn'},c);h('summary',{},d,'Header cycle of '+x.files.length+': '+x.files.join(', '));var u=h('ul',{},d);x.lines.forEach(function(l){h('li',{},u,l)})})}
 if(D.pairs.length){var d=h('details',{'class':'warn'},c);h('summary',{},d,D.pairs.length+' module pairs include each other both ways (their include lines)');
  D.pairs.forEach(function(p){var e=h('details',{},d);h('summary',{},e,p[0]+' <-> '+p[1]+' ('+p[2].length+' + '+p[3].length+')');
   [[p[0]+' includes '+p[1],p[2]],[p[1]+' includes '+p[0],p[3]]].forEach(function(s){h('div',{'class':'muted'},e,s[0]+':');var u=h('ul',{},e);s[1].forEach(function(l){h('li',{'class':'mono'},u,l)})})})}
})();
// the module graph
var sel=-1,g=D.geom,CX=g.max+g.w/2,W=g.max*2+g.w,H=M.length*(g.h+22)+20;
var svg=$('graph');svg.setAttribute('viewBox','0 0 '+W+' '+H);
var edgeG=el('g',{},svg),nodeG=el('g',{},svg),edgeEls=[];
function drawEdges(){
 var min=+$('min').value;$('minv').textContent=min;
 while(edgeG.firstChild)edgeG.removeChild(edgeG.firstChild);edgeEls=[];
 D.edges.forEach(function(e){if(e[2]<min)return;
  var y1=M[e[0]].y+g.h/2,y2=M[e[1]].y+g.h/2,back=e[3],x=back?CX-g.w/2:CX+g.w/2,k=Math.min(g.max,g.step*Math.abs(e[0]-e[1])+8)*(back?-1:1);
  var p=el('path',{'class':'edge'+(back?' back':''),d:'M'+x+' '+y1+' C'+(x+k*1.3)+' '+y1+' '+(x+k*1.3)+' '+y2+' '+x+' '+y2,'stroke-width':(0.6+Math.sqrt(e[2])*0.55).toFixed(2)},edgeG);
  el('title',{},p,M[e[0]].name+' -> '+M[e[1]].name+': '+e[2]+' include lines'+(back?' (against the order)':''));
  p._e=e;edgeEls.push(p)});
 mark()}
function mark(){svg.classList.toggle('dim',sel>=0);edgeEls.forEach(function(p){p.classList.toggle('hi',sel>=0&&(p._e[0]==sel||p._e[1]==sel))})}
M.forEach(function(m,i){var n=el('g',{'class':'node',tabindex:0},nodeG);
 el('rect',{x:CX-g.w/2,y:m.y,width:g.w,height:g.h,rx:6},n);
 el('text',{x:CX,y:m.y+g.h/2+4.5,'text-anchor':'middle'},n,m.name+' ('+m.files.length+')');
 el('title',{},n,m.why);
 function pick(){sel=sel==i?-1:i;[].forEach.call(nodeG.childNodes,function(c,j){c.classList.toggle('sel',j==sel)});mark()}
 n.addEventListener('click',pick);n.addEventListener('keydown',function(ev){if(ev.key=='Enter'||ev.key==' '){ev.preventDefault();pick()}})});
$('min').addEventListener('input',drawEdges);drawEdges();
// the file search
var dl=$('files');D.files.forEach(function(f){h('option',{value:N[f[0]]},dl)});
function link(i,box,extra){var li=h('li',{},box);var a=h('a',{'class':'f mono'},li,N[i]);a.addEventListener('click',function(){$('q').value=N[i];show(i)});if(extra)li.appendChild(document.createTextNode(' '+extra));return li}
function list(title,items,sort,extra){var box=$('file');h('div',{},box,title+' ('+items.length+')');var u=h('ul',{},box);items.forEach(function(e){link(e[0],u,extra?extra(e):'')})}
function show(i){var box=$('file');box.className='';box.textContent='';var f=info[i];
 h('div',{},box,N[i]+' - '+M[f[1]].name+(f[2]>=0?', reach '+f[2]+' of '+D.cpps:''));
 var down=closure(succ,i),up=closure(pred,i);
 h('div',{'class':'muted'},box,'Includes '+count(down)+' core files transitively; '+count(up)+' core files include it transitively.');
 list('Includes',succ[i].slice().sort(function(a,b){return a[1]-b[1]}),0,function(e){return'(line '+e[1]+')'});
 list('Included by',pred[i].slice().sort(function(a,b){return N[a[0]]<N[b[0]]?-1:1}),0,function(e){return'(line '+e[1]+')'});
}
$('q').addEventListener('input',function(){var v=this.value.trim();if(!v)return;
 var hits=D.files.filter(function(f){return N[f[0]]==v||N[f[0]]=='core/'+v||N[f[0]]=='tests/'+v});
 if(!hits.length)hits=D.files.filter(function(f){return N[f[0]].indexOf(v)>=0});
 if(hits.length==1)show(hits[0][0]);else{var box=$('file');box.className='muted';box.textContent=hits.length?hits.length+' files match.':'No such file.'}});
// the reach table
var rows=D.files.filter(function(f){return f[2]>=0}).map(function(f){return[N[f[0]],f[2],succ[f[0]].length,pred[f[0]].length,M[f[1]].name,f[0]]});
var sk=1,sd=-1;
function drawReach(){rows.sort(function(a,b){var x=a[sk],y=b[sk];return(x<y?-1:x>y?1:a[0]<b[0]?-1:1)*(sk==0?1:sd)});
 var tb=document.querySelector('#reach tbody');tb.textContent='';
 rows.forEach(function(r){var tr=h('tr',{},tb),td=h('td',{},tr);var a=h('a',{'class':'f mono'},td,r[0]);a.addEventListener('click',function(){$('q').value=r[0];show(r[5]);$('q').scrollIntoView()});
  h('div',{'class':'muted'},td,r[4]);h('td',{'class':'n'},tr,r[1]);h('td',{'class':'n opt'},tr,r[2]);h('td',{'class':'n opt'},tr,r[3])})}
[].forEach.call(document.querySelectorAll('#reach th'),function(th){th.addEventListener('click',function(){var k=+th.getAttribute('data-k');if(k==sk)sd=-sd;else{sk=k;sd=-1}drawReach()})});
drawReach();
// the module table
var tb=document.querySelector('#mods tbody');
M.forEach(function(m){var tr=h('tr',{},tb),td=h('td',{},tr),d=h('details',{},td),s=h('summary',{},d,m.name+' ('+m.files.length+')');
 h('div',{'class':'muted'},d,m.why+(m.libs.length?'. External: '+m.libs.join(', '):''));var u=h('ul',{},d);m.files.forEach(function(f){h('li',{'class':'mono'},u,f)});
 h('td',{'class':'n'},tr,m.uses.length);h('td',{'class':'n'},tr,m.usedby.length);h('td',{'class':'n opt'},tr,m.two_way.length);h('td',{'class':'n opt'},tr,m.lines_out);h('td',{'class':'n opt'},tr,m.lines_in)});
})();
</script>
</body>
</html>
'''


if __name__ == '__main__':
    sys.exit(main(sys.argv))
