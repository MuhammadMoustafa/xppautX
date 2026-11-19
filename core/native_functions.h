#pragma once
/* The lookup of a module's compiled function by its name, shared by the two compilers (xpp_mir.cpp, xpp_tcc.cpp): both keep the
   addresses of the module's functions in a map by name, and a name the module does not have is the same error from either. */
#include "xpp_error.h"
#include <string>
#include <string_view>
#include <unordered_map>

namespace xpp::native {

/* the address of `name`, or the error naming the compiler `tag` ("MIR", "TinyCC") and the function, at `place` */
inline Result<void *> function_at(const std::unordered_map<std::string, void *> &functions, const std::string &name, const Place &place, std::string_view tag)
{
    const auto found = functions.find(name);
    if (found == functions.end()) return fail(std::string(tag), "function not found: " + name, place);
    return found->second;
}

} // namespace xpp::native
