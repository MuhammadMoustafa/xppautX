#include "xpptest.h"
#include "xpp_tcc.h"
#include "xpp_math.h"
#include <bit>
#include <cstdint>

int main()
{
    const std::string source =
        "extern double host_exp(double);\n"
        "double evaluate(double x) { return (x * 1.25 + 0.125) / 3.0 + host_exp(x); }\n";
    const xpp::Place place{"generated.c", 1, 0, source};
    const xpp::tcc::Symbol symbols[] = {
        {"host_exp", reinterpret_cast<const void *>(&xpp::math::exp)}
    };
    auto program = xpp::tcc::Program::compile(source, symbols, place);
    CHECK(program.has_value());
    if (!program) printf("%s\n", program.error().text().c_str());
    if (program) {
        auto address = (*program)->function("evaluate");
        CHECK(address.has_value());
        if (address) {
            auto evaluate = reinterpret_cast<double (*)(double)>(*address);
            for (double x : {-2.0, -0.25, 0.0, 0.5, 2.0}) {
                double expected = (x * 1.25 + 0.125) / 3.0 + xpp::math::exp(x);
                CHECK(std::bit_cast<std::uint64_t>(evaluate(x)) ==
                      std::bit_cast<std::uint64_t>(expected));
            }
        }
        auto missing = (*program)->function("absent");
        CHECK(!missing);
        if (!missing) CHECK(missing.error().what.find("absent") != std::string::npos);
    }

    auto syntax = xpp::tcc::Program::compile("double broken( {", {}, place);
    CHECK(!syntax);
    if (!syntax) {
        CHECK(syntax.error().what.find("error:") != std::string::npos);
        CHECK(syntax.error().what.find(":1:") != std::string::npos);
        CHECK(syntax.error().place.file == place.file);
        CHECK(syntax.error().place.line == place.line);
        CHECK(syntax.error().place.source == place.source);
    }
    /* The host process's exp must not be found implicitly. This fails during
       relocation, exercising the same error path as refused executable pages. */
    auto unresolved = xpp::tcc::Program::compile(
        "extern double exp(double); double evaluate(double x) { return exp(x); }", {}, place);
    CHECK(!unresolved);
    if (!unresolved) {
        CHECK(unresolved.error().what.find("exp") != std::string::npos);
        CHECK(unresolved.error().what.find("unresolved reference") != std::string::npos);
    }
    auto header = xpp::tcc::Program::compile("#include <stdio.h>\n", {}, place);
    CHECK(!header);
    if (!header) CHECK(header.error().what.find("stdio.h") != std::string::npos);

    /* A failed state does not poison another compiler. */
    auto next = xpp::tcc::Program::compile("double value(void) { return 7.0; }", {}, place);
    CHECK(next.has_value());
    if (next) {
        auto value = (*next)->function("value");
        CHECK(value.has_value());
        if (value) CHECK(reinterpret_cast<double (*)()>(*value)() == 7.0);
    }
    TEST_REPORT("TinyCC");
}
