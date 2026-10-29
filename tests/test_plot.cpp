/* W524 (#588): the core functions behind the plot.* commands, which the Curves, Axes and Numerics forms
   call too: a curve added, edited and deleted, the plot's variables, the rUelle shifts, the frozen
   curves' limits and the markers on the data, each accepted and refused. A refusal changes nothing.
   The commands' own arguments are servercheck's registry section. */
#include "load_args.h"
#include "xpptest.h"
#include "browse.h"
#include "data_formats.h"
#include "graf_par.h"
#include "grobs.h"
#include "numerics.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_util.h"
#include <array>
#include <string>

int main()
{
    char name[] = "test_plot", path[] = "examples/ode/lecar.odex";
    char *argv[] = {name, path, nullptr};
    const xpp::Loaded loaded = load_args(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("plot");
    xpp::Session &s = **loaded;
    xpp::batch_start(s);
    GRAPH &g = *s.plot_windows.current;

    /* curves: added, the later ones moving up when one goes, the first one the graph's own */
    xpp::CurveSettings curve = xpp::curve_settings(s, 0);
    curve.x = 0;
    curve.y = 2;
    const auto first = xpp::add_curve(s, curve);
    CHECK(first && *first == 1 && g.nvars == 2 && g.yv[1] == 2);
    curve.y = 1;
    const auto second = xpp::add_curve(s, curve);
    CHECK(second && *second == 2 && g.nvars == 3);
    CHECK(xpp::delete_curve(s, 1));
    CHECK(g.nvars == 2 && g.yv[1] == 1 && g.plot_points[0].size() == 2 && g.style.size() == 2 && g.color.size() == 2);
    CHECK(!xpp::delete_curve(s, 0) && g.nvars == 2);
    CHECK(!xpp::delete_curve(s, 5) && !xpp::delete_curve(s, -1) && g.nvars == 2);
    curve.style.width = 0;
    CHECK(!xpp::add_curve(s, curve) && g.nvars == 2); /* a refused curve leaves none behind */
    curve.style.width = 1;
    curve.style.marker_size = -1;
    CHECK(!xpp::set_curve(s, 1, curve) && g.yv[1] == 1);
    curve.style.marker_size = 3;
    curve.y = 2;
    curve.style.draw = xpp::LineDraw::both;
    CHECK(xpp::set_curve(s, 1, curve) && g.yv[1] == 2 && g.style[1].draw == xpp::LineDraw::both);
    xpp::delete_all_curves(s);
    CHECK(g.nvars == 1);

    /* the plot's variables: counts the way each plot needs them, nothing changed by a refusal */
    const std::array<int, 1> one = {1};
    const std::array<int, 2> up = {1, 2}, down = {2, 1};
    const std::array<int, 4> four = {1, 2, 1, 2};
    CHECK(!xpp::plot_variables(s, xpp::PlotHow::phase_plane, one));
    CHECK(!xpp::plot_variables(s, xpp::PlotHow::phase_plane, four));
    CHECK(!xpp::plot_variables(s, xpp::PlotHow::time_series, std::span<const int>{}));
    CHECK(!xpp::plot_variables(s, xpp::PlotHow::array_plot, down));
    CHECK(!xpp::plot_variables(s, xpp::PlotHow::array_plot, up)); /* no data yet */
    CHECK(xpp::plot_variables(s, xpp::PlotHow::time_series, down) && g.nvars == 2 && g.yv[0] == 2 && g.yv[1] == 1 && g.xv[0] == 0);
    CHECK(xpp::plot_variables(s, xpp::PlotHow::phase_plane, up) && g.nvars == 1 && g.xv[0] == 1 && g.yv[0] == 2);

    /* the rUelle shifts */
    CHECK(xpp::set_axis_shifts(s, 1, 2, 3) && g.xshft == 1 && g.yshft == 2 && g.zshft == 3);
    CHECK(!xpp::set_axis_shifts(s, 4, -1, 0) && g.xshft == 1 && g.yshft == 2 && g.zshft == 3);

    /* the axes' font size and the limits of a 2D plot: refused whole */
    xpp::AxesOrigin axes = xpp::axes_origin(s);
    axes.font_size = 0.5;
    CHECK(!xpp::set_axes_origin(s, axes) && s.plot_export.font_size != 0.5);
    axes.font_size = 14;
    axes.origin[0] = 0.25;
    axes.shown[0] = false;
    CHECK(xpp::set_axes_origin(s, axes) && g.xorg == 0.25 && g.xorgflag == 0 && s.plot_export.font_size == 14);
    xpp::AxesView view = xpp::axes_view(s, 0);
    const double was_low = g.xmin;
    view.lo[0] = view.hi[0] + 1;
    CHECK(!xpp::set_axes_view(s, 0, false, view) && g.xmin == was_low);
    view = xpp::axes_view(s, 0);
    view.lo[0] = 0;
    view.hi[0] = 5;
    view.label[0] = "time";
    CHECK(xpp::set_axes_view(s, 0, false, view) && g.xlo == 0 && g.xhi == 5 && g.xlabel == "time" && g.grtype == 0);

    /* a 3D view's movie varies theta or phi and nothing else */
    xpp::ViewParams params = xpp::view_params(s);
    params.movie = true;
    params.vary = "psi";
    CHECK(!xpp::set_view_params(s, params));

    /* markers on the data: the rows they reach are checked against the stored rows, all or nothing */
    xpp::DataTable data;
    data.names = {"T", "V", "W"};
    data.columns = {{0, 1, 2, 3}, {0.5, 1.5, 2.5, 3.5}, {1, 2, 3, 4}};
    CHECK(xpp::put_stored_data(s, data) == 4);
    xpp::MarkInfo marks;
    marks.type = xpp::MARKER;
    marks.number = 2;
    marks.start = 1;
    marks.skip = 2;
    const std::size_t before = s.grobs.size();
    const auto put = xpp::add_data_markers(s, marks);
    CHECK(put && *put == 2 && s.grobs.size() == before + 2);
    marks.start = 2;
    CHECK(!xpp::add_data_markers(s, marks) && s.grobs.size() == before + 2); /* rows 2 and 4: there is no row 4 */
    marks.start = 0;
    marks.skip = 0;
    CHECK(!xpp::add_data_markers(s, marks) && s.grobs.size() == before + 2);

    /* frozen curves: the legend text and the name have their widths, a slot belongs to its window */
    const auto slot = xpp::freeze_curve(s, 0);
    CHECK(slot.has_value());
    if (slot) {
        xpp::FrozenSettings frozen = xpp::frozen_settings(s, *slot);
        frozen.key = std::string(xpp::frozen_key_max + 1, 'k');
        CHECK(!xpp::set_frozen_curve(s, *slot, frozen));
        frozen.key = std::string(xpp::frozen_key_max, 'k');
        frozen.name = std::string(xpp::frozen_name_max, 'n');
        CHECK(xpp::set_frozen_curve(s, *slot, frozen) && xpp::frozen_settings(s, *slot).name == frozen.name);
        CHECK(!xpp::delete_frozen(s, g.w + 1, *slot));
        CHECK(!xpp::delete_frozen(s, s.plot_windows.draw_win, *slot + 1));
        CHECK(xpp::delete_frozen(s, s.plot_windows.draw_win, *slot));
        CHECK(!xpp::delete_frozen(s, s.plot_windows.draw_win, *slot));
    }
    TEST_REPORT("plot");
}
