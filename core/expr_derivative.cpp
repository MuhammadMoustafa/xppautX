/* Symbolic differentiation of a model's programs (expr_derivative.h,
   W260): the graph, a program read into it, and the chain rule over it. */
#include "expr_derivative.h"
#include "expr_decode.h"
#include "expr_native.h"
#include "model.h"
#include "session.h"
#include "xpp_io.h"

#include <algorithm>
#include <bit>
#include <map>
#include <optional>
#include <span>
#include <utility>

namespace xpp::expr {

/* ---- the graph ---- */

std::size_t Symbolic::Hash::operator()(const Node &n) const noexcept
{
    std::uint64_t h = static_cast<std::uint64_t>(n.op) * 0x9E3779B97F4A7C15ULL;
    auto mix = [&h](std::uint64_t x) { h ^= x + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2); };
    mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(n.index)));
    for (int a : n.args) mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)));
    mix(std::bit_cast<std::uint64_t>(n.number));
    return static_cast<std::size_t>(h);
}

bool Symbolic::Same::operator()(const Node &a, const Node &b) const noexcept
{
    return a.op == b.op && a.index == b.index && a.args == b.args
        && std::bit_cast<std::uint64_t>(a.number) == std::bit_cast<std::uint64_t>(b.number);
}

Term Symbolic::make(const Node &n)
{
    if (const auto it = index_.find(n); it != index_.end()) return {it->second};
    if (nodes_.size() >= max_nodes_) throw TooLarge{};
    const int id = static_cast<int>(nodes_.size());
    nodes_.push_back(n);
    index_.emplace(n, id);
    return {id};
}

bool Symbolic::is(Term t, double x) const
{
    const Node &n = node(t);
    return n.op == Op::Number && n.number == x;
}

Term Symbolic::number(double x) { return make({Op::Number, 0, {-1, -1, -1}, x}); }
Term Symbolic::constant(int k) { return make({Op::Constant, k}); }
Term Symbolic::variable(int k) { return make({Op::Variable, k}); }
Term Symbolic::fun1(int i, Term x) { return make({Op::Fun1, i, {x.id, -1, -1}}); }
Term Symbolic::table(int i, Term x) { return make({Op::Table, i, {x.id, -1, -1}}); }
Term Symbolic::table_slope(int i, Term x) { return make({Op::TableSlope, i, {x.id, -1, -1}}); }

Term Symbolic::fun2(int i, Term x, Term y)
{
    const bool numbers = node(x).op == Op::Number && node(y).op == Op::Number;
    const double a = node(x).number, b = node(y).number;
    switch (i) {
    case ADD:
        if (is(x, 0)) return y;
        if (is(y, 0)) return x;
        if (numbers) return number(a + b);
        break;
    case SUBTRACT:
        if (is(y, 0)) return x;
        if (numbers) return number(a - b);
        break;
    case MULTIPLY:
        if (is(x, 0) || is(y, 0)) return number(0);
        if (is(x, 1)) return y;
        if (is(y, 1)) return x;
        if (numbers) return number(a * b);
        break;
    case DIVIDE:
    case IEEE_DIVIDE:
        if (is(x, 0)) return number(0);
        if (is(y, 1)) return x;
        if (numbers && i == IEEE_DIVIDE) return number(a / b);
        break;
    default:
        break;
    }
    return make({Op::Fun2, i, {x.id, y.id, -1}});
}

Term Symbolic::select(Term condition, Term then, Term otherwise)
{
    if (then == otherwise) return then;
    if (node(condition).op == Op::Number) return node(condition).number != 0 ? then : otherwise;
    return make({Op::Select, 0, {condition.id, then.id, otherwise.id}});
}

namespace {

/* The graph of a model's values and derivatives holds at most this many
   nodes and derivative terms together: about 25 times what the largest
   example with one needs (clustor, 38152 values), and past it the
   generated C would take TinyCC seconds to compile and its workspace
   megabytes. A model past it
   has no exact Jacobian. */
constexpr std::size_t MAX_JACOBIAN_NODES = 1000000;
/* The program instructions read in all, user functions read again at
   each call: ten for each node allowed (a call with arguments seen before
   is read once). */
constexpr std::size_t MAX_JACOBIAN_READ = 10 * MAX_JACOBIAN_NODES;
/* User functions nest this deep at most: each level is one recursion of
   the reader, and a model's own nest two or three. */
constexpr int MAX_INLINE_DEPTH = 32;

std::unexpected<Error> refuse(const Place &where, std::string what)
{
    return fail("jacobian", std::move(what), where);
}

/* why an instruction a program may hold has no derivative here */
const char *no_derivative(int op)
{
    switch (op) {
    case SUMSYM: case ENDSUM: return "uses sum(...)of(...), which the exact Jacobian does not differentiate";
    case ENDDELAY: case ENDDELSHFT: return "uses a delay";
    case ENDSHIFT: case ENDISHIFT: return "uses shift(), which picks a variable by a computed index";
    case ENDSET: return "uses set(), which changes a variable";
    case RANDUNI: case RANDPOI: case RANDNORM: return "uses random numbers";
    default: break;
    }
    switch (op / MAXTYPE) {
    case SVARTYPE: case SCONTYPE: return "uses shift() or a delay";
    case NETTYPE: return "uses a network";
    case VECTYPE: return "uses a vectorizer";
    case KERTYPE: return "uses an integral";
    default: return "uses an operation that has no derivative";
    }
}

/* The model's programs read into a graph: the derived parameters, the
   fixed variables, then the equations, each in order, a user function
   read in place at each call. The program each node was read from is
   kept, for an error's place. */
class Reader {
public:
    Reader(const Session &s, Symbolic &g, std::span<const ModelProgram> programs) : s_(s), m_(s.model()), g_(g)
    {
        for (const ModelProgram &p : programs) entries_.emplace(p.code, &p);
        for (const auto &d : m_.derived) derived_.emplace(d.index, std::nullopt);
    }

    Result<std::vector<Term>> equations()
    {
        for (const auto &d : m_.derived) {
            auto value = read(d.form, {}, 0);
            if (!value) return std::unexpected(value.error());
            derived_[d.index] = *value;
        }
        for (int i = m_.node; i < m_.node + m_.fix_var; ++i) {
            auto value = read(m_.programs[i], {}, 0);
            if (!value) return std::unexpected(value.error());
            fixed_.push_back(*value);
        }
        std::vector<Term> f;
        for (int i = 0; i < m_.node; ++i) {
            auto value = read(m_.programs[i], {}, 0);
            if (!value) return std::unexpected(value.error());
            f.push_back(*value);
        }
        return f;
    }

    /* the place of the program node id was read from */
    const Place &place_of(Term t) const { return places_[origin_[static_cast<std::size_t>(t.id)]]; }
    Place last_place() const { return places_.empty() ? Place{} : places_.back(); }

private:
    /* the nodes made since the last call were made by program `program` */
    void made_by(std::size_t program) { origin_.resize(g_.size(), program); }

    Result<Term> read(const Program &p, std::span<const Term> args, int depth)
    {
        const auto entry = entries_.find(&p);
        if (entry == entries_.end()) return refuse(last_place(), "reads a program the model does not hold");
        const Place &where = entry->second->place;
        if (!p.native) return refuse(where, "was not compiled, so it has no exact Jacobian");
        places_.push_back(where);
        const std::size_t program = places_.size() - 1;
        made_by(program);
        auto decoded = decoded_.find(&p);
        if (decoded == decoded_.end()) {
            auto code = decode(s_, p.rpn, depth == 0 ? -1 : static_cast<int>(args.size()));
            if (!code) return refuse(where, "is not a valid program (" + code.error() + ")");
            decoded = decoded_.emplace(&p, std::move(*code)).first;
        }
        struct Conditional {
            Term condition, then;
        };
        std::vector<Term> stack;
        std::vector<Conditional> frames;
        const auto pop = [&] { const Term t = stack.back(); stack.pop_back(); return t; };
        for (const Instr &i : decoded->second) {
            if (++read_ > MAX_JACOBIAN_READ) throw Symbolic::TooLarge{};
            const int op = i.op, type = i.type, in = i.in;
            if (op == JOIN) {
                const Conditional f = frames.back();
                frames.pop_back();
                const Term otherwise = pop();
                stack.push_back(g_.select(f.condition, f.then, otherwise));
            } else if (op == NUMSYM) {
                stack.push_back(g_.number(i.number));
            } else if (op == MYIF) {
                frames.push_back({pop(), {}});
            } else if (op == MYTHEN) {
                frames.back().then = pop();
            } else if (op == INDXCOM) {
                stack.push_back(g_.number(CURRENT_INDEX));
            } else if (type == CONTYPE) {
                if (in == SUM_INDEX) return refuse(where, no_derivative(SUMSYM));
                const auto derived = derived_.find(in);
                if (derived == derived_.end()) stack.push_back(g_.constant(in));
                else if (derived->second) stack.push_back(*derived->second);
                else return refuse(where, "reads a derived parameter defined after it");
            } else if (type == VARTYPE) {
                if (in <= m_.node) stack.push_back(g_.variable(in));
                else if (in <= m_.node + m_.fix_var) {
                    const std::size_t k = static_cast<std::size_t>(in - m_.node - 1);
                    if (k >= fixed_.size()) return refuse(where, "reads a fixed variable defined after it");
                    stack.push_back(fixed_[k]);
                } else return refuse(where, "reads a Markov variable");
            } else if (type == USTACKTYPE) {
                stack.push_back(args[static_cast<std::size_t>(in)]);
            } else if (type == FUN1TYPE) {
                const Term x = pop();
                stack.push_back(g_.fun1(in, x));
            } else if (type == FUN2TYPE) {
                const Term y = pop(), x = pop();
                stack.push_back(g_.fun2(in, x, y));
            } else if (type == UFUNTYPE) {
                const std::size_t n = static_cast<std::size_t>(i.count);
                if (depth >= MAX_INLINE_DEPTH) return refuse(where, xpp::format("calls user functions nested more than {} deep", MAX_INLINE_DEPTH));
                std::vector<Term> call(stack.end() - static_cast<std::ptrdiff_t>(n), stack.end());
                stack.resize(stack.size() - n);
                std::vector<int> key{in};
                for (const Term t : call) key.push_back(t.id);
                if (const auto it = calls_.find(key); it != calls_.end()) stack.push_back(it->second);
                else {
                    auto value = read(m_.ufun_programs[static_cast<std::size_t>(in)], call, depth + 1);
                    if (!value) return value;
                    calls_.emplace(std::move(key), *value);
                    stack.push_back(*value);
                }
            } else if (type == TABTYPE) {
                if (s_.tables[static_cast<std::size_t>(in)].flag == 2)
                    return refuse(where, "uses a function table, whose values the parameters change");
                const Term x = pop();
                stack.push_back(g_.table(in, x));
            } else {
                return refuse(where, no_derivative(op));
            }
            made_by(program);
        }
        return stack.back();
    }

    const Session &s_;
    const Model &m_;
    Symbolic &g_;
    std::map<const Program *, const ModelProgram *> entries_;
    std::map<const Program *, std::vector<Instr>> decoded_; /* each program decoded once */
    std::map<int, std::optional<Term>> derived_; /* by its constant; none until read */
    std::vector<Term> fixed_;
    std::map<std::vector<int>, Term> calls_;
    std::vector<Place> places_;
    std::vector<std::size_t> origin_;
    std::size_t read_ = 0;
};

/* d value / d column for each column the value depends on, by column */
using Gradient = std::vector<std::pair<int, Term>>;

/* a's and b's terms of each column, joined by join (a missing one 0) */
template <class Join>
Gradient merge(Symbolic &g, const Gradient &a, const Gradient &b, Join &&join)
{
    Gradient out;
    std::size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        if (j == b.size() || (i < a.size() && a[i].first < b[j].first)) {
            out.emplace_back(a[i].first, join(a[i].second, g.number(0)));
            ++i;
        } else if (i == a.size() || b[j].first < a[i].first) {
            out.emplace_back(b[j].first, join(g.number(0), b[j].second));
            ++j;
        } else {
            out.emplace_back(a[i].first, join(a[i].second, b[j].second));
            ++i;
            ++j;
        }
    }
    std::erase_if(out, [&g](const auto &e) { return g.is(e.second, 0); });
    return out;
}

Gradient scaled(Symbolic &g, const Gradient &a, Term factor)
{
    Gradient out;
    for (const auto &[column, d] : a) {
        const Term t = g.multiply(d, factor);
        if (!g.is(t, 0)) out.emplace_back(column, t);
    }
    return out;
}

} // namespace

Result<ModelDerivatives> differentiate_model(const Session &s, std::span<const ModelProgram> programs)
{
    const Model &m = s.model();
    if (m.nmarkov > 0) return refuse(model_place(m, m.markov[0].name), "a Markov variable changes the state at random");
    if (m.nkernel > 0) return refuse(m.kernels[0].where, "has an integral equation");
    if (m.nsvar > 0 || m.naeqn > 0) return refuse(m.nsvar > 0 ? m.svars[0].where : m.aeqns[0].where, "has algebraic equations");
    ModelDerivatives d{Symbolic(MAX_JACOBIAN_NODES), {}};
    Symbolic &g = d.graph;
    Reader reader(s, g, programs);
    std::size_t entries = 0;
    try {
        const auto f = reader.equations();
        if (!f) return std::unexpected(f.error());
        /* the nodes the equations read, each differentiated after its
           arguments (a node's arguments are numbered before it) */
        const std::size_t read = g.size();
        std::vector<char> needed(read, 0);
        std::vector<int> stack;
        for (const Term t : *f) stack.push_back(t.id);
        while (!stack.empty()) {
            const int id = stack.back();
            stack.pop_back();
            if (needed[static_cast<std::size_t>(id)]) continue;
            needed[static_cast<std::size_t>(id)] = 1;
            for (int a : g.node({id}).args)
                if (a >= 0) stack.push_back(a);
        }
        std::vector<Gradient> grad(read);
        for (std::size_t id = 0; id < read; ++id) {
            if (!needed[id]) continue;
            const Term t{static_cast<int>(id)};
            const Symbolic::Node n = g.node(t);
            const Term x{n.args[0]}, y{n.args[1]};
            const auto partial = [&](const Gradient &gx, auto rule, auto &&...at) -> Result<Gradient> {
                if (gx.empty()) return Gradient{};
                if (!rule) return refuse(reader.place_of(t), "calls a function that has no derivative (lgamma, or a Bessel function in its order)");
                return scaled(g, gx, rule(g, at...));
            };
            Gradient &out = grad[id];
            switch (n.op) {
            case Symbolic::Op::Number:
            case Symbolic::Op::TableSlope:
                break;
            case Symbolic::Op::Constant:
                out = {{constant_column(n.index), g.number(1)}};
                break;
            case Symbolic::Op::Variable:
                out = {{n.index, g.number(1)}};
                break;
            case Symbolic::Op::Fun1: {
                auto r = partial(grad[static_cast<std::size_t>(x.id)], fun1_derivative[static_cast<std::size_t>(n.index)], x);
                if (!r) return std::unexpected(r.error());
                out = std::move(*r);
                break;
            }
            case Symbolic::Op::Fun2: {
                const Fun2Derivative &rule = fun2_derivative[static_cast<std::size_t>(n.index)];
                auto rx = partial(grad[static_cast<std::size_t>(x.id)], rule.x, x, y);
                if (!rx) return std::unexpected(rx.error());
                auto ry = partial(grad[static_cast<std::size_t>(y.id)], rule.y, x, y);
                if (!ry) return std::unexpected(ry.error());
                out = merge(g, *rx, *ry, [&g](Term a, Term b) { return g.add(a, b); });
                break;
            }
            case Symbolic::Op::Select: {
                const Term condition{n.args[0]};
                out = merge(g, grad[static_cast<std::size_t>(n.args[1])], grad[static_cast<std::size_t>(n.args[2])],
                            [&g, condition](Term a, Term b) { return g.select(condition, a, b); });
                break;
            }
            case Symbolic::Op::Table:
                out = scaled(g, grad[static_cast<std::size_t>(x.id)], g.table_slope(n.index, x));
                break;
            }
            entries += out.size();
            if (entries > MAX_JACOBIAN_NODES) throw Symbolic::TooLarge{};
        }
        for (int row = 0; row < m.node; ++row)
            for (const auto &[column, value] : grad[static_cast<std::size_t>((*f)[static_cast<std::size_t>(row)].id)])
                d.entries.push_back({row, column, value});
    } catch (const Symbolic::TooLarge &) {
        return refuse(reader.last_place(), xpp::format("its derivatives need more than {} values", MAX_JACOBIAN_NODES));
    }
    return d;
}

} // namespace xpp::expr
