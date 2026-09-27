/* The one JSON number writer (json_number.h). */
#include "json_number.h"
#include <array>
#include <cstdio>
#include <cstdlib>

namespace xpp::json {

bool json_finite(double v) { return v == v && v <= 1e308 && v >= -1e308; }

void json_append_number(std::string &s, double v, int sig)
{
    if (!json_finite(v)) {
        s += "null";
        return;
    }
    std::array<char, 32> t{};
    const int k = std::snprintf(t.data(), t.size(), "%.*g", sig, v);
    if (k > 0) s.append(t.data(), static_cast<std::size_t>(k));
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
