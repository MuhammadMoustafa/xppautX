#include "xpptest.h"
#include "expr_ir.h"
#include "xpp_math.h"
#include "xpp_mir.h"
#include <bit>
#include <cstdint>

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
} // namespace

TEST_CASE("test_mir")
{
    const xpp::Place place{"model.ode", 3, 0, "x' = f(x)"};
    double (*unary)(double) = &xpp::math::exp;
    double c[4] = {0.5, 3.0, 7.0, 0.0}, v[4] = {0.25, -2.0, 0.0, 0.0}, o[3] = {0, 0, 0}, w[8] = {};

    /* a module of every construct: a user function of two arguments called by an equation, a sum, an if
       statement, the guarded division, a select, the Jacobian's workspace */
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
    /* j: w[0] = v[1] * v[1]; o[0] = w[0] - c[0] */
    {
        IrFunction f;
        f.kind = IrFunction::Kind::Jacobian;
        f.name = "j";
        const int v1 = op(m, IrOp::Variable, -1, -1, -1, 1);
        f.statements.push_back({IrStatement::Kind::Workspace, 0, op(m, IrOp::Multiply, v1, v1)});
        f.statements.push_back({IrStatement::Kind::Output, 0, op(m, IrOp::Subtract, op(m, IrOp::Workspace, -1, -1, -1, 0), op(m, IrOp::Constant, -1, -1, -1, 0))});
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
            xpp::mir::call(run, c, v, nullptr, nullptr); /* no output array: only the fixed variable */
            CHECK(same(v[2], 0.5 / 2.23e-15));
            CHECK(o[0] == 0.0);
            xpp::mir::call(run, c, v, nullptr, o);
            /* the sum is 3.0 * (1 + 2 + 3), nonzero: the else arm u0(v[0], total) = (total + v0) * 1.25 / v0 */
            const double total = ((3.0 * 1.0 + 0.0) + 3.0 * 2.0) + 3.0 * 3.0;
            CHECK(same(o[0], (total + 0.25) * 1.25 / 0.25));
            CHECK(o[1] == 2.0); /* c[2] is 7, not 0: the select takes its third operand */
            auto derivative = reinterpret_cast<void (*)(double *, double *, void *, double *, double *)>(*jacobian);
            xpp::mir::call(derivative, c, v, nullptr, w, o);
            CHECK(same(o[0], 4.0 - 0.5));
        }
        CHECK(!(*program)->function("absent"));
    }

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
        if (address) CHECK(xpp::mir::call(reinterpret_cast<double (*)(void *, void *, void *)>(*address), nullptr, nullptr, nullptr) == 7.0);
    }
    TEST_REPORT("MIR");
}
