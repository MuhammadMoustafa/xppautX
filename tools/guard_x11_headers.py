#!/usr/bin/env python3
"""Wrap X11-typed declarations in a header in `#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)` blocks and
drop the header's own `#include <X11/Xlib.h>`.

One-off refactoring helper for phase 1 of xppautX. Idempotent enough to
re-run, but review the diff.
"""
import re
import sys

XTYPES = r"\b(Window|Display|GC|XEvent|XFontStruct|Pixmap|Colormap|KeySym|XPoint|Cursor|Drawable|XColor|XImage|Visual|XSizeHints|XButtonEvent|XKeyEvent|Atom|XSegment|XRectangle|XGCValues|XWindowAttributes)\b"


def process(path):
    src = open(path, newline="").read()
    nl = "\r\n" if "\r\n" in src else "\n"
    lines = src.split(nl)

    # 1. drop the Xlib include
    lines = [l for l in lines if not re.match(r"\s*#\s*include\s*<X11/", l)]

    # 2. find typedef struct blocks; mark whole block if it uses X types
    marked = [False] * len(lines)
    guarded_types = set()
    i = 0
    while i < len(lines):
        if re.match(r"\s*typedef\s+struct\s*\{?", lines[i]) and "}" not in lines[i]:
            j = i
            while j < len(lines) and not re.search(r"\}\s*\w+\s*;", lines[j]):
                j += 1
            block = lines[i : j + 1]
            if any(re.search(XTYPES, l) for l in block):
                for k in range(i, j + 1):
                    marked[k] = True
                m = re.search(r"\}\s*(\w+)\s*;", lines[j])
                if m:
                    guarded_types.add(m.group(1))
            i = j + 1
        else:
            i += 1

    # 3. mark single-line declarations that use X types or guarded structs
    pat = XTYPES
    if guarded_types:
        pat = XTYPES + "|" + r"\b(" + "|".join(sorted(guarded_types)) + r")\b"
    for idx, l in enumerate(lines):
        if marked[idx]:
            continue
        if l.strip().startswith("#") or l.strip().startswith("/*") or l.strip().startswith("*"):
            continue
        if re.search(pat, l):
            marked[idx] = True

    # 4. emit with grouped ifdef blocks
    out = []
    in_block = False
    for l, m in zip(lines, marked):
        if m and not in_block:
            out.append("#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)")
            in_block = True
        elif not m and in_block and l.strip() != "":
            out.append("#endif /* Xlib.h */")
            in_block = False
        out.append(l)
    if in_block:
        out.append("#endif /* Xlib.h */")

    # keep the trailing #endif of the include guard last
    open(path, "w", newline="").write(nl.join(out))
    n = sum(marked)
    print(f"{path}: guarded {n} lines, structs: {sorted(guarded_types)}")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        process(p)
