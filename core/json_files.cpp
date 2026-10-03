/* File results rendered for the protocol and HTTP /files, in one place. */
#include "json_files.h"
#include "xpp_io.h"

namespace xpp::json {
std::string files_json(const xpp::files::CommandResult &result)
{
    std::string s = "{\"files\":[";
    for (size_t i = 0; i < result.files.size(); i++) {
        const auto &f = result.files[i];
        if (i) s += ',';
        s += "{\"name\":";
        xpp::json_append_string(s, f.name);
        s += xpp::format(",\"size\":{},\"mtime\":{},\"sha256\":", f.size, f.mtime);
        xpp::json_append_string(s, f.sha);
        s += '}';
    }
    s += "]}";
    return s;
}

std::string file_event(std::string_view op, const xpp::files::CommandResult &result)
{
    std::string s = "{\"ev\":\"file\",\"op\":";
    xpp::json_append_string(s, op);
    if (!result.name.empty()) {
        s += ",\"name\":";
        xpp::json_append_string(s, result.name);
    }
    if (result.error) {
        s += ",\"ok\":0,\"error\":";
        xpp::json_append_string(s, result.error->what);
        return s + '}';
    }
    s += ",\"ok\":1,";
    if (op == "list") {
        s += files_json(result).substr(1);
        return s;
    }
    s += xpp::format("\"size\":{},\"sha256\":\"{}\"", result.size, result.sha);
    if (op == "get") {
        s += ",\"data\":\"";
        xpp::base64_append(s, result.bytes);
        s += '"';
    }
    return s + '}';
}
} // namespace xpp::json
