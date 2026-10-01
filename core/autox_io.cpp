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
#include "browse.h"
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

/* a saved view's axes as the settings that give view k them */
AutoSettingsSet view_settings(const SavedView &v, int k)
{
    AutoSettingsSet a;
    a.has_plot = 1;
    a.plot = v.plot;
    a.var = v.var;
    a.par1 = v.par1;
    a.par2 = v.par2;
    a.view = k;
    return a;
}

} // namespace

bool add_members(const Session &s, std::vector<xpp::zip::Entry> &entries, std::string_view prefix)
{
    const xpp::Model &m = s.model();
    const std::string solutions_path = auto_solutions_file(s);
    std::string solutions;
    if (!xpp::read_bytes(solutions_path.c_str(), solutions)) {
        command_error("save AUTO", xpp::format("AUTO's solutions {} cannot be read: the diagram is not saved without the orbits a grab restarts from",
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

/* the views saved (W50) as this session's, each with its axes and zoom
   (members_read checked them) */
void restore_views(xpp::Session &s, const SavedViews &saved)
{
    s.auto_state.views.assign(saved.views.size(), AutoDiagramView{s.auto_state.axes(), {}, false, {}});
    s.auto_state.active_view = 0;
    for (std::size_t k = 0; k < saved.views.size(); k++) {
        const SavedView &v = saved.views[k];
        std::string why;
        auto_settings_apply(s, view_settings(v, static_cast<int>(k)), why);
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

Result<Members> members_read(const Session &s, const std::map<std::string, std::string> &members, std::string_view prefix,
                             const std::string &name)
{
    /* a member's file, for its errors: the file it is in and its own name */
    const auto file = [&](const char *member) { return xpp::format("{}/{}", name, named(prefix, member)); };
    for (const char *member : {settings_member, diagram_member, solutions_member, views_member})
        if (!members.contains(named(prefix, member)))
            return xpp::fail("AUTO file", xpp::format("its {} is missing", named(prefix, member)), Place{name});
    const auto text = [&](const char *member) -> const std::string & { return members.at(named(prefix, member)); };
    Result<SettingsRead> settings = parse_settings(text(settings_member), file(settings_member));
    if (!settings) return std::unexpected(settings.error());
    Result<std::deque<DiagramPoint>> points = parse_diagram_csv(text(diagram_member), s.model().node, file(diagram_member));
    if (!points) return std::unexpected(points.error());
    Result<ViewsRead> views = parse_views(text(views_member), file(views_member));
    if (!views) return std::unexpected(views.error());
    /* what AUTO accepts for this model: the settings, then each view's
       axes with the settings' parameters */
    const int nviews = static_cast<int>(views->views.views.size());
    std::string why, key;
    if (!auto_settings_check(s, settings->set, -1, why, key)) {
        const auto at = settings->lines.find(key);
        return xpp::fail("AUTO file", why,
                         line_place(file(settings_member), text(settings_member), at == settings->lines.end() ? 0 : at->second));
    }
    for (int k = 0; k < nviews; k++) {
        AutoSettingsSet a = view_settings(views->views.views[static_cast<std::size_t>(k)], k);
        a.npars = settings->set.npars;
        a.pars = settings->set.pars;
        if (!auto_settings_check(s, a, nviews, why, key))
            return xpp::fail("AUTO file", why, line_place(file(views_member), text(views_member), views->lines[static_cast<std::size_t>(k)]));
    }
    return Members{std::move(settings->set), std::move(*points), std::move(views->views), text(solutions_member)};
}

Result<> restore_members(Session &s, Members m)
{
    /* the solution file first: when it cannot be written, nothing is restored */
    const std::string solutions_path = auto_solutions_file(s);
    xpp::Writer w = xpp::Writer::binary(solutions_path.c_str());
    if (!w || !w.write(m.solutions) || !w.commit())
        return xpp::fail("AUTO file",
                         xpp::format("AUTO's solutions could not be written to {}: a grab cannot restart from them", solutions_path),
                         Place{solutions_path});
    if (!s.auto_state.bifur.exist) do_auto_win(s); /* the diagram needs a window to draw into */
    std::string why;
    auto_settings_apply(s, m.settings, why); /* members_read checked it */
    auto_data_forget(s); /* the strip described the diagram this one replaces */
    diagram_restore(s, std::move(m.points));
    restore_views(s, m.views);
    if (s.auto_state.bifur.exist) redraw_diagram(s);
    return {};
}

bool import_file(Session &s, const std::string &path)
{
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) {
        err_reading(path, "cannot be opened");
        return false;
    }
    if (xpp::zip::is_zip(bytes)) {
        command_error("import AUTO", xpp::format("{} is not an XPPAUT .auto file (an AUTO file of xppautX is a .autox)", file_name(path)));
        return false;
    }
    xpp::UniqueFile fp = xpp::open_read(path.c_str());
    if (!fp) {
        err_reading(path, "cannot be opened");
        return false;
    }
    if (!s.auto_state.bifur.exist) do_auto_win(s);
    if (import_auto_file(s, fp.get()) != 1) {
        command_error("import AUTO", xpp::format("{} holds no AUTO diagram", path));
        return false;
    }
    fp.reset();
    if (s.auto_state.bifur.exist) redraw_diagram(s);
    return true;
}

bool save_settings_file(const Session &s, const std::string &path)
{
    xpp::Writer w = open_writer_asking(path);
    if (!w) return false;
    if (!w.write(settings_text(auto_settings_now(s)))) {
        w.abort();
        command_error("save AUTO settings", xpp::format("Cannot write {}", path));
        return false;
    }
    return w.commit();
}

bool load_settings_file(Session &s, const std::string &path)
{
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) {
        err_reading(path, "cannot be opened");
        return false;
    }
    Result<SettingsRead> set = parse_settings(bytes, path);
    std::string why, key;
    if (set && !auto_settings_check(s, set->set, -1, why, key)) {
        const auto at = set->lines.find(key);
        set = xpp::fail("AUTO's settings", why, line_place(path, bytes, at == set->lines.end() ? 0 : at->second));
    }
    if (!set) {
        show_error(set.error());
        return false;
    }
    auto_settings_apply(s, set->set, why);
    return true;
}

} // namespace xpp::autox
