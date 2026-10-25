"""tools/cstrip.py: C/C++ source with its comments removed, for the source checks (W481, #545).

One lexer for aliascheck, dupcheck, errorcheck and optioncheck. Newlines
are kept, so a line number in the result is the source's own. A "//" or
"/*" inside a string or character literal is not a comment, and a raw
string R"d(...)d" is one literal.

  strip(text)           comments removed, string and character literals
                        emptied ("" and '')
  strip_comments(text)  comments removed, literals kept as written

tools/deadcheck.py's lex() also returns the comments themselves (its
commented-out-code search reads them), and the awk strippers serve the
shell checks, which pipe a file through awk: each stays, for that reason.
"""
import re


def _strip(text, keep_literals):
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append('\n' * text.count('\n', i, j))
            i = j
        elif c in '"\'':
            if c == '"' and i > 0 and text[i - 1] == 'R':
                m = re.match(r'"([^(]*)\(', text[i:])
                end = text.find(')' + m.group(1) + '"', i) if m else -1
                if end >= 0:
                    stop = end + len(m.group(1)) + 2
                    out.append(text[i:stop] if keep_literals else '""' + '\n' * text.count('\n', i, end))
                    i = stop
                    continue
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1] if keep_literals else c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def strip(text):
    return _strip(text, False)


def strip_comments(text):
    return _strip(text, True)
