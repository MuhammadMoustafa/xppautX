#ifndef XPP_JSON_FILES_H
#define XPP_JSON_FILES_H
#include "xpp_files.h"
namespace xpp::json {
std::string files_json(const xpp::files::CommandResult &result);
std::string file_event(std::string_view op, const xpp::files::CommandResult &result);
}
#endif
