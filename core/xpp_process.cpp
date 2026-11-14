/* xpp_process.h: the argument quoting and the checks here; starting the program is
   xpp_process_internal.h's start_program, once per platform */
#include "xpp_process.h"
#include "xpp_process_internal.h"
#include "xpp_io.h"

#include <algorithm>
#include <cstdlib>

namespace xpp::process {

namespace {

constexpr std::string_view BLANKS_AND_QUOTE = " \t\n\v\"";

/* a property name: what the installer allows in a public one */
bool property_name(std::string_view name)
{
    return !name.empty() && std::ranges::all_of(name, [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'; });
}

} // namespace

std::string quote_argument(std::string_view arg, Quoting quoting)
{
    const std::size_t equals = arg.find('=');
    if (quoting == Quoting::installer && equals != std::string_view::npos && property_name(arg.substr(0, equals))) {
        const std::string_view value = arg.substr(equals + 1);
        if (value.find_first_of(BLANKS_AND_QUOTE) == std::string_view::npos) return std::string(arg);
        std::string out = std::string(arg.substr(0, equals + 1)) + "\"";
        for (const char c : value) out += c == '"' ? std::string("\"\"") : std::string(1, c);
        return out + "\"";
    }
    if (!arg.empty() && arg.find_first_of(BLANKS_AND_QUOTE) == std::string_view::npos) return std::string(arg);
    std::string out = "\"";
    std::size_t slashes = 0;
    for (const char c : arg) {
        if (c == '\\') {
            slashes++;
            continue;
        }
        if (c == '"') out.append(slashes * 2 + 1, '\\');
        else out.append(slashes, '\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, '\\');
    return out + "\"";
}

Result<> spawn(std::span<const std::string> argv, Wait wait, Quoting quoting)
{
    if (argv.empty() || argv.front().empty()) return fail(std::string(WHERE), "no program is named", Place{std::string(WHERE)});
    return start_program(argv, wait, quoting);
}

} // namespace xpp::process
