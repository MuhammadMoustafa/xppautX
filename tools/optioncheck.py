#!/usr/bin/env python3
"""One option, one row (W293; AGENTS.md "Code quality": adding an option is
one row of the option table plus its member in the settings struct, and
nothing else). The table is core/model_options.cpp. This fails when

  * a core/*.cpp or core/*.h other than the table passes an option's name
    or a numerics key as a string literal (comments stripped) to a function
    that takes one (API below) more often than tests/option_names.baseline
    allows for that file: a file in the baseline has a reason on its line,
    and a count may only drop (a menu's ask, a foreign file's import);
  * web2/src/store/steady.ts, the one place the page spells a numerics key,
    names a key the table does not have.

Modes: --check (sourcecheck.sh), --update (rewrite the baseline, keeping
the reasons of the files still in it), --list (every site).
"""
import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TABLE = "core/model_options.cpp"
BASELINE = "tests/option_names.baseline"
PAGE_KEYS = "web2/src/store/steady.ts"


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace", newline="") as f:
        return f.read().replace("\r\n", "\n")


def strip_comments(text):
    """text with /* */ and // comments blanked, string and char literals kept"""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def table_words():
    """(names lower case, numerics keys) the table spells"""
    t = strip_comments(read(TABLE))
    names = set(m.lower() for m in re.findall(r'\.name = "([A-Za-z_0-9]+)"', t))
    names |= set(m.lower() for m in re.findall(r'\.alias = "([A-Za-z_0-9]+)"', t))
    for group in re.findall(r'\.xppaut_names = \{([^}]*)\}', t):
        names |= set(m.lower() for m in re.findall(r'"([A-Za-z_0-9]+)"', group))
    keys = set(re.findall(r'\.key = "([a-z_0-9]+)"', t))
    return names | keys, keys


# the functions that take an option's name or key; keep_real, keep_whole and
# early are lunch-new.cpp's, which maps XPPAUT's positional .set file to keys
API = ("set_option", "option_problem", "numerics_settings_ask", "numerics_settings_set", "numerics_settings_check",
       "saved_setting", "numerics_option", "find_odex_option", "odex_option_key", "find_option", "keep_real",
       "keep_whole", "early")
CALL = re.compile(r"\b(?:" + "|".join(API) + r")\s*\((.*)")


def sites():
    words, _ = table_words()
    found = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "core", "*.cpp")) + glob.glob(os.path.join(ROOT, "core", "*.h"))):
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        if rel == TABLE:
            continue
        for number, line in enumerate(strip_comments(read(rel)).split("\n"), 1):
            for call in CALL.finditer(line):
                for m in re.finditer(r'"([A-Za-z_0-9]+)"', call.group(1)):
                    if m.group(1).lower() in words:
                        found.setdefault(rel, []).append((number, m.group(1)))
    return found


def baseline():
    allowed = {}
    for line in read(BASELINE).split("\n"):
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split(" ", 2)
        if len(parts) < 3 or not parts[1].isdigit() or not parts[2].strip():
            print(f"optioncheck: {BASELINE}: a line is FILE COUNT REASON: {line!r}")
            sys.exit(1)
        allowed[parts[0]] = (int(parts[1]), parts[2])
    return allowed


def page_problems():
    _, keys = table_words()
    text = strip_comments(read(PAGE_KEYS))
    bad = []
    for m in re.finditer(r"export const ([^;]*);", text):
        for name, key in re.findall(r"(NUM_[A-Z_]+) = '([a-z_0-9]+)'", m.group(1)):
            if key not in keys:
                bad.append(f"{PAGE_KEYS}: {name} = '{key}' is no numerics key of {TABLE}")
    return bad


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--check"
    found = sites()
    if mode == "--list":
        for rel, hits in found.items():
            for number, word in hits:
                print(f"{rel}:{number}: {word}")
        return 0
    if mode == "--update":
        old = baseline() if os.path.exists(os.path.join(ROOT, BASELINE)) else {}
        lines = [f"{rel} {len(hits)} {old[rel][1] if rel in old else 'TODO: why this file names options'}" for rel, hits in found.items()]
        with open(os.path.join(ROOT, BASELINE), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        return 0
    allowed = baseline()
    bad = page_problems()
    for rel, hits in found.items():
        if len(hits) > allowed.get(rel, (0, ""))[0]:
            bad.append(f"{rel}: {len(hits)} option names passed as literals, {allowed.get(rel, (0,))[0]} allowed: "
                       + ", ".join(f"{n}:{w}" for n, w in hits))
    for line in bad:
        print("optioncheck:", line)
    if bad:
        print("An option is one row of core/model_options.cpp: name it there only (or give the file a reason in "
              f"{BASELINE}, uv run --locked python tools/optioncheck.py --update).")
        return 1
    print(f"optioncheck ok: {sum(len(h) for h in found.values())} names in {len(found)} files, none beyond the baseline")
    return 0


sys.exit(main())
