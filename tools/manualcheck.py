#!/usr/bin/env python3
"""Sanity check for docs/manual/*.md (W81): every whole model the manual
shows must still load.

A model block is an indented (4-space) code block whose last non-blank
line, dedented, is exactly "done" -- the ODE file format's own end
marker (docs/manual/02-ode-files.md). That is a model an author wrote to
be typed in and run, as opposed to a syntax fragment (a single command,
a table snippet, one line of a bigger example) that never ends in
"done" and is skipped by this rule, not by an allowlist of line numbers.

Each block found is written to a temp .ode file and loaded with
`xppautX -silent ... -qics` (docs/comline.h's -qics: batch mode, a dry
run -- it queries the initial conditions and writes them to -outfile
instead of integrating, but still runs the whole parser and model
set-up, so a bad model still fails to load). A nonzero exit is a load
failure and fails the check, printing the file, the block's starting
line and xppautX's stderr tail.

Also checks that every "-name" command-line option the manual names
(docs/manual/*.md, backtick-quoted, e.g. `-silent`, `-parfile
*filename*`) is one `xppautX --help` and core/comline.cpp's option table
both know: `--help` doesn't itself list xpp's own options (only its own
front-end flags), so this reads comline.cpp's my_cmd[] table instead of
running --help for that part; --help is still run and checked for the
handful of front-end flags (--browser, --server, ...) the manual names.

usage: tools/manualcheck.py [--bin ./xppautX] [-v]
  --bin  the xppautX binary (default ./xppautX, or ./xppautX.exe)
  -v     print each model block as it is checked
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def find_bin(explicit):
    if explicit:
        return explicit
    for name in ("xppautX", "xppautX.exe"):
        p = os.path.join(ROOT, name)
        if os.path.isfile(p):
            return p
    print("manualcheck: no xppautX binary found (build first, or pass --bin)")
    sys.exit(2)


def dedent_block(lines):
    """lines: raw lines of an indented code block (with their leading
    4+ spaces). Strips the common leading whitespace (markdown's own
    4-space indent, at least)."""
    indents = [len(l) - len(l.lstrip(" ")) for l in lines if l.strip()]
    cut = min(indents) if indents else 0
    return [l[cut:] if len(l) >= cut else l.lstrip(" ") for l in lines]


PLACEHOLDER_RE = re.compile(r"<[A-Za-z_][\w]*>")


def is_syntax_template(lines):
    """True for a block that shows the *grammar* of ODE files (docs/manual/
    02-ode-files.md's "ODE File format", 16-quick-reference.md's "ODE File
    Format") rather than a whole runnable model: it uses `<name>`-style
    placeholders, "<--" annotations pointing at a line, or a bare "..."
    line standing in for repeated/omitted lines -- none of which is ever
    real ODE syntax (a real model's ">"/"<" are comparisons like `x>1`,
    never a lone `<word>`)."""
    for l in lines:
        if "<--" in l:
            return True
        if PLACEHOLDER_RE.search(l):
            return True
    return False


def find_model_blocks(path):
    """Yields (start_line, [dedented source lines]) for every indented
    code block in a manual file whose last non-blank line is "done"."""
    with open(path, "r", encoding="utf-8") as f:
        raw = f.readlines()

    blocks = []
    i = 0
    n = len(raw)
    in_fence = False
    while i < n:
        line = raw[i]
        stripped = line.rstrip("\n")
        # skip fenced ``` blocks entirely -- they are prose/math/shell,
        # never the manual's model examples (those are 4-space indented)
        if stripped.strip().startswith("```"):
            in_fence = not in_fence
            i += 1
            continue
        if in_fence:
            i += 1
            continue
        is_indented = stripped.startswith("    ") and stripped.strip() != ""
        is_blank = stripped.strip() == ""
        if is_indented:
            start = i
            block = []
            while i < n:
                s = raw[i].rstrip("\n")
                if s.strip() == "":
                    # a single blank line inside an indented block is
                    # still part of it (markdown keeps the block open
                    # across blank lines followed by more indented text)
                    j = i + 1
                    if j < n and raw[j].rstrip("\n").startswith("    ") and raw[j].strip() != "":
                        block.append(s)
                        i += 1
                        continue
                    break
                if not s.startswith("    "):
                    break
                block.append(s)
                i += 1
            dedented = dedent_block(block)
            last = ""
            for l in reversed(dedented):
                if l.strip() != "":
                    last = l.strip()
                    break
            if last == "done" and not is_syntax_template(dedented):
                blocks.append((start + 1, dedented))
            continue
        i += 1
    return blocks


def check_models(xppbin, verbose):
    failures = []
    checked = 0
    tmpdir = tempfile.mkdtemp(prefix="manualcheck-")
    try:
        for path in sorted(glob.glob(os.path.join(ROOT, "docs", "manual", "*.md"))):
            for start, lines in find_model_blocks(path):
                checked += 1
                relpath = os.path.relpath(path, ROOT)
                if verbose:
                    print(f"checking {relpath}:{start}")
                model_path = os.path.join(tmpdir, "model.ode")
                converted_path = os.path.join(tmpdir, "model.odex")
                if os.path.exists(converted_path):
                    os.remove(converted_path)  # each snippet is a new conversion input
                out_path = os.path.join(tmpdir, "out.txt")
                with open(model_path, "w", encoding="utf-8", newline="\n") as f:
                    f.write("\n".join(lines) + "\n")
                proc = subprocess.run(
                    [xppbin, model_path, "-silent", "-qics", "-outfile", out_path],
                    cwd=tmpdir,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    timeout=30,
                )
                if proc.returncode != 0:
                    tail = proc.stdout.decode("utf-8", "replace").strip().splitlines()[-10:]
                    failures.append(
                        f"{relpath}:{start}: xppautX exited {proc.returncode}\n  "
                        + "\n  ".join(tail)
                    )
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)
    return checked, failures


# Only two conventions the manual uses to document an actual xppautX
# command-line flag (never prose, which also puts a hyphenated word or a
# negated formula variable in backticks -- `-a`, `-thick` -- that is not
# a flag at all):
#   | `-name ARG` | ... |          a table row (docs/manual/01-introduction.md)
#   - **-name *arg***: ...         a bulleted definition (docs/manual/16-quick-reference.md)
TABLE_ROW_RE = re.compile(r"^\|\s*`-")
TABLE_OPTION_RE = re.compile(r"`(-{1,2}[A-Za-z][\w-]*)")
BOLD_OPTION_RE = re.compile(r"^-?\s*\*\*(-{1,2}[A-Za-z][\w-]*)\*\*")


def manual_options():
    """Every option an actual "flags" table row or bulleted definition
    names, across docs/manual/*.md. A table row's first cell may name
    several with `/` between them (`-qsets` / `-qpars` / `-qics`)."""
    found = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "docs", "manual", "*.md"))):
        relpath = os.path.relpath(path, ROOT)
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                stripped = line.strip()
                if TABLE_ROW_RE.match(stripped):
                    cell = stripped.split("|")[1]
                    for m in TABLE_OPTION_RE.finditer(cell):
                        found.setdefault(m.group(1), relpath)
                    continue
                m = BOLD_OPTION_RE.match(stripped)
                if m:
                    found.setdefault(m.group(1), relpath)
    return found


def comline_options():
    """The option names core/comline.cpp's my_cmd[] table accepts."""
    path = os.path.join(ROOT, "core", "comline.cpp")
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"my_cmd\[NCMD\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        return set()
    return set(re.findall(r'\{"(-[\w-]+)"', m.group(1)))


def help_options(xppbin):
    proc = subprocess.run([xppbin, "--help"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
    text = proc.stdout.decode("utf-8", "replace")
    return set(re.findall(r"(?:^|\s)(--[A-Za-z][\w-]*)", text))


def check_options(xppbin):
    manual = manual_options()
    known = comline_options() | help_options(xppbin) | {"--help"}
    failures = []
    for opt, where in sorted(manual.items()):
        if opt.startswith("--"):
            # a front-end flag: must be in --help's own text or comline's
            # table (a few, like -silent, are xpp's and predate --)
            if opt not in known:
                failures.append(f"{where}: `{opt}` is not in `xppautX --help` or comline.cpp")
        else:
            if opt not in known:
                failures.append(f"{where}: `{opt}` is not in core/comline.cpp's option table")
    return manual, failures


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    xppbin = find_bin(args.bin)

    checked, model_failures = check_models(xppbin, args.verbose)
    manual_opts, option_failures = check_options(xppbin)

    failures = model_failures + option_failures
    if failures:
        for f in failures:
            print(f"FAIL {f}")
        print(f"manualcheck: {checked} model blocks, {len(manual_opts)} options named, "
              f"{len(failures)} failure(s)")
        return 1

    print(f"manualcheck: {checked} model blocks loaded, {len(manual_opts)} command-line "
          f"options named in the manual all recognised")
    return 0


if __name__ == "__main__":
    sys.exit(main())
