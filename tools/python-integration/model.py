#!/usr/bin/env python3
"""tools/python-integration/model.py: an .odex model as Python functions (W427, #489).

The equations, formulas, auxiliary quantities, tables, networks and events of a parsed Odex
(tools/odex_model.py) are turned into Python source and compiled once: the right-hand side takes
the time and the state and returns the derivatives, evaluated in xppautX's order (the formulas in
their order, then the networks, then the equations; core/my_rhs.cpp). The arithmetic is the
model's own, operation for operation. A feature of a model that is not translated is refused
with its name, never approximated.
"""

import functools
import math
import operator
import re

import numpy as np

from odex_model import Refused, parse_aux, parse_event, parse_network, parse_table, split_args

UNARY = {'sin': 'math.sin', 'cos': 'math.cos', 'tan': 'math.tan', 'tanh': 'math.tanh', 'atan': 'math.atan',
         'exp': '_exp', 'cosh': '_cosh', 'sinh': '_sinh', 'log': '_log', 'sqrt': '_sqrt', 'abs': 'abs', 'heav': '_heav'}
BINARY = {'max': '_max', 'min': '_min'}
COMPARISONS = ('<', '>', '<=', '>=', '==', '!=')
# the networks xppautX's conv makes, by the first letter of the type the model writes (core/simplenet.cpp)
CONVOLUTIONS = {'e': 'even', '0': 'zero', 'z': 'zero', 'p': 'periodic'}
# the most points a model's tables have in all (core/tabular.h table_points_limit): the allocation a model can ask of this reader
TABLE_POINTS_LIMIT = 1000000


class ModelError(Exception):
    """The model's formulas failed at run time (an exception Python raises where xppautX gets inf or NaN)."""


# ---- the functions the compiled formulas call: xppautX's, with its results where Python raises ----

def _exp(x):
    try:
        return math.exp(x)
    except OverflowError:
        return math.inf


def _cosh(x):
    try:
        return math.cosh(x)
    except OverflowError:
        return math.inf


def _sinh(x):
    try:
        return math.sinh(x)
    except OverflowError:
        return math.copysign(math.inf, x)


def _log(x):
    if x > 0:
        return math.log(x)
    return -math.inf if x == 0 else math.nan


def _sqrt(x):
    return math.sqrt(x) if x >= 0 else math.nan


def _max(x, y):
    return x if x > y else y


def _min(x, y):
    return x if x < y else y


def _heav(z):
    return 0.0 if z < 0 else 1.0


def _mod(x, y):
    z = math.fmod(x, y)
    return z + y if z < 0 else z


def _sum(terms):
    """The terms added one after another from 0, as xppautX's SUM does (Python's own sum compensates)."""
    return functools.reduce(operator.add, terms, 0.0)


def _shift(values, slot, offset):
    """The variable `offset` places after the one in `slot` (xppautX's shift); values[0] is the time."""
    i = slot + int(offset)
    if not 0 <= i < len(values):
        raise ModelError('shift reaches variable %d of %d: xppautX reads the quantity after the states there' % (i, len(values) - 1))
    return values[i]


FUNCTIONS = {'math': math, '_exp': _exp, '_cosh': _cosh, '_sinh': _sinh, '_log': _log, '_sqrt': _sqrt, '_max': _max,
             '_min': _min, '_heav': _heav, '_mod': _mod, '_sum': _sum, '_shift': _shift}


class Table:
    """A formula table: linear interpolation between its points, as core/tabular.cpp does."""
    def __init__(self, n, lo, hi, formula):
        if not 2 <= n <= TABLE_POINTS_LIMIT or lo > hi:
            raise Refused('table with %d points (2 to %d) from %g to %g' % (n, TABLE_POINTS_LIMIT, lo, hi))
        self.n, self.lo, self.hi = n, lo, hi
        self.dx = (hi - lo) / (n - 1)
        self.y = [formula(self.dx * i + lo) for i in range(n)]

    def lookup(self, x):
        n, lo, dx, y = self.n, self.lo, self.dx, self.y
        i1 = int((x - lo) / dx)
        if i1 > -1 and i1 + 1 < n:
            x1 = dx * i1 + lo
            return y[i1] + (y[i1 + 1] - y[i1]) * (x - x1) / dx
        if i1 < 0:
            return y[0] + (y[1] - y[0]) * (x - lo) / dx
        return y[n - 1] + (y[n - 1] - y[n - 2]) * (x - self.hi) / dx


class Network:
    """conv(type, n, ncon, weights, root): the convolution of n consecutive variables with a weight table,
    summed over the offsets in order as core/simplenet.cpp does (an array of n values per offset)."""
    def __init__(self, kind, n, ncon, weights, roots):
        self.n, self.values, self.roots = n, [0.0] * n, roots
        rows = np.arange(n)
        self.offsets = []
        for j in range(-ncon, ncon + 1):
            k = rows + j
            if kind == 'even':
                k = np.abs(k)
                valid = k < 2 * n
                k = np.where(k >= n, np.abs(2 * n - 2 - k), k)
            elif kind == 'zero':
                valid = (k >= 0) & (k < n)
            else:
                valid = np.ones(n, dtype=bool)
                k = (2 * n + k) % n
            self.offsets.append((weights[j + ncon], np.where(valid, k, 0), valid))

    def update(self, roots):
        y = np.array(roots)
        total = np.zeros(self.n)
        for weight, k, valid in self.offsets:
            total = total + np.where(valid, weight * y[k], 0.0)
        self.values = total.tolist()

    def value(self, x):
        j = int(x)
        return self.values[j] if 0 <= j < self.n else 0.0


def guarded(function, what):
    """function(t, y) over an array y, its result an array, with the arithmetic exceptions Python
    raises (where xppautX gets inf or NaN) as ModelError."""
    def run(t, y):
        try:
            return np.array(function(t, y.tolist()), dtype=np.float64)
        except (ZeroDivisionError, OverflowError, ValueError, TypeError) as e:
            raise ModelError('%s at t=%g: %s: %s' % (what, t, type(e).__name__, e))
    return run


class Event:
    """A global event: its crossing direction, the condition that crosses and what happens at the crossing."""
    def __init__(self, sign, condition, action):
        self.sign, self.condition, self.action = sign, guarded(condition, 'event condition'), guarded(action, 'event')

    def cond(self, t, y):
        return float(self.condition(t, y))

    def act(self, t, y):
        return self.action(t, y)


class _Parser:
    """The tokens of one expression (functions already inlined) as Python source (docs/odex.md "Grammar")."""
    def __init__(self, tr, toks, scope, in_sum=False):
        self.tr, self.t, self.scope, self.in_sum, self.i = tr, toks, scope, in_sum, 0

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def take(self, expected=None):
        if self.peek() is None or (expected and self.peek() != expected):
            raise Refused('expected %s in %s' % (expected or 'more', ' '.join(self.t)))
        self.i += 1
        return self.t[self.i - 1]

    def sub(self, toks):
        return _Parser(self.tr, toks, self.scope, self.in_sum).whole()

    def whole(self):
        src = self.expr()
        if self.i != len(self.t):
            raise Refused('cannot translate %s' % ' '.join(self.t[self.i:]))
        return src

    def expr(self):
        src = self.and_expr()
        while self.peek() == 'or':
            self.take()
            src = '(bool(%s) or bool(%s))' % (src, self.and_expr())
        return src

    def and_expr(self):
        src = self.not_expr()
        while self.peek() == 'and':
            self.take()
            src = '(bool(%s) and bool(%s))' % (src, self.not_expr())
        return src

    def not_expr(self):
        if self.peek() == 'not':
            self.take()
            return '(%s == 0.0)' % self.not_expr()
        return self.comparison()

    def comparison(self):
        src = self.sum_of_terms()
        if self.peek() in COMPARISONS:
            op = self.take()
            return '(%s %s %s)' % (src, op, self.sum_of_terms())
        return src

    def sum_of_terms(self):
        src, chained = self.product(), False
        while self.peek() in ('+', '-'):
            src += ' %s %s' % (self.take(), self.product())
            chained = True
        return '(%s)' % src if chained else src

    def product(self):
        src, chained = self.unary(), False
        while self.peek() in ('*', '/', 'mod'):
            op = self.take()
            if op == 'mod':
                src, chained = '_mod(%s, %s)' % (src, self.unary()), False
            else:
                src += ' %s %s' % (op, self.unary())
                chained = True
        return '(%s)' % src if chained else src

    def unary(self):
        if self.peek() == '-':
            self.take()
            return '(-%s)' % self.unary()
        base = self.primary()
        if self.peek() == '^':
            self.take()
            return '(%s ** %s)' % (base, self.unary())
        return base

    def primary(self):
        t = self.take()
        if re.match(r'\.?\d', t):
            return repr(float(t))
        if t == '(':
            src = self.expr()
            self.take(')')
            return src
        if t == 'if':
            condition = self.expr()
            self.take('then')
            then = self.expr()
            self.take('else')
            return '(%s if %s != 0.0 else %s)' % (then, condition, self.expr())
        if self.peek() == '(':
            args, self.i = split_args(self.t, self.i)
            return self.call(t, args)
        return self.name(t)

    def name(self, t):
        tr = self.tr
        if t == 't':
            return 't'
        if t == "i'" and self.in_sum:
            return '_ip'
        if t == 'pi':
            return repr(math.pi)
        if t in tr.state_index and self.scope != 'table':
            return 'm_' + t
        if t in tr.assigned and self.scope == 'rhs':
            return 'm_' + t
        if t in tr.odex.pars:
            return repr(tr.odex.pars[t])
        raise Refused('%s in %s: not a parameter, nor a quantity a %s formula can read' % (t, ' '.join(self.t), self.scope))

    def call(self, name, args):
        tr = self.tr
        if name == 'sum':
            return self.summation(args)
        if name == 'shift':
            if self.scope != 'rhs' or len(args) != 2 or len(args[0]) != 1 or args[0][0] not in tr.state_index:
                raise Refused('shift is translated for a state in an equation only')
            tr.uses_shift = True
            return '_shift(_V, %d, %s)' % (1 + tr.state_index[args[0][0]], self.sub(args[1]))
        if len(args) == 1 and name in tr.tables:
            return '_tab_%s(%s)' % (name, self.sub(args[0]))
        if len(args) == 1 and name in tr.networks:
            return '_net_%s.value(%s)' % (name, self.sub(args[0]))
        if len(args) == 1 and name in UNARY:
            return '%s(%s)' % (UNARY[name], self.sub(args[0]))
        if len(args) == 2 and name in BINARY:
            return '%s(%s, %s)' % (BINARY[name], self.sub(args[0]), self.sub(args[1]))
        raise Refused('the function %s with %d arguments is not translated' % (name, len(args)))

    def summation(self, args):
        if self.in_sum:
            raise Refused('a sum inside a sum is not translated')
        ends = {}
        for a in args[1:]:
            if len(a) < 3 or a[0] not in ('from', 'to') or a[1] != '=':
                raise Refused('sum(expression, from=a, to=b) expected')
            ends[a[0]] = self.sub(a[2:])
        if len(args) != 3 or set(ends) != {'from', 'to'}:
            raise Refused('sum(expression, from=a, to=b) expected')
        body = _Parser(self.tr, args[0], self.scope, True).whole()
        return '_sum(%s for _ip in range(int(%s), int(%s) + 1))' % (body, ends['from'], ends['to'])


class Translator:
    """The Odex's names and what it declares, for the expression parser."""
    def __init__(self, odex):
        self.odex = odex
        self.state_index = {s: i for i, s in enumerate(odex.states)}
        self.assigned = {name for name, _ in odex.assigns}
        self.tables, self.networks = {}, {}
        self.uses_shift = False

    def python(self, toks, scope):
        try:
            return _Parser(self, self.odex.expand(toks), scope).whole()
        except RecursionError:
            raise Refused('an expression nested too deeply, or a function that calls itself: ' + ' '.join(toks)[:80])


def unpack(odex):
    """The first line of a compiled function: the state in a local variable for each variable."""
    return '    %s%s = y' % (', '.join('m_' + s for s in odex.states), ',' if len(odex.states) == 1 else '')


def names_read(odex):
    """Every name the model's equations, formulas, functions, auxiliary quantities, tables and events read."""
    texts = [' '.join(toks) for toks in list(odex.odes.values()) + [t for _, t in odex.assigns] + [b for _, b in odex.funs.values()]]
    return set(re.findall(r"[A-Za-z_]\w*'?", ' '.join(texts + odex.aux + odex.events + odex.tables)))


def add_network(tr, env, text):
    """The network of a `network` statement, when anything reads it (core/simplenet.cpp's conv)."""
    name, call, args = parse_network(text)
    if name not in names_read(tr.odex):
        return  # nothing reads it: its values would change nothing
    if call != 'conv' or len(args) != 5 or any(len(a) != 1 for a in args):
        raise Refused('only conv(type, n, ncon, weights, root) is translated: network ' + text)
    kind, n, ncon, weights, root = (a[0] for a in args)
    pool = tr.odex.states if root in tr.state_index else [a for a, _ in tr.odex.assigns]
    if kind[0].lower() not in CONVOLUTIONS or weights not in tr.tables or len(tr.tables[weights].y) < 2 * int(ncon) + 1:
        raise Refused('network %s: the type %s or the table %s does not fit' % (name, kind, weights))
    if int(n) * (2 * int(ncon) + 1) > TABLE_POINTS_LIMIT:
        raise Refused('network %s: %s values at %s offsets are more than the %d a table may hold' % (name, n, 2 * int(ncon) + 1, TABLE_POINTS_LIMIT))
    if root not in pool or pool.index(root) + int(n) > len(pool):
        raise Refused('network %s: %s and the %s variables after it are not all declared' % (name, root, n))
    first = pool.index(root)
    tr.networks[name] = Network(CONVOLUTIONS[kind[0].lower()], int(n), int(ncon), tr.tables[weights].y, pool[first:first + int(n)])
    env['_net_' + name] = tr.networks[name]


def compile_model(odex):
    """The compiled model of an Odex (its parameters and initial values are those it holds now)."""
    tr = Translator(odex)
    env = dict(FUNCTIONS)
    for text in odex.tables:
        name, toks, n, lo, hi = parse_table(text)
        tr.tables[name] = Table(n, lo, hi, eval('lambda t: ' + tr.python(toks, 'table'), env))
        env['_tab_' + name] = tr.tables[name].lookup
    for text in odex.networks:
        add_network(tr, env, text)
    formulas = ['    m_%s = %s' % (name, tr.python(toks, 'rhs')) for name, toks in odex.assigns]
    updates = ['    _net_%s.update([%s])' % (name, ', '.join('m_' + r for r in net.roots)) for name, net in tr.networks.items()]
    equations = [tr.python(odex.odes[s], 'rhs') for s in odex.states]
    aux = [(name, toks) for text in odex.aux for name, toks in parse_aux(text)]
    quantities = [tr.python(toks, 'rhs') for _, toks in aux]
    shift = ['    _V = [t, %s]' % ', '.join('m_' + s for s in odex.states)] if tr.uses_shift else []
    source = '\n'.join(['def _rhs(t, y):', unpack(odex)] + shift + formulas + updates + ['    return [%s]' % ', '.join(equations),
                       'def _aux(t, y):', unpack(odex)] + shift + formulas + ['    return [%s]' % ', '.join(quantities)])
    code = {}
    exec(source, env, code)
    events = []
    for text in odex.events:
        sign, cond, actions = parse_event(text)
        for target, _ in actions:
            if target not in tr.state_index:
                raise Refused('event sets %s: only a state is translated' % target)
        lines = ['def _condition(t, y):', unpack(odex), '    return %s' % tr.python(cond, 'event'), 'def _action(t, y):', unpack(odex)]
        lines += ['    m_%s = %s' % (target, tr.python(toks, 'event')) for target, toks in actions]
        lines += ['    return [%s]' % ', '.join('m_' + s for s in odex.states)]
        event_code = {}
        exec('\n'.join(lines), env, event_code)
        events.append(Event(sign, event_code['_condition'], event_code['_action']))
    return CompiledModel(odex, code['_rhs'], code['_aux'], [n for n, _ in aux], events)


class CompiledModel:
    """A model ready to run: states, auxiliary names, initial values, right-hand side, auxiliary quantities, events."""
    def __init__(self, odex, rhs, aux, aux_names, events):
        self.states, self.aux_names, self.events = odex.states, aux_names, events
        self.y0 = np.array([odex.inits.get(s, 0.0) for s in odex.states])  # a state without init starts at 0
        self.rhs, self.aux = guarded(rhs, 'right-hand side'), guarded(aux, 'auxiliary quantity')
