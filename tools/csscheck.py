#!/usr/bin/env python3
"""One rule per thing in web2's CSS (W638, maintainer 2026-11-07: "the rule is
unifications and no duplications"). Fails, in web2/src/**/*.css:
  - a declaration block (two or more declarations, in any order) that two
    rules of one media condition both write: one rule with a selector list;
  - a selector that two rules of one media condition both start: one rule;
  - a width breakpoint off docs/ui-v2.md's list (R3: 48rem and 80rem, written
    min-width: 48rem / 80rem, and max-width: 47.99rem / 79.99rem on the other
    side; @container queries are a component's own, R4, and are not checked);
  - a z-index that is not a named layer, `var(--z-...)` (tokens.css owns the
    numbers, each with its reason; a modal dialog and a popup menu are in the
    browser's top layer, ui/Modal.tsx, over every z-index);
Comments are stripped first; @keyframes and @font-face bodies are not rules.
Usage: tools/csscheck.py [--check] [FILE-OR-DIR...]   (exit 1 on a finding)"""
import re, sys, glob, os

BREAKPOINTS = {"min-width": {"48rem", "80rem"}, "max-width": {"47.99rem", "79.99rem"}}
MIN_DECLS = 2  # a one-declaration block (display: none) is too small to be worth a shared rule

def strip_comments(text):
    return re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)

def parse(text, path):
    """(path, line, media, selectors, declarations) per style rule."""
    rules, stack, i, n, line, cur_start = [], [], 0, len(text), 1, 0
    while i < n:
        c = text[i]
        if c == "{":
            head = text[cur_start:i].strip()
            if head.startswith("@"):
                stack.append(("at", head))
            else:
                j = text.index("}", i)
                body = text[i + 1:j]
                at = [h for k, h in stack if k == "at"]
                if not any(h.startswith(("@keyframes", "@font-face")) for h in at):
                    media = " and ".join(re.sub(r"\s+", " ", h) for h in at)
                    decls = [re.sub(r"\s+", " ", d.strip()) for d in body.split(";") if d.strip()]
                    sels = [re.sub(r"\s+", " ", s.strip()) for s in head.split(",") if s.strip()]
                    rules.append((path, line, media, sels, decls))
                i = j
                line += body.count("\n")
            cur_start = i + 1
        elif c == "}":
            if stack:
                stack.pop()
            cur_start = i + 1
        elif c == "\n":
            line += 1
        i += 1
    return rules

def check(paths):
    rules = []
    for p in paths:
        with open(p, encoding="utf-8") as f:
            rules += parse(strip_comments(f.read()), p)
    bad = []
    blocks, selectors = {}, {}
    for path, line, media, sels, decls in rules:
        where = f"{path}:{line}"
        key = frozenset(decls)
        if len(key) >= MIN_DECLS:
            blocks.setdefault((media, key), []).append((where, sels))
        for s in sels:
            selectors.setdefault((media, s), []).append(where)
        for d in decls:
            m = re.match(r"z-index\s*:\s*(.*)$", d)
            if m and not re.fullmatch(r"var\(--z-[a-z0-9-]+\)", m.group(1)):
                bad.append(f"{where}: z-index {m.group(1)} is not a named layer (var(--z-...), tokens.css)")
        for m in re.finditer(r"\((min|max)-width:\s*([^)\s]+)\s*\)", media if "@media" in media else ""):
            if m.group(2) not in BREAKPOINTS[m.group(1) + "-width"]:
                bad.append(f"{where}: breakpoint {m.group(1)}-width: {m.group(2)} is not on docs/ui-v2.md's list "
                           f"({', '.join(sorted(BREAKPOINTS[m.group(1) + '-width']))})")
    for (media, _), found in blocks.items():
        if len(found) > 1:
            names = " | ".join(", ".join(s) for _, s in found)
            bad.append(f"{found[0][0]}: the same declarations in {len(found)} rules ({', '.join(w for w, _ in found)}): {names}"
                       + (f" [{media}]" if media else ""))
    for (media, sel), found in selectors.items():
        if len(found) > 1:
            bad.append(f"{found[0]}: selector {sel} is in {len(found)} rules ({', '.join(found)})" + (f" [{media}]" if media else ""))
    return len(rules), bad

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    paths = []
    for a in args or ["web2/src"]:
        paths += sorted(glob.glob(os.path.join(a, "**", "*.css"), recursive=True)) if os.path.isdir(a) else [a]
    n, bad = check(paths)
    for b in bad:
        print(b)
    print(f"csscheck: {n} rules in {len(paths)} files, {len(bad)} findings")
    return 1 if bad and "--check" in sys.argv else 0

sys.exit(main())
