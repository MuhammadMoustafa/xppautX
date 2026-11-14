/* autoX's members for this session: see xpp_session_autox.h. Their text is
   xpp_session_autox.cpp's, the manifest and the model's members snapx.cpp's and
   xpp_session.cpp's (the one reader and writer of a file that carries a
   model), the zip xpp_zip.cpp's; this file gathers autoX's state into
   them and puts it back. */
#include "xpp_session_autox.h"
#include "session.h"
#include "autox_state.h"
#include "model.h"
#include "xpp_zip.h"
#include "xpp_io.h"
#include "autox_data.h"
#include "autox_nox.h"
#include "autox_settings.h"
#include "diagram.h"
#include "autox_axis.h"
#include "display_state.h"
#include "odex_block.h"
#include "xpp_error.h"

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <cstddef>
#include <deque>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace xpp::snapx::autox_members {

namespace {

/* the error r, which is one, as the ReadFailed a reader throws */
[[noreturn]] void raise(Result<> r) { throw ReadFailed{std::move(r.error())}; }

/* member name of a file whose autoX members are named prefix and theirs */
std::string named(std::string_view prefix, const char *name) { return std::string(prefix) + name; }

/* a saved view's axes as the settings that give view k them */
AutoxSettingsSet view_settings(const SavedView &v, int k)
{
    AutoxSettingsSet a;
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
    if (!s.autox_state->diag_flag) return {};
    const std::string solutions_path = autox_solutions_file(s);
    std::string solutions;
    if (!xpp::read_bytes(solutions_path, solutions))
        return xpp::fail("save session", "autoX's restart solutions cannot be read", xpp::Place{solutions_path});
    const std::vector<std::string> vars(m.uvar_names.begin(), m.uvar_names.begin() + m.node);
    entries.push_back({named(prefix, diagram_member), diagram_csv(s.diagram->points, m.nupar, vars)});
    entries.push_back({named(prefix, solutions_member), std::move(solutions)});
    return {};
}

void write_state(odex::Out &out, const Session &s)
{
    out.begin("autox");
    write_autox_settings(out, autox_settings_now(s));
    SavedViews views;
    views.active = s.autox_state->active_view;
    for (std::size_t k = 0; k < s.autox_state->views.size(); k++) {
        const AutoxSettingsSet a = autox_settings_view(s, static_cast<int>(k));
        views.views.push_back({*a.plot, a.var, a.par1, a.par2, a.range, s.autox_state->views[k].zoom, s.autox_state->views[k].name, s.autox_state->views[k].hidden, s.autox_state->views[k].second});
    }
    write_views(out, views);
    out.end();
}

namespace {

/* the views saved (W50) as this session's, each with its axes and zoom
   (members_read checked them) */
void restore_views(xpp::Session &s, const SavedViews &saved)
{
    s.autox_state->views.assign(saved.views.size(), AutoxDiagramView{s.autox_state->axes(), {}, false, {}});
    s.autox_state->active_view = 0;
    for (std::size_t k = 0; k < saved.views.size(); k++) {
        const SavedView &v = saved.views[k];
        std::string why;
        autox_settings_apply(s, view_settings(v, static_cast<int>(k)), why);
        /* the ranges as they were, even the ones a form refuses (a Fit of
           a flat quantity leaves its axis a point) */
        AUTOAX &ax = s.autox_state->views[k].axes;
        ax.xmin = v.range[0];
        ax.xmax = v.range[1];
        ax.ymin = v.range[2];
        ax.ymax = v.range[3];
        s.autox_state->views[k].zoom = v.zoom;
        s.autox_state->views[k].name=v.name;
        s.autox_state->views[k].hidden=v.hidden;
        s.autox_state->views[k].second=v.second;
    }
    s.autox_state->active_view = saved.active;
}

} // namespace

Members members_read(const Session &s, const std::map<std::string, std::string> &members, std::string_view prefix,
                     const std::string &name, odex::Block &session)
{
    /* a member's file, for its errors: the file it is in and its own name */
    const auto file = [&](const char *member) { return xpp::format("{}/{}", name, named(prefix, member)); };
    const auto text = [&](const char *member) -> const std::string & { return members.at(named(prefix, member)); };
    for (const auto &[member, bytes] : members) {
        if (!member.starts_with(prefix)) continue;
        if (member != named(prefix, diagram_member) && member != named(prefix, solutions_member))
            raise(xpp::fail("session autoX", "unknown autoX member", Place{name + "/" + member}));
    }
    odex::Block autox = *session.group("autox");
    std::optional<odex::Block> group = autox.group("settings");
    SettingsRead settings = read_autox_settings(*group);
    if (const Result<> checked = autox_settings_check(s, settings.set, -1); !checked) {
        const auto at = settings.places.find(checked.error().field);
        if (at == settings.places.end()) raise(xpp::fail("autoX file", checked.error().what, Place{name}));
        group->fail_at(at->second.first, at->second.second, checked.error().what);
    }
    const bool diagram = members.contains(named(prefix, diagram_member));
    const bool solutions = members.contains(named(prefix, solutions_member));
    if (diagram != solutions)
        raise(xpp::fail("session autoX", xpp::format("its {} is missing", named(prefix, diagram ? solutions_member : diagram_member)), Place{name}));
    std::deque<DIAGRAM> points;
    if (diagram) {
        if (const Result<> checked = check_autox_solutions(text(solutions_member), file(solutions_member), AutoxState::state_copies*s.model().node, s.autox_state->parameter_layout.size(),false,
            2*s.model().node+2); !checked)
            raise(checked);
        Result<std::deque<DIAGRAM>> read = parse_diagram_csv(text(diagram_member), s.model().node, s.model().nupar, file(diagram_member));
        if (!read) raise(std::unexpected(read.error()));
        points = std::move(*read);
    }
    ViewsRead views = read_views(autox);
    autox.done();
    /* what autoX accepts for this model: the settings, then each view's
       axes with the settings' parameters */
    if (points.size() > 1)
        for (const DIAGRAM &p : points)
            if (!p.icp1.is_parameter())
                raise(xpp::fail("session autoX", "diagram parameter index is not among its saved parameters", Place{file(diagram_member)}));
    const int nviews = static_cast<int>(views.views.views.size());
    for (int k = 0; k < nviews; k++) {
        AutoxSettingsSet a = view_settings(views.views.views[static_cast<std::size_t>(k)], k);
        if (const Result<> checked = autox_settings_check(s, a, nviews); !checked)
            autox.fail_at(views.places[static_cast<std::size_t>(k)].first, views.places[static_cast<std::size_t>(k)].second, checked.error().what);
    }
    return Members{std::move(settings.set), std::move(points), std::move(views.views), diagram ? std::string_view(text(solutions_member)) : std::string_view()};
}

Result<> restore_members(Session &s, Members m)
{
    if (!m.points.empty()) {
        /* Write the restart file before applying any held state. */
        const std::string solutions_path = autox_solutions_file(s);
        xpp::Writer w = xpp::Writer::binary(solutions_path);
        if (!w || !w.write(m.solutions) || !w.commit())
            return xpp::fail("session autoX", "autoX's restart solutions cannot be written", Place{solutions_path});
        s.autox_state->solutions_index.reset(); /* the file is another one */
        if (!s.autox_state->bifur.exist) do_autox_win(s);
    }
    std::string why;
    autox_settings_apply(s, m.settings, why); /* members_read checked it */
    autox_data_forget(s); /* the strip described the diagram this one replaces */
    if (!m.points.empty()) diagram_restore(s, std::move(m.points));
    // Foreign XPPAUT imports hold one active axis record, with no saved view list.
    if (!m.views.views.empty()) restore_views(s, m.views);
    if (s.autox_state->bifur.exist) redraw_diagram(s);
    return {};
}

} // namespace xpp::snapx::autox_members
