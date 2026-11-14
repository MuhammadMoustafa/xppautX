#pragma once
#include <string>
#include <string_view>
namespace xpp { namespace files { struct CommandResult; } }
namespace xpp::json {
std::string files_json(const xpp::files::CommandResult &result);
std::string file_event(std::string_view op, const xpp::files::CommandResult &result);
}
