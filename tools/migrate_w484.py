"""One-shot migration of the repo's own files to W484's settings vocabulary
(format version 4). Applied once in W484's commit and kept for the record:
never re-run, never shipped (W308 question 9).

What it rewrites in a .odex (its options block and every set's options):
  transient_time -> store_from, tolerance -> relative_tolerance,
  abs_tolerance -> absolute_tolerance, singpt_newton_tolerance ->
  newton_tolerance, singpt_max_iterates -> max_iterations,
  singpt_jacobian_epsilon -> jacobian_step,
  bound: V -> state_limit: V and, in the group equilibria beside it,
  search_limit: V (a .ode's bound was both),
  ncdraw: 0|1|2 -> nullclines.draw / nullclines.file,
  rangereset|rangeoldic|rangefiles: yes|no -> true|false,
  poincare_sign: -1|0|1 -> negative|both|positive,
  poincare_stop: 0|1 -> false|true,
  colormap: 0..6 -> jet, periodic, hot, cool, blue_red, gray, cubehelix,
  lt: 1|0|-n -> line.draw (and line.marker, line.marker_size: n),
  xnc|ync|smc|umc: 0..10 and sec|uec|spc|upc: 0|20..29 -> a colour, "#rrggbb" or foreground.

A .recx's snapshot (a .snapx, a zip of settings files) is migrated member by member
(the same key renames, a yes or no written `true` or `false`, its version 4) and the
recording's fingerprint written again.

Usage: uv run --locked python tools/migrate_w484.py FILE...  (rewrites in place)
"""
import base64
import hashlib
import io
import re
import sys
import zipfile

RENAMES = {
    "transient_time": "store_from",
    "abs_tolerance": "absolute_tolerance",
    "singpt_newton_tolerance": "newton_tolerance",
    "singpt_max_iterates": "max_iterations",
    "singpt_jacobian_epsilon": "jacobian_step",
}
# a key written `key:`, not part of a longer name or a dotted one
KEY = r"(?<![\w.])%s(?=\s*:)"
SIGN = {"-1": "negative", "0": "both", "1": "positive"}
COLORMAPS = ["jet", "periodic", "hot", "cool", "blue_red", "gray", "cubehelix"]
# XPPAUT's eleven colours (colormap.cpp's table): 0 the foreground, then red to purple
COLORS = ["foreground", '"#ff0000"', '"#f06400"', '"#ffa500"', '"#ffcd00"', '"#c8c800"', '"#c8eb4b"', '"#00e100"', '"#00c8c8"',
          '"#0000ff"', '"#a020f0"']


def mask(text):
    """text with comments and strings blanked (same length), so braces and
    keys inside them are never seen"""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "#":
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif c == '"':
            out[i] = " "
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\":
                    out[i] = " "
                    i += 1
                if i < n and text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
        i += 1
    return "".join(out)


def blocks(masked):
    """every {...} as (open, close, parent index or -1), by open position"""
    stack, found = [], []
    for i, c in enumerate(masked):
        if c == "{":
            found.append([i, -1, stack[-1] if stack else -1])
            stack.append(len(found) - 1)
        elif c == "}" and stack:
            found[stack.pop()][1] = i
    return found


def group_name(masked, open_at):
    m = re.search(r"([A-Za-z_]\w*)\s*:?\s*$", masked[:open_at])
    return m.group(1) if m else ""


def migrate(text):
    # 1. the bound that was one limit becomes two (structure first: it moves text between groups)
    for _ in range(1000):
        masked = mask(text)
        bs = blocks(masked)
        done = True
        for idx, (o, c, parent) in enumerate(bs):
            if c < 0 or group_name(masked, o) != "integration" or parent < 0:
                continue
            body = masked[o + 1:c]
            m = re.search(r"(?<![\w.])bound\s*:\s*([^\s,}]+)", body)
            if not m:
                continue
            value = text[o + 1 + m.start(1):o + 1 + m.end(1)]
            # the search limit goes into the equilibria group beside, or a new one after this group
            po, pc, _p = bs[parent]
            sibling = next((b for b in bs if b[2] == parent and b[1] > 0 and group_name(masked, b[0]) == "equilibria"), None)
            edits = [(o + 1 + m.start(), o + 1 + m.start() + len("bound"), "state_limit")]
            if sibling:
                edits.append((sibling[0] + 1, sibling[0] + 1, f" search_limit: {value},"))
            else:
                after = c + 1
                k = after
                while k < len(masked) and masked[k] in " \t":
                    k += 1
                line_start = text.rfind("\n", 0, o) + 1
                indent = re.match(r"[ \t]*", text[line_start:]).group(0)
                if k < len(masked) and masked[k] == ",":
                    edits.append((k + 1, k + 1, f"\n{indent}equilibria: {{ search_limit: {value} }},"))
                else:
                    edits.append((after, after, f",\n{indent}equilibria: {{ search_limit: {value} }}"))
            for a, b, new in sorted(edits, reverse=True):
                text = text[:a] + new + text[b:]
            done = False
            break
        if done:
            break
    # 2. renames of keys
    for old, new in RENAMES.items():
        text = re.sub(KEY % old, new, text)
    text = re.sub(KEY % "tolerance", "relative_tolerance", text)
    # 3. values
    def ncdraw(m):
        return {"0": "nullclines.draw: false", "1": "nullclines.draw: true",
                "2": 'nullclines.draw: false, nullclines.file: "nullclines.dat"'}[m.group(1)]
    text = re.sub(r"(?<![\w.])ncdraw\s*:\s*([012])\b", ncdraw, text)
    text = re.sub(r"(?<![\w.])(rangereset|rangeoldic|rangefiles)(\s*:\s*)(yes|no)\b",
                  lambda m: m.group(1) + m.group(2) + ("true" if m.group(3) == "yes" else "false"), text)
    text = re.sub(r"(?<![\w.])poincare_sign(\s*:\s*)(-?[01])\b", lambda m: "poincare_sign" + m.group(1) + SIGN[m.group(2)], text)
    text = re.sub(r"(?<![\w.])poincare_stop(\s*:\s*)([01])\b",
                  lambda m: "poincare_stop" + m.group(1) + ("true" if m.group(2) == "1" else "false"), text)
    def line_type(m):
        n = int(m.group(1))
        if n == 1:
            return "line.draw: line"
        return "line.draw: points, line.marker: circle, line.marker_size: %d" % -n
    text = re.sub(r"(?<![\w.])lt\s*:\s*(-?\d+)\b", line_type, text)
    text = re.sub(r"(?<![\w.])colormap(\s*:\s*)([0-6])\b", lambda m: "colormap" + m.group(1) + COLORMAPS[int(m.group(2))], text)
    text = re.sub(r"(?<![\w.])(xnc|ync|smc|umc)(\s*:\s*)(\d+)\b", lambda m: m.group(1) + m.group(2) + COLORS[int(m.group(3))], text)
    text = re.sub(r"(?<![\w.])(sec|uec|spc|upc)(\s*:\s*)(\d+)\b",
                  lambda m: m.group(1) + m.group(2) + COLORS[0 if m.group(3) == "0" else int(m.group(3)) - 19], text)
    return text


FLAGS = "open|previous_runs|show_earlier|used|set|data|on|inflag|delay_flag"


def migrate_setx(text):
    """a settings file of a session (.setx): the key and value renames, and the flags as true or false"""
    text = migrate(text)
    # the numerics group is flat: the one bound is the state limit (stopping the run) and the equilibria's search limit
    text = re.sub(r"^(\s*)bound:\s*([^\s,}]+),", r"\g<1>state_limit: \2,\n\g<1>stop_at_limit: true,\n\g<1>search_limit: \2,", text, flags=re.M)
    text = re.sub(r"(?<![\w.])(%s)(\s*:\s*)([01])\b" % FLAGS, lambda m: m.group(1) + m.group(2) + ("true" if m.group(3) == "1" else "false"), text)
    text = re.sub(r"(?<![\w.])(rangereset|rangeoldic|rangefiles)(\s*:\s*)(yes|no)\b",
                  lambda m: m.group(1) + m.group(2) + ("true" if m.group(3) == "yes" else "false"), text)
    return re.sub(r"(?<![\w.])version(\s*:\s*)2\b", r"version\g<1>4", text)


def migrate_recx(text):
    lines = text.split("\n")
    start, end = lines.index("@snapshot"), lines.index("@end")
    old = zipfile.ZipFile(io.BytesIO(base64.b64decode("".join(lines[start + 1:end]))))
    out = io.BytesIO()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as new:
        for info in old.infolist():
            body = old.read(info.filename)
            if info.filename.endswith(".setx"):
                body = migrate_setx(body.decode()).encode()
            elif info.filename.endswith(".odex"):
                body = migrate(body.decode()).encode()
            entry = zipfile.ZipInfo(info.filename, info.date_time)
            entry.compress_type = zipfile.ZIP_DEFLATED
            new.writestr(entry, body)
    digits = base64.b64encode(out.getvalue()).decode()
    width = len(lines[start + 1])
    lines[start + 1:end] = [digits[i:i + width] for i in range(0, len(digits), width)]
    lines = [re.sub(r"^(\s*version:\s*)2,", r"\g<1>4,", l) if i < start else l for i, l in enumerate(lines)]
    # the files after the snapshot are model text: migrate them as .odex
    file_at = [i for i, l in enumerate(lines) if l.startswith("@file ") and l.endswith(".odex")]
    for i in file_at:
        j = lines.index("@end", i)
        lines[i + 1:j] = migrate("\n".join(lines[i + 1:j])).split("\n")
    # the fingerprint: the lines of the snapshot, the files and the steps (not the notes)
    steps = lines.index("@steps")
    hashed = lines[start:lines.index("@end", start) + 1]
    body = [l for l in lines[lines.index("@end", start) + 1:steps] if l != ""]
    hashed += body
    hashed += [l for l in lines[steps + 1:] if l and not l.startswith("#") and not l.startswith("fingerprint: ")]
    digest = hashlib.sha256("".join(l + "\n" for l in hashed).encode()).hexdigest()
    lines = [("fingerprint: " + digest) if l.startswith("fingerprint: ") else l for l in lines]
    return "\n".join(lines)


if __name__ == "__main__":
    for path in sys.argv[1:]:
        with open(path, newline="") as f:
            old = f.read()
        new = migrate_recx(old) if path.endswith(".recx") else migrate(old)
        if new != old:
            with open(path, "w", newline="") as f:
                f.write(new)
            print("migrated", path)
