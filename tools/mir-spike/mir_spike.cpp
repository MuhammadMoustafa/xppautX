/* W706 spike: compiles a model's generated right-hand side (the C text
   core/expr_native.cpp hands TinyCC, as a fixture) with TinyCC and with MIR
   and calls both on the same states, comparing every output bit for bit.
     mir_spike [--level N] FIXTURE...   compare; exit 1 on any difference or failure
     mir_spike [--level N] --count tcc|mir FIXTURE
                                        only calls (for callgrind --toggle-collect='*call_rhs*')
   N is MIR's optimization level (0-3, default 2, MIR's own default).
   A fixture is <base>.c (the text) and <base>.manifest (see read_manifest);
   tests/mir-spike/README.md says how they were written. The time lines it
   prints (`perf:`) are measured, never a result. */
#include "xpp_mir.h"
#include "../../core/expr_program.h"
#include "../../core/xpp_tcc.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Rhs = void (*)(double *, double *, void *, double *);

struct Manifest {
    size_t node = 0, nvar = 0;
    std::vector<double> c, N, rows; /* rows: node+1 values each (t and the states) */
    std::vector<int> f1, f2;
    std::vector<std::string> helpers;
};

/* node N / nvar N / c, N, rows: C99 hex floats / f1, f2: function indices /
   helpers: names of the Session helpers the text declares: only those of
   `helper_stub` (the right-hand side p_rhs never calls them) */
bool read_manifest(const std::string &path, Manifest &m)
{
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key, token;
        ss >> key;
        if (key == "node") ss >> m.node;
        else if (key == "nvar") ss >> m.nvar;
        else if (key == "c") while (ss >> token) m.c.push_back(std::strtod(token.c_str(), nullptr));
        else if (key == "N") while (ss >> token) m.N.push_back(std::strtod(token.c_str(), nullptr));
        else if (key == "row") while (ss >> token) m.rows.push_back(std::strtod(token.c_str(), nullptr));
        else if (key == "f1") { int i; while (ss >> i) m.f1.push_back(i); }
        else if (key == "f2") { int i; while (ss >> i) m.f2.push_back(i); }
        else if (key == "helpers") while (ss >> token) m.helpers.push_back(token);
    }
    return m.node > 0 && m.nvar > m.node && m.rows.size() % (m.node + 1) == 0 && !m.rows.empty();
}

double now_ms()
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* the user-function bridges' helpers (core/expr_native.cpp): declared in the
   text, called by no p_rhs; a fixture that needs another has none here */
double h_arg_stub(void *, int) { return 0; }
double h_end_stub(void *, double v, int) { return v; }
const void *helper_stub(const std::string &name)
{
    if (name == "h_arg") return reinterpret_cast<const void *>(&h_arg_stub);
    if (name == "h_end") return reinterpret_cast<const void *>(&h_end_stub);
    return nullptr;
}

/* one compiler's p_rhs, its table N filled; owns the compiled program */
struct Compiled {
    std::unique_ptr<xpp::tcc::Program> tcc;
    std::unique_ptr<xpp::mir::Program> mir;
    Rhs rhs = nullptr;
    double ms = 0;
};

template <class Program, class Compile>
bool build(std::unique_ptr<Program> &program, Rhs &rhs, double &ms, const Manifest &m, const char *who,
           Compile compile)
{
    const double t0 = now_ms();
    auto compiled = compile();
    ms = now_ms() - t0;
    if (!compiled) {
        std::fprintf(stderr, "%s: %s\n", who, compiled.error().text().c_str());
        return false;
    }
    program = std::move(*compiled);
    auto table = program->function("N");
    auto function = program->function("p_rhs");
    if (!table || !function) {
        std::fprintf(stderr, "%s: no N or p_rhs\n", who);
        return false;
    }
    std::copy(m.N.begin(), m.N.end(), static_cast<double *>(*table));
    rhs = reinterpret_cast<Rhs>(*function);
    return true;
}

/* the generated code's externals: the table of xpp::expr's correctly rounded
   functions, the same addresses for both compilers */
template <class Symbol>
std::vector<Symbol> symbols_of(const Manifest &m, std::vector<std::string> &names)
{
    for (int i : m.f1) names.push_back("f1_" + std::to_string(i));
    for (int i : m.f2) names.push_back("f2_" + std::to_string(i));
    for (const auto &h : m.helpers) names.push_back(h);
    std::vector<Symbol> symbols;
    size_t k = 0;
    for (int i : m.f1) symbols.push_back({names[k++].c_str(), reinterpret_cast<const void *>(xpp::expr::fun1[static_cast<size_t>(i)])});
    for (int i : m.f2) symbols.push_back({names[k++].c_str(), reinterpret_cast<const void *>(xpp::expr::fun2[static_cast<size_t>(i)])});
    for (const auto &h : m.helpers) symbols.push_back({names[k++].c_str(), helper_stub(h)});
    return symbols;
}

__attribute__((noinline)) void call_rhs(Rhs rhs, double *c, double *v, double *o)
{
    rhs(c, v, nullptr, o);
}

bool same(const std::vector<double> &a, const std::vector<double> &b)
{
    for (size_t i = 0; i < a.size(); ++i)
        if (std::bit_cast<std::uint64_t>(a[i]) != std::bit_cast<std::uint64_t>(b[i])) return false;
    return true;
}

/* what the adapter must refuse, as TinyCC's does (tests/test_tcc.cpp) */
bool adapter_refusals()
{
    bool ok = true;
    const auto refused = [&](const char *what, const std::string &source, std::span<const xpp::mir::Symbol> symbols) {
        auto r = xpp::mir::Program::compile(source, symbols, xpp::Place{}, 2);
        if (r) { std::fprintf(stderr, "FAIL: MIR accepted %s\n", what); ok = false; }
    };
    refused("a syntax error", "double broken( {", {});
    refused("an #include", "#include <stdio.h>\ndouble f(void){return 1.0;}\n", {});
    refused("an unresolved symbol", "extern double nowhere(double);\ndouble f(double x){return nowhere(x);}\n", {});
    return ok;
}

} // namespace

int main(int argc, char **argv)
{
    int first = 1;
    unsigned level = 2; /* MIR's default: W472 measured 0.48x of TinyCC's model code at it (O1 0.95x) */
    std::string only;
    if (argc > 2 && std::string(argv[1]) == "--level") { level = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)); first = 3; }
    if (argc > first + 1 && std::string(argv[first]) == "--count") { only = argv[first + 1]; first += 2; }
    if (first >= argc) { std::fprintf(stderr, "usage: mir_spike [--count tcc|mir] FIXTURE...\n"); return 2; }
    int failures = 0;
    if (only.empty() && !adapter_refusals()) ++failures;
    for (int a = first; a < argc; ++a) {
        const std::string base = argv[a];
        Manifest m;
        std::ifstream text(base + ".c");
        std::stringstream source;
        source << text.rdbuf();
        if (!text || !read_manifest(base + ".manifest", m) ||
            std::any_of(m.helpers.begin(), m.helpers.end(), [](const std::string &h) { return !helper_stub(h); })) {
            std::fprintf(stderr, "FAIL %s: unreadable fixture or one that needs a Session helper\n", base.c_str());
            ++failures;
            continue;
        }
        std::vector<std::string> tcc_names, mir_names; /* the symbols point into them */
        Compiled t, r;
        const auto tcc_symbols = symbols_of<xpp::tcc::Symbol>(m, tcc_names);
        const bool have_tcc = only == "mir" || build(t.tcc, t.rhs, t.ms, m, "TinyCC", [&] {
            return xpp::tcc::Program::compile(source.str(), tcc_symbols, xpp::Place{});
        });
        const auto mir_symbols = symbols_of<xpp::mir::Symbol>(m, mir_names);
        const bool have_mir = only == "tcc" || build(r.mir, r.rhs, r.ms, m, "MIR", [&] {
            return xpp::mir::Program::compile(source.str(), mir_symbols, xpp::Place{}, level);
        });
        if (!have_tcc || !have_mir) { ++failures; continue; }
        const size_t states = m.rows.size() / (m.node + 1);
        if (!only.empty()) { /* callgrind: one backend, every state once */
            Rhs rhs = only == "tcc" ? t.rhs : r.rhs;
            std::vector<double> v(m.nvar), o(m.node);
            for (size_t k = 0; k < states; ++k) {
                std::fill(v.begin(), v.end(), 0.0);
                std::copy_n(m.rows.begin() + static_cast<long>(k * (m.node + 1)), m.node + 1, v.begin());
                call_rhs(rhs, const_cast<double *>(m.c.data()), v.data(), o.data());
            }
            std::printf("calls %zu\n", states);
            continue;
        }
        size_t differing = 0;
        for (size_t k = 0; k < states; ++k) {
            std::vector<double> vt(m.nvar, 0.0), vm(m.nvar, 0.0), ot(m.node, 0.0), om(m.node, 0.0);
            std::copy_n(m.rows.begin() + static_cast<long>(k * (m.node + 1)), m.node + 1, vt.begin());
            vm = vt;
            call_rhs(t.rhs, const_cast<double *>(m.c.data()), vt.data(), ot.data());
            call_rhs(r.rhs, const_cast<double *>(m.c.data()), vm.data(), om.data());
            if (!same(ot, om) || !same(vt, vm)) ++differing;
        }
        std::printf("%s: %zu states, %zu differ%s\n", base.c_str(), states, differing, differing ? " FAIL" : ", every output bit-identical");
        std::printf("perf: %s compile ms: TinyCC %.3f MIR %.3f\n", base.c_str(), t.ms, r.ms);
        if (differing) ++failures;
    }
    return failures ? 1 : 0;
}
