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
std::string file_name(const std::string &path) { return xpp::files::split_path(path).second; }

/* member name of a file whose AUTO members are named prefix and theirs */
std::string named(std::string_view prefix, const char *name) { return std::string(prefix) + name; }

} // namespace

bool add_members(const Session &s, std::vector<xpp::zip::Entry> &entries, std::string_view prefix)
{
    const xpp::Model &m = s.model();
    const std::string solutions_path = auto_solutions_file();
    std::string solutions;
    if (!xpp::read_bytes(solutions_path.c_str(), solutions)) {
        err_msg(xpp::format("AUTO's solutions {} cannot be read: the diagram is not saved without the orbits a grab restarts from",
                            solutions_path)
                    .c_str());
        return false;
    }
    const std::vector<std::string> vars(m.uvar_names.begin(), m.uvar_names.begin() + m.node);
    entries.push_back({named(prefix, settings_member), settings_text(auto_settings_now(s))});
    entries.push_back({named(prefix, diagram_member), diagram_csv(s.diagram.points, vars)});
    entries.push_back({named(prefix, solutions_member), std::move(solutions)});
    SavedViews views;
    views.active = s.auto_state.active_view;
    for (std::size_t k = 0; k < s.auto_state.views.size(); k++) {
        const AutoSettingsSet a = auto_settings_view(s, static_cast<int>(k));
        views.views.push_back({a.plot, a.var, a.par1, a.par2, a.range, s.auto_state.views[k].zoom});
    }
    entries.push_back({named(prefix, views_member), views_text(views)});
    return true;
}

namespace {

/* the views saved (W50) as this session's, each with its axes and zoom */
void restore_views(xpp::Session &s, const SavedViews &saved, const std::string &name)
{
    s.auto_state.views.assign(saved.views.size(), AutoDiagramView{s.auto_state.axes(), {}, false, {}});
    s.auto_state.active_view = 0;
    for (std::size_t k = 0; k < saved.views.size(); k++) {
        const SavedView &v = saved.views[k];
        AutoSettingsSet a;
        a.has_plot = 1;
        a.plot = v.plot;
        a.var = v.var;
        a.par1 = v.par1;
        a.par2 = v.par2;
        a.view = static_cast<int>(k);
        std::string why;
        if (auto_settings_apply(s, a, why) != 0)
            xpp_session_warn(xpp::format("{}: view {} of the diagram keeps the axes it had: {}", file_name(name), k + 1, why));
        /* the ranges as they were, even the ones a form refuses (a Fit of
           a flat quantity leaves its axis a point) */
        AUTOAX &ax = s.auto_state.views[k].axes;
        ax.xmin = v.range[0];
        ax.xmax = v.range[1];
        ax.ymin = v.range[2];
        ax.ymax = v.range[3];
        s.auto_state.views[k].zoom = v.zoom;
    }
    s.auto_state.active_view = saved.active;
}

} // namespace

std::optional<std::string> file_bytes(const Session &s)
{
    if (diagram_count(s.diagram) <= 1) return std::nullopt; /* an empty diagram */
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp_saved_entries(s, xpp::snapx::Manifest{}, kind);
    if (!entries || !add_members(s, *entries, "")) return std::nullopt;
    return xpp::zip::make_zip(*entries);
}

std::expected<Members, std::string> members_read(const Session &s, const std::map<std::string, std::string> &members,
                                                 std::string_view prefix)
{
    for (const char *member : {settings_member, diagram_member, solutions_member, views_member})
        if (!members.contains(named(prefix, member)))
            return std::unexpected(xpp::format("its {} is missing", named(prefix, member)));
    const auto text = [&](const char *member) -> const std::string & { return members.at(named(prefix, member)); };
    std::optional<AutoSettingsSet> settings = parse_settings(text(settings_member));
    std::optional<std::deque<DiagramPoint>> points = parse_diagram_csv(text(diagram_member), s.model().node);
    std::optional<SavedViews> views = parse_views(text(views_member));
    if (!settings || !points || !views) {
        const char *bad = !settings ? settings_member : !points ? diagram_member : views_member;
        return std::unexpected(xpp::format("its {} cannot be read", named(prefix, bad)));
    }
    return Members{std::move(*settings), std::move(*points), std::move(*views), text(solutions_member)};
}

std::optional<std::string> restore_members(Session &s, Members m, const std::string &name)
{
    if (!s.auto_state.bifur.exist) do_auto_win(s); /* the diagram needs a window to draw into */
    std::string why;
    if (auto_settings_apply(s, m.settings, why) != 0)
        xpp_session_warn(xpp::format("{}: AUTO's settings are left as they were: {}", file_name(name), why));
    auto_data_forget(); /* the strip described the diagram this one replaces */
    diagram_restore(s, std::move(m.points));
    restore_views(s, m.views, name);
    const std::string solutions_path = auto_solutions_file();
    xpp::Writer w = xpp::Writer::binary(solutions_path.c_str());
    const bool written = w && w.write(m.solutions) && w.commit();
    if (s.auto_state.bifur.exist) redraw_diagram(s);
    if (!written) return xpp::format("AUTO's solutions could not be written to {}: a grab cannot restart from them", solutions_path);
    return std::nullopt;
}

bool import_file(Session &s, const std::string &path)
{
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) {
        err_msg(xpp::format("Cannot open {}", path));
        return false;
    }
    if (xpp::zip::is_zip(bytes)) {
        err_msg(xpp::format("{} is not an XPPAUT .auto file (an AUTO file of xppautX is a .autox)", file_name(path)));
        return false;
    }
    xpp::UniqueFile fp = xpp::open_read(path.c_str());
    if (!fp) {
        err_msg(xpp::format("Cannot open {}", path));
        return false;
    }
    if (!s.auto_state.bifur.exist) do_auto_win(s);
    if (import_auto_file(s, fp.get()) != 1) {
        err_msg(xpp::format("{} holds no AUTO diagram", path));
        return false;
    }
    fp.reset();
    if (s.auto_state.bifur.exist) redraw_diagram(s);
    return true;
}

} // namespace xpp::autox
