#!/usr/bin/env python3
"""One letter, one command, every key in a menu (W60, decision 6).

core/command_table.h and core/menus.cpp are the only places a key is defined:
the rows of the command table (the three main-window menus' items) and every
XppMenu's `keys`. This fails when

  - the key handler of a main-window menu handles a key its string does
    not list (commander's switch for the main and File menus,
    numerics.cpp get_num_par for the numerics menu), or the string lists
    a key the handler lacks;
  - two case labels share one body (a fall-through: two keys, one
    command), or a key is handled twice;
  - a key string repeats a letter, or an XppMenu's item count differs from
    its key count;
  - a window's key layer (menu_auto_window ... menu_equilibrium_window) has
    a different number of keys than its enum in menus.h (the page has no
    copy of them: hello.windows sends them, W95).

Usage: python3 tools/keycheck.py   (exit 1 on a failure; no options)
"""
import re
import sys
from pathlib import Path

BS = chr(92)
ROOT = Path(__file__).resolve().parent.parent
errors = []


def strip(text):
    """the source without comments and string/char literal contents kept
    (case labels are char literals, so only comments go)"""
    text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def key_char(lit):
    """a char literal or number of a case label as a code"""
    lit = lit.strip()
    if lit.startswith("'"):
        body = lit[1:-1]
        if body.startswith(BS):
            return {'n': 10, 't': 9, BS: 92, "'": 39}.get(body[1], None) if len(body) == 2 else int(body[1:], 8)
        return ord(body)
    return int(lit, 0)


def c_string(lit):
    """the code points of a C string literal's body (octal escapes too)"""
    out, i = [], 0
    while i < len(lit):
        if lit[i] == BS:
            m = re.match(r'\\([0-7]{1,3})', lit[i:])
            if m:
                out.append(int(m.group(1), 8))
                i += len(m.group(0))
                continue
            out.append({'n': 10, 't': 9}.get(lit[i + 1], ord(lit[i + 1])))
            i += 2
            continue
        out.append(ord(lit[i]))
        i += 1
    return out


def block(text, start, open_ch='{', close_ch='}'):
    """the text from the first open_ch at/after start to its match"""
    i = text.index(open_ch, start)
    depth = 0
    for j in range(i, len(text)):
        if text[j] == open_ch:
            depth += 1
        elif text[j] == close_ch:
            depth -= 1
            if depth == 0:
                return text[i + 1:j]
    raise ValueError('unbalanced')


def top_cases(body):
    """the case labels directly in a switch body (depth 0), each with the
    text of its statements up to the next label; a label with no
    statements before the next one is a fall-through"""
    depth, cases, i = 0, [], 0
    label = re.compile(r"case\s+('(?:\\.|[^'\\])+'|\d+|0x[0-9a-fA-F]+)\s*:")
    while i < len(body):
        c = body[i]
        if c in '{(':
            depth += 1
        elif c in '})':
            depth -= 1
        elif depth == 0:
            m = label.match(body, i)
            if m and (i == 0 or not (body[i - 1].isalnum() or body[i - 1] == '_')):
                cases.append([key_char(m.group(1)), m.end(), None])
                i = m.end()
                continue
        i += 1
    for n, c in enumerate(cases):
        end = cases[n + 1][1] if n + 1 < len(cases) else len(body)
        nxt = label.search(body, c[1])
        stop = nxt.start() if nxt and n + 1 < len(cases) else end
        c[2] = body[c[1]:stop].strip()
    return cases


def check_handler(name, cases, keys):
    codes = [c[0] for c in cases]
    for code in set(codes):
        if codes.count(code) > 1:
            errors.append(f'{name}: key {chr(code)!r} is handled twice')
    for code, _, statements in cases:
        if not statements:
            errors.append(f'{name}: key {chr(code)!r} falls through to the next key '
                          f'(two keys, one command)')
    for code in sorted(set(codes) - set(keys)):
        errors.append(f'{name}: handles key {chr(code)!r}, which no menu lists (core/command_table.h)')
    for code in sorted(set(keys) - set(codes)):
        errors.append(f'{name}: the menu lists key {chr(code)!r}, which nothing handles')


def main():
    menus = strip((ROOT / 'core/menus.cpp').read_text(encoding='utf-8'))
    # the three main-window menus' keys are the rows of core/command_table.h (W207)
    table = strip((ROOT / 'core/command_table.h').read_text(encoding='utf-8'))
    strings = {f'{m}_menu_keys': [] for m in ('main', 'file', 'num')}
    for m in re.finditer(r"\{(MAIN|FILE|NUM)_MENU,\s*('(?:\\.|[^'\\])+')\s*,", table):
        strings[f'{m.group(1).lower()}_menu_keys'].append(key_char(m.group(2)))
    for name, codes in strings.items():
        if not codes:
            errors.append(f'command_table.h: no rows for {name}')
        for code in set(codes):
            if codes.count(code) > 1:
                errors.append(f'command_table.h: {name} lists {chr(code)!r} twice')

    # every XppMenu: as many keys as items, no key twice
    for m in re.finditer(r'const XppMenu (\w+)\s*=\s*\{\s*"[^"]*"\s*,\s*"[^"]*"\s*,\s*(\d+)\s*,\s*[\w_]+\s*,'
                         r'\s*"((?:\\.|[^"\\])*)"', menus):
        name, n, keys = m.group(1), int(m.group(2)), c_string(m.group(3))
        if len(keys) != n:
            errors.append(f'menus.cpp: {name} has {n} items and {len(keys)} keys')
        for code in set(keys):
            if keys.count(code) > 1:
                errors.append(f'menus.cpp: {name} lists key {chr(code)!r} twice')

    # the windows' key layers: an enum item per key, in the menu's order
    header = strip((ROOT / 'core/menus.h').read_text(encoding='utf-8'))
    layers = {'AutoWindowKey': 'menu_auto_window', 'BrowserWindowKey': 'menu_browser_window',
              'AniWindowKey': 'menu_ani_window', 'AplotWindowKey': 'menu_aplot_window',
              'EquilibriumWindowKey': 'menu_equilibrium_window'}
    # a menu's size: its XPP_MENU's key string (the compiler checks one
    # key and one kind per item, W95), or a braced initializer's count
    sizes = {m.group(1): len(m.group(2)) for m in re.finditer(
        r'const XppMenu (\w+)\s*=\s*XPP_MENU\(\s*"[^"]*"\s*,\s*"[^"]*"\s*,\s*\w+\s*,\s*"([^"]*)"', menus)}
    sizes.update({m.group(1): int(m.group(2)) for m in re.finditer(
        r'const XppMenu (\w+)\s*=\s*\{\s*"[^"]*"\s*,\s*"[^"]*"\s*,\s*(\d+)', menus)})
    for enum, menu in layers.items():
        m = re.search(r'enum ' + enum + r'\s*\{([^}]*)\}', header)
        count = len(m.group(1).split(',')) if m else -1
        if count != sizes.get(menu):
            errors.append(f'menus.h: enum {enum} has {count} keys, {menu} has {sizes.get(menu)}')

    cmds = strip((ROOT / 'core/commands.cpp').read_text(encoding='utf-8'))
    start = cmds.index('void commander(xpp::Session &s, int ch)')
    body = block(cmds, start)
    outer = block(body, body.index('switch (s.help_menu)'))
    for menu, keys in (('MAIN_MENU', 'main_menu_keys'), ('FILE_MENU', 'file_menu_keys')):
        i = outer.index(f'case {menu}:')
        inner = block(outer, outer.index('switch (ch)', i))
        check_handler(f'commander {menu}', top_cases(inner), strings.get(keys, []))

    nums = strip((ROOT / 'core/numerics.cpp').read_text(encoding='utf-8'))
    start = nums.index('void  get_num_par(xpp::Session &s, char ch)')
    inner = block(nums, nums.index('switch(ch)', start))
    check_handler('get_num_par', top_cases(inner), strings.get('num_menu_keys', []))
    quick = re.search(r'quick_num\(xpp::Session &s, int com\)\s*\{\s*static const char \*const key="([^"]*)"', nums)
    if quick:
        for ch in quick.group(1):
            if ord(ch) not in strings.get('num_menu_keys', []):
                errors.append(f'quick_num: key {ch!r} is not in num_menu_keys')

    if errors:
        print('KEY CHECK FAILED')
        for e in errors:
            print('  ' + e)
        return 1
    print('key check: ok (main, File and numerics keys all in command_table.h; no two keys for one command)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
