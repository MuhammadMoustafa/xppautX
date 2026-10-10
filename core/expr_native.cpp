/* The sole owner of translating validated RPN into trusted C. No model text
   enters the translation; every emitted name and subscript is an index. */
#include "expr_native.h"
#include "expr_program.h"
#include "session.h"
#include "simplenet.h"
#include "tabular.h"
#include "volterra2.h"
#include "xpp_tcc.h"
#include "xpp_io.h"
#include <array>
#include <bit>
#include <cstdint>
#include <set>
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
};

/* A conditional being read: cond is the tested value, base the stack depth
   under the branches, else_pc the first token of the else branch, mark and
   then_mark the statements written so far at the MYIF and at the MYTHEN. */
struct Frame {
    Node cond, then_value;
    size_t base, else_pc, end_pc = 0;
    bool has_then = false;
    size_t mark = 0, then_mark = 0;
};

/* Sums nest this deep at most: each level is one recursion of the
   translator, and a model's own sums nest two or three. */
constexpr int MAX_SUM_DEPTH = 16;

/* The interpreter's own helpers the generated code calls, as C names and
   prototypes: the code does the same operations in the same order and
   these do the arithmetic, so the results are the interpreter's, bit for
   bit. The first argument of each is the Session. */
enum class Helper : size_t { Table, Network, Vector, Kernel, Delay, DelayShift, Shift, IShift, Set, Uniform, Poisson, Normal, Count };
struct HelperText {
    std::string_view name, prototype;
};
constexpr std::array<HelperText, static_cast<size_t>(Helper::Count)> helper_text{{
    {"h_tab", "double h_tab(void*,double,int)"},
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
}};

/* The helpers with exactly the C prototypes above, so the generated code
   calls each through its own type; S is the Session the native function
   was given. */
Session &session(void *S) { return *static_cast<Session *>(S); }
double h_tab(void *S, double x, int i) { return lookup(session(S), x, i); }
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

const void *helper_address(Helper h)
{
    switch (h) {
    case Helper::Table: return reinterpret_cast<const void *>(&h_tab);
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
    std::array<bool, static_cast<size_t>(Helper::Count)> used{};
    /* the function being written: its statements in order, how many
       temporaries and loop counters it declares, how deep in sums it is,
       and whether it changes state or draws (its callers then call it
       as a statement) */
    std::vector<std::string> out;
    int temps = 0, loops = 0, depth = 0;
    bool impure = false;

    std::unexpected<Error> refuse(std::string reason) const
    {
        return fail("compile", "not compiled: " + reason + "; running the interpreter", place);
    }

    std::string signature(int i, bool user) const
    {
        std::string text = xpp::format("double {}{}(double *c,double *v,void *S", user ? "u" : "p", i);
        if (user)
            for (int a = 0; a < s.model().narg_fun[i]; ++a) text += xpp::format(",double a{}", a);
        return text + ")";
    }

    /* the helper's C name, declared in the program */
    std::string_view helper(Helper h)
    {
        used[static_cast<size_t>(h)] = true;
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
        return statement(std::move(call));
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

    /* The value of the instructions from pc up to the terminator (ENDEXP,
       or a sum's ENDSUM), as one C expression: the interpreter's stack is
       simulated over trees, so each operation is done on the operands the
       interpreter pops, in its order, and TinyCC keeps temporaries in
       registers instead of a stack array. pc ends after the terminator.
       Iterative; only a sum's body is a recursion, MAX_SUM_DEPTH deep. */
    Result<Node> sequence(const std::vector<int> &p, size_t &pc, size_t limit, int terminator, bool user, int index)
    {
        const Model &m = s.model();
        std::vector<Node> stack;
        std::vector<Frame> frames;
        auto lower = [&] { return frames.empty() ? size_t{0} : frames.back().base; };
        auto pop = [&] { Node n = std::move(stack.back()); stack.pop_back(); return n; };
        auto need = [&](size_t n) { return stack.size() >= lower() + n; };
        while (pc < limit) {
            while (!frames.empty() && frames.back().has_then && frames.back().end_pc == pc) {
                const Frame f = std::move(frames.back());
                frames.pop_back();
                if (stack.size() != f.base+1) return refuse("inconsistent conditional stacks");
                const Node other = pop();
                join(stack, f, other, lower());
            }
            if (!frames.empty() && frames.back().has_then && pc > frames.back().end_pc) return refuse("an invalid conditional jump");
            const int op = p[pc], type = op / MAXTYPE, in = op % MAXTYPE;
            size_t next = pc + 1;
            if (op == terminator) {
                if (stack.size() != 1 || !frames.empty()) return refuse("an invalid result stack");
                pc = next;
                return pop();
            } else if (op == ENDEXP || op == ENDSUM) {
                return refuse("a misplaced end of program");
            } else if (op == NUMSYM) {
                if (limit - pc < 3) return refuse("a truncated number");
                stack.push_back({xpp::format("N[{}]", numbers.size()), true});
                numbers.push_back(expr::number_from_halves(p[pc+2], p[pc+1]));
                next += 2;
            } else if (op == MYIF) {
                if (next >= limit || p[next] < 0 || !need(1)) return refuse("an invalid conditional jump");
                const size_t target = next + 1 + static_cast<size_t>(p[next]);
                if (target >= limit) return refuse("an out-of-range conditional jump");
                Node cond = pop();
                frames.push_back({std::move(cond), {}, stack.size(), target});
                frames.back().mark = out.size();
                ++next;
            } else if (op == MYTHEN) {
                if (next >= limit || p[next] < 0 || frames.empty() || frames.back().has_then) return refuse("an invalid conditional jump");
                Frame &f = frames.back();
                const size_t end = next + 1 + static_cast<size_t>(p[next]);
                ++next;
                if (next != f.else_pc || end >= limit || end < next || stack.size() != f.base+1) return refuse("an invalid conditional jump");
                if (frames.size() > 1) {
                    const Frame &outer = frames[frames.size()-2];
                    if (end > (outer.has_then ? outer.end_pc : outer.else_pc-2)) return refuse("an invalid conditional jump");
                }
                f.then_value = pop();
                f.end_pc = end;
                f.has_then = true;
                f.then_mark = out.size();
            } else if (op == MYELSE) {
                /* the else branch starts here; nothing to emit */
            } else if (op == ENDFUN) {
                if (!user || next >= limit || p[next] != m.narg_fun[index]) return refuse("an invalid function end");
                ++next;
            } else if (op == INDXCOM) {
                stack.push_back({xpp::format("N[{}]", numbers.size()), true});
                numbers.push_back(expr::CURRENT_INDEX);
            } else if (op == SUMSYM) {
                if (next >= limit || p[next] <= 0 || !need(2)) return refuse("an invalid sum");
                const size_t body = next + 1, end = body + static_cast<size_t>(p[next]);
                if (end > limit) return refuse("an out-of-range sum");
                if (depth >= MAX_SUM_DEPTH) return refuse("sums nested too deep");
                const Node high = pop(), low = pop();
                flush(stack, lower());
                impure = true;
                const int k = loops++;
                const Node total{xpp::format("t{}", temps++), true};
                /* lo, hi and ix are int: the assignments truncate the bounds and
                   widen the index as the interpreter's casts do */
                out.push_back(xpp::format("lo{0}={1};hi{0}={2};{3}=0.0;if(lo{0}<=hi{0}){{for(ix{0}=lo{0};;ix{0}++){{c[{4}]=ix{0};",
                                          k, low.text, high.text, total.text, expr::SUM_INDEX));
                size_t body_pc = body;
                ++depth;
                auto value = sequence(p, body_pc, end, ENDSUM, user, index);
                --depth;
                if (!value) return value;
                if (body_pc != end) return refuse("a sum that does not end where it says");
                out.push_back(xpp::format("{0}={0}+{1};if(ix{2}==hi{2})break;}}}}", total.text, value->text, k));
                stack.push_back(total);
                next = end;
            } else if (op == ENDDELAY || op == ENDSHIFT || op == ENDISHIFT) {
                if (!need(2)) return refuse("a delay or shift stack underflow");
                const Node first = pop(), second = pop();
                if (op == ENDISHIFT) {
                    stack.push_back({xpp::format("{}({},{})", helper(Helper::IShift), first.text, second.text), false, first.mut || second.mut});
                } else if (op == ENDSHIFT) {
                    stack.push_back({xpp::format("{}(S,{},{})", helper(Helper::Shift), first.text, second.text), false, true});
                } else {
                    stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{})", helper(Helper::Delay), first.text, second.text)));
                }
            } else if (op == ENDDELSHFT) {
                if (!need(3)) return refuse("a delay stack underflow");
                const Node tau = pop(), shift = pop(), variable = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{},{})", helper(Helper::DelayShift), tau.text, shift.text, variable.text)));
            } else if (op == ENDSET) {
                if (!need(3)) return refuse("a set stack underflow");
                const Node value = pop(), shift = pop(), variable = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{},{})", helper(Helper::Set), shift.text, variable.text, value.text)));
            } else if (op == RANDUNI || op == RANDPOI) {
                if (!need(1)) return refuse("a random stack underflow");
                const Node x = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{})", helper(op == RANDUNI ? Helper::Uniform : Helper::Poisson), x.text)));
            } else if (op == RANDNORM) {
                if (!need(2)) return refuse("a random stack underflow");
                const Node sd = pop(), mean = pop();
                stack.push_back(effect(stack, lower(), xpp::format("{}(S,{},{})", helper(Helper::Normal), mean.text, sd.text)));
            } else if (type == SVARTYPE || type == SCONTYPE) {
                /* the variable or constant a delay, shift or set names, as the number the interpreter pushes */
                const size_t bound = type == SCONTYPE ? s.parser.constants.size() : s.parser.variables.size();
                if (in < 0 || static_cast<size_t>(in) >= bound) return refuse("an out-of-range operand");
                stack.push_back({xpp::format("{}.0", type == SCONTYPE ? COM(CONTYPE, in) : COM(VARTYPE, in)), true});
            } else if (type == CONTYPE || type == VARTYPE) {
                const size_t bound = type == CONTYPE ? s.parser.constants.size() : s.parser.variables.size();
                if (in < 0 || static_cast<size_t>(in) >= bound) return refuse("an out-of-range operand");
                stack.push_back({xpp::format("{}[{}]", type == CONTYPE ? "c" : "v", in), true, type == VARTYPE || in == expr::SUM_INDEX});
            } else if (type == USTACKTYPE) {
                if (!user || in < 0 || in >= m.narg_fun[index]) return refuse("an out-of-range argument");
                stack.push_back({xpp::format("a{}", in), true});
            } else if (type == FUN1TYPE) {
                if (in < 0 || static_cast<size_t>(in) >= expr::fun1.size() || !expr::fun1[in]) return refuse("an invalid unary function");
                if (!need(1)) return refuse("a unary stack underflow");
                unary.insert(in);
                const Node x = pop();
                stack.push_back({xpp::format("f1_{}({})", in, x.text), false, x.mut});
            } else if (type == FUN2TYPE) {
                if (in < 0 || static_cast<size_t>(in) >= expr::fun2.size()) return refuse("an out-of-range binary function");
                if (!need(2)) return refuse("a binary stack underflow");
                const Node right = pop(), left = pop();
                std::string code;
                if (in == 0 || in == 2) code = xpp::format("({}{}{})", right.text, in == 0 ? "+" : "*", left.text);
                else if (in == 1 || in == expr::IEEE_DIVIDE) code = xpp::format("({}{}{})", left.text, in == 1 ? "-" : "/", right.text);
                else if (in == 3) {
                    /* the guarded division: a zero divisor becomes N[0] */
                    if (right.atom) code = xpp::format("({}/({}==0.0?N[0]:{}))", left.text, right.text, right.text);
                    else code = xpp::format("dv({},{})", left.text, right.text);
                } else {
                    if (!expr::fun2[in]) return refuse("an invalid binary function");
                    binary.insert(in);
                    code = xpp::format("f2_{}({},{})", in, left.text, right.text);
                }
                stack.push_back({std::move(code), false, left.mut || right.mut});
            } else if (type == UFUNTYPE) {
                if (in < 0 || in >= m.nfun || next >= limit || p[next] != m.narg_fun[in]) return refuse("an invalid user function call");
                const int n = p[next++];
                if (n < 0 || !need(static_cast<size_t>(n))) return refuse("a function stack underflow");
                if (user) calls[index].insert(in);
                std::string code = xpp::format("u{}(c,v,S", in);
                const size_t first = stack.size() - static_cast<size_t>(n);
                for (size_t a = first; a < stack.size(); ++a) code += "," + stack[a].text;
                stack.resize(first);
                code += ")";
                stack.push_back(pure_function[in] ? Node{std::move(code), false, true} : effect(stack, lower(), std::move(code)));
            } else if (type == TABTYPE || type == NETTYPE || type == VECTYPE || type == KERTYPE) {
                /* the arrays the helpers index are the interpreter's own: the bound is theirs */
                const size_t bound = type == TABTYPE ? s.tables.size() : type == NETTYPE ? m.networks.size() : type == VECTYPE ? m.vectors.size() : m.kernels.size();
                if (in < 0 || static_cast<size_t>(in) >= bound) return refuse("an out-of-range table, network, vectorizer or kernel");
                if (type == KERTYPE) {
                    stack.push_back({xpp::format("{}(S,{})", helper(Helper::Kernel), in), false});
                } else {
                    if (!need(1)) return refuse("a lookup stack underflow");
                    const Node x = pop();
                    const Helper h = type == TABTYPE ? Helper::Table : type == NETTYPE ? Helper::Network : Helper::Vector;
                    stack.push_back({xpp::format("{}(S,{},{})", helper(h), x.text, in), false, type != TABTYPE || x.mut});
                }
            } else {
                return refuse("uses an unsupported instruction");
            }
            if (stack.size() > EXPR_STACK) return refuse("an out-of-range stack");
            pc = next;
        }
        return refuse("an unterminated program");
    }

    /* One C function for a program (an equation's, or user function index's). */
    Result<> program(const std::vector<int> &p, int index, bool user)
    {
        if (p.empty() || p.size() > MAXEXPLEN) return refuse("an invalid program length");
        out.clear();
        temps = loops = depth = 0;
        impure = false;
        size_t pc = 0;
        auto value = sequence(p, pc, p.size(), ENDEXP, user, index);
        if (!value) return std::unexpected(value.error());
        std::string declarations;
        for (int i = 0; i < temps; ++i) declarations += xpp::format("{}t{}", i ? "," : "double ", i);
        if (temps) declarations += ";";
        for (int i = 0; i < loops; ++i) declarations += xpp::format("{1}lo{0},hi{0},ix{0}", i, i ? "," : "int ");
        if (loops) declarations += ";";
        source += signature(index, user) + "{" + declarations;
        for (const std::string &statement : out) source += statement;
        source += xpp::format("return {};}}\n", value->text);
        if (user) pure_function[index] = !impure;
        return {};
    }
};
}

Result<> compile_model(Session &s)
{
    Model &m = s.model();
    m.native_program.reset();
    m.native_functions.fill(nullptr);
    if (!program.compile) return {};
    Generator g{s};
    /* The expressions of events, boundary conditions, derived parameters,
       algebraic equations, Markov chains and kernels run outside
       Model::programs, through evaluate(): they stay interpreted, each
       one giving the value the compiled one would. */
    if (m.nfun < 0 || static_cast<size_t>(m.nfun) > m.ufun_programs.size()) return g.refuse("an out-of-range function count");
    g.calls.resize(static_cast<size_t>(m.nfun));
    g.pure_function.assign(static_cast<size_t>(m.nfun), 0);
    for (int i=0; i<m.nfun; ++i) {
        g.place=model_place(m,m.ufun_names[i]);
        if (m.narg_fun[i]<0 || m.narg_fun[i]>EXPR_STACK) return g.refuse("an out-of-range argument count");
        g.source += g.signature(i,true)+";\n";
    }
    auto equation_place=[&](size_t i) {
        return model_place(m, i < static_cast<size_t>(m.node) ? m.uvar_names[i] : i < static_cast<size_t>(m.node+m.fix_var) ? m.fixinfo[i-m.node].name : m.uvar_names[i-m.fix_var]);
    };
    Place first;
    for (size_t i=0; i<m.programs.size() && first.file.empty(); ++i)
        if (!m.programs[i].empty()) first=equation_place(i);
    /* the functions first: an equation's call of one is then known to change
       no state, or to */
    for (int i=0; i<m.nfun; ++i) {
        g.place=model_place(m,m.ufun_names[i]);
        if (auto r=g.program(m.ufun_programs[i],i,true); !r) return r;
    }
    for (size_t i=0; i<m.programs.size(); ++i) {
        if (m.programs[i].empty()) continue;
        g.place=equation_place(i);
        if (auto r=g.program(m.programs[i],static_cast<int>(i),false); !r) return r;
    }
    /* Topological removal, with no recursive traversal of user input. */
    std::set<int> done;
    for (int pass=0; pass<m.nfun; ++pass)
        for (int i=0; i<m.nfun; ++i)
            if (!done.contains(i) && std::all_of(g.calls[i].begin(),g.calls[i].end(),[&](int j){return done.contains(j);})) done.insert(i);
    if (done.size()!=static_cast<size_t>(m.nfun)) {
        for (int i=0; i<m.nfun; ++i) if (!done.contains(i)) { g.place=model_place(m,m.ufun_names[i]); break; }
        return g.refuse("uses recursive user functions");
    }
    /* the literals are the host's own doubles, shared bit for bit; dv is the
       guarded division of a non-trivial divisor */
    std::string declarations=xpp::format("double N[{}];\nstatic double dv(double a,double b){{if(b==0.0)b=N[0];return a/b;}}\n",g.numbers.size());
    std::vector<tcc::Symbol> symbols;
    std::vector<std::string> names;
    std::vector<const void *> addresses;
    for (int i:g.unary) {
        names.push_back(xpp::format("f1_{}",i));
        declarations+=xpp::format("extern double f1_{}(double);\n",i);
        addresses.push_back(reinterpret_cast<const void *>(expr::fun1[i]));
    }
    for (int i:g.binary) {
        names.push_back(xpp::format("f2_{}",i));
        declarations+=xpp::format("extern double f2_{}(double,double);\n",i);
        addresses.push_back(reinterpret_cast<const void *>(expr::fun2[i]));
    }
    for (size_t i=0; i<g.used.size(); ++i) {
        if (!g.used[i]) continue;
        names.emplace_back(helper_text[i].name);
        declarations+=xpp::format("extern {};\n",helper_text[i].prototype);
        addresses.push_back(helper_address(static_cast<Helper>(i)));
    }
    for (size_t i=0; i<names.size(); ++i) symbols.push_back({names[i].c_str(),addresses[i]});
    auto compiled=tcc::Program::compile(declarations+g.source,symbols,first);
    if (!compiled) { g.place=compiled.error().place; return g.refuse("TinyCC: "+compiled.error().what); }
    /* fill the literals' table the generated code reads (its data symbol) */
    auto table=(*compiled)->function("N");
    if (!table) { g.place=table.error().place; return g.refuse("TinyCC: "+table.error().what); }
    std::copy(g.numbers.begin(),g.numbers.end(),static_cast<double *>(*table));
    decltype(m.native_functions) functions{};
    for (size_t i=0; i<m.programs.size(); ++i) {
        if (m.programs[i].empty()) continue;
        auto address=(*compiled)->function(xpp::format("p{}",i));
        if (!address) { g.place=address.error().place; return g.refuse("TinyCC: "+address.error().what); }
        functions[i]=reinterpret_cast<double (*)(double *,double *,Session *)>(*address);
    }
    m.native_program=std::move(*compiled);
    m.native_functions=functions;
    const auto equations=std::count_if(functions.begin(),functions.end(),[](auto f){return f!=nullptr;});
    log(XPP_LOG_INFO,"Compiled {} equations and {} user functions\n",equations,m.nfun);
    return {};
}
}
