/* The model export's translator (export_model.h): the model's expression trees, as model_native.h's model_ir gives them, written
   through a Language. The trees carry indices; this file gives them back the model's names (renamed where the language reserves
   them), writes the statements of each program in the language's syntax, and fills the language's template. No formula text is read. */
#include "export_model.h"

#include "comline.h"
#include "expr.h"
#include "expr_ir.h"
#include "expr_program.h"
#include "model.h"
#include "model_added_columns.h"
#include "model_load.h"
#include "model_native.h"
#include "model_numerics.h"
#include "model_options.h"
#include "model_place.h"
#include "odex.h"
#include "session.h"
#include "xpp_fs.h"
#include "xpp_log.h"
#include "xpp_path.h"
#include "xpp_text.h"
#include "xpp_ui.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace xpp {
namespace {

/* An expression nests this deep at most: each level is a recursion of the writer and a parenthesis in the file, and Python's parser
   refuses more than 200 of those; no model's formula comes near it. */
constexpr std::size_t MAX_EXPRESSION_DEPTH = 100;
/* One expression's text is this long at most, so a model whose trees share much (a user function used in a user function ...) cannot
   make a file of its own size squared */
constexpr std::size_t MAX_EXPRESSION_CHARS = std::size_t{1} << 20;

using Values = std::vector<std::pair<std::string_view, std::string>>;

/* pattern with each {name} that values has replaced by its text (a brace that names no value stays as it is) */
std::string substitute(std::string_view pattern, const Values &values)
{
    std::string out;
    for (std::size_t i = 0; i < pattern.size();) {
        if (pattern[i] == '{') {
            const std::size_t close = pattern.find('}', i);
            if (close != std::string_view::npos) {
                const std::string_view name = pattern.substr(i + 1, close - i - 1);
                const auto found = std::ranges::find_if(values, [&](const auto &value) { return value.first == name; });
                if (found != values.end()) {
                    out += found->second;
                    i = close + 1;
                    continue;
                }
            }
        }
        out += pattern[i++];
    }
    return out;
}

/* a spelling with its arguments in {0}, {1}, {2} */
std::string spell(std::string_view pattern, std::initializer_list<std::string_view> arguments)
{
    Values values;
    std::vector<std::string> numbers; /* the keys' text, alive as long as values */
    numbers.reserve(arguments.size());
    for (std::size_t i = 0; i < arguments.size(); ++i) numbers.push_back(std::to_string(i));
    std::size_t i = 0;
    for (const std::string_view argument : arguments) {
        values.emplace_back(numbers[i], std::string(argument));
        ++i;
    }
    return substitute(pattern, values);
}

std::string joined(const std::vector<std::string> &parts)
{
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += (i ? ", " : "") + parts[i];
    return out;
}

/* The names a script uses: the model's, each written as the language can (a character a name cannot have becomes `_`), and a
   name the language reserves, or one already given, gets `_` and then a number: del -> del_, del_1, ... The renames are kept for the file's header. */
class Names {
public:
    explicit Names(const Language &language) : taken_(language.reserved.begin(), language.reserved.end()) {}

    /* a model's name as the script writes it */
    std::string model(std::string_view name)
    {
        std::string written = unique(name);
        if (written != name) renames_.emplace_back(std::string(name), written);
        return written;
    }
    /* a name the script makes up (a temporary, an argument), unlike any other */
    std::string made(std::string_view base) { return unique(base); }
    const std::vector<std::pair<std::string, std::string>> &renames() const { return renames_; }

private:
    std::string unique(std::string_view name)
    {
        std::string clean;
        for (const char c : name) clean += is_word_char(c) ? c : '_';
        if (clean.empty() || (clean[0] >= '0' && clean[0] <= '9')) clean.insert(clean.begin(), '_');
        std::string candidate = clean;
        if (taken_.contains(candidate)) {
            candidate = clean + "_";
            for (int n = 1; taken_.contains(candidate); ++n) candidate = clean + "_" + std::to_string(n);
        }
        taken_.insert(candidate);
        return candidate;
    }
    std::set<std::string, std::less<>> taken_;
    std::vector<std::pair<std::string, std::string>> renames_;
};

/* what a function of the module reads and calls, found once from its trees */
struct Reads {
    std::set<int> variables;
    std::set<int> constants;
    std::set<std::size_t> functions; /* the user functions it calls, by ir.functions */
};

/* the things a run does that the script does not, which no tree of an equation shows: refused first, with the model line or the setting */
Result<> check_model(const SessionParts &s, const Language &language)
{
    const Model &m_ = s.model();
    const SessionParts &s_ = s;
    const NumericsSettings &n = *s_.numerics;
    const auto refuse = [&](std::string what, Place place) -> Result<> {
        return fail("export model", xpp::format("{} cannot be written in {}", what, language.name), std::move(place));
    };
    if (m_.node == 0) return refuse("a model with no differential equation", Place{m_.this_file});
    if (m_.nmarkov > 0) return refuse("Markov variables", model_place(m_, m_.markov.front().name));
    if (m_.nnetwork > 0) return refuse("networks", model_place(m_, m_.networks.front().name));
    if (m_.compiler.nkernel > 0) return refuse("integral equations", m_.compiler.kernels.front().where);
    if (m_.nsvar > 0) return refuse("algebraic solve variables (a DAE)", m_.svars.front().where);
    if (m_.naeqn > 0) return refuse("algebraic equations (a DAE)", m_.aeqns.front().where);
    if (m_.flag_count() > 0) return refuse("global flags (events)", model_statement_place(m_, odex::Statement::Kind::Event, 0));
    if (!m_.wiener.empty()) return refuse("Wiener noise", model_place(m_, m_.parameter_name(m_.wiener.front())));
    if (!m_.ou.empty()) return refuse("an Ornstein-Uhlenbeck process", model_place(m_, m_.ou.front().name));
    if (n.method == method::DISCRETE || n.method == method::VOLTERRA)
        return refuse(xpp::format("a model run as a {} ({})", n.method == method::DISCRETE ? "map" : "set of integral equations", named_settings(s_, {"integration.method"})),
                      settings_place(m_, {"integration.method"}));
    if (n.poincare_map != PoincareMap::none)
        return refuse(xpp::format("a Poincare map ({})", named_settings(s_, {"poincare.map"})), settings_place(m_, {"poincare.map"}));
    if (n.torus) return refuse("variables folded onto a torus", settings_place(m_, {"integration.torus.on", "integration.torus.fold", "integration.torus.period"}));
    return {};
}

class Translation {
public:
    Translation(const SessionParts &s, const Language &language, const ModelIr &model)
        : s_(s), m_(s.model()), lang_(language), mir_(model), ir_(model.ir), names_(language)
    {
    }
    Result<std::string> script();

private:
    /* the equations of an array line that one expression writes: copies whose trees have one shape, their variables and numbers
       stepping by a constant from copy to copy (`steps`, by node of the first copy's tree: only those that step) */
    struct Run {
        std::size_t first, count; /* the equations, from mir_.equations[first] */
        std::size_t line;         /* mir_.arrays */
        std::unordered_map<int, double> steps;
    };
    /* the place an error is at, and where an expression is written */
    struct Frame {
        const Place *place;
        std::size_t loops = 0;   /* the sums open around it */
        bool variables = true;   /* the run's variables are in scope (not so for a derived parameter) */
        const Run *run = nullptr; /* the equations written at once: its steps make slices and ranges */
    };

    std::unexpected<Error> cannot(const Frame &frame, std::string_view what) const
    {
        return fail("export model", xpp::format("{} cannot be written in {}", what, lang_.name), *frame.place);
    }
    void scan();
    void find_runs();
    /* the pairing of two trees made by `alike` */
    struct Pairing {
        std::set<std::pair<int, int>> seen;
        std::unordered_map<int, double> steps;
    };
    bool alike(int a, int b, std::size_t depth, Pairing &pairing) const;
    bool alike(const IrFunction &a, const IrFunction &b, Pairing &pairing) const;
    /* a function that only names values, in order (the statements that hold a call's value): no loop, no conditional */
    static bool temporaries_only(const IrFunction &f)
    {
        return f.loops == 0 && std::ranges::all_of(f.statements, [](const IrStatement &st) { return st.kind == IrStatement::Kind::Temporary; });
    }
    std::string slice(int variable, std::size_t count) const;
    std::string range(double first, double step, std::size_t count) const;
    Result<> name_everything();
    Result<std::string> expression(int node, Frame &frame, std::size_t depth);
    Result<std::string> call(const IrNode &node, Frame &frame, std::size_t depth);
    Result<std::string> number(double x) const;
    Result<std::string> math_function(const IrCallee &callee, const std::vector<std::string> &arguments, const Frame &frame);
    /* function f's statements at `depth` levels in, appended to out */
    Result<> statements(const IrFunction &f, Frame &frame, std::size_t depth, std::string &out);
    /* function f's statements, then its value assigned to `target` */
    Result<> program(std::size_t function, const std::string &target, Frame &frame, std::size_t depth, std::string &out);
    Result<std::string> user_function(std::size_t function);
    std::string line(std::size_t depth, std::string_view text) const
    {
        std::string out;
        for (std::size_t i = 0; i < depth; ++i) out += lang_.indent;
        out += text;
        out += '\n';
        return out;
    }
    /* text as comment lines. A model's name or file name may hold any character: a control one (a lone carriage return ends a
       Python comment) becomes a space, so what a name says can never be a line of the program. */
    std::string comment_block(std::string_view text) const
    {
        std::string out;
        const std::string whole(text); /* split_lines' parts point into it */
        for (const std::string_view part : split_lines(whole)) {
            out += lang_.comment;
            for (const char c : part) out += static_cast<unsigned char>(c) < ' ' || c == '\x7f' ? ' ' : c;
            out += '\n';
        }
        return out;
    }
    const std::string &made(const char *kind, int index)
    {
        const auto [found, added] = made_.emplace(std::string(kind) + std::to_string(index), std::string());
        if (added) found->second = names_.made(found->first);
        return found->second;
    }
    /* the name the formula language has for the symbol with this com: a built-in function's, or a constant's the model made */
    std::string symbol_name(int com) const
    {
        for (int i = 0; i < s_.parser.nsym; ++i)
            if (s_.parser.symbols[static_cast<std::size_t>(i)].com == com) return s_.parser.symbols[static_cast<std::size_t>(i)].name;
        return {};
    }
    const std::string &constant_name(int index) const { return constant_names_.at(index); }
    std::string variable_name(int index) const { return variable_names_.at(static_cast<std::size_t>(index)); }
    std::string argument_list(std::size_t function, const std::vector<std::string> &arguments) const;
    /* a variable the statements being written read as a name: the state vector's places a script that reads each one (read_state) unpacks */
    void note_read(int variable)
    {
        if (variable >= 1 && variable <= m_.node) read_states_.insert(variable);
    }
    /* how the body just written gets the states it reads, as lines at depth 1 (the one line that reads them all, for a language that does) */
    std::string unpack() const;
    std::string part(std::string_view name) const;
    /* a list of values as the language writes one */
    std::string array_of(const std::vector<std::string> &items) const { return items.empty() ? std::string(lang_.empty_array) : spell(lang_.array_literal, {joined(items)}); }

    const SessionParts &s_;
    const Model &m_;
    const Language &lang_;
    const ModelIr &mir_;
    const IrModule &ir_;
    Names names_;
    std::vector<Reads> reads_;                               /* by ir.functions: its own and, for a variable, through the functions it calls */
    std::unordered_map<std::string, std::size_t> function_by_callee_; /* a user function's callee name -> its ir.functions */
    std::set<std::size_t> reached_;                          /* the user functions some program calls */
    std::vector<char> array_safe_;                           /* by ir.functions: a user function a slice may call (an expression of elementwise operations alone) */
    std::vector<Run> runs_;                                  /* in the equations' order */
    std::set<int> read_states_;                              /* the states the body being written reads by name */
    Values parts_;                                           /* the language's parts of the template, their own parts filled in */
    std::vector<std::string> variable_names_;                /* by the trees' variable index: time, the states, the fixed variables */
    std::map<int, std::string> constant_names_;              /* by the trees' constant index */
    std::map<std::size_t, std::string> function_names_;      /* by ir.functions */
    std::map<std::string, std::string> made_;                /* the names the script made up, by what they are for */
    std::unordered_map<int, std::string> symbol_names_;      /* a math function's name by its symbol, cached */
};

/* what each function reads and calls, and which user functions the programs reach */
void Translation::scan()
{
    reads_.assign(ir_.functions.size(), {});
    for (std::size_t f = 0; f < ir_.functions.size(); ++f) {
        const IrFunction &function = ir_.functions[f];
        if (function.kind == IrFunction::Kind::User) function_by_callee_.emplace(function.name, f);
    }
    for (std::size_t f = 0; f < ir_.functions.size(); ++f) {
        const IrFunction &function = ir_.functions[f];
        Reads &reads = reads_[f];
        std::vector<int> pending;
        std::unordered_set<int> seen;
        const auto push = [&](int node) {
            if (node >= 0 && seen.insert(node).second) pending.push_back(node);
        };
        for (const IrStatement &st : function.statements) {
            push(st.value);
            push(st.other);
        }
        push(function.result);
        while (!pending.empty()) {
            const IrNode &node = ir_.nodes[static_cast<std::size_t>(pending.back())];
            pending.pop_back();
            if (node.op == IrOp::Variable) reads.variables.insert(node.index);
            else if (node.op == IrOp::Constant) reads.constants.insert(node.index);
            else if (node.op == IrOp::Call) {
                for (const int argument : ir_.arguments(node)) push(argument);
                const IrCallee &callee = ir_.callees[static_cast<std::size_t>(node.callee)];
                if (!callee.address)
                    if (const auto user = function_by_callee_.find(callee.name); user != function_by_callee_.end()) reads.functions.insert(user->second);
                continue;
            }
            for (const int argument : node.args) push(argument);
        }
    }
    /* a function reads what the functions it calls read: passes until nothing grows (no recursion on a model's own chain of calls) */
    for (bool grew = true; grew;) {
        grew = false;
        for (Reads &reads : reads_)
            for (const std::size_t callee : std::set<std::size_t>(reads.functions)) {
                const std::size_t before = reads.variables.size() + reads.functions.size();
                reads.variables.insert(reads_[callee].variables.begin(), reads_[callee].variables.end());
                reads.functions.insert(reads_[callee].functions.begin(), reads_[callee].functions.end());
                if (reads.variables.size() + reads.functions.size() != before) grew = true;
            }
    }
    /* a user function a slice may call: its statements only name values, and so are those of every function it calls (reads_ holds all it reaches) */
    array_safe_.assign(ir_.functions.size(), 0);
    for (std::size_t f = 0; f < ir_.functions.size(); ++f)
        array_safe_[f] = temporaries_only(ir_.functions[f]) && std::ranges::all_of(reads_[f].functions, [&](std::size_t g) { return temporaries_only(ir_.functions[g]); });
    const auto reach = [&](std::size_t program) { reached_.insert(reads_[program].functions.begin(), reads_[program].functions.end()); };
    for (const auto *list : {&mir_.equations, &mir_.fixed, &mir_.aux})
        for (const std::size_t program : *list) reach(program);
    for (const ModelIr::Derived &d : mir_.derived) reach(d.function);
}

/* whether the trees at a and b have one shape: the same operations, constants and callees, their variables and numbers differing
   by the steps the pairing records (the state a slice reads is the next one, a number is a whole number). A node is paired the same way
   wherever the trees share it; the walk is as deep as an expression may be written. */
bool Translation::alike(int a, int b, std::size_t depth, Pairing &pairing) const
{
    if (depth > MAX_EXPRESSION_DEPTH) return false;
    if (!pairing.seen.emplace(a, b).second) return true;
    const IrNode &x = ir_.nodes[static_cast<std::size_t>(a)], &y = ir_.nodes[static_cast<std::size_t>(b)];
    if (x.op != y.op) return false;
    const auto stepped = [&](double change) {
        const auto [at, added] = pairing.steps.emplace(a, change);
        return added || at->second == change;
    };
    const auto whole = [](double v) { return std::isfinite(v) && v == std::trunc(v) && std::fabs(v) < 1e9; };
    switch (x.op) {
    case IrOp::Number:
        if (std::bit_cast<std::uint64_t>(x.number) == std::bit_cast<std::uint64_t>(y.number)) return stepped(0.0);
        return whole(x.number) && whole(y.number) && stepped(y.number - x.number);
    case IrOp::Variable: {
        const double change = static_cast<double>(y.index) - static_cast<double>(x.index);
        if (change == 0) return stepped(0.0);
        return change > 0 && x.index >= 1 && y.index <= m_.node && stepped(change); /* the states, in order */
    }
    case IrOp::Constant: case IrOp::Temporary: return x.index == y.index;
    case IrOp::Call: {
        if (x.callee != y.callee || x.index != y.index || x.args[1] != y.args[1]) return false;
        const IrCallee &callee = ir_.callees[static_cast<std::size_t>(x.callee)];
        if (!callee.address) { /* a user function: one a slice may call */
            const auto user = function_by_callee_.find(callee.name);
            if (user == function_by_callee_.end() || !array_safe_[user->second]) return false;
        }
        const std::span<const int> left = ir_.arguments(x), right = ir_.arguments(y);
        for (std::size_t i = 0; i < left.size(); ++i)
            if (!alike(left[i], right[i], depth + 1, pairing)) return false;
        return true;
    }
    case IrOp::Add: case IrOp::Subtract: case IrOp::Multiply: case IrOp::Divide: case IrOp::GuardedDivide: case IrOp::Select:
        for (std::size_t i = 0; i < x.args.size(); ++i) {
            if ((x.args[i] < 0) != (y.args[i] < 0)) return false;
            if (x.args[i] >= 0 && !alike(x.args[i], y.args[i], depth + 1, pairing)) return false;
        }
        return true;
    default: return false; /* a temporary, an argument, a loop's counter, a network's operand: no equation of one expression has one */
    }
}

/* two equations' programs of one shape: the same temporaries, then the same value */
bool Translation::alike(const IrFunction &a, const IrFunction &b, Pairing &pairing) const
{
    if (!temporaries_only(a) || !temporaries_only(b) || a.statements.size() != b.statements.size()) return false;
    for (std::size_t i = 0; i < a.statements.size(); ++i)
        if (a.statements[i].index != b.statements[i].index || !alike(a.statements[i].value, b.statements[i].value, 0, pairing)) return false;
    return alike(a.result, b.result, 0, pairing);
}

/* the steps of a pairing that move (a node that is the same in both copies has none) */
std::unordered_map<int, double> moving(const std::unordered_map<int, double> &steps)
{
    std::unordered_map<int, double> out;
    for (const auto &[node, change] : steps)
        if (change != 0) out.emplace(node, change);
    return out;
}

/* The runs of each array line of the model (mir_.arrays): from a copy on, the next copies that are the first's tree with its
   variables and numbers stepped by the same amount again (the line's index, and the neighbours it names: v[j-1], v[j+1]). The
   copies at an end of the line, which read a fixed variable (v0, v201), are the scalars the language writes besides. */
void Translation::find_runs()
{
    if (lang_.slice.empty()) return;
    const auto function = [&](std::size_t equation) -> const IrFunction & { return ir_.functions[mir_.equations[equation]]; };
    for (std::size_t line = 0; line < mir_.arrays.size(); ++line) {
        const std::size_t end = mir_.arrays[line].first + mir_.arrays[line].count;
        for (std::size_t i = mir_.arrays[line].first; i < end;) {
            Run run{i, 1, line, {}};
            Pairing pairing;
            if (i + 1 < end && alike(function(i), function(i + 1), pairing)) {
                run.steps = moving(pairing.steps);
                /* a slice reads neighbouring states: a variable moves to the next one */
                const bool next_state = std::ranges::all_of(run.steps, [&](const auto &step) { return ir_.nodes[static_cast<std::size_t>(step.first)].op != IrOp::Variable || step.second == 1; });
                if (!run.steps.empty() && next_state) run.count = 2;
            }
            /* the copy n steps n times as much as the second copy, node for node */
            for (; run.count > 1 && i + run.count < end; ++run.count) {
                Pairing next;
                if (!alike(function(i), function(i + run.count), next)) break;
                const auto now = moving(next.steps);
                if (now.size() != run.steps.size() ||
                    !std::ranges::all_of(run.steps, [&](const auto &step) {
                        const auto found = now.find(step.first);
                        return found != now.end() && found->second == step.second * static_cast<double>(run.count);
                    }))
                    break;
            }
            if (run.count > 1) runs_.push_back(run);
            i += run.count;
        }
    }
}

std::string Translation::slice(int variable, std::size_t count) const
{
    const std::size_t first = static_cast<std::size_t>(variable - 1); /* the state vector has no time */
    return spell(lang_.slice, {std::to_string(first), std::to_string(first + count)});
}

std::string Translation::range(double first, double step, std::size_t count) const
{
    return spell(lang_.index_range, {*number(first), *number(first + step * static_cast<double>(count)), *number(step)});
}

std::string Translation::unpack() const
{
    std::string out;
    if (!lang_.unpack_states.empty()) {
        std::vector<std::string> names;
        for (int i = 1; i <= m_.node; ++i) names.push_back(variable_name(i));
        return line(1, spell(lang_.unpack_states, {joined(names)}));
    }
    for (const int variable : read_states_) out += line(1, spell(lang_.read_state, {variable_name(variable), std::to_string(variable - 1)}));
    return out;
}

std::string Translation::part(std::string_view name) const
{
    const auto found = std::ranges::find_if(parts_, [&](const auto &p) { return p.first == name; });
    return found == parts_.end() ? std::string() : found->second;
}

/* the names of everything the script writes, in the model's order */
Result<> Translation::name_everything()
{
    variable_names_.push_back(names_.model("t"));
    for (int i = 0; i < m_.node; ++i) variable_names_.push_back(names_.model(m_.uvar_names[static_cast<std::size_t>(i)]));
    for (int i = 0; i < m_.fix_var; ++i) variable_names_.push_back(names_.model(m_.fixinfo[static_cast<std::size_t>(i)].name));
    /* the parameters in the model's order, then any other constant the trees read (PI, a derived parameter, the sum's index) */
    for (int i = 0; i < m_.nupar; ++i) constant_names_.emplace(m_.upar_con[static_cast<std::size_t>(i)], names_.model(m_.upar_names[static_cast<std::size_t>(i)]));
    std::set<int> others;
    for (const Reads &reads : reads_) others.insert(reads.constants.begin(), reads.constants.end());
    for (const ModelIr::Derived &d : mir_.derived) others.insert(d.constant);
    for (const int index : others) {
        if (constant_names_.contains(index)) continue;
        const std::string name = symbol_name(COM(CONTYPE, index));
        if (name.empty()) return fail("export model", xpp::format("the model reads the constant {}, which has no name", index), Place{m_.this_file});
        constant_names_.emplace(index, names_.model(name));
    }
    for (const std::size_t f : reached_) function_names_.emplace(f, names_.model(mir_.origin[f].name));
    return {};
}

Result<std::string> Translation::number(double x) const
{
    if (std::isnan(x)) return std::string(lang_.number_not_a_number);
    if (std::isinf(x)) return std::string(x < 0 ? "-" : "") + std::string(lang_.number_infinity);
    std::string text = xpp::number(x);
    if (text.find_first_of(".en") == std::string::npos) text += ".0"; /* a float, whatever the language does with a whole number */
    return text;
}

Result<std::string> Translation::math_function(const IrCallee &callee, const std::vector<std::string> &arguments, const Frame &frame)
{
    auto [cached, added] = symbol_names_.emplace(callee.symbol, std::string());
    if (added) cached->second = symbol_name(callee.symbol);
    const std::string &name = cached->second;
    if (name.empty()) return cannot(frame, "an internal function of the formula language");
    const auto spelling = std::ranges::find(lang_.functions, name, &Spelling::name);
    if (spelling == lang_.functions.end()) return cannot(frame, lower_case(name) + "()");
    Values values;
    std::vector<std::string> keys;
    for (std::size_t i = 0; i < arguments.size(); ++i) keys.push_back(std::to_string(i));
    for (std::size_t i = 0; i < arguments.size(); ++i) values.emplace_back(keys[i], arguments[i]);
    return substitute(spelling->text, values);
}

std::string Translation::argument_list(std::size_t function, const std::vector<std::string> &arguments) const
{
    std::vector<std::string> all = arguments;
    for (const int variable : reads_[function].variables) all.push_back(variable_name(variable));
    return joined(all);
}

Result<std::string> Translation::call(const IrNode &node, Frame &frame, std::size_t depth)
{
    const IrCallee &callee = ir_.callees[static_cast<std::size_t>(node.callee)];
    std::vector<std::string> arguments;
    for (const int argument : ir_.arguments(node)) {
        Result<std::string> text = expression(argument, frame, depth + 1);
        if (!text) return text;
        arguments.push_back(std::move(*text));
    }
    if (!callee.address) { /* a user function */
        const std::size_t function = function_by_callee_.at(callee.name);
        if (!frame.variables && !reads_[function].variables.empty())
            return cannot(frame, "a function of the run's variables in a derived parameter");
        for (const int variable : reads_[function].variables) note_read(variable);
        return function_names_.at(function) + "(" + argument_list(function, arguments) + ")";
    }
    if (callee.symbol >= 0) return math_function(callee, arguments, frame);
    return cannot(frame, callee.what.empty() ? callee.name : callee.what);
}

Result<std::string> Translation::expression(int node_index, Frame &frame, std::size_t depth)
{
    if (depth > MAX_EXPRESSION_DEPTH) return cannot(frame, xpp::format("a formula nested more than {} levels deep", MAX_EXPRESSION_DEPTH));
    const IrNode &node = ir_.nodes[static_cast<std::size_t>(node_index)];
    const auto operands = [&](std::string_view pattern, int count) -> Result<std::string> {
        std::array<std::string, 3> text;
        for (int i = 0; i < count; ++i) {
            Result<std::string> operand = expression(node.args[static_cast<std::size_t>(i)], frame, depth + 1);
            if (!operand) return operand;
            text[static_cast<std::size_t>(i)] = std::move(*operand);
        }
        return spell(pattern, {text[0], text[1], text[2]});
    };
    Result<std::string> text;
    switch (node.op) {
    case IrOp::Number: /* a negative one in parentheses: the language's own minus may bind otherwise */
        if (frame.run)
            if (const auto stepping = frame.run->steps.find(node_index); stepping != frame.run->steps.end()) {
                text = range(node.number, stepping->second, frame.run->count);
                break;
            }
        text = number(node.number);
        if (text && std::signbit(node.number)) text = "(" + *text + ")";
        break;
    case IrOp::Constant:
        if (node.index == expr::SUM_INDEX && frame.loops == 0) return cannot(frame, "the sum's index outside its sum");
        text = constant_name(node.index);
        break;
    case IrOp::Variable:
        if (!frame.variables) return cannot(frame, "a derived parameter that reads a variable");
        if (frame.run)
            if (const auto stepping = frame.run->steps.find(node_index); stepping != frame.run->steps.end()) {
                text = slice(node.index, frame.run->count);
                break;
            }
        note_read(node.index);
        text = variable_name(node.index);
        break;
    case IrOp::Temporary: text = made("tmp", node.index); break;
    case IrOp::Argument: text = made("a", node.index); break;
    case IrOp::LoopCounter: text = spell(lang_.loop_counter, {made("k", node.index)}); break;
    case IrOp::Operand: return cannot(frame, "a network's operand");
    case IrOp::Add: text = operands(lang_.add, 2); break;
    case IrOp::Subtract: text = operands(lang_.subtract, 2); break;
    case IrOp::Multiply: text = operands(lang_.multiply, 2); break;
    case IrOp::Divide: text = operands(lang_.divide, 2); break;
    case IrOp::GuardedDivide: text = operands(lang_.guarded_divide, 2); break;
    case IrOp::Select: text = operands(lang_.select, 3); break;
    case IrOp::Call: text = call(node, frame, depth); break;
    }
    if (text && text->size() > MAX_EXPRESSION_CHARS) return cannot(frame, "a formula that expands to more than a million characters");
    return text;
}

Result<> Translation::statements(const IrFunction &f, Frame &frame, std::size_t depth, std::string &out)
{
    const std::size_t loops_before = frame.loops;
    std::vector<std::size_t> opened; /* the length of out when each open block began: a block that wrote nothing is `empty_block` */
    const auto close = [&](std::size_t level) {
        if (out.size() == opened.back() && !lang_.empty_block.empty()) out += line(level, lang_.empty_block);
        opened.pop_back();
    };
    for (const IrStatement &st : f.statements) {
        switch (st.kind) {
        case IrStatement::Kind::Temporary: {
            const Result<std::string> value = expression(st.value, frame, 0);
            if (!value) return std::unexpected(value.error());
            out += line(depth, spell(lang_.assign, {made("tmp", st.index), *value}));
            break;
        }
        case IrStatement::Kind::Constant: { /* the sum's index, set at the head of its loop */
            const Result<std::string> value = expression(st.value, frame, 0);
            if (!value) return std::unexpected(value.error());
            out += line(depth, spell(lang_.assign, {constant_name(st.index), *value}));
            break;
        }
        case IrStatement::Kind::LoopBegin: {
            const Result<std::string> low = expression(st.value, frame, 0), high = expression(st.other, frame, 0);
            if (!low) return std::unexpected(low.error());
            if (!high) return std::unexpected(high.error());
            out += line(depth, spell(lang_.assign, {made("tmp", st.slot), *number(0.0)}));
            out += line(depth, spell(lang_.loop_begin, {made("k", st.index), *low, *high}));
            opened.push_back(out.size());
            ++depth;
            ++frame.loops;
            break;
        }
        case IrStatement::Kind::LoopEnd: {
            const Result<std::string> value = expression(st.value, frame, 0);
            if (!value) return std::unexpected(value.error());
            const std::string &total = made("tmp", st.slot);
            out += line(depth, spell(lang_.assign, {total, spell(lang_.add, {total, *value})}));
            close(depth);
            --depth;
            --frame.loops;
            if (!lang_.loop_end.empty()) out += line(depth, lang_.loop_end);
            break;
        }
        case IrStatement::Kind::IfBegin: {
            const Result<std::string> condition = expression(st.value, frame, 0);
            if (!condition) return std::unexpected(condition.error());
            out += line(depth, spell(lang_.if_zero, {*condition}));
            opened.push_back(out.size());
            ++depth;
            break;
        }
        case IrStatement::Kind::IfElse:
            close(depth);
            --depth;
            out += line(depth, lang_.else_branch);
            opened.push_back(out.size());
            ++depth;
            break;
        case IrStatement::Kind::IfEnd:
            close(depth);
            --depth;
            if (!lang_.if_end.empty()) out += line(depth, lang_.if_end);
            break;
        default: return cannot(frame, "a statement of the compiled right-hand side"); /* Output, Variable, ReturnWithoutOutput: a program has none */
        }
    }
    frame.loops = loops_before;
    return {};
}

Result<> Translation::program(std::size_t function, const std::string &target, Frame &frame, std::size_t depth, std::string &out)
{
    const IrFunction &f = ir_.functions[function];
    if (Result<> written = statements(f, frame, depth, out); !written) return written;
    const Result<std::string> value = expression(f.result, frame, 0);
    if (!value) return std::unexpected(value.error());
    out += line(depth, spell(lang_.assign, {target, frame.run ? spell(lang_.slice_value, {*value, std::to_string(frame.run->count)}) : *value}));
    return {};
}

Result<std::string> Translation::user_function(std::size_t function)
{
    const IrFunction &f = ir_.functions[function];
    Frame frame{&mir_.origin[function].place};
    std::vector<std::string> parameters;
    for (int i = 0; i < f.doubles; ++i) parameters.push_back(made("a", i));
    std::string out = line(0, spell(lang_.function_begin, {function_names_.at(function), argument_list(function, parameters)}));
    if (Result<> body = statements(f, frame, 1, out); !body) return std::unexpected(body.error());
    const Result<std::string> value = expression(f.result, frame, 0);
    if (!value) return std::unexpected(value.error());
    out += line(1, spell(lang_.function_return, {*value}));
    if (!lang_.function_end.empty()) out += line(0, lang_.function_end);
    return out;
}

Result<std::string> Translation::script()
{
    Values own;
    for (const Spelling &piece : lang_.parts) own.emplace_back(piece.name, std::string(piece.text));
    parts_ = own;
    for (auto &[name, text] : parts_) text = substitute(text, own); /* a part may use another's name */
    scan();
    if (Result<> named = name_everything(); !named) return std::unexpected(named.error());
    find_runs();
    /* a fixed variable reads the ones set before it (a later one holds a value no step of the script has made) */
    for (std::size_t j = 0; j < mir_.fixed.size(); ++j)
        for (const int variable : reads_[mir_.fixed[j]].variables)
            if (variable >= m_.node + 1 + static_cast<int>(j) && variable <= m_.node + m_.fix_var)
                return fail("export model",
                            xpp::format("{} cannot be written in {}: it reads the fixed variable {}, which is set after it", m_.fixinfo[j].name, lang_.name, variable_name(variable)),
                            mir_.origin[mir_.fixed[j]].place);

    /* the parameters, then the other constants the trees read, then the derived parameters */
    std::string parameters = comment_block("Parameters, with the values the model had.");
    for (int i = 0; i < m_.nupar; ++i) {
        const int k = m_.upar_con[static_cast<std::size_t>(i)];
        parameters += line(0, spell(lang_.assign, {constant_name(k), *number(s_.parser.constants[static_cast<std::size_t>(k)])}));
    }
    std::set<int> derived_constants;
    for (const ModelIr::Derived &d : mir_.derived) derived_constants.insert(d.constant);
    std::set<int> parameter_constants(m_.upar_con.begin(), m_.upar_con.begin() + m_.nupar);
    for (const auto &[k, name] : constant_names_)
        if (k != expr::SUM_INDEX && !parameter_constants.contains(k) && !derived_constants.contains(k))
            parameters += line(0, spell(lang_.assign, {name, *number(s_.parser.constants[static_cast<std::size_t>(k)])}));
    std::string derived;
    if (!mir_.derived.empty()) derived += comment_block("Derived parameters, worked out from the parameters above.");
    for (const ModelIr::Derived &d : mir_.derived) {
        Frame frame{&mir_.origin[d.function].place, 0, false};
        if (Result<> written = program(d.function, constant_name(d.constant), frame, 0, derived); !written) return std::unexpected(written.error());
    }
    std::string functions;
    for (const std::size_t f : reached_) {
        Result<std::string> text = user_function(f);
        if (!text) return std::unexpected(text.error());
        functions += (functions.empty() ? "" : "\n") + *text;
    }

    /* the right-hand side: the fixed variables in order, then each equation; the aux columns after them the same way */
    std::string rhs_body, aux_body;
    std::vector<std::string> derivatives, aux_values;
    const auto fixed_variables = [&](std::string &out) -> Result<> {
        for (std::size_t j = 0; j < mir_.fixed.size(); ++j) {
            Frame frame{&mir_.origin[mir_.fixed[j]].place};
            if (Result<> written = program(mir_.fixed[j], variable_name(m_.node + 1 + static_cast<int>(j)), frame, 1, out); !written) return written;
        }
        return {};
    };
    read_states_.clear();
    if (Result<> fixed = fixed_variables(rhs_body); !fixed) return std::unexpected(fixed.error());
    for (std::size_t i = 0; i < mir_.equations.size();) {
        const auto run = std::ranges::find(runs_, i, &Run::first);
        Frame frame{&mir_.origin[mir_.equations[i]].place};
        derivatives.push_back(made("d", static_cast<int>(i)));
        if (run != runs_.end()) frame.run = &*run; /* the copies are one program, over slices */
        if (Result<> written = program(mir_.equations[i], derivatives.back(), frame, 1, rhs_body); !written) return std::unexpected(written.error());
        i += run == runs_.end() ? 1 : run->count;
    }
    if (!rhs_body.empty()) rhs_body.pop_back(); /* the template ends the line */
    rhs_body = unpack() + rhs_body;
    read_states_.clear();
    if (Result<> fixed = fixed_variables(aux_body); !fixed) return std::unexpected(fixed.error());
    for (std::size_t i = 0; i < mir_.aux.size(); ++i) {
        Frame frame{&mir_.origin[mir_.aux[i]].place};
        aux_values.push_back(made("out", static_cast<int>(i)));
        if (Result<> written = program(mir_.aux[i], aux_values.back(), frame, 1, aux_body); !written) return std::unexpected(written.error());
    }
    if (!aux_body.empty()) aux_body.pop_back();
    aux_body = unpack() + aux_body;

    std::vector<std::string> initial, columns;
    for (int i = 0; i < m_.node; ++i) initial.push_back(*number(s_.last_ic[static_cast<std::size_t>(i)]));
    for (int j = 0; j <= m_.neq; ++j) {
        std::string quoted;
        json_append_string(quoted, data_column_name(m_, {}, j));
        columns.push_back(std::move(quoted));
    }

    const NumericsSettings &n = *s_.numerics;
    std::string header = comment_block(xpp::format("xppautX export of \"{}\" as {}.", files::split_path(m_.this_file).second, lang_.name));
    header += comment_block("The equations are the model's and the numbers are those it had when this file was written; " + part("imports_note") + ".\n"
                            "It integrates with the classic fixed-step Runge-Kutta method (RK4) at step DT and writes the rows xppautX stores (T, each variable, then each\n"
                            "auxiliary quantity) to a CSV file beside itself, one row every STORE_EVERY steps.");
    if (n.method != method::RK4)
        header += comment_block(xpp::format("The model's own method is {} (integration.method): the numbers differ from xppautX's for that reason.", method_info(n.method).name));
    if (!runs_.empty()) {
        std::string written;
        for (const Run &run : runs_) {
            const ModelIr::ArrayLine &array = mir_.arrays[run.line];
            const int low = array.low + array.step * static_cast<int>(run.first - array.first);
            written += xpp::format("{}{}..{} ({} = {}..{})", written.empty() ? "" : ", ", m_.uvar_names[run.first], m_.uvar_names[run.first + run.count - 1], array.index, low,
                                   low + array.step * static_cast<int>(run.count - 1));
        }
        header += comment_block("Equations written as slices, each run from one array line of the model: " + written);
    }
    if (!names_.renames().empty()) {
        std::string renamed;
        for (const auto &[from, to] : names_.renames()) renamed += (renamed.empty() ? "" : ", ") + from + " -> " + to;
        header += comment_block("Names written differently, because " + std::string(lang_.name) + " reserves them or cannot spell them: " + renamed);
    }
    header.pop_back();

    const auto trimmed = [](std::string block) {
        if (!block.empty() && block.back() == '\n') block.pop_back();
        return block;
    };
    const Values values{
        {"header", header},
        {"start_time", *number(n.start_time)},
        {"dt", *number(n.delta_t)},
        {"total_time", *number(n.total_time)},
        {"store_every", std::to_string(n.store_every)},
        {"store_from", *number(n.store_from)},
        {"state_limit", n.stop_at_limit ? *number(n.state_limit) : std::string(lang_.number_infinity)},
        {"zero_divisor", *number(expr::ZERO_DIVISOR)},
        {"initial", array_of(initial)},
        {"columns", joined(columns)},
        {"parameters", trimmed(parameters)},
        {"derived", trimmed(derived)},
        {"functions", trimmed(functions)},
        {"rhs_body", rhs_body},
        {"derivatives", array_of(derivatives)},
        {"aux_body", aux_body},
        {"aux_values", array_of(aux_values)},
    };
    Values all = values;
    all.insert(all.end(), parts_.begin(), parts_.end());
    return substitute(lang_.file, all);
}

} // namespace

const Language *language_named(std::string_view name)
{
    for (const Language *language : {&python_language(), &python_numpy_language(), &python_numba_language()})
        if (language->name == name) return language;
    return nullptr;
}

Result<std::string> export_to_file(FileSystem &fs, const SessionParts &s, const Language &language, std::string_view file)
{
    if (Result<> checked = check_model(s, language); !checked) return std::unexpected(checked.error());
    const Result<ModelIr> model = model_ir(s);
    if (!model) return std::unexpected(model.error());
    const Result<std::string> text = Translation(s, language, *model).script();
    if (!text) return std::unexpected(text.error());
    Result<> opened;
    const std::unique_ptr<Sink> sink = open_sink_asking(fs, file, false, &opened);
    if (!opened) return std::unexpected(opened.error());
    if (!sink) return fail("export model", xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), command_place());
    if (Result<> written = sink->write(*text); !written) {
        abort_save(*sink);
        return std::unexpected(written.error());
    }
    if (Result<> saved = commit_save(*sink); !saved) return std::unexpected(saved.error());
    return std::string(file);
}

Result<std::string> export_model(Session &s, const Language &language, std::string_view file)
{
    std::string name(file);
    if (name.empty()) {
        name = files::output_name(s.model().this_file, language.extension);
        if (!file_selector("Export model", name, xpp::format("*{}", language.extension))) return std::string();
    }
    return export_to_file(*s.model().fs, s, language, name);
}

int convert_to_language(FileSystem &fs, const ModelLoader &loader, const std::string &model, const Language &language, const LoadServices &services)
{
    if (!odex::is_odex(model))
        if (const int status = odex::convert_file(fs, loader, model, services); status != 0) return status;
    const std::string odex_file = odex::is_odex(model) ? model : odex::odex_name(model);
    const Result<CommandLine> line = parse_command_line({"xppautX", odex_file, "--convert"});
    if (!line) { show_error(line.error()); return 1; }
    const auto [folder, base] = files::split_path(odex_file);
    const std::string named = files::output_name(base, language.extension);
    const std::string out = folder.empty() ? named : files::join(folder, named);
    Result<std::string> written = std::string();
    /* the model read as a load reads an .odex; the file is written once it is whole (inspect's check, which finds nothing wrong) */
    const Result<> read = loader.inspect(fs, *line, loader.read_odex, [&](const SessionParts &s) -> std::optional<Error> {
        written = export_to_file(fs, s, language, out);
        return std::nullopt;
    }, nullptr, &services);
    if (!read) written = std::unexpected(read.error());
    if (!written) {
        xpp::log(XPP_LOG_ERROR, "{}\n", written.error().text());
        return 1;
    }
    xpp::log(XPP_LOG_INFO, "wrote {}\n", out);
    return 0;
}

} // namespace xpp
