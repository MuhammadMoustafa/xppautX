#!/usr/bin/env python3
"""Counts the errors the core reports with no place (W140; CLAUDE.md "Every
error names its file and line"): every error is an xpp::Error
(core/xpp_error.h), which carries its place (file, line, column, the line
as written), and is shown through show_error (xpp_ui.h) or, for a model
that does not load, load_model; Error::text() renders it. What this counts,
in core/*.cpp and core/*.h with comments stripped and string literals
emptied, is each place an error is made or reported with no place:

  err_msg      a call of err_msg(text) or j_err_msg(text): text alone
  fail         xpp::fail("where", what) with no Place (a third argument)
  Error{}      an Error built inline from "where" (or {}) and what alone
  log ERROR    an xpp::log/log_printf at XPP_LOG_ERROR not of an
               Error's text() (an error written to the log by hand)

A Place given as an explicit empty one ({} or Place{}) is no place and is
counted too. err_reading(path, ...) and fail_reading(..., file) name their
file and are not counted; an Error built elsewhere (a Result's error()) is
counted where it was made. The owners are not counted: xpp_error.h (fail
itself), xpp_ui.cpp (the dispatchers) and json_prompts.cpp (j_err_msg
itself).

Where an error's place comes from: a file read, the file and its line; a
model line at run time, model_place (model_files.h) or the place the
parser kept; a command, command_error(command, what) or command_place()
(xpp_ui.h: in a --script or a recording's Play, the step's file and line);
a model being loaded, model_failed(what) (xpp_batch.h: the load's place).

ALLOWED below lists the sites that are not errors with a place to give,
each with its reason. W140b took the rest to 0, so tests/errors.baseline
holds none: any new site fails --check.

Modes:
  (default)  print the per-file counts and the total
  --list     print every site, file:line: kind
  --check    compare against tests/errors.baseline (sourcecheck.sh)
  --update   rewrite tests/errors.baseline from the current tree
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(ROOT, 'tests', 'errors.baseline')
OWNERS = {'core/xpp_error.h', 'core/xpp_ui.cpp', 'core/json_prompts.cpp'}


def strip(text):
    """comments removed, string and character literals emptied, newlines kept"""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            while i < n and text[i] != '\n':
                i += 1
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append('\n' * text.count('\n', i, j))
            i = j
        elif c in '"\'':
            if c == '"' and i > 0 and text[i - 1] == 'R':
                # a raw string R"d(...)d"
                m = re.match(r'"([^(]*)\(', text[i:])
                end = text.find(')' + m.group(1) + '"', i) if m else -1
                if end >= 0:
                    out.append('""' + '\n' * text.count('\n', i, end))
                    i = end + len(m.group(1)) + 2
                    continue
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def args_at(text, i):
    """the top-level arguments of the call or braces opened just before i"""
    depth, args, start = 0, [], i
    pairs = {'(': ')', '[': ']', '{': '}'}
    while i < len(text):
        c = text[i]
        if c in pairs:
            depth += 1
        elif c in ')]}':
            if depth == 0:
                args.append(text[start:i].strip())
                return [a for a in args if a]
            depth -= 1
        elif c == ',' and depth == 0:
            args.append(text[start:i].strip())
            start = i + 1
        i += 1
    return []


ARGUMENT_CHECK = "CVODE's check of an argument cv2.cpp passes: a bug of ours if it fires, no file or model line"
# (file, a regex the site's lines as written match, why it has no place)
ALLOWED = [
    # results shown through err_msg that are not errors: W133 (#185) shows
    # an outcome once, as a result
    ('core/histogram.cpp', r'Mean=', 'W133: a result (the mean and deviation), not an error'),
    ('core/do_fit.cpp', r'Success!', "W133: the fit's outcome, a result"),
    ('core/adj2.cpp', r'Maximal exponent', 'W133: the Liapunov exponent, a result'),
    ('core/pp_shoot.cpp', r'Saving anyway', 'W133: a notice that the BVP saved its result'),
    ('core/auto_nox.cpp', r'Redraw is O|Draw orbits', "W133: a toggle's new state, not an error"),
    # the vendored CVODE's checks of what cv2.cpp passes it
    ('core/cvode.cpp', r'MSG_(Y0_NULL|BAD_N|BAD_LMM|BAD_ITER|BAD_ITOL|F_NULL|RELTOL_NULL|BAD_RELTOL|ABSTOL_NULL'
     r'|BAD_ABSTOL|BAD_OPTIN|BAD_OPT|BAD_HMIN_HMAX|MEM_FAIL|BAD_EWT|CVODE_NO_MEM|YOUT_NULL|T_NULL|BAD_ITASK'
     r'|LINIT_NULL|LINIT_FAIL|LSETUP_NULL|LSOLVE_NULL|LFREE_NULL|DKY_NO_MEM|BAD_DKY|BAD_K|BAD_T)\b', ARGUMENT_CHECK),
    ('core/cvband.cpp', r'MSG_(MEM_FAIL|BAD_SIZES)', ARGUMENT_CHECK),
    ('core/cvdense.cpp', r'MSG_MEM_FAIL', ARGUMENT_CHECK),
    # the process itself, about no file or command
    ('core/xpp_mem.cpp', r'out of memory', 'out of memory: the process exits'),
    ('core/xpp_io.cpp', r'out of memory', 'out of memory: the process exits'),
    ('core/xpp_io.cpp', r'no destination path given', "a Writer given no path: its caller's bug, no file to name"),
    ('core/ode_read.cpp', r'out of memory', 'out of memory: the process exits'),
    ('core/xpp_math.cpp', r'Fourier transform of', 'pocketfft out of memory: the process exits'),
    ('core/ui_json.cpp', r'cannot start the input thread', 'the system refused a thread'),
    ('core/xpp_http.cpp', r'cannot open a port', 'the system refused a port'),
    ('core/xppautx_main.cpp', r'usage: xppautX --convert', "the command line's usage"),
    ('core/xpp_batch.cpp', r'"model", std::string\(what\), xpp::Place\{\}',
     'model_failed(what) outside a load: a model is built only by one, so never reached'),
]

EMPTY_PLACE = re.compile(r'(?:(?:xpp::)?Place\s*)?\{\s*\}')
DEFINITION = re.compile(r'(?:void|int|bool|Error|Result<[^>]*>)\s*[&*]?\s*$')


def sites(path):
    rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
    with open(path, encoding='utf-8', errors='replace') as f:
        text = strip(f.read())
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = f.read().split('\n')
    found = []

    def at(pos, kind):
        line = text.count('\n', 0, pos) + 1
        # a call over several lines is matched by its first three
        written = '\n'.join(lines[line - 1:line + 2])
        if any(f == rel and re.search(pattern, written) for f, pattern, _ in ALLOWED):
            return
        found.append((rel, line, kind))

    for m in re.finditer(r'(?<![\w.>])(?:xpp::(?:json::)?)?(j_)?err_msg\s*\(', text):
        before = text[max(0, m.start() - 40):m.start()]
        if DEFINITION.search(before):
            continue
        args = args_at(text, m.end())
        # the Error form (j_err_msg(e), err_msg's own Error) carries its place
        if len(args) == 1 and not re.match(r'(?:xpp::)?Error\s*\{', args[0]) and not re.fullmatch(r'e|error|\w+\.error\(\)', args[0]):
            at(m.start(), 'err_msg')
    for m in re.finditer(r'(?<![\w.>])(?:xpp::)?fail\s*\(', text):
        args = args_at(text, m.end())
        if (len(args) == 2 and args[0].startswith('"')) or (len(args) == 3 and EMPTY_PLACE.fullmatch(args[2])):
            at(m.start(), 'fail')
    for m in re.finditer(r'(?<![\w.>])(?:xpp::)?Error\s*\{', text):
        args = args_at(text, m.end())
        if (len(args) == 2 and (args[0].startswith('"') or args[0] == '{}')) or \
                (len(args) >= 3 and EMPTY_PLACE.fullmatch(args[2])):
            at(m.start(), 'Error{}')
    for m in re.finditer(r'(?<![\w.>])(?:xpp::)?log(?:_printf)?\s*\(\s*XPP_LOG_ERROR\s*,', text):
        args = args_at(text, m.end())
        if not any('.text()' in a for a in args[1:]):
            at(m.start(), 'log ERROR')
    return found


def collect():
    out = []
    for path in sorted(glob.glob(os.path.join(ROOT, 'core', '*.cpp')) + glob.glob(os.path.join(ROOT, 'core', '*.h'))):
        if os.path.relpath(path, ROOT).replace(os.sep, '/') in OWNERS:
            continue
        out += sites(path)
    return out


def counts(found):
    c = {}
    for rel, _, _ in found:
        c[rel] = c.get(rel, 0) + 1
    return c


def read_baseline():
    base = {}
    with open(BASELINE, encoding='utf-8') as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                base[parts[0]] = int(parts[1])
    return base


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else ''
    if mode not in ('', '--list', '--check', '--update'):
        print('usage: tools/errorcheck.py [--list|--check|--update]', file=sys.stderr)
        return 2
    found = collect()
    now = counts(found)
    total = sum(now.values())
    kinds = {}
    for _, _, k in found:
        kinds[k] = kinds.get(k, 0) + 1
    summary = 'errors with no place: %d in %d files (%s)' % (
        total, len(now), ', '.join('%s %d' % kv for kv in sorted(kinds.items())))
    if mode == '--list':
        for rel, line, kind in found:
            print('%s:%d: %s' % (rel, line, kind))
        print(summary)
    elif mode == '':
        for rel, n in sorted(now.items(), key=lambda kv: (-kv[1], kv[0])):
            print(rel, n)
        print('---')
        print(summary)
    elif mode == '--update':
        with open(BASELINE, 'w', encoding='utf-8', newline='\n') as f:
            for rel in sorted(now):
                f.write('%s %d\n' % (rel, now[rel]))
        print('%s (baseline updated: tests/errors.baseline)' % summary)
    else:
        if not os.path.exists(BASELINE):
            print('errorcheck: missing tests/errors.baseline (run tools/errorcheck.py --update)')
            return 1
        base = read_baseline()
        base_total = sum(base.values())
        grew = [(rel, base.get(rel, 0), n) for rel, n in sorted(now.items()) if n > base.get(rel, 0)]
        for rel, b, n in grew:
            print('errorcheck: %s grew: %d -> %d' % (rel, b, n))
            for r, line, kind in found:
                if r == rel:
                    print('  %s:%d: %s' % (r, line, kind))
        print('%s (baseline %d)' % (summary, base_total))
        if grew:
            print('errorcheck FAILED: report an error as an xpp::Error with its place (xpp_error.h, show_error)')
            return 1
        if total < base_total:
            print('errorcheck: %d fewer than the baseline: run tools/errorcheck.py --update' % (base_total - total))
        print('errorcheck ok: no file grew past the baseline')
    return 0


if __name__ == '__main__':
    sys.exit(main())
