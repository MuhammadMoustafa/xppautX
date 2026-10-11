/* The recent models: see xpp_recent.h. */
#include "xpp_recent.h"

#include <utility>

#include "json_error.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_prefs.h"

namespace xpp::recent {

Result<std::vector<std::string>> load_recent()
{
    Result<prefs::Preferences> p = prefs::load_prefs();
    if (!p) return std::unexpected(p.error());
    return std::move(p->recent_models);
}

Result<> note(std::string_view model)
{
    /* a recording playing is not the user opening models: it keeps no list of theirs */
    if (files::serving_reads()) return {};
    return prefs::update_prefs([&](prefs::Preferences &p) {
        prefs::remember_newest(p.recent_models, std::string(model), prefs::MAX_RECENT_MODELS);
    });
}

std::string start_json(const Result<std::vector<std::string>> &loaded, std::string_view more)
{
    std::string out = "{\"path\":";
    json_append_string(out, prefs::prefs_path());
    out += xpp::format(",\"limit\":{},\"recent\":[", prefs::MAX_RECENT_MODELS);
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
