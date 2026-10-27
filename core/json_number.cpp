/* The one JSON number writer (json_number.h). */
#include "json_number.h"
#include "xpp_io.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace xpp::json {

/* the one rule: a double is a JSON number when it is finite, so DBL_MAX is a number and NaN and the infinities are not */
bool json_finite(double v) { return std::isfinite(v); }

void json_append_number(std::string &s, double v, int sig)
{
    if (!json_finite(v)) {
        s += "null";
        return;
    }
    xpp::append_general(s, v, sig);
}

void json_append_number_shortest(std::string &s, double v)
{
    if (!json_finite(v)) {
        s += "null";
        return;
    }
    std::array<char, 32> t{};
    std::snprintf(t.data(), t.size(), "%.15g", v);
    if (std::strtod(t.data(), nullptr) != v) std::snprintf(t.data(), t.size(), "%.17g", v);
    s += t.data();
}

void json_append_field(std::string &s, const char *name, double v)
{
    s += ",\"";
    s += name;
    s += "\":";
    json_append_number_shortest(s, v);
}

} // namespace xpp::json
