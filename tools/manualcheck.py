#!/usr/bin/env python3
"""The manual is for users (W643, maintainer 2026-11-07: "Card them"): docs/manual names only what a user sees and does. Fails, in
docs/manual/*.md:
  - a card number (`W` and digits: W526, W121b);
  - a source path (a file ending .tsx, .ts, .cpp, .h or .mjs: `ui/TableView.tsx`, `core/comline.cpp`);
  - a letter mnemonic: XPPAUT's menu letters ("(F)ind", "n(U)merics") and a capital inside a lower-case word ("sHoot", "reDraw"),
    in prose and in code spans; fenced code, math ($`...`$ and $...$) and link targets are not prose and are skipped.
An entry of ALLOWED_WORDS or ALLOWED names a reason: a product's own spelling (macOS, LaTeX), a model's own name in an example.
What differs from XPPAUT is said once, in docs/xppautx-vs-xppaut.md, and linked.
Usage: tools/manualcheck.py [--check] [DIR]   (exit 1 on a finding)"""
import glob, os, re, sys

# words with a capital inside that are a name's own spelling, each with its reason
ALLOWED_WORDS = {
    "QualRK": "a numerical method's own name",
    "autoX": "the program's continuation, spelled so",
    "xppautX": "the program's name, spelled so",
    "macOS": "Apple's name", "iPhone": "Apple's name", "iPad": "Apple's name", "WKWebView": "Apple's web view class",
    "WebKitGTK": "the web engine's name", "GitHub": "a site's name", "LinkedIn": "a site's name", "openSUSE": "a Linux distribution's name",
    "LaTeX": "the typesetting system's name", "NumPy": "the package's name", "PostScript": "the page language's name",
    "ImageMagick": "a program's name", "MIR": "the compiler's name", "SemVer": "the versioning scheme's name",
    "DoPri": "the integrator's name (Dormand-Prince)", "MiB": "the unit's symbol", "NaN": "the floating-point value's name",
    "MuhammadMoustafa": "the repository owner in a web address",
}
# (file, text): a model's own name or an XPPAUT name a model uses, with its reason
ALLOWED = {
    ("02-ode-files.md", "DeltaT"): "a parameter's name in a model line",
    ("09-autox.md", "SuppBP"): "autoX's own name for the setting, as AUTO spells it",
}
SOURCE_PATH = re.compile(r"[\w./-]+\.(?:tsx|ts|cpp|h|mjs)\b")
CARD = re.compile(r"\bW\d{2,4}[a-z]?\b")
PAREN_LETTER = re.compile(r"[A-Za-z]*\([A-Z]\)[A-Za-z]+|[A-Za-z]+\([A-Z]\)|\(\d\)[A-Za-z]+")
INNER_CAPITAL = re.compile(r"\b[A-Za-z]*[a-z][A-Z][A-Za-z]*\b")
NOT_PROSE = re.compile(r"```.*?```|\$`.*?`\$|\$[^$\n]*\$|\]\([^)\n]*\)|<[^>\n]*>", re.S)

def blank(text):
    return re.sub(r"[^\n]", " ", text)

def scan(path):
    base = os.path.basename(path)
    with open(path, encoding="utf-8") as f:
        text = f.read()
    visible = NOT_PROSE.sub(lambda m: blank(m.group(0)), text)
    found = []
    def add(kind, m):
        word = m.group(0)
        if (base, word) in ALLOWED or word in ALLOWED_WORDS: return
        found.append(f"{base}:{visible.count(chr(10), 0, m.start()) + 1}: {kind} {word}")
    for m in CARD.finditer(visible): add("card number", m)
    for m in SOURCE_PATH.finditer(visible): add("source path", m)
    for m in PAREN_LETTER.finditer(visible.replace("**", "  ")):
        if not re.search(r"\\|[=+*/^<>_]", visible[max(0, m.start() - 2):m.end() + 2]):
            add("letter mnemonic", m)
    for m in INNER_CAPITAL.finditer(visible.replace("**", "  ")): add("capital inside a word", m)
    return found

def main():
    root = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "manual")
    args = [a for a in sys.argv[1:] if a != "--check"]
    paths = sorted(glob.glob(os.path.join(args[0] if args else root, "*.md")))
    bad = [line for path in paths for line in scan(path)]
    for line in bad[:60]: print(line)
    print(f"manualcheck: {len(paths)} files, {len(bad)} findings")
    sys.exit(1 if bad else 0)

main()
