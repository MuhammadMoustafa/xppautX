/* Symbolic differentiation (expr_derivative.h, W260): the graph of values and the nodes a set of terms needs. */
#include "expr_derivative.h"
#include "expr_decode.h"
#include "xpp_text.h"

#include <bit>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <cstdint>
#include <expected>
#include <string>

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
    case INTEGER_POWER:
        if (is(y, 1)) return x;
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

std::vector<int> needed_nodes(const Symbolic &g, std::span<const Term> terms)
{
    std::vector<char> needed(g.size(), 0);
    std::vector<int> pending;
    for (const Term t : terms) pending.push_back(t.id);
    while (!pending.empty()) {
        const int id = pending.back();
        pending.pop_back();
        if (needed[static_cast<std::size_t>(id)]) continue;
        needed[static_cast<std::size_t>(id)] = 1;
        for (int a : g.node({id}).args)
            if (a >= 0) pending.push_back(a);
    }
    std::vector<int> nodes;
    for (std::size_t id = 0; id < g.size(); ++id)
        if (needed[id]) nodes.push_back(static_cast<int>(id));
    return nodes;
}

} // namespace xpp::expr
