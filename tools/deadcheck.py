#!/usr/bin/env python3
# Dead declarations in core/ (W46a), the companion of tools/deadcode.sh:
# that one asks the linker which functions and file-scope data nothing
# reaches; this one reads the source for the dead code a linker cannot
# see. It lists
#   - macro:    a #define nothing expands (nor tests with #ifdef), but one
#               a platform, compiler or third_party/ header reads;
#   - type:     a struct/class/union/enum tag, typedef or using alias
#               nothing names (a tag and its typedef alias are one type;
#               an enum is used when an enumerator is);
#   - field:    a struct member nothing names but its declaration, or, in a
#               struct with no member functions, nothing reaches by . or ->;
#   - written:  a member only ever assigned (x.f = ..., x->f++) and never
#               read, whatever struct the same name belongs to (a write
#               through it, *x.p = or x.p[i] =, reads it);
#   - nodef:    a function or extern declared, with no definition anywhere;
#   - redecl:   a function or extern declared in more than one header, or
#               in a header and again in a .cpp: it lives in its owner's
#               header only (CLAUDE.md, "Single source");
#   - ifdead:   #if 0, or #ifdef/#if defined() of a macro nothing defines
#               (not a platform's, the compiler's or the Makefile's), or of
#               a switch the file itself always defines above it (#define X
#               then #ifdef X: its #else is dead);
#   - comment:  commented-out code: a comment most of whose lines read as
#               statements (a comment that explains stays);
#   - header:   a core header nothing includes, or with nothing in it.
# Uses count across core/ and tests/, comments and string literals
# stripped. Heuristic and line-based like tools/dupcheck.py, not a C++
# parser: a name that is also a common word elsewhere reads as used, so
# it misses some; what it lists is dead or allowlisted below.
# Not here: unused functions and data (deadcode.sh); set-but-unread
# locals and parameters and a .cpp's unused macros (-Wunused-but-set-*,
# -Wunused-macros, errors under WERROR=1); includes a file does not need
# (a one-off at W46a, by compiling without each).
# Usage: tools/deadcheck.py [--check]   (a few seconds)
#   --check   exit 1 when anything outside ALLOW is listed, or an ALLOW
#             entry names nothing (tools/sourcecheck.sh)
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE = ROOT / "core"
TESTS = ROOT / "tests"

# "kind file name|reason": what stays although it reads as dead. Delete
# rather than add an entry.
ALLOW = """\
nodef core/xpp_http.cpp rand_s|the Windows C library's; its stdlib.h declares it only with _CRT_RAND_S
nodef core/xpp_assets.h web_assets|defined in the generated build/.../web_assets.cpp (tools/embed.cpp, W173)
nodef core/xpp_assets.h icon_png|defined in the generated build/.../icon_assets.cpp (tools/embed_bytes.cpp, W173)
nodef core/xpp_assets.h window_lib|defined in the generated build/.../window_lib.cpp (tools/embed_bytes.cpp, W173)
"""

# macros that come from outside core/: platforms, compilers, libraries,
# the Makefile's -D (read from it below)
EXTERNAL_MACROS = {
    "_WIN32", "_WIN64", "__APPLE__", "__linux__", "__unix__", "__MACH__",
    "__cplusplus", "__GNUC__", "__clang__", "_MSC_VER", "__MINGW32__",
    "__MINGW64__", "__has_include", "__GLIBC__", "__FreeBSD__",
    "__SANITIZE_ADDRESS__", "__has_feature", "NDEBUG", "_GNU_SOURCE",
    "__STDC_VERSION__", "__x86_64__", "__aarch64__", "__arm64__",
    "__EMSCRIPTEN__", "__cpp_lib_format", "__BYTE_ORDER__",
    "WEBVIEW_STATIC", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN",
    "__has_builtin", "__OPTIMIZE__", "__SIZEOF_POINTER__",
    "__cpp_lib_to_chars", "_DARWIN_C_SOURCE", "_POSIX_C_SOURCE",
}

IDENT_RE = re.compile(r"[A-Za-z_]\w*")
KEYWORDS = {
    "if", "else", "for", "while", "do", "switch", "case", "default",
    "break", "continue", "return", "goto", "sizeof", "typedef", "struct",
    "union", "enum", "class", "namespace", "template", "typename",
    "public", "private", "protected", "virtual", "friend", "operator",
    "new", "delete", "this", "static", "const", "volatile", "extern",
    "inline", "void", "int", "char", "float", "double", "long", "short",
    "unsigned", "signed", "bool", "true", "false", "nullptr", "auto",
    "using", "throw", "try", "catch", "noexcept", "explicit", "constexpr",
    "decltype", "mutable", "register", "restrict", "static_assert",
    "alignas", "constinit", "consteval", "override", "final",
}


# ---------------------------------------------------------------------
# Lexing: code with comments and literals blanked (newlines kept, so line
# numbers hold), and the comments: (line, text, a // comment, alone on
# its line).

def lex(text: str) -> tuple[str, list[tuple[int, str, bool, bool]]]:
    out: list[str] = []
    comments: list[tuple[int, str, bool, bool]] = []
    i, n, line = 0, len(text), 1
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            own = text[text.rfind("\n", 0, i) + 1:i].strip() == ""
            comments.append((line, text[i + 2:j], True, own))
            out.append(" ")
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j
            body = text[i + 2:j]
            own = text[text.rfind("\n", 0, i) + 1:i].strip() == ""
            comments.append((line, body, False, own))
            nl = body.count("\n")
            out.append(" " + "\n" * nl)
            line += nl
            i = j + 2
        elif c == "R" and i + 1 < n and text[i + 1] == '"' and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            p = text.find("(", i + 2)
            delim = text[i + 2:p]
            end = text.find(")" + delim + '"', p)
            end = n if end < 0 else end
            body = text[i:end + len(delim) + 2]
            nl = body.count("\n")
            out.append('""' + "\n" * nl)
            line += nl
            i = end + len(delim) + 2
        elif c in "\"'":
            if c == "'" and i > 0 and text[i - 1].isalnum() and text[i - 2:i].isdigit():
                out.append(c)  # a digit separator (1'000)
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
        else:
            if c == "\n":
                line += 1
            out.append(c)
            i += 1
    return "".join(out), comments


class Source:
    def __init__(self, path: Path):
        self.path = path
        self.rel = path.relative_to(ROOT).as_posix()
        self.raw = path.read_text(encoding="utf-8", errors="replace")
        self.code, self.comments = lex(self.raw)
        self.lines = self.code.split("\n")
        self.is_header = path.suffix == ".h"
        self.is_core = path.parent == CORE


def line_of(src: Source, pos: int) -> int:
    return src.code.count("\n", 0, pos) + 1


# ---------------------------------------------------------------------
# Scopes: walk the braces, telling a namespace (or extern "C") block and a
# type's body from a function's or an initializer's.

TYPE_OPEN_RE = re.compile(r"\b(struct|class|union|enum)\b(\s+class\b|\s+struct\b)?\s*(\w+)?\s*(?:final\s*)?(?::[^{;()]*)?$")
NS_OPEN_RE = re.compile(r"(\bnamespace\b\s*[\w:]*|\bextern\s*\"\")\s*$")


_statements: dict[str, list] = {}


def statements(src: Source):
    if src.rel not in _statements:
        _statements[src.rel] = list(walk(src))
    return _statements[src.rel]


def walk(src: Source):
    """Yield (kind, text, start, end, scope) for each statement ended by ';'
    and each block opener, where scope is the stack of enclosing kinds:
    'ns' (namespace/extern "C"), ('type', kind, name, its brace's
    offset), 'code' (a function's body, an initializer)."""
    code = src.code
    stack: list = []
    start = 0
    i = 0
    n = len(code)
    depth_paren = 0
    while i < n:
        c = code[i]
        if c == "#" and (i == 0 or code.rfind("\n", 0, i) == i - 1 or code[code.rfind("\n", 0, i) + 1:i].strip() == ""):
            # a preprocessor line, continuation lines included
            j = i
            while True:
                j = code.find("\n", j)
                if j < 0:
                    j = n
                    break
                if code[j - 1] == "\\":
                    j += 1
                    continue
                break
            yield ("pp", code[i:j], i, j, tuple(stack))
            if code[start:i].strip() == "":
                start = j
            i = j
            continue
        if c == "(":
            depth_paren += 1
        elif c == ")":
            depth_paren -= 1
        elif c == ";" and depth_paren == 0:
            yield ("stmt", code[start:i], start, i, tuple(stack))
            start = i + 1
        elif c == "{" and depth_paren == 0:
            head = code[start:i]
            h = " ".join(head.split())
            if NS_OPEN_RE.search(h):
                stack.append("ns")
                yield ("open-ns", head, start, i, tuple(stack[:-1]))
            else:
                m = TYPE_OPEN_RE.search(h)
                if m and "(" not in h and "=" not in h:
                    stack.append(("type", m.group(1), m.group(3) or "", i))
                    yield ("open-type", head, start, i, tuple(stack[:-1]))
                else:
                    stack.append("code")
                    yield ("open", head, start, i, tuple(stack[:-1]))
            start = i + 1
        elif c == "}" and depth_paren == 0:
            top = stack.pop() if stack else "ns"
            if isinstance(top, tuple):
                yield ("close-type", top, top[3], i, tuple(stack))
                # the statement goes on to its ';': a typedef's alias, a
                # variable of the type
                start = top_start(code, top[3])
            else:
                start = i + 1
        i += 1


def top_start(code: str, brace: int) -> int:
    # the statement a type body belongs to started after the last ';', '{'
    # or '}' before its opening brace (or a preprocessor line)
    j = brace - 1
    while j >= 0 and code[j] not in ";{}":
        if code[j] == "\n":
            k = code.rfind("\n", 0, j)
            if code[k + 1:j].lstrip().startswith("#"):
                return j + 1
        j -= 1
    return j + 1


def file_scope(scope) -> bool:
    return all(s == "ns" for s in scope)


# ---------------------------------------------------------------------

def load_sources() -> list[Source]:
    files = sorted(CORE.glob("*.cpp")) + sorted(CORE.glob("*.h"))
    files += sorted(TESTS.glob("*.c")) + sorted(TESTS.glob("*.cpp")) + sorted(TESTS.glob("*.h"))
    return [Source(p) for p in files]


def makefile_macros() -> set[str]:
    mk = (ROOT / "Makefile").read_text(encoding="utf-8", errors="replace")
    return set(re.findall(r"-D([A-Za-z_]\w*)", mk))


PP_DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)")
PP_UNDEF_RE = re.compile(r"^\s*#\s*undef\s+([A-Za-z_]\w*)")
PP_COND_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif)\b(.*)")
PP_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def find_macros(sources, words: Counter):
    # a macro's own #define and #undef lines do not use it
    own: Counter = Counter()
    for t in sources:
        for line in t.lines:
            m = PP_DEFINE_RE.match(line) or PP_UNDEF_RE.match(line)
            if m:
                own[m.group(1)] += 1
    outside = EXTERNAL_MACROS | third_party_words()
    found = []
    for s in sources:
        if not s.is_core:
            continue
        for ln, text in enumerate(s.lines, 1):
            m = PP_DEFINE_RE.match(text)
            if m and m.group(1) in outside:
                continue
            if m and words[m.group(1)] <= own[m.group(1)]:
                found.append(("macro", s.rel, m.group(1), ln))
    return found


_third_party: set[str] = set()


def third_party_words() -> set[str]:
    """Vendored identifiers: header configuration and APIs called from
    source ports (CORE-MATH and musl Bessel), outside the core scan."""
    if not _third_party:
        for p in (ROOT / "third_party").rglob("*"):
            if p.suffix not in {".h", ".hpp", ".c", ".cpp"}:
                continue
            _third_party.update(IDENT_RE.findall(p.read_text(encoding="utf-8", errors="replace")))
    return _third_party


def fixed_switches(sources) -> dict[str, tuple[str, int]]:
    """Macros defined once, empty, outside any #if, and never #undef'd:
    name -> (file, line). A later #ifdef of one in that file is always
    taken (its #else never), which is dead code too."""
    defs: dict[str, list] = {}
    undef: set[str] = set()
    for s in sources:
        depth = 0
        for ln, text in enumerate(s.lines, 1):
            if re.match(r"\s*#\s*if", text):
                depth += 1
            elif re.match(r"\s*#\s*endif", text):
                depth -= 1
            m = PP_UNDEF_RE.match(text)
            if m:
                undef.add(m.group(1))
            m = re.match(r"\s*#\s*define\s+(\w+)\s*$", text)
            if m:
                defs.setdefault(m.group(1), []).append((s.rel, ln, depth))
            elif PP_DEFINE_RE.match(text):
                defs.setdefault(PP_DEFINE_RE.match(text).group(1), []).append((s.rel, ln, -1))
    return {n: (d[0][0], d[0][1]) for n, d in defs.items()
            if len(d) == 1 and d[0][2] == 0 and n not in undef}


def find_conditionals(sources, defined: set[str]):
    fixed = fixed_switches(sources)
    found = []
    for s in sources:
        if not s.is_core:
            continue
        for ln, text in enumerate(s.lines, 1):
            m = PP_COND_RE.match(text)
            if not m:
                continue
            kw, rest = m.group(1), m.group(2)
            if kw in ("if", "elif") and re.fullmatch(r"\s*0\s*", rest):
                found.append(("ifdead", s.rel, "#if 0", ln))
                continue
            for name in IDENT_RE.findall(rest):
                if name in fixed and fixed[name][0] == s.rel and fixed[name][1] < ln:
                    found.append(("ifdead", s.rel, name + " (always defined)", ln))
                    continue
                if (name in ("defined",) or name in defined or name in EXTERNAL_MACROS
                        or name in third_party_words()):
                    continue
                if kw in ("if", "elif") and not re.search(r"defined\s*\(?\s*" + name, rest):
                    continue  # a value compared, not a presence
                found.append(("ifdead", s.rel, name, ln))
    return found


def type_defs(s: Source):
    """(names, line, enumerators, declares a variable, own text) of every
    named type: a tag and its typedef aliases are one type."""
    out = []
    for kind, text, start, end, scope in statements(s):
        if kind != "stmt":
            continue
        h = " ".join(text.split())
        ln = line_of(s, start + len(text) - len(text.lstrip()))
        if "{" in h and "}" in h:
            prefix, body = h.split("{", 1)
            body, suffix = body.rsplit("}", 1)
            m = TYPE_OPEN_RE.search(prefix)
            if not m or "(" in prefix or "=" in prefix:
                continue
            names = [m.group(3)] if m.group(3) else []
            ids = [w for part in split_top(re.sub(r"\[[^\]]*\]", "", suffix))
                   for w in IDENT_RE.findall(part)[-1:] if w not in KEYWORDS]
            typedef = re.search(r"\btypedef\b", prefix) is not None
            if typedef:
                names += ids
            enumerators = []
            if m.group(1) == "enum":
                enumerators = [IDENT_RE.match(p.strip()).group(0) for p in split_top(body)
                               if IDENT_RE.match(p.strip())]
            if names:
                out.append((names, ln, enumerators, bool(ids) and not typedef, text))
        elif re.match(r"(?:\w+\s+)*typedef\b", h):
            # the alias: the name inside (*name) for a pointer to a
            # function, else the last identifier of each declarator
            m = re.search(r"\(\s*\*\s*(\w+)\s*\)\s*\(", h)
            if m:
                out.append(([m.group(1)], ln, [], False, text))
            else:
                parts = split_top(re.sub(r"\[[^\]]*\]", "", h))
                names = [IDENT_RE.findall(p)[-1] for p in parts if IDENT_RE.findall(p)]
                out.append((names[-len(parts):], ln, [], False, text))
        else:
            m = re.match(r"using\s+(\w+)\s*=", h)
            if m:
                out.append(([m.group(1)], ln, [], False, text))
    return out


def find_types(sources, words: Counter):
    found = []
    for s in sources:
        if not s.is_core:
            continue
        for names, ln, enumerators, declares_var, text in type_defs(s):
            if declares_var or any(words[e] > 1 for e in enumerators):
                continue
            own = Counter(IDENT_RE.findall(text))
            if any(words[n] > own[n] + forward_decls(sources, n) for n in names):
                continue
            found.append(("type", s.rel, "/".join(names), ln))
    return found


_fwd: Counter = Counter()


def forward_decls(sources, name: str) -> int:
    """How often `struct name;` (a forward declaration, not a use)."""
    if not _fwd:
        for s in sources:
            _fwd.update(re.findall(r"\b(?:struct|class|union)\s+(\w+)\s*;", s.code))
        _fwd[""] = 1  # counted
    return _fwd[name]


def split_top(text: str, sep: str = ",") -> list[str]:
    parts, depth, cur = [], 0, []
    for c in text:
        if c in "([{<":
            depth += 1
        elif c in ")]}>":
            depth -= 1
        if c == sep and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    parts.append("".join(cur))
    return parts


def member_decls(s: Source):
    """(type name, member, line, the type has member functions) of each
    data member."""
    methods = set()
    for kind, text, start, end, scope in statements(s):
        if scope and isinstance(scope[-1], tuple) and (kind == "open" or kind == "stmt" and "(" in text.split("=")[0] and not re.search(r"\(\s*\*", text)):
            methods.add(scope[-1][3])
    out = []
    for kind, text, start, end, scope in statements(s):
        if kind != "stmt" or not scope or not isinstance(scope[-1], tuple):
            continue
        tkind, tname = scope[-1][1], scope[-1][2]
        has_methods = scope[-1][3] in methods
        if tkind == "enum":
            continue
        h = " ".join(text.split())
        h = re.sub(r"^(public|private|protected)\s*:\s*", "", h)
        if not h or re.match(r"(using|typedef|static_assert|friend|template)\b", h):
            continue
        m = re.search(r"\(\s*\*\s*(\w+)\s*\)\s*\(", h)
        if m:
            out.append((tname, m.group(1), line_of(s, start + len(text) - len(text.lstrip())), has_methods))
            continue
        if "}" in h:
            h = h.rsplit("}", 1)[1]  # a nested type's members are its own
        if "(" in h.split("=")[0].split("{")[0]:
            continue  # a member function
        h = re.split(r"=|\{", h, maxsplit=1)[0]
        for part in split_top(h):
            part = re.sub(r"\[[^\]]*\]", "", part)
            part = re.sub(r":\s*\d+\s*$", "", part)
            ids = [w for w in IDENT_RE.findall(part) if w not in KEYWORDS]
            if ids:
                out.append((tname, ids[-1], line_of(s, end), has_methods))
    return out


ACCESS_RE = re.compile(r"(?:\.|->)\s*([A-Za-z_]\w*)(?=((?:\s*\[[^\]\n]*\])*)\s*(\S\S?)?)")
WRITE_RE = re.compile(r"=[^=]|[-+*/|&^]=|\+\+|--|<<=|>>=")


def find_fields(sources, words: Counter):
    # every member access, by name: what follows it says a write
    access: dict[str, list[str]] = {}
    for s in sources:
        for m in ACCESS_RE.finditer(s.code):
            after = (m.group(3) or "") + " "
            # *x.p = ..., x.p[i] = ...: a write through the member, which
            # reads it (a pointer); &x.f: its address, read through
            j = m.start() - 1
            while j >= 0 and (s.code[j].isalnum() or s.code[j] in "_.->[] "):
                j -= 1
            if m.group(2) or j >= 0 and s.code[j] in "*&":
                after = "read "
            access.setdefault(m.group(1), []).append(after)
    found = []
    for s in sources:
        if not s.is_core:
            continue
        for tname, name, ln, has_methods in member_decls(s):
            acc = access.get(name, [])
            # named nowhere else; or, in a plain struct, never after . or ->
            if words[name] <= 1 or not acc and not has_methods:
                found.append(("field", s.rel, f"{tname}.{name}", ln))
                continue
            # every access, whatever its struct, a write: nothing reads it
            if acc and not has_methods and all(WRITE_RE.match(a) for a in acc):
                found.append(("written", s.rel, f"{tname}.{name}", ln))
    return found


PROTO_RE = re.compile(r"(?:^|[\s*&])([A-Za-z_]\w*)\s*\(([^()]|\([^()]*\))*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->\s*[\w:<>\s*&]+)?(?:__attribute__\s*\(\(.*\)\))?\s*$")


LITERAL_ARGS_RE = re.compile(r"(\w+)\s*\(\s*[-\d\"'{]")
FUNCTION_MACROS: set[str] = set()


def declarations(s: Source):
    """(name, kind, line) of every file-scope function prototype ('func')
    and extern variable ('var'), definitions not included."""
    out = []
    for kind, text, start, end, scope in statements(s):
        if kind != "stmt" or not file_scope(scope):
            continue
        h = " ".join(text.split())
        h = re.sub(r'^extern\s*""\s*', "extern ", h)
        if not h or h.startswith("#") or re.match(r"(typedef|using|static_assert|template|namespace|return)\b", h):
            continue
        if re.search(r"\b(struct|class|union|enum)\b[^(]*\{", h):
            continue
        if "=" in h.split("(")[0]:
            continue
        m = PROTO_RE.search(h)
        if m and not re.search(r"\(\s*\*", h):
            name = m.group(1)
            if name in KEYWORDS or name == "__attribute__" or name in FUNCTION_MACROS:
                continue
            if LITERAL_ARGS_RE.search(h):
                continue  # T name(1): a variable constructed
            out.append((name, "func", line_of(s, start + len(text) - len(text.lstrip()))))
        elif h.startswith("extern ") and "(" not in h:
            for part in split_top(h[len("extern "):]):
                part = re.sub(r"\[[^\]]*\]", "", part)
                ids = [w for w in IDENT_RE.findall(part) if w not in KEYWORDS]
                if ids:
                    out.append((ids[-1], "var", line_of(s, start + len(text) - len(text.lstrip()))))
    return out


def definitions(sources) -> dict[str, set[str]]:
    """Names with a body somewhere: name(...) [qualifiers] {, and file-scope
    variables declared without extern: name -> the files defining it."""
    names: dict[str, set[str]] = {}
    for s in sources:
        for kind, text, start, end, scope in statements(s):
            if kind not in ("open", "stmt"):
                continue
            h = " ".join(text.split())
            if kind == "open":
                m = re.search(r"([A-Za-z_]\w*)\s*\(([^()]|\([^()]*\))*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:->\s*[^{]*)?(?::[^{]*)?$", h)
                if m:
                    names.setdefault(m.group(1), set()).add(s.rel)
                # a variable with an initializer list: T name[] = {
                m = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*=\s*$", h)
                if m and file_scope(scope):
                    names.setdefault(m.group(1), set()).add(s.rel)
                m = re.search(r"([A-Za-z_]\w*)\s*$", h)
                if m and file_scope(scope) and not re.search(r"\(", h):
                    names.setdefault(m.group(1), set()).add(s.rel)  # T name{...}
            elif kind == "stmt" and file_scope(scope):
                h2 = re.sub(r'^extern\s*""\s*', "", h)
                if h2.startswith("extern ") or re.match(r"(typedef|using)\b", h2):
                    continue
                if "(" in h2.split("=")[0]:
                    continue
                for part in split_top(h2.split("=")[0] if "=" in h2 else h2):
                    part = re.sub(r"\[[^\]]*\]", "", part)
                    ids = [w for w in IDENT_RE.findall(part) if w not in KEYWORDS]
                    if ids:
                        names.setdefault(ids[-1], set()).add(s.rel)
                if "=" in h2:
                    for part in split_top(h2):
                        lhs = part.split("=")[0]
                        lhs = re.sub(r"\[[^\]]*\]", "", lhs)
                        ids = [w for w in IDENT_RE.findall(lhs) if w not in KEYWORDS]
                        if ids:
                            names.setdefault(ids[-1], set()).add(s.rel)
    return names


def find_declarations(sources, words: Counter):
    found = []
    defined = definitions(sources)
    where: dict[str, list[tuple[Source, int, str]]] = {}
    for s in sources:
        if not s.is_core:
            continue
        for name, kind, ln in declarations(s):
            where.setdefault(name, []).append((s, ln, kind))
    for name, locs in sorted(where.items()):
        if name not in defined:
            for s, ln, kind in locs:
                found.append(("nodef", s.rel, name, ln))
        headers = sorted({s.rel for s, _, _ in locs if s.is_header})
        if len(headers) > 1:
            for s, ln, kind in locs:
                if s.is_header:
                    found.append(("redecl", s.rel, name, ln))
        if headers:
            for s, ln, kind in locs:
                if not s.is_header:
                    found.append(("redecl", s.rel, name, ln))
    return found


# a comment line that reads as a statement
CODE_LINE_RES = [re.compile(p) for p in [
    r"^[A-Za-z_][\w.\[\]>*()-]*\s*(?:[-+*/|&]?=|\+\+|--)\s*[^;]*;\s*(?://.*)?$",  # x = y;
    r"^[A-Za-z_][\w.>:-]*\s*\(.*\)\s*;\s*$",                                      # f(...);
    r"^(?:if|for|while|switch)\s*\(.*\)\s*[{;]?.*$",
    r"^(?:return\b[^.]*|break|continue);\s*$",
    r"^(?:[{}];?|\}\s*else\s*\{?|else\s*\{?|else)$",
    r"^(?:static\s+|extern\s+|const\s+)*(?:int|double|float|char|long|void|FILE|integer|doublereal|unsigned|short)\b[\w\s*,\[\]=.()+-]*[;{]\s*$",
    r"^#\s*(?:include|define|if|ifdef|ifndef|endif|else)\b",
    r"^case\s+[\w']+\s*:.*$",
    r"^(?:printf|fprintf|plintf|sprintf|xpp_log|xpp::log)\s*\(",
    r"^[A-Za-z_][\w.>-]*\s*\([^;]*\)\s*\{\s*$",                                   # a definition's head
    r"^[\w\s,()*]*\);\s*$",                                                        # a call's last line
]]


def comment_blocks(s: Source):
    """(line, text) of each comment, a run of // comments alone on
    consecutive lines one block."""
    blocks: list[list] = []
    for ln, text, is_line, own in s.comments:
        if is_line and own and blocks and blocks[-1][2] and blocks[-1][3] == ln - 1:
            blocks[-1][1] += "\n" + text
            blocks[-1][3] = ln
        else:
            blocks.append([ln, text, is_line and own, ln + text.count("\n")])
    return [(b[0], b[1]) for b in blocks]


def find_commented_code(sources):
    """A comment most of whose lines (60%) read as statements is code."""
    found = []
    for s in sources:
        if not s.is_core:
            continue
        for ln, text in comment_blocks(s):
            lines = []
            for l in text.split("\n"):
                t = re.sub(r"^\*+(?!/)", "", l.strip()).strip()
                if t:
                    lines.append(t)
            code = sum(1 for t in lines if any(rx.match(t) for rx in CODE_LINE_RES))
            if code and code * 10 >= len(lines) * 6:
                found.append(("comment", s.rel, " ".join(lines[0].split())[:60], ln))
    return found


def find_headers(sources):
    found = []
    included: set[str] = set()
    for p in list(ROOT.glob("core/*")) + list(ROOT.glob("tests/*")) + list(ROOT.glob("tools/*.c")):
        if p.suffix not in (".c", ".cpp", ".h"):
            continue
        for line in p.read_text(encoding="utf-8", errors="replace").split("\n"):
            m = PP_INCLUDE_RE.match(line)
            if m:
                included.add(Path(m.group(1)).name)
    for s in sources:
        if not (s.is_core and s.is_header):
            continue
        if s.path.name not in included:
            found.append(("header", s.rel, s.path.name, 1))
            continue
        # nothing in it but its guard (and extern "C" braces)
        body = " ".join(l for l in s.lines if not l.strip().startswith("#"))
        body = re.sub(r'extern\s*""\s*\{|\}', "", body).strip()
        guard = next((m.group(1) for l in s.lines for m in [re.match(r"\s*#\s*ifndef\s+(\w+)", l)] if m), None)
        defines = sum(1 for l in s.lines if PP_DEFINE_RE.match(l) and PP_DEFINE_RE.match(l).group(1) != guard)
        includes = sum(1 for l in s.lines if re.match(r"\s*#\s*include", l))
        if not body and not defines and not includes:
            found.append(("header", s.rel, s.path.name + " (empty)", 1))
    return found


def load_allow() -> dict[str, str]:
    allow = {}
    for line in ALLOW.strip("\n").split("\n"):
        if not line.strip():
            continue
        key, _, reason = line.partition("|")
        allow[" ".join(key.split())] = reason
    return allow


def main(argv: list[str]) -> int:
    check = "--check" in argv
    sources = load_sources()
    words: Counter = Counter()
    for s in sources:
        words.update(IDENT_RE.findall(s.code))
    defined_macros = makefile_macros()
    for s in sources:
        for line in s.lines:
            m = PP_DEFINE_RE.match(line)
            if m:
                defined_macros.add(m.group(1))
                if re.match(r"\s*#\s*define\s+\w+\(", line):
                    FUNCTION_MACROS.add(m.group(1))

    found = []
    found += find_macros(sources, words)
    found += find_types(sources, words)
    found += find_fields(sources, words)
    found += find_declarations(sources, words)
    found += find_conditionals(sources, defined_macros)
    found += find_commented_code(sources)
    found += find_headers(sources)

    allow = load_allow()
    used = set()
    bad = 0
    for kind, rel, name, ln in found:
        key = f"{kind} {rel} {name}"
        if key in allow:
            used.add(key)
            continue
        bad += 1
        print(f"{kind:8} {rel}:{ln} {name}")
    for key in allow:
        if key not in used:
            bad += 1
            print(f"stale ALLOW entry: {key}")
    if bad:
        print(f"deadcheck: {bad} dead or stale")
        return 1 if check else 0
    print("deadcheck: nothing dead beyond the allowlist")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
