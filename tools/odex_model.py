#!/usr/bin/env python3
"""tools/odex_model.py: the .odex reader shared by tools/auto_reference.py (AUTO-07p) and
tools/python-integration/ (the Python integrators), W427 (#489).

It classifies the statements of a model (docs/odex.md "Statements"), expands its ranges
(`for j in 1..n`, "Arrays") and keeps the statements the two programs translate: the parameters,
initial values, equations, formulas, functions and boundary conditions as token lists, and the
text of the statements only the Python integrators translate (aux, table, network, event) for
their own readers (`parse_table`, `parse_network`, `parse_event`, `parse_aux`). It evaluates
nothing: a statement it does not know is refused with its text, never skipped silently.
"""

from pathlib import Path
import re

INTRINSICS = {'exp', 'sin', 'cos', 'tan', 'tanh', 'cosh', 'sinh', 'sqrt', 'max', 'min', 'abs', 'log', 'atan'}
TOKEN = re.compile(r"\d+\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?|[A-Za-z_][A-Za-z_0-9]*'?|\^|<=|>=|==|!=|[()+\-*/,=<>]")
# the groups of a model's options block whose keys the translators read (docs/odex.md "Statements")
OPTION_GROUPS = ('integration', 'equilibria')
# a statement made once for each value of an index: `STATEMENT for INDEX in FIRST..LAST [by STEP]`
RANGE = re.compile(r'^(.*?)\s+for\s+([A-Za-z_]\w*)\s+in\s+(.+?)\.\.(.+?)(?:\s+by\s+(.+))?$')
# a range makes at most this many copies of a statement (core/odex.h max_range_length, docs/odex.md "Statements")
MAX_RANGE = 100000
# the largest exponent in a range's whole-number expression: 2^62 is the largest power of two a 64-bit integer holds
MAX_POWER = 62


class Refused(Exception):
    """A problem AUTO-07p or the integrator cannot run: the reason is the message."""


def tokens(text):
    out = TOKEN.findall(text)
    if ''.join(out) != re.sub(r'\s+', '', text):
        raise Refused('expression not understood: ' + text)
    return out


def split_args(toks, i):
    """toks[i] is '(': the argument token lists up to its ')' and the index after it."""
    depth, args, cur = 0, [], []
    while i < len(toks):
        t = toks[i]
        if t == '(':
            depth += 1
            if depth > 1:
                cur.append(t)
        elif t == ')':
            depth -= 1
            if depth == 0:
                args.append(cur)
                return args, i + 1
            cur.append(t)
        elif t == ',' and depth == 1:
            args.append(cur)
            cur = []
        else:
            cur.append(t)
        i += 1
    raise Refused('unbalanced parentheses')


def split_top(text):
    """The parts of text between its commas outside parentheses."""
    depth, parts, cur = 0, [], ''
    for c in text:
        depth += (c == '(') - (c == ')')
        if c == ',' and depth == 0:
            parts.append(cur.strip())
            cur = ''
        else:
            cur += c
    parts.append(cur.strip())
    return [p for p in parts if p]


def whole_number(text):
    """The value of an expression of whole numbers with `+ - * / mod ^`, a sign and parentheses (docs/odex.md
    "Arrays"); `/` is the whole quotient and `^` takes an exponent of at most MAX_POWER."""
    toks = re.findall(r'\d+|mod|[-+*/^()]', text)
    if ''.join(toks) != re.sub(r'\s+', '', text):
        raise Refused('not a whole-number expression: ' + text)
    pos = [0]

    def peek():
        return toks[pos[0]] if pos[0] < len(toks) else None

    def take():
        pos[0] += 1
        return toks[pos[0] - 1]

    def atom():
        t = peek()
        if t == '(':
            take()
            v = total()
            if take() != ')':
                raise Refused('unbalanced parentheses: ' + text)
            return v
        if t is None or not t.isdigit():
            raise Refused('not a whole-number expression: ' + text)
        return int(take())

    def power():
        base = atom()
        if peek() != '^':
            return base
        take()
        exponent = unary()
        if not 0 <= exponent <= MAX_POWER:
            raise Refused('exponent %d not in 0..%d: %s' % (exponent, MAX_POWER, text))
        return base ** exponent

    def unary():
        if peek() == '-':
            take()
            return -unary()
        return power()

    def product():
        v = unary()
        while peek() in ('*', '/', 'mod'):
            op, w = take(), unary()
            if op != '*' and w == 0:
                raise Refused('division by zero: ' + text)
            v = v * w if op == '*' else v // w if op == '/' else v % w
        return v

    def total():
        v = product()
        while peek() in ('+', '-'):
            v = v + product() if take() == '+' else v - product()
        return v

    try:
        v = total()
    except RecursionError:
        raise Refused('expression nested too deeply: ' + text)
    if pos[0] != len(toks):
        raise Refused('not a whole-number expression: ' + text)
    return v


def named_pairs(text, what):
    """`name = text, name = text, ...` of a par, init or aux statement: (name, value text) pairs."""
    pairs = []
    for item in split_top(text):
        name, equals, value = item.partition('=')
        if not equals or not name.strip():
            raise Refused('%s not understood: %s' % (what, item))
        pairs.append((name.strip(), value.strip()))
    return pairs


def parse_aux(text):
    """`aux` statement text (after the keyword): (name, tokens) for each quantity."""
    return [(name, tokens(value)) for name, value in named_pairs(text, 'aux')]


def parse_table(text):
    """`table` statement text: (name, formula tokens, points, low end, high end) of a formula table."""
    m = re.match(r'(\w+)\s+(.*)$', text)
    if not m:
        raise Refused('table not understood: ' + text)
    parts = split_top(m.group(2))
    keys = {k.strip(): v for k, v in (p.split('=', 1) for p in parts[1:] if '=' in p)}
    try:
        return m.group(1), tokens(parts[0]), int(keys['n']), float(keys['lo']), float(keys['hi'])
    except (KeyError, ValueError):
        raise Refused('only a formula table with n, lo and hi is translated: table ' + text)


def parse_network(text):
    """`network` statement text: (name, call name, argument token lists)."""
    m = re.match(r'(\w+)\s*=\s*(\w+)\s*(\(.*\))$', text)
    if not m:
        raise Refused('network not understood: ' + text)
    args, _ = split_args(tokens(m.group(3)), 0)
    return m.group(1), m.group(2).lower(), args


def parse_event(text):
    """`event` statement text: (sign, condition tokens, [(variable, tokens)...])."""
    m = re.match(r'(-?\d+)\s+(.*)$', text)
    if not m:
        raise Refused('event not understood: ' + text)
    parts = split_top(m.group(2))
    actions = []
    for item in parts[1:]:
        name, equals, value = item.partition('=')
        if not equals:
            raise Refused('event action not understood: ' + item)
        actions.append((name.strip(), tokens(value)))
    return int(m.group(1)), tokens(parts[0]), actions


def options_of(block):
    """The keys of the integration and equilibria groups of an options block, as text."""
    out = {}
    for group in OPTION_GROUPS:
        m = re.search(r'\b%s\s*:\s*\{([^{}]*)\}' % group, block)
        for item in split_top(m.group(1)) if m else []:
            key, colon, value = item.partition(':')
            if colon:
                out[key.strip()] = value.strip()
    return out


class Odex:
    """The statements of an .odex the translators use."""
    def __init__(self, path):
        self.path = Path(path)
        self.pars, self.inits, self.odes, self.assigns, self.funs, self.bcs = {}, {}, {}, [], {}, []
        self.aux, self.tables, self.networks, self.events = [], [], [], []
        self.options = {}
        depth, block = 0, ''
        for raw in self.path.read_text(encoding='utf-8').splitlines():
            line = raw.split('#')[0].strip()
            if depth:
                depth += line.count('{') - line.count('}')
                block += ' ' + line
            elif not line or line.startswith('"') or line == 'legacy':
                continue
            elif re.match(r'(options|set\s+("[^"]*"|\w+))\s*\{', line):  # a set's name is quoted ("vvst") in .odex
                depth = line.count('{') - line.count('}')
                block = line
            else:
                self.line(line)
                continue
            if not depth and block.startswith('options'):
                self.options.update(options_of(block))
        if not self.odes:
            raise Refused('no differential equations')
        self.states = list(self.odes)

    @property
    def discrete(self):
        return self.options.get('method') == 'discrete'

    def line(self, line):
        m = RANGE.match(line)
        if m:
            statement, index, first, last, step = m.groups()
            first, last, step = whole_number(first), whole_number(last), whole_number(step) if step else 1
            if step < 1 or (last - first) // step + 1 > MAX_RANGE:
                raise Refused('range %d..%d by %d: at most %d elements' % (first, last, step, MAX_RANGE))
            for k in range(first, last + 1, step):
                def element(bracket):
                    return str(whole_number(re.sub(r'\b%s\b' % index, str(k), bracket.group(1))))
                self.line(re.sub(r'\b%s\b' % index, str(k), re.sub(r'\[([^\]]*)\]', element, statement)))
            return
        m = re.match(r'wiener\s+(\w+)$', line)
        if m:
            self.assigns.append((m.group(1), ['0']))  # deterministic: the noise term is zero
            return
        m = re.match(r'(par|init)\s+(.*)$', line)
        if m:
            target = self.pars if m.group(1) == 'par' else self.inits
            for name, value in named_pairs(m.group(2), m.group(1)):
                try:
                    target[name] = float(value)
                except ValueError:
                    raise Refused('%s %s is not a number: %s' % (m.group(1), name, value))
            return
        m = re.match(r'fun\s+(\w+)\s*\(([^)]*)\)\s*=\s*(.*)$', line)
        if m:
            self.funs[m.group(1)] = ([a.strip() for a in m.group(2).split(',')], tokens(m.group(3)))
            return
        m = re.match(r'(aux|table|network|event)\s+(.*)$', line)
        if m:
            getattr(self, {'aux': 'aux', 'table': 'tables', 'network': 'networks', 'event': 'events'}[m.group(1)]).append(m.group(2))
            return
        m = re.match(r'boundary\s+(.*)$', line)
        if m:
            self.bcs.append(tokens(m.group(1)))
            return
        m = re.match(r"(\w+)('?)\s*=\s*(.*)$", line)
        if m:
            if m.group(2):
                self.odes[m.group(1)] = tokens(m.group(3))
            else:
                self.assigns.append((m.group(1), tokens(m.group(3))))
            return
        if re.match(r'version\s+\d+$', line):
            return  # the format version every .odex starts with (core/format_version.h)
        if re.match(r'(done|markov|number|global|dnet)\b', line):
            return
        raise Refused('model line not understood: ' + line)

    def expand(self, toks, unguarded=()):
        """The tokens with every call of a user function replaced by its body (arguments
        substituted), and a call of a name in `unguarded` replaced by its argument."""
        out, i = [], 0
        while i < len(toks):
            t = toks[i]
            if t in unguarded and i + 1 < len(toks) and toks[i + 1] == '(':
                args, i = split_args(toks, i + 1)
                out += ['('] + self.expand(args[0], unguarded) + [')']
            elif t in self.funs and i + 1 < len(toks) and toks[i + 1] == '(':
                params, body = self.funs[t]
                args, i = split_args(toks, i + 1)
                if len(args) != len(params):
                    raise Refused('call of %s with %d arguments' % (t, len(args)))
                args = [self.expand(a, unguarded) for a in args]
                sub = []
                for b in body:
                    sub += ['('] + args[params.index(b)] + [')'] if b in params else [b]
                out += ['('] + self.expand(sub, unguarded) + [')']
            else:
                out.append(t)
                i += 1
        return out
