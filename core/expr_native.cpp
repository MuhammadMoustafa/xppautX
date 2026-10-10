/* The sole owner of translating validated RPN into trusted C. No model text
   enters the translation; every emitted name and subscript is an index. */
#include "expr_native.h"
#include "expr_program.h"
#include "session.h"
#include "xpp_tcc.h"
#include "xpp_io.h"
#include <bit>
#include <cstdint>
#include <set>
#include <algorithm>
#include "xpp_log.h"
#include "xpp_globals.h"

namespace xpp {
namespace {
struct Node {
    std::string text;
    bool atom; /* a name, subscript or literal: may be repeated in the C */
};

/* A conditional being read: cond is the tested value, base the stack depth
   under the branches, else_pc the first token of the else branch. */
struct Frame {
    Node cond, then_value;
    size_t base, else_pc, end_pc = 0;
    bool has_then = false;
};

struct Generator {
    Session &s;
    Place place;
    std::string source;
    std::set<int> unary, binary;
    std::vector<std::set<int>> calls;
    std::vector<double> numbers{expr::ZERO_DIVISOR}; /* N[0]: the zero divisor's value, bit-exact */

    Result<> refuse(std::string reason) const
    {
        return fail("compile", "not compiled: " + reason + "; running the interpreter", place);
    }

    std::string signature(int i, bool user) const
    {
        std::string out = xpp::format("double {}{}(double *c,double *v", user ? "u" : "p", i);
        if (user)
            for (int a = 0; a < s.model().narg_fun[i]; ++a) out += xpp::format(",double a{}", a);
        return out + ")";
    }

    /* The program's value as one C expression: the interpreter's stack is
       simulated over trees, so each operation is done on the operands the
       interpreter pops, in its order, and TinyCC keeps temporaries in
       registers instead of a stack array. */
    Result<> program(const std::vector<int> &p, int index, bool user)
    {
        const Model &m = s.model();
        if (p.empty() || p.size()>MAXEXPLEN) return refuse("an invalid program length");
        std::vector<Node> stack;
        std::vector<Frame> frames;
        auto pop = [&] { Node n = std::move(stack.back()); stack.pop_back(); return n; };
        for (size_t pc = 0; pc < p.size();) {
            while (!frames.empty() && frames.back().has_then && frames.back().end_pc == pc) {
                Frame f = std::move(frames.back());
                frames.pop_back();
                if (stack.size() != f.base+1) return refuse("inconsistent conditional stacks");
                Node other = pop();
                stack.push_back({xpp::format("({}==0.0?{}:{})", f.cond.text, other.text, f.then_value.text), false});
            }
            if (!frames.empty() && frames.back().has_then && pc > frames.back().end_pc) return refuse("an invalid conditional jump");
            const int op = p[pc], type = op / MAXTYPE, in = op % MAXTYPE;
            size_t next = pc + 1;
            auto need = [&](size_t n) { return stack.size() >= n; };
            if (op == ENDEXP) {
                if (stack.size() != 1 || !frames.empty()) return refuse("an invalid result stack");
                source += signature(index, user) + xpp::format("{{return {};}}\n", stack[0].text);
                return {};
            } else if (op == NUMSYM) {
                if (p.size() - pc < 3) return refuse("a truncated number");
                stack.push_back({xpp::format("N[{}]", numbers.size()), true});
                numbers.push_back(expr::number_from_halves(p[pc+2], p[pc+1]));
                next += 2;
            } else if (op == MYIF) {
                if (next >= p.size() || p[next] < 0 || !need(1)) return refuse("an invalid conditional jump");
                const size_t target = next + 1 + static_cast<size_t>(p[next]);
                if (target >= p.size()) return refuse("an out-of-range conditional jump");
                Node cond = pop();
                frames.push_back({std::move(cond), {}, stack.size(), target});
                ++next;
            } else if (op == MYTHEN) {
                if (next >= p.size() || p[next] < 0 || frames.empty() || frames.back().has_then) return refuse("an invalid conditional jump");
                Frame &f = frames.back();
                const size_t end = next + 1 + static_cast<size_t>(p[next]);
                ++next;
                if (next != f.else_pc || end >= p.size() || end < next || stack.size() != f.base+1) return refuse("an invalid conditional jump");
                if (frames.size() > 1) {
                    const Frame &outer = frames[frames.size()-2];
                    if (end > (outer.has_then ? outer.end_pc : outer.else_pc-2)) return refuse("an invalid conditional jump");
                }
                f.then_value = pop();
                f.end_pc = end;
                f.has_then = true;
            } else if (op == MYELSE) {
                /* the else branch starts here; nothing to emit */
            } else if (op == ENDFUN) {
                if (!user || next >= p.size() || p[next] != m.narg_fun[index]) return refuse("an invalid function end");
                ++next;
            } else if (type == CONTYPE || type == VARTYPE) {
                const size_t bound = type == CONTYPE ? s.parser.constants.size() : s.parser.variables.size();
                if (in < 0 || static_cast<size_t>(in) >= bound) return refuse("an out-of-range operand");
                stack.push_back({xpp::format("{}[{}]", type == CONTYPE ? "c" : "v", in), true});
            } else if (type == USTACKTYPE) {
                if (!user || in < 0 || in >= m.narg_fun[index]) return refuse("an out-of-range argument");
                stack.push_back({xpp::format("a{}", in), true});
            } else if (type == FUN1TYPE) {
                if (in < 0 || static_cast<size_t>(in) >= expr::fun1.size() || !expr::fun1[in]) return refuse("an invalid unary function");
                if (!need(1)) return refuse("a unary stack underflow");
                unary.insert(in);
                Node x = pop();
                stack.push_back({xpp::format("f1_{}({})", in, x.text), false});
            } else if (type == FUN2TYPE) {
                if (in < 0 || static_cast<size_t>(in) >= expr::fun2.size()) return refuse("an out-of-range binary function");
                if (!need(2)) return refuse("a binary stack underflow");
                Node right = pop(), left = pop();
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
                stack.push_back({std::move(code), false});
            } else if (type == UFUNTYPE) {
                if (in < 0 || in >= m.nfun || next >= p.size() || p[next] != m.narg_fun[in]) return refuse("an invalid user function call");
                const int n = p[next++];
                if (n < 0 || !need(static_cast<size_t>(n))) return refuse("a function stack underflow");
                if (user) calls[index].insert(in);
                std::string code = xpp::format("u{}(c,v", in);
                const size_t first = stack.size() - static_cast<size_t>(n);
                for (size_t a = first; a < stack.size(); ++a) code += "," + stack[a].text;
                stack.resize(first);
                stack.push_back({code + ")", false});
            } else {
                const char *reason = type == TABTYPE ? "uses a lookup table" : type == NETTYPE ? "uses a network" : type == VECTYPE ? "uses a vectorizer" : type == KERTYPE ? "uses a kernel" : op == SUMSYM ? "uses a sum" : op == RANDUNI || op == RANDPOI || op == RANDNORM ? "uses random draws" : op == ENDSET ? "sets a variable" : type == SVARTYPE || type == SCONTYPE || op == ENDDELAY || op == ENDDELSHFT || op == ENDSHIFT || op == ENDISHIFT ? "uses delays or shifts" : op == INDXCOM ? "uses the vector index" : "uses an unsupported instruction";
                return refuse(reason);
            }
            if (stack.size() > EXPR_STACK) return refuse("an out-of-range stack");
            pc = next;
        }
        return refuse("an unterminated program");
    }
};
}

Result<> compile_model(Session &s)
{
    Model &m = s.model();
    m.native_program.reset();
    m.native_functions.fill(nullptr);
    if (!program.compile) return {};
    Generator g{s, {}, {}, {}, {}, {}};
    /* Expressions owned by these kinds run outside Model::programs. Keep
       one execution mode for the entire model until W259 supports them. */
    for (const auto &st : m.statements) {
        const char *reason = nullptr;
        switch (st.kind) {
        case odex::Statement::Kind::Volterra: reason="uses an integral equation"; break;
        case odex::Statement::Kind::Markov: reason="uses a Markov chain"; break;
        case odex::Statement::Kind::Wiener: reason="uses Wiener noise"; break;
        case odex::Statement::Kind::Event: reason="uses an event"; break;
        case odex::Statement::Kind::Boundary: reason="uses a boundary condition"; break;
        case odex::Statement::Kind::Derived: reason="uses a derived parameter"; break;
        case odex::Statement::Kind::Solv:
        case odex::Statement::Kind::Dae: reason="uses an algebraic equation"; break;
        case odex::Statement::Kind::Table: reason="uses a lookup table"; break;
        case odex::Statement::Kind::Network: reason="uses a network"; break;
        case odex::Statement::Kind::Vector: reason="uses a vectorizer"; break;
        default: break;
        }
        if (reason) { g.place=model_place(m,st.pos); return g.refuse(reason); }
    }
    if (m.nfun < 0 || static_cast<size_t>(m.nfun) > m.ufun_programs.size()) return g.refuse("an out-of-range function count");
    g.calls.resize(static_cast<size_t>(m.nfun));
    for (int i=0; i<m.nfun; ++i) {
        g.place=model_place(m,m.ufun_names[i]);
        if (m.narg_fun[i]<0 || m.narg_fun[i]>EXPR_STACK) return g.refuse("an out-of-range argument count");
        g.source += g.signature(i,true)+";\n";
    }
    Place first;
    for (size_t i=0; i<m.programs.size(); ++i) {
        if (m.programs[i].empty()) continue;
        g.place=model_place(m, i < static_cast<size_t>(m.node) ? m.uvar_names[i] : i < static_cast<size_t>(m.node+m.fix_var) ? m.fixinfo[i-m.node].name : m.uvar_names[i-m.fix_var]);
        if (first.file.empty()) first=g.place;
        if (auto r=g.program(m.programs[i],static_cast<int>(i),false); !r) return r;
    }
    for (int i=0; i<m.nfun; ++i) {
        g.place=model_place(m,m.ufun_names[i]);
        if (auto r=g.program(m.ufun_programs[i],i,true); !r) return r;
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
        functions[i]=reinterpret_cast<double (*)(double *,double *)>(*address);
    }
    m.native_program=std::move(*compiled);
    m.native_functions=functions;
    const auto equations=std::count_if(functions.begin(),functions.end(),[](auto f){return f!=nullptr;});
    log(XPP_LOG_INFO,"Compiled {} equations and {} user functions\n",equations,m.nfun);
    return {};
}
}
