/* The .autox file for this session: see autox.h. Its members' text is
   autox.cpp's, the manifest and fingerprint snapx.cpp's and
   xpp_session.cpp's, the zip xpp_zip.cpp's; this file gathers AUTO's state
   into them and puts it back. */
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

} // namespace

std::optional<std::string> file_bytes()
{
    if (diagram_count() <= 1) return std::nullopt; /* an empty diagram */
    const xpp::Model &m = xpp::model();
    const std::string solutions_path = auto_solutions_file();
    std::string solutions;
    if (!xpp::read_bytes(solutions_path.c_str(), solutions))
        xpp::log(XPP_LOG_WARN, "AUTO's solutions {} cannot be read: the diagram is saved without its orbits\n", solutions_path);
    const std::vector<std::string> vars(m.uvar_names.begin(), m.uvar_names.begin() + m.node);
    std::vector<xpp::zip::Entry> entries;
    entries.push_back({manifest_member, xpp::snapx::manifest_text(xpp_session_manifest(), kind)});
    entries.push_back({settings_member, settings_text(auto_settings_now())});
    entries.push_back({diagram_member, diagram_csv(xpp::session().diagram.points, vars)});
    entries.push_back({solutions_member, std::move(solutions)});
    return xpp::zip::make_zip(entries);
}

bool load_bytes(std::string_view bytes, const std::string &name, bool warn_changed)
{
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(bytes);
    if (!entries) {
        err_msg(xpp::format("{} is not an AUTO file (.autox)", name).c_str());
        return false;
    }
    std::map<std::string, std::string> mem;
    for (xpp::zip::Entry &e : *entries) mem[e.name] = std::move(e.bytes);
    std::optional<xpp::snapx::Manifest> man;
    if (mem.contains(manifest_member)) man = xpp::snapx::parse_manifest(mem[manifest_member], kind);
    if (!man) {
        err_msg(xpp::format("{} is not an AUTO file of this version", name).c_str());
        return false;
    }
    const xpp::Model &m = xpp::model();
    if (!xpp_session_same_names(*man)) {
        err_msg(xpp::format("{} is a diagram of {}, whose variables or parameters are not this model's", name,
                            man->model_name)
                    .c_str());
        return false;
    }
    if (warn_changed && man->sha256 != xpp_session_fingerprint())
        xpp_session_warn(xpp::format("{} has changed since {} was saved: its names are the same, and the diagram is loaded",
                                     m.this_file, file_name(name)));
    std::optional<AutoSettingsSet> settings = parse_settings(mem[settings_member]);
    std::optional<std::deque<DiagramPoint>> points = parse_diagram_csv(mem[diagram_member], m.node);
    if (!mem.contains(settings_member) || !settings || !mem.contains(diagram_member) || !points) {
        err_msg(xpp::format("{}: its {} cannot be read", name, settings && mem.contains(settings_member) ? diagram_member : settings_member)
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
    if (!w || !w.write(mem[solutions_member]) || !w.commit())
        xpp_session_warn(xpp::format("{}: AUTO's solutions could not be written to {}: a grab cannot restart from them",
                                     file_name(name), solutions_path));
    if (s.auto_state.bifur.exist) redraw_diagram();
    return true;
}

bool load_file(const std::string &path)
{
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) {
        err_msg(xpp::format("Cannot open {}", path).c_str());
        return false;
    }
    if (is_zip(bytes)) return load_bytes(bytes, path, true);

    /* an XPPAUT .auto: its settings, diagram (6 digits) and solutions */
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
