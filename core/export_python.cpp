/* The Python back end of the model export (export_model.h): Python 3's reserved words, its spellings of the operators and the
   math functions, and the one template of the file. The script imports the standard library only (csv, math, os). */
#include "export_model.h"

#include <array>
#include <string_view>

namespace xpp {
namespace {

/* Python's keywords, the builtins the script calls, the modules it imports and every name it defines itself: the template's
   top-level names, which a model's name of the same spelling would replace */
constexpr auto RESERVED = std::to_array<std::string_view>({
    "False", "None", "True", "and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try", "while",
    "with", "yield", "abs", "float", "int", "len", "list", "open", "range", "zip", "SystemExit",
    "csv", "math", "os", "START_TIME", "DT", "TOTAL_TIME", "STORE_EVERY", "STORE_FROM", "STATE_LIMIT", "ZERO_DIVISOR", "INITIAL", "COLUMNS",
    "OUTPUT", "rhs", "outputs", "step", "stored", "row_of", "run",
    "_div", "_ieee_div", "_heav", "_sign", "_flr", "_max", "_min", "_mod"});

/* a comparison or a logical operator is 1.0 or 0.0, as the expression language's is; Python's math functions raise an error
   where C's give infinity or NaN (log(0), exp(1000)), so the script stops there */
constexpr auto FUNCTIONS = std::to_array<Spelling>({
    {"SIN", "math.sin({0})"},
    {"COS", "math.cos({0})"},
    {"TAN", "math.tan({0})"},
    {"ASIN", "math.asin({0})"},
    {"ACOS", "math.acos({0})"},
    {"ATAN", "math.atan({0})"},
    {"SINH", "math.sinh({0})"},
    {"TANH", "math.tanh({0})"},
    {"COSH", "math.cosh({0})"},
    {"ABS", "abs({0})"},
    {"EXP", "math.exp({0})"},
    {"LN", "math.log({0})"},
    {"LOG10", "math.log10({0})"},
    {"SQRT", "math.sqrt({0})"},
    {"~", "(-{0})"},
    {"HEAV", "_heav({0})"},
    {"SIGN", "_sign({0})"},
    {"FLR", "_flr({0})"},
    {"NOT", "float({0} == 0.0)"},
    {"ERF", "math.erf({0})"},
    {"ERFC", "math.erfc({0})"},
    {"HOM_BCS", "0.0"},
    {"LGAMMA", "math.lgamma({0})"},
    {"ATAN2", "math.atan2({0}, {1})"},
    {"MAX", "_max({0}, {1})"},
    {"MIN", "_min({0}, {1})"},
    {"MOD", "_mod({0}, {1})"},
    {"^", "math.pow({0}, {1})"},
    {"&", "float({0} != 0.0 and {1} != 0.0)"},
    {"|", "float({0} != 0.0 or {1} != 0.0)"},
    {">", "float({0} > {1})"},
    {"<", "float({0} < {1})"},
    {"==", "float({0} == {1})"},
    {">=", "float({0} >= {1})"},
    {"<=", "float({0} <= {1})"},
});

/* The parts in braces are the translator's (export_model.cpp's script()); no other brace is in the file. */
constexpr std::string_view FILE = R"PY({header}
import csv
import math
import os

# What xppautX had when this file was written.
START_TIME = {start_time}
DT = {dt}
TOTAL_TIME = {total_time}
STORE_EVERY = {store_every}
STORE_FROM = {store_from}
STATE_LIMIT = {state_limit}
ZERO_DIVISOR = {zero_divisor}
INITIAL = [{initial}]
COLUMNS = [{columns}]
OUTPUT = os.path.splitext(os.path.abspath(__file__))[0] + ".csv"


def _div(x, y):
    # XPP's division: a zero divisor is replaced by ZERO_DIVISOR
    return x / (y if y != 0.0 else ZERO_DIVISOR)


def _ieee_div(x, y):
    # IEEE division: x / 0 is infinite or NaN as in C, where Python raises
    if y == 0.0:
        if x == 0.0 or x != x:
            return math.nan
        return math.copysign(math.inf, x) * math.copysign(1.0, y)
    return x / y


def _heav(z):
    return 0.0 if z < 0.0 else 1.0


def _sign(z):
    return 1.0 if z > 0.0 else (-1.0 if z < 0.0 else 0.0)


def _flr(z):
    return z if math.isinf(z) or math.isnan(z) else float(math.floor(z))


def _max(x, y):
    return x if x > y else y


def _min(x, y):
    return x if x < y else y


def _mod(x, y):
    # the remainder with the sign of y, in [0, y) for a positive y
    try:
        z = math.fmod(x, y)
    except ValueError:
        return math.nan
    return z + y if z < 0.0 else z


{parameters}
{functions}
{derived}


def rhs(t, _y):
    [{states}] = _y
{rhs_body}
    return [{derivatives}]


def outputs(t, _y):
    [{states}] = _y
{aux_body}
    return [{aux_values}]


def step(t, _y):
    # one step of the classic fixed-step Runge-Kutta method (RK4, four stages) of length DT
    _n = range(len(_y))
    _k1 = rhs(t, _y)
    _a = [_y[_i] + DT * _k1[_i] / 6.0 for _i in _n]
    _b = [_y[_i] + DT * _k1[_i] * 0.5 for _i in _n]
    _h = t + 0.5 * DT
    _k2 = rhs(_h, _b)
    _a = [_a[_i] + DT * _k2[_i] / 3.0 for _i in _n]
    _b = [_y[_i] + 0.5 * DT * _k2[_i] for _i in _n]
    _k3 = rhs(_h, _b)
    _a = [_a[_i] + DT * _k3[_i] / 3.0 for _i in _n]
    _b = [_y[_i] + DT * _k3[_i] for _i in _n]
    _k4 = rhs(t + DT, _b)
    return [_a[_i] + DT * _k4[_i] / 6.0 for _i in _n]


def stored(t):
    return t <= STORE_FROM if DT < 0 else t >= STORE_FROM


def row_of(t, _y):
    return [t] + list(_y) + outputs(t, _y)


def run():
    t = START_TIME
    _y = list(INITIAL)
    steps = int((TOTAL_TIME + 0.1 * abs(DT)) / abs(DT))
    groups = (steps + STORE_EVERY - 1) // STORE_EVERY
    with open(OUTPUT, "w", newline="") as _file:
        writer = csv.writer(_file)
        writer.writerow(COLUMNS)
        if stored(t):
            writer.writerow(row_of(t, _y))
        for _group in range(groups):
            for _ in range(STORE_EVERY):
                _y = step(t, _y)
                t = t + DT
            row = row_of(t, _y)
            for column, value in zip(COLUMNS[1:], row[1:]):
                if value != value:
                    raise SystemExit("%s is NaN at t = %s" % (column, t))
                if abs(value) > STATE_LIMIT:
                    raise SystemExit("%s out of bounds at t = %s (state_limit = %s)" % (column, t, STATE_LIMIT))
            if stored(t):
                writer.writerow(row)


if __name__ == "__main__":
    run()
)PY";

constexpr Language PYTHON{
    .name = "python",
    .extension = ".py",
    .comment = "# ",
    .reserved = RESERVED,
    .indent = "    ",
    .add = "({0} + {1})",
    .subtract = "({0} - {1})",
    .multiply = "({0} * {1})",
    .divide = "_ieee_div({0}, {1})",
    .guarded_divide = "_div({0}, {1})",
    .select = "({1} if {0} == 0.0 else {2})",
    .number_infinity = "math.inf",
    .number_not_a_number = "math.nan",
    .loop_counter = "float({0})",
    .functions = FUNCTIONS,
    .assign = "{0} = {1}",
    .loop_begin = "for {0} in range(int({1}), int({2}) + 1):",
    .loop_end = "",
    .if_zero = "if {0} == 0.0:",
    .else_branch = "else:",
    .if_end = "",
    .empty_block = "pass",
    .function_begin = "def {0}({1}):",
    .function_return = "return {0}",
    .function_end = "",
    .file = FILE,
};

} // namespace

const Language &python_language() { return PYTHON; }

} // namespace xpp
