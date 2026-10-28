/* The window resource's registry rows (W353, moved here at W530): create,
   rename, select, hide, show, destroy and list the windows (graphs, AUTO
   views, the data browser, the animation and the array plot: W515), the
   redraw mode, SimPlot.
   Each handler gets its arguments validated and typed (command_def.h). */
#include "command_def.h"
#include "command_registry.h"
#include "json_error.h"
#include "many_pops.h"
#include "auto_nox.h"
#include "session.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include <algorithm>
#include <array>

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
    if (const Result<std::string> refused = named_by_user(command, name, target->kind, place); !refused) return refused;
    if (target->kind == WindowKind::graph) make_active(s, target->index, 1);
    else auto_activate_view(s, target->index);
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
constexpr std::array WINDOW_EVENTS = {"window", "state", "done"};
constexpr std::array RENAME_EVENTS = {"window.renamed", "window", "state", "done"};
constexpr std::array STATE_EVENTS = {"state", "done"};

/* XPPAUT's imported button sequences MC, MK, MD, MB, MA, MM, MS, by button number */
constexpr std::array BUTTON_COMMANDS = {"window.create", "window.destroy_all", "window.destroy", "window.send_to_bottom",
                                        "window.set_redraw_mode", "window.set_redraw_mode", "window.set_simultaneous_plot"};
constexpr int BUTTON_REDRAW_AUTO = 4, BUTTON_REDRAW_MANUAL = 5, BUTTON_SIMULTANEOUS = 6;

constexpr std::array ROWS = {
    Command{"window.create", CREATE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Make window/Create", "MC",
            "Create a graph copied from the active graph.", WINDOW_RESULTS, WINDOW_EVENTS, true, create_window},
    Command{"window.rename", RENAME_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "", "",
            "Rename an open graph or diagram view.", RENAME_RESULTS, RENAME_EVENTS, true, rename_window},
    Command{"window.select", SELECT_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "", "",
            "Select an open graph or diagram view.", WINDOW_RESULTS, STATE_EVENTS, true, select_window},
    Command{"window.hide", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "", "",
            "Hide a window: it keeps everything it holds and comes back with window.show.", SHOWN_RESULTS, STATE_EVENTS, true, hide_window},
    Command{"window.show", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "", "",
            "Show a hidden window exactly as it was.", SHOWN_RESULTS, STATE_EVENTS, true, show_window},
    Command{"window.destroy", DESTROY_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Make window/Destroy", "MD",
            "Destroy a window and what it holds (a graph that holds texts, arrows, markers or frozen curves needs confirm true, or the page's answer); the main graph and the data browser stay; destroying an AUTO view keeps the diagram.",
            CLOSE_RESULTS, WINDOW_EVENTS, true, destroy_window},
    Command{"window.list", {}, XPP_KIND_VIEW, CommandCategory::Plot, "", "",
            "List every window with its name, kind and whether it is shown.", LIST_RESULTS, STATE_EVENTS, true, list_windows_result},
    Command{"window.destroy_all", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Make window/Kill all", "MK",
            "Close every extra graph.", CLOSE_ALL_RESULTS, WINDOW_EVENTS, true, destroy_all},
    Command{"window.send_to_bottom", WINDOW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Make window/Bottom", "MB",
            "Send a graph to the bottom.", WINDOW_RESULTS, STATE_EVENTS, true, send_to_bottom},
    Command{"window.set_redraw_mode", MODE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Make window/Auto or Manual", "MA/MM",
            "Choose automatic or manual graph redrawing.", MODE_RESULTS, STATE_EVENTS, true, set_redraw_mode},
    Command{"window.set_simultaneous_plot", SIMULTANEOUS_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Make window/SimPlot", "MS",
            "Draw on every open graph when enabled.", SIMULTANEOUS_RESULTS, STATE_EVENTS, true, set_simultaneous_plot}};

} // namespace

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
