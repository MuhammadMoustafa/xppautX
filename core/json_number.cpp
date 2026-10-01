/* The one JSON number writer (json_number.h). */
#include "json_number.h"
#include <array>
#include <charconv>
#include <system_error>
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
    /* std::to_chars's general format with a precision is printf's "%.*g"
       character for character, without its locale and format parsing */
    std::array<char, 32> t{};
    const std::to_chars_result r = std::to_chars(t.data(), t.data() + t.size(), v, std::chars_format::general, sig);
    if (r.ec == std::errc()) s.append(t.data(), r.ptr);
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
