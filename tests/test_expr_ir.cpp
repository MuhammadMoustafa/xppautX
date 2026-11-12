/* W711 spike: the Generator's trees for a model's right-hand side (core/expr_ir.h, xpp::rhs_ir): what the
   back end in tools/mir-spike lowers; its bit-for-bit comparison with TinyCC is `make mirspike`. */
#include "load_args.h"
#include "xpptest.h"
#include "expr_native.h"
#include "session.h"

TEST_CASE("test_expr_ir")
{
    char arg0[] = "test_expr_ir";
    char plain[] = "examples/ode/izh_2003_rs.odex";
    char summed[] = "examples/ode/wcring.odex";
    char *argv_plain[] = {arg0, plain, NULL};
    char *argv_summed[] = {arg0, summed, NULL};

    auto loaded = load_args(2, argv_plain, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("expr_ir");
    auto ir = xpp::rhs_ir(**loaded);
    CHECK(ir.has_value());
    if (!ir) TEST_REPORT("expr_ir");
    CHECK(ir->unsupported.empty());
    CHECK(ir->zero_divisor == xpp::expr::ZERO_DIVISOR);
    CHECK(ir->temporaries >= 0);
    size_t outputs = 0;
    for (const auto &s : ir->statements) outputs += s.kind == xpp::IrStatement::Kind::Output;
    CHECK(outputs == static_cast<size_t>((**loaded).model().node));
    /* a function the code calls is named by the table and has an address: nothing else is resolved */
    for (const auto &callee : ir->callees) {
        CHECK(callee.address != nullptr);
        CHECK(callee.name.starts_with("f1_") || callee.name.starts_with("f2_") || callee.name.starts_with("h_"));
    }

    /* a sum is not written as trees: the right-hand side calls the function compiled for the equation that has it */
    loaded = load_args(2, argv_summed, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("expr_ir");
    ir = xpp::rhs_ir(**loaded);
    CHECK(ir.has_value() && !ir->unsupported.empty());
    TEST_REPORT("expr_ir");
}
