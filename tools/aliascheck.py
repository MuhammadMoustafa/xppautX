#!/usr/bin/env python3
"""One thing, one name (W113, maintainer 2026-10-01). Fails a second name
for one of our own names in core/ and tests/: `using X = <type>;`,
`namespace a = b;`, `typedef <type> X;`, `using xpp::name;` inside
namespace xpp, and a #define that only renames an identifier. Comments
and string literals are stripped first. Allowed without an entry: an
alias of a std:: type, a function-pointer type (a new type, not a second
name), `typedef struct/enum/union {...} name;` (a definition), and a
typedef of a builtin type. Anything else is in ALLOW below, each entry
with its reason: use the real name rather than add an entry.
Usage: tools/aliascheck.py [--check]   (exit 1 on a finding)"""
import re, sys, glob, os

# (file basename, alias name) -> reason
ALLOW = {
    ("xpp_files_internal.h", "Stat"): "platform shim: struct stat vs _stat64",
    ("xpp_http.cpp", "sock_t"): "platform shim: SOCKET vs int",
    ("xpp_http.cpp", "close_sock"): "platform shim: closesocket vs close",
    ("xpp_http.cpp", "dup"): "platform shim: Windows _dup",
    ("xpp_http.cpp", "dup2"): "platform shim: Windows _dup2",
    ("xpp_http.cpp", "write"): "platform shim: Windows _write",
    ("xpp_http.cpp", "read"): "platform shim: Windows _read",
    ("cvode.cpp", "H_BIAS"): "vendored CVODE constant",
    ("auto_f2c.h", "doublecomplex"): "vendored f2c: single-line struct definition",
    ("auto_f2c.h", "logical"): "vendored f2c typedef",
    ("cvode.cpp", "HMIN_DEFAULT"): "vendored CVODE default constant",
    ("cvode.cpp", "HMAX_INV_DEFAULT"): "vendored CVODE default constant",
    ("cvode.cpp", "RDIV"): "vendored CVODE default constant",
    ("cvode.h", "Q_MAX"): "vendored CVODE default constant",
    ("load_eqn.h", "XPP_MAX_NAME"): "FILENAME_MAX is the C library's, not ours",
}

def strip(text):
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            while i < n and text[i] != "\n": i += 1
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2); j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j)); i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + c); i = j + 1
        else:
            out.append(c); i += 1
    return "".join(out)

BUILTIN = re.compile(r"^(unsigned |signed |long |short |const )*(int|char|float|double|long|short|bool|size_t|void)\b[\s\w*]*$")

def check(path):
    bad = []
    base = os.path.basename(path)
    text = strip(open(path, encoding="utf-8", errors="replace", newline="").read())
    ns = []   # stack: namespace name or None per open brace
    pending = None
    for no, line in enumerate(text.split("\n"), 1):
        s = line.strip()
        def flag(name, what):
            if (base, name) not in ALLOW:
                bad.append(f"{path}:{no}: {name}: {what}")
        m = re.match(r"(?:template\s*<[^>]*>\s*)?using\s+(\w+)\s*=\s*(.+?);?$", s)
        if m and not s.startswith("using namespace"):
            rhs = m.group(2)
            if "std::" not in rhs and "(*)" not in rhs and not BUILTIN.match(rhs):
                flag(m.group(1), f"a second name for {rhs.strip()}")
        m = re.match(r"namespace\s+(\w+)\s*=\s*([\w:]+);", s)
        if m: flag(m.group(1), f"namespace alias of {m.group(2)}")
        m = re.match(r"typedef\s+(.+?)\s+(\w+)\s*;$", s)
        if m and "(*" not in m.group(1) and not re.match(r"(struct|enum|union)\b[^{]*\{?$", m.group(1)) \
           and not BUILTIN.match(m.group(1)):
            flag(m.group(2), f"typedef of {m.group(1)}")
        m = re.match(r"#\s*define\s+(\w+)\s+([A-Za-z_][\w:]*)\s*$", s)
        if m: flag(m.group(1), f"#define renaming {m.group(2)}")
        m = re.match(r"using\s+(xpp::[\w:]+);", s)
        if m and "xpp" in ns:
            flag(m.group(1).split("::")[-1], f"using {m.group(1)}; inside namespace xpp")
        for ch in re.finditer(r"namespace\s+(\w+)\s*\{|([{}])", line):
            if ch.group(1): ns.append(ch.group(1))
            elif ch.group(2) == "{": ns.append(None)
            elif ns: ns.pop()
    return bad

def main():
    files = sorted(glob.glob("core/*.cpp") + glob.glob("core/*.h") + glob.glob("tests/*.cpp") + glob.glob("tests/*.h"))
    bad = [b for f in files if "third_party" not in f for b in check(f)]
    for b in bad: print(b)
    print(f"aliascheck: {len(bad)} second name(s) in {len(files)} files")
    return 1 if bad and "--check" in sys.argv else 0

sys.exit(main())
