#!/usr/bin/env -S uv run --locked python
"""W269 (#326): ModelDB 267710's model for AUTO-07p: its .odex (xppautX's conversion of
rest.ode, the includes inlined) written as AUTO-07p's C equation file, the same equations
in the same order, so AUTO-07p continues the same problem as XPPAUT and xppautX.

Only what this model uses is translated: par, fixed quantities (name = expr, evaluated in
file order before the right-hand sides, as XPPAUT does), fun, x' = expr, init; expressions
with + - * / ^, comparisons, if/then/else and calls. The start point is given separately
(the state AUTO starts from in XPPAUT: the end of the 600 ms integration): the last row
of that integration's output.dat (time, then the states in the model's order).

    uv run --locked python odex_to_c.py MODEL.odex OUTPUT.dat OUT.c
"""
import re, sys

TOKEN = re.compile(r'\s*(?:(\d+\.?\d*(?:[eE][-+]?\d+)?|\.\d+(?:[eE][-+]?\d+)?)|([A-Za-z_][A-Za-z_0-9]*)|(<=|>=|==|!=|[-+*/^(),<>]))')
BUILTIN = {'exp': 'exp', 'abs': 'fabs', 'sqrt': 'sqrt', 'log': 'log', 'tanh': 'tanh', 'sin': 'sin', 'cos': 'cos'}
CONSTANT = {'pi': 'M_PI'}


def tokens(text):
    pos, out = 0, []
    while pos < len(text.rstrip()):
        m = TOKEN.match(text, pos)
        if not m:
            raise ValueError(f'cannot read {text[pos:]!r} in {text!r}')
        out.append(m.group(1) or m.group(2) or m.group(3))
        pos = m.end()
    return out


class Expr:
    """XPP's expression grammar, written as C: if/then/else lowest, then comparisons, + -,
    * /, unary minus, ^ (right-associative, above unary minus: -x^2 is -(x^2))"""

    def __init__(self, text, names):
        self.t, self.i, self.names = tokens(text), 0, names

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def take(self, want=None):
        tok = self.peek()
        if want is not None and tok != want:
            raise ValueError(f'expected {want!r}, got {tok!r} in {" ".join(self.t)}')
        self.i += 1
        return tok

    def parse(self):
        out = self.cond()
        if self.peek() is not None:
            raise ValueError(f'unread {self.t[self.i:]}')
        return out

    def cond(self):
        if self.peek() == 'if':
            self.take()
            c = self.cond()
            self.take('then')
            a = self.cond()
            self.take('else')
            b = self.cond()
            return f'(({c}) ? ({a}) : ({b}))'
        return self.compare()

    def compare(self):
        left = self.additive()
        while self.peek() in ('<', '>', '<=', '>=', '==', '!='):
            op = self.take()
            left = f'(double)({left} {op} {self.additive()})'
        return left

    def additive(self):
        left = self.term()
        while self.peek() in ('+', '-'):
            op = self.take()
            left = f'({left} {op} {self.term()})'
        return left

    def term(self):
        left = self.unary()
        while self.peek() in ('*', '/'):
            op = self.take()
            left = f'({left} {op} {self.unary()})'
        return left

    def unary(self):
        if self.peek() == '-':
            self.take()
            return f'(-{self.unary()})'
        if self.peek() == '+':
            self.take()
            return self.unary()
        return self.power()

    def power(self):
        base = self.primary()
        if self.peek() == '^':
            self.take()
            return f'pow({base}, {self.unary()})'
        return base

    def primary(self):
        tok = self.take()
        if tok == '(':
            inner = self.cond()
            self.take(')')
            return f'({inner})'
        if tok[0].isdigit() or tok[0] == '.':
            return tok if any(ch in tok for ch in '.eE') else tok + '.0'  # every XPP number is a double: 1/225 is not 0
        if self.peek() == '(':
            self.take()
            args = [] if self.peek() == ')' else [self.cond()]
            while self.peek() == ',':
                self.take()
                args.append(self.cond())
            self.take(')')
            name = BUILTIN.get(tok) or self.names.get(('fun', tok))
            if name is None:
                raise ValueError(f'unknown function {tok}')
            return f'{name}({", ".join(args)})'
        if tok in CONSTANT and ('var', tok) not in self.names:
            return CONSTANT[tok]
        name = self.names.get(('var', tok))
        if name is None:
            raise ValueError(f'unknown name {tok}')
        return name


def read(path):
    """(pars [(name, value)], fixed [(name, expr)], funs [(name, args, expr)], odes [(name, expr)], inits {name: value})"""
    pars, fixed, funs, odes, inits = [], [], [], [], {}
    depth = 0
    for raw in open(path):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        if depth or line.startswith('options'):  # the options block: AUTO's constants go in c.* files
            depth += line.count('{') - line.count('}')
            continue
        if line.startswith('par '):
            for item in line[4:].split(','):
                name, value = item.split('=')
                pars.append((name.strip(), value.strip()))
        elif line.startswith('init '):
            for item in line[5:].split(','):
                name, value = item.split('=')
                inits[name.strip()] = value.strip()
        elif line.startswith('fun '):
            m = re.fullmatch(r'fun\s+(\w+)\s*\(([^)]*)\)\s*=\s*(.+)', line)
            funs.append((m.group(1), [a.strip() for a in m.group(2).split(',')], m.group(3)))
        else:
            m = re.fullmatch(r"(\w+)(')?\s*=\s*(.+)", line)
            if not m:
                raise ValueError(f'{path}: cannot read {line!r}')
            (odes if m.group(2) else fixed).append((m.group(1), m.group(3)))
    return pars, fixed, funs, odes, inits


def emit(path, start, out):
    pars, fixed, funs, odes, inits = read(path)
    names = {('fun', f): f'fn_{f}' for f, _, _ in funs}
    c = ['/* Generated by odex_to_c.py (W269, #326) from ' + path.split('/')[-1] + ': do not edit. */',
         '#include "auto_f2c.h"', '#include <math.h>', '']
    for f, args, _ in funs:
        c.append(f'static double fn_{f}({", ".join("double a_" + a for a in args)});')
    for f, args, body in funs:
        local = {**names, **{('var', a): f'a_{a}' for a in args}}
        c.append(f'static double fn_{f}({", ".join("double a_" + a for a in args)}) {{ return {Expr(body, local).parse()}; }}')
    for i, (n, _) in enumerate(odes):
        names[('var', n)] = f'u[{i}]'
    for i, (n, _) in enumerate(pars):
        names[('var', n)] = f'par[{i}]'
    # XPPAUT evaluates the quantities in file order, a later one read before it is computed
    # keeping its last value. This model reads only constants ahead (conn.inc's conductances,
    # which no state changes), so C computes each quantity once what it reads is: file order,
    # a quantity deferred until then (the same values as XPPAUT's after its first evaluation).
    defined = {n for n, _ in odes} | {n for n, _ in pars} | {f for f, _, _ in funs} | set(CONSTANT)
    known = {n for n, _ in fixed}
    body, todo = [], list(fixed)
    while todo:
        ready = [(n, e) for n, e in todo if all(t in defined or t not in known for t in tokens(e))]
        if not ready:
            raise ValueError(f'quantities that read each other: {[n for n, _ in todo]}')
        for n, e in ready:
            body.append(f'  const double q_{n} = {Expr(e, names).parse()};')
            names[('var', n)] = f'q_{n}'
            defined.add(n)
        todo = [x for x in todo if x not in ready]
    c += ['', 'int func(integer ndim, const doublereal *u, const integer *icp, const doublereal *par, integer ijac,',
          '         doublereal *f, doublereal *dfdu, doublereal *dfdp)', '{'] + body
    c += [f'  f[{i}] = {Expr(e, names).parse()}; /* {n} */' for i, (n, e) in enumerate(odes)]
    c += ['  return 0;', '}', '']
    last = [line.split() for line in open(start) if line.strip()][-1]
    if len(last) != len(odes) + 1:
        raise ValueError(f'{start}: {len(last)} columns, expected time and {len(odes)} states')
    values = dict(zip((n for n, _ in odes), last[1:]))
    c += ['int stpnt(integer ndim, doublereal t, doublereal *u, doublereal *par)', '{']
    c += [f'  par[{i}] = {v}; /* {n} */' for i, (n, v) in enumerate(pars)]
    c += [f'  u[{i}] = {values[n]}; /* {n} */' for i, (n, _) in enumerate(odes)]
    c += ['  return 0;', '}', '',
          'int bcnd(integer ndim, const doublereal *par, const integer *icp, integer nbc, const doublereal *u0,',
          '         const doublereal *u1, integer ijac, doublereal *fb, doublereal *dbc) { return 0; }',
          'int icnd(integer ndim, const doublereal *par, const integer *icp, integer nint, const doublereal *u,',
          '         const doublereal *uold, const doublereal *udot, const doublereal *upold, integer ijac,',
          '         doublereal *fi, doublereal *dint) { return 0; }',
          'int fopt(integer ndim, const doublereal *u, const integer *icp, const doublereal *par, integer ijac,',
          '         doublereal *fs, doublereal *dfdu, doublereal *dfdp) { return 0; }',
          'int pvls(integer ndim, const doublereal *u, doublereal *par) { return 0; }', '']
    open(out, 'w').write('\n'.join(c))
    print(f'{out}: {len(odes)} states, {len(pars)} parameters, {len(fixed)} quantities, {len(funs)} functions')


if __name__ == '__main__':
    emit(*sys.argv[1:4])
