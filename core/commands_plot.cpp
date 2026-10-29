/* The plot resource's registry rows (W524): a graph's curves (add, edit, delete), its frozen curves and their
   legend, an imported bifurcation diagram, the axes (2D, 3D, the origin, the 3D view, the rUelle shifts), the
   variables it plots, the colormap, markers on its data, and what it exports (a PDF or SVG picture, its
   data). Each handler gets its arguments validated and typed (command_def.h) and calls the same core
   function the key menus' forms do (graf_par.h, grobs.h, numerics.h): one behaviour, whichever front end
   asks. A graph is named by `window` (the selected one when it is left out); an argument left out keeps the
   setting the graph has. */
#include "browse.h"
#include "command_def.h"
#include "command_graph.h"
#include "graf_par.h"
#include "grobs.h"
#include "image_format.h"
#include "numerics.h"
#include "model_options.h"
#include "session.h"
#include "typed_values.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_util.h"
#include <array>
#include <cstdio>
#include <string_view>

namespace xpp::command {
namespace {

constexpr const char *WINDOW_KEY = "window", *COLOR_KEY = "color";

constexpr auto DRAW_CHOICES = c_names(line_draw_names);
constexpr auto DASH_CHOICES = c_names(line_dash_names);
constexpr auto MARKER_CHOICES = c_names(line_marker_names);
constexpr auto SHAPE_CHOICES = c_names(marker_shape_names);
constexpr auto COLORMAP_CHOICES = c_names(colormap_names);
constexpr std::array VARY_CHOICES = {"theta", "phi"};
constexpr std::array<std::string_view, 3> HOW_NAMES = {"time_series", "phase_plane", "array_plot"};
constexpr auto HOW_CHOICES = c_names(HOW_NAMES);
constexpr std::array HOW_PLOTTED = {PlotHow::time_series, PlotHow::phase_plane, PlotHow::array_plot};

std::string column_name(const Session &s, int column) { return ind_to_sym(s, column); }

/* a style: the given members over `style` */
LineStyle style_of(const Arguments &args, LineStyle style)
{
    if (const auto draw = args.optional<std::string>("draw")) style.draw = *parse_line_draw(*draw);
    if (const auto dash = args.optional<std::string>("dash")) style.dash = *parse_line_dash(*dash);
    if (const auto width = args.optional<double>("width")) style.width = *width;
    if (const auto marker = args.optional<std::string>("marker")) style.marker = *parse_line_marker(*marker);
    if (const auto size = args.optional<double>("marker_size")) style.marker_size = *size;
    return style;
}

/* the graph the command acts on, active while it works */
struct OnGraph {
    Result<int> index;
    OnGraph(Session &s, const Command &command, const Arguments &args, const Place &place) : index(graph_of_command(s, command, args, place))
    {
        if (index) active.emplace(s, *index);
    }
    std::optional<ActiveGraph> active;
};

#define ON_GRAPH(name)                                                                  \
    OnGraph name(s, command, args, place);                                              \
    if (!name.index) return std::unexpected<Error>(name.index.error())

CurveSettings curve_of(const Session &s, const Arguments &args, CurveSettings curve)
{
    if (const auto x = args.optional<std::string>("x")) curve.x = variable_column(s, *x);
    if (const auto y = args.optional<std::string>("y")) curve.y = variable_column(s, *y);
    if (const auto z = args.optional<std::string>("z")) curve.z = variable_column(s, *z);
    if (const auto color = args.optional<Color>(COLOR_KEY)) curve.color = *color;
    curve.style = style_of(args, curve.style);
    return curve;
}

Result<std::string> add_curve_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const Result<int> added = add_curve(s, curve_of(s, args, curve_settings(s, 0)));
    if (!added) return std::unexpected<Error>(added.error());
    return ResultObject().integer("curve_index", *added).done();
}

Result<std::string> edit_curve_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const int curve = static_cast<int>(args.get<long long>("curve_index"));
    if (curve < 0 || curve >= s.plot_windows.current->nvars)
        return fail(command.name, xpp::format("no curve {}: the window has curves 0 to {}", curve, s.plot_windows.current->nvars - 1), place);
    if (const Result<> set = set_curve(s, curve, curve_of(s, args, curve_settings(s, curve))); !set) return std::unexpected<Error>(set.error());
    return ResultObject().integer("curve_index", curve).boolean("updated", true).done();
}

Result<std::string> delete_curve_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const int curve = static_cast<int>(args.optional<long long>("curve_index").value_or(s.plot_windows.current->nvars - 1));
    if (const Result<> deleted = delete_curve(s, curve); !deleted) return std::unexpected<Error>(deleted.error());
    return ResultObject().integer("remaining_curves", s.plot_windows.current->nvars).done();
}

/* a command that clears something of the graph: the core function, then {"cleared":true} */
template <void (*Clear)(Session &)>
Result<std::string> clear_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    Clear(s);
    return ResultObject().boolean("cleared", true).done();
}

FrozenSettings frozen_of(const Arguments &args, FrozenSettings frozen)
{
    if (const auto key = args.optional<std::string>("key")) frozen.key = *key;
    if (const auto name = args.optional<std::string>("name")) frozen.name = *name;
    if (const auto color = args.optional<Color>(COLOR_KEY)) frozen.color = *color;
    frozen.style = style_of(args, frozen.style);
    return frozen;
}

Result<std::string> freeze_curve_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    /* checked before the curve is made: a refused setting leaves no frozen curve behind */
    if (const Result<> checked = check_frozen_settings(frozen_of(args, FrozenSettings{})); !checked) return std::unexpected<Error>(checked.error());
    const Result<int> slot = freeze_curve(s, 0);
    if (!slot) return std::unexpected<Error>(slot.error());
    if (const Result<> set = set_frozen_curve(s, *slot, frozen_of(args, frozen_settings(s, *slot))); !set) return std::unexpected<Error>(set.error());
    return ResultObject().integer("slot", *slot).text("name", s.frozen_curves.curve[static_cast<std::size_t>(*slot)].name).done();
}

Result<std::string> edit_frozen_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const int slot = static_cast<int>(args.get<long long>("slot"));
    if (const Result<> held = frozen_slot_in_window(s, s.plot_windows.draw_win, slot, command.name); !held) return std::unexpected<Error>(held.error());
    if (const Result<> set = set_frozen_curve(s, slot, frozen_of(args, frozen_settings(s, slot))); !set) return std::unexpected<Error>(set.error());
    return ResultObject().integer("slot", slot).boolean("updated", true).done();
}

Result<std::string> delete_frozen_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const int slot = static_cast<int>(args.get<long long>("slot"));
    if (const Result<> deleted = delete_frozen(s, s.plot_windows.draw_win, slot); !deleted) return std::unexpected<Error>(deleted.error());
    return ResultObject().integer("slot", slot).boolean("deleted", true).done();
}

Result<std::string> set_frozen_legend_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    std::optional<std::pair<double, double>> at;
    if (args.has("x")) at = std::pair<double, double>{args.get<double>("x"), args.get<double>("y")};
    const bool visible = args.get<bool>("visible");
    set_frozen_legend(s, visible, at);
    return ResultObject().boolean("legend_visible", visible).done();
}

Result<std::string> import_bifurcation_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const std::string &file = args.get<std::string>("file");
    const Result<int> count = import_bifurcation(s, file);
    if (!count) return std::unexpected<Error>(count.error());
    return ResultObject().text("file", file).integer("curves_imported", *count).done();
}

Result<std::string> set_auto_freeze_command(Session &s, const Command &, const Arguments &args, const Place &)
{
    const bool enabled = args.get<bool>("enabled");
    s.frozen_curves.auto_freeze = enabled ? 1 : 0;
    return ResultObject().boolean("auto_freeze", enabled).done();
}

Result<std::string> export_picture(Session &s, const Command &command, const Arguments &args, const Place &place, int format)
{
    ON_GRAPH(graph);
    const std::string &file = args.get<std::string>("file");
    XppPlotExport settings=s.plot_export;
    if (const auto font_size = args.optional<double>("axes_font_size")) settings.font_size = *font_size;
    settings.color = args.optional<bool>("color").value_or(settings.color);
    const Result<bool> written = write_plot_picture(s, format, file, &settings);
    if (!written) return std::unexpected<Error>(written.error());
    if (!*written) return fail(command.name, xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), place);
    const Result<unsigned long long> size = file_size(file, command, place);
    if (!size) return std::unexpected<Error>(size.error());
    return ResultObject().text("file", file).integer("bytes_written", static_cast<long long>(*size)).done();
}

Result<std::string> export_pdf(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_picture(s, command, args, place, IMAGE_FORMAT_PDF);
}

Result<std::string> export_svg(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_picture(s, command, args, place, IMAGE_FORMAT_SVG);
}

Result<std::string> export_data(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const std::string &file = args.get<std::string>("file");
    const Result<std::size_t> rows = write_plot_data(s, args.optional<std::string>("format").value_or(std::string{}), file);
    if (!rows) return std::unexpected<Error>(rows.error());
    if (*rows == 0) return fail(command.name, xpp::format("\"{}\" exists and replacing it was declined: nothing was written", file), place);
    return ResultObject().text("file", file).integer("rows_written", static_cast<long long>(*rows)).done();
}

Result<std::string> set_axes_origin_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    AxesOrigin axes = axes_origin(s);
    constexpr std::array<const char *, 3> origins = {"x_origin", "y_origin", "z_origin"}, shown = {"x_origin_shown", "y_origin_shown", "z_origin_shown"};
    for (std::size_t i = 0; i < 3; ++i) {
        axes.origin[i] = args.optional<double>(origins[i]).value_or(axes.origin[i]);
        axes.shown[i] = args.optional<bool>(shown[i]).value_or(axes.shown[i]);
    }
    axes.font_size = args.optional<double>("axes_font_size").value_or(axes.font_size);
    if (const Result<> set = set_axes_origin(s, axes); !set) return std::unexpected<Error>(set.error());
    return ResultObject().boolean("origin_updated", true).done();
}

Result<std::string> set_colormap_command(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &name = args.get<std::string>("colormap");
    set_colormap(s, static_cast<Colormap>(choice_index(colormap_names, name)));
    return ResultObject().text("colormap", name).done();
}

/* the axes the arguments name over the window's own */
AxesView axes_of(const Session &s, const Arguments &args, AxesView view, bool three)
{
    constexpr std::array<const char *, 3> columns = {"x", "y", "z"}, lows = {"x_min", "y_min", "z_min"}, highs = {"x_max", "y_max", "z_max"},
                                          labels = {"x_label", "y_label", "z_label"};
    for (std::size_t i = 0; i < (three ? 3u : 2u); ++i) {
        if (const auto name = args.optional<std::string>(columns[i])) view.column[i] = variable_column(s, *name);
        view.lo[i] = args.optional<double>(lows[i]).value_or(view.lo[i]);
        view.hi[i] = args.optional<double>(highs[i]).value_or(view.hi[i]);
        if (const auto label = args.optional<std::string>(labels[i])) view.label[i] = *label;
    }
    if (three) {
        constexpr std::array<const char *, 4> window = {"window_x_min", "window_x_max", "window_y_min", "window_y_max"};
        for (std::size_t i = 0; i < window.size(); ++i) view.window[i] = args.optional<double>(window[i]).value_or(view.window[i]);
    }
    return view;
}

Result<std::string> set_axes(Session &s, const Command &command, const Arguments &args, const Place &place, bool three)
{
    ON_GRAPH(graph);
    if (const Result<> set = set_axes_view(s, 0, three, axes_of(s, args, axes_view(s, 0), three)); !set) return std::unexpected<Error>(set.error());
    const GRAPH &g = *s.plot_windows.current;
    ResultObject result;
    result.text("axes", three ? "3d" : "2d").text("x", column_name(s, g.xv[0])).text("y", column_name(s, g.yv[0]));
    if (three) result.text("z", column_name(s, g.zv[0]));
    return result.done();
}

Result<std::string> set_2d(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return set_axes(s, command, args, place, false);
}

Result<std::string> set_3d(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return set_axes(s, command, args, place, true);
}

Result<std::string> set_3d_view(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    if (s.plot_windows.current->grtype < 5) return fail(command.name, "The window is not a 3D plot: make it one with plot.set_3d", place);
    ViewParams view = view_params(s);
    if (const auto perspective = args.optional<bool>("perspective")) view.persp = *perspective ? 1 : 0;
    view.zplane = args.optional<double>("z_plane").value_or(view.zplane);
    view.zview = args.optional<double>("z_view").value_or(view.zview);
    view.theta = args.optional<double>("theta").value_or(view.theta);
    view.phi = args.optional<double>("phi").value_or(view.phi);
    view.movie = args.optional<bool>("movie").value_or(false);
    if (view.movie) {
        view.yes = "Y";
        view.vary = args.optional<std::string>("vary").value_or(view.vary);
        view.start = args.optional<double>("start_angle").value_or(view.start);
        view.incr = args.optional<double>("increment").value_or(view.incr);
        view.nclip = static_cast<int>(args.optional<long long>("increments").value_or(view.nclip));
    } else if (args.has("vary") || args.has("start_angle") || args.has("increment") || args.has("increments"))
        return fail(command.name, "vary, start_angle, increment and increments are the movie's: give movie true with them", place);
    if (const Result<> set = set_view_params(s, view); !set) return std::unexpected<Error>(set.error());
    return ResultObject().boolean("view_updated", true).done();
}

Result<std::string> set_shifted_axes(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const GRAPH &g = *s.plot_windows.current;
    const auto shift = [&](const char *name, int now) { return static_cast<int>(args.optional<long long>(name).value_or(now)); };
    const int x = shift("x_shift", g.xshft), y = shift("y_shift", g.yshft), z = shift("z_shift", g.zshft);
    if (const Result<> set = set_axis_shifts(s, x, y, z); !set) return std::unexpected<Error>(set.error());
    return ResultObject().integer("x_shift", x).integer("y_shift", y).integer("z_shift", z).done();
}

Result<std::string> variable_vs_time_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const std::string &variable = args.get<std::string>("variable");
    if (const Result<> set = variable_vs_time(s, variable); !set) return std::unexpected<Error>(set.error());
    return ResultObject().text("x", "t").text("y", variable).done();
}

Result<std::string> add_data_markers_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    MarkInfo markers = s.marker;
    if (const auto shape = args.optional<std::string>("shape")) markers.type = MARKER + choice_index(marker_shape_names, *shape);
    if (const auto color = args.optional<Color>(COLOR_KEY)) markers.color = *color;
    markers.size = args.optional<double>("size").value_or(markers.size);
    markers.number = static_cast<int>(args.optional<long long>("count").value_or(markers.number));
    markers.start = static_cast<int>(args.optional<long long>("first_row").value_or(markers.start));
    markers.skip = static_cast<int>(args.optional<long long>("stride").value_or(markers.skip));
    const Result<int> added = add_data_markers(s, markers);
    if (!added) return std::unexpected<Error>(added.error());
    return ResultObject().integer("count", *added).done();
}

Result<std::string> set_variables(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    ON_GRAPH(graph);
    const std::string &how = args.get<std::string>("how");
    const std::vector<std::string> names = args.list<std::string>("names");
    const Model &model = s.model();
    std::vector<int> variables;
    std::string listed = "[";
    for (const std::string &name : names) {
        const int index = find_user_name(model, ICBOX, name);
        if (index < 0 || index >= model.node + model.nmarkov)
            return fail(command.name, xpp::format("\"{}\" is not a state of the model: only the states plot with set_variables", name), place);
        variables.push_back(index + 1);
        if (listed.size() > 1) listed += ',';
        json_append_string(listed, name);
    }
    const PlotHow plotted = HOW_PLOTTED[static_cast<std::size_t>(choice_index(HOW_NAMES, how))];
    if (const Result<> set = plot_variables(s, plotted, variables); !set) return std::unexpected<Error>(set.error());
    return ResultObject().text("how", how).raw("names", listed + ']').done();
}

constexpr Argument WINDOW = {WINDOW_KEY, Type::window, false};
constexpr Argument COLOR = {COLOR_KEY, Type::color, false};
constexpr Argument COLUMN_X = {"x", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument COLUMN_Y = {"y", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument COLUMN_Z = {"z", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument REQUIRED_X = {"x", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument REQUIRED_Y = {"y", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column};
constexpr Argument CURVE = {"curve_index", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0)};
constexpr Argument SLOT = {"slot", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0)};
constexpr Argument ENABLED = {"enabled", Type::boolean, true};
constexpr Argument FILE = {"file", Type::file, true};
constexpr Argument REAL_X_MIN = {"x_min", Type::real, false}, REAL_X_MAX = {"x_max", Type::real, false}, REAL_Y_MIN = {"y_min", Type::real, false},
                   REAL_Y_MAX = {"y_max", Type::real, false}, REAL_Z_MIN = {"z_min", Type::real, false}, REAL_Z_MAX = {"z_max", Type::real, false};
constexpr Argument LABEL_X = {"x_label", Type::text, false}, LABEL_Y = {"y_label", Type::text, false}, LABEL_Z = {"z_label", Type::text, false};
constexpr Argument AXES_FONT = {"axes_font_size", Type::real, false, MAX_TEXT_BYTES, {}, constant(axes_font_size_min), constant(axes_font_size_max)};

constexpr std::array STYLE_ARGUMENTS = {COLOR, Argument{"draw", Type::choice, false, MAX_TEXT_BYTES, DRAW_CHOICES},
                                        Argument{"dash", Type::choice, false, MAX_TEXT_BYTES, DASH_CHOICES}, Argument{"width", Type::real, false},
                                        Argument{"marker", Type::choice, false, MAX_TEXT_BYTES, MARKER_CHOICES},
                                        Argument{"marker_size", Type::real, false, MAX_TEXT_BYTES, {}, constant(0)}};
constexpr std::array WINDOW_ARGUMENTS = {WINDOW};
constexpr std::array NEW_CURVE_ARGUMENTS = join(std::array{WINDOW, REQUIRED_X, REQUIRED_Y, COLUMN_Z}, STYLE_ARGUMENTS);
constexpr std::array EDIT_CURVE_ARGUMENTS = join(std::array{WINDOW, CURVE, COLUMN_X, COLUMN_Y, COLUMN_Z}, STYLE_ARGUMENTS);
constexpr std::array DELETE_CURVE_ARGUMENTS = {WINDOW, Argument{"curve_index", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)}};
constexpr std::array FREEZE_ARGUMENTS = join(std::array{WINDOW, Argument{"name", Type::text, false, frozen_name_max}, Argument{"key", Type::text, false, frozen_key_max}}, STYLE_ARGUMENTS);
constexpr std::array EDIT_FROZEN_ARGUMENTS = join(std::array{WINDOW, SLOT, Argument{"name", Type::text, false, frozen_name_max}, Argument{"key", Type::text, false, frozen_key_max}}, STYLE_ARGUMENTS);
constexpr std::array DELETE_FROZEN_ARGUMENTS = {WINDOW, SLOT};
constexpr std::array LEGEND_ARGUMENTS = {WINDOW, Argument{"visible", Type::boolean, true}, Argument{"x", Type::real, false}, Argument{"y", Type::real, false}};
constexpr std::array LEGEND_POSITION = {"x", "y"};
constexpr std::array LEGEND_GROUPS = {Group{GroupRule::together, LEGEND_POSITION}};
constexpr std::array IMPORT_ARGUMENTS = {WINDOW, FILE};
constexpr std::array ENABLED_ARGUMENTS = {ENABLED};
constexpr std::array PICTURE_ARGUMENTS = {WINDOW, FILE, Argument{"color", Type::boolean, false}, AXES_FONT};
constexpr std::array DATA_ARGUMENTS = {WINDOW, FILE, Argument{"format", Type::text, false}};
constexpr std::array ORIGIN_ARGUMENTS = {WINDOW, Argument{"x_origin", Type::real, false}, Argument{"y_origin", Type::real, false},
                                         Argument{"z_origin", Type::real, false}, Argument{"x_origin_shown", Type::boolean, false},
                                         Argument{"y_origin_shown", Type::boolean, false}, Argument{"z_origin_shown", Type::boolean, false}, AXES_FONT};
constexpr std::array COLORMAP_ARGUMENTS = {Argument{"colormap", Type::choice, true, MAX_TEXT_BYTES, COLORMAP_CHOICES}};
constexpr std::array AXES_2D_ARGUMENTS = {WINDOW, COLUMN_X, COLUMN_Y, REAL_X_MIN, REAL_X_MAX, REAL_Y_MIN, REAL_Y_MAX, LABEL_X, LABEL_Y};
constexpr std::array AXES_3D_ARGUMENTS = {WINDOW, COLUMN_X, COLUMN_Y, COLUMN_Z, REAL_X_MIN, REAL_X_MAX, REAL_Y_MIN, REAL_Y_MAX, REAL_Z_MIN, REAL_Z_MAX,
                                          LABEL_X, LABEL_Y, LABEL_Z, Argument{"window_x_min", Type::real, false}, Argument{"window_x_max", Type::real, false},
                                          Argument{"window_y_min", Type::real, false}, Argument{"window_y_max", Type::real, false}};
constexpr std::array VIEW_3D_ARGUMENTS = {WINDOW, Argument{"perspective", Type::boolean, false}, Argument{"z_plane", Type::real, false},
                                          Argument{"z_view", Type::real, false}, Argument{"theta", Type::real, false}, Argument{"phi", Type::real, false},
                                          Argument{"movie", Type::boolean, false}, Argument{"vary", Type::choice, false, MAX_TEXT_BYTES, VARY_CHOICES},
                                          Argument{"start_angle", Type::real, false}, Argument{"increment", Type::real, false},
                                          Argument{"increments", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)}};
constexpr std::array SHIFT_ARGUMENTS = {WINDOW, Argument{"x_shift", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)},
                                        Argument{"y_shift", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)},
                                        Argument{"z_shift", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)}};
constexpr std::array VARIABLE_ARGUMENTS = {WINDOW, Argument{"variable", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::column}};
constexpr std::array MARKERS_ARGUMENTS = {WINDOW, Argument{"shape", Type::choice, false, MAX_TEXT_BYTES, SHAPE_CHOICES}, COLOR,
                                          Argument{"size", Type::real, false, MAX_TEXT_BYTES, {}, constant(0)},
                                          Argument{"count", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
                                          Argument{"first_row", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)},
                                          Argument{"stride", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)}};
constexpr std::array VARIABLES_ARGUMENTS = {WINDOW, Argument{"how", Type::choice, true, MAX_TEXT_BYTES, HOW_CHOICES},
                                            Argument{"names", Type::list, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable, Type::model_name}};

constexpr std::array CURVE_RESULTS = {ResultField{"curve_index", "integer"}};
constexpr std::array EDITED_CURVE_RESULTS = {ResultField{"curve_index", "integer"}, ResultField{"updated", "boolean"}};
constexpr std::array REMAINING_RESULTS = {ResultField{"remaining_curves", "integer"}};
constexpr std::array CLEARED_RESULTS = {ResultField{"cleared", "boolean"}};
constexpr std::array FROZEN_RESULTS = {ResultField{"slot", "integer"}, ResultField{"name", "text"}};
constexpr std::array EDITED_FROZEN_RESULTS = {ResultField{"slot", "integer"}, ResultField{"updated", "boolean"}};
constexpr std::array DELETED_FROZEN_RESULTS = {ResultField{"slot", "integer"}, ResultField{"deleted", "boolean"}};
constexpr std::array LEGEND_RESULTS = {ResultField{"legend_visible", "boolean"}};
constexpr std::array IMPORT_RESULTS = {ResultField{"file", "file"}, ResultField{"curves_imported", "integer"}};
constexpr std::array AUTO_FREEZE_RESULTS = {ResultField{"auto_freeze", "boolean"}};
constexpr std::array PICTURE_RESULTS = {ResultField{"file", "file"}, ResultField{"bytes_written", "integer"}};
constexpr std::array DATA_RESULTS = {ResultField{"file", "file"}, ResultField{"rows_written", "integer"}};
constexpr std::array ORIGIN_RESULTS = {ResultField{"origin_updated", "boolean"}};
constexpr std::array COLORMAP_RESULTS = {ResultField{"colormap", "choice"}};
constexpr std::array AXES_2D_RESULTS = {ResultField{"axes", "choice"}, ResultField{"x", "name"}, ResultField{"y", "name"}};
constexpr std::array AXES_3D_RESULTS = {ResultField{"axes", "choice"}, ResultField{"x", "name"}, ResultField{"y", "name"}, ResultField{"z", "name"}};
constexpr std::array VIEW_3D_RESULTS = {ResultField{"view_updated", "boolean"}};
constexpr std::array SHIFT_RESULTS = {ResultField{"x_shift", "integer"}, ResultField{"y_shift", "integer"}, ResultField{"z_shift", "integer"}};
constexpr std::array VARIABLE_RESULTS = {ResultField{"x", "name"}, ResultField{"y", "name"}};
constexpr std::array MARKERS_RESULTS = {ResultField{"count", "integer"}};
constexpr std::array VARIABLES_RESULTS = {ResultField{"how", "choice"}, ResultField{"names", "list of name"}};
constexpr std::array PLOT_EVENTS = {"plots", "state", "done"};
constexpr std::array SERIES_EVENTS = {"plots", "series", "state", "done"};
constexpr std::array MARKS_EVENTS = {"marks", "state", "done"};
constexpr std::array STATE_EVENTS = {"state", "done"};

constexpr std::array ROWS = {
    Command{"plot.add_curve", NEW_CURVE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Curves/Add curve",
            "Add a curve to a graph: the data columns it plots (x, y and, in 3D, z) and its colour and line style; what is left out is the first curve's.",
            CURVE_RESULTS, SERIES_EVENTS, true, add_curve_command},
    Command{"plot.delete_curve", DELETE_CURVE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Curves/Delete last",
            "Delete a curve of a graph (the last one when curve_index is left out); the first curve is the graph's own and stays.", REMAINING_RESULTS,
            PLOT_EVENTS, true, delete_curve_command},
    Command{"plot.delete_all_curves", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Curves/Remove all",
            "Delete every curve of a graph but the first.", CLEARED_RESULTS, PLOT_EVENTS, true, clear_command<delete_all_curves>},
    Command{"plot.edit_curve", EDIT_CURVE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Curves/Edit curve",
            "Change a curve of a graph: its data columns, colour and line style; what is left out stays.", EDITED_CURVE_RESULTS, PLOT_EVENTS, true,
            edit_curve_command},
    Command{"plot.freeze_curve", FREEZE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Freeze/Freeze",
            "Freeze the graph's first curve as it is now into a frozen curve of its own, with a legend key, a name, a colour and a line style.",
            FROZEN_RESULTS, SERIES_EVENTS, true, freeze_curve_command},
    Command{"plot.delete_frozen", DELETE_FROZEN_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Freeze/Delete",
            "Delete one of the graph's frozen curves, by its slot (freeze_curve and the marks event give it).", DELETED_FROZEN_RESULTS, PLOT_EVENTS,
            true, delete_frozen_command},
    Command{"plot.edit_frozen", EDIT_FROZEN_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Freeze/Edit",
            "Change one of the graph's frozen curves: its legend key, name, colour and line style; what is left out stays.", EDITED_FROZEN_RESULTS,
            PLOT_EVENTS, true, edit_frozen_command},
    Command{"plot.delete_all_frozen", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Freeze/Remove all",
            "Delete every frozen curve of a graph.", CLEARED_RESULTS, PLOT_EVENTS, true, clear_command<kill_frz>},
    Command{"plot.set_frozen_legend", LEGEND_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Freeze/Key",
            "Show or hide the frozen curves' legend; x and y, the plot coordinates of its first line, move it.", LEGEND_RESULTS, PLOT_EVENTS, true,
            set_frozen_legend_command, LEGEND_GROUPS},
    Command{"plot.import_bifurcation", IMPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "Plot/Freeze/Bif.Diag",
            "Show an AUTO Write pts file of the model's folder (six columns, checked whole first) in the graph as curves by kind.", IMPORT_RESULTS,
            SERIES_EVENTS, true, import_bifurcation_command},
    Command{"plot.clear_bifurcation", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Freeze/Clr. BD",
            "Remove the imported bifurcation diagram from the graph.", CLEARED_RESULTS, PLOT_EVENTS, true, clear_command<clear_bifurcation>},
    Command{"plot.set_auto_freeze", ENABLED_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Plot, "Plot/Freeze/On freeze",
            "Freeze the first curve of a graph after every run.", AUTO_FREEZE_RESULTS, STATE_EVENTS, true, set_auto_freeze_command},
    Command{"plot.export_pdf", PICTURE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "File/Curves/PDF",
            "Write the graph as a PDF picture into a file of the model's folder; colour and axes_font_size are the session's when left out and stay.",
            PICTURE_RESULTS, STATE_EVENTS, true, export_pdf, {}, {}, "write", "*"},
    Command{"plot.export_svg", PICTURE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "File/Curves/SVG",
            "Write the graph as an SVG picture into a file of the model's folder; colour and axes_font_size are the session's when left out and stay.",
            PICTURE_RESULTS, STATE_EVENTS, true, export_svg, {}, {}, "write", "*"},
    Command{"plot.export_data", DATA_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "File/Curves/export data",
            "Write what the graph shows (its curves and frozen curves, as the columns curve, x, y and z in 3D) into a file of the model's folder, in the data format named by format (dat, csv, csv.gz, npz) or by the file's extension.",
            DATA_RESULTS, STATE_EVENTS, true, export_data, {}, {}, "write", "*"},
    Command{"plot.set_axes_origin", ORIGIN_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Curves/axes opts",
            "Set where the graph's axes cross, whether each axis is drawn there, and the font size of the axes' text; what is left out stays.",
            ORIGIN_RESULTS, PLOT_EVENTS, true, set_axes_origin_command},
    Command{"plot.set_colormap", COLORMAP_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Plot, "Plot/Curves/Colormap",
            "Set the session's colour scale, which the array plot, the direction field and the pictures use.", COLORMAP_RESULTS, PLOT_EVENTS, true,
            set_colormap_command},
    Command{"plot.set_2d", AXES_2D_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Axes/2D",
            "Make the graph a 2D plot of two data columns with these limits and labels; what is left out stays.", AXES_2D_RESULTS, PLOT_EVENTS, true,
            set_2d},
    Command{"plot.set_3d", AXES_3D_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Axes/3D",
            "Make the graph a 3D plot of three data columns with these limits, labels and window on the screen; what is left out stays.",
            AXES_3D_RESULTS, PLOT_EVENTS, true, set_3d},
    Command{"plot.set_3d_view", VIEW_3D_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/3D view",
            "Set a 3D graph's perspective, planes and angles; movie true films the rotation as the angle named by vary goes from start_angle in increments steps of increment.",
            VIEW_3D_RESULTS, PLOT_EVENTS, true, set_3d_view},
    Command{"plot.set_shifted_axes", SHIFT_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Plot/Numerics/rUelle plot",
            "Plot each axis of the graph that many rows behind (a Ruelle-Takens plot of delayed values); what is left out stays.", SHIFT_RESULTS,
            PLOT_EVENTS, true, set_shifted_axes},
    Command{"plot.variable_vs_time", VARIABLE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Variable vs time",
            "Plot a data column against time in the graph, its view fitted to the data.", VARIABLE_RESULTS, PLOT_EVENTS, true,
            variable_vs_time_command},
    Command{"plot.add_data_markers", MARKERS_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Text,etc/marker(S)",
            "Put count markers on the graph's first curve, every stride-th row from first_row; what is left out is the last Add markers' setting.",
            MARKERS_RESULTS, MARKS_EVENTS, true, add_data_markers_command},
    Command{"plot.set_variables", VARIABLES_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Set variables",
            "Plot states of the model: time_series (each against time), phase_plane (two or three, the first on the x axis) or array_plot (from the first of two to the later).",
            VARIABLES_RESULTS, PLOT_EVENTS, true, set_variables}};

} // namespace

std::span<const Command> plot_commands() { return ROWS; }

Result<unsigned long long> file_size(std::string_view file, const Command &command, const Place &place)
{
    ::FILE *opened = nullptr;
    unsigned long long size = 0;
    const int status = files::open(file, opened, size);
    const UniqueFile closer(opened);
    if (status != XPP_FILES_OK)
        return fail(command.name, xpp::format("\"{}\" was written but cannot be read back: {}", file, files::status_text(status)), place);
    return size;
}

} // namespace xpp::command
