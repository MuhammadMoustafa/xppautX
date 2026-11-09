#!/usr/bin/env python3
"""One spelling in user text (W669, maintainer 2026-11-09: "color", the
convention). Fails the British "colour", "colours" and "coloured" where a
user reads them:
  - web2/src/**/*.tsx: a JSX text node, and a string literal of a title,
    aria-label, placeholder or label attribute (its value, or a string inside
    its {expression}); a `${...}` in a template is code and is not checked;
  - docs/manual/*.md prose: not in a code span, a fenced code block, an
    indented code block or a link's target (13-colors.md is a file name).
Identifiers, CSS properties, option keys, protocol fields and file keywords
are code and are never checked, so they need no allowlist. A label derived
from a key is listed by the report, not checked (the maintainer decides keys).
Usage: tools/colorcheck.py [--check] [FILE-OR-DIR...]   (exit 1 on a finding
with --check; default paths web2/src and docs/manual)"""
import re, sys, glob, os

BRITISH = re.compile(r"\bcolour(s|ed|ing)?\b", re.I)
RENDER_ATTRS = r"(?:title|aria-label|placeholder|label)"

def british(text):
    return [m.group(0) for m in BRITISH.finditer(text)]

def strip_block_comments(text):
    return re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)

def line_of(text, pos):
    return text.count("\n", 0, pos) + 1

def balanced(text, start):
    """the text inside the braces that open at text[start] == '{', and the index after them"""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start + 1:i], i + 1
    return text[start + 1:], len(text)

def string_parts(expr):
    """the text of each string literal in a JS expression: quoted, and a template's text outside ${...}"""
    parts = []
    for m in re.finditer(r"'((?:[^'\\\n]|\\.)*)'|\"((?:[^\"\\\n]|\\.)*)\"|`((?:[^`\\]|\\.)*)`", expr):
        if m.group(3) is not None:
            parts.append(re.sub(r"\$\{[^}]*\}", " ", m.group(3)))
        else:
            parts.append(m.group(1) if m.group(1) is not None else m.group(2))
    return parts

def check_tsx(path, text):
    bad = []
    text = strip_block_comments(text)
    text = "\n".join(" " * len(l) if l.lstrip().startswith("//") else l for l in text.split("\n"))
    # a tag's closing >, after an attribute or a name, then its text up to the next tag (not => or />, not code)
    for m in re.finditer(r"(?<=[\w\"'}\]])>([^<>{}=;]+)<", text):
        for word in british(m.group(1)):
            bad.append(f"{path}:{line_of(text, m.start(1))}: JSX text has {word!r}")
    for m in re.finditer(r"\b(" + RENDER_ATTRS + r")\s*=\s*", text):
        at = m.end()
        if text[at:at + 1] in "\"'":
            quote = text[at]
            end = text.find(quote, at + 1)
            value = text[at + 1:end] if end > 0 else ""
            parts = [value]
        elif text[at:at + 1] == "{":
            inner, _ = balanced(text, at)
            parts = string_parts(inner)
        else:
            continue
        for part in parts:
            for word in british(part):
                bad.append(f"{path}:{line_of(text, m.start())}: {m.group(1)} has {word!r}")
    return bad

def prose_lines(text):
    """(line number, prose) for each line of a manual page outside code: fenced and indented code blocks
    dropped, code spans and link targets blanked"""
    out, fence, prev_blank = [], False, True
    list_item = re.compile(r"^\s*(?:[-*+]|\d+[.)])\s")
    for n, line in enumerate(text.split("\n"), 1):
        if line.lstrip().startswith("```"):
            fence = not fence
            prev_blank = False
            continue
        if fence:
            continue
        if line.startswith("    ") and prev_blank and not list_item.match(line):
            continue
        if line.strip() == "": prev_blank = True
        else: prev_blank = False
        line = re.sub(r"\]\([^)\s]*\)", lambda m: "](" + " " * (len(m.group(0)) - 3) + ")", line)
        line = re.sub(r"`[^`]*`", lambda m: " " * len(m.group(0)), line)
        out.append((n, line))
    return out

def check_md(path, text):
    bad = []
    for n, line in prose_lines(text):
        for word in british(line):
            bad.append(f"{path}:{n}: prose has {word!r}")
    return bad

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    targets = args or ["web2/src", "docs/manual"]
    files = []
    for a in targets:
        if os.path.isdir(a):
            files += sorted(glob.glob(os.path.join(a, "**", "*.tsx"), recursive=True))
            files += sorted(glob.glob(os.path.join(a, "*.md")))
        else:
            files.append(a)
    bad = []
    for f in files:
        with open(f, encoding="utf-8", newline="") as fh:
            text = fh.read().replace("\r\n", "\n")
        bad += check_tsx(f, text) if f.endswith(".tsx") else check_md(f, text) if f.endswith(".md") else []
    for b in bad:
        print(b)
    print(f"colorcheck: {len(files)} files, {len(bad)} findings")
    return 1 if bad and "--check" in sys.argv else 0

if __name__ == "__main__":
    sys.exit(main())
