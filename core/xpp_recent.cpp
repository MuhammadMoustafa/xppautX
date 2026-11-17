/* The recent models: see xpp_recent.h. */
#include "xpp_recent.h"
#include "model_switch.h"

#include <utility>
#include <expected>
#include <functional>

#include "json_error.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_settings.h"

namespace xpp::recent {

Result<std::vector<std::string>> load_recent(FileSystem &fs)
{
    Result<settings::State> s = settings::load_state(fs);
    if (!s) return std::unexpected(s.error());
    return std::move(s->recent_models);
}

Result<> note(FileSystem &fs, std::string_view model)
{
    /* a recording playing is not the user opening models: it keeps no list of theirs */
    if (files::serving_reads()) return {};
    return settings::update_state(fs, [&](settings::State &s) {
        settings::remember_newest(s.recent_models, std::string(model), settings::MAX_RECENT_MODELS);
    });
}

std::string start_json(FileSystem &fs, const Result<std::vector<std::string>> &loaded, std::string_view more)
{
    std::string out = "{\"path\":";
    json_append_string(out, settings::state_path(fs));
    out += xpp::format(",\"limit\":{},\"recent\":[", settings::MAX_RECENT_MODELS);
    if (loaded) {
        bool first = true;
        for (const std::string &entry : *loaded) {
            if (!first) out += ',';
            first = false;
            out += "{\"path\":";
            json_append_string(out, entry);
            out += files::exists(entry) ? ",\"missing\":false}" : ",\"missing\":true}";
        }
    }
    out += ']';
    if (!loaded) json::append_error(out, loaded.error());
    out += more;
    return out + "}";
}

} // namespace xpp::recent
