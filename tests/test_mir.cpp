#include "xpptest.h"
#include "expr_ir.h"
#include "xpp_math.h"
#include "native_code.h"
#include "xpp_mir.h"
#include "xpp_error.h"
#include <bit>
#include <cstddef>
#include <limits>
#include <cstdint>
#include <stdio.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {
using xpp::IrFunction;
using xpp::IrModule;
using xpp::IrNode;
using xpp::IrOp;
using xpp::IrStatement;

int add(IrModule &m, IrNode n)
{
    m.nodes.push_back(n);
    return static_cast<int>(m.nodes.size()) - 1;
}
int op(IrModule &m, IrOp o, int a = -1, int b = -1, int c = -1, int index = 0)
{
    IrNode n;
    n.op = o;
    n.index = index;
    n.args = {a, b, c};
    return add(m, n);
}
int number(IrModule &m, double x)
{
    IrNode n;
    n.number = x;
    return add(m, n);
}
int call(IrModule &m, int callee, std::initializer_list<int> arguments)
{
    IrNode n;
    n.op = IrOp::Call;
    n.callee = callee;
    n.args = {static_cast<int>(m.call_arguments.size()), static_cast<int>(arguments.size()), -1};
    m.call_arguments.insert(m.call_arguments.end(), arguments.begin(), arguments.end());
    return add(m, n);
}
bool same(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }
/* a right-hand side of hundreds of equations is compiled as chunks (W817) and gives the one function's values: each
   equation a temporary and an output, a fixed variable and the early return before them; a temporary kept from the first
   statement to the last forbids every cut: the rest is one chunk */
void chunks()
{
    const xpp::Place place{"model.ode", 1, 0, "x[i]' = x[i] * 2"};
    constexpr int equations = 300;
    double c[1] = {0.5}, v[4] = {0.25, -2.0, 3.0, 1.0}, o[equations + 1] = {};
    IrModule m;
    const auto rhs = [&](const char *name, bool kept) {
        IrFunction f;
        f.kind = IrFunction::Kind::Rhs;
        f.name = name;
        f.temporaries = equations + 1;
        f.statements.push_back({IrStatement::Kind::Variable, 3, op(m, IrOp::Multiply, op(m, IrOp::Constant, -1, -1, -1, 0), number(m, 2.0))});
        f.statements.push_back({IrStatement::Kind::ReturnWithoutOutput});
        if (kept) f.statements.push_back({IrStatement::Kind::Temporary, equations, op(m, IrOp::Add, op(m, IrOp::Variable, -1, -1, -1, 0), number(m, 1.0))});
        for (int i = 0; i < equations; ++i) {
            f.statements.push_back({IrStatement::Kind::Temporary, i, op(m, IrOp::Multiply, op(m, IrOp::Variable, -1, -1, -1, i % 3), number(m, i + 1.0))});
            f.statements.push_back({IrStatement::Kind::Output, i, op(m, IrOp::Add, op(m, IrOp::Temporary, -1, -1, -1, i), op(m, IrOp::Constant, -1, -1, -1, 0))});
        }
        if (kept) f.statements.push_back({IrStatement::Kind::Output, equations, op(m, IrOp::Temporary, -1, -1, -1, equations)});
        m.functions.push_back(f);
    };
    rhs("cut", false);
    rhs("whole", true);
    auto program = xpp::mir::Program::compile(m, place);
    CHECK(program.has_value());
    if (!program) return;
    CHECK((*program)->function("cut.0").has_value() && (*program)->function("cut.1").has_value());
    CHECK((*program)->function("cut.2").has_value() && !(*program)->function("whole.1")); /* the kept temporary: one chunk, not cut */
    for (const char *name : {"cut", "whole"}) {
        auto address = (*program)->function(name);
        CHECK(address.has_value());
        if (!address) continue;
        auto run = reinterpret_cast<void (*)(double *, double *, void *, double *)>(*address);
        v[3] = 0.0;
        std::fill(std::begin(o), std::end(o), 0.0);
        xpp::native::call(run, c, v, nullptr, nullptr);
        CHECK(v[3] == 1.0 && o[0] == 0.0 && o[equations - 1] == 0.0); /* no output array: the fixed variable only */
        xpp::native::call(run, c, v, nullptr, o);
        bool all = true;
        for (int i = 0; i < equations; ++i) all = all && same(o[i], v[i % 3] * (i + 1.0) + 0.5);
        CHECK(all);
        if (std::string(name) == "whole") CHECK(same(o[equations], 1.25));
    }
}
/* the copies of an array equation as one Repeat (W820): o[2 + n] = v[1 + n] * c[0 + n] - c[4] for n in 0..4, the constant a single
   one (stride 0) and a conditional value kept in a temporary of the body, each element the one the unrolled copy reads */
/* the copies of one array equation: a Repeat of `copies` passes whose strided operands and output step with its counter */
constexpr int repeat_copies = 5;
IrModule repeat_module()
{
    IrModule m;
    const auto element = [&](IrOp kind, int index, int stride) {
        IrNode n;
        n.op = kind;
        n.index = index;
        n.stride = stride;
        return add(m, n);
    };
    IrFunction f;
    f.kind = IrFunction::Kind::Rhs;
    f.name = "p_rhs";
    f.temporaries = 1;
    f.statements.push_back({IrStatement::Kind::ReturnWithoutOutput});
    f.statements.push_back({IrStatement::Kind::RepeatBegin, repeat_copies});
    f.statements.push_back({IrStatement::Kind::Temporary, 0, op(m, IrOp::Multiply, element(IrOp::Variable, 1, 1), element(IrOp::Constant, 0, 1))});
    f.statements.push_back({IrStatement::Kind::Output, 2, op(m, IrOp::Subtract, op(m, IrOp::Temporary, -1, -1, -1, 0), element(IrOp::Constant, 4, 0)), -1, -1, 1});
    f.statements.push_back({IrStatement::Kind::RepeatEnd});
    m.functions.push_back(f);
    return m;
}

void repeat_loop()
{
    const xpp::Place place{"model.ode", 1, 0, "x[j]' = x[j-1] * a[j] - b for j in 2..6"};
    constexpr int copies = repeat_copies;
    double c[5] = {1.5, -2.0, 0.25, 4.0, 0.125}, v[8] = {9.0, 0.5, -1.0, 3.0, 2.0, -0.75, 6.0, 1.0}, o[8] = {};
    IrModule m = repeat_module();
    IrFunction &f = m.functions[0];
    auto program = xpp::mir::Program::compile(m, place);
    CHECK(program.has_value());
    if (!program) return;
    auto address = (*program)->function("p_rhs");
    CHECK(address.has_value());
    if (!address) return;
    xpp::native::call(reinterpret_cast<void (*)(double *, double *, void *, double *)>(*address), c, v, nullptr, o);
    bool all = o[0] == 0.0 && o[1] == 0.0 && o[2 + copies] == 0.0;
    for (int n = 0; n < copies; ++n) all = all && same(o[2 + n], v[1 + n] * c[n] - c[4]);
    CHECK(all);
    /* a stride outside a repeat, and a repeat of no pass, are refused */
    IrModule outside;
    outside.nodes = m.nodes;
    IrFunction g = f;
    g.statements.erase(g.statements.begin() + 1);
    g.statements.erase(g.statements.begin() + 3);
    outside.functions.push_back(g);
    CHECK(!xpp::mir::Program::compile(outside, place));
    IrModule empty = m;
    empty.functions[0].statements[1].index = 0;
    CHECK(!xpp::mir::Program::compile(empty, place));
    /* the same module, compiled by TinyCC's text, refused the same way */
    CHECK(!xpp::native::Code::compile(outside, xpp::native::Compiler::TinyCC, place));
    CHECK(!xpp::native::Code::compile(empty, xpp::native::Compiler::TinyCC, place));
}

double with_index(double x, int i) { return x * 0.5 + i; }

/* W821: a module's functions give the same bits from TinyCC's C text as from MIR's code. Every Rhs, Jacobian, User and Equation
   function runs on the same inputs (finite, zeros, a NaN and infinities, drawn in a fixed order) and writes and returns the same
   values; the module is one a model's can be (the Repeat's strides, a loop, a select, a guarded division, an external call with
   an index, the calls of module functions). */
void same_as_tinycc(const IrModule &m, const xpp::Place &place)
{
    auto mir_code = xpp::native::Code::compile(m, xpp::native::Compiler::MIR, place);
    auto tiny_code = xpp::native::Code::compile(m, xpp::native::Compiler::TinyCC, place);
    CHECK(mir_code.has_value());
    if (!tiny_code) printf("%s\n", tiny_code.error().text().c_str());
    CHECK(tiny_code.has_value());
    if (!mir_code || !tiny_code) return;
    const double specials[] = {0.0, -0.0, 1.0, -2.5, 0.75, 1e-300, 3e8, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    constexpr size_t kinds = std::size(specials);
    unsigned seed = 20261119;
    const auto draw = [&] {
        seed = seed * 1103515245u + 12345u;
        return specials[(seed >> 16) % kinds];
    };
    bool all = true;
    for (const IrFunction &f : m.functions) {
        auto mir_address = (*mir_code)->function(f.name), tiny_address = (*tiny_code)->function(f.name);
        CHECK(mir_address.has_value() && tiny_address.has_value());
        if (!mir_address || !tiny_address) return;
        for (int trial = 0; trial < 60; ++trial) {
            double c[8], v[8], o_mir[16] = {}, o_tiny[16] = {}, v_mir[8], v_tiny[8];
            for (double &x : c) x = draw();
            for (double &x : v) x = draw();
            std::copy(std::begin(v), std::end(v), v_mir);
            std::copy(std::begin(v), std::end(v), v_tiny);
            const double a0 = draw(), a1 = draw();
            double r_mir = 0.0, r_tiny = 0.0;
            switch (f.kind) {
            case IrFunction::Kind::Rhs:
            case IrFunction::Kind::Jacobian: {
                auto run = [](void *address, double *cc, double *vv, double *o) {
                    xpp::native::call(reinterpret_cast<void (*)(double *, double *, void *, double *)>(address), cc, vv, nullptr, o);
                };
                run(*mir_address, c, v_mir, o_mir);
                run(*tiny_address, c, v_tiny, o_tiny);
                break;
            }
            case IrFunction::Kind::Equation:
                r_mir = xpp::native::call(reinterpret_cast<double (*)(double *, double *, void *, const int *)>(*mir_address), c, v_mir, nullptr, nullptr);
                r_tiny = xpp::native::call(reinterpret_cast<double (*)(double *, double *, void *, const int *)>(*tiny_address), c, v_tiny, nullptr, nullptr);
                break;
            case IrFunction::Kind::User:
                if (f.doubles == 2) {
                    using Fn = double (*)(double *, double *, void *, double, double);
                    r_mir = xpp::native::call(reinterpret_cast<Fn>(*mir_address), c, v_mir, nullptr, a0, a1);
                    r_tiny = xpp::native::call(reinterpret_cast<Fn>(*tiny_address), c, v_tiny, nullptr, a0, a1);
                } else {
                    using Fn = double (*)(double *, double *, void *, double);
                    r_mir = xpp::native::call(reinterpret_cast<Fn>(*mir_address), c, v_mir, nullptr, a0);
                    r_tiny = xpp::native::call(reinterpret_cast<Fn>(*tiny_address), c, v_tiny, nullptr, a0);
                }
                break;
            }
            bool same_bits = same(r_mir, r_tiny);
            for (int k = 0; k < 8; ++k) same_bits = same_bits && same(v_mir[k], v_tiny[k]);
            for (int k = 0; k < 16; ++k) same_bits = same_bits && same(o_mir[k], o_tiny[k]);
            all = all && same_bits;
        }
    }
    CHECK(all);
}

void tinycc_agrees()
{
    const xpp::Place place{"model.ode", 1, 0, "x[j]' = x[j-1] * a[j] - b for j in 2..6"};
    /* the copies of an array equation, with an external call of an index, a select and an exp of a strided operand in them, and a
       guarded division after them */
    IrModule m = repeat_module();
    m.zero_divisor = 2.23e-15;
    m.callees.push_back({"exp", reinterpret_cast<const void *>(&xpp::math::exp), false, 1, false}); /* 0 */
    m.callees.push_back({"with_index", reinterpret_cast<const void *>(&with_index), false, 1, true}); /* 1 */
    const auto strided = [&](IrOp kind, int index, int stride) {
        const int id = op(m, kind, -1, -1, -1, index);
        m.nodes[static_cast<size_t>(id)].stride = stride;
        return id;
    };
    IrFunction &f = m.functions[0];
    const int indexed = call(m, 1, {strided(IrOp::Constant, 1, 1)});
    m.nodes[static_cast<size_t>(indexed)].index = 2;
    f.statements.insert(f.statements.end() - 1, {IrStatement::Kind::Output, 3, indexed, -1, -1, 1});
    f.statements.insert(f.statements.end() - 1, {IrStatement::Kind::Output, 6, op(m, IrOp::Select, strided(IrOp::Constant, 2, 1), number(m, 1.5), op(m, IrOp::Variable, -1, -1, -1, 3)), -1, -1, 1});
    f.statements.insert(f.statements.end() - 1, {IrStatement::Kind::Output, 0, call(m, 0, {strided(IrOp::Variable, 0, 1)}), -1, -1, 1});
    f.statements.push_back({IrStatement::Kind::Output, 9, op(m, IrOp::GuardedDivide, op(m, IrOp::Constant, -1, -1, -1, 0), op(m, IrOp::Variable, -1, -1, -1, 3))});
    same_as_tinycc(m, place);
}
/* the strides of a derivative's Repeat (W822) through both lowerings: an Output two apart a copy and a variable stepping by two,
   each element computed as MIR's code and TinyCC's text compute it, bit for bit (the harness's arrays hold 8 variables and 16
   outputs, which this module stays inside) */
void tinycc_repeat_strides()
{
    const xpp::Place place{"model.ode", 1, 0, "x[j]' = x[j-1] * a[j] for j in 2..4"};
    constexpr int copies = 3, period = 2;
    IrModule m;
    const auto element = [&](IrOp kind, int index, int stride) {
        IrNode n;
        n.op = kind;
        n.index = index;
        n.stride = stride;
        return add(m, n);
    };
    IrFunction f;
    f.kind = IrFunction::Kind::Jacobian;
    f.name = "j_states";
    f.temporaries = 1;
    f.statements.push_back({IrStatement::Kind::RepeatBegin, copies});
    f.statements.push_back({IrStatement::Kind::Temporary, 0, op(m, IrOp::Multiply, element(IrOp::Variable, 1, 1), element(IrOp::Constant, 0, 0))});
    f.statements.push_back({IrStatement::Kind::Output, 0, op(m, IrOp::Temporary, -1, -1, -1, 0), -1, -1, period});
    f.statements.push_back({IrStatement::Kind::Output, 1, op(m, IrOp::Subtract, element(IrOp::Variable, 3, 2), element(IrOp::Constant, 2, 0)), -1, -1, period});
    f.statements.push_back({IrStatement::Kind::RepeatEnd});
    m.functions.push_back(f);
    same_as_tinycc(m, place);
}
/* the copies of a derivative as one Repeat with two outputs a period apart (W822): o[2n] = v[1 + n] * c[0] and o[2n + 1] =
   v[3 + 2n] - c[2] for n in 0..3, the second's variable stepping by 2 a copy (a stride other than 1 is multiplied out at its
   use), each element the one the unrolled copy reads */
void repeat_period()
{
    const xpp::Place place{"model.ode", 1, 0, "x[j]' = x[j-1] * a[j] for j in 2..5"};
    constexpr int copies = 4, period = 2;
    double c[3] = {1.5, -2.0, 0.25}, v[10] = {9.0, 0.5, -1.0, 3.0, 2.0, -0.75, 6.0, 1.0, 4.5, -3.0}, o[2 * copies] = {};
    IrModule m;
    const auto element = [&](IrOp kind, int index, int stride) {
        IrNode n;
        n.op = kind;
        n.index = index;
        n.stride = stride;
        return add(m, n);
    };
    IrFunction f;
    f.kind = IrFunction::Kind::Jacobian;
    f.name = "j_states";
    f.temporaries = 1;
    f.statements.push_back({IrStatement::Kind::RepeatBegin, copies});
    f.statements.push_back({IrStatement::Kind::Temporary, 0, op(m, IrOp::Multiply, element(IrOp::Variable, 1, 1), element(IrOp::Constant, 0, 0))});
    f.statements.push_back({IrStatement::Kind::Output, 0, op(m, IrOp::Temporary, -1, -1, -1, 0), -1, -1, period});
    f.statements.push_back({IrStatement::Kind::Output, 1, op(m, IrOp::Subtract, element(IrOp::Variable, 3, 2), element(IrOp::Constant, 2, 0)), -1, -1, period});
    f.statements.push_back({IrStatement::Kind::RepeatEnd});
    m.functions.push_back(f);
    auto program = xpp::mir::Program::compile(m, place);
    CHECK(program.has_value());
    if (!program) return;
    auto address = (*program)->function("j_states");
    CHECK(address.has_value());
    if (!address) return;
    xpp::native::call(reinterpret_cast<void (*)(double *, double *, void *, double *)>(*address), c, v, nullptr, o);
    bool all = true;
    for (int n = 0; n < copies; ++n) {
        all = all && same(o[period * n], v[1 + n] * c[0]);
        all = all && same(o[period * n + 1], v[3 + 2 * n] - c[2]);
    }
    CHECK(all);
}
} // namespace

TEST_CASE("test_mir")
{
    const xpp::Place place{"model.ode", 3, 0, "x' = f(x)"};
    double (*unary)(double) = &xpp::math::exp;
    double c[4] = {0.5, 3.0, 7.0, 0.0}, v[4] = {0.25, -2.0, 0.0, 0.0}, o[3] = {0, 0, 0};

    /* a module of every construct: a user function of two arguments called by an equation, a sum, an if
       statement, the guarded division, a select, a Jacobian function with a temporary */
    IrModule m;
    m.zero_divisor = 2.23e-15;
    m.callees.push_back({"f1_0", reinterpret_cast<const void *>(unary), false, 1, false}); /* 0: exp */
    m.callees.push_back({"u0", nullptr, false, 2, false});                                   /* 1: the user function */
    m.callees.push_back({"p_eq1", nullptr, false, 0, false});                                /* 2: an equation */
    /* u0(a0, a1) = (a1 + a0) * 1.25 / a0 */
    {
        IrFunction f;
        f.kind = IrFunction::Kind::User;
        f.name = "u0";
        f.doubles = 2;
        const int a0 = op(m, IrOp::Argument, -1, -1, -1, 0), a1 = op(m, IrOp::Argument, -1, -1, -1, 1);
        const int sum = op(m, IrOp::Add, a1, a0);
        f.result = op(m, IrOp::GuardedDivide, op(m, IrOp::Multiply, sum, number(m, 1.25)), a0);
        m.functions.push_back(f);
    }
    /* p_eq1: the sum over i = 1..3 of c[1] * i, then if the sum is 0 then exp(v[0]) else u0(v[0], total) */
    {
        IrFunction f;
        f.kind = IrFunction::Kind::Equation;
        f.name = "p_eq1";
        f.temporaries = 2;
        f.loops = 1;
        f.statements.push_back({IrStatement::Kind::LoopBegin, 0, number(m, 1), number(m, 3), 0});
        f.statements.push_back({IrStatement::Kind::Constant, 3, op(m, IrOp::LoopCounter, -1, -1, -1, 0)});
        f.statements.push_back({IrStatement::Kind::LoopEnd, 0, op(m, IrOp::Multiply, op(m, IrOp::Constant, -1, -1, -1, 1), op(m, IrOp::Constant, -1, -1, -1, 3)), -1, 0});
        const int total = op(m, IrOp::Temporary, -1, -1, -1, 0);
        f.statements.push_back({IrStatement::Kind::IfBegin, 0, total});
        f.statements.push_back({IrStatement::Kind::Temporary, 1, call(m, 0, {op(m, IrOp::Variable, -1, -1, -1, 0)})});
        f.statements.push_back({IrStatement::Kind::IfElse});
        f.statements.push_back({IrStatement::Kind::Temporary, 1, call(m, 1, {op(m, IrOp::Variable, -1, -1, -1, 0), total})});
        f.statements.push_back({IrStatement::Kind::IfEnd});
        f.result = op(m, IrOp::Temporary, -1, -1, -1, 1);
        m.functions.push_back(f);
    }
    /* p_rhs: v[2] = c[0] / v[3] (guarded: v[3] is 0), then o[0] = p_eq1, o[1] = select(c[2], 1, 2) */
    {
        IrFunction f;
        f.kind = IrFunction::Kind::Rhs;
        f.name = "p_rhs";
        f.temporaries = 0;
        f.statements.push_back({IrStatement::Kind::Variable, 2, op(m, IrOp::GuardedDivide, op(m, IrOp::Constant, -1, -1, -1, 0), op(m, IrOp::Variable, -1, -1, -1, 3))});
        f.statements.push_back({IrStatement::Kind::ReturnWithoutOutput});
        f.statements.push_back({IrStatement::Kind::Output, 0, call(m, 2, {})});
        f.statements.push_back({IrStatement::Kind::Output, 1, op(m, IrOp::Select, op(m, IrOp::Constant, -1, -1, -1, 2), number(m, 1), number(m, 2))});
        m.functions.push_back(f);
    }
    /* j: t0 = v[1] * v[1]; o[0] = t0 - c[0] */
    {
        IrFunction f;
        f.kind = IrFunction::Kind::Jacobian;
        f.name = "j";
        f.temporaries = 1;
        const int v1 = op(m, IrOp::Variable, -1, -1, -1, 1);
        f.statements.push_back({IrStatement::Kind::Temporary, 0, op(m, IrOp::Multiply, v1, v1)});
        f.statements.push_back({IrStatement::Kind::Output, 0, op(m, IrOp::Subtract, op(m, IrOp::Temporary, -1, -1, -1, 0), op(m, IrOp::Constant, -1, -1, -1, 0))});
        m.functions.push_back(f);
    }

    auto program = xpp::mir::Program::compile(m, place);
    CHECK(program.has_value());
    if (!program) printf("%s\n", program.error().text().c_str());
    if (program) {
        auto rhs = (*program)->function("p_rhs");
        auto jacobian = (*program)->function("j");
        CHECK(rhs.has_value() && jacobian.has_value());
        if (rhs && jacobian) {
            auto run = reinterpret_cast<void (*)(double *, double *, void *, double *)>(*rhs);
            xpp::native::call(run, c, v, nullptr, nullptr); /* no output array: only the fixed variable */
            CHECK(same(v[2], 0.5 / 2.23e-15));
            CHECK(o[0] == 0.0);
            xpp::native::call(run, c, v, nullptr, o);
            /* the sum is 3.0 * (1 + 2 + 3), nonzero: the else arm u0(v[0], total) = (total + v0) * 1.25 / v0 */
            const double total = ((3.0 * 1.0 + 0.0) + 3.0 * 2.0) + 3.0 * 3.0;
            CHECK(same(o[0], (total + 0.25) * 1.25 / 0.25));
            CHECK(o[1] == 2.0); /* c[2] is 7, not 0: the select takes its third operand */
            auto derivative = reinterpret_cast<void (*)(double *, double *, void *, double *)>(*jacobian);
            xpp::native::call(derivative, c, v, nullptr, o);
            CHECK(same(o[0], 4.0 - 0.5));
        }
        CHECK(!(*program)->function("absent"));
    }

    same_as_tinycc(m, place); /* the module of every construct, through TinyCC's text as through MIR's code */
    /* the errors are values at the place, and a failed compile leaves nothing behind that poisons the next
       (a call of a function the module lacks; a name MIR refuses as repeated; a tree too deep to lower) */
    IrModule undefined = m;
    undefined.functions.erase(undefined.functions.begin());
    auto lacking = xpp::mir::Program::compile(undefined, place);
    CHECK(!lacking);
    if (!lacking) {
        CHECK(lacking.error().what.find("u0") != std::string::npos);
        CHECK(lacking.error().place.file == place.file);
        CHECK(lacking.error().place.line == place.line);
        CHECK(lacking.error().place.source == place.source);
    }
    IrModule repeated = m;
    repeated.functions.push_back(m.functions.back());
    CHECK(!xpp::mir::Program::compile(repeated, place));
    IrModule deep;
    IrFunction chain;
    chain.kind = IrFunction::Kind::User;
    chain.name = "chain";
    int value = number(deep, 1.0);
    for (int i = 0; i < 5000; ++i) value = op(deep, IrOp::Add, value, number(deep, 1.0));
    chain.result = value;
    deep.functions.push_back(chain);
    auto too_deep = xpp::mir::Program::compile(deep, place);
    CHECK(!too_deep);
    if (!too_deep) CHECK(too_deep.error().what.find("nested") != std::string::npos);

    IrModule small;
    IrFunction seven;
    seven.kind = IrFunction::Kind::User;
    seven.name = "seven";
    seven.result = number(small, 7.0);
    small.functions.push_back(seven);
    auto next = xpp::mir::Program::compile(small, place);
    CHECK(next.has_value());
    if (next) {
        auto address = (*next)->function("seven");
        CHECK(address.has_value());
        if (address) CHECK(xpp::native::call(reinterpret_cast<double (*)(void *, void *, void *)>(*address), nullptr, nullptr, nullptr) == 7.0);
    }
    chunks();
    repeat_loop();
    tinycc_agrees();
    repeat_period();
    tinycc_repeat_strides();
    TEST_REPORT("MIR");
}
