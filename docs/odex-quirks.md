# .odex: inventory of .ode quirks

Since W154 (#206), opening `.ode` always converts and saves `.odex`
beside it. These quirks belong to the converter's foreign input reader;
loading, Reload, sessions and recordings use the resulting `.odex`.


W73 (#121): every quirk of the `.ode` reader found in the maintainer's VS
Code extension (C:\gitRepos\XPP-ODE-Extension: `operatorCheckerCore.ts`,
`semanticCheckerCore.ts`, `variableCheckerCore.ts`,
`parenthesesCheckerCore.ts`, `constants.ts`) and checked against xppautX's
own parser: line level in `core/form_ode.cpp`, formula level in
`core/expr_symbols.cpp` (the built-in symbol table, `builtins`). Each entry gives a
one-line `.ode` that shows the quirk, the value or error xppautX gives
today (measured with `./xppautX model.ode -silent`, reading `output.dat`;
`y'=0; y(0)=1; aux z=EXPR; @ total=0,dt=1; done` isolates one expression:
`aux` evaluates `EXPR` with the model's parameters in scope every step,
where a scalar `y(0)=EXPR` starts y at the number `atof` finds in
`EXPR` (0 for a parameter; the formula is kept only as delay history,
see the initial-conditions rows below) — a further quirk, out of
this card's list but worth W75 catching it too), what the extension
says (or "not flagged"), and .odex's rule. .odex's rules below restate
the maintainer's decisions on issue #121.

## Operator precedence (expr_symbols.cpp, `builtins`)

The table: priority 7 is `^`, `**` and every comparison (`<`, `>`, `<=`,
`>=`, `==`, `!=`); priority 6 is `*`, `/`, `&`, `not` and unary minus
(`~`, its own operator); priority 4 is binary `+`, `-` and `|`. So
**comparisons bind tighter than all arithmetic** — the opposite of every
other language — and `&`/`|` sit at `*`/`+`'s own priority.

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| `^` groups left | `aux z=2^3^2` | `64` (`(2^3)^2`, not `2^(3^2)`=512) | `power-associativity`, info | `^` right-associative: `2^(3^2)`=512; `--convert` parenthesizes |
| unary minus weaker than `^` | `aux z=-2^2` | `-4` (`-(2^2)`, not `4`) | `unary-minus-power`, info | same rule, kept (matches ordinary maths: `-x^2`=`-(x^2)`); no warning |
| comparison beside `*`/`/` | `aux z=2*3<4` | `2` (`2*(3<4)`, not `0`) | `comparison-precedence`, **warning** | usual precedence (arithmetic, then comparison): `0`; `--check` warns, `--convert` parenthesizes |
| comparison beside `+`/`-` | `aux z=3-1<2` | `2` (`3-(1<2)`, not `0`) | `comparison-precedence`, warning | as above: `0` |
| comparison both sides | `aux z=1+2<3+4` | `6` (`1+(2<3)+4`, not `1`) | `comparison-precedence`, warning | `1` |
| comparison over `/` | `aux z=1/2<1` | `~4.5e14` (`1/(2<1)`=`1/0`) | `comparison-precedence`, warning | `1` (`1/2`=0.5 `<` `1`) |
| chained comparison | `aux z=3<2<1` | `1` (`(3<2)<1`=`0<1`, TRUE, though neither `3<2` nor `2<1` holds) | `chained-comparison`, warning | `.odex` has no chaining: `3<2<1` is a grammar error (write `(3<2)&(2<1)`) |
| `&`/`|` beside arithmetic | `aux z=1+1&1` | `2` (`1+(1&1)`, `&` is priority 6 like `*`) | `logical-precedence`, warning, where the grouping differs from the usual one (0.4.1) | `and`/`or` are lower precedence than `+`/`-` (usual order), so `--check` still warns a `.ode` using `&`/`|` beside arithmetic since its meaning differs from `.odex`'s `and`/`or` |
| `&` vs `|` between themselves | `aux z=1|0&0` | `1` (`&` before `|`, as expected) | not flagged | `and` before `or`, same order |
| unary minus over a comparison | `aux z=-1<0` | `-0` (`-(1<0)`=`-0`, **false**, though `-1` is really below `0`) | `comparison-precedence`, warning | `1` (true) |
| unary minus over `>=` | `aux z=-1>=0` | `-1` (`-(1>=0)`=`-1`, **true**, though `-1<0`) | `comparison-precedence`, warning | `0` (false) |

## Negative signs and unary minus

A sign is legal only at the very start of an expression, after `(`, or
after `,`; anywhere else the `.ode` file refuses to load. Confirmed by
running `aux z=2*-3`:

```
Illegal syntax (Ref:5 4)
2*-3
  ^
ERROR compiling z
```

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| sign after an operator | `aux z=2*-3` | load error ("Illegal syntax") | `unary-sign`, **error**, with a rewrite to `2*(-3)` | grammar allows a unary `-` anywhere an operand is expected: `2*-3` parses as `2*(-3)`=`-6` |
| sign after a comparison | `x<-1` (e.g. `if(y<-1.308)then...`) | load error, same as above (measured: `if(y<-1.308)...` -> `Illegal syntax (Ref:47 4)`) | `unary-sign`, error | same: `-1.308` parses fine as a signed operand |
| unary `+` | `aux z=2+ +3` | load error; XPP has no unary `+` at all, not even bracketed (`(+2)` is refused too) | `unary-sign`, error (`+` has no fix but "drop it") | grammar has no unary `+` either (unnecessary; `--convert` drops a source `+2` to `2`) |
| bracketed sign, always legal | `aux z=2*(-3)` | `-6` | (nothing to flag) | same, `-6` |
| `a*-b` in general | any binary op immediately before a sign | load error unless the sign is bracketed | `unary-sign`, error | legal without brackets |
| `-x^2` | `aux z=-x^2` (with `x` a par) | `-(x^2)` (see precedence table above) | `unary-minus-power`, info | same, `-(x^2)`: matches ordinary maths, so **no** warning even though `--check` still reports it as information for a reader unfamiliar with the rule |

## `e`: Euler's number, exponent notation, or a name

Measured on master (`xppautX -silent`, a one-parameter model
`par NAME=2`, `y'=-NAME*y`):

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| bare `e` used, undefined | `par v=e` (no `par e=...`) | load error: unknown name `e` | not flagged specifically (falls under undefined-name) | `.odex`: `e` is an ordinary name (not reserved; e^x is `exp(x)`), so an undefined `e` is an unknown-name error with its line and column |
| `e` as a parameter name | `par e=2 ... exp(-2)` value used | accepted: `e`/`E` are the same name (case-insensitive), used as an ordinary parameter | not flagged (not a reserved word in `constants.ts`'s `reservedWords`, since `e` is not listed there) | `.odex`: `e` is an ordinary name, accepted (case-sensitive: `e` and `E` are two names) |
| `e` as a variable | `e'=-e` | accepted, runs as an ordinary state variable | not flagged | refused: reserved |
| `1e-3` | `aux z=1e-3` | a single number, `0.001`: the exponent's `-` is not an operator | not flagged (the tokenizer's `NUMBER` regex matches the whole literal, exactly this rule) | same: `1e-3` is one numeric-literal token |
| `1e-3^2` | `aux z=1e-3^2` | `1e-06` (`(1e-3)^2`; `1e-3` is one token, so `^` applies to the whole literal) | not flagged | same |
| exponent ambiguity after a keyword with no space | `if(1>0)then 10 else 20` (spaces around `then`/`else` kept, but xpp removes spaces first) | load error `illegal expression: 10EL`: spaces are stripped before tokenizing, so `10 else` becomes `10else`, and the `e` of `else` is read as `10`'s exponent digit-less marker | not directly (the extension flags the missing-parens `if` form separately) | .odex tokenizes on word boundaries, never after stripping whitespace: `then 10 else 20` reads as three separate tokens regardless of spacing |

## Spaces around `=` and spaces removed before tokenizing

Formula text is space-stripped before compiling (`parse_a_string` in
`form_ode.cpp` calls `remove_blanks`/`de_space`), which is harmless
inside a formula but breaks the `@` option line's own `name=value` sub-
splitter, and can fuse two words together at a token boundary.

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| spaces around `=` in `par` | `par a = 1` | accepted, `a=1` (declaration-line splitting tolerates it) | not flagged | same, whitespace-insensitive around `=` everywhere |
| spaces around `=` in `@` option | `@ total = 0.03` | **silently ignored**, `total` keeps its default (measured: with `@ total = 0.03` and `@ dt=0.01` together, the run went to `t=20`, the default `total`, not `0.03`) | `semanticCheckerCore.ts`'s `badSpacing`, **error**: "ignored by XPP... spaces and commas both separate one option from the next, so `dt = 0.1` is read as the three unrelated words" | `.odex` requires (and its grammar defines) `name=value` with optional spaces around `=`, parsed as one token: never silently dropped |
| word fused across a stripped space | `if(1>0)then 10 else 20` | load error, `10else` (above) | flagged by the parenthesized-`if` check together with the sign/precedence checks | tokenization never strips whitespace inside identifiers/keywords first |

## Floating-point literals

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| leading-dot literal | `aux z=.5` | `0.5` | not flagged | same, `.5` legal |
| trailing-dot literal | `aux z=1.` | `1` | not flagged | same, `1.` legal |
| plain exponent | `aux z=1e-3` | `0.001` (see `e` section) | not flagged | same |
| `@` option value cut by `atof` | `@ total=2*3` | silently `2`, not `6`: `.ode`'s numeric `@` options are read with `atof`, which stops at the first non-numeric character and never errors | `semanticCheckerCore.ts` `option-value`, **error**, names the exact cut (`"@ total=2*3"` gives 2) | `.odex`'s `@` values are still plain numbers, not expressions (unchanged, since AUTO/numerics options are not formulas), but a non-numeric suffix is a parse error, never a silent cut |
| `@` option, no number at all | `@ total=(4)` | silently `0` (`atof` finds no number) | same check, error | parse error |

## Several names or options on one line

In XPPAUT, `p a=1 # some words` declares parameters named `#`, `some`
and `words`; a second such line fails on the duplicate `#` ([finding
28](xppaut-findings.md#28-a--comment-after-a-declaration-makes-names-of-its-words)).
xppautX now reads `#` as a comment wherever it occurs in a `.ode` line,
before splitting declarations or joining continuations, as `.odex` does
(W160, #212). `#include`, `#done`, displayed `"` comments and Volterra
convolution separators (`int{kernel#variable}`, `int[mu]{kernel#variable}`,
and the first separator in a `k` declaration) keep their existing meanings.
Conversion retains comment text as separate `.odex` comment lines.

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| several parameters, one `par` | `par a=1, b=2` | both declared correctly (measured: `aux z=a+b` gives `3`) | not flagged | same, comma list |
| several `@` options, one line | `@ total=20,dt=0.05` | both applied (comma-separated option pairs on one `@` line, xppautX's own examples use this form) | not flagged | same |
| several `@` options, spaces around `=` mixed in | `@ total = 0.03,dt=0.01` | `total`'s value silently dropped, `dt`'s kept (each `name=value` pair is split independently, so one bad pair does not break its neighbour) | `badSpacing` error, per pair | grammar: comma-separated `name=value` list, every pair checked, none silently dropped |

## `^` grouping, comparisons, chained comparisons, `&`/`|`

Covered in "Operator precedence" above (the whole table comes from one
priority table in `expr_symbols.cpp`, `builtins`).

## `!=`, `&&`, `||`, `!`

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| `!=` | `aux z=(1!=2)` | load error: `illegal expression: 1!` — the symbol is in the operator table (priority 7, same as the other comparisons) but the tokenizer never reaches it, so there is no working not-equal in `.ode` | `unsupported-operator`, **error**, quick fix `not(a==b)` (0.4.1) | `!=` is a working operator, usual comparison precedence |
| `&&` | `aux z=1&&1` | load error (not an operator XPP recognises) | `unsupported-operator`, **error**, quick fix `&` (0.4.1) | not `.odex` syntax; `and` is the word |
| `||` | `aux z=1||1` | load error | `unsupported-operator`, **error**, quick fix `|` (0.4.1) | not `.odex` syntax; `or` is the word |
| `!` (unary not) | `aux z=!1` | load error (`not(...)` is the working spelling) | `unsupported-operator`, **error**, quick fix `not(x)` (0.4.1) | not `.odex` syntax; `not` is the word |

## if/then/else, with and without parentheses

`expr_compile.cpp` (`alg_to_rpn`) compiles `MYIF`/`MYTHEN`/`MYELSE` into jumps;
`:1541-1551` at runtime takes exactly one branch (short-circuit: the
untaken branch is never evaluated). The condition is true when not
`0.0`.

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| full parenthesized form | `if(1>0)then(10)else(20)` | works, `10` | (no finding: this is the only form that loads) | one-line form works with or without parens: `if 1>0 then 10 else 20` (parens optional) |
| bare (no parens) | `if 1>0 then 10 else 20` | load error: "Illegal syntax" | `if-syntax`, **error** (0.4.1) | grammar accepts this directly |
| parenthesized condition, bare branches | `if(1>0)then 10 else 20` | load error: `illegal expression: 10EL` (space-stripping fuses `10` and `else`, see the `e` section) | `if-syntax`, **error**, naming the space-stripping (0.4.1) | accepted |
| bare condition, parenthesized `then`, unparenthesized `else` value with a sign | `if(1>0)then -10 else 20` | load error: "Illegal syntax (Ref:52 4)" | `if-syntax`, **error** (0.4.1) | accepted, sign is a legal unary minus in operand position |
| `then` with no `else` | `if(1>0)then(10)` | load error: "If statement missing ELSE or THEN" | `if-syntax`, **error**: no else part (0.4.1) | `.odex` requires `else` too, but reports it as one clear parse error with line and column, not this message |
| trailing operator applies to the whole `if` | `aux z=if(1>0)then(10)else(20)+5` | `15` (`+5` applies to the whole `if`'s result, not just the `else` branch) | `if-trailing-operator`, information (0.4.1) | same behaviour, since it is the only sane reading |
| `^` on the whole `if` after unary minus | `aux z=-if(1>0)then(10)else(20)^2` | `-100`: `^` binds to the `if`'s result first (`10^2`), then the leading minus | `unary-minus-power`-style reasoning applies but the extension does not parse through `if(...)` today | same precedence rule as the bare `-x^2` case |
| `if` with a NaN-producing condition | `aux z=if(0/0)then(1)else(2)` | takes the **else** branch (`MYIF` jumps on `temx==0.0`; a NaN compares unequal to 0.0 in IEEE terms, so it is read as "true" and does *not* jump — but `0/0` on the parser's own guarded division was measured to jump to else, see next row) | not flagged | `.odex`: a condition is any nonzero value is true, `0` is false, exactly as in `.ode`; a genuine NaN condition is not given a special rule (documented as an open question below) |
| `0/0` in the parser | `aux z=0/0` | xppautX's parser guards division: measured result is `0`, not NaN or an error (confirms `if(0/0)` takes the else branch because the guarded `0/0` evaluates to `0`, which is falsy) | not flagged | `.odex` keeps the same guarded division (numerics unchanged); still `0/0`=`0`, no warning needed since it is not a NaN case here |
| multi-way branching, `.ode` | `if(c1)then(a)else(if(c2)then(b)else(c))` (nested) | works when fully parenthesized | not flagged | `.odex` has `else if` directly: `if c1 then a else if c2 then b else c` |
| block/curly form | not available in `.ode` | n/a | n/a | `.odex` block functions use `{ }`, see docs/odex.md |

## Reserved words and letters used as names

Measured on master (`par NAME=2`, `y'=-NAME*y`):

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| `t` as a name | `par t=1` | load error (refused) | `reservedWords` includes `t` (from `builtinConstants`); `variableCheckerCore.ts` reports `reserved-word` | refused, clear parse error naming the reserved word |
| `pi` as a name | `par pi=1` | load error | same, `pi` in `builtinConstants` | refused |
| a built-in function name (`exp`, `ln`, `abs`, `heav`, `mod`, `ran`, `if`, `then`, `else`, `delay`, `max`, `flr`, `normal`, `sign`, `mouse_x`, ...) | `par exp=1` | load error, every one tried | `builtinFunctions` (constants.ts:31-39) lists them; `reservedWords` = `builtinFunctions` (minus `int`) + `builtinConstants` | refused, one list of reserved words (see docs/odex.md) |
| `e`/`E` as a name | `par e=2` | **accepted**, case-insensitively; works harmlessly as an ordinary parameter | **not** in `reservedWords` (an extension gap: `e` is not listed in `builtinConstants`, only `t` and `pi` are) | `.odex`: not reserved either (decided), so no mismatch with the extension |
| `int` (a "special" function name) | `par int=1` | accepted (not a formula reserved word; `int` is filtered out of `reservedWords` deliberately since `constants.ts` notes it is only special on the right side of a `special` declaration) | `reservedWords` explicitly excludes `int` | `.odex` need not reserve `int` either, for the same reason: it is not a formula-level operator |
| a declaration keyword as a fixed-variable name | `par=1` (no space, so `par` itself is a variable, not a declaration) | accepted: `par=1` defines a fixed variable named `par` | `semanticCheckerCore.ts` `keyword-name`, warning: "XPP treats `par=...` as a fixed variable... but `par x=1` would be a declaration" | `.odex` keeps `par`/`init`/... as reserved statement keywords: `par=1` is a parse error (a name can never collide with a keyword, removing the ambiguity) |

## Case-insensitive names

`form_ode.cpp` (`xpp::to_upper`, xpp_io.h) and
`expr_symbols.cpp` (`converted`) upper-case every name, so `V` and `v` are the same
name.

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| two spellings, one name | `par V=1 ... par v=2` (second overwrites/conflicts with the first) | one name: the parser folds both to `V` (measured with `e`/`E` above: accepted as one name) | not flagged (the extension itself is case-insensitive in its own duplicate-name checks, comparing `nameLower`) | `.odex` names are **case-sensitive**: `V` and `v` are two names; `--convert` reports (does not silently merge) a `.ode` that spells one name two ways |

## Name length

A name has no length limit since W76 (it was 64, `XPP_NAME_MAX`);
classic XPPAUT cut names at about 9-10 characters.

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| a long name | `par verylongparametername...=1` (any length; tools/models/longnames.ode has 200) | accepted in full, no limit (W76; before, over 64 was refused at load) | not flagged | accepted, no limit at all |

## Initial conditions and division

| quirk | `.ode` | xppautX today | extension | .odex |
|---|---|---|---|---|
| a formula in `init` | `par a=2`, `init y=a`; `init y=2*3` | the value is read with `atof` (form_ode.cpp, `take_apart`), like a numeric `@` value: `init y=a` starts y at 0, `init y=2*3` at 2, `init x[1..2]=a` both at 0 | `init-value`, **error** (0.4.1) | an initial value is an expression, evaluated after the parameters: y starts at 2 and 6 |
| a formula in a scalar `y(0)=` | `par a=2`, `y(0)=a`; `y(0)=2*3`; `y(0)=exp(0)` | y starts at the number `atof` finds: 0, 2 and 0. The formula is kept only as y's history for delay equations (`delay_string`). Measured 2026-09-27; the cause is `atof`, not the order in which parameters are set | `initcond-formula`, **warning** (information when the model uses `delay`) (0.4.1) | parameters are set first: y starts at 2; `--convert` writes `y(0)=0` with a comment naming `a` |
| a derived parameter reading what changes | `!d=v`, `!d=a*t`, `!d=ran(1)` | `!` formulas are evaluated only at a run's start, after a parameter change (a slider, an event setting a parameter) and at each AUTO evaluation (derived.cpp `evaluate_derived`), with whatever the symbols hold then: `!d=v` is v's value at that moment, frozen, not v; `!d=a*t` freezes t; `!d=ran(1)` is one draw per run, redrawn at every AUTO evaluation. Read from the code; every `!` in the examples reads parameters only | `derived-frozen`, **warning**, for `!` reading t, a variable, `ran` or `normal` (next release; measured 2026-09-29: with `y'=1`, `y(0)=1`, `!d=y` stays 1, `!e=t` 0, `!r=ran(1)` one draw) | no `!`: `d = expr` is always current, and the loader computes a parameter-only formula only when parameters change (docs/odex.md, question 9); `--convert` refuses a `!` formula reading t, a variable or a random function |
| division by zero | `aux z=1/0`, `aux z=0/0` | guarded: a zero divisor becomes 2.23e-15 (expr_program.h, `ZERO_DIVISOR`), so `1/0` = 4.5e14 and `0/0` = 0, silently (kept in `.ode`; `--check` warns) | `division-by-zero`, warning, for a literal `0` divisor (0.4.1) | IEEE: inf and NaN, and the run stops with an error at the first NaN or inf in the state |

## Checked, not quirks

Candidates that were measured on xppautX and turned out to behave as a
reader expects, or as any language would. The extension does not report
them (decided by the maintainer 2026-09-28 for the argument case); they are
listed so they are not investigated again.

| candidate | `.ode` | xppautX today | why it is not a quirk | extension |
|---|---|---|---|---|
| a function argument with a global's name | `par v=3`, `f(v)=v*2`, `aux z=f(1)` | `f(1)`=`2`: inside `f`, `v` is the argument, and the global `v` cannot be reached from inside `f` (XPP has no way to name it there; pass it as another argument or rename one of them) | ordinary lexical scoping, as in C, Python or MATLAB; nothing is read differently from how it looks. Naming an argument after a state variable (`f(v)=v*(1-v)` beside `v'=`) is the usual XPP style: 81 such arguments in 29 of `examples/ode` | not reported (a hint was tried and removed as noise); rename (`F2`) already keeps the scopes apart. .odex: `--check` (W75) may still add an info note |
| a formula in an array's initial condition | `par a=2`, `x[1..2]'=0`, `x[1..2](0)=a` | evaluated: both start at 2; `x[1..2](0)=2*3` at 6, `=[j]*2` at 2 and 4, `=ran(1)*2` at random values (measured 2026-09-27) | the array form is expanded into formulas that XPP evaluates, unlike a scalar `y(0)=` (see "Initial conditions" above) | not reported |
| `&` after `*`, `\|` after `+` | `aux z=a*b&c`, `aux z=a+b\|c` | `(a*b)&c` and `(a+b)\|c` | same priority, grouped left, which is also the usual reading | not reported (only the groupings that differ are, `logical-precedence`) |
| `&` before `\|` | `aux z=1\|0&0` | `1` | `&` binds tighter than `\|`, as usual | not reported |
| operators weaker than a comparison | `aux z=1<2&3<4`, `aux z=2^2<3` | `(1<2)&(3<4)` = 1, `(2^2)<3` = 0 | the usual reading | not reported |
| a leading minus before `^` | `aux z=-x^2` | `-(x^2)` | matches ordinary maths; `exp(-x^2)` means what it looks like | information only (`unary-minus-power`), for readers who expect `(-x)^2` |
| the minus of an exponent | `aux z=1e-3^2` | `(1e-3)^2` = 1e-06 | `1e-3` is one number token; its `-` is not an operator | not reported |
| a sign at the start or after `(` / `,` | `aux z=-3E-5*a`, `aux z=2*(-3)`, `aux z=max(-1,2)` | loads, as written | the only places XPP allows a sign | not reported |
| spaces around `=` outside `@` lines | `par a = 1`, `x' = -x`, `x(0) = 1` | accepted | only `@` options and `solv` split on spaces | not reported |
| declaration values | `par a=-3E-5` | a plain number | not an expression, so no sign rule applies | not reported |

## Left-to-right operator grouping / precedence summary

Everything above at priority 6 (`*`, `/`, `&`, `not`, unary minus) and
priority 4 (`+`, `-`, `|`) groups left as usual; the one surprise is `^`
(priority 7, groups **left**: `2^3^2`=`(2^3)^2`=`64`) and that
comparisons share `^`'s own priority 7, so they bind tighter than every
arithmetic operator instead of looser.

## Examples that hit a silent-wrong-value quirk

A search of `examples/**/*.ode` (184 files) for `^` chains, `-name^`,
comparisons beside arithmetic, and `&`/`|` beside arithmetic found:

- **`examples/ode/amarig.ode:2`**: `mh(x)=exp(-x^2)-c*b*exp(-(b*x)^2)`.
  `-x^2` relies on the "unary minus weaker than `^`" rule
  (`-(x^2)`), which happens to match ordinary maths notation, so the
  value is *not* wrong — but it is a real model depending on the rule
  the extension reports as `unary-minus-power` (information severity).
- **`examples/ode/candelator.ode:16`**: `f(t)=if(t<-1.308)then(...)`.
  This file is already a documented `noload` example
  (`tests/examples.md5:29`, and the file's own header comment says so):
  the sign after `<` is exactly the "sign after a comparison" quirk
  above and XPP refuses to load it ("Illegal syntax"), matching the
  file's own note that it is deliberately left broken.
- No other example under `examples/` was found using `^` chains
  (`a^b^c`), `&`/`|` next to `+`/`-`/`*`/`/`, or a comparison directly
  beside arithmetic without parentheses; every `if(...)` in the example
  set is fully parenthesized (`ratchet.ode`, `candelator.ode`,
  `cobweb2.ode`, `lin.ode` (commented out), `test.ode`, `wcstim.ode`),
  so none of them hit the "bare `if`" or "comparison beside arithmetic"
  quirks in a silent, wrong way.

## Open items carried from this inventory

None: `e` is not reserved in .odex, and division by zero (hence NaN
conditions) is decided (docs/odex.md, open questions 1 and 4).
