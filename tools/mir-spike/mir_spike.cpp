/* W706 spike: compiles a model's generated right-hand side (the C text
   core/expr_native.cpp hands TinyCC, as a fixture) with TinyCC and with MIR
   and calls both on the same states, comparing every output bit for bit.
     mir_spike [--level N] FIXTURE...   compare; exit 1 on any difference or failure
     mir_spike [--level N] --count tcc|c2mir|direct FIXTURE
                                        only calls (for callgrind --toggle-collect='*call_rhs*')
   Three compilers: TinyCC and MIR through c2mir take the fixture's C text; MIR direct (W711) takes the
   trees the Generator writes for the fixture's model (its manifest's `model` line, loaded here), through
   MIR's API with no C text. Each of the other two is compared with TinyCC.
   N is MIR's optimization level (0-3, default 2, MIR's own default).
   A fixture is <base>.c (the text) and <base>.manifest (see read_manifest);
   tests/mir-spike/README.md says how they were written. The time lines it
   prints (`perf:`) are measured, never a result. */
#include "xpp_mir.h"
#include "../../core/expr_native.h"
#include "../../core/expr_program.h"
#include "../../core/session.h"
#include "../../core/xpp_batch.h"
#include "../../core/xpp_files.h"
#include "../../core/xpp_tcc.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
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
    std::string model; /* the .ode or .odex the text was made from, for the direct path; "" when none */
};

/* node N / nvar N / model PATH (from the repository's root) / c, N, rows: C99 hex floats / f1, f2: function indices /
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
        else if (key == "model") ss >> m.model;
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
    std::unique_ptr<xpp::mir::Program> direct;
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
    auto function = program->function("p_rhs");
    if (!function) {
        std::fprintf(stderr, "%s: no p_rhs\n", who);
        return false;
    }
    /* the table of literals N: the text has it, the direct path puts the numbers into the code */
    if (auto table = program->function("N")) std::copy(m.N.begin(), m.N.end(), static_cast<double *>(*table));
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

/* bit for bit; two NaNs are equal (which NaN an operation passes on is the CPU's, by operand order) */
bool same(const std::vector<double> &a, const std::vector<double> &b)
{
    for (size_t i = 0; i < a.size(); ++i)
        if (std::bit_cast<std::uint64_t>(a[i]) != std::bit_cast<std::uint64_t>(b[i]) && !(std::isnan(a[i]) && std::isnan(b[i]))) return false;
    return true;
}

/* the model's right-hand side as the Generator's trees (W711), its load and walk timed */
std::optional<xpp::RhsIr> trees_of(const std::string &model, double &ms)
{
    /* an .ode is loaded from a scratch copy: its first load writes the .odex beside it (or asks to replace one) */
    const xpp::TempDir scratch;
    std::string path = model;
    if (model.ends_with(".ode")) {
        path = scratch.file(std::filesystem::path(model).filename().string());
        xpp::files::copy(model, path);
    }
    auto cl = xpp::parse_command_line({"mir_spike", path});
    if (!cl) { std::fprintf(stderr, "%s\n", cl.error().text().c_str()); return std::nullopt; }
    auto loaded = xpp::load_model(std::move(*cl), 1);
    if (!loaded) { std::fprintf(stderr, "%s\n", loaded.error().text().c_str()); return std::nullopt; }
    const double t0 = now_ms();
    auto ir = xpp::rhs_ir(**loaded);
    ms = now_ms() - t0;
    if (!ir) { std::fprintf(stderr, "%s: %s\n", path.c_str(), ir.error().text().c_str()); return std::nullopt; }
    return std::move(*ir);
}

/* every state of the fixture, then four of ours: all zero (every division guard), a NaN everywhere,
   all -0.0 and all 1 */
std::vector<std::vector<double>> inputs_of(const Manifest &m)
{
    std::vector<std::vector<double>> inputs;
    for (size_t k = 0; k < m.rows.size() / (m.node + 1); ++k) {
        std::vector<double> v(m.nvar, 0.0);
        std::copy_n(m.rows.begin() + static_cast<long>(k * (m.node + 1)), m.node + 1, v.begin());
        inputs.push_back(std::move(v));
    }
    for (const double fill : {0.0, std::numeric_limits<double>::quiet_NaN(), -0.0, 1.0}) inputs.emplace_back(m.nvar, fill);
    return inputs;
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
    if (first >= argc) { std::fprintf(stderr, "usage: mir_spike [--level N] [--count tcc|c2mir|direct] FIXTURE...\n"); return 2; }
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
        Compiled t, r, d;
        const auto want = [&](const char *backend) { return only.empty() || only == backend; };
        const auto tcc_symbols = symbols_of<xpp::tcc::Symbol>(m, tcc_names);
        const bool have_tcc = !want("tcc") || build(t.tcc, t.rhs, t.ms, m, "TinyCC", [&] {
            return xpp::tcc::Program::compile(source.str(), tcc_symbols, xpp::Place{});
        });
        const auto mir_symbols = symbols_of<xpp::mir::Symbol>(m, mir_names);
        const bool have_c2mir = !want("c2mir") || build(r.mir, r.rhs, r.ms, m, "MIR c2mir", [&] {
            return xpp::mir::Program::compile(source.str(), mir_symbols, xpp::Place{}, level);
        });
        double walk_ms = 0;
        std::optional<xpp::RhsIr> trees;
        if (want("direct") && !m.model.empty()) trees = trees_of(m.model, walk_ms);
        const bool have_direct = !want("direct") || m.model.empty() || (trees && build(d.direct, d.rhs, d.ms, m, "MIR direct", [&] {
            return xpp::mir::Program::compile(*trees, xpp::Place{}, level);
        }));
        if (!have_tcc || !have_c2mir || !have_direct) { ++failures; continue; }
        const bool direct = trees.has_value();
        const auto inputs = inputs_of(m);
        if (!only.empty()) { /* callgrind: one backend, the fixture's states once each */
            Rhs rhs = only == "tcc" ? t.rhs : only == "c2mir" ? r.rhs : d.rhs;
            if (!rhs) { std::fprintf(stderr, "FAIL %s: no %s backend for it\n", base.c_str(), only.c_str()); ++failures; continue; }
            std::vector<double> v(m.nvar), o(m.node);
            const size_t states = m.rows.size() / (m.node + 1);
            for (size_t k = 0; k < states; ++k) {
                v = inputs[k];
                call_rhs(rhs, const_cast<double *>(m.c.data()), v.data(), o.data());
            }
            std::printf("calls %zu\n", states);
            continue;
        }
        size_t differing_c2mir = 0, differing_direct = 0;
        for (const auto &input : inputs) {
            std::vector<double> vt = input, vr = input, vd = input, ot(m.node, 0.0), orr(m.node, 0.0), od(m.node, 0.0);
            call_rhs(t.rhs, const_cast<double *>(m.c.data()), vt.data(), ot.data());
            call_rhs(r.rhs, const_cast<double *>(m.c.data()), vr.data(), orr.data());
            if (!same(ot, orr) || !same(vt, vr)) ++differing_c2mir;
            if (direct) {
                call_rhs(d.rhs, const_cast<double *>(m.c.data()), vd.data(), od.data());
                if (!same(ot, od) || !same(vt, vd)) ++differing_direct;
            }
        }
        std::printf("%s: %zu states, c2mir: %zu differ%s", base.c_str(), inputs.size(), differing_c2mir, differing_c2mir ? " FAIL" : "");
        if (direct) std::printf(", direct: %zu differ%s\n", differing_direct, differing_direct ? " FAIL" : "");
        else std::printf(", direct: no model\n");
        std::printf("perf: %s compile ms: TinyCC %.3f c2mir %.3f (front %.3f, generate %.3f)", base.c_str(), t.ms, r.ms,
                    r.mir->timing().front_ms, r.mir->timing().generate_ms);
        if (direct) std::printf(" direct %.3f (lower %.3f, generate %.3f; trees of every program %.3f)", d.ms, d.direct->timing().front_ms,
                                d.direct->timing().generate_ms, walk_ms);
        std::printf("\n");
        if (differing_c2mir || differing_direct) ++failures;
    }
    return failures ? 1 : 0;
}
