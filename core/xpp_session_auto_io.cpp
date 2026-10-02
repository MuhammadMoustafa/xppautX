/* AUTO's members for this session: see xpp_session_auto.h. Their text is
   xpp_session_auto.cpp's, the manifest and the model's members snapx.cpp's and
   xpp_session.cpp's (the one reader and writer of a file that carries a
   model), the zip xpp_zip.cpp's; this file gathers AUTO's state into
   them and puts it back. */
#include "xpp_session_auto.h"
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

namespace xpp::snapx::auto_members {

namespace {

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

Result<> add_members(const Session &s, std::vector<xpp::zip::Entry> &entries, std::string_view prefix)
{
    const xpp::Model &m = s.model();
    entries.push_back({named(prefix, settings_member), settings_text(auto_settings_now(s))});
    SavedViews views;
    views.active = s.auto_state.active_view;
    for (std::size_t k = 0; k < s.auto_state.views.size(); k++) {
        const AutoSettingsSet a = auto_settings_view(s, static_cast<int>(k));
        views.views.push_back({a.plot, a.var, a.par1, a.par2, a.range, s.auto_state.views[k].zoom});
    }
    entries.push_back({named(prefix, views_member), views_text(views)});
    if (diagram_count(s.diagram) <= 1) return {};
    const std::string solutions_path = auto_solutions_file(s);
    std::string solutions;
    if (!xpp::read_bytes(solutions_path, solutions))
        return xpp::fail("save session", "AUTO's restart solutions cannot be read", xpp::Place{solutions_path});
    const std::vector<std::string> vars(m.uvar_names.begin(), m.uvar_names.begin() + m.node);
    entries.push_back({named(prefix, diagram_member), diagram_csv(s.diagram.points, vars)});
    entries.push_back({named(prefix, solutions_member), std::move(solutions)});
    return {};
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

Result<Members> members_read(const Session &s, const std::map<std::string, std::string> &members, std::string_view prefix,
                             const std::string &name)
{
    if (s.model().node > NAUTO)
        return xpp::fail("session AUTO", "this model has too many variables for AUTO", Place{name});
    /* a member's file, for its errors: the file it is in and its own name */
    const auto file = [&](const char *member) { return xpp::format("{}/{}", name, named(prefix, member)); };
    for (const char *member : {settings_member, views_member})
        if (!members.contains(named(prefix, member)))
            return xpp::fail("AUTO file", xpp::format("its {} is missing", named(prefix, member)), Place{name});
    const auto text = [&](const char *member) -> const std::string & { return members.at(named(prefix, member)); };
    for (const auto &[member, bytes] : members) {
        if (!member.starts_with(prefix)) continue;
        if (member != named(prefix, settings_member) && member != named(prefix, views_member)
            && member != named(prefix, diagram_member) && member != named(prefix, solutions_member))
            return xpp::fail("session AUTO", "unknown AUTO member", Place{name + "/" + member});
    }
    Result<SettingsRead> settings = parse_settings(text(settings_member), file(settings_member));
    if (!settings) return std::unexpected(settings.error());
    const bool diagram = members.contains(named(prefix, diagram_member));
    const bool solutions = members.contains(named(prefix, solutions_member));
    if (diagram != solutions)
        return xpp::fail("session AUTO", xpp::format("its {} is missing", named(prefix, diagram ? solutions_member : diagram_member)), Place{name});
    std::deque<DiagramPoint> points;
    if (diagram) {
        if (const Result<> checked = check_auto_solutions(text(solutions_member), file(solutions_member)); !checked)
            return std::unexpected(checked.error());
        Result<std::deque<DiagramPoint>> read = parse_diagram_csv(text(diagram_member), s.model().node, file(diagram_member));
        if (!read) return std::unexpected(read.error());
        points = std::move(*read);
    }
    Result<ViewsRead> views = parse_views(text(views_member), file(views_member));
    if (!views) return std::unexpected(views.error());
    /* what AUTO accepts for this model: the settings, then each view's
       axes with the settings' parameters */
    if (points.size() > 1)
        for (const DiagramPoint &p : points)
            if (p.d.icp1 >= settings->set.npars)
                return xpp::fail("session AUTO", "diagram parameter index is not among its saved parameters", Place{file(diagram_member)});
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
    return Members{std::move(settings->set), std::move(points), std::move(views->views), diagram ? std::string_view(text(solutions_member)) : std::string_view()};
}

Result<> restore_members(Session &s, Members m)
{
    if (!m.points.empty()) {
        /* Write the restart file before applying any held state. */
        const std::string solutions_path = auto_solutions_file(s);
        xpp::Writer w = xpp::Writer::binary(solutions_path);
        if (!w || !w.write(m.solutions) || !w.commit())
            return xpp::fail("session AUTO", "AUTO's restart solutions cannot be written", Place{solutions_path});
        if (!s.auto_state.bifur.exist) do_auto_win(s);
    }
    std::string why;
    auto_settings_apply(s, m.settings, why); /* members_read checked it */
    auto_data_forget(s); /* the strip described the diagram this one replaces */
    if (!m.points.empty()) diagram_restore(s, std::move(m.points));
    restore_views(s, m.views);
    if (s.auto_state.bifur.exist) redraw_diagram(s);
    return {};
}

} // namespace xpp::snapx::auto_members
