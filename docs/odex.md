# .odex: the model format without .ode's quirks

W73 (#121). `.odex` is a second, optional model format: `.ode` stays
supported forever, never deprecated. Both parsers build the same
in-memory `Model` (core/model.h); everything after the parse — the
integrators, AUTO, the front end — is one route regardless of which
file was read. The extension (MuhammadMoustafa/XPP-ODE-Extension) picks
the front door: `.ode`, `.odex`, and (per #1) `.recx`/`.snapx`.

This document is the spec only: no parser code. W74 implements the
`.odex` parser and `--convert`; W75 implements `xppautX --check` for a
`.ode`'s own quirks. docs/odex-quirks.md is the companion inventory of
every `.ode` quirk `.odex` removes.

## Design decisions (from issue #121)

- Branching and functions get a formal shape, marked with curly braces,
  never indentation: models travel through email, PDFs and papers, where
  indentation breaks silently. A missing `}` is a parse error at a line
  and column; a wrong indent is never significant.
- Usual arithmetic operator precedence: arithmetic first, then
  comparisons, then `not`, then `and`, then `or`. `^` is right-
  associative. `-x^2` means `-(x^2)`, matching ordinary maths notation
  (unchanged from `.ode`'s own accidental behaviour there).
- Logical operators are the words `and`, `or`, `not` — reserved words.
  `.ode`'s `&`, `|`, and `not(...)` are not `.odex` syntax; `--convert`
  maps them over exactly (`&``->``and`, `|``->``or`, `not(x)``->``not x`).
- `!=` is a working operator (`.ode`'s is dead in the parser table).
- One-line `if` form: an ordinary expression with usual precedence,
  parentheses optional: `if c then a else b`. `else if` chains a further
  condition (no `elif` keyword): `if a then 0 else if b then 1 else 2`.
- Block functions use curly braces: `fun g(v, w) { let s = v + w  if
  s > vth { return a*s } else if s > 0 { return b } else { return 0 } }`.
  A block function is pure: it may declare locals with `let` and must
  `return`, and may never write a parameter or a state variable. It
  compiles to the same stack program with jumps the current `if/then/
  else` compiler already produces (parserslow2.cpp:918-940), so there is
  no runtime cost.
- Names are case-sensitive (`V` and `v` are two names) with no length
  limit (W76 removes `.ode`'s own `XPP_NAME_MAX` cap too).
- Every problem is an error carrying a line and column. Nothing is
  silently dropped, cut, or ignored — the opposite of `.ode`'s `atof`
  truncation on `@` options, its spacing-sensitive `@` lines, and its
  "line not recognised, skip it" behaviour.

## Statements

`.odex` keeps the same statements as `.ode`, spelled with `.odex`'s own
grammar:

- State variable: `x' = EXPR` (the only derivative spelling; `.ode`'s
  `dx/dt=`, `x(t+1)=` and `!x=` are covered by `--convert`, not by new
  `.odex` syntax, since a fresh `.odex` model always uses `x'=`).
- `par NAME = EXPR, NAME = EXPR, ...` — one or more parameters. Unlike
  `.ode`'s `@`-option numbers, a parameter's default value is a full
  expression, evaluated once at load (already true in `.ode`).
- `init NAME = EXPR, ...` — initial conditions, evaluated once at load
  with every parameter in scope (fixing the `.ode` quirk in
  docs/odex-quirks.md where a parameter read into `init`/`x(0)=` was
  measured as 0).
- `aux NAME = EXPR, ...` — auxiliary (recorded, non-integrated) values.
- `fun NAME(ARG, ...) = EXPR` — a one-line function, usual precedence,
  may use the one-line `if`.
- `fun NAME(ARG, ...) { ... }` — a block function (above).
- `@ NAME = VALUE, NAME = VALUE, ...` — options, comma-separated, each
  parsed independently and never silently dropped; a bad value is an
  error naming the option and the bad text, not a silent truncation.
- `set NAME = EXPR, ...` — a named parameter set, as `.ode`.
- `table NAME EXPR` / `table NAME FILE` — as `.ode`.
- `markov`, `wiener`, `global`, `bdry`, `volt`, `special` — kept, same
  meaning as `.ode`, `.odex`-spelled arguments.
- `done` ends the file (optional in `.odex`: end-of-file is equivalent,
  since nothing after it was ever meaningful in `.ode` either).
- `#` starts a line comment (as `.ode`).

## Grammar (EBNF)

```ebnf
model        = { statement } ;
statement    = ode_decl | par_decl | init_decl | aux_decl
             | fun_decl | option_decl | set_decl | table_decl
             | markov_decl | wiener_decl | global_decl | bdry_decl
             | volt_decl | special_decl | comment | "done" ;

ode_decl     = name , "'" , "=" , expr ;
par_decl     = "par" , name_init_list ;
init_decl    = "init" , name_init_list ;
aux_decl     = "aux" , name_init_list ;
name_init_list = name , "=" , expr , { "," , name , "=" , expr } ;

fun_decl     = "fun" , name , "(" , [ arg_list ] , ")" , "=" , expr
             | "fun" , name , "(" , [ arg_list ] , ")" , block ;
arg_list     = name , { "," , name } ;

block        = "{" , { block_stmt } , "}" ;
block_stmt   = let_stmt | if_stmt | return_stmt ;
let_stmt     = "let" , name , "=" , expr ;
return_stmt  = "return" , expr ;
if_stmt      = "if" , expr , block ,
               { "else" , "if" , expr , block } ,
               [ "else" , block ] ;

option_decl  = "@" , opt_list ;
opt_list     = name , "=" , opt_value , { "," , name , "=" , opt_value } ;
opt_value    = number | name | filename ;

set_decl     = "set" , name , "=" , name_init_list ;
table_decl   = "table" , name , ( expr | filename ) ;
markov_decl  = "markov" , name , integer , { transition_row } ;
wiener_decl  = "wiener" , name , { "," , name } ;
global_decl  = "global" , sign , expr , { "," , event_stmt } ;
bdry_decl    = "bdry" , name , "=" , expr ;
volt_decl    = "volt" , name , "=" , expr ;
special_decl = "special" , name , "=" , special_call ;

expr         = or_expr ;
or_expr      = and_expr , { "or" , and_expr } ;
and_expr     = not_expr , { "and" , not_expr } ;
not_expr     = [ "not" ] , cmp_expr ;
cmp_expr     = add_expr , [ cmp_op , add_expr ] ;
cmp_op       = "<" | ">" | "<=" | ">=" | "==" | "!=" ;
add_expr     = mul_expr , { ( "+" | "-" ) , mul_expr } ;
mul_expr     = unary_expr , { ( "*" | "/" | "mod" ) , unary_expr } ;
unary_expr   = [ "-" ] , pow_expr ;
pow_expr     = postfix_expr , [ "^" , unary_expr ] ;  (* right-assoc *)
postfix_expr = primary , { "(" , [ arg_exprs ] , ")" | "[" , expr , "]" } ;
primary      = number | name | "(" , expr , ")" | if_expr | call ;
if_expr      = "if" , expr , "then" , expr ,
               { "else" , "if" , expr , "then" , expr } ,
               "else" , expr ;
call         = name , "(" , [ arg_exprs ] , ")" ;
arg_exprs    = expr , { "," , expr } ;

number       = digits , [ "." , [ digits ] ] , [ exponent ]
             | "." , digits , [ exponent ] ;
exponent     = ( "e" | "E" ) , [ "+" | "-" ] , digits ;
name         = letter , { letter | digit | "_" } ;   (* case-sensitive, no length limit *)
```

`pow_expr`'s right recursion into `unary_expr` (rather than back into
`pow_expr`) gives `^` its right-associativity while still letting a sign
appear on the right operand: `2^-3` parses as `2^(-3)`, and `-x^2`
parses as `unary_expr = "-" pow_expr` where `pow_expr = x^2`, i.e.
`-(x^2)`.

## Precedence, low to high

1. `or`
2. `and`
3. `not` (unary)
4. comparisons (`<`, `>`, `<=`, `>=`, `==`, `!=`) — non-associative
   (`a < b < c` is a grammar error: `.odex` requires `(a<b) and (b<c)`)
5. `+`, `-` (binary, left-associative)
6. `*`, `/`, `mod` (left-associative)
7. unary `-`
8. `^` (right-associative)
9. postfix (call, index)

This is the usual order every other language uses, unlike `.ode`'s table
(docs/odex-quirks.md's "Operator precedence" section) where comparisons
outrank all arithmetic.

## Reserved words

Statement keywords: `par`, `init`, `aux`, `fun`, `let`, `return`, `if`,
`then`, `else`, `set`, `table`, `markov`, `wiener`, `global`, `bdry`,
`volt`, `special`, `done`.

Logical/operator words: `and`, `or`, `not`, `mod`.

Built-in functions (from `.ode`'s own reserved set, `constants.ts:31-39`,
checked against `parserslow2.cpp`'s symbol table): `sin`, `cos`, `tan`,
`asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `exp`, `ln`,
`log`, `log10`, `sqrt`, `heav`, `sign`, `flr`, `ran`, `abs`, `delay`,
`shift`, `ishift`, `del_shft`, `sum`, `of`, `max`, `min`, `normal`,
`besselj`, `bessely`, `besseli`, `erf`, `erfc`, `hom_bcs`.

Built-in constants: `t` (time), `pi`, `e` (Euler's number — reserved in
`.odex` even though `.ode` accepts it harmlessly as a name today; see
docs/odex-quirks.md).

Every reserved word is refused as a declared name with a parse error
naming the word, the line, and the column — never the late, misleading
`.ode` errors ("Too many/few equations", "ERROR compiling X'") that
result from a reserved word slipping through as an ordinary token.

## Errors

Every problem `.odex` finds is a compile error: unmatched brackets, an
undeclared name, a chained comparison, a reserved word used as a name, a
`@` option with a non-numeric value, a block function that writes a
global, a block function with a path that does not `return`, and a
`.odex` file with no `.ode`-style silent fallback for any of them. Each
error carries the file, line, and column of the offending token (`.ode`'s
own errors, by contrast, sometimes point at a re-written, upper-cased,
space-stripped copy of the line: see the `Ref:N M` style errors in
docs/odex-quirks.md). Nothing is dropped, cut short, or ignored the way
`.ode` drops an unrecognised line or truncates a numeric `@` option with
`atof`.

## What `--convert` (W74) writes for each quirk

`--convert` reads a `.ode` file with the existing `.ode` parser (so it
sees exactly what xppautX itself understood, quirks included) and
re-emits the same `Model` as `.odex` text. Per quirk (see
docs/odex-quirks.md for the measured `.ode` behaviour each row starts
from):

- `^` chains (`a^b^c`): written with explicit parentheses matching
  `.ode`'s left grouping, `(a^b)^c`, so the `.odex` value (right-
  associative by default) still matches.
- `-x^2`: written as `-(x^2)` is unnecessary (matches `.odex`'s own
  rule by default) but `--convert` writes it anyway when the source
  used extra parentheses of its own, to stay a literal translation.
- A comparison beside arithmetic without parentheses (e.g. `2*3<4`,
  read by `.ode` as `2*(3<4)`): `--convert` writes the parentheses that
  keep the `.ode` value, i.e. `2*(3<4)`, not the naive `(2*3)<4` a
  `.odex` reader would otherwise assume.
- A chained comparison (`3<2<1`, read by `.ode` as `(3<2)<1`):
  `--convert` writes `(3<2)<1` explicitly, since `.odex` has no
  chaining of its own.
- `&`/`|`: mapped to `and`/`or` exactly (`.ode`'s `&` is priority 6, its
  own `and` is lower than `.odex`'s comparisons, so `--convert` adds
  parentheses around a comparison beside `&`/`|` only when needed to
  preserve the original grouping, following the same left-to-right
  reading `.ode` used).
- `not(x)`: mapped to `not x`.
- `!=`: since `.ode`'s `!=` never actually loads (docs/odex-quirks.md),
  there is no `.ode` file using it to convert; `--convert` maps
  `.odex`'s own `!=` back to nothing special (used only going forward).
- `if(c)then(a)else(b)`, with or without a trailing operator: written in
  the one-line form with the parentheses the `.ode` value needs, e.g.
  `if(1>0)then(10)else(20)+5` (whose `.ode` value applies `+5` to the
  whole `if`) becomes `(if 1>0 then 10 else 20)+5` in `.odex`, keeping
  the grouping explicit rather than relying on `.odex` giving `if`
  the same "whole expression" precedence by convention.
- Nested `if`/`else`-`if`: written with `.odex`'s `else if`.
- A unary sign the `.ode` file was forced to bracket (`2*(-3)`, since
  `2*-3` does not load): `--convert` may drop the now-unnecessary
  parentheses, since `.odex` allows a bare sign there, or keep them —
  an open question below.
- `@` options with spaces around `=` or an `atof`-truncated value:
  `--convert` writes the value the `.ode` file's own truncation
  produced (e.g. `@ total=2*3` becomes `@ total=2`, matching the value
  `.ode` actually ran with), not the source text, since `--convert`'s
  job is to preserve the `.ode`'s *meaning*, not its spelling. An
  ignored option (spaces around `=`) is dropped entirely (the default
  applies), also matching what `.ode` actually did.
- A name over 64 characters, or two names differing only by case:
  passed through unchanged (`.odex` has no length limit, and is case-
  sensitive, so both are legal there without any rewrite) — but
  `--convert` reports a case-collision (two `.ode` names that folded to
  one) rather than silently keeping only one.
- A function argument hiding a global name: passed through unchanged
  (same lexical-scoping meaning in both formats).

## Open questions for the maintainer

1. **NaN conditions**: `.ode`'s guarded division makes `0/0` evaluate to
   `0` rather than produce a NaN in practice (measured in
   docs/odex-quirks.md), so a genuine NaN condition may never arise from
   ordinary arithmetic. Does `.odex` need an explicit rule for a NaN
   condition (error, or treated as false), or is "inherits `.ode`'s
   guarded numerics, so this cannot happen" the whole answer?
2. **`--convert`'s bracket policy for an unforced sign**: when `.ode`
   required parentheses only because of its no-bare-sign rule (`2*(-3)`),
   should `--convert` drop them (since `.odex` allows `2*-3`) or keep
   them for a smaller diff against the original file? Affects how noisy
   a converted file looks next to its `.ode` source.
3. **Case-collision severity**: when a `.ode` file defines both `V` and
   `v` as if they were different names (relying on `.ode`'s case
   folding to make them one), should `--convert` refuse to convert the
   file (a real ambiguity: which spelling wins in `.odex`?), or convert
   with a renamed second name and a loud warning?
4. **`e` as a name**: `.odex` reserves `e`; a `.ode` file that already
   uses `e` as a parameter or variable name (legal today, see
   docs/odex-quirks.md) needs a `--convert` rule — rename it (to what?)
   or refuse to convert.
5. **`int`**: `.ode` treats `int` as an ordinary name outside a
   `special` declaration's right-hand side; does `.odex` reserve `int`
   everywhere (simpler grammar) or only inside a `special_call` (matches
   `.ode`, avoids surprising a converted file that used `int` as a
   variable)?
6. **Extension gap**: the VS Code extension's `reservedWords`
   (constants.ts:95-98) omits `e`, so it never flags `e` as reserved,
   even though `.odex` will. Is fixing that extension in scope for the
   `.odex` work (MuhammadMoustafa/XPP-ODE-Extension#1), or a separate,
   later issue there?
