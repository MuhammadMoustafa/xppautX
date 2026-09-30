/* AUTO's members for this session: see autox.h. Their text is
   autox.cpp's, the manifest and the model's members snapx.cpp's and
   xpp_session.cpp's (the one reader and writer of a file that carries a
   model), the zip xpp_zip.cpp's; this file gathers AUTO's state into
   them and puts it back. */
#include "autox.h"
#include "snapx.h"
#include "session.h"
#include "model.h"
#include "xpp_session.h"
#include "xpp_zip.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include "xpp_files.h"
#include "auto_data.h"
#include "auto_nox.h"
#include "auto_settings.h"
#include "diagram.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace xpp::autox {

namespace {

/* the file name of path, for messages */
std::string file_name(const std::string &path) { return xpp_files_split_path(path).second; }

/* member name of a file whose AUTO members are named prefix and theirs */
std::string named(std::string_view prefix, const char *name) { return std::string(prefix) + name; }

} // namespace

void add_members(std::vector<xpp::zip::Entry> &entries, std::string_view prefix)
{
    const xpp::Model &m = xpp::model();
    const std::string solutions_path = auto_solutions_file();
    std::string solutions;
    if (!xpp::read_bytes(solutions_path.c_str(), solutions))
        xpp::log(XPP_LOG_WARN, "AUTO's solutions {} cannot be read: the diagram is saved without its orbits\n", solutions_path);
    const std::vector<std::string> vars(m.uvar_names.begin(), m.uvar_names.begin() + m.node);
    entries.push_back({named(prefix, settings_member), settings_text(auto_settings_now())});
    entries.push_back({named(prefix, diagram_member), diagram_csv(xpp::session().diagram.points, vars)});
    entries.push_back({named(prefix, solutions_member), std::move(solutions)});
}

std::optional<std::string> file_bytes()
{
    if (diagram_count() <= 1) return std::nullopt; /* an empty diagram */
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp_saved_entries(xpp::snapx::Manifest{}, kind);
    if (!entries) return std::nullopt;
    add_members(*entries, "");
    return xpp::zip::make_zip(*entries);
}

bool restore_members(const std::map<std::string, std::string> &members, std::string_view prefix, const std::string &name)
{
    const auto text = [&](const char *member) -> const std::string * {
        const auto it = members.find(named(prefix, member));
        return it == members.end() ? nullptr : &it->second;
    };
    const std::string *settings_text = text(settings_member), *diagram_text = text(diagram_member),
                      *solutions = text(solutions_member);
    std::optional<AutoSettingsSet> settings;
    std::optional<std::deque<DiagramPoint>> points;
    if (settings_text) settings = parse_settings(*settings_text);
    if (diagram_text) points = parse_diagram_csv(*diagram_text, xpp::model().node);
    if (!settings || !points) {
        err_msg(xpp::format("{}: its {} cannot be read", file_name(name), named(prefix, settings ? diagram_member : settings_member))
                    .c_str());
        return false;
    }

    xpp::Session &s = xpp::session();
    if (!s.auto_state.bifur.exist) do_auto_win(); /* the diagram needs a window to draw into */
    std::string why;
    if (auto_settings_apply(*settings, why) != 0)
        xpp_session_warn(xpp::format("{}: AUTO's settings are left as they were: {}", file_name(name), why));
    auto_data_forget(); /* the strip described the diagram this one replaces */
    diagram_restore(std::move(*points));
    const std::string solutions_path = auto_solutions_file();
    xpp::Writer w = xpp::Writer::binary(solutions_path.c_str());
    if (!w || !w.write(solutions ? *solutions : std::string()) || !w.commit())
        xpp_session_warn(xpp::format("{}: AUTO's solutions could not be written to {}: a grab cannot restart from them",
                                     file_name(name), solutions_path));
    if (s.auto_state.bifur.exist) redraw_diagram();
    return true;
}

bool import_file(const std::string &path)
{
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) {
        err_msg(xpp::format("Cannot open {}", path).c_str());
        return false;
    }
    if (xpp::zip::is_zip(bytes)) {
        err_msg(xpp::format("{} is not an XPPAUT .auto file (an AUTO file of xppautX is a .autox)", file_name(path)).c_str());
        return false;
    }
    xpp::UniqueFile fp = xpp::open_read(path.c_str());
    if (!fp) {
        err_msg(xpp::format("Cannot open {}", path).c_str());
        return false;
    }
    if (!xpp::session().auto_state.bifur.exist) do_auto_win();
    if (import_auto_file(fp.get()) != 1) {
        err_msg(xpp::format("{} holds no AUTO diagram", path).c_str());
        return false;
    }
    fp.reset();
    if (xpp::session().auto_state.bifur.exist) redraw_diagram();
    return true;
}

} // namespace xpp::autox
