#!/usr/bin/env python3
"""One thing, one name (W113, maintainer 2026-10-01). Fails a second name
for one of our own names in core/, tests/ and web2/src: `using X = <type>;`,
`namespace a = b;`, `typedef <type> X;`, `using xpp::name;` inside
namespace xpp, a #define that only renames an identifier, and in TypeScript,
renamed imports (`import {x as y}`) and duplicate exported function/const names
across files. Comments and string literals are stripped first. Allowed without
an entry: an alias of a std:: type, a function-pointer type (a new type, not a
second name), `typedef struct/enum/union {...} name;` (a definition), a
typedef of a builtin type, and in TypeScript, namespace imports
(`import * as x`). Anything else is in ALLOW below, each entry with its reason:
use the real name rather than add an entry.
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

def check_typescript(path):
    """Check TypeScript files for renamed imports (import {x as y}), which
    violate the one-name rule. Also check for duplicate exported function/const
    names across files (detected by collecting all exports and flagging duplicates)."""
    bad = []
    base = os.path.basename(path)
    text = open(path, encoding="utf-8", errors="replace", newline="").read()

    for no, line in enumerate(text.split("\n"), 1):
        # Check for renamed imports: import {x as y} or import {x as y, ...}
        # Allow namespace imports: import * as x (third-party imports are OK)
        m = re.search(r'import\s+\{([^}]*)\}\s+from', line)
        if m:
            imports = m.group(1)
            # Skip if it's a namespace import (import * as x) - those are allowed
            if 'as' in imports and 'import *' not in line:
                # Look for specific renamed imports
                for item in imports.split(','):
                    item = item.strip()
                    if ' as ' in item:
                        parts = item.split(' as ')
                        if len(parts) == 2:
                            original = parts[0].strip()
                            alias = parts[1].strip()
                            # Skip type imports (prefixed with type)
                            if not original.startswith('type '):
                                if (base, alias) not in ALLOW:
                                    bad.append(f"{path}:{no}: {alias}: renamed import of {original}")

    return bad

def collect_exports():
    """Collect all exported function and const names in web2/src to detect duplicates."""
    files = sorted(glob.glob("web2/src/**/*.ts", recursive=True))
    exports = {}  # name -> [list of (filepath, lineno, basename)]

    for path in files:
        base = os.path.basename(path)
        text = open(path, encoding="utf-8", errors="replace", newline="").read()
        for no, line in enumerate(text.split("\n"), 1):
            # Match: export function name(...) or export const name =
            # But skip type/interface exports
            m = re.search(r'^export\s+(?:function|const)\s+(\w+)[\s(=]', line)
            if m:
                name = m.group(1)
                if name not in exports:
                    exports[name] = []
                exports[name].append((path, no, base))

    return exports

def main():
    files = sorted(glob.glob("core/*.cpp") + glob.glob("core/*.h") + glob.glob("tests/*.cpp") + glob.glob("tests/*.h"))
    bad = [b for f in files if "third_party" not in f for b in check(f)]

    # Check TypeScript files in web2/src
    ts_files = sorted(glob.glob("web2/src/**/*.ts", recursive=True))
    for f in ts_files:
        bad.extend(check_typescript(f))

    # Check for duplicate exported names in web2/src
    exports = collect_exports()
    for name, locations in exports.items():
        if len(locations) > 1:
            for path, lineno, base in locations:
                if (base, name) not in ALLOW:
                    bad.append(f"{path}:{lineno}: {name}: duplicate exported function/const name (also in {len(locations)-1} other file(s))")

    total_files = len(files) + len(ts_files)
    for b in bad: print(b)
    print(f"aliascheck: {len(bad)} second name(s) in {total_files} files")
    return 1 if bad and "--check" in sys.argv else 0

sys.exit(main())
