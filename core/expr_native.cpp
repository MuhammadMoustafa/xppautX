/* The sole owner of translating validated RPN into trusted C. No model text
   enters the translation; every emitted name and subscript is an index. */
#include "expr_native.h"
#include "expr_decode.h"
#include "expr_derivative.h"
#include "expr_program.h"
#include "jacobian.h"
#include "session.h"
#include "simplenet.h"
#include "tabular.h"
#include "volterra2.h"
#include "xpp_tcc.h"
#include "xpp_util.h"
#include "xpp_io.h"
#include "xpp_math.h"
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <unordered_map>
#include <string_view>
#include <algorithm>
#include "xpp_log.h"
#include "xpp_globals.h"

namespace xpp {
namespace {
struct Node {
    std::string text;
    bool atom = false; /* a name, subscript or literal: may be repeated in the C */
    bool mut = false;  /* reads state a later statement may change (a variable, the sum's index) */
    int power = 0;     /* a literal whole n INTEGER_POWER multiplies out (math::is_integer_power), else 0 */
};

/* A conditional being read: cond is the tested value, base the stack depth
   under the branches, mark and then_mark the statements written so far at
   the MYIF and at the MYTHEN. */
struct Frame {
    Node cond, then_value;
    size_t base;
    size_t mark = 0, then_mark = 0;
};

/* Sums nest this deep at most: each level is one recursion of the
   translator, and a model's own sums nest two or three. */
constexpr int MAX_SUM_DEPTH = 16;

/* The interpreter's own helpers the generated code calls, as C names and
   prototypes: the code does the same operations in the same order and
   these do the arithmetic, so the results are the interpreter's, bit for
   bit. The first argument of each is the Session. */
enum class Helper : size_t { Table, TableSlope, Network, Vector, Kernel, Delay, DelayShift, Shift, IShift, Set, Uniform, Poisson, Normal, Argument, FunctionEnd, Count };
struct HelperText {
    std::string_view name, prototype;
};
constexpr std::array<HelperText, static_cast<size_t>(Helper::Count)> helper_text{{
    {"h_tab", "double h_tab(void*,double,int)"},
    {"h_tabs", "double h_tabs(void*,double,int)"},
    {"h_net", "double h_net(void*,double,int)"},
    {"h_vec", "double h_vec(void*,double,int)"},
    {"h_ker", "double h_ker(void*,int)"},
    {"h_del", "double h_del(void*,double,double)"},
    {"h_dsh", "double h_dsh(void*,double,double,double)"},
    {"h_sft", "double h_sft(void*,double,double)"},
    {"h_ish", "double h_ish(double,double)"},
    {"h_set", "double h_set(void*,double,double,double)"},
    {"h_ru", "double h_ru(void*,double)"},
    {"h_rp", "double h_rp(void*,double)"},
    {"h_rn", "double h_rn(void*,double,double)"},
    {"h_arg", "double h_arg(void*,int)"},
    {"h_end", "double h_end(void*,double,int)"},
}};

/* The helpers with exactly the C prototypes above, so the generated code
   calls each through its own type; S is the Session the native function
   was given. */
Session &session(void *S) { return *static_cast<Session *>(S); }
double h_tab(void *S, double x, int i) { return lookup(session(S), x, i); }
double h_tabs(void *S, double x, int i) { return lookup_slope(session(S), x, i); }
double h_net(void *S, double x, int i) { return network_value(session(S), x, i); }
double h_vec(void *S, double x, int i) { return vector_value(session(S), x, i); }
double h_ker(void *S, int i) { return ker_val(session(S), i); }
double h_del(void *S, double delay, double i) { return expr::do_delay(session(S), delay, i); }
double h_dsh(void *S, double delay, double shift, double variable) { return expr::do_delay_shift(session(S), delay, shift, variable); }
double h_sft(void *S, double shift, double variable) { return expr::do_shift(session(S), shift, variable); }
double h_set(void *S, double shift, double variable, double value) { return expr::do_set(session(S), shift, variable, value); }
double h_ru(void *S, double x) { return expr::do_random_uniform(session(S), x); }
double h_rp(void *S, double mean) { return expr::do_random_poisson(session(S), mean); }
double h_rn(void *S, double mean, double sd) { return expr::do_random_normal(session(S), mean, sd); }

double h_arg(void *S,int i) {const auto &stack=session(S).parser.stack;return stack.args[stack.nargs-1-i];}
double h_end(void *S,double value,int n) {session(S).parser.stack.nargs-=n;return value;}

const void *helper_address(Helper h)
{
    switch (h) {
    case Helper::Table: return reinterpret_cast<const void *>(&h_tab);
    case Helper::TableSlope: return reinterpret_cast<const void *>(&h_tabs);
    case Helper::Network: return reinterpret_cast<const void *>(&h_net);
    case Helper::Vector: return reinterpret_cast<const void *>(&h_vec);
    case Helper::Kernel: return reinterpret_cast<const void *>(&h_ker);
    case Helper::Delay: return reinterpret_cast<const void *>(&h_del);
    case Helper::DelayShift: return reinterpret_cast<const void *>(&h_dsh);
    case Helper::Shift: return reinterpret_cast<const void *>(&h_sft);
    case Helper::IShift: return reinterpret_cast<const void *>(&expr::do_ishift);
    case Helper::Set: return reinterpret_cast<const void *>(&h_set);
    case Helper::Uniform: return reinterpret_cast<const void *>(&h_ru);
    case Helper::Poisson: return reinterpret_cast<const void *>(&h_rp);
    case Helper::Normal: return reinterpret_cast<const void *>(&h_rn);
    case Helper::Argument: return reinterpret_cast<const void *>(&h_arg);
    case Helper::FunctionEnd: return reinterpret_cast<const void *>(&h_end);
    case Helper::Count: break;
    }
    return nullptr;
}

/* A user function whose body is one C expression of its arguments (no
   statement, temporary or loop) is written into each call instead of
   called: TinyCC does not inline, and the XPP division guard
   (ode_divisor) is called for every division of a converted model. A body
   longer than this many characters stays a call, so nested expansions
   stay small. */
constexpr size_t MAX_INLINE_TEXT = 512;

/* body with each of its argument names a0, a1, ... (whole identifiers: the
   text has no other name of that form) replaced by the argument's text */
std::string with_arguments(std::string_view body, const std::vector<std::string> &arguments)
{
    const auto word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    std::string text;
    for (size_t i = 0; i < body.size();) {
        size_t end = i + 1;
        if (body[i] == 'a' && (i == 0 || !word(body[i - 1])))
            while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end]))) ++end;
        if (end > i + 1 && (end == body.size() || !word(body[end]))) {
            text += arguments.at(std::stoul(std::string(body.substr(i + 1, end - i - 1))));
            i = end;
        } else {
            text += body[i++];
        }
    }
    return text;
}

struct Generator {
    Session &s;
    Place place;
    std::string source;
    std::set<int> unary, binary;
    std::vector<std::set<int>> calls;
    std::vector<char> pure_function; /* a user function written so far that changes no state */
    std::vector<std::string> inline_text; /* a user function's body as one expression, empty when it is called */
    std::vector<double> numbers{expr::ZERO_DIVISOR}; /* N[0]: the zero divisor's value, bit-exact */
    std::unordered_map<std::uint64_t, size_t> literal_slot; /* a literal's bits: its N[], so one value has one name */
    /* the function being written: the pure calls already made at its top
       level, by their C text, each held in a temporary (pure_call) */
    std::unordered_map<std::string, Node> calls_made;
    /* the right-hand side's programs whose function the one right-hand-side
       function calls (rhs_value), each with its symbol */
    std::map<const Program *, std::string> called;
    /* an index over calls_made, so a variable's write finds the calls that
       may read it without scanning them all (W559): each call's text by
       the variables it names as v[k] or reaches through a pure user
       function, and the calls that may read any variable. An entry
       whose call was forgotten is skipped when it is looked up. */
    std::unordered_map<int, std::vector<std::string>> call_readers;
    std::vector<std::string> call_readers_any;
    std::array<bool, static_cast<size_t>(Helper::Count)> used{};
    /* the function being written: its statements in order, how many
       temporaries and loop counters it declares, how deep in sums it is,
       and whether it changes state or draws (its callers then call it
       as a statement) */
    std::vector<std::string> out;
    int temps = 0, loops = 0, depth = 0;
    bool impure = false;
    bool array_pure = true;
    std::vector<char> array_pure_function;
    std::set<int> variable_reads;
    std::vector<std::set<int>> function_reads;
    bool dynamic = false; /* a network updates its first two variable operands */
    std::vector<char> allowed; /* user functions with a fully compiled dependency graph */
    std::unordered_map<int, int> number_slot; /* a derivative graph's number: its N[] */

    std::unexpected<Error> refuse(std::string reason) const
    {
        return fail("compile", "not compiled: " + reason + "; running the interpreter", place);
    }

    std::string signature(int i, bool user, std::string_view name={}) const
    {
        std::string text = "double " + (user ? xpp::format("u{}",i) : std::string(name)) + "(double *c,double *v,void *S";
        if (!user) text+=",const int *R";
        if (user)
            for (int a = 0; a < s.model().narg_fun[i]; ++a) text += xpp::format(",double a{}", a);
        return text + ")";
    }

    /* the helper's C name, declared in the program */
    std::string_view helper(Helper h)
    {
        used[static_cast<size_t>(h)] = true;
        if (h != Helper::Table && h != Helper::TableSlope && h != Helper::IShift) array_pure = false;
        return helper_text[static_cast<size_t>(h)].name;
    }

    /* The interpreter does a state-changing operation where its token is,
       so the generated code does it as a statement of its own, in order,
       and what the stack holds that a statement may change is first
       written to a temporary. */
    Node statement(std::string value)
    {
        Node t{xpp::format("t{}", temps++), true};
        out.push_back(xpp::format("{}={};", t.text, value));
        return t;
    }
    void flush(std::vector<Node> &stack, size_t from)
    {
        for (size_t i = from; i < stack.size(); ++i)
            if (stack[i].mut) stack[i] = statement(std::move(stack[i].text));
    }
    Node effect(std::vector<Node> &stack, size_t from, std::string call)
    {
        flush(stack, from);
        impure = true;
        forget_calls();
        return statement(std::move(call));
    }

    /* A literal's N[] slot: the same value, the same slot, so two writings
       of one expression have one text. */
    Node literal(double x)
    {
        const auto [slot, added] = literal_slot.emplace(std::bit_cast<std::uint64_t>(x), numbers.size());
        if (added) numbers.push_back(x);
        return {xpp::format("N[{}]", slot->second), true, false, math::is_integer_power(x) ? static_cast<int>(x) : 0};
    }

    /* The call text of a math function (a table's fun1/fun2: pure, they
       read and change no state) as a node. A call made before in this
       function, with the same text, is its temporary again; else, at the
       function's top level (in no conditional or sum, whose statements
       may not run), it is written once to a temporary now. The same
       operations on the same values in the same order: the call is only
       made once instead of twice. A state-changing statement forgets the
       calls made (effect, a sum), since their text may read what it
       changes. */
    Node pure_call(std::string text, bool mut, bool top)
    {
        if (depth > 0) return {std::move(text), false, mut};
        if (const auto made = calls_made.find(text); made != calls_made.end()) return made->second;
        if (!top) return {std::move(text), false, mut};
        Node t = statement(text);
        t.mut = false; /* a value of the state at its statement: never rewritten */
        index_call(text);
        calls_made.emplace(std::move(text), t);
        return t;
    }

    /* x^n for a literal whole n (INTEGER_POWER), multiplied out as the
       interpreter does (math::power_by_squaring), each product a
       temporary, so x and each square are computed once */
    Node integer_power(const Node &x, int n)
    {
        const Node a = x.atom ? x : statement(x.text);
        return math::power_by_squaring(a, n, [&](const Node &p, const Node &q) { return statement("(" + p.text + "*" + q.text + ")"); });
    }

    /* The two branches met (other is the else value, already off the
       stack): a conditional expression when neither wrote a statement,
       else an if with a temporary for the value. */
    void join(std::vector<Node> &stack, const Frame &f, const Node &other, size_t outer_base)
    {
        if (out.size() == f.mark) {
            stack.push_back({xpp::format("({}==0.0?{}:{})", f.cond.text, other.text, f.then_value.text), false,
                             f.cond.mut || other.mut || f.then_value.mut});
            return;
        }
        const std::vector<std::string> branches(out.begin() + static_cast<std::ptrdiff_t>(f.mark), out.end());
        out.resize(f.mark);
        flush(stack, outer_base);
        const std::string result = xpp::format("t{}", temps++);
        const auto split = branches.begin() + static_cast<std::ptrdiff_t>(f.then_mark - f.mark);
        out.push_back(xpp::format("if({}==0.0){{", f.cond.text));
        out.insert(out.end(), split, branches.end());
        out.push_back(xpp::format("{}={};}}else{{", result, other.text));
        out.insert(out.end(), branches.begin(), split);
        out.push_back(xpp::format("{}={};}}", result, f.then_value.text));
        stack.push_back({result, true});
    }

    /* The value of the instructions from k up to the end of the program
       or of a sum's body (its ENDSUM), as one C expression: the
       interpreter's stack is simulated over trees, so each operation is
       done on the operands the interpreter pops, in its order, and TinyCC
       keeps temporaries in registers instead of a stack array. The
       instructions are expr_decode.h's, already checked: only what C
       cannot be written for is refused here. k ends after the terminator.
       Iterative; only a sum's body is a recursion, MAX_SUM_DEPTH deep. */
    Result<Node> sequence(const std::vector<expr::Instr> &code, size_t &k, bool user, int index)
    {
        std::vector<Node> stack;
        std::vector<Frame> frames;
        auto lower = [&] { return frames.empty() ? size_t{0} : frames.back().base; };
        auto pop = [&] { Node n = std::move(stack.back()); stack.pop_back(); return n; };
        while (k < code.size()) {
            const expr::Instr &i = code[k++];
            const int op = i.op, type = i.type, in = i.in;
            if (op == expr::JOIN) {
                const Frame f = std::move(frames.back());
                frames.pop_back();
                const Node other = pop();
                join(stack, f, other, lower());
            } else if (op == ENDSUM) {
                return pop();
            } else if (op == NUMSYM) {
                stack.push_back(literal(i.number));
            } else if (op == MYIF) {
                Node cond = pop();
                frames.push_back({std::move(cond), {}, stack.size()});
                frames.back().mark = out.size();
            } else if (op == MYTHEN) {
                Frame &f = frames.back();
                f.then_value = pop();
                f.then_mark = out.size();
            } else if (op == INDXCOM) {
                stack.push_back(literal(expr::CURRENT_INDEX));
            } else if (op == SUMSYM) {
                if (depth >= MAX_SUM_DEPTH) return refuse("sums nested too deep");
                const Node high = pop(), low = pop();
                flush(stack, lower());
                impure = true;
                forget_calls();
                const int n = loops++;
                const Node total{xpp::format("t{}", temps++), true};
                /* lo, hi and ix are int: the assignments truncate the bounds and
                   widen the index as the interpreter's casts do */
                out.push_back(xpp::format("lo{0}={1};hi{0}={2};{3}=0.0;if(lo{0}<=hi{0}){{for(ix{0}=lo{0};;ix{0}++){{c[{4}]=ix{0};",
                                          n, low.text, high.text, total.text, expr::SUM_INDEX));
                ++depth;
                auto value = sequence(code, k, user, index);
                --depth;
                if (!value) return value;
                out.push_back(xpp::format("{0}={0}+{1};if(ix{2}==hi{2})break;}}}}", total.text, value->text, n));
                stack.push_back(total);
            } else if (op == ENDDELAY || op == ENDSHIFT || op == ENDISHIFT) {
                const Node first = pop(), second = pop();
                if (op == ENDISHIFT) {
                    stack.push_back({xpp::format("{}({},{})", helper(Helper::IShift), first.text, second.text), false, first.mut || second.mut});
                } else if (op == ENDSHIFT) {
                    stack.push_back({xpp::format("{}(S,{},{})", helper(Helper::Shift), first.text, second.text), false, true});
                } else {
                    stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{})", helper(Helper::Delay), first.text, second.text)));
                }
            } else if (op == ENDDELSHFT) {
                const Node tau = pop(), shift = pop(), variable = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{},{})", helper(Helper::DelayShift), tau.text, shift.text, variable.text)));
            } else if (op == ENDSET) {
                const Node value = pop(), shift = pop(), variable = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{},{})", helper(Helper::Set), shift.text, variable.text, value.text)));
            } else if (op == RANDUNI || op == RANDPOI) {
                const Node x = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{})", helper(op == RANDUNI ? Helper::Uniform : Helper::Poisson), x.text)));
            } else if (op == RANDNORM) {
                const Node sd = pop(), mean = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{})", helper(Helper::Normal), mean.text, sd.text)));
            } else if (type == SVARTYPE || type == SCONTYPE) {
                /* the variable or constant a delay, shift or set names, as the number the interpreter pushes */
                stack.push_back({xpp::format("{}.0", type == SCONTYPE ? COM(CONTYPE, in) : COM(VARTYPE, in)), true});
            } else if (type == CONTYPE || type == VARTYPE) {
                if (dynamic && i.pc<2) {
                    if (type!=VARTYPE) return refuse("an invalid network operand");
                    /* The network changes these tokens during each pair evaluation.
                       do_shift bounds the selected variable just as the interpreter
                       does for a computed index; no model name enters C. */
                    stack.push_back({xpp::format("{}(S,0,R[{}])",helper(Helper::Shift),i.pc),false,true});
                } else {
                    if (type == VARTYPE) variable_reads.insert(in);
                    if (type == CONTYPE && in == expr::SUM_INDEX && depth == 0) array_pure = false;
                    stack.push_back({(type == CONTYPE ? "c[" : "v[") + std::to_string(in) + "]", true, type == VARTYPE || in == expr::SUM_INDEX});
                }
            } else if (type == USTACKTYPE) {
                stack.push_back({"a" + std::to_string(in), true});
            } else if (type == FUN1TYPE) {
                unary.insert(in);
                const Node x = pop();
                stack.push_back(pure_call(xpp::format("f1_{}({})", in, x.text), x.mut, frames.empty()));
            } else if (type == FUN2TYPE) {
                const Node right = pop(), left = pop();
                std::string text;
                if (in == expr::INTEGER_POWER && right.power) {
                    stack.push_back(integer_power(left, right.power));
                    continue;
                }
                if (in == expr::ADD || in == expr::MULTIPLY) text = "(" + right.text + (in == expr::ADD ? "+" : "*") + left.text + ")";
                else if (in == expr::SUBTRACT || in == expr::IEEE_DIVIDE) text = "(" + left.text + (in == expr::SUBTRACT ? "-" : "/") + right.text + ")";
                else if (in == expr::DIVIDE) {
                    /* the guarded division: a zero divisor becomes N[0] */
                    if (right.atom) text = xpp::format("({}/({}==0.0?N[0]:{}))", left.text, right.text, right.text);
                    else text = xpp::format("dv({},{})", left.text, right.text);
                } else {
                    binary.insert(in);
                    stack.push_back(pure_call(xpp::format("f2_{}({},{})", in, left.text, right.text), left.mut || right.mut, frames.empty()));
                    continue;
                }
                stack.push_back({std::move(text), false, left.mut || right.mut});
            } else if (type == UFUNTYPE) {
                if (!allowed.empty() && !allowed[in]) return refuse("calls a refused user function");
                if (user) calls[index].insert(in);
                const size_t first = stack.size() - static_cast<size_t>(i.count);
                std::string text;
                /* inlined: each argument once, a name as it is, an expression in a temporary that
                   the same expression shares (pure_call; only where a statement may be written) */
                std::vector<std::string> arguments;
                if (!inline_text[in].empty())
                    for (size_t a = first; a < stack.size(); ++a) {
                        const Node argument = stack[a].atom ? stack[a] : pure_call(stack[a].text, stack[a].mut, frames.empty());
                        if (!argument.atom) { arguments.clear(); break; }
                        arguments.push_back(argument.text);
                    }
                if (arguments.size() == static_cast<size_t>(i.count) && !inline_text[in].empty()) {
                    text = with_arguments(inline_text[in], arguments);
                } else {
                    text = xpp::format("u{}(c,v,S", in);
                    for (size_t a = first; a < stack.size(); ++a) text += "," + stack[a].text;
                    text += ")";
                }
                stack.resize(first);
                if (!user) {
                    if (!array_pure_function[in]) array_pure = false;
                    variable_reads.insert(function_reads[in].begin(),function_reads[in].end());
                }
                stack.push_back(pure_function[in] ? Node{std::move(text), false, true} : effect(stack, lower(), std::move(text)));
            } else if (type == KERTYPE) {
                stack.push_back({xpp::format("{}(S,{})", helper(Helper::Kernel), in), false});
            } else {
                /* a lookup table, network or vectorizer: the arrays the helpers index are the interpreter's own */
                const Node x = pop();
                const Helper h = type == TABTYPE ? Helper::Table : type == NETTYPE ? Helper::Network : Helper::Vector;
                stack.push_back({xpp::format("{}(S,{},{})", helper(h), x.text, in), false, type != TABTYPE || x.mut});
            }
        }
        return pop();
    }

    /* A new function: no statements, temporaries, loops or calls made yet. */
    void begin()
    {
        out.clear();
        forget_calls();
        temps = loops = depth = 0;
        impure = false;
        array_pure = true;
        variable_reads.clear();
    }

    /* Program p's value in the function being written, its statements
       appended to out. */
    Result<Node> value(const std::vector<int> &p, int index, bool user)
    {
        if (p.empty()) return refuse("an invalid program length");
        const auto code = expr::decode(s, p, user ? s.model().narg_fun[index] : -1);
        if (!code) return refuse(code.error());
        size_t k = 0;
        return sequence(*code, k, user, index);
    }

    /* The function being written into the source: head, then its
       declarations and statements, then tail. */
    void write(std::string_view head, std::string_view tail)
    {
        std::string declarations;
        for (int i = 0; i < temps; ++i) declarations += xpp::format("{}t{}", i ? "," : "double ", i);
        if (temps) declarations += ";";
        for (int i = 0; i < loops; ++i) declarations += xpp::format("{1}lo{0},hi{0},ix{0}", i, i ? "," : "int ");
        if (loops) declarations += ";";
        source += xpp::format("{}{{{}", head, declarations);
        for (const std::string &statement : out) source += statement;
        source += xpp::format("{}}}\n", tail);
    }

    /* One C function for a program (an equation's, or user function index's). */
    Result<> program(const std::vector<int> &p, int index, bool user, std::string_view name={})
    {
        begin();
        const auto result = value(p, index, user);
        if (!result) return std::unexpected(result.error());
        write(signature(index, user, name), xpp::format("return {};", result->text));
        if (user) {
            pure_function[index] = !impure;
            if (!impure && out.empty() && temps == 0 && loops == 0 && result->text.size() <= MAX_INLINE_TEXT) inline_text[index] = result->text;
        }
        return {};
    }

    /* The calls made are all forgotten. */
    void forget_calls()
    {
        calls_made.clear();
        call_readers.clear();
        call_readers_any.clear();
    }

    /* Index a call's text by what it may read: the variables it names as
       v[k], and through a call given the Session S (the only S in the
       text): a user function's that reads k or what is not its arguments or
       variables (array_pure_function) reads those it lists, and a helper's
       other than a table's may read any variable. */
    void index_call(const std::string &text)
    {
        for (size_t at = text.find("v["); at != std::string::npos; at = text.find("v[", at + 1)) {
            int k = 0;
            const auto [end, error] = std::from_chars(text.data() + at + 2, text.data() + text.size(), k);
            if (error == std::errc() && end < text.data() + text.size() && *end == ']') call_readers[k].push_back(text);
        }
        for (size_t at = text.find('S'); at != std::string::npos; at = text.find('S', at + 1)) {
            const size_t open = text.rfind('(', at);
            size_t start = open;
            while (start > 0 && (std::isalnum(static_cast<unsigned char>(text[start - 1])) || text[start - 1] == '_')) --start;
            const std::string_view call = std::string_view(text).substr(start, open - start);
            if (call == helper_text[static_cast<size_t>(Helper::Table)].name || call == helper_text[static_cast<size_t>(Helper::TableSlope)].name) continue;
            if (call.starts_with('u')) {
                const size_t f = std::stoul(std::string(call.substr(1)));
                if (array_pure_function[f]) {
                    for (int k : function_reads[f]) call_readers[k].push_back(text);
                    continue;
                }
            }
            call_readers_any.push_back(text);
            return;
        }
    }

    /* Writing variable k forgets the calls whose text may read it. */
    void forget_calls_reading(int k)
    {
        for (const std::string &text : call_readers_any) calls_made.erase(text);
        call_readers_any.clear();
        if (const auto readers = call_readers.find(k); readers != call_readers.end()) {
            for (const std::string &text : readers->second) calls_made.erase(text);
            call_readers.erase(readers);
        }
    }

    /* The right-hand side's program p's value in the function being
       written: the function already compiled for it (W703: called when it has
       a sum, whose run is long beside the call, and so not written and
       compiled twice), else p written inline, so a math call
       two programs make on the same values is made once. A call is a
       statement, as an effect is: it forgets the calls made. A program
       that is not dynamic reads no R. */
    Result<Node> rhs_value(const Program &p)
    {
        const auto function = called.find(&p);
        if (function == called.end()) return value(p.rpn, 0, false);
        forget_calls();
        return statement(xpp::format("{}(c,v,S,0)", function->second));
    }

    /* One C function, name, for the right-hand side in its order: each
       fixed program's value written into its variable, first and on, as
       fixed_values (my_rhs.cpp) sets them one by one (W468), then, when
       the caller gives o, each equation's value into o[i], as
       equation_values does (W469); so a math call two of them make on the
       same values is made once. Writing a variable forgets the calls whose
       text may read it (forget_calls_reading); nothing reads o. The
       programs in called are called, not written (rhs_value). */
    Result<> rhs(std::span<const Program> fixed, int first, std::span<const Program> equations, std::string_view name)
    {
        begin();
        for (size_t i = 0; i < fixed.size(); ++i) {
            const auto result = rhs_value(fixed[i]);
            if (!result) return std::unexpected(result.error());
            const int variable = first + static_cast<int>(i);
            out.push_back(xpp::format("v[{}]={};", variable, result->text));
            forget_calls_reading(variable);
        }
        if (!equations.empty()) out.push_back("if(!o)return;");
        for (size_t i = 0; i < equations.size(); ++i) {
            const auto result = rhs_value(equations[i]);
            if (!result) return std::unexpected(result.error());
            out.push_back(xpp::format("o[{}]={};", i, result->text));
        }
        write(xpp::format("void {}(double *c,double *v,void *S,double *o)", name), "");
        return {};
    }

    /* One C function, name, writing the value of terms[k] into o[k]
       (W260): each node of the graph they need once, in the graph's order
       (a node's arguments first), into the workspace w, of which it uses
       the first `workspace` doubles. */
    void derivatives(const expr::Symbolic &g, std::span<const expr::Term> terms, std::string_view name, int &workspace)
    {
        std::vector<char> needed(g.size(), 0);
        std::vector<int> pending;
        for (const expr::Term t : terms) pending.push_back(t.id);
        while (!pending.empty()) {
            const int id = pending.back();
            pending.pop_back();
            if (needed[static_cast<size_t>(id)]) continue;
            needed[static_cast<size_t>(id)] = 1;
            for (int a : g.node({id}).args)
                if (a >= 0) pending.push_back(a);
        }
        std::unordered_map<int, int> slot;
        int products = 0; /* the locals an INTEGER_POWER's products are kept in */
        auto text = [&](int id) {
            const expr::Symbolic::Node &n = g.node({id});
            switch (n.op) {
            case expr::Symbolic::Op::Number: {
                const auto [it, added] = number_slot.emplace(id, static_cast<int>(numbers.size()));
                if (added) numbers.push_back(n.number);
                return xpp::format("N[{}]", it->second);
            }
            case expr::Symbolic::Op::Constant: return xpp::format("c[{}]", n.index);
            case expr::Symbolic::Op::Variable: return xpp::format("v[{}]", n.index);
            default: return xpp::format("w[{}]", slot.at(id));
            }
        };
        source += xpp::format("void {}(const double*c,const double*v,void*S,double*w,double*o){{", name);
        for (size_t id = 0; id < g.size(); ++id) {
            if (!needed[id]) continue;
            const expr::Symbolic::Node &n = g.node({static_cast<int>(id)});
            std::string value;
            switch (n.op) {
            case expr::Symbolic::Op::Number:
            case expr::Symbolic::Op::Constant:
            case expr::Symbolic::Op::Variable:
                continue;
            case expr::Symbolic::Op::Fun1:
                unary.insert(n.index);
                value = xpp::format("f1_{}({})", n.index, text(n.args[0]));
                break;
            case expr::Symbolic::Op::Fun2: {
                const std::string x = text(n.args[0]), y = text(n.args[1]);
                switch (n.index) {
                case expr::ADD: value = xpp::format("({}+{})", x, y); break;
                case expr::SUBTRACT: value = xpp::format("({}-{})", x, y); break;
                case expr::MULTIPLY: value = xpp::format("({}*{})", x, y); break;
                case expr::DIVIDE: value = xpp::format("dv({},{})", x, y); break;
                case expr::IEEE_DIVIDE: value = xpp::format("({}/{})", x, y); break;
                case expr::INTEGER_POWER:
                    /* x^n multiplied out as the interpreter does, each product a local */
                    if (const expr::Symbolic::Node &e = g.node({n.args[1]});
                        e.op == expr::Symbolic::Op::Number && math::is_integer_power(e.number)) {
                        value = math::power_by_squaring(x, static_cast<int>(e.number), [&](const std::string &p, const std::string &q) {
                            std::string t = xpp::format("q{}", products++);
                            source += xpp::format("double {}=({}*{});", t, p, q);
                            return t;
                        });
                        break;
                    }
                    [[fallthrough]];
                default:
                    binary.insert(n.index);
                    value = xpp::format("f2_{}({},{})", n.index, x, y);
                }
                break;
            }
            case expr::Symbolic::Op::Select:
                value = xpp::format("({}==0.0?{}:{})", text(n.args[0]), text(n.args[2]), text(n.args[1]));
                break;
            case expr::Symbolic::Op::Table:
            case expr::Symbolic::Op::TableSlope:
                value = xpp::format("{}(S,{},{})", helper(n.op == expr::Symbolic::Op::Table ? Helper::Table : Helper::TableSlope), text(n.args[0]), n.index);
                break;
            }
            const int k = static_cast<int>(slot.size());
            slot.emplace(static_cast<int>(id), k);
            source += xpp::format("w[{}]={};", k, value);
        }
        for (size_t k = 0; k < terms.size(); ++k) source += xpp::format("o[{}]={};", k, text(terms[k].id));
        source += "}\n";
        workspace = std::max(workspace, static_cast<int>(slot.size()));
    }

    /* the source written so far compiled by TinyCC, with the numbers and
       the functions and helpers it calls; its errors at place */
    Result<std::unique_ptr<tcc::Program>> build(const Place &place) const
    {
        /* Literals are host doubles in N[], never model text in generated C. */
        std::string declarations=xpp::format("double N[{}];\nstatic double dv(double a,double b){{if(b==0.0)b=N[0];return a/b;}}\n",numbers.size());
        std::vector<tcc::Symbol> symbols;
        std::vector<std::string> names;
        std::vector<const void *> addresses;
        for (int i:unary) {
            names.push_back(xpp::format("f1_{}",i));
            declarations+=xpp::format("extern double f1_{}(double);\n",i);
            addresses.push_back(xpp::math::resolve(expr::fun1[i]));
        }
        for (int i:binary) {
            names.push_back(xpp::format("f2_{}",i));
            declarations+=xpp::format("extern double f2_{}(double,double);\n",i);
            addresses.push_back(xpp::math::resolve(expr::fun2[i]));
        }
        for (size_t i=0;i<used.size();++i) {
            if (!used[i]) continue;
            names.emplace_back(helper_text[i].name);
            declarations+=xpp::format("extern {};\n",helper_text[i].prototype);
            addresses.push_back(helper_address(static_cast<Helper>(i)));
        }
        for (size_t i=0;i<names.size();++i) symbols.push_back({names[i].c_str(),addresses[i]});
        auto compiled=tcc::Program::compile(declarations+source,symbols,place);
        if (!compiled) return std::unexpected(compiled.error());
        auto table=(*compiled)->function("N");
        if (!table) return std::unexpected(table.error());
        std::copy(numbers.begin(),numbers.end(),static_cast<double *>(*table));
        return std::move(*compiled);
    }
};
}

std::vector<ModelProgram> model_programs(Model &m)
{
    std::vector<ModelProgram> entries;
    auto add=[&](Program &p,std::string symbol,Place place,int user=-1,bool dynamic=false) {
        if (p.rpn.empty()) return;
        if (place.source.empty()) place.source=model_source_line(m,place.file,place.line);
        entries.push_back({&p,std::move(symbol),std::move(place),user,dynamic});
    };
    for (size_t i=0;i<m.programs.size();++i) {
        if (m.programs[i].rpn.empty()) continue;
        const std::string &name=i<static_cast<size_t>(m.node)?m.uvar_names[i]
            :i<static_cast<size_t>(m.node+m.fix_var)?m.fixinfo[i-m.node].name:m.uvar_names[i-m.fix_var];
        add(m.programs[i],xpp::format("p_eq{}",i),model_place(m,name));
    }
    for (size_t i=0;i<m.ufun_programs.size();++i)
        if (!m.ufun_programs[i].rpn.empty()) add(m.ufun_programs[i],xpp::format("p_fun{}",i),model_place(m,m.ufun_names[i]),static_cast<int>(i));
    for (size_t i=0;i<m.svars.size();++i) add(m.svars[i].form,xpp::format("p_solv{}",i),m.svars[i].where);
    for (size_t i=0;i<m.aeqns.size();++i) add(m.aeqns[i].form,xpp::format("p_dae{}",i),m.aeqns[i].where);
    for (size_t i=0;i<m.markov.size();++i)
        for (size_t j=0;j<m.markov[i].command.size();++j)
            add(m.markov[i].command[j],xpp::format("p_markov{}_{}",i,j),model_place(m,m.markov[i].name));
    for (size_t i=0;i<m.kernels.size();++i) {
        add(m.kernels[i].formula,xpp::format("p_kernel{}",i),m.kernels[i].where);
        add(m.kernels[i].kerform,xpp::format("p_convolution{}",i),m.kernels[i].where);
    }
    for (size_t i=0;i<m.flags.size();++i) {
        if (m.flags[i].comcond.rpn.empty()) continue;
        const Place place=model_place(m,odex::Statement::Kind::Event,static_cast<int>(i));
        add(m.flags[i].comcond,xpp::format("p_condition{}",i),place);
        for (size_t j=0;j<m.flags[i].comrhs.size();++j) add(m.flags[i].comrhs[j],xpp::format("p_event{}_{}",i,j),place);
    }
    for (size_t i=0;i<m.derived.size();++i) add(m.derived[i].form,xpp::format("p_derived{}",i),m.derived[i].where);
    for (size_t i=0;i<m.networks.size();++i)
        if (!m.networks[i].f.rpn.empty()) add(m.networks[i].f,xpp::format("p_network{}",i),model_place(m,m.networks[i].name),-1,true);
    return entries;
}

namespace {
/* Why the model's right-hand side is not pure (Model::rhs_impurity), or
   "": every program native and reading only the constants, the variables
   and the immutable lookup tables, nothing running between the fixed
   variables and the equations. Networks and DAEs run between them even when
   no equation calls their helpers; function tables can rebuild on parameter
   changes (or when autoeval is toggled), so they are never frozen. */
std::string rhs_impurity(const Session &s)
{
    const Model &m=s.model();
    if (m.nsvar>0) return "algebraic solve variables (a DAE)";
    if (m.ndelays>0) return "delays";
    if (m.naeqn>0) return "algebraic equations";
    if (m.nnetwork>0) return "networks";
    if (m.nmarkov>0) return "Markov variables";
    if (m.nkernel>0) return "integral equations";
    for (int i=0;i<s.ntable;++i) if (s.tables[i].flag==2) return "a function table that is rebuilt";
    for (int i=0;i<m.node+m.fix_var;++i) {
        const auto &p=m.programs[i];
        const int available=i<m.node?m.node+m.fix_var:i;
        if (!p.native && !p.in_rhs) return "a right-hand side that is not compiled";
        if (!p.array_pure) return "a right-hand side that uses shifts, random numbers or sums over an index";
        if (!std::all_of(p.native_reads.begin(),p.native_reads.end(),[&](int v){return v<=available;}))
            return "a fixed variable that reads one after it";
    }
    for (const auto &d:m.derived)
        if (!d.form.native || !d.form.array_pure || !d.form.native_reads.empty()) return "a derived parameter that is not compiled or reads variables";
    return "";
}

/* what couples m's states beyond what their equations read, or "" (Model::subsystems) */
std::string subsystem_joined_by(const Session &s)
{
    const Model &m=s.model();
    if (m.flag_count()>0) return "global flags (events)";
    if (m.bc_defined>0) return "boundary conditions";
    return rhs_impurity(s);
}

Result<std::vector<Error>> compile_programs(Session &s, const std::vector<ModelProgram> &entries)
{
    Model &m=s.model();
    for (const auto &entry:entries) { entry.code->native=nullptr; entry.code->in_rhs=false; entry.code->array_pure=false; entry.code->native_reads.clear(); }
    m.native_rhs=nullptr;
    m.native_equations=false;
    m.native_program.reset();
    if (!s.numerics.compile || entries.empty()) return {};
    Generator g{s};
    std::vector<Error> warnings;
    if (m.nfun<0 || static_cast<size_t>(m.nfun)>m.ufun_programs.size()) return g.refuse("an out-of-range function count");
    g.calls.resize(static_cast<size_t>(m.nfun));
    g.pure_function.assign(static_cast<size_t>(m.nfun),0);
    g.inline_text.assign(static_cast<size_t>(m.nfun),std::string());
    g.array_pure_function.assign(static_cast<size_t>(m.nfun),0);
    g.function_reads.resize(static_cast<size_t>(m.nfun));
    std::vector<std::string> bodies(static_cast<size_t>(m.nfun));
    std::vector<Error> refusals(static_cast<size_t>(m.nfun));
    for (int i=0;i<m.nfun;++i) {
        g.place=model_place(m,m.ufun_names[i]);
        if (m.narg_fun[i]<0) {
            refusals[i]=g.refuse("an out-of-range argument count").error();
            continue;
        }
        g.source.clear();
        if (auto r=g.program(m.ufun_programs[i].rpn,i,true);!r) refusals[i]=r.error();
        else { bodies[i]=std::move(g.source); g.array_pure_function[i]=g.array_pure; g.function_reads[i]=g.variable_reads; }
    }
    /* Only functions whose entire dependency graph compiled are callable.
       Iterative topological removal also refuses recursion, without following
       untrusted input on the host stack. Impurity was conservative during
       translation, so removing a function cannot change a caller's ordering. */
    g.allowed.assign(static_cast<size_t>(m.nfun),0);
    for (int pass=0;pass<m.nfun;++pass)
        for (int i=0;i<m.nfun;++i)
            if (!g.allowed[i] && !bodies[i].empty()
                && std::all_of(g.calls[i].begin(),g.calls[i].end(),[&](int j){return g.allowed[j];})) g.allowed[i]=1;
    for (int pass=0;pass<m.nfun;++pass)
        for (int i=0;i<m.nfun;++i)
            if (!g.allowed[i] || std::any_of(g.calls[i].begin(),g.calls[i].end(),[&](int j){return !g.array_pure_function[j];})) g.array_pure_function[i]=0;
    for (int pass=0;pass<m.nfun;++pass)
        for (int i=0;i<m.nfun;++i)
            for (int j:g.calls[i]) g.function_reads[i].insert(g.function_reads[j].begin(),g.function_reads[j].end());
    g.source.clear();
    for (int i=0;i<m.nfun;++i) {
        if (!g.allowed[i]) {
            g.place=model_place(m,m.ufun_names[i]);
            const Error error=bodies[i].empty()?refusals[i]
                :g.refuse("calls a refused or recursive user function").error();
            warnings.push_back(error);
            continue;
        }
        g.source+=g.signature(i,true)+";\n";
    }
    for (int i=0;i<m.nfun;++i) if (g.allowed[i]) g.source+=bodies[i];
    std::vector<const ModelProgram *> emitted;
    std::vector<std::pair<size_t,size_t>> written; /* each emitted program's text in g.source */
    for (const auto &entry:entries) {
        g.place=entry.place;
        const size_t text_begin=g.source.size();
        if (entry.user_index>=0) {
            const int i=entry.user_index;
            if (i>=m.nfun || !g.allowed[i]) continue;
            /* The interpreter has already pushed arguments in reverse order.
               This bridge drops them as ENDFUN does; equations call u directly. */
            std::string call=xpp::format("u{}(c,v,S",i);
            for (int a=0;a<m.narg_fun[i];++a) call+=xpp::format(",{}(S,{})",g.helper(Helper::Argument),a);
            call+=")";
            g.source+=xpp::format("double {}(double*c,double*v,void*S,const int*R){{double z={};return {}(S,z,{});}}\n",
                             entry.symbol,call,g.helper(Helper::FunctionEnd),m.narg_fun[i]);
        } else {
            g.dynamic=entry.dynamic_operands;
            if (auto r=g.program(entry.code->rpn,0,false,entry.symbol);!r) {
                warnings.push_back(r.error());
                continue;
            }
        }
        entry.code->array_pure=entry.user_index<0 && g.array_pure;
        if (entry.user_index<0) entry.code->native_reads.assign(g.variable_reads.begin(),g.variable_reads.end());
        if (g.loops>0 && entry.user_index<0) g.called.emplace(entry.code,entry.symbol);
        emitted.push_back(&entry);
        written.emplace_back(text_begin,g.source.size());
    }
    /* the fixed variables and the equations once more, as one function
       (W468, W469): the fixed variables when each compiled alone, the
       equations too when each did and nothing runs between them and the
       fixed variables (my_rhs: no network, no DAE) */
    const std::string rhs_symbol="p_rhs";
    std::set<const Program *> native;
    for (const auto *entry:emitted) native.insert(entry->code);
    const auto compiled_alone=[&](const Program &p){return native.contains(&p);};
    const auto fixed_begin=m.programs.begin()+m.node;
    bool rhs=static_cast<size_t>(m.node+m.fix_var)<=m.programs.size()
        && std::all_of(fixed_begin,fixed_begin+m.fix_var,compiled_alone);
    bool equations=rhs && m.node>0 && m.nnetwork==0 && m.nsvar==0 && m.naeqn==0
        && std::all_of(m.programs.begin(),fixed_begin,compiled_alone);
    rhs=rhs && (m.fix_var>0 || equations);
    if (rhs) {
        g.place=m.fix_var>0?model_place(m,m.fixinfo[0].name):model_place(m,m.uvar_names[0]);
        g.dynamic=false;
        const std::span<const Program> equation_programs=equations?std::span<const Program>{m.programs.data(),static_cast<size_t>(m.node)}:std::span<const Program>{};
        if (auto r=g.rhs({m.programs.data()+m.node,static_cast<size_t>(m.fix_var)},m.node+1,equation_programs,rhs_symbol);!r) {
            warnings.push_back(r.error());
            rhs=equations=false;
        }
    }
    /* W708: when p_rhs writes the equations, an equation's own function is
       not compiled: nothing calls it in a run with p_rhs but a Markov
       variable's or an integral equation's evaluation (they keep theirs),
       and the interpreter serves any other caller of evaluate; one with a
       sum is called by p_rhs and keeps its own. */
    const size_t compiled_count=emitted.size();
    std::vector<Program *> in_rhs;
    if (equations && m.nmarkov==0 && m.nkernel==0) {
        for (size_t k=emitted.size();k-->0;) {
            Program *code=emitted[k]->code;
            if (code<m.programs.data() || code>=m.programs.data()+m.node || g.called.contains(code)) continue;
            g.source.erase(written[k].first,written[k].second-written[k].first);
            in_rhs.push_back(code);
            emitted.erase(emitted.begin()+static_cast<std::ptrdiff_t>(k));
        }
    }
    if (emitted.empty() && !rhs) return warnings;
    auto compiled=g.build(entries.front().place);
    if (!compiled) {g.place=compiled.error().place;return g.refuse("TinyCC: "+compiled.error().what);}
    std::vector<decltype(Program::native)> functions;
    for (const auto *entry:emitted) {
        auto address=(*compiled)->function(entry->symbol);
        if (!address) {g.place=address.error().place;return g.refuse("TinyCC: "+address.error().what);}
        functions.push_back(reinterpret_cast<decltype(Program::native)>(*address));
    }
    decltype(Model::native_rhs) native_rhs=nullptr;
    if (rhs) {
        auto address=(*compiled)->function(rhs_symbol);
        if (!address) {g.place=address.error().place;return g.refuse("TinyCC: "+address.error().what);}
        native_rhs=reinterpret_cast<decltype(Model::native_rhs)>(*address);
    }
    m.native_program=std::move(*compiled);
    m.native_rhs=native_rhs;
    m.native_equations=equations;
    for (size_t i=0;i<emitted.size();++i) emitted[i]->code->native=functions[i];
    for (Program *code:in_rhs) code->in_rhs=true;
    log(XPP_LOG_INFO,"Compiled {} of {} programs\n",compiled_count,entries.size());
    return warnings;
}

/* The model's exact Jacobian (W260): its derivatives (expr_derivative.h)
   compiled by TinyCC as a program of their own, one function for the
   state's columns, one for the time's and one for each constant's that a
   right-hand side depends on; or why there is none. */
ExactJacobian compile_jacobian(Session &s, const std::vector<ModelProgram> &entries)
{
    ExactJacobian j;
    if (!s.numerics.compile) {
        j.unavailable = "the model is interpreted (option compile is false)";
        return j;
    }
    auto derived = expr::differentiate_model(s, entries);
    if (!derived) {
        j.unavailable = derived.error().text();
        return j;
    }
    const int node = s.model().node;
    std::vector<expr::Term> states, time;
    /* each constant's terms and their rows, by the constant's index */
    std::map<int, std::pair<std::vector<expr::Term>, std::vector<int>>> by_constant;
    for (const auto &e : derived->entries) {
        if (e.column > 0 && e.column <= node) {
            states.push_back(e.value);
            j.states.rows.push_back(e.row);
            j.states.columns.push_back(e.column - 1);
        } else if (e.column == 0) {
            time.push_back(e.value);
            j.time.rows.push_back(e.row);
        } else {
            auto &[terms, rows] = by_constant[expr::column_constant(e.column)];
            terms.push_back(e.value);
            rows.push_back(e.row);
        }
    }
    Generator g{s};
    g.derivatives(derived->graph, states, "j_states", j.workspace);
    g.derivatives(derived->graph, time, "j_time", j.workspace);
    for (auto &[k, column] : by_constant) {
        g.derivatives(derived->graph, column.first, xpp::format("j_c{}", k), j.workspace);
        j.by_constant.emplace_back(k, JacobianProgram{nullptr, std::move(column.second), {}});
    }
    const Place place = entries.empty() ? Place{} : entries.front().place;
    auto compiled = g.build(place);
    if (!compiled) {
        j.unavailable = "TinyCC: " + compiled.error().what;
        return j;
    }
    auto function = [&](const std::string &name, JacobianProgram &p) -> bool {
        const auto address = (*compiled)->function(name);
        if (address) p.function = reinterpret_cast<decltype(JacobianProgram::function)>(*address);
        else j.unavailable = "TinyCC: " + address.error().what;
        return address.has_value();
    };
    for (auto &[k, column] : j.by_constant)
        if (!function(xpp::format("j_c{}", k), column)) return j;
    if (!function("j_time", j.time) || !function("j_states", j.states)) return j;
    j.code = std::move(*compiled);
    log(XPP_LOG_INFO, "Compiled the exact Jacobian: {} values, {} derivatives\n", derived->graph.size(), derived->entries.size());
    return j;
}

}

std::vector<std::vector<int>> equation_reads(const Model &m)
{
    const int n=m.node,programs=n+m.fix_var;
    /* the states each fixed variable and equation reads; a fixed variable
       reads only those before it (Model::rhs_impurity), so one pass in order */
    std::vector<std::vector<int>> reads(static_cast<size_t>(programs));
    std::vector<char> read(static_cast<size_t>(n));
    for (int p=n;p<programs+n;p++) {
        const int q=p<programs?p:p-programs; /* the fixed variables, then the equations */
        std::fill(read.begin(),read.end(),0);
        for (const int v:m.programs[static_cast<size_t>(q)].native_reads) {
            if (v>=1 && v<=n) read[static_cast<size_t>(v-1)]=1;
            else if (v>n && v<=programs)
                for (const int k:reads[static_cast<size_t>(v-1)]) read[static_cast<size_t>(k)]=1;
        }
        if (q<n) read[static_cast<size_t>(q)]=1; /* a map's f(x)-x reads its own state */
        for (int k=0;k<n;k++)
            if (read[static_cast<size_t>(k)]) reads[static_cast<size_t>(q)].push_back(k);
    }
    reads.resize(static_cast<size_t>(n));
    return reads;
}

std::string run_joined_by(const Session &s)
{
    if (s.numerics.subsystem_split != SubsystemSplit::run) return "";
    if (s.numerics.poincare_stop) return "a Poincare stop";
    if (s.numerics.poincare_map != PoincareMap::none) return "a Poincare section";
    return "";
}

bool has_subsystems(const Session &s)
{
    return !s.model().subsystems.empty() && run_joined_by(s).empty();
}

std::string describe_subsystems(const Session &s)
{
    const Model &m=s.model();
    if (const std::string run=run_joined_by(s); !run.empty() && !m.subsystems.empty())
        return xpp::format("1 subsystem, joined by {} (subsystem_split: run)", run);
    if (m.subsystems.empty())
        return m.subsystem_joined_by.empty() ? "1 subsystem: the equations read each other's states"
                                             : xpp::format("1 subsystem, joined by {}", m.subsystem_joined_by);
    /* a model of hundreds shows its first ones */
    constexpr size_t SHOWN = 8;
    std::string text = xpp::format("{} independent subsystems: ", m.subsystems.size());
    for (size_t c = 0; c < std::min(SHOWN, m.subsystems.size()); ++c) {
        text += c ? ", {" : "{";
        for (size_t i = 0; i < m.subsystems[c].size(); ++i)
            text += xpp::format("{}{}", i ? ", " : "", m.uvar_names[static_cast<size_t>(m.subsystems[c][i])]);
        text += '}';
    }
    return m.subsystems.size() > SHOWN ? text + ", ..." : text;
}

Result<std::vector<Error>> compile_model(Session &s)
{
    Model &m=s.model();
    const auto entries=model_programs(m);
    for(const auto &entry:entries)
        size_expr_stack(s,entry.code->rpn,entry.user_index<0?-1:m.narg_fun[entry.user_index]);
    auto compiled=compile_programs(s,entries);
    m.rhs_impurity=rhs_impurity(s);
    m.subsystem_joined_by=subsystem_joined_by(s);
    m.subsystems.clear();
    if (m.subsystem_joined_by.empty()) {
        auto parts=connected_components(equation_reads(m));
        if (parts.size()>1) m.subsystems=std::move(parts);
    }
    log(XPP_LOG_INFO,"{}\n",describe_subsystems(s));
    Place requested=std::move(m.jacobian.requested);
    m.jacobian=ExactJacobian{};
    m.jacobian.requested=std::move(requested);
    if (s.numerics.jacobian==JACOBIAN_EXACT) ensure_exact_jacobian(s);
    return compiled;
}

bool ensure_exact_jacobian(Session &s)
{
    ExactJacobian &j=s.model().jacobian;
    if (!j.tried) {
        Place requested=std::move(j.requested);
        j=compile_jacobian(s,model_programs(s.model()));
        j.requested=std::move(requested);
        j.tried=true;
    }
    return j.available();
}
}
