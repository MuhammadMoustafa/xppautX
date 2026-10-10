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
#include <chrono>
#include "xpp_log.h"

namespace xpp {
namespace {
struct Generator {
    Session &s;
    Place place;
    std::string source;
    std::set<int> unary, binary;
    std::vector<std::set<int>> calls;

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

    Result<> program(const std::vector<int> &p, int index, bool user)
    {
        const Model &m = s.model();
        /* Each token is visited once. Depths at forward joins must agree;
           this also rejects jumps into immediates, underflow and loops. */
        std::vector<int> depth(p.size(), -1);
        if (p.empty() || p.size()>MAXEXPLEN) return refuse("an invalid program length");
        depth[0] = 0;
        std::string body;
        bool ended = false;
        for (size_t pc = 0; pc < p.size();) {
            const int op = p[pc], type = op / MAXTYPE, in = op % MAXTYPE;
            const int d = depth[pc];
            if (d < 0) return refuse("an unreachable instruction");
            size_t next = pc + 1;
            int after = d;
            std::string code;
            auto push = [&](const std::string &value) { code = xpp::format("r[{}]={};", d, value); ++after; };
            auto need = [&](int n) { return d >= n; };
            if (op == ENDEXP) {
                if (d != 1) return refuse("an invalid result stack");
                body += xpp::format("L{}: return r[0];\n", pc);
                ended = true;
                break;
            } else if (op == NUMSYM) {
                if (p.size() - pc < 3) return refuse("a truncated number");
                const auto bits = std::bit_cast<std::uint64_t>(expr::number_from_halves(p[pc+2], p[pc+1]));
                code = xpp::format("{{ union {{ unsigned long long b; double d; }} n; n.b={}ULL; r[{}]=n.d; }}", bits, d);
                ++after;
                next += 2;
            } else if (op == MYIF || op == MYTHEN) {
                if (next >= p.size() || p[next] < 0) return refuse("an invalid conditional jump");
                const size_t target = next + 1 + static_cast<size_t>(p[next]);
                ++next;
                if (target < next || target >= p.size()) return refuse("an out-of-range conditional jump");
                if (op == MYIF) {
                    if (!need(1)) return refuse("a conditional stack underflow");
                    --after;
                    code = xpp::format("if(r[{}]==0.0) goto L{};", after, target);
                } else code = xpp::format("goto L{};", target);
                if (depth[target] != -1 && depth[target] != after) return refuse("inconsistent conditional stacks");
                depth[target] = after;
                if (op == MYTHEN) {
                    body += xpp::format("L{}: {};\n", pc, code);
                    pc = next;
                    /* The else entry is normally the following token. */
                    continue;
                }
            } else if (op == MYELSE) {
                code = ";";
            } else if (op == ENDFUN) {
                if (!user || next >= p.size() || p[next] != m.narg_fun[index]) return refuse("an invalid function end");
                ++next;
                code = ";";
            } else if (type == CONTYPE || type == VARTYPE) {
                const size_t bound = type == CONTYPE ? s.parser.constants.size() : s.parser.variables.size();
                if (in < 0 || static_cast<size_t>(in) >= bound) return refuse("an out-of-range operand");
                push(xpp::format("{}[{}]", type == CONTYPE ? "c" : "v", in));
            } else if (type == USTACKTYPE) {
                if (!user || in < 0 || in >= m.narg_fun[index]) return refuse("an out-of-range argument");
                push(xpp::format("a{}", in));
            } else if (type == FUN1TYPE) {
                if (in < 0 || static_cast<size_t>(in) >= expr::fun1.size() || !expr::fun1[in]) return refuse("an invalid unary function");
                if (!need(1)) return refuse("a unary stack underflow");
                unary.insert(in);
                code = xpp::format("r[{}]=f1_{}(r[{}]);", d-1, in, d-1);
            } else if (type == FUN2TYPE) {
                if (in < 0 || static_cast<size_t>(in) >= expr::fun2.size()) return refuse("an out-of-range binary function");
                if (!need(2)) return refuse("a binary stack underflow");
                --after;
                const std::string left = xpp::format("r[{}]", d-2), right = xpp::format("r[{}]", d-1);
                if (in == 0 || in == 2) code = xpp::format("{}={}{}{};", left, right, in == 0 ? "+" : "*", left);
                else if (in == 1 || in == expr::IEEE_DIVIDE) code = xpp::format("{}={}{}{};", left, left, in == 1 ? "-" : "/", right);
                else if (in == 3) {
                    const auto bits = std::bit_cast<std::uint64_t>(expr::ZERO_DIVISOR);
                    code = xpp::format("{{ union {{ unsigned long long b; double d; }} n; n.b={}ULL; if({}==0.0) {}=n.d; {}={}/{}; }}", bits, right, right, left, left, right);
                } else {
                    if (!expr::fun2[in]) return refuse("an invalid binary function");
                    binary.insert(in);
                    code = xpp::format("{}=f2_{}({},{});", left, in, left, right);
                }
            } else if (type == UFUNTYPE) {
                if (in < 0 || in >= m.nfun || next >= p.size() || p[next] != m.narg_fun[in]) return refuse("an invalid user function call");
                const int n = p[next++];
                if (n < 0 || !need(n)) return refuse("a function stack underflow");
                if (user) calls[index].insert(in);
                code = xpp::format("r[{}]=u{}(c,v", d-n, in);
                for (int a = 0; a < n; ++a) code += xpp::format(",r[{}]", d-n+a);
                code += ");";
                after = d-n+1;
            } else {
                const char *reason = type == TABTYPE ? "uses a lookup table" : type == NETTYPE ? "uses a network" : type == VECTYPE ? "uses a vectorizer" : type == KERTYPE ? "uses a kernel" : op == SUMSYM ? "uses a sum" : op == RANDUNI || op == RANDPOI || op == RANDNORM ? "uses random draws" : op == ENDSET ? "sets a variable" : type == SVARTYPE || type == SCONTYPE || op == ENDDELAY || op == ENDDELSHFT || op == ENDSHIFT || op == ENDISHIFT ? "uses delays or shifts" : op == INDXCOM ? "uses the vector index" : "uses an unsupported instruction";
                return refuse(reason);
            }
            if (after < 0 || after > EXPR_STACK) return refuse("an out-of-range stack");
            if (next >= p.size()) return refuse("an unterminated program");
            /* Never allow a jump into the immediate words just consumed. */
            for (size_t k = pc+1; k < next; ++k)
                if (depth[k] != -1) return refuse("a jump into an immediate operand");
            if (depth[next] != -1 && depth[next] != after) return refuse("inconsistent conditional stacks");
            depth[next] = after;
            body += xpp::format("L{}: {};\n", pc, code);
            pc = next;
        }
        if (!ended) return refuse("an unterminated program");
        source += signature(index, user) + xpp::format("{{ double r[{}];\n", EXPR_STACK) + body + "}\n";
        return {};
    }
};
}

Result<> compile_model(Session &s)
{
    const auto started=std::chrono::steady_clock::now();
    Model &m = s.model();
    m.native_program.reset();
    m.native_functions.fill(nullptr);
    if (m.no_compile) return {};
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
    std::string declarations;
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
    std::vector<tcc::Symbol> symbols;
    for (size_t i=0; i<names.size(); ++i) symbols.push_back({names[i].c_str(),addresses[i]});
    auto compiled=tcc::Program::compile(declarations+g.source,symbols,first);
    if (!compiled) { g.place=compiled.error().place; return g.refuse("TinyCC: "+compiled.error().what); }
    decltype(m.native_functions) functions{};
    for (size_t i=0; i<m.programs.size(); ++i) {
        if (m.programs[i].empty()) continue;
        auto address=(*compiled)->function(xpp::format("p{}",i));
        if (!address) { g.place=address.error().place; return g.refuse("TinyCC: "+address.error().what); }
        functions[i]=reinterpret_cast<double (*)(double *,double *)>(*address);
    }
    m.native_program=std::move(*compiled);
    m.native_functions=functions;
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    const auto equations=std::count_if(functions.begin(),functions.end(),[](auto f){return f!=nullptr;});
    log(XPP_LOG_INFO,"Compiled {} equations and {} user functions in {:.3f} ms\n",equations,m.nfun,elapsed);
    return {};
}
}
