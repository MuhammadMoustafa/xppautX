/* The Python back ends of the model export (export_model.h): Python 3's reserved words, its spellings of the operators and the
   math functions, and the one template of the file, three times over: `python` imports the standard library only (csv, math, os) and
   keeps the state in lists; `python_numba` is that script with numba's @njit on the right-hand side, the outputs and the RK4 step,
   over numpy arrays; `python_numpy` keeps the state in a numpy array and writes an array line of the model as slices. They share the
   template and what is the same in their tables; what differs is each language's `parts`, the pieces its template names. */
#include "export_model.h"

#include <algorithm>
#include <array>
#include <cstddef>
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

/* the names the numpy and numba scripts add: the numpy module, numba's decorator and numpy's own helpers (the ones math has and numpy has not) */
constexpr auto ARRAY_NAMES = std::to_array<std::string_view>({"np", "njit", "_erf", "_erfc", "_lgamma"});

constexpr auto reserved_with_array_names()
{
    std::array<std::string_view, RESERVED.size() + ARRAY_NAMES.size()> all{};
    std::ranges::copy(RESERVED, all.begin());
    std::ranges::copy(ARRAY_NAMES, all.begin() + RESERVED.size());
    return all;
}
constexpr auto ARRAY_RESERVED = reserved_with_array_names();

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

/* numba cannot make a float of a bool, which is how python writes a comparison or a logical operator */
constexpr auto NUMBA_SPELLINGS = std::to_array<Spelling>({
    {"NOT", "(1.0 if {0} == 0.0 else 0.0)"},
    {"&", "(1.0 if ({0} != 0.0 and {1} != 0.0) else 0.0)"},
    {"|", "(1.0 if ({0} != 0.0 or {1} != 0.0) else 0.0)"},
    {">", "(1.0 if {0} > {1} else 0.0)"},
    {"<", "(1.0 if {0} < {1} else 0.0)"},
    {"==", "(1.0 if {0} == {1} else 0.0)"},
    {">=", "(1.0 if {0} >= {1} else 0.0)"},
    {"<=", "(1.0 if {0} <= {1} else 0.0)"},
});

/* python's functions with numba's spellings of those */
constexpr auto numba_functions()
{
    auto all = FUNCTIONS;
    for (Spelling &function : all)
        for (const Spelling &spelling : NUMBA_SPELLINGS)
            if (function.name == spelling.name) function.text = spelling.text;
    return all;
}
constexpr auto NUMBA_FUNCTIONS = numba_functions();

/* numpy's spellings: every one is elementwise (a comparison or a logical operator is a 1.0 or 0.0 array, a conditional both of its
   branches selected by where), so the same text writes a scalar and a slice; numpy has no erf, erfc or lgamma (_erf, _erfc and
   _lgamma of the script's helpers) */
constexpr auto NUMPY_FUNCTIONS = std::to_array<Spelling>({
    {"SIN", "np.sin({0})"},
    {"COS", "np.cos({0})"},
    {"TAN", "np.tan({0})"},
    {"ASIN", "np.arcsin({0})"},
    {"ACOS", "np.arccos({0})"},
    {"ATAN", "np.arctan({0})"},
    {"SINH", "np.sinh({0})"},
    {"TANH", "np.tanh({0})"},
    {"COSH", "np.cosh({0})"},
    {"ABS", "np.abs({0})"},
    {"EXP", "np.exp({0})"},
    {"LN", "np.log({0})"},
    {"LOG10", "np.log10({0})"},
    {"SQRT", "np.sqrt({0})"},
    {"~", "(-{0})"},
    {"HEAV", "_heav({0})"},
    {"SIGN", "_sign({0})"},
    {"FLR", "_flr({0})"},
    {"NOT", "np.where({0} == 0.0, 1.0, 0.0)"},
    {"ERF", "_erf({0})"},
    {"ERFC", "_erfc({0})"},
    {"HOM_BCS", "0.0"},
    {"LGAMMA", "_lgamma({0})"},
    {"ATAN2", "np.arctan2({0}, {1})"},
    {"MAX", "_max({0}, {1})"},
    {"MIN", "_min({0}, {1})"},
    {"MOD", "_mod({0}, {1})"},
    {"^", "np.power({0}, {1})"},
    {"&", "np.where(({0} != 0.0) & ({1} != 0.0), 1.0, 0.0)"},
    {"|", "np.where(({0} != 0.0) | ({1} != 0.0), 1.0, 0.0)"},
    {">", "np.where({0} > {1}, 1.0, 0.0)"},
    {"<", "np.where({0} < {1}, 1.0, 0.0)"},
    {"==", "np.where({0} == {1}, 1.0, 0.0)"},
    {">=", "np.where({0} >= {1}, 1.0, 0.0)"},
    {"<=", "np.where({0} <= {1}, 1.0, 0.0)"},
});

/* The parts in braces are the translator's (export_model.cpp's script()) or the language's own (its `parts`); no other brace is in
   the file. */
constexpr std::string_view FILE = R"PY({header}
{imports}

# What xppautX had when this file was written.
START_TIME = {start_time}
DT = {dt}
TOTAL_TIME = {total_time}
STORE_EVERY = {store_every}
STORE_FROM = {store_from}
STATE_LIMIT = {state_limit}
ZERO_DIVISOR = {zero_divisor}
INITIAL = {initial}
COLUMNS = [{columns}]
OUTPUT = os.path.splitext(os.path.abspath(__file__))[0] + ".csv"


{helpers}


{mod}


{parameters}
{functions}
{derived}


{jit}def rhs(t, _y):
{rhs_body}
    return {derivatives}


{jit}def outputs(t, _y):
{aux_body}
    return {aux_values}


{step}


def stored(t):
    return t <= STORE_FROM if DT < 0 else t >= STORE_FROM


def row_of(t, _y):
    return {row}


def run():
    t = START_TIME
    _y = {initial_state}
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

/* the helpers of the scalar scripts (python's, and numba's, which compiles them: {jit} is its decorator); Python's math functions
   raise an error where C's give infinity or NaN, hence _ieee_div and _flr */
constexpr std::string_view SCALAR_HELPERS = R"PY({jit}def _div(x, y):
    # XPP's division: a zero divisor is replaced by ZERO_DIVISOR
    return x / (y if y != 0.0 else ZERO_DIVISOR)


{jit}def _ieee_div(x, y):
    # IEEE division: x / 0 is infinite or NaN as in C, where Python raises
    if y == 0.0:
        if x == 0.0 or x != x:
            return math.nan
        return math.copysign(math.inf, x) * math.copysign(1.0, y)
    return x / y


{jit}def _heav(z):
    return 0.0 if z < 0.0 else 1.0


{jit}def _sign(z):
    return 1.0 if z > 0.0 else (-1.0 if z < 0.0 else 0.0)


{jit}def _flr(z):
    return z if math.isinf(z) or math.isnan(z) else float(math.floor(z))


{jit}def _max(x, y):
    return x if x > y else y


{jit}def _min(x, y):
    return x if x < y else y)PY";

constexpr std::string_view PYTHON_MOD = R"PY(def _mod(x, y):
    # the remainder with the sign of y, in [0, y) for a positive y
    try:
        z = math.fmod(x, y)
    except ValueError:
        return math.nan
    return z + y if z < 0.0 else z)PY";

constexpr std::string_view NUMBA_MOD = R"PY({jit}def _mod(x, y):
    # the remainder with the sign of y, in [0, y) for a positive y (numba has no math.fmod: np.fmod, NaN where Python's raises)
    z = np.fmod(x, y)
    return z + y if z < 0.0 else z)PY";

/* numpy's helpers, elementwise: they take a number or an array. numpy is quiet where C is (a zero divisor, log(0), exp(1000) give
   infinity or NaN, which the run's checks catch) */
constexpr std::string_view NUMPY_HELPERS = R"PY(np.seterr(all="ignore")


def _div(x, y):
    # XPP's division: a zero divisor is replaced by ZERO_DIVISOR
    return x / np.where(y != 0.0, y, ZERO_DIVISOR)


def _ieee_div(x, y):
    # IEEE division: x / 0 is infinite or NaN as in C
    return np.divide(x, y)


def _heav(z):
    return np.where(z < 0.0, 0.0, 1.0)


def _sign(z):
    return np.where(z > 0.0, 1.0, np.where(z < 0.0, -1.0, 0.0))


def _flr(z):
    return np.where(np.isfinite(z), np.floor(z), z)


def _max(x, y):
    return np.where(x > y, x, y)


def _min(x, y):
    return np.where(x < y, x, y)


_erf = np.vectorize(math.erf, otypes=[float])
_erfc = np.vectorize(math.erfc, otypes=[float])
_lgamma = np.vectorize(math.lgamma, otypes=[float]))PY";

constexpr std::string_view NUMPY_MOD = R"PY(def _mod(x, y):
    # the remainder with the sign of y, in [0, y) for a positive y
    z = np.fmod(x, y)
    return np.where(z < 0.0, z + y, z))PY";

constexpr std::string_view LIST_STEP = R"PY(def step(t, _y):
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
    return [_a[_i] + DT * _k4[_i] / 6.0 for _i in _n])PY";

/* the same steps, in the same order of operations, on arrays: every number is the list step's */
constexpr std::string_view ARRAY_STEP = R"PY({jit}def step(t, _y):
    # one step of the classic fixed-step Runge-Kutta method (RK4, four stages) of length DT
    _k1 = rhs(t, _y)
    _a = _y + DT * _k1 / 6.0
    _b = _y + DT * _k1 * 0.5
    _h = t + 0.5 * DT
    _k2 = rhs(_h, _b)
    _a = _a + DT * _k2 / 3.0
    _b = _y + 0.5 * DT * _k2
    _k3 = rhs(_h, _b)
    _a = _a + DT * _k3 / 3.0
    _b = _y + DT * _k3
    _k4 = rhs(t + DT, _b)
    return _a + DT * _k4 / 6.0)PY";

constexpr auto PYTHON_PARTS = std::to_array<Spelling>({
    {"imports", "import csv\nimport math\nimport os"},
    {"imports_note", "the standard library is all it imports"},
    {"jit", ""},
    {"helpers", SCALAR_HELPERS},
    {"mod", PYTHON_MOD},
    {"step", LIST_STEP},
    {"row", "[t] + list(_y) + outputs(t, _y)"},
    {"initial_state", "list(INITIAL)"},
});

constexpr auto NUMBA_PARTS = std::to_array<Spelling>({
    {"imports", "import csv\nimport math\nimport os\n\nimport numpy as np\nfrom numba import njit"},
    {"imports_note", "it imports numpy and numba, which compiles the right-hand side, the outputs and the RK4 step on their first call (@njit, never fastmath: the numbers are the python script's)"},
    {"jit", "@njit\n"},
    {"helpers", SCALAR_HELPERS},
    {"mod", NUMBA_MOD},
    {"step", ARRAY_STEP},
    {"row", "[t] + _y.tolist() + outputs(t, _y).tolist()"},
    {"initial_state", "INITIAL.copy()"},
});

constexpr auto NUMPY_PARTS = std::to_array<Spelling>({
    {"imports", "import csv\nimport math\nimport os\n\nimport numpy as np"},
    {"imports_note", "it imports numpy: the state is an array, and an array line of the model is written as slices"},
    {"jit", ""},
    {"helpers", NUMPY_HELPERS},
    {"mod", NUMPY_MOD},
    {"step", ARRAY_STEP},
    {"row", "[t] + _y.tolist() + outputs(t, _y).tolist()"},
    {"initial_state", "INITIAL.copy()"},
});

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
    .array_literal = "[{0}]",
    .empty_array = "[]",
    .unpack_states = "[{0}] = _y",
    .read_state = "",
    .slice = "",
    .index_range = "",
    .slice_value = "",
    .parts = PYTHON_PARTS,
    .file = FILE,
};

/* python with numba's @njit: numpy arrays for the state (numba compiles lists of floats a function is passed with a warning), the
   same scalar statements */
constexpr Language numba_language()
{
    Language l = PYTHON;
    l.name = "python_numba";
    l.reserved = ARRAY_RESERVED;
    l.functions = NUMBA_FUNCTIONS;
    l.function_begin = "@njit\ndef {0}({1}):";
    l.array_literal = "np.array([{0}])";
    l.empty_array = "np.empty(0)";
    l.unpack_states = "";
    l.read_state = "{0} = _y[{1}]";
    l.parts = NUMBA_PARTS;
    return l;
}
constexpr Language PYTHON_NUMBA = numba_language();

/* python on numpy: the state an array, an array line of the model a slice, every function numpy's */
constexpr Language numpy_language()
{
    Language l = PYTHON;
    l.name = "python_numpy";
    l.reserved = ARRAY_RESERVED;
    l.select = "np.where({0} == 0.0, {1}, {2})";
    l.number_infinity = "np.inf";
    l.number_not_a_number = "np.nan";
    l.functions = NUMPY_FUNCTIONS;
    l.array_literal = "np.hstack([{0}])";
    l.empty_array = "np.empty(0)";
    l.unpack_states = "";
    l.read_state = "{0} = _y[{1}]";
    l.slice = "_y[{0}:{1}]";
    l.index_range = "np.arange({0}, {1}, {2})";
    l.slice_value = "np.broadcast_to({0}, ({1},))";
    l.parts = NUMPY_PARTS;
    return l;
}
constexpr Language PYTHON_NUMPY = numpy_language();

} // namespace

const Language &python_language() { return PYTHON; }
const Language &python_numpy_language() { return PYTHON_NUMPY; }
const Language &python_numba_language() { return PYTHON_NUMBA; }

} // namespace xpp
