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
#include "xpp_io.h"
#include <array>
#include <bit>
#include <cctype>
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
    bool two = false;  /* the literal 2: an exponent written as x*x */
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

struct Generator {
    Session &s;
    Place place;
    std::string source;
    std::set<int> unary, binary;
    std::vector<std::set<int>> calls;
    std::vector<char> pure_function; /* a user function written so far that changes no state */
    std::vector<double> numbers{expr::ZERO_DIVISOR}; /* N[0]: the zero divisor's value, bit-exact */
    std::unordered_map<std::uint64_t, size_t> literal_slot; /* a literal's bits: its N[], so one value has one name */
    /* the function being written: the pure calls already made at its top
       level, by their C text, each held in a temporary (pure_call) */
    std::unordered_map<std::string, Node> calls_made;
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
        calls_made.clear();
        return statement(std::move(call));
    }

    /* A literal's N[] slot: the same value, the same slot, so two writings
       of one expression have one text. */
    Node literal(double x)
    {
        const auto [slot, added] = literal_slot.emplace(std::bit_cast<std::uint64_t>(x), numbers.size());
        if (added) numbers.push_back(x);
        return {xpp::format("N[{}]", slot->second), true, false, x == 2.0};
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
        calls_made.emplace(std::move(text), t);
        return t;
    }

    /* x^2 for the literal exponent 2, as x*x with x written once (a
       temporary when it is no atom). A correctly rounded pow(x,2) is the
       exact square rounded to nearest once, which is what the product is:
       the same bits for every x, signed zeros, infinities, NaN (both give
       the quieted x), overflow and subnormals included; ^3 and the others
       are two roundings and stay pow. */
    Node square(const Node &x)
    {
        const Node a = x.atom ? x : statement(x.text);
        return {xpp::format("({0}*{0})", a.text), false, a.mut};
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
                calls_made.clear();
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
                if (in == expr::POW && right.two) {
                    stack.push_back(square(left));
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
                std::string text = xpp::format("u{}(c,v,S", in);
                const size_t first = stack.size() - static_cast<size_t>(i.count);
                for (size_t a = first; a < stack.size(); ++a) text += "," + stack[a].text;
                stack.resize(first);
                text += ")";
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
        calls_made.clear();
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
        if (user) pure_function[index] = !impure;
        return {};
    }

    /* Whether the C text may read variable k: by its name v[k], or through
       a call given the Session S (the only S in the text): a user
       function's that reads k or what is not its arguments or variables
       (array_pure_function), or a helper's other than a table's. */
    bool may_read(std::string_view text, int k) const
    {
        if (text.find(xpp::format("v[{}]", k)) != std::string_view::npos) return true;
        for (size_t at = text.find('S'); at != std::string_view::npos; at = text.find('S', at + 1)) {
            const size_t open = text.rfind('(', at);
            size_t start = open;
            while (start > 0 && (std::isalnum(static_cast<unsigned char>(text[start - 1])) || text[start - 1] == '_')) --start;
            const std::string_view call = text.substr(start, open - start);
            if (call == helper_text[static_cast<size_t>(Helper::Table)].name || call == helper_text[static_cast<size_t>(Helper::TableSlope)].name) continue;
            if (call.starts_with('u')) {
                const size_t f = std::stoul(std::string(call.substr(1)));
                if (array_pure_function[f] && !function_reads[f].contains(k)) continue;
            }
            return true;
        }
        return false;
    }

    /* One C function, name, for the fixed variables (W468): each program's
       value written into its variable, first and on, in order, as
       fixed_values (my_rhs.cpp) sets them one by one, so a math call two
       of them make on the same values is made once. Writing a variable
       forgets the calls whose text may read it. */
    Result<> fixed(std::span<const Program> programs, int first, std::string_view name)
    {
        begin();
        for (size_t i = 0; i < programs.size(); ++i) {
            const auto result = value(programs[i].rpn, 0, false);
            if (!result) return std::unexpected(result.error());
            const int variable = first + static_cast<int>(i);
            out.push_back(xpp::format("v[{}]={};", variable, result->text));
            std::erase_if(calls_made, [&](const auto &made) { return may_read(made.first, variable); });
        }
        write(xpp::format("void {}(double *c,double *v,void *S)", name), "");
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
                case expr::POW:
                    if (g.is(expr::Term{n.args[1]}, 2)) {
                        value = xpp::format("({0}*{0})", x);
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
            addresses.push_back(reinterpret_cast<const void *>(expr::fun1[i]));
        }
        for (int i:binary) {
            names.push_back(xpp::format("f2_{}",i));
            declarations+=xpp::format("extern double f2_{}(double,double);\n",i);
            addresses.push_back(reinterpret_cast<const void *>(expr::fun2[i]));
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
Result<std::vector<Error>> compile_programs(Session &s, const std::vector<ModelProgram> &entries)
{
    Model &m=s.model();
    for (const auto &entry:entries) { entry.code->native=nullptr; entry.code->array_pure=false; entry.code->native_reads.clear(); }
    m.auto_rhs_pure=false;
    m.native_fixed=nullptr;
    m.native_program.reset();
    if (!::program.compile || entries.empty()) return {};
    Generator g{s};
    std::vector<Error> warnings;
    if (m.nfun<0 || static_cast<size_t>(m.nfun)>m.ufun_programs.size()) return g.refuse("an out-of-range function count");
    g.calls.resize(static_cast<size_t>(m.nfun));
    g.pure_function.assign(static_cast<size_t>(m.nfun),0);
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
    for (const auto &entry:entries) {
        g.place=entry.place;
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
        emitted.push_back(&entry);
    }
    /* the fixed variables once more, as one function, when each compiled alone (W468) */
    const std::string fixed_symbol="p_fixed";
    bool fixed=m.fix_var>0 && static_cast<size_t>(m.node+m.fix_var)<=m.programs.size()
        && std::all_of(m.programs.begin()+m.node,m.programs.begin()+m.node+m.fix_var,[&](const Program &p){
               return std::any_of(emitted.begin(),emitted.end(),[&](const ModelProgram *e){return e->code==&p;});});
    if (fixed) {
        g.place=model_place(m,m.fixinfo[0].name);
        g.dynamic=false;
        if (auto r=g.fixed({m.programs.data()+m.node,static_cast<size_t>(m.fix_var)},m.node+1,fixed_symbol);!r) {
            warnings.push_back(r.error());
            fixed=false;
        }
    }
    if (emitted.empty()) return warnings;
    auto compiled=g.build(emitted.front()->place);
    if (!compiled) {g.place=compiled.error().place;return g.refuse("TinyCC: "+compiled.error().what);}
    std::vector<decltype(Program::native)> functions;
    for (const auto *entry:emitted) {
        auto address=(*compiled)->function(entry->symbol);
        if (!address) {g.place=address.error().place;return g.refuse("TinyCC: "+address.error().what);}
        functions.push_back(reinterpret_cast<decltype(Program::native)>(*address));
    }
    decltype(Model::native_fixed) native_fixed=nullptr;
    if (fixed) {
        auto address=(*compiled)->function(fixed_symbol);
        if (!address) {g.place=address.error().place;return g.refuse("TinyCC: "+address.error().what);}
        native_fixed=reinterpret_cast<decltype(Model::native_fixed)>(*address);
    }
    m.native_program=std::move(*compiled);
    m.native_fixed=native_fixed;
    for (size_t i=0;i<emitted.size();++i) emitted[i]->code->native=functions[i];
    // Networks and DAEs run between the fixed variables and equations even
    // when no equation calls their helpers. Function tables can rebuild on
    // parameter changes (or when autoeval is toggled), so never freeze them.
    m.auto_rhs_pure=m.nsvar==0 && m.naeqn==0 && m.nnetwork==0 && m.nmarkov==0 && m.nkernel==0;
    for (int i=0;i<s.ntable;++i) if (s.tables[i].flag==2) m.auto_rhs_pure=false;
    for (int i=0;i<m.node+m.fix_var;++i) {
        const auto &p=m.programs[i];
        const int available=i<m.node?m.node+m.fix_var:i;
        m.auto_rhs_pure=m.auto_rhs_pure && p.native && p.array_pure
            && std::all_of(p.native_reads.begin(),p.native_reads.end(),[&](int v){return v<=available;});
    }
    for (const auto &d:m.derived)
        m.auto_rhs_pure=m.auto_rhs_pure && d.form.native && d.form.array_pure && d.form.native_reads.empty();
    log(XPP_LOG_INFO,"Compiled {} of {} programs\n",emitted.size(),entries.size());
    return warnings;
}

/* The model's exact Jacobian (W260): its derivatives (expr_derivative.h)
   compiled by TinyCC as a program of their own, one function for the
   state's columns, one for the time's and one for each constant's that a
   right-hand side depends on; or why there is none. */
ExactJacobian compile_jacobian(Session &s, const std::vector<ModelProgram> &entries)
{
    ExactJacobian j;
    if (!::program.compile) {
        j.unavailable = "the model is interpreted (--no-compile)";
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

Result<std::vector<Error>> compile_model(Session &s)
{
    Model &m=s.model();
    const auto entries=model_programs(m);
    for(const auto &entry:entries)
        size_expr_stack(s,entry.code->rpn,entry.user_index<0?-1:m.narg_fun[entry.user_index]);
    auto compiled=compile_programs(s,entries);
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
