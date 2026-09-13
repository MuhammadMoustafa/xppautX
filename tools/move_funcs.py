#!/usr/bin/env python3
"""Cut named top-level C function definitions out of a source file.

Usage: move_funcs.py SRC.c OUT.c name1 name2 ...
Removes each function (K&R or ANSI definition style) from SRC.c and appends
it to OUT.c. Also handles a '/* comment */' block immediately before the
definition. Refactoring helper; review the diff.
"""
import re
import sys

src, out, names = sys.argv[1], sys.argv[2], sys.argv[3:]
text = open(src, newline="").read()
nl = "\r\n" if "\r\n" in text else "\n"
lines = text.split(nl)

start_re = re.compile(
    r"^\s*(?:(?:static\s+|extern\s+)?(?:unsigned\s+|const\s+)*(?:void|int|char|float|double|long|short|FILE|Window|BROWSER|GRAPH)\s*\**\s+)?\**(%s)\s*\("
    % "|".join(re.escape(n) for n in names)
)

moved = {}
i = 0
keep = []
while i < len(lines):
    m = start_re.match(lines[i])
    if m and not lines[i].rstrip().endswith(";"):
        name = m.group(1)
        # find the opening brace of the body, then its matching close brace
        j = i
        depth = 0
        seen_open = False
        while j < len(lines):
            for ch in lines[j]:
                if ch == "{":
                    depth += 1
                    seen_open = True
                elif ch == "}":
                    depth -= 1
            if seen_open and depth == 0:
                break
            j += 1
        block = lines[i : j + 1]
        moved[name] = block
        i = j + 1
        # drop one trailing blank line
        if i < len(lines) and lines[i].strip() == "":
            i += 1
        continue
    keep.append(lines[i])
    i += 1

missing = [n for n in names if n not in moved]
if missing:
    sys.exit("not found in %s: %s" % (src, " ".join(missing)))

open(src, "w", newline="").write(nl.join(keep))
with open(out, "a", newline="") as f:
    for n in names:
        f.write(nl + nl.join(moved[n]) + nl)
print("moved from %s to %s: %s" % (src, out, " ".join(names)))
