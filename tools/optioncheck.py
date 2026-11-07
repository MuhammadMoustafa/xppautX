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
    names a key the table does not have;
  * a grouped row of the table has no `.odex` key, a key that is not snake_case (dots only between
    a section and its key), or one key twice in a group (W615), or no one-line `.help` (W619: key, label and help on every
    grouped row; the label is derived from the key, option_label).

Modes: --check (sourcecheck.sh), --update (rewrite the baseline, keeping
the reasons of the files still in it), --list (every site).
"""
import glob
import os
import re
import sys

import cstrip

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TABLE = "core/model_options.cpp"
BASELINE = "tests/option_names.baseline"
PAGE_KEYS = "web2/src/store/steady.ts"


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace", newline="") as f:
        return f.read().replace("\r\n", "\n")


def table_words():
    """(names lower case, numerics keys) the table spells"""
    t = cstrip.strip_comments(read(TABLE))
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
        for number, line in enumerate(cstrip.strip_comments(read(rel)).split("\n"), 1):
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
    text = cstrip.strip_comments(read(PAGE_KEYS))
    bad = []
    for m in re.finditer(r"export const ([^;]*);", text):
        for name, key in re.findall(r"(NUM_[A-Z_]+) = '([a-z_0-9]+)'", m.group(1)):
            if key not in keys:
                bad.append(f"{PAGE_KEYS}: {name} = '{key}' is no numerics key of {TABLE}")
    return bad


def key_problems():
    """the table's grouped rows: each has a key, in snake case, once in its group"""
    text = cstrip.strip_comments(read(TABLE))
    start = text.index("constexpr OptionRow rows[] = {")
    bad, seen = [], {}
    for row in re.split(r"\n  \{", text[start:])[1:]:
        group = re.match(r"\.group = option_group::([a-z_]+)", row)
        if not group:
            continue
        head = row.split("\n")[0]
        name = re.search(r'\.name = "([A-Za-z_0-9.]+)"', head)
        key = re.search(r'\.odex = (?:"([^"]*)"|(autox_num_keys\[AUTOX_NUM_[A-Z_]+\]))', head)
        label = name.group(1) if name else head[:40]
        if not key or not (key.group(1) or key.group(2)):
            bad.append(f"{TABLE}: the grouped row {label} has no .odex key")
            continue
        if key.group(2):  # autoX's numerics: their keys are autox_settings.h's, one list
            if not re.search(r'\.help = "[^"]{4,}"', head):
                bad.append(f"{TABLE}: the grouped row {label} has no one-line .help after its .odex key")
            seen[(group.group(1), key.group(2), False)] = label
            continue
        if not re.search(r'\.help = "[^"]{4,}"', head):
            bad.append(f"{TABLE}: the grouped row {label} has no one-line .help after its .odex key")
        if not re.fullmatch(r"([a-z][a-z0-9_]*\.)?[a-z][a-z0-9_]*", key.group(1)):
            bad.append(f"{TABLE}: the key {key.group(1)} of {label} is not snake_case (a section, a dot, the key)")
        pair = (group.group(1), key.group(1), "first_digit" in row.split("\n")[0] + (row.split("\n")[1] if "\n" in row else ""))
        if pair in seen:
            bad.append(f"{TABLE}: the key {key.group(1)} is given twice in the group {group.group(1)} ({seen[pair]} and {label})")
        seen[pair] = label
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
    bad = page_problems() + key_problems()
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
