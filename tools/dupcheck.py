#!/usr/bin/env python3
# Duplication audit of core/ (W30): lists code that is a candidate to be
# merged into a single owning module ("Single source" in CLAUDE.md's
# Conventions), in the style of tools/deadcode.sh: a plain run prints the
# report grouped by kind; --check fails on anything not in the ALLOW list
# below, each entry with a reason (which W32 card absorbs it, or
# "vendored/numerical, keep"). It finds:
#   - duplicated functions: identical or near-identical bodies (whitespace,
#     comments and identifiers normalised) under the same or different
#     names, in different places;
#   - duplicated blocks: runs of >= BLOCK_MIN identical normalised lines
#     (whitespace/comments stripped, identifiers left alone), within or
#     across files;
#   - a struct/typedef named more than once;
#   - a function declared in more than one header.
# Usage: tools/dupcheck.py [--check] [core files...]
#   --check   exit 1 when anything outside ALLOW is reported
#             (tools/sourcecheck.sh)
# Heuristic and line-based, not a real C++ parser: it is meant to surface
# candidates for a human to allowlist or merge, not to be exact. Runs in a
# few seconds over core/, deterministic (no filesystem mtimes, no
# randomness).
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE = ROOT / "core"

# "file func|reason" (function/struct name as reported below) or
# "file *|reason" for a whole file's worth of a repeated pattern (used
# sparingly). Merge into the owning module named by the reason instead of
# adding an entry for new code.
ALLOW = """\

core/aniparse.cpp draw_ani_circ|keep: per-primitive-type dispatch wrappers (circle/filled circle/rect/filled rect/ellipse/filled ellipse), same shape by design; no W32a-d card owns aniparse.cpp yet
core/aniparse.cpp draw_ani_fcirc|keep: see core/aniparse.cpp draw_ani_circ
core/aniparse.cpp draw_ani_rect|keep: see core/aniparse.cpp draw_ani_circ
core/aniparse.cpp draw_ani_frect|keep: see core/aniparse.cpp draw_ani_circ
core/aniparse.cpp draw_ani_ellip|keep: see core/aniparse.cpp draw_ani_circ
core/aniparse.cpp draw_ani_fellip|keep: see core/aniparse.cpp draw_ani_circ
core/array_print.cpp ps_bar|W32b xpp_files: PostScript-export bar-drawing pair (linear vs log/hsb scale), same print-helper shape; merge with the array-export print helpers
core/array_print.cpp ps_hsb_bar|W32b xpp_files: see core/array_print.cpp ps_bar
core/autlib1.cpp fnuzae|vendored/numerical, keep: AUTO (Doedel), translated Fortran user-function stubs
core/autlib1.cpp fnuzbv|vendored/numerical, keep: AUTO (Doedel), translated Fortran user-function stubs
core/autlib2.cpp mynode|vendored/numerical, keep: AUTO's parallel-stub functions (mynode/numnodes/csend/crecv/...) for the non-MPI build, translated Fortran
core/autlib2.cpp numnodes|vendored/numerical, keep: see core/autlib2.cpp mynode
core/autlib3.cpp fnhd|vendored/numerical, keep: AUTO (Doedel), translated Fortran boundary-condition stubs (fnhd/fnhb/fnhw/fnsp/fnpe/fnpl/fnpd/fntr/fnbl/fnho)
core/autlib3.cpp fnhb|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib3.cpp fnhw|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib3.cpp fnsp|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib3.cpp fnpe|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib3.cpp fnpl|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib3.cpp fnpd|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib3.cpp fntr|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib3.cpp fnbl|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/autlib5.cpp fnho|vendored/numerical, keep: see core/autlib3.cpp fnhd
core/auto_nox.cpp auto_twopar_double|keep: two-parameter continuation dispatch pair (double/torus), same shape by design; no W32a-d card owns auto_nox.cpp yet
core/auto_nox.cpp auto_torus|keep: see core/auto_nox.cpp auto_twopar_double
core/auto_nox.cpp save_auto_file|a save/load mirror, not a copy: save_auto_file writes the four parts load_auto_file reads back in the same order (W32b looked: keep)
core/auto_nox.cpp load_auto_file|W32b xpp_files: see core/auto_nox.cpp save_auto_file
core/cvband.cpp CVBandSolve|vendored/numerical, keep: CVODE's band and dense linear solvers (cvband.cpp/cvdense.cpp), the same solve and free by the CVODE API's own design, each over its own matrix type (BandBacksolve/DenseBacksolve, BandFreeMat/DenseFreeMat); W33a looked: keep
core/cvdense.cpp CVDenseSolve|vendored/numerical, keep: see core/cvband.cpp CVBandSolve
core/cvband.cpp CVBandFree|vendored/numerical, keep: see core/cvband.cpp CVBandSolve
core/cvdense.cpp CVDenseFree|vendored/numerical, keep: see core/cvband.cpp CVBandSolve
core/conpar2.cpp time_start|vendored/numerical, keep: AUTO's parallel worker files (conpar2.cpp/worker2.cpp) share a timing helper pair by the AUTO source's own design
core/worker2.cpp time_start|vendored/numerical, keep: see core/conpar2.cpp time_start
core/conpar2.cpp time_end|vendored/numerical, keep: see core/conpar2.cpp time_start
core/worker2.cpp time_end|vendored/numerical, keep: see core/conpar2.cpp time_start
core/dormpri.cpp hinit|vendored/numerical, keep: Hairer's dop853/dopri5, two integrators of parallel structure (hinit/hinit5) by the original source's own design
core/dormpri.cpp hinit5|vendored/numerical, keep: see core/dormpri.cpp hinit
core/extra.cpp set_dll_library|keep: dlopen/dlsym pair for a plugin's library vs. function lookup, same shape by design; no W32a-d card owns extra.cpp yet
core/extra.cpp set_dll_function|keep: see core/extra.cpp set_dll_library
core/flags.cpp one_flag_step_symp|keep: per-integration-method single-step dispatch (symplectic/euler/discrete/heun/rk4), same shape by design; no W32a-d card owns flags.cpp yet
core/flags.cpp one_flag_step_euler|keep: see core/flags.cpp one_flag_step_symp
core/flags.cpp one_flag_step_discrete|keep: see core/flags.cpp one_flag_step_symp
core/flags.cpp one_flag_step_heun|keep: see core/flags.cpp one_flag_step_symp
core/flags.cpp one_flag_step_rk4|keep: see core/flags.cpp one_flag_step_symp
core/graphics.cpp point|keep: pixel-primitive pairs behind the XppUi seam (point/bead, line/frect, point_abs/bead_abs), same shape by design; no W32a-d card owns graphics.cpp yet
core/graphics.cpp bead|keep: see core/graphics.cpp point
core/graphics.cpp line|keep: see core/graphics.cpp point
core/graphics.cpp frect|keep: see core/graphics.cpp point
core/graphics.cpp point_abs|keep: see core/graphics.cpp point
core/graphics.cpp bead_abs|keep: see core/graphics.cpp point
core/integrate.cpp range_item|keep: range_item/range_item2 look like a coincidental structural match (same small loop shape, different purpose); verify before merging, no W32a-d card owns integrate.cpp yet
core/integrate.cpp range_item2|keep: see core/integrate.cpp range_item
core/json_ani.cpp j_ani_show|same shape, not a duplicate: two-line functions each calling a different pair (flush/out, blank/axes, advance/arm, xpprc/options); nothing to merge (W32c), keep
core/json_windows.cpp j_reset_graphics|see core/json_ani.cpp j_ani_show
core/ui_json.cpp script_next|see core/json_ani.cpp j_ani_show
core/xpp_batch.cpp do_vis_env|see core/json_ani.cpp j_ani_show
core/lunch-new.cpp io_int|keep: fscanf-style int/double token readers over a plain FILE * (xpp_io.h's xpp::TokenReader is the owner going forward, per CLAUDE.md's Strings and I/O section); not yet moved, no new copy added
core/lunch-new.cpp io_double|keep: see core/lunch-new.cpp io_int
core/odesol2.cpp discrete|merged (W33a): the step loop they shared is odesol2.cpp's fixed_steps; what is left is each method naming its own two step functions
core/odesol2.cpp euler|keep: see core/odesol2.cpp discrete
core/xpp_io.cpp xpp_line_reader_open|keep: xpp_io.h's own two reader kinds (whole-line vs whitespace-token), open/attach pairs of the same shape by design (CLAUDE.md's Strings and I/O section); not a copy to merge
core/xpp_io.cpp xpp_token_reader_open|keep: see core/xpp_io.cpp xpp_line_reader_open
core/xpp_io.cpp xpp_line_reader_attach|keep: see core/xpp_io.cpp xpp_line_reader_open
core/xpp_io.cpp xpp_token_reader_attach|keep: see core/xpp_io.cpp xpp_line_reader_open
core/xpp_io.h close|keep: xpp::LineReader/TokenReader/Writer's RAII close()/abort() methods, same one-line "if open, release" shape by design; not a copy to merge
core/xpp_io.h abort|keep: see core/xpp_io.h close

core/autlib1.cpp *block*|vendored/numerical, keep: AUTO (Doedel), translated Fortran; its long repeated per-branch-type blocks are the algorithm's own structure
core/autlib3.cpp *block*|vendored/numerical, keep: see core/autlib1.cpp *block*
core/autlib5.cpp *block*|vendored/numerical, keep: see core/autlib1.cpp *block*
core/conpar2.cpp *block*|vendored/numerical, keep: AUTO's parallel worker files share their startup block by the AUTO source's own design
core/worker2.cpp *block*|vendored/numerical, keep: see core/conpar2.cpp *block*
core/cvband.cpp *block*|vendored/numerical, keep: CVODE's band/dense solvers, translated C of parallel structure by the CVODE API's own design
core/cvdense.cpp *block*|vendored/numerical, keep: see core/cvband.cpp *block*
core/dormpri.cpp *block*|vendored/numerical, keep: Hairer's dop853/dopri5, two integrators of parallel structure by the original source's own design
core/dormpri.h *block*|vendored/numerical, keep: see core/dormpri.cpp *block*
core/eispack.cpp *block*|vendored/numerical, keep: EISPACK, translated Fortran eigenvalue routines
core/integrate.cpp *block*|keep: integrate.cpp's two similar range-stepping loops (17 lines); no W32a-d card owns integrate.cpp yet
core/nullcline.cpp *block*|keep: nullcline.cpp's two nullcline-branch blocks (16 lines); no W32a-d card owns nullcline.cpp yet



"""

# The tool compares normalised text, so it cannot see two implementations
# of the same algorithm written differently. The W30 audit found by hand
# three LU solves and two eigenvalue routines; W32a moved LINPACK's
# sgefa/sgesl, the banded bandfac/bandsol and the equilibria's eigenvalues
# into core/xpp_math.cpp and left AUTO's complete-pivoting ge() and
# EISPACK's hqr (autlib1.cpp's eig()) where they are, since merging either
# would change results (core/xpp_math.h says why). Nothing above
# allowlists them: no finding would ever match such an entry.


BLOCK_MIN = 16  # duplicated-block threshold, in normalised lines

KEYWORDS = {
    "if", "else", "for", "while", "do", "switch", "case", "default",
    "break", "continue", "return", "goto", "sizeof", "typedef", "struct",
    "union", "enum", "class", "namespace", "template", "typename",
    "public", "private", "protected", "virtual", "friend", "operator",
    "new", "delete", "this", "static", "const", "volatile", "extern",
    "inline", "void", "int", "char", "float", "double", "long", "short",
    "unsigned", "signed", "bool", "true", "false", "nullptr", "NULL",
    "auto", "using", "throw", "try", "catch", "noexcept", "explicit",
    "constexpr", "decltype", "mutable", "register", "restrict",
    "FILE", "std", "xpp", "const_cast", "static_cast",
    "dynamic_cast", "reinterpret_cast",
}

CONTROL_KW = {
    "if", "for", "while", "switch", "catch", "else", "do", "namespace",
    "struct", "class", "union", "enum", "extern", "typedef",
}

C_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)
CPP_COMMENT_RE = re.compile(r"//.*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')
CHAR_RE = re.compile(r"'(?:\\.|[^'\\])*'")
IDENT_RE = re.compile(r"\b[A-Za-z_]\w*\b")
NUMBER_RE = re.compile(r"\b\d[\d.eEfFuUlLxX]*\b")


def strip_comments(text: str) -> str:
    text = C_COMMENT_RE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    text = CPP_COMMENT_RE.sub("", text)
    return text


def mask_literals(text: str) -> str:
    text = STRING_RE.sub('"S"', text)
    text = CHAR_RE.sub("'c'", text)
    return text


def normalise_line(line: str) -> str:
    return " ".join(line.split())


def core_files(extra: list[str]) -> list[Path]:
    if extra:
        return [Path(p) for p in extra]
    return sorted(CORE.glob("*.cpp")) + sorted(CORE.glob("*.h"))


# ---------------------------------------------------------------------
# Function extraction: a line-based brace-depth walk. At depth 0, a run of
# lines up to a '{' that opens depth 1 is a candidate signature; if it
# looks like "name(...)" (not a control keyword, not a struct/class/
# namespace opening, parens present) it is treated as a function
# definition and its body captured up to the matching '}'.

FUNC_HEAD_RE = re.compile(
    r"([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\([^;{}]*\)\s*"
    r"(?:const\s*)?(?:noexcept\b[^{;]*)?(?:override\s*)?$"
)


def looks_like_function(head: str) -> tuple[bool, str]:
    h = head.strip()
    if not h or h.endswith(";"):
        return False, ""
    first_word = re.match(r"[A-Za-z_]\w*", h)
    if first_word and first_word.group(0) in CONTROL_KW:
        return False, ""
    if "(" not in h or ")" not in h:
        return False, ""
    m = FUNC_HEAD_RE.search(h)
    if not m:
        return False, ""
    name = m.group(1).split("::")[-1]
    if name in CONTROL_KW or name in ("sizeof",):
        return False, ""
    return True, name


def extract_functions(path: Path, raw_lines: list[str]):
    """Yield (name, start_idx, end_idx) 0-based, inclusive, of function bodies."""
    n = len(raw_lines)
    i = 0
    while i < n:
        # gather a candidate signature: lines from i until a top-level
        # '{' opens (local_depth reaches 1) or a ';' ends it at depth 0
        local_depth = 0
        j = i
        found_open = False
        seen_semicolon = False
        while j < n:
            l = raw_lines[j]
            local_depth += l.count("{") - l.count("}")
            if "{" in l and local_depth >= 1:
                found_open = True
                break
            if ";" in l and local_depth <= 0:
                seen_semicolon = True
                break
            if local_depth < 0:
                break
            j += 1
            if j - i > 60:  # runaway guard
                break
        if not found_open or seen_semicolon:
            i = j + 1
            continue
        sig_text = " ".join(raw_lines[i : j + 1])
        before_brace = sig_text.rsplit("{", 1)[0]
        ok, name = looks_like_function(before_brace)
        if not ok:
            i = j + 1
            continue
        # capture the body until local_depth returns to 0
        depth = local_depth
        k = j
        while depth > 0 and k + 1 < n:
            k += 1
            depth += raw_lines[k].count("{") - raw_lines[k].count("}")
        yield name, i, k
        i = k + 1


# dropped rather than kept literal when comparing bodies for near-identical
# duplication: a type qualifier that does not change what the code does
# (a local "const std::string s = ..." vs "std::string s = ..." is still
# the same helper)
DROP_QUALIFIERS = {"const", "volatile", "static", "inline", "register"}


def structural_tokens(body: list[str]) -> list[str]:
    """Normalise a body for near-identical comparison: identifiers other
    than keywords become placeholders in first-seen order, numbers and
    strings/chars are already masked by the caller, and harmless
    qualifiers are dropped."""
    mapping: dict[str, str] = {}

    def repl(m: re.Match) -> str:
        w = m.group(0)
        if w in DROP_QUALIFIERS:
            return ""
        if w in KEYWORDS:
            return w
        if w not in mapping:
            mapping[w] = f"V{len(mapping)}"
        return mapping[w]

    out = []
    for line in body:
        line = NUMBER_RE.sub("N", line)
        line = IDENT_RE.sub(repl, line)
        out.append(normalise_line(line))
    return out


def find_duplicated_functions(files: list[Path]):
    funcs = []  # (file, name, start_line(1-based), end_line, exact_lines)
    for path in files:
        if path.suffix not in (".cpp", ".h"):
            continue
        raw = path.read_text(encoding="utf-8", errors="replace")
        stripped = strip_comments(raw)
        stripped = mask_literals(stripped)
        lines = stripped.split("\n")
        for name, s, e in extract_functions(path, lines):
            # compare only the statements inside the braces, not the
            # signature (return type, qualifiers and parameter names
            # legitimately differ between near-identical functions)
            brace_at = s
            for idx in range(s, e + 1):
                if "{" in lines[idx]:
                    brace_at = idx
                    break
            body = lines[brace_at : e + 1]
            norm = [normalise_line(x) for x in body if normalise_line(x)]
            if len(norm) < 4:
                continue
            funcs.append((path, name, s + 1, e + 1, norm))

    groups: dict[tuple, list[tuple]] = {}
    for path, name, s, e, norm in funcs:
        key = tuple(structural_tokens(norm))
        groups.setdefault(key, []).append((path, name, s, e))

    dups = []
    for key, members in groups.items():
        if len(members) < 2:
            continue
        # skip a group that is really one function seen once (shouldn't
        # happen) and require it not all be the same file+name (recursive
        # extraction artefact)
        locs = {(str(p), nm) for p, nm, _, _ in members}
        if len(locs) < 2:
            continue
        dups.append(members)
    dups.sort(key=lambda m: (str(m[0][0]), m[0][2]))
    return dups


def find_duplicated_blocks(files: list[Path]):
    # per-file list of (lineno, normalised) after stripping comments,
    # dropping blank/near-empty lines (len < 4) to cut noise from lone
    # braces and similar boilerplate
    file_lines: dict[Path, list[tuple[int, str]]] = {}
    for path in files:
        raw = path.read_text(encoding="utf-8", errors="replace")
        stripped = strip_comments(raw)
        entries = []
        for i, line in enumerate(stripped.split("\n"), start=1):
            norm = normalise_line(line)
            if len(norm) >= 4:
                entries.append((i, norm))
        file_lines[path] = entries

    # hash of BLOCK_MIN-line windows -> list of (file, index-into-entries)
    windows: dict[tuple, list[tuple[Path, int]]] = {}
    for path, entries in file_lines.items():
        texts = [t for _, t in entries]
        for idx in range(len(texts) - BLOCK_MIN + 1):
            key = tuple(texts[idx : idx + BLOCK_MIN])
            windows.setdefault(key, []).append((path, idx))

    # keep windows seen at >= 2 distinct (file, region) locations, merge
    # overlaps and adjacent windows into maximal runs
    seeds = [(k, v) for k, v in windows.items() if len(v) >= 2]
    # extend each seed's matches into maximal equal runs pairwise, but to
    # stay simple: pick each occurrence, grow it forward while it keeps
    # matching every other occurrence in lockstep; report runs keyed by
    # their starting texts to dedupe overlapping windows of one run.
    reported: set[tuple] = set()
    blocks = []
    for key, occ in seeds:
        if key in reported:
            continue
        reported.add(key)
        # grow this run forward as long as ALL occurrences agree
        length = BLOCK_MIN
        while True:
            nxt = []
            ok = True
            for path, idx in occ:
                texts = [t for _, t in file_lines[path]]
                if idx + length >= len(texts):
                    ok = False
                    break
                nxt.append(texts[idx + length])
            if not ok or len(set(nxt)) != 1:
                break
            length += 1
        locs = []
        for path, idx in occ:
            entries = file_lines[path]
            start_line = entries[idx][0]
            end_line = entries[idx + length - 1][0]
            locs.append((path, start_line, end_line))
        # a "run" degenerates if all locations are the same file/lines
        distinct = {(str(p), s) for p, s, _ in locs}
        if len(distinct) < 2:
            continue
        blocks.append((length, locs))

    # drop a block fully nested (same file, overlapping range) inside a
    # longer one already reported, and dedupe by rounded location set
    blocks.sort(key=lambda b: -b[0])
    kept = []
    seen_loc_sets = set()
    for length, locs in blocks:
        loc_key = frozenset((str(p), s) for p, s, _ in locs)
        if loc_key in seen_loc_sets:
            continue
        contained = False
        for _, klocs in kept:
            kset = {(str(p), s, e) for p, s, e in klocs}
            here = {(str(p), s, e) for p, s, e in locs}
            if here and all(
                any(str(p) == str(kp) and ks <= s and e <= ke for kp, ks, ke in kset)
                for p, s, e in here
            ):
                contained = True
                break
        if contained:
            continue
        seen_loc_sets.add(loc_key)
        kept.append((length, locs))
    kept.sort(key=lambda b: (str(b[1][0][0]), b[1][0][1]))
    return kept


TYPEDEF_STRUCT_RE = re.compile(
    r"\}\s*([A-Za-z_]\w*)\s*;\s*$"
)
STRUCT_DEF_RE = re.compile(
    r"^\s*(?:typedef\s+)?struct\s+([A-Za-z_]\w*)\s*\{"
)


def find_duplicated_structs(files: list[Path]):
    names: dict[str, list[tuple[Path, int]]] = {}
    for path in files:
        raw = path.read_text(encoding="utf-8", errors="replace")
        stripped = strip_comments(raw)
        lines = stripped.split("\n")
        depth = 0
        struct_start_depth = None
        for i, line in enumerate(lines, start=1):
            if struct_start_depth is None:
                m = STRUCT_DEF_RE.match(line)
                if m:
                    struct_start_depth = depth
                    start_name = m.group(1)
                else:
                    start_name = None
            m2 = TYPEDEF_STRUCT_RE.match(line.strip())
            if m2 and "{" not in line:
                nm = m2.group(1)
                names.setdefault(nm, []).append((path, i))
                struct_start_depth = None
            elif struct_start_depth is not None and depth == struct_start_depth and "}" in line and start_name:
                # struct Name { ... }; closing at same depth without a
                # trailing typedef alias name
                names.setdefault(start_name, []).append((path, i))
                struct_start_depth = None
            depth += line.count("{") - line.count("}")
    dups = {nm: locs for nm, locs in names.items() if len({str(p) for p, _ in locs}) > 1}
    return dups


HEADER_DECL_RE = re.compile(
    r"^\s*[A-Za-z_][\w:<>,\*\s]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*;\s*$"
)


def find_duplicated_header_decls(files: list[Path]):
    names: dict[str, list[Path]] = {}
    for path in files:
        if path.suffix != ".h":
            continue
        raw = path.read_text(encoding="utf-8", errors="replace")
        stripped = strip_comments(raw)
        for line in stripped.split("\n"):
            s = line.strip()
            if not s or s.startswith("#") or s.startswith("//"):
                continue
            first = re.match(r"[A-Za-z_]\w*", s)
            if first and first.group(0) in CONTROL_KW:
                continue
            m = HEADER_DECL_RE.match(s)
            if not m:
                continue
            name = m.group(1)
            if name in KEYWORDS:
                continue
            names.setdefault(name, []).append(path)
    dups = {}
    for name, paths in names.items():
        distinct = sorted({relpath(p) for p in paths})
        if len(distinct) > 1:
            dups[name] = distinct
    return dups


def load_allow():
    entries = {}
    for line in ALLOW.strip("\n").split("\n"):
        if not line.strip():
            continue
        loc, reason = line.split("|", 1)
        entries[loc.strip()] = reason.strip()
    return entries


def relpath(p: Path) -> str:
    try:
        return str(p.relative_to(ROOT)).replace("\\", "/")
    except ValueError:
        return str(p).replace("\\", "/")


def main(argv: list[str]) -> int:
    check = "--check" in argv
    extra = [a for a in argv if a != "--check"]
    files = core_files(extra)
    allow = load_allow()
    matched_allow: set[str] = set()

    report_lines = []
    unallowed = 0

    def check_allow(file_rel: str, key: str) -> bool:
        entry = f"{file_rel} {key}"
        if entry in allow:
            matched_allow.add(entry)
            return True
        whole = f"{file_rel} *"
        if whole in allow:
            matched_allow.add(whole)
            return True
        return False

    # duplicated functions
    dupfuncs = find_duplicated_functions(files)
    shown = []
    for members in dupfuncs:
        allowed_all = True
        for p, name, s, e in members:
            if not check_allow(relpath(p), name):
                allowed_all = False
        loc_str = ", ".join(f"{relpath(p)}:{s} {name}" for p, name, s, e in members)
        if not allowed_all:
            unallowed += 1
        shown.append((allowed_all, loc_str))
    if shown:
        report_lines.append(f"duplicated functions ({len(shown)}):")
        for allowed_all, s in shown:
            mark = "" if not allowed_all else " (allowed)"
            report_lines.append(f"  {s}{mark}")

    # duplicated blocks
    blocks = find_duplicated_blocks(files)
    if blocks:
        report_lines.append(f"duplicated blocks (>= {BLOCK_MIN} lines, {len(blocks)}):")
        for length, locs in blocks:
            allowed_all = True
            for p, s, e in locs:
                if not check_allow(relpath(p), "*block*"):
                    allowed_all = False
            if not allowed_all:
                unallowed += 1
            loc_str = ", ".join(f"{relpath(p)}:{s}-{e}" for p, s, e in locs)
            mark = " (allowed)" if allowed_all else ""
            report_lines.append(f"  {length} lines: {loc_str}{mark}")

    # struct/typedef duplicated
    structs = find_duplicated_structs(files)
    if structs:
        report_lines.append(f"struct/typedef defined more than once ({len(structs)}):")
        for name, locs in sorted(structs.items()):
            allowed_all = True
            for p, ln in locs:
                if not check_allow(relpath(p), name):
                    allowed_all = False
            if not allowed_all:
                unallowed += 1
            loc_str = ", ".join(f"{relpath(p)}:{ln}" for p, ln in locs)
            mark = " (allowed)" if allowed_all else ""
            report_lines.append(f"  {name}: {loc_str}{mark}")

    # function declared in more than one header
    hdecls = find_duplicated_header_decls(files)
    if hdecls:
        report_lines.append(f"function declared in more than one header ({len(hdecls)}):")
        for name, paths in sorted(hdecls.items()):
            allowed_all = True
            for p in paths:
                if not check_allow(p, name):
                    allowed_all = False
            if not allowed_all:
                unallowed += 1
            mark = " (allowed)" if allowed_all else ""
            report_lines.append(f"  {name}: {', '.join(paths)}{mark}")

    stale = sorted(set(allow) - matched_allow)

    print("\n".join(report_lines) if report_lines else "dupcheck: nothing found")
    if stale:
        print("stale ALLOW entries:")
        for s in stale:
            print(f"  {s}")

    total_bad = unallowed + len(stale)
    if total_bad > 0:
        print(f"dupcheck: {unallowed} unallowed, {len(stale)} stale ALLOW entries")
        if check:
            return 1
    else:
        print("dupcheck: everything found is in the allowlist")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
