/* One protocol rendering of Error's fields, including its place. */
#include "json_error.h"
#include "xpp_io.h"

namespace xpp::json {
void append_error(std::string &out, const xpp::Error &error)
{
    out += ",\"error\":";
    xpp::json_append_string(out, error.what);
    out += ",\"file\":";
    xpp::json_append_string(out, error.place.file);
    out += xpp::format(",\"line\":{},\"col\":{},\"source\":", error.place.line, error.place.col);
    xpp::json_append_string(out, error.place.source);
    out += ",\"field\":";
    xpp::json_append_string(out, error.field);
}
} // namespace xpp::json
