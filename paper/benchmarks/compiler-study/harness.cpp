// W472 study harness: loads a model's generated C (dumped by xppautX with the study patch,
// XPP_DUMP_NATIVE) with one of several compilers and calls the right-hand side on sampled states.
// Usage: harness BACKEND MODELDIR MODE [K] [OUT]
//   BACKEND: tcc | mir0 mir1 mir2 mir3 | so:<path to a .so built from modelN.c>
//   MODE: count (one pass, for callgrind) | time (median ns/call) | dump (write results to OUT)
//         | compile (median compile ms, tcc and mir only)
#include "expr_program.h"
#include "xpp_tcc.h"
#include "xpp_error.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
extern "C" {
#include "mir.h"
#include "mir-gen.h"
#include "c2mir/c2mir.h"
}

using Eq = double (*)(double *, double *, void *, const int *);
using Fixed = void (*)(double *, double *, void *);

// Every external call of the generated code goes through these, one per index, to the same
// table xppautX's TinyCC code reads (xpp::expr::fun1/fun2): the same correctly rounded functions.
#define F1(i) extern "C" double f1_##i(double x) { return xpp::expr::fun1[i](x); }
#define F2(i) extern "C" double f2_##i(double x, double y) { return xpp::expr::fun2[i](x, y); }
F1(0) F1(1) F1(2) F1(3) F1(4) F1(5) F1(6) F1(7) F1(8) F1(9) F1(10) F1(11) F1(12) F1(13) F1(14) F1(15)
F1(16) F1(17) F1(18) F1(19) F1(20) F1(21) F1(22) F1(23) F1(24) F1(25)
F2(4) F2(5) F2(6) F2(7) F2(8) F2(9) F2(10) F2(11) F2(12) F2(13) F2(14) F2(15) F2(16) F2(17) F2(18)
F2(19) F2(20) F2(21) F2(22)
// The Session helpers (tables, networks, shifts, user-function arguments) need a Session: stubs
// of plain arithmetic, the same for every compiler.
extern "C" {
double h_tab(void *, double x, int i) { return x * 0.5 + i; }
double h_tabs(void *, double x, int i) { return x * 0.25 + i; }
double h_net(void *, double x, int i) { return x * 0.5 + i; }
double h_vec(void *, double x, int i) { return x * 0.5 + i; }
double h_ker(void *, int i) { return i; }
double h_sft(void *, double shift, double v) { return v * 0.5 + shift; }
double h_arg(void *, int i) { return i; }
double h_end(void *, double v, int) { return v; }
}

static double now_ms() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Manifest {
    int node = 0, fix = 0;
    size_t nconst = 0, nvar = 0;
    std::vector<double> c, N;
    std::vector<int> f1, f2;
    std::vector<std::string> helpers;
};

static Manifest read_manifest(const std::string &path) {
    Manifest m;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key, tok;
        ss >> key;
        if (key == "node") ss >> m.node;
        else if (key == "fix") ss >> m.fix;
        else if (key == "nconst") ss >> m.nconst;
        else if (key == "nvar") ss >> m.nvar;
        else if (key == "c") while (ss >> tok) m.c.push_back(strtod(tok.c_str(), nullptr));
        else if (key == "N") while (ss >> tok) m.N.push_back(strtod(tok.c_str(), nullptr));
        else if (key == "f1") { int i; while (ss >> i) m.f1.push_back(i); }
        else if (key == "f2") { int i; while (ss >> i) m.f2.push_back(i); }
        else if (key == "helpers") while (ss >> tok) m.helpers.push_back(tok);
    }
    return m;
}

static std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct Loaded {
    std::vector<Eq> eq;
    Fixed fixed = nullptr;
    std::function<void()> keep; // owns the compiler's state
    std::shared_ptr<void> hold;
};

static std::vector<std::string> wanted_symbols(const Manifest &m) {
    std::vector<std::string> names;
    for (int i : m.f1) names.push_back("f1_" + std::to_string(i));
    for (int i : m.f2) names.push_back("f2_" + std::to_string(i));
    for (auto &h : m.helpers) names.push_back(h);
    return names;
}

static std::vector<std::string> entry_names(const Manifest &m) {
    std::vector<std::string> n;
    for (int i = 0; i < m.node; ++i) n.push_back("p_eq" + std::to_string(i));
    return n;
}

static Loaded load_tcc(const Manifest &m, const std::string &src, double *compile_ms) {
    Loaded L;
    auto names = wanted_symbols(m);
    std::vector<xpp::tcc::Symbol> syms;
    for (auto &n : names) syms.push_back({n.c_str(), dlsym(RTLD_DEFAULT, n.c_str())});
    double t0 = now_ms();
    auto prog = xpp::tcc::Program::compile(src, syms, xpp::Place{});
    if (compile_ms) *compile_ms = now_ms() - t0;
    if (!prog) { fprintf(stderr, "tcc: %s\n", prog.error().text().c_str()); exit(2); }
    auto table = (*prog)->function("N");
    std::copy(m.N.begin(), m.N.end(), static_cast<double *>(*table));
    for (auto &n : entry_names(m)) L.eq.push_back(reinterpret_cast<Eq>(*(*prog)->function(n)));
    if (auto f = (*prog)->function("p_fixed")) L.fixed = reinterpret_cast<Fixed>(*f);
    L.hold = std::shared_ptr<void>(prog->release(), [](void *p) { delete static_cast<xpp::tcc::Program *>(p); });
    return L;
}

static void *resolver(const char *name) { return dlsym(RTLD_DEFAULT, name); }

struct Src { const char *s; size_t pos; };
static int src_getc(void *d) { auto *x = static_cast<Src *>(d); return x->s[x->pos] ? (unsigned char)x->s[x->pos++] : EOF; }

static Loaded load_mir(const Manifest &m, const std::string &src, int level, double *compile_ms) {
    Loaded L;
    double t0 = now_ms();
    MIR_context_t ctx = MIR_init();
    c2mir_init(ctx);
    c2mir_options opts{};
    opts.message_file = stderr;
    opts.ignore_warnings_p = 1;
    Src s{src.c_str(), 0};
    if (!c2mir_compile(ctx, &opts, src_getc, &s, "model.c", nullptr)) { fprintf(stderr, "mir: compile failed\n"); exit(2); }
    MIR_module_t mod = DLIST_TAIL(MIR_module_t, *MIR_get_module_list(ctx));
    MIR_load_module(ctx, mod);
    MIR_gen_init(ctx);
    MIR_gen_set_optimize_level(ctx, (unsigned)level);
    MIR_link(ctx, MIR_set_gen_interface, resolver);
    if (compile_ms) *compile_ms = now_ms() - t0;
    std::vector<std::string> need = entry_names(m);
    std::vector<MIR_item_t> items(need.size(), nullptr);
    MIR_item_t fixed = nullptr, table = nullptr;
    for (MIR_item_t it = DLIST_HEAD(MIR_item_t, mod->items); it; it = DLIST_NEXT(MIR_item_t, it)) {
        if (it->item_type == MIR_func_item) {
            const char *nm = it->u.func->name;
            if (!strcmp(nm, "p_fixed")) fixed = it;
            for (size_t i = 0; i < need.size(); ++i) if (need[i] == nm) items[i] = it;
        } else if (it->item_type == MIR_bss_item && it->u.bss->name && !strcmp(it->u.bss->name, "N")) table = it;
        else if (it->item_type == MIR_data_item && it->u.data->name && !strcmp(it->u.data->name, "N")) table = it;
    }
    if (!table) { fprintf(stderr, "mir: no N\n"); exit(2); }
    std::copy(m.N.begin(), m.N.end(), static_cast<double *>(table->addr));
    for (auto *it : items) { if (!it) { fprintf(stderr, "mir: missing equation\n"); exit(2); } L.eq.push_back(reinterpret_cast<Eq>(it->addr)); }
    if (fixed) L.fixed = reinterpret_cast<Fixed>(fixed->addr);
    return L; // the context is kept for the life of the process
}

static Loaded load_so(const Manifest &m, const std::string &path) {
    Loaded L;
    void *h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); exit(2); }
    double *N = static_cast<double *>(dlsym(h, "N"));
    std::copy(m.N.begin(), m.N.end(), N);
    for (auto &n : entry_names(m)) L.eq.push_back(reinterpret_cast<Eq>(dlsym(h, n.c_str())));
    L.fixed = reinterpret_cast<Fixed>(dlsym(h, "p_fixed"));
    return L;
}

// The one right-hand side: the fixed variables, then each equation, in xppautX's order (my_rhs.cpp).
extern "C" __attribute__((noinline)) void call_rhs(const Loaded *L, double *c, double *v, double *ydot, int node) {
    if (L->fixed) L->fixed(c, v, nullptr);
    for (int i = 0; i < node; ++i) ydot[i] = L->eq[i](c, v, nullptr, nullptr);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage\n"); return 1; }
    std::string backend = argv[1], dir = argv[2], mode = argv[3];
    size_t K = argc > 4 ? strtoul(argv[4], nullptr, 10) : 200;
    Manifest m = read_manifest(dir + "/model0.manifest");
    std::string src = slurp(dir + "/model0.c");
    if (m.fix > 0 && src.find("p_fixed") == std::string::npos) { fprintf(stderr, "no p_fixed\n"); return 2; }

    if (mode == "compile") {
        std::vector<double> t;
        for (int r = 0; r < 7; ++r) {
            double ms = 0;
            if (backend == "tcc") { load_tcc(m, src, &ms); }
            else if (backend.rfind("mir", 0) == 0) { load_mir(m, src, backend[3] - '0', &ms); }
            t.push_back(ms);
        }
        std::sort(t.begin(), t.end());
        printf("%.3f\n", t[t.size() / 2]);
        return 0;
    }

    Loaded L;
    if (backend == "tcc") L = load_tcc(m, src, nullptr);
    else if (backend.rfind("mir", 0) == 0) L = load_mir(m, src, backend[3] - '0', nullptr);
    else if (backend.rfind("so:", 0) == 0) L = load_so(m, backend.substr(3));
    else { fprintf(stderr, "backend?\n"); return 1; }

    // states: the rows of the model's output.dat (time, then the states), spread evenly
    std::vector<std::vector<double>> rows;
    {
        std::ifstream in(dir + "/output.dat");
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ss(line);
            std::vector<double> r(1 + m.node);
            bool ok = true;
            for (auto &x : r) if (!(ss >> x)) { ok = false; break; }
            if (ok) rows.push_back(std::move(r));
        }
    }
    if (rows.empty()) { fprintf(stderr, "no states\n"); return 2; }
    std::vector<std::vector<double>> st;
    K = std::min(K, rows.size());
    for (size_t k = 0; k < K; ++k) st.push_back(rows[k * rows.size() / K]);

    // YPAD (doubles) shifts ydot against v: a model's time can depend on the two arrays' addresses
    // modulo 4096 (store-to-load false dependences), so the study runs several pads.
    const size_t ypad = getenv("YPAD") ? strtoul(getenv("YPAD"), nullptr, 10) : 0;
    std::vector<double> c = m.c, v(m.nvar, 0.0), ydot_store(m.node + ypad, 0.0);
    double *ydot_p = ydot_store.data() + ypad;
    struct { double *p; double *data() { return p; } } ydot{ydot_p};
    auto set_state = [&](const std::vector<double> &r) { for (int i = 0; i <= m.node; ++i) v[i] = r[i]; };

    if (mode == "count") { // callgrind: only call_rhs is collected
        for (auto &r : st) { set_state(r); call_rhs(&L, c.data(), v.data(), ydot.data(), m.node); }
        printf("calls %zu\n", st.size());
    } else if (mode == "time") {
        std::vector<double> per;
        double p0 = now_ms();
        for (auto &s : st) { set_state(s); call_rhs(&L, c.data(), v.data(), ydot.data(), m.node); }
        // a batch lasts about 20 ms, 31 batches, the median of them
        size_t reps = std::max<size_t>(1, (size_t)(20.0 / std::max(now_ms() - p0, 1e-3)));
        for (int rep = 0; rep < 31; ++rep) {
            double t0 = now_ms();
            for (size_t r = 0; r < reps; ++r)
                for (auto &s : st) { set_state(s); call_rhs(&L, c.data(), v.data(), ydot.data(), m.node); }
            per.push_back((now_ms() - t0) * 1e6 / (double)(reps * K));
        }
        std::sort(per.begin(), per.end());
        printf("%.1f\n", per[per.size() / 2]);
    } else if (mode == "dump") {
        FILE *f = fopen(argv[5], "wb");
        for (auto &s : st) {
            set_state(s);
            call_rhs(&L, c.data(), v.data(), ydot.data(), m.node);
            fwrite(ydot.data(), 8, m.node, f);
            fwrite(v.data(), 8, m.nvar, f);
        }
        fclose(f);
    }
    return 0;
}
