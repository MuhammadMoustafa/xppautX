/* The window resource's registry rows (W353, moved here at W530): create,
   rename, select, hide, show, destroy and list the windows (graphs, AUTO
   views, the data browser, the animation and the array plot: W515), the
   redraw mode, SimPlot.
   Each handler gets its arguments validated and typed (command_def.h). */
#include "command_def.h"
#include "command_graph.h"
#include "command_registry.h"
#include "json_error.h"
#include "many_pops.h"
#include "auto_nox.h"
#include "graf_par.h"
#include "graphics.h"
#include "model_options.h"
#include "plot_data.h"
#include "session.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include <algorithm>
#include <array>
#include <tuple>

namespace xpp::command {
namespace {

constexpr const char *WINDOW_KEY = "window";
constexpr const char *NAME_KEY = "name";
constexpr const char *CONFIRM_KEY = "confirm";

std::string target_name(const Session &s, const Arguments &args)
{
    return args.optional<std::string>(WINDOW_KEY).value_or(s.selected_window);
}

Result<std::string> unknown_window(const Command &command, const std::string &name, const Place &place)
{
    return fail(command.name, xpp::format("Unknown window \"{}\"", name), place);
}

/* the windows that are graphs or AUTO views: the ones with a name of the user's, to select or rename */
Result<std::string> named_by_user(const Command &command, std::string_view name, WindowKind kind, const Place &place)
{
    if (kind == WindowKind::graph || kind == WindowKind::diagram) return {};
    return fail(command.name, xpp::format("\"{}\" is the {} window: this applies to a graph or an AUTO view", name,
                                          window_kind_name(kind)), place);
}

std::string window_result(std::string_view name)
{
    std::string result = "{\"window\":";
    json_append_string(result, name);
    return result;
}

std::string bounds_result(const Session &s)
{
    const GRAPH &g = *s.plot_windows.current;
    return xpp::format("{{\"x_min\":{},\"x_max\":{},\"y_min\":{},\"y_max\":{}}}", xpp::number(g.xlo), xpp::number(g.xhi),
                       xpp::number(g.ylo), xpp::number(g.yhi));
}

/* the bounds of the active graph's view after an operation on it */
using BoundsOperation = void (*)(Session &, const Arguments &);

Result<std::string> change_view(Session &s, const Command &command, const Arguments &args, const Place &place, BoundsOperation operate)
{
    const Result<int> index = graph_of_command(s, command, args, place);
    if (!index) return std::unexpected<Error>(index.error());
    const ActiveGraph active(s, *index);
    operate(s, args);
    set_normal_scale(s);
    return bounds_result(s);
}

/* what the box arguments say, over the view now: an argument left out keeps the view's */
struct Box { double x_min, x_max, y_min, y_max; };
Box box_of(const Session &s, const Arguments &args)
{
    const GRAPH &g = *s.plot_windows.current;
    return {args.optional<double>("x_min").value_or(g.xlo), args.optional<double>("x_max").value_or(g.xhi),
            args.optional<double>("y_min").value_or(g.ylo), args.optional<double>("y_max").value_or(g.yhi)};
}

/* the order of a box's ends, once, with the argument names */
Result<> box_problem(const Command &command, const Box &box, const Place &place)
{
    for (const auto &[lo_name, lo, hi_name, hi] : {std::tuple{"x_min", box.x_min, "x_max", box.x_max}, std::tuple{"y_min", box.y_min, "y_max", box.y_max}})
        if (const auto bad = order_problem(lo_name, lo, hi_name, hi, true)) return fail(command.name, *bad, place);
    return {};
}

/* an operation on the active graph's view with the box the arguments give (a bound left out keeps the view's) */
using BoxOperation = void (*)(Session &, const Box &);

Result<std::string> change_box(Session &s, const Command &command, const Arguments &args, const Place &place, BoxOperation operate)
{
    const Result<int> index = graph_of_command(s, command, args, place);
    if (!index) return std::unexpected<Error>(index.error());
    const ActiveGraph active(s, *index);
    const Box box = box_of(s, args);
    if (const Result<> bad = box_problem(command, box, place); !bad) return std::unexpected<Error>(bad.error());
    operate(s, box);
    set_normal_scale(s);
    return bounds_result(s);
}

Result<std::string> set_bounds(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return change_box(s, command, args, place, [](Session &session, const Box &box) { update_view(session, box.x_min, box.x_max, box.y_min, box.y_max); });
}

Result<std::string> zoom(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return change_box(s, command, args, place, [](Session &session, const Box &box) { zoom_in(session, box.x_min, box.y_min, box.x_max, box.y_max); });
}

Result<std::string> zoom_out_window(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> index = graph_of_command(s, command, args, place);
    if (!index) return std::unexpected<Error>(index.error());
    const ActiveGraph active(s, *index);
    const GRAPH &g = *s.plot_windows.current;
    if (const auto factor = args.optional<double>("factor")) {
        const double cx = args.optional<double>("center_x").value_or((g.xlo + g.xhi) / 2);
        const double cy = args.optional<double>("center_y").value_or((g.ylo + g.yhi) / 2);
        zoom_out_by(s, cx, cy, *factor);
    } else {
        if (args.has("center_x")) return fail(command.name, "center_x and center_y go with factor, not with a box", place);
        const Box box = box_of(s, args);
        if (const Result<> bad = box_problem(command, box, place); !bad) return std::unexpected<Error>(bad.error());
        zoom_out(s, box.x_min, box.y_min, box.x_max, box.y_max);
    }
    set_normal_scale(s);
    return bounds_result(s);
}

Result<std::string> fit(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return change_view(s, command, args, place, [](Session &session, const Arguments &) { fit_window(session); });
}

Result<std::string> reset_default(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return change_view(s, command, args, place, [](Session &session, const Arguments &) { default_window(session); });
}

Result<std::string> scroll(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> index = graph_of_command(s, command, args, place);
    if (!index) return std::unexpected<Error>(index.error());
    const ActiveGraph active(s, *index);
    const GRAPH &g = *s.plot_windows.current;
    const double dx = args.get<double>("dx"), dy = args.get<double>("dy");
    update_view(s, g.xlo + dx, g.xhi + dx, g.ylo + dy, g.yhi + dy);
    set_normal_scale(s);
    return xpp::format("{{\"new_center\":[{},{}]}}", xpp::number((g.xlo + g.xhi) / 2), xpp::number((g.ylo + g.yhi) / 2));
}

/* [] (or the argument left out) is the window's own range */
Result<> read_range(const Command &command, const Arguments &args, const char *name, AxisRange &range, const Place &place)
{
    if (!args.has(name)) return {};
    const std::vector<double> values = args.list<double>(name);
    if (values.empty()) {
        range = AxisRange();
        return {};
    }
    if (values.size() != 2) return fail(command.name, xpp::format("{} is [low, high], or [] for the window's own range", name), place);
    if (const auto bad = order_problem(xpp::format("{} low", name), values[0], xpp::format("{} high", name), values[1], true))
        return fail(command.name, *bad, place);
    range = {true, values[0], values[1]};
    return {};
}

std::string range_result(const AxisRange &range)
{
    return range.set ? xpp::format("[{},{}]", xpp::number(range.lo), xpp::number(range.hi)) : "[]";
}

Result<std::string> set_display(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> index = graph_of_command(s, command, args, place);
    if (!index) return std::unexpected<Error>(index.error());
    PlotDisplay &display = s.plot_display[static_cast<std::size_t>(*index)];
    Zoom zoom = display.zoom;
    for (const auto &[name, range] : {std::pair<const char *, AxisRange *>{"x_range", &zoom.x}, {"y_range", &zoom.y}})
        if (const Result<> read = read_range(command, args, name, *range, place); !read) return std::unexpected<Error>(read.error());
    display.zoom = zoom;
    if (const auto runs = args.optional<bool>("show_earlier_runs")) display.show_runs = *runs;
    if (const auto keep = args.optional<bool>("keep_replaced_run")) display.freeze_runs = *keep;
    return xpp::format("{{\"x_range\":{},\"y_range\":{}}}", range_result(display.zoom.x), range_result(display.zoom.y));
}

Result<std::string> redraw(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = target_name(s, args);
    const auto target = find_window(s, name);
    if (!target) return unknown_window(command, name, place);
    if (target->kind == WindowKind::diagram) ui.redraw_diagram_for_client(s);
    else if (target->kind == WindowKind::graph) {
        const ActiveGraph active(s, target->index);
        ui.redraw_graph(s);
        plot_data_picture(s, 1); /* a data client draws the current data again */
    } else return fail(command.name, xpp::format("\"{}\" is the {} window: this applies to a graph or an AUTO view", name, window_kind_name(target->kind)), place);
    return window_result(name) + ",\"redrawn\":true}";
}

Result<std::string> clear(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> index = graph_of_command(s, command, args, place);
    if (!index) return std::unexpected<Error>(index.error());
    const ActiveGraph active(s, *index);
    clear_windows(s);
    return window_result(target_name(s, args)) + ",\"cleared\":true}";
}

Result<std::string> create_window(Session &s, const Command &, const Arguments &args, const Place &)
{
    create_a_pop(s, args.optional<std::string>(NAME_KEY).value_or(std::string{}));
    return window_result(s.plot_windows.current->name) + '}';
}

Result<std::string> rename_window(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = target_name(s, args), &new_name = args.get<std::string>(NAME_KEY);
    const auto target = find_window(s, name);
    if (!target) return unknown_window(command, name, place);
    if (const Result<std::string> refused = named_by_user(command, name, target->kind, place); !refused) return refused;
    if (target->kind == WindowKind::graph) s.plot_windows.graph[target->index].name = new_name;
    else s.auto_state.views[target->index].name = new_name;
    ui.window_renamed(s, name, new_name);
    if (s.selected_window == name) s.selected_window = new_name;
    std::string result = window_result(name) + ",\"new_name\":";
    json_append_string(result, new_name);
    return result + '}';
}

Result<std::string> select_window(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = target_name(s, args);
    const auto target = find_window(s, name);
    if (!target) return unknown_window(command, name, place);
    if (target->kind == WindowKind::graph) make_active(s, target->index, 1);
    else if (target->kind == WindowKind::diagram) auto_activate_view(s, target->index);
    else s.selected_window = name;
    ui.window_selected(s, name, window_kind_name(target->kind));
    return window_result(name) + '}';
}

Result<std::string> set_shown(Session &s, const Command &command, const Arguments &args, const Place &place, bool hidden)
{
    const std::string name = target_name(s, args);
    const auto target = find_window(s, name);
    if (!target) return unknown_window(command, name, place);
    set_window_hidden(s, *target, hidden);
    return window_result(name) + xpp::format(",\"shown\":{}}}", !hidden);
}

Result<std::string> hide_window(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return set_shown(s, command, args, place, true);
}

Result<std::string> show_window(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return set_shown(s, command, args, place, false);
}

Result<std::string> list_windows_result(Session &s, const Command &, const Arguments &, const Place &)
{
    return "{\"windows\":" + windows_json(s) + '}';
}

/* A graph's texts, arrows, markers and frozen curves cannot be made again:
   destroying a graph that holds some is confirmed first. An AUTO view's
   diagram is the session's (a new view shows it again), the animation and
   the array plot are made again from their file and the stored data, so
   none of them asks. The question is asked in every front end as the
   others are (the page's dialog, an `ask` for --server, the terminal for
   --silent); confirm true answers it beforehand. */
Result<> confirm_destroy(const Command &command, const Arguments &args, std::string_view name, const std::string &content,
                         const Place &place)
{
    if (content.empty() || args.optional<bool>(CONFIRM_KEY).value_or(false)) return {};
    if (!confirm(xpp::format("Destroy {}? It holds {}, which cannot be made again.", name, content)))
        return fail(command.name, xpp::format("\"{}\" was not destroyed: it holds {}, which was not confirmed (confirm true destroys it)", name, content), place);
    return {};
}

Result<std::string> destroy_window(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = target_name(s, args);
    const auto target = find_window(s, name);
    if (!target) return unknown_window(command, name, place);
    switch (target->kind) {
    case WindowKind::graph:
        if (target->index == 0) return fail(command.name, "The main graph stays open", place);
        if (const Result<> asked = confirm_destroy(command, args, name, window_content(s, s.plot_windows.graph[target->index].w), place); !asked)
            return std::unexpected<Error>(asked.error());
        make_active(s, target->index, 1);
        destroy_a_pop(s);
        break;
    case WindowKind::diagram:
        if (!auto_close_view(s, target->index)) {
            s.auto_state.bifur.exist = 0;
            s.auto_view = AutoView();
            for (AutoDiagramView &view : s.auto_state.views) {
                view.zoom = Zoom();
                view.hidden = false;
            }
            ui.auto_closed(s, name);
            s.selected_window = s.plot_windows.graph[s.plot_windows.active].name;
        } else s.selected_window = s.auto_state.views[s.auto_state.active_view].name;
        break;
    case WindowKind::browser:
        return fail(command.name, "The data browser stays open: it shows the session's stored data (Clear plot's Erase empties it)", place);
    case WindowKind::animation:
        s.animation.vcr.iexist = 0;
        s.animation.grab_flag = 0;
        s.tools_hidden.animation = false;
        ui.tool_window_closed(s, name, window_kind_name(target->kind));
        break;
    case WindowKind::array_plot:
        s.array_plot.plot.alive = 0;
        s.tools_hidden.array_plot = false;
        ui.tool_window_closed(s, name, window_kind_name(target->kind));
        break;
    }
    return window_result(name) + ",\"closed\":true}";
}

Result<std::string> destroy_all(Session &s, const Command &, const Arguments &, const Place &)
{
    const int count = s.plot_windows.count - 1;
    kill_all_pops(s);
    return xpp::format("{{\"closed_count\":{}}}", count);
}

Result<std::string> send_to_bottom(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = target_name(s, args);
    const auto target = find_window(s, name);
    if (!target) return unknown_window(command, name, place);
    if (target->kind != WindowKind::graph) return fail(command.name, "This setting applies to a graph", place);
    ui.lower_plot_window();
    return window_result(name) + '}';
}

Result<std::string> set_redraw_mode(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = target_name(s, args), &mode = args.get<std::string>("mode");
    const auto target = find_window(s, name);
    if (!target) return unknown_window(command, name, place);
    if (target->kind != WindowKind::graph) return fail(command.name, "This setting applies to a graph", place);
    s.plot_windows.graph[target->index].Nullrestore = mode == "auto";
    return "{\"redraw_mode\":\"" + mode + "\"}";
}

Result<std::string> set_simultaneous_plot(Session &s, const Command &, const Arguments &args, const Place &)
{
    const bool enabled = args.get<bool>("enabled");
    s.plot_windows.simul = enabled;
    return xpp::format("{{\"simultaneous_plot\":{}}}", enabled);
}

constexpr std::array CREATE_ARGUMENTS = {Argument{NAME_KEY, Type::new_name, false}};
constexpr std::array RENAME_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, true}, Argument{NAME_KEY, Type::new_name, true}};
constexpr std::array SELECT_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, true}};
constexpr std::array WINDOW_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false}};
constexpr std::array DESTROY_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false}, Argument{CONFIRM_KEY, Type::boolean, false}};
constexpr std::array BOX_NAMES = {"x_min", "x_max", "y_min", "y_max"};
constexpr std::array CENTER_NAMES = {"center_x", "center_y"};
constexpr std::array FACTOR_OR_BOX = {"factor", "x_min"};
constexpr std::array BOUNDS_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false}, Argument{"x_min", Type::real, false},
    Argument{"x_max", Type::real, false}, Argument{"y_min", Type::real, false}, Argument{"y_max", Type::real, false}};
constexpr std::array ZOOM_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false}, Argument{"x_min", Type::real, true},
    Argument{"x_max", Type::real, true}, Argument{"y_min", Type::real, true}, Argument{"y_max", Type::real, true}};
constexpr std::array ZOOM_OUT_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false},
    Argument{"factor", Type::real, false, MAX_TEXT_BYTES, {}, constant(1)}, Argument{"center_x", Type::real, false},
    Argument{"center_y", Type::real, false}, Argument{"x_min", Type::real, false}, Argument{"x_max", Type::real, false},
    Argument{"y_min", Type::real, false}, Argument{"y_max", Type::real, false}};
constexpr std::array ZOOM_OUT_GROUPS = {Group{GroupRule::exactly_one, FACTOR_OR_BOX}, Group{GroupRule::together, BOX_NAMES},
                                        Group{GroupRule::together, CENTER_NAMES}};
constexpr std::array BOX_GROUPS = {Group{GroupRule::together, BOX_NAMES}};
constexpr std::array SCROLL_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false}, Argument{"dx", Type::real, true},
                                         Argument{"dy", Type::real, true}};
constexpr std::array DISPLAY_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false},
    Argument{"x_range", Type::list, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real, 2},
    Argument{"y_range", Type::list, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real, 2},
    Argument{"show_earlier_runs", Type::boolean, false}, Argument{"keep_replaced_run", Type::boolean, false}};
constexpr std::array BOUNDS_RESULTS = {ResultField{"x_min", "real"}, ResultField{"x_max", "real"}, ResultField{"y_min", "real"},
                                       ResultField{"y_max", "real"}};
constexpr std::array SCROLL_RESULTS = {ResultField{"new_center", "list of 2 real"}};
constexpr std::array DISPLAY_RESULTS = {ResultField{"x_range", "list of 2 real, or []"}, ResultField{"y_range", "list of 2 real, or []"}};
constexpr std::array REDRAWN_RESULTS = {ResultField{WINDOW_KEY, "window"}, ResultField{"redrawn", "boolean"}};
constexpr std::array CLEARED_RESULTS = {ResultField{WINDOW_KEY, "window"}, ResultField{"cleared", "boolean"}};
constexpr std::array PLOT_EVENTS = {xpp::events::PLOT_SHOWN, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array REDRAW_CHOICES = {"auto", "manual"};
constexpr std::array MODE_ARGUMENTS = {Argument{WINDOW_KEY, Type::window, false},
                                       Argument{"mode", Type::choice, true, MAX_TEXT_BYTES, REDRAW_CHOICES}};
constexpr std::array SIMULTANEOUS_ARGUMENTS = {Argument{"enabled", Type::boolean, true}};
constexpr std::array WINDOW_RESULTS = {ResultField{WINDOW_KEY, "window"}};
constexpr std::array RENAME_RESULTS = {ResultField{WINDOW_KEY, "window"}, ResultField{"new_name", "text"}};
constexpr std::array CLOSE_RESULTS = {ResultField{WINDOW_KEY, "window"}, ResultField{"closed", "boolean"}};
constexpr std::array SHOWN_RESULTS = {ResultField{WINDOW_KEY, "window"}, ResultField{"shown", "boolean"}};
constexpr std::array LIST_RESULTS = {ResultField{"windows", "list of {name, kind, shown}"}};
constexpr std::array CLOSE_ALL_RESULTS = {ResultField{"closed_count", "integer"}};
constexpr std::array MODE_RESULTS = {ResultField{"redraw_mode", "choice"}};
constexpr std::array SIMULTANEOUS_RESULTS = {ResultField{"simultaneous_plot", "boolean"}};
constexpr std::array WINDOW_EVENTS = {xpp::events::WINDOW_CREATED, xpp::events::WINDOW_SELECTED, xpp::events::WINDOW_DESTROYED, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array RENAME_EVENTS = {xpp::events::WINDOW_RENAMED, xpp::events::WINDOW_CREATED, xpp::events::WINDOW_SELECTED, xpp::events::WINDOW_DESTROYED, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array STATE_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};

/* XPPAUT's imported button sequences MC, MK, MD, MB, MA, MM, MS, by button number */
constexpr std::array BUTTON_COMMANDS = {"window.create", "window.destroy_all", "window.destroy", "window.send_to_bottom",
                                        "window.set_redraw_mode", "window.set_redraw_mode", "window.set_simultaneous_plot"};
constexpr int BUTTON_REDRAW_AUTO = 4, BUTTON_REDRAW_MANUAL = 5, BUTTON_SIMULTANEOUS = 6;

constexpr std::array ROWS = {
    Command{"window.create", CREATE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/New graph",
            "Create a graph copied from the active graph.", WINDOW_RESULTS, WINDOW_EVENTS, true, create_window},
    Command{"window.rename", RENAME_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Window/Rename",
            "Rename an open graph or diagram view.", RENAME_RESULTS, RENAME_EVENTS, true, rename_window},
    Command{"window.select", SELECT_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Window/Select",
            "Bring an open window to front and select it.", WINDOW_RESULTS, WINDOW_EVENTS, true, select_window},
    Command{"window.hide", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Window/Hide",
            "Hide a window: it keeps everything it holds and comes back with window.show.", SHOWN_RESULTS, STATE_EVENTS, true, hide_window},
    Command{"window.show", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Window/Show",
            "Show a hidden window exactly as it was.", SHOWN_RESULTS, STATE_EVENTS, true, show_window},
    Command{"window.destroy", DESTROY_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Make window/Destroy",
            "Destroy a window and what it holds (a graph that holds texts, arrows, markers or frozen curves needs confirm true, or the page's answer); the main graph and the data browser stay; destroying an AUTO view keeps the diagram.",
            CLOSE_RESULTS, WINDOW_EVENTS, true, destroy_window},
    Command{"window.list", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Window/List",
            "List every window with its name, kind and whether it is shown.", LIST_RESULTS, STATE_EVENTS, true, list_windows_result},
    Command{"window.destroy_all", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Make window/Kill all",
            "Close every extra graph.", CLOSE_ALL_RESULTS, WINDOW_EVENTS, true, destroy_all},
    Command{"window.set_bounds", BOUNDS_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Window/Window",
            "Set the view's bounds; a bound left out keeps the view's.", BOUNDS_RESULTS, PLOT_EVENTS, true, set_bounds},
    Command{"window.zoom", ZOOM_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Window/Zoom In",
            "Zoom in: the view becomes the box.", BOUNDS_RESULTS, PLOT_EVENTS, true, zoom, BOX_GROUPS},
    Command{"window.zoom_out", ZOOM_OUT_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Window/Zoom Out",
            "Zoom out: the view grows by factor about a center (the view's own), or fills the box.", BOUNDS_RESULTS, PLOT_EVENTS, true,
            zoom_out_window, ZOOM_OUT_GROUPS},
    Command{"window.fit", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Window/Fit",
            "Fit the view to the data.", BOUNDS_RESULTS, PLOT_EVENTS, true, fit},
    Command{"window.reset_default", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Window/Default",
            "Give the view its default bounds.", BOUNDS_RESULTS, PLOT_EVENTS, true, reset_default},
    Command{"window.scroll", SCROLL_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Window/Scroll",
            "Move the view by dx and dy, in plot coordinates.", SCROLL_RESULTS, PLOT_EVENTS, true, scroll},
    Command{"window.set_display", DISPLAY_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Window/Set display",
            "Set what the page shows of a graph: the x and y ranges ([] for the window's own), the earlier runs, keeping a replaced run.",
            DISPLAY_RESULTS, PLOT_EVENTS, true, set_display},
    Command{"window.redraw", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Redraw plot",
            "Draw a graph again from the stored data (and its nullclines, texts and frozen curves), or send an AUTO view's diagram again.", REDRAWN_RESULTS, PLOT_EVENTS, false, redraw},
    Command{"window.clear", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Clear plot",
            "Clear a graph: its picture and earlier runs go until the next run.", CLEARED_RESULTS, PLOT_EVENTS, true, clear},
    Command{"window.send_to_bottom", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Make window/Bottom",
            "Send a graph to the bottom.", WINDOW_RESULTS, STATE_EVENTS, true, send_to_bottom},
    Command{"window.set_redraw_mode", MODE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Make window/Auto or Manual",
            "Choose automatic or manual graph redrawing.", MODE_RESULTS, STATE_EVENTS, true, set_redraw_mode},
    Command{"window.set_simultaneous_plot", SIMULTANEOUS_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Plot/Make window/SimPlot",
            "Draw on every open graph when enabled.", SIMULTANEOUS_RESULTS, STATE_EVENTS, true, set_simultaneous_plot}};

} // namespace

Result<int> graph_of_command(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = target_name(s, args);
    const auto target = find_window(s, name);
    if (!target) return std::unexpected<Error>(unknown_window(command, name, place).error());
    if (target->kind != WindowKind::graph) return fail(command.name, xpp::format("\"{}\" is not a graph", name), place);
    return target->index;
}

std::span<const Command> window_commands() { return ROWS; }

Result<> window_button(Session &s, int button, const Place &place)
{
    if (button < 0 || static_cast<std::size_t>(button) >= BUTTON_COMMANDS.size())
        return fail("window button", "Unknown imported window button", place);
    std::string line = "{\"cmd\":";
    json_append_string(line, BUTTON_COMMANDS[static_cast<std::size_t>(button)]);
    if (button == BUTTON_REDRAW_AUTO) line += ",\"mode\":\"auto\"";
    else if (button == BUTTON_REDRAW_MANUAL) line += ",\"mode\":\"manual\"";
    if (button == BUTTON_SIMULTANEOUS) line += s.plot_windows.simul ? ",\"enabled\":false" : ",\"enabled\":true";
    line += '}';
    const Result<std::string> result = run(s, line.c_str(), place, false);
    if (!result) return std::unexpected<Error>(result.error());
    return {};
}

} // namespace xpp::command
