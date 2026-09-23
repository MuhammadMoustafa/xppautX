#!/usr/bin/env python3
"""Give core headers `extern "C"` guards, so C and C++ files can call each
other (the core converts to C++ file by file; CLAUDE.md, "C and C++").

For each header that declares a function or a non-static variable:

    #ifndef X_H              (include guard, when there is one)
    #define X_H
    #include ...             (the leading includes stay outside)
    #ifdef __cplusplus
    extern "C" {
    #endif
    ... declarations ...
    #ifdef __cplusplus
    }
    #endif
    #endif

An #include further down (a few headers include others between their
declarations) is closed around, so no header, system or core, is ever
included inside an extern "C" block. Headers that hold only types and
macros are left alone, as are those already mentioning __cplusplus or
__BEGIN_DECLS (fftn.h, macdirent.h).

One-shot refactoring helper, like the others under tools/: already applied,
kept for the record. It is idempotent (a guarded header is skipped), and
preserves line endings. Usage: tools/cxx_guard_headers.py [-n] core/*.h
(-n: list what it would change, write nothing).
"""
import re
import sys

OPEN = ["#ifdef __cplusplus", 'extern "C" {', "#endif"]
CLOSE = ["#ifdef __cplusplus", "}", "#endif"]


def classify(lines):
    """Per line: 'pp' (preprocessor directive or its continuation),
    'blank' (blank or only comment), 'code'. Also returns the code text of
    each line with comments removed."""
    kinds, code = [], []
    in_comment = False
    cont = False
    for line in lines:
        text, i, out = line, 0, []
        while i < len(text):
            if in_comment:
                j = text.find("*/", i)
                if j < 0:
                    i = len(text)
                else:
                    in_comment, i = False, j + 2
            elif text.startswith("/*", i):
                in_comment, i = True, i + 2
            elif text.startswith("//", i):
                break
            else:
                out.append(text[i])
                i += 1
        stripped = "".join(out).strip()
        if cont:
            kinds.append("pp")
        elif stripped.startswith("#"):
            kinds.append("pp")
        elif stripped == "":
            kinds.append("blank")
        else:
            kinds.append("code")
        cont = kinds[-1] == "pp" and line.rstrip().endswith("\\")
        code.append(stripped if kinds[-1] == "code" else "")
    return kinds, code


def declares(code):
    """Whether the header's code declares a function or a variable other
    code links to: a top-level statement that is not a typedef, not a bare
    struct/union/enum definition and not static."""
    text = " ".join(c for c in code if c)
    depth, stmt = 0, []
    for ch in text:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                s = "".join(stmt).strip()
                if re.search(r"\)\s*$", s.split("{")[0]):  # function definition
                    if not s.startswith("static"):
                        return True
                    stmt = []
                    continue
        if ch == ";" and depth == 0:
            s = re.sub(r"\{.*\}", "{}", "".join(stmt), flags=re.S).strip()
            stmt = []
            if s.startswith("typedef") or s.startswith("static"):
                continue
            if re.fullmatch(r"(struct|union|enum)\s*\w*\s*\{\}", s):
                continue
            if s:
                return True
            continue
        stmt.append(ch)
    return False


def directive(line):
    m = re.match(r"\s*#\s*(\w+)\s*(\S*)", line)
    return (m.group(1), m.group(2)) if m else (None, None)


def process(path, dry):
    src = open(path, newline="").read()
    if "__cplusplus" in src or "__BEGIN_DECLS" in src:
        return "skip (has its own)"
    nl = "\r\n" if "\r\n" in src else "\n"
    trailing = src.endswith(nl)
    lines = src.split(nl)
    if trailing:
        lines.pop()
    kinds, code = classify(lines)
    if not declares(code):
        return "skip (types and macros only)"

    # the include guard: first two directives #ifndef X / #define X, closed
    # by the last #endif of the file
    first = [i for i, k in enumerate(kinds) if k != "blank"]
    start, end = 0, len(lines)
    if len(first) >= 2 and kinds[first[0]] == kinds[first[1]] == "pp":
        d0, d1 = directive(lines[first[0]]), directive(lines[first[1]])
        if d0[0] == "ifndef" and d1 == ("define", d0[1]):
            last = first[-1]
            if directive(lines[last])[0] == "endif":
                start, end = first[1] + 1, last

    # the leading directives: the extern block opens after the last
    # #include there that is at the top conditional level (or after the
    # conditional block holding it)
    depth, open_at, saw, i = 0, start, False, start
    while i < end and kinds[i] != "code":
        if kinds[i] == "pp":
            d = directive(lines[i])[0]
            if d in ("if", "ifdef", "ifndef"):
                depth += 1
            elif d == "endif":
                depth -= 1
                if depth == 0 and saw:
                    open_at = i + 1
            elif d == "include":
                saw = True
                if depth == 0:
                    open_at = i + 1
        i += 1
    # later #includes: close the block around them
    wraps = []
    j = open_at
    while j < end:
        if kinds[j] == "pp" and directive(lines[j])[0] == "include":
            k = j
            while k + 1 < end and (kinds[k + 1] == "blank" and lines[k + 1].strip() == ""
                                   or kinds[k + 1] == "pp" and directive(lines[k + 1])[0] == "include"):
                k += 1
            while lines[k].strip() == "":
                k -= 1
            wraps.append((j, k))
            j = k + 1
        else:
            j += 1
    if dry:
        return "guard (open after line %d, %d include group(s) inside)" % (open_at, len(wraps))

    out = []
    for n, line in enumerate(lines):
        if n == open_at:
            out += OPEN + ([""] if line.strip() else [])
        if any(n == a for a, _ in wraps):
            out += CLOSE
        if n == end:
            if out and out[-1].strip() != "":
                out.append("")
            out += CLOSE
        out.append(line)
        if any(n == b for _, b in wraps):
            out += OPEN
    if end == len(lines):
        if out and out[-1].strip() != "":
            out.append("")
        out += CLOSE
    if open_at == len(lines):  # cannot happen: a header that declares has code
        raise SystemExit("%s: nothing to guard" % path)
    open(path, "w", newline="").write(nl.join(out) + (nl if trailing else ""))
    return "guarded (%d include group(s) closed around)" % len(wraps)


def main():
    dry = "-n" in sys.argv[1:]
    for path in [a for a in sys.argv[1:] if a != "-n"]:
        print("%-24s %s" % (path, process(path, dry)))


if __name__ == "__main__":
    main()
