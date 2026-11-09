#!/usr/bin/env python3
"""One spelling: "color" (W669, maintainer 2026-11-09: "I think color everywhere? since color seems to be the
convention"). Fails the British spelling in any form, text and code alike, in core/, tests/, tools/, web2/src,
web2/test, docs/manual and docs/protocol.md: since no code or text of ours spells it so, a plain word scan is the
whole check. History (docs/studies, the roadmap, released CHANGELOG sections) is not scanned.
Usage: tools/colorcheck.py [--check] [FILE-OR-DIR...]   (exit 1 on a finding with --check)"""
import os, re, sys

BRITISH = re.compile("colo" + "ur", re.I)  # split so this file does not find itself
PATHS = ["core", "tests", "tools", "web2/src", "web2/test", "docs/manual", "docs/protocol.md"]
SUFFIXES = (".cpp", ".h", ".py", ".sh", ".mjs", ".ts", ".tsx", ".css", ".md", ".odex", ".ode", ".json")


def files(paths):
    for p in paths:
        if os.path.isfile(p):
            yield p
        for root, _, names in os.walk(p):
            for n in sorted(names):
                if n.endswith(SUFFIXES):
                    yield os.path.join(root, n)


def main(argv):
    check = "--check" in argv
    paths = [a for a in argv if a != "--check"] or PATHS
    found = 0
    for f in files(paths):
        with open(f, encoding="utf-8", errors="replace") as fh:
            for i, line in enumerate(fh, 1):
                if BRITISH.search(line):
                    print(f"{f}:{i}: {line.strip()[:120]}")
                    found += 1
    print(f"colorcheck: {found} finding(s)" if found else "colorcheck ok: one spelling, color")
    return 1 if check and found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
