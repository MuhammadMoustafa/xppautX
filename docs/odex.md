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

- **A `.ode` gives XPPAUT's numbers** (maintainer, 2026-09-27): every quirk
  of the `.ode` reader stays as XPPAUT has it, so the same file gives the
  same result in both programs; a fix goes into `.odex` only, and the
  `.ode` gets a warning (at load and from `--check`, W75). `--convert`
  (W74) keeps the `.ode`'s meaning, writing what XPPAUT computes (with a
  comment where that differs from what the line seems to say). Example:
  `y(0)=a` starts y at 0 in `.ode` (a scalar initial value is read with
  `atof`, which finds no number in `a`; measured 2026-09-27, see
  odex-quirks.md); `.odex` evaluates it after the parameters;
  `--convert` writes `y(0)=0` with a comment naming the original expression.

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
  else` compiler already produces (expr_compile.cpp, `alg_to_rpn`), so there is
  no runtime cost.
- Names are case-sensitive (`V` and `v` are two names) with no length
  limit (neither has `.ode` since W76).
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
- `markov`, `wiener` kept; `event` (`.ode`'s `global`), `boundary`
  (`bdry`), `network` (`special`) renamed, same meaning, `.odex`-spelled
  arguments; no `volt` (an equation calling `volterra(...)` is a Volterra
  equation); `include "file.incx"` includes a file.
- No `done`: the end of the file ends the model (an include file too).
- `#` starts a line comment (as `.ode`). `/* ... */` is a block comment
  (decided, maintainer, 2026-09-27): it may span lines and nest (so a
  region that already holds one can be commented out), and a `/*` never
  closed is an error at the line where it opened.

## Grammar (EBNF)

```ebnf
model        = { statement } ;
statement    = ode_decl | par_decl | init_decl | aux_decl
             | fun_decl | option_decl | set_decl | table_decl
             | markov_decl | wiener_decl | event_decl | boundary_decl
             | network_decl | include_decl ;
(* comments, '#' to the end of the line and nested '/* */' blocks, are
   removed by the tokenizer before this grammar *)

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
event_decl   = "event" , sign , expr , { "," , event_stmt } ;
boundary_decl = "boundary" , name , "=" , expr ;
network_decl = "network" , name , "=" , network_call ;
include_decl = "include" , filename ;

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
arg_exprs    = expr , { "," , ( expr | name , "=" , expr ) } ;
(* a built-in's arguments after the first are named: volterra(e, of=u, mu=0.5) *)

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
`then`, `else`, `set`, `table`, `markov`, `wiener`, `event`, `boundary`,
`network`, `include`.

Renamed from `.ode` (decided, maintainer, 2026-09-27): `global` is `event`,
`bdry` is `boundary`, `special` is `network`; `volt` is gone (an equation
whose right-hand side calls `volterra(...)` is a Volterra equation);
`done` and `#done` are gone (the end of the file ends it; `--convert`
moves what followed a `.ode`'s `done` into a closing comment block);
`#include` is the statement `include "file.incx"`, include files taking
the `.incx` extension.

Kept from `.ode` (decided, maintainer, 2026-09-28): `aux` stays `aux`, a
quantity recorded as an output column (saved, shown, plotted), where
`d = expr` is the same computation not recorded. `watch` was considered
and refused: it reads as a watchpoint that fires or pauses; `output`
was the alternative, but `aux` is what every XPPAUT user, the book, the
examples and the extension already use, and `--convert` keeps it as is.

Logical/operator words: `and`, `or`, `not`, `mod`.

Integral operator: `volterra` (replaces `.ode`'s `int{...}`, `int[mu]{...}`;
decided, maintainer, 2026-09-27). Its first argument is the expression,
and every argument after it is named (`volterra(exp(-x), of=u, mu=0.5)`):
a rule for every built-in that takes options. The exact names are settled
against the 11 Volterra examples before W74.

Built-in functions (from `.ode`'s own reserved set, `constants.ts:31-39`,
checked against `expr_symbols.cpp`'s symbol table): `sin`, `cos`, `tan`,
`asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `exp`, `ln`,
`log`, `log10`, `sqrt`, `heav`, `sign`, `flr`, `ran`, `abs`, `delay`,
`shift`, `ishift`, `del_shft`, `sum`, `of`, `max`, `min`, `normal`,
`besselj`, `bessely`, `besseli`, `erf`, `erfc`, `hom_bcs`.

Built-in constants: `t` (time) and `pi`. `e` is not reserved (decided,
maintainer, 2026-09-27): it stays an ordinary name, as in `.ode`, and
e^x is written `exp(x)`.

### Changes from `.ode`'s reserved words

`.ode`'s reserved names (refused as a name by XPP: the built-in functions,
`not`, `set`, `if`/`then`/`else`, `arg1`..`arg9`, `t`, `pi`) against `.odex`'s:

- New in `.odex` (accepted as names in `.ode` today): the statement
  keywords `par`, `init`, `aux`, `table`, `markov`, `wiener`, `event`,
  `boundary`, `network`, `include`; `fun`, `let`, `return`; `and`, `or`;
  `volterra`.
- Dropped in `.odex`: `arg1`..`arg9` (a function's arguments are named);
  `global`, `bdry`, `special` (renamed), `volt`, `done` (gone);
  `int` loses its meaning (it was never refused as a name in `.ode`, and
  `volterra` replaces the operator).
- Unchanged: the built-in functions, `not`, `mod`, `set`, `if`/`then`/`else`,
  `t`, `pi`. `e` is reserved in neither.
- Every change to this list is sent to the VS Code extension
  (MuhammadMoustafa/XPP-ODE-Extension#1) when it is made.

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
- A long name, or two names differing only by case:
  passed through unchanged (`.odex` has no length limit, and is case-
  sensitive, so both are legal there without any rewrite) — but
  `--convert` reports a case-collision (two `.ode` names that folded to
  one) rather than silently keeping only one.
- A function argument hiding a global name: passed through unchanged
  (same lexical-scoping meaning in both formats).

## Questions the maintainer decided

1. **Division by zero and NaN** (decided, maintainer, 2026-09-27):
   `.odex` gives real IEEE results, `1/0` is inf and `0/0` is NaN, and a
   run stops with an error naming the equation and the time at the step
   where a NaN or inf first enters the state. `.ode` keeps XPP's guard
   (a zero divisor replaced by 2.23e-15, expr_program.h, `ZERO_DIVISOR`: `1/0` is
   4.5e14, `0/0` is 0), so old models give XPP's numbers; `--check`
   (W75) warns where a `.ode` formula divides by something that can be 0.
   One flag per model, chosen by the file's extension; the evaluator is
   shared. A NaN condition in `.odex` is therefore an error, never a branch.
2. **`--convert`'s bracket policy** (decided, maintainer, 2026-09-27):
   parentheses `.ode` needed only for its no-bare-sign rule (`2*(-3)`,
   `x<(-1)`) are kept, so a converted file differs from its `.ode` only
   where the meaning requires it.
3. **Spellings and renames** (decided, maintainer, 2026-09-27):
   - A name spelled two ways in a `.ode` (`par V=1`, then `v*2`: one name,
     since `.ode` folds case) is written with its declaration's spelling
     everywhere; `--convert` lists the lines it rewrote. Two declarations
     that differ only by case cannot occur: such a `.ode` does not load
     (measured: `par V=1` and `par v=2` give 'ERROR at line N').
   - A `.ode` name that `.odex` makes a keyword (`and`, `or`, `fun`, `let`,
     `return`) must be renamed: `--convert` lists each one and asks for its
     new name, one by one, with a suggestion ready (the name plus `_`, then
     a number if taken); a dialog in the program, a prompt on the command
     line. `--convert --auto` accepts every suggestion without asking. With
     no one to answer and no `--auto`, it stops and names what needs a new
     name. No `--rename` switch.
   - The converted file starts with a comment listing every rename.
4. **`e`** (decided, maintainer, 2026-09-27): not reserved in `.odex` (see
   Reserved words), so a `.ode` using `e` as a name converts unchanged.
5. **`int`** (decided, maintainer, 2026-09-27): `volterra(...)` replaces the
   integral operator, so `int` is an ordinary name in `.odex`.
6. **Extension gap**: moot, since `.odex` does not reserve `e` (question 4).
7. **Approximate equality** (decided, maintainer, 2026-09-28; W78): `==`
   and `!=` stay exact. `near(a, b)` is true when |a-b| <= tol ·
   max(1, |a|, |b|), tol being `@ neartol=` (default 1e-9) or
   `near(a, b, tol=...)` for one call. Not `~=` (MATLAB, Lua and Octave's
   "not equal"), not `eps` (a common parameter name) and not `atol` (the
   integrator's tolerance: the solver must never change the model's
   logic). `near` is reserved; `neartol` is an option name, not reserved.
8. **Arrays** (decided, maintainer, 2026-09-28; W80): a trailing range on
   each statement, `x[j]' = -x[j] + x[j-1]  for j in 1..n by 2`; the index
   named explicitly (no implicit `[j]`), an index named like a declared
   name an error; `a..b` includes both ends; `by k` a positive integer
   step (not `a..b..k`, ambiguous, nor `step`, a likely parameter name);
   bounds and step integer expressions of literals and `const`s, never
   `par`; any integer expression as an index. No blocks and no logic:
   boundaries are separate ranges or the formula's own `if`. `--convert`
   writes arrays back as arrays.
9. **Constants and derived quantities** (decided, maintainer, 2026-09-28;
   W80): `const n = 20` is fixed at load (.ode's `number`). There is no
   `!` and `par` takes numbers only: `d = expr` always means the
   formula's current value, and when it reads only parameters, consts
   and pure functions (not `ran`, `normal`, `poisson` or a function
   calling one) the loader computes it only when a parameter changes (a
   slider, an event setting a parameter, each AUTO evaluation), as
   .ode's `!` did, so the numbers are identical. `--convert` writes `!d =
   expr` as `d = expr` and refuses one reading t, a variable or a random
   function. No per-run snapshot (`hold`): that was a side effect of
   .ode's `!`, not a feature.
10. **One Model builder** (decided, maintainer, 2026-09-28; W79): the .ode
    and .odex readers each produce the statement list, one builder makes
    the Model; W74's translation into .ode lines goes.

## The implementation (W74, W79)

Two readers and one builder (question 10). `core/odex.h` holds the
statement list both formats are read into. The `.ode` reader,
`core/ode_read.cpp`, turns an `.ode` file's lines into statements with
every quirk of its reading kept (a backslash joining lines, `#include`,
arrays and for loops, a command told apart by its first letters,
`name=value` items read with atof, a global's events split at every `=`
and `;`, `x(0)=formula` an init of the number at the formula's front and
the formula as the history); its formulas stay `.ode` text, which the
expression engine compiles as written, `.ode`'s precedence and all. The
`.odex` reader, `odex_parse.cpp` (tokenizer and grammar) and
`odex_load.cpp`, checks a model (every name declared once, read where it
may be, called with its arguments) and readies its statements: a block
function becomes the one if/then/else expression its returns make,
`near()` gets its tol, and the model's divisions are IEEE's
(`Parsed::ieee_division`, which only this reader sets). The builder,
`form_ode.cpp`'s `build_model`, makes the Model from either list: an
`.odex` formula is written as the expression engine's text
(`odex_print.cpp`'s `engine_text`, every sign and `if` bracketed and the
parentheses the engine's precedence needs, so no `.ode` quirk is
reached), a parameter's value evaluated in order, an `.odex` initial
value once the model is set up (an `.ode`'s numbers at once), an
event's actions taken one by one (so `==` in one needs no rewriting).
`odex_convert.cpp` is `--convert`. The integrator already stops a run at
a NaN in the state (always) and at |x| > `bound` (an inf), naming the
variable and the time.

### Where this spec was open: the smallest reading taken

- **Statements `.ode` has and this spec does not list**, kept with the
  smallest spelling: a fixed variable `w = expr` (a named quantity
  computed before the right-hand sides; an `aux` may repeat its name,
  `aux ica = ica`, as `.ode` allows); a Volterra equation `u(t) = expr`
  (with or without `volterra(...)`: `.ode`'s `volt u=...` has none in
  fp.ode, and `junk = volterra(0, of=x)` is a fixed variable, fr.ode); a
  derived parameter `!d = expr` (`.ode`'s `number n=100` too); the
  algebraic `solv y = guess` and `0 = expr`; `only x, y`; a comment the
  model shows is a string statement, `"{gk=0} text"` with its action.
  New reserved words: `solv`, `only`, `history`; the built-ins the spec's
  list lacks are reserved too (`lgamma`, `poisson`, `besselis`, the
  animator's `mouse_x` ... `mouse_vy`).
- **`history x = expr`** (new): x's values before the start, what a delay
  reads (`.ode`'s `x(0)=formula` keeps its formula for exactly that; its
  `init x=1` leaves the history at 0). `init` gives the initial value
  only, evaluated once the model is set up, where `.ode` evaluates its
  array initial values, so a random initial value draws in XPP's order.
- **The spec's other open forms:** `table w "w.tab"` and `table w
  expr, n=51, lo=-25, hi=25` (t the table's variable); `set name = a = 1,
  xp = v` (each value a number or a name); `network k = conv(even, 51,
  12, w, u0)` (arguments as written: names, numbers, `p{1-4}`);
  `boundary expr` (0 = expr, `x'` the value at the right end: the
  grammar's `boundary name = expr` has no meaning to give the name, so
  it is not implemented); `sum(expr, from=lo, to=hi)` with `i'` the
  index, as `volterra(kernel, of=u, mu=m)` names its options (`t'` the
  variable of integration in the kernel); `event 1 x - 1, x = 0` sets a
  variable, an aux quantity, a parameter, `out_put`, `arret` or
  `no_interp`.
- A list statement ends with its line: `par a=1 b=2` is an error (the
  missing comma), never a second statement `b=2`. A parameter's value
  reads numbers, `pi`, built-in functions and the parameters before it.
- Not in `.odex`: `.ode`'s arrays and `%` loops (a converted file has
  them expanded), `vector`, `group` and `options`; `x[i]` parses (the
  grammar's postfix) and is refused as not part of `.odex` yet.
- Until W76 and a core that keeps a name's case: names differing only by
  case, and a name that reads as a built-in in upper case (`Sin`), are
  refused with an error saying so. A name has no length limit (W76).

### What --convert writes

`xppautX --convert [--auto] model.ode` writes model.odex beside it:
the statements in the order the reader read them (Model::statements,
arrays expanded), each formula read back from the program it compiled
to. A quirk of `.ode`'s precedence becomes explicit parentheses; a
number `atof` cut, an option spaced around its `=` (dropped) or cut, a
formula in `x(0)=` (its value, and its history in a delay model) are
written as XPP read them, with a comment. A division by something that
can be 0 is written as XPP computes it, `a/(if b then b else
2.23e-15)`; a divisor that is a constant other than 0, or a sum positive
by its form (`1+exp(-x)`), is left as it is. Names are written as their
declarations spell them (the header lists the `.ode` lines that spelled
one another way); a name `.odex` reserves is renamed as question 3 says
(a question on the terminal, `--auto` for the suggestions, none asked
with no terminal: the conversion stops and names them). What followed
`done` becomes comments at the end. The `.odex` is then loaded and must
compile to the `.ode`'s own programs (an `.odex` division and its guard
read as `.ode`'s division), or `--convert` fails: exact by construction,
and checked. `tools/odexcheck.sh` (verify.sh) converts every example
that loads by itself and runs the `.odex`: every output.dat's md5 is its
`.ode`'s (tests/examples.md5).
