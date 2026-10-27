/* The one JSON number rule (core/json_number.h, W509 / W477 finding 8): every finite double is a
   number, NaN and the infinities are "null". make test runs this from the top of the tree. */
#include "xpptest.h"
#include "json_number.h"

#include <cfloat>
#include <cmath>
#include <limits>
#include <string>

int main(void)
{
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    /* the rule: finite is a number, at any magnitude up to DBL_MAX; NaN and the infinities are not */
    for (double v : {0.0, -0.0, 1.0, 1e308, -1e308, 1.7e308, DBL_MAX, -DBL_MAX, DBL_MIN, 5e-324})
        CHECK(xpp::json::json_finite(v));
    for (double v : {nan, inf, -inf})
        CHECK(!xpp::json::json_finite(v));

    /* the shortest writer: a finite value above 1e308 is its number, not null */
    std::string s;
    xpp::json::json_append_number_shortest(s, 1e308);
    CHECK_STR(s.c_str(), "1e+308");
    s.clear();
    xpp::json::json_append_number_shortest(s, DBL_MAX);
    CHECK_STR(s.c_str(), "1.7976931348623157e+308");
    s.clear();
    xpp::json::json_append_number_shortest(s, -DBL_MAX);
    CHECK_STR(s.c_str(), "-1.7976931348623157e+308");
    s.clear();
    xpp::json::json_append_number_shortest(s, 1.7e308);
    CHECK_STR(s.c_str(), "1.7e+308");

    /* NaN and the infinities are null in both writers */
    for (double v : {nan, inf, -inf}) {
        s.clear();
        xpp::json::json_append_number_shortest(s, v);
        CHECK_STR(s.c_str(), "null");
        s.clear();
        xpp::json::json_append_number(s, v, 17);
        CHECK_STR(s.c_str(), "null");
    }

    /* the general writer (with its significant digits) gives a finite edge value its number too */
    s.clear();
    xpp::json::json_append_number(s, DBL_MAX, 17);
    CHECK_STR(s.c_str(), "1.7976931348623157e+308");

    /* a field of an object: the name, then the number under the same rule */
    s.clear();
    xpp::json::json_append_field(s, "hi", DBL_MAX);
    CHECK_STR(s.c_str(), ",\"hi\":1.7976931348623157e+308");

    TEST_REPORT("json numbers");
}
