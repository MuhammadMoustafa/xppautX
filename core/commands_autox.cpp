/* The auto resource's registry rows (W516): open autoX, run it (autox.run, with W306's kinds, starts and settings),
   grab a stored point, set its Numerics, axes, Mark values and threads, move and zoom the diagram's views, and the
   File menu's exports and imports. Each handler gets its arguments validated and typed (command_def.h) and calls the
   core function the autoX window's keys call after their questions, so a script, the page and the keys do one thing.
   Closing a view, selecting one and the autoX window's own close are window.close and window.select (W353). */
#include "command_def.h"
#include "command_registry.h"
#include "autox_run.h"
#include "autox_settings.h"
#include "diagram.h"
#include "image_format.h"
#include "integrate.h"
#include "json_error.h"
#include "many_pops.h"
#include "session.h"
#include "xpp_cpu.h"
#include "xpp_io.h"
#include "xpp_prefs.h"
#include "xpp_log.h"
#include "xpp_session.h"
#include "xpp_ui.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace xpp::command {
namespace {

constexpr const char *VIEW_KEY = "view";
constexpr const char *WINDOW_KEY = "window";
constexpr const char *FILE_KEY = "file";
constexpr const char *LABEL_KEY = "label";

/* ---- what every handler asks of the session ---- */

Result<std::string> need_open(const Session &s, const Command &command, const Place &place)
{
    if (!s.autox_state.bifur.exist)
        return fail(command.name, "autoX is not open: open it first (autox.open, File > autoX continuation)", place);
    return std::string();
}

/* the diagram view an argument names (the active one without it), without changing the active view */
Result<int> target_view(Session &s, const Command &command, const Arguments &args, const Place &place, const char *key = VIEW_KEY)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return std::unexpected<Error>(open.error());
    const std::optional<std::string> name = args.optional<std::string>(key);
    if (!name) return s.autox_state.active_view;
    const auto target = find_window(s, *name);
    if (!target || target->kind != WindowKind::diagram)
        return fail(command.name, xpp::format("\"{}\" is not an autoX view (a diagram window)", *name), place);
    return target->index;
}

/* the active view's ranges as the result of a view command */
std::string view_result(const Session &s)
{
    const AUTOAX &a = s.autox_state.axes();
    return xpp::format("{{\"window\":{},\"x_range\":[{},{}],\"y_range\":[{},{}]}}",
                       [&] { std::string name; json_append_string(name, s.autox_state.views[static_cast<std::size_t>(s.autox_state.active_view)].name); return name; }(),
                       xpp::number(a.xmin), xpp::number(a.xmax), xpp::number(a.ymin), xpp::number(a.ymax));
}

/* the same Error as the setting's own refusal says it, as this command's */
Error as_command_error(const Command &command, Error error, const Place &place)
{
    error.where = command.name;
    if (error.place.line == 0 && error.place.file.empty()) error.place = place;
    return error;
}

/* ---- the Numerics and the axes ---- */

/* the Numerics an autox.run or autox.configure_numerics line gave, into set */
Result<> read_numerics(const Command &command, const Arguments &args, const Place &place, AutoxSettingsSet &set)
{
    for (int i = 0; i < AUTOX_NUM_N; ++i) {
        const char *key = autox_settings_num_key(i);
        if (!args.has(key)) continue;
        switch (autox_settings_num_shape(i)) {
        case AutoxFieldShape::whole: set.num[static_cast<std::size_t>(i)] = static_cast<double>(args.get<long long>(key)); break;
        case AutoxFieldShape::real: set.num[static_cast<std::size_t>(i)] = args.get<double>(key); break;
        case AutoxFieldShape::boolean: set.num[static_cast<std::size_t>(i)] = args.get<bool>(key) ? 1 : 0; break;
        case AutoxFieldShape::choice: {
            const Result<double> level = autox_settings_num_parse(i, args.get<std::string>(key));
            if (!level) return std::unexpected<Error>(as_command_error(command, level.error(), place));
            set.num[static_cast<std::size_t>(i)] = *level;
            break;
        }
        }
        set.has_num[static_cast<std::size_t>(i)] = 1;
    }
    return {};
}

/* the settings a set changes applied, all or nothing, or why not */
Result<> apply_settings(Session &s, const Command &command, const AutoxSettingsSet &set, const Place &place)
{
    if (const Result<> checked = autox_settings_check(s, set, -1); !checked) return std::unexpected<Error>(as_command_error(command, checked.error(), place));
    std::string why;
    if (autox_settings_apply(s, set, why) != 0) return fail(command.name, why, place);
    return {};
}

Result<std::string> configure_numerics(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    AutoxSettingsSet set;
    if (const Result<> read = read_numerics(command, args, place, set); !read) return std::unexpected<Error>(read.error());
    for (const auto &[name, enabled] : args.map<bool>("special_points")) {
        const auto at = std::find(autox_codim_keys.begin(), autox_codim_keys.end(), name);
        if (at == autox_codim_keys.end()) return fail(command.name, xpp::format("special_points names one of {}, not \"{}\"", autox_codim_key_list(), name), place);
        set.special_points[static_cast<std::size_t>(at - autox_codim_keys.begin())] = enabled;
    }
    if (const Result<> done = apply_settings(s, command, set, place); !done) return std::unexpected<Error>(done.error());
    return "{\"settings_updated\":true}";
}

Result<std::string> configure_axes(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    AutoxSettingsSet set;
    if (const auto plot = args.optional<std::string>("plot_type")) set.plot = autox_projection_from_name(*plot);
    set.var = args.optional<std::string>("variable").value_or(std::string());
    set.par1 = args.optional<std::string>("parameter1").value_or(std::string());
    set.par2 = args.optional<std::string>("parameter2").value_or(std::string());
    constexpr std::array<const char *, 4> ranges = {"x_min", "x_max", "y_min", "y_max"};
    for (std::size_t i = 0; i < ranges.size(); ++i)
        if (const auto value = args.optional<double>(ranges[i])) {
            set.range[i] = *value;
            set.has_range[i] = 1;
        }
    set.fit = args.optional<bool>("fit").value_or(false) ? 1 : 0;
    if (const auto name = args.optional<std::string>(VIEW_KEY)) {
        const auto target = find_window(s, *name);
        if (!target || target->kind != WindowKind::diagram)
            return fail(command.name, xpp::format("\"{}\" is not an autoX view (a diagram window)", *name), place);
        set.view = target->index;
    }
    if (const Result<> done = apply_settings(s, command, set, place); !done) return std::unexpected<Error>(done.error());
    return "{\"plot_configured\":true}";
}

Result<std::string> configure_marks(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    AutoxSettingsSet set;
    set.mark_name = args.list<std::string>("parameters");
    set.mark_value = args.list<double>("values");
    if (set.mark_name.size() != set.mark_value.size())
        return fail(command.name, xpp::format("Mark values: {} parameters but {} values", set.mark_name.size(), set.mark_value.size()), place);
    set.nmarks = static_cast<int>(set.mark_name.size());
    if (const Result<> done = apply_settings(s, command, set, place); !done) return std::unexpected<Error>(done.error());
    return xpp::format("{{\"marks_count\":{}}}", set.mark_name.size());
}

/* the preferences as they stand: the thread count (and whether it is automatic), what the log shows and the file it goes to */
Result<std::string> preferences_now(const Session &s)
{
    const Result<prefs::Preferences> kept = prefs::load_prefs();
    const bool automatic = s.threads == prefs::THREADS_AUTO;
    return ResultObject().integer("threads", automatic ? AutoxParallel::automatic_threads() : s.threads).boolean("automatic", automatic)
        .text("log_level", log_level_names[static_cast<std::size_t>(kept ? kept->log_level : XPP_LOG_WARN)]).text("log_file", kept ? kept->log_file : std::string()).done();
}

Result<std::string> get_preferences(Session &s, const Command &, const Arguments &, const Place &) { return preferences_now(s); }

/* the program's preferences: the thread count autoX and sweeps use, and what the log shows and the file it goes to (kept in
   preferences.odex; the log applies from the next start, and --verbose and --debug win over it) */
Result<std::string> configure_preferences(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const bool threads = args.has("threads") || args.has("automatic"), log = args.has("log_level") || args.has("log_file");
    if (!threads && !log) return fail(command.name, "Give the thread count, the log level or the log file", place);
    if (log && files::serving_reads()) return fail(command.name, "a recording's replay leaves the user's preferences alone", place);
    if (threads) {
        AutoxSettingsSet set;
        if (const auto count = args.optional<long long>("threads")) set.threads = static_cast<unsigned>(*count);
        else if (args.get<bool>("automatic")) set.threads = prefs::THREADS_AUTO;
        else return fail(command.name, "automatic is true; give threads for a count", place);
        if (const Result<> done = apply_settings(s, command, set, place); !done) return std::unexpected<Error>(done.error());
    }
    if (log) {
        const Result<> saved = prefs::update_prefs([&](prefs::Preferences &p) {
            if (const auto level = args.optional<std::string>("log_level")) p.log_level = static_cast<XppLogLevel>(choice_index(log_level_names, *level));
            if (const auto file = args.optional<std::string>("log_file")) p.log_file = *file;
        });
        if (!saved) return std::unexpected<Error>(saved.error());
    }
    return preferences_now(s);
}

/* ---- running autoX ---- */

Result<std::string> open_autox(Session &s, const Command &, const Arguments &, const Place &)
{
    do_autox_win(s);
    return xpp::format("{{\"open\":{}}}", s.autox_state.bifur.exist != 0);
}

Result<std::string> run_autox(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    AutoxRunRequest request;
    const std::string &kind = args.get<std::string>("kind");
    request.kind = *autox_run_kind_from_name(kind);
    request.switch_branch = args.optional<bool>("switch").value_or(false);
    const DIAGRAM *point=nullptr;
    if(const auto label=args.optional<long long>(LABEL_KEY)){
        point=diagram_of_label(s,static_cast<int>(*label));
        if(!point)return fail(command.name,xpp::format("No point is labelled {}",*label),place);
    }else if(const auto type=args.optional<std::string>("type")){
        const int index=static_cast<int>(args.get<long long>("index"));
        point=autox_point_by_type(s,*autox_point_kind_from_name(*type),index);
        if(!point)return fail(command.name,xpp::format("No {} point number {}",*type,index),place);
    }
    request.from_point=point!=nullptr;
    request.replace_diagram = args.optional<bool>("replace_diagram").value_or(false);
    if (!request.from_point && request.kind == AutoxRunKind::equilibrium && diagram_count(s.diagram) > 1 && !args.has("replace_diagram"))
        return fail(command.name, xpp::format("The diagram holds {} points: say replace_diagram true to begin it again, false to add this run to it", diagram_count(s.diagram)), place);
    if (args.has("unstable_dimension") || args.has("stable_dimension") || args.has("left_equilibrium") || args.has("right_equilibrium")) {
        HomoclinicStart start{s.autox_state.homo_equilibria.nunstab, s.autox_state.homo_equilibria.nstab, s.autox_state.homo_l, s.autox_state.homo_r};
        if (const auto v = args.optional<long long>("unstable_dimension")) start.unstable = static_cast<int>(*v);
        if (const auto v = args.optional<long long>("stable_dimension")) start.stable = static_cast<int>(*v);
        if (args.has("left_equilibrium")) start.left = args.list<double>("left_equilibrium");
        if (args.has("right_equilibrium")) start.right = args.list<double>("right_equilibrium");
        request.homoclinic = start;
    }
    /* the settings given hold for this run only: the others and these afterwards are the session's */
    AutoxSettingsSet given;
    if (const Result<> read = read_numerics(command, args, place, given); !read) return std::unexpected<Error>(read.error());
    if(const auto checked=autox_settings_check(s,given,-1); !checked)return std::unexpected<Error>(as_command_error(command,checked.error(),place));
    const std::optional<GRABPT> proposed=point?std::optional<GRABPT>(autox_point_data(*point)):std::nullopt;
    if(const auto checked=autox_run_check(s,request,proposed?&*proposed:nullptr,args.has("decimals")); !checked)
        return std::unexpected<Error>(as_command_error(command,checked.error(),place));
    if(args.has("decimals")){
        const SteadyStateSettings settle{static_cast<int>(args.get<long long>("decimals")),args.get<double>("hold"),args.get<double>("maximum")};
        if(const auto settled=run_to_steady_state(s,settle); !settled)return std::unexpected<Error>(settled.error());
        set_initial_data(s,std::span<const double>(s.data_store.current.data(),static_cast<std::size_t>(s.model().node)));
    }
    if(point)autox_grab_label(s,point->lab);
    const AutoxSettingsSet before = autox_settings_now(s);
    const bool any = std::find(given.has_num.begin(), given.has_num.end(), 1) != given.has_num.end();
    if (any)
        if (const Result<> done = apply_settings(s, command, given, place); !done) return std::unexpected<Error>(done.error());
    const int before_points = s.autox_state.diag_flag ? diagram_count(s.diagram) : 0;
    const Result<> ran = autox_run_start(s, request);
    if (any) {
        AutoxSettingsSet restore;
        for (std::size_t i = 0; i < given.has_num.size(); ++i)
            if (given.has_num[i]) {
                restore.has_num[i] = 1;
                restore.num[i] = before.num[i];
            }
        std::string why;
        autox_settings_apply(s, restore, why);
    }
    if (!ran) return std::unexpected<Error>(as_command_error(command, ran.error(), place));
    const int first = request.replace_diagram && !request.from_point ? 0 : before_points;
    const int after_points = s.autox_state.diag_flag ? diagram_count(s.diagram) : 0;
    std::vector<int> branches;
    for (int i = first; i < after_points; ++i) {
        const int branch = std::abs(s.diagram.points[static_cast<std::size_t>(i)].ibr);
        if (std::find(branches.begin(), branches.end(), branch) == branches.end()) branches.push_back(branch);
    }
    return xpp::format("{{\"points_computed\":{},\"branches_tracked\":{}}}", std::max(0, after_points - first), branches.size());
}

Result<std::string> grab_point(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    if (const auto label = args.optional<long long>(LABEL_KEY)) {
        if (!autox_grab_label(s, static_cast<int>(*label))) return fail(command.name, xpp::format("Grab: no point labelled {}", *label), place);
    } else if (args.has("type")) {
        const std::string &type = args.get<std::string>("type");
        const long long index = args.get<long long>("index");
        const auto kind = autox_point_kind_from_name(type);
        if (!kind) return fail(command.name, xpp::format("Grab: \"{}\" is not a point type", type), place);
        if (!autox_grab_type_index(s, *kind, static_cast<int>(index))) return fail(command.name, xpp::format("Grab: no {} point number {}", type, index), place);
    }
    if (!args.has(LABEL_KEY) && !args.has("type")) autox_grab(s);
    const GRABPT &g = s.autox_state.grabpt;
    std::string result = xpp::format("{{\"grabbed\":true,\"label\":{},\"type\":", g.lab);
    json_append_string(result, autox_point_kind_name(g.itp.kind));
    return result + xpp::format(",\"branch\":{},\"point\":{}}}", std::abs(g.ibr), 0);
}

Result<std::string> clear_grab(Session &s, const Command &command, const Arguments &, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    s.autox_state.grabpt.flag = 0;
    return "{\"cleared\":true}";
}

/* ---- the diagram's views ---- */

Result<std::string> hide_earlier(Session &s, const Command &command, const Arguments &, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    autox_hide_earlier(s);
    redraw_diagram(s); /* clearing blanks the picture: its points are sent again, the earlier ones marked */
    return xpp::format("{{\"hidden_points\":{}}}", s.autox_view.earlier);
}

Result<std::string> redraw(Session &s, const Command &command, const Arguments &, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    redraw_diagram(s);
    return "{\"redrawn\":true}";
}

Result<std::string> pick_point(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> view = target_view(s, command, args, place);
    if (!view) return std::unexpected<Error>(view.error());
    autox_activate_view(s, *view);
    const double x = args.get<double>("x"), y = args.get<double>("y");
    autox_point_xy(s, x, y);
    return xpp::format("{{\"x\":{},\"y\":{}}}", xpp::number(x), xpp::number(y));
}

/* a zoom range: nothing (the view shows the whole), or low then high */
Result<std::optional<AxisRange>> zoom_range(const Command &command, const Arguments &args, const char *key, const Place &place)
{
    if (!args.has(key)) return std::optional<AxisRange>{};
    const std::vector<double> bounds = args.list<double>(key);
    AxisRange range;
    if (bounds.empty()) return std::optional<AxisRange>{AxisRange{}};
    if (bounds.size() != 2 || !(bounds[0] < bounds[1]))
        return fail(command.name, xpp::format("{} is [low, high] with low below high, or [] for the whole range", key), place);
    range.set = true;
    range.lo = bounds[0];
    range.hi = bounds[1];
    return std::optional<AxisRange>{range};
}

Result<std::string> set_display(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<std::optional<AxisRange>> x = zoom_range(command, args, "x_range", place), y = zoom_range(command, args, "y_range", place);
    if (!x) return std::unexpected<Error>(x.error());
    if (!y) return std::unexpected<Error>(y.error());
    const Result<int> view = target_view(s, command, args, place);
    if (!view) return std::unexpected<Error>(view.error());
    autox_activate_view(s, *view);
    Zoom zoom = s.autox_state.views[static_cast<std::size_t>(*view)].zoom;
    if (*x) zoom.x = **x;
    if (*y) zoom.y = **y;
    s.autox_state.views[static_cast<std::size_t>(*view)].zoom = zoom;
    if (const auto show = args.optional<bool>("show_earlier")) s.autox_view.show_earlier = *show && s.autox_view.earlier > 0;
    const AutoxDiagramView &shown = s.autox_state.views[static_cast<std::size_t>(*view)];
    const auto range_text = [](const AxisRange &r) { return r.set ? xpp::format("[{},{}]", xpp::number(r.lo), xpp::number(r.hi)) : std::string("[]"); };
    return xpp::format("{{\"x_range\":{},\"y_range\":{}}}", range_text(shown.zoom.x), range_text(shown.zoom.y));
}

Result<std::string> new_view(Session &s, const Command &, const Arguments &, const Place &)
{
    if (!s.autox_state.bifur.exist) do_autox_win(s);
    else autox_new_view(s);
    return view_result(s);
}

Result<std::string> use_last_plot(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> view = target_view(s, command, args, place);
    if (!view) return std::unexpected<Error>(view.error());
    autox_activate_view(s, *view);
    const std::string &which = args.get<std::string>("parameters");
    load_last_plot(s, which == "one" ? LastPlot::OneParameter : LastPlot::TwoParameter);
    redraw_diagram(s);
    return xpp::format("{{\"plot\":\"{}\"}}", which);
}

Result<std::string> scroll_view(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<int> view = target_view(s, command, args, place);
    if (!view) return std::unexpected<Error>(view.error());
    autox_activate_view(s, *view);
    autox_scroll_by(s, args.get<double>("dx"), args.get<double>("dy"));
    redraw_diagram(s);
    return view_result(s);
}

/* a change of the target view, drawn again: `change` runs on the target */
template <typename Change>
Result<std::string> change_view(Session &s, const Command &command, const Arguments &args, const Place &place, Change change)
{
    const Result<int> view = target_view(s, command, args, place);
    if (!view) return std::unexpected<Error>(view.error());
    autox_activate_view(s, *view);
    change();
    redraw_diagram(s);
    return view_result(s);
}

Result<std::string> default_view(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return change_view(s, command, args, place, [&] { autox_default(s); });
}

Result<std::string> fit_view(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return change_view(s, command, args, place, [&] { autox_fit(s); });
}

/* a box x_min..x_max by y_min..y_max, both ordered */
Result<> box_of(const Command &command, const Arguments &args, const Place &place, std::array<double, 4> &box)
{
    box = {args.get<double>("x_min"), args.get<double>("x_max"), args.get<double>("y_min"), args.get<double>("y_max")};
    if (!(box[0] < box[1])) return fail(command.name, xpp::format("x_min ({}) must be below x_max ({})", xpp::number(box[0]), xpp::number(box[1])), place);
    if (!(box[2] < box[3])) return fail(command.name, xpp::format("y_min ({}) must be below y_max ({})", xpp::number(box[2]), xpp::number(box[3])), place);
    return {};
}

/* a zoom to the box given: `zoom` runs on the target view with the ordered box */
template <typename Zoom>
Result<std::string> zoom_box(Session &s, const Command &command, const Arguments &args, const Place &place, Zoom zoom)
{
    std::array<double, 4> box;
    if (const Result<> ok = box_of(command, args, place, box); !ok) return std::unexpected<Error>(ok.error());
    return change_view(s, command, args, place, [&] { zoom(box); });
}

Result<std::string> zoom_in(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return zoom_box(s, command, args, place, [&](const std::array<double, 4> &b) { autox_zoom_to(s, b[0], b[1], b[2], b[3]); });
}

Result<std::string> zoom_out(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return zoom_box(s, command, args, place, [&](const std::array<double, 4> &b) { autox_zoom_out_to(s, b[0], b[1], b[2], b[3]); });
}

/* ---- the File menu ---- */

/* the file argument as the export's optional name: none asks the dialog's question, a command never does (the default name) */
std::optional<std::string> file_of(const Session &s, const Arguments &args, std::string_view what, std::string_view extension)
{
    return args.optional<std::string>(FILE_KEY).value_or(diagram_default_file(s, what, extension));
}

std::string file_result(const std::string &file, const char *count_name = nullptr, int count = 0)
{
    std::string out = "{\"file\":";
    json_append_string(out, file);
    if (count_name) out += xpp::format(",\"{}\":{}", count_name, count);
    return out + '}';
}

/* a file export of the open diagram: `write` writes the file given (the default name without one), `count_name` and `count` go to the result */
template <typename Write>
Result<std::string> export_file(Session &s, const Command &command, const Arguments &args, const Place &place, std::string_view what,
                                std::string_view extension, Write write, const char *count_name, int count)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    const std::string file = *file_of(s, args, what, extension);
    if (const Result<> done = write(file); !done) return std::unexpected<Error>(as_command_error(command, done.error(), place));
    return file_result(file, count_name, count);
}

Result<std::string> export_points(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_file(s, command, args, place, "diagram", ".dat", [&](const std::string &f) { return write_pts(s, f); }, "points_written", diagram_count(s.diagram));
}

Result<std::string> export_all_info(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_file(s, command, args, place, "allinfo", ".dat", [&](const std::string &f) { return write_info_out(s, f); }, "written", 1);
}

Result<std::string> export_initial_data(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_file(s, command, args, place, "initdata", ".dat", [&](const std::string &f) { return write_init_data_file(s, f); }, "written", 1);
}

Result<std::string> export_csv(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_file(s, command, args, place, "diagram", ".csv", [&](const std::string &f) { return export_autox_csv(s, f); }, "points_written", diagram_count(s.diagram));
}

Result<std::string> export_picture(Session &s, const Command &command, const Arguments &args, const Place &place, int format, const char *extension)
{
    const Result<int> view = target_view(s, command, args, place, WINDOW_KEY);
    if (!view) return std::unexpected<Error>(view.error());
    const std::string file = *file_of(s, args, "diagram", extension);
    if (const Result<> done = export_autox_picture(s, format, file, *view); !done) return std::unexpected<Error>(as_command_error(command, done.error(), place));
    return file_result(file);
}

Result<std::string> export_pdf(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_picture(s, command, args, place, IMAGE_FORMAT_PDF, ".pdf");
}

Result<std::string> export_svg(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return export_picture(s, command, args, place, IMAGE_FORMAT_SVG, ".svg");
}

Result<std::string> import_diagram(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    const std::string &file = args.get<std::string>(FILE_KEY);
    if (const Result<> done = import_autox(s, file); !done) return std::unexpected<Error>(as_command_error(command, done.error(), place));
    return file_result(file, "points_imported", diagram_count(s.diagram));
}

Result<std::string> reset_diagram(Session &s, const Command &command, const Arguments &, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    const bool reset = yes_reset_autox(s) != 0;
    redraw_diagram(s);
    return xpp::format("{{\"reset\":{}}}", reset);
}

Result<std::string> set_auto_redraw(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    s.autox_state.redraw = args.get<bool>("enabled");
    return xpp::format("{{\"auto_redraw\":{}}}", s.autox_state.redraw);
}

/* the stretch of a branch the commands below work on: marked as the grab's S and E keys mark it */
Result<> mark_stretch(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (diagram_count(s.diagram) < 2) return fail(command.name, "The diagram has no points", place);
    const int branch = static_cast<int>(args.get<long long>("branch"));
    const int first = static_cast<int>(args.get<long long>("first_point")), last = static_cast<int>(args.get<long long>("last_point"));
    if (!diagram_find(s.diagram, branch, first) || !diagram_find(s.diagram, branch, last))
        return fail(command.name, xpp::format("Branch {} has no points {} and {}", branch, first, last), place);
    AutoxDiagramMark &mark = s.autox_state.diagram_mark;
    mark.state = 2;
    mark.start_branch = mark.end_branch = branch;
    mark.start_point = first;
    mark.end_point = last;
    return {};
}

Result<std::string> range_branch(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    if (const Result<> marked = mark_stretch(s, command, args, place); !marked) return std::unexpected<Error>(marked.error());
    do_autox_range(s);
    return xpp::format("{{\"runs_completed\":{}}}", std::abs(std::abs(s.autox_state.diagram_mark.end_point) - std::abs(s.autox_state.diagram_mark.start_point)) + 1);
}

Result<std::string> load_branch_into_browser(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    if (const Result<> marked = mark_stretch(s, command, args, place); !marked) return std::unexpected<Error>(marked.error());
    const AutoxDiagramMark &mark = s.autox_state.diagram_mark;
    load_browser_with_branch(s, mark.start_branch, mark.start_point, mark.end_point);
    return xpp::format("{{\"rows_loaded\":{}}}", s.data_store.rows);
}

Result<std::string> load_orbit_into_browser(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    const std::optional<long long> label = args.optional<long long>(LABEL_KEY);
    if (const Result<> done = load_autox_orbit(s, label ? std::optional<int>(static_cast<int>(*label)) : std::nullopt); !done)
        return std::unexpected<Error>(as_command_error(command, done.error(), place));
    return xpp::format("{{\"orbit_loaded\":true,\"rows\":{}}}", s.data_store.rows);
}

Result<std::string> select_two_parameter_point(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    if (s.autox_state.axes().plot != AutoxProjection::two_parameter) return fail(command.name, "Must be in a two parameter plot", place);
    if (args.has("x")) autox_point_xy(s, args.get<double>("x"), args.get<double>("y"));
    setautoxpoint(s);
    return "{\"selected\":true}";
}

Result<std::string> draw_labeled_orbits(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<std::string> open = need_open(s, command, place); !open) return open;
    const std::string &mode = args.get<std::string>("mode");
    s.autox_state.load_all_labeled_orbits = mode == "off" ? LabeledOrbits::Off : mode == "draw" ? LabeledOrbits::Draw : LabeledOrbits::ClearAndDraw;
    return xpp::format("{{\"orbits_drawn\":\"{}\"}}", mode);
}

/* ---- the rows ---- */

double threads_maximum(const Session &) { return static_cast<double>(cpu::logical_processors()); }

constexpr std::array<const char *, 7> PROJECTION_CHOICES = {"maximum", "norm", "maximum_and_minimum", "period", "two_parameter", "frequency", "average"};
constexpr std::array<const char *, 2> LAST_PLOT_CHOICES = {"one", "two"};
constexpr std::array<const char *, 3> ORBIT_MODES = {"off", "draw", "clear_and_draw"};

/* the Numerics arguments, from autox_settings.h's lists: each by its shape */
constexpr Type numerics_type(AutoxFieldShape shape)
{
    switch (shape) {
    case AutoxFieldShape::whole: return Type::integer;
    case AutoxFieldShape::real: return Type::real;
    case AutoxFieldShape::boolean: return Type::boolean;
    case AutoxFieldShape::choice: return Type::choice;
    }
    return Type::text;
}

constexpr std::size_t NUMERICS_COUNT = static_cast<std::size_t>(AUTOX_NUM_N);

/* made when the program starts, from the option table's continuation rows */
std::array<Argument, NUMERICS_COUNT> numerics_arguments()
{
    std::array<Argument, NUMERICS_COUNT> arguments{};
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const int field = static_cast<int>(i);
        arguments[i] = Argument{autox_settings_num_key(field), numerics_type(autox_settings_num_shape(field)), false};
        if (autox_settings_num_shape(field) == AutoxFieldShape::choice) arguments[i].choices = autox_output_detail_names;
    }
    return arguments;
}
const std::array<Argument, NUMERICS_COUNT> NUMERICS_ARGUMENTS = numerics_arguments();

/* autox.configure_numerics: the Numerics, and the codimension-two switches */
std::array<Argument, NUMERICS_COUNT + 1> configure_numerics_arguments()
{
    std::array<Argument, NUMERICS_COUNT + 1> arguments{};
    for (std::size_t i = 0; i < NUMERICS_COUNT; ++i) arguments[i] = NUMERICS_ARGUMENTS[i];
    arguments[NUMERICS_COUNT] = Argument{"special_points", Type::map, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::boolean};
    return arguments;
}
const std::array<Argument, NUMERICS_COUNT + 1> CONFIGURE_NUMERICS_ARGUMENTS = configure_numerics_arguments();

/* autox.run: its kind, its start and the settings of this run only (the Numerics, by their names) */
constexpr std::size_t RUN_OWN = 13;
std::array<Argument, RUN_OWN + NUMERICS_COUNT> run_arguments()
{
    std::array<Argument, RUN_OWN + NUMERICS_COUNT> arguments{};
    arguments[0] = Argument{"kind", Type::choice, true, MAX_TEXT_BYTES, autox_run_kind_names};
    arguments[1] = Argument{"switch", Type::boolean, false};
    arguments[2] = Argument{LABEL_KEY, Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)};
    arguments[3] = Argument{"type", Type::text, false};
    arguments[4] = Argument{"index", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)};
    arguments[5] = Argument{"decimals", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)};
    arguments[6] = Argument{"hold", Type::real, false, MAX_TEXT_BYTES, {}, constant(0)};
    arguments[7] = Argument{"maximum", Type::real, false, MAX_TEXT_BYTES, {}, constant(0)};
    arguments[8] = Argument{"replace_diagram", Type::boolean, false};
    arguments[9] = Argument{"unstable_dimension", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)};
    arguments[10] = Argument{"stable_dimension", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)};
    arguments[11] = Argument{"left_equilibrium", Type::list, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real};
    arguments[12] = Argument{"right_equilibrium", Type::list, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real};
    for (std::size_t i = 0; i < NUMERICS_COUNT; ++i) arguments[RUN_OWN + i] = NUMERICS_ARGUMENTS[i];
    return arguments;
}
const std::array<Argument, RUN_OWN + NUMERICS_COUNT> RUN_ARGUMENTS = run_arguments();
constexpr std::array<const char *, 3> RUN_START_ONE = {LABEL_KEY, "type", "decimals"};
constexpr std::array<const char *, 2> RUN_START_TYPE = {"type", "index"};
constexpr std::array<const char *, 3> RUN_START_SETTLE = {"decimals", "hold", "maximum"};
constexpr std::array<Group, 3> RUN_GROUPS = {Group{GroupRule::at_most_one, RUN_START_ONE}, Group{GroupRule::together, RUN_START_TYPE},
                                             Group{GroupRule::together, RUN_START_SETTLE}};

constexpr std::array GRAB_ARGUMENTS = {Argument{LABEL_KEY, Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)}, Argument{"type", Type::text, false},
                                       Argument{"index", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)}};
constexpr std::array<const char *, 2> GRAB_BY = {LABEL_KEY, "type"};
constexpr std::array<const char *, 2> GRAB_TYPE = {"type", "index"};
constexpr std::array<Group, 2> GRAB_GROUPS = {Group{GroupRule::at_most_one, GRAB_BY}, Group{GroupRule::together, GRAB_TYPE}};

constexpr std::array PLOT_ARGUMENTS = {
    Argument{"plot_type", Type::choice, false, MAX_TEXT_BYTES, PROJECTION_CHOICES},
    set_by(Argument{"variable", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable}, "variable"),
    Argument{"parameter1", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::parameter},
    Argument{"parameter2", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::parameter},
    set_by(Argument{"x_min", Type::real, false}, "x_min"), set_by(Argument{"x_max", Type::real, false}, "x_max"),
    set_by(Argument{"y_min", Type::real, false}, "y_min"), set_by(Argument{"y_max", Type::real, false}, "y_max"),
    Argument{VIEW_KEY, Type::window, false}, Argument{"fit", Type::boolean, false}};
constexpr std::array MARKS_ARGUMENTS = {Argument{"parameters", Type::list, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::text},
                                        Argument{"values", Type::list, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real}};
constexpr std::array<const char *, 2> THREADS_ONE = {"threads", "automatic"};
constexpr auto LOG_LEVELS = c_names(log_level_names);
constexpr std::array THREADS_ARGUMENTS = {
    Argument{.name = "threads", .type = Type::integer, .required = false, .min = constant(1), .max = session_quantity("threads_max", threads_maximum),
             .description = "How many threads autoX and a sweep's runs use, at most the computer's logical processors."},
    Argument{.name = "automatic", .type = Type::boolean, .required = false, .description = "Let the program choose: all the physical cores."},
    Argument{.name = "log_level", .type = Type::choice, .required = false, .choices = LOG_LEVELS,
             .description = "What the log shows; it applies from the next start."},
    Argument{.name = "log_file", .type = Type::text, .required = false, .description = "The file the log goes to, an absolute path; empty is the terminal."}};
constexpr std::array<Group, 1> THREADS_GROUPS = {Group{GroupRule::at_most_one, THREADS_ONE}};
constexpr std::array VIEW_ARGUMENTS = {Argument{VIEW_KEY, Type::window, false}};
constexpr std::array PICK_ARGUMENTS = {Argument{"x", Type::real, true}, Argument{"y", Type::real, true}, Argument{VIEW_KEY, Type::window, false}};
constexpr std::array DISPLAY_ARGUMENTS = {Argument{VIEW_KEY, Type::window, false},
                                          Argument{"x_range", Type::list, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real, 2},
                                          Argument{"y_range", Type::list, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real, 2},
                                          Argument{"show_earlier", Type::boolean, false}};
constexpr std::array LAST_PLOT_ARGUMENTS = {Argument{"parameters", Type::choice, true, MAX_TEXT_BYTES, LAST_PLOT_CHOICES}, Argument{VIEW_KEY, Type::window, false}};
constexpr std::array SCROLL_ARGUMENTS = {Argument{"dx", Type::real, true}, Argument{"dy", Type::real, true}, Argument{VIEW_KEY, Type::window, false}};
constexpr std::array BOX_ARGUMENTS = {Argument{"x_min", Type::real, true}, Argument{"x_max", Type::real, true}, Argument{"y_min", Type::real, true},
                                      Argument{"y_max", Type::real, true}, Argument{VIEW_KEY, Type::window, false}};
constexpr std::array FILE_OPTIONAL = {Argument{FILE_KEY, Type::file, false}};
constexpr std::array FILE_REQUIRED = {Argument{FILE_KEY, Type::file, true}};
constexpr std::array PICTURE_ARGUMENTS = {Argument{FILE_KEY, Type::file, false}, Argument{WINDOW_KEY, Type::window, false}};
constexpr std::array REDRAW_ARGUMENTS = {Argument{"enabled", Type::boolean, true}};
constexpr std::array STRETCH_ARGUMENTS = {Argument{"branch", Type::integer, true, MAX_TEXT_BYTES, {}, constant(1)}, Argument{"first_point", Type::integer, true, MAX_TEXT_BYTES, {}, constant(1)},
                                          Argument{"last_point", Type::integer, true, MAX_TEXT_BYTES, {}, constant(1)}};
constexpr std::array LABEL_OPTIONAL = {Argument{LABEL_KEY, Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)}};
constexpr std::array SELECT_ARGUMENTS = {Argument{"x", Type::real, false}, Argument{"y", Type::real, false}};
constexpr std::array<const char *, 2> SELECT_TOGETHER = {"x", "y"};
constexpr std::array<Group, 1> SELECT_GROUPS = {Group{GroupRule::together, SELECT_TOGETHER}};
constexpr std::array ORBIT_ARGUMENTS = {Argument{"mode", Type::choice, true, MAX_TEXT_BYTES, ORBIT_MODES}};

constexpr std::array RESULT_OPEN = {ResultField{"open", "boolean"}};
constexpr std::array RESULT_RUN = {ResultField{"points_computed", "integer"}, ResultField{"branches_tracked", "integer"}};
constexpr std::array RESULT_GRAB = {ResultField{"grabbed", "boolean"}, ResultField{LABEL_KEY, "integer"}, ResultField{"type", "text"},
                                    ResultField{"branch", "integer"}, ResultField{"point", "integer"}};
constexpr std::array RESULT_SETTINGS = {ResultField{"settings_updated", "boolean"}};
constexpr std::array RESULT_PLOT = {ResultField{"plot_configured", "boolean"}};
constexpr std::array RESULT_MARKS = {ResultField{"marks_count", "integer"}};
constexpr std::array RESULT_THREADS = {ResultField{"threads", "integer"}, ResultField{"automatic", "boolean"}, ResultField{"log_level", "choice"}, ResultField{"log_file", "text"}};
constexpr std::array RESULT_HIDDEN = {ResultField{"hidden_points", "integer"}};
constexpr std::array RESULT_REDRAWN = {ResultField{"redrawn", "boolean"}};
constexpr std::array RESULT_PICK = {ResultField{"x", "real"}, ResultField{"y", "real"}};
constexpr std::array RESULT_DISPLAY = {ResultField{"x_range", "list"}, ResultField{"y_range", "list"}};
constexpr std::array RESULT_VIEW = {ResultField{WINDOW_KEY, "window"}, ResultField{"x_range", "list"}, ResultField{"y_range", "list"}};
constexpr std::array RESULT_PLOT_CHOICE = {ResultField{"plot", "choice"}};
constexpr std::array RESULT_CLEARED = {ResultField{"cleared", "boolean"}};
constexpr std::array RESULT_ORBIT = {ResultField{"orbit_loaded", "boolean"}, ResultField{"rows", "integer"}};
constexpr std::array RESULT_FILE = {ResultField{FILE_KEY, "file"}};
constexpr std::array RESULT_POINTS_FILE = {ResultField{FILE_KEY, "file"}, ResultField{"points_written", "integer"}};
constexpr std::array RESULT_WRITTEN_FILE = {ResultField{FILE_KEY, "file"}, ResultField{"written", "integer"}};
constexpr std::array RESULT_IMPORTED = {ResultField{FILE_KEY, "file"}, ResultField{"points_imported", "integer"}};
constexpr std::array RESULT_RESET = {ResultField{"reset", "boolean"}};
constexpr std::array RESULT_REDRAW_MODE = {ResultField{"auto_redraw", "boolean"}};
constexpr std::array RESULT_RANGE = {ResultField{"runs_completed", "integer"}};
constexpr std::array RESULT_BRANCH_ROWS = {ResultField{"rows_loaded", "integer"}};
constexpr std::array RESULT_SELECTED = {ResultField{"selected", "boolean"}};
constexpr std::array RESULT_ORBITS_DRAWN = {ResultField{"orbits_drawn", "choice"}};

constexpr std::array EVENTS_OPEN = {xpp::events::WINDOW_CREATED, xpp::events::WINDOW_SELECTED, xpp::events::WINDOW_DESTROYED, xpp::events::AUTOX_DIAGRAM, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_RUN = {xpp::events::AUTOX_JOBS, xpp::events::AUTOX_DIAGRAM, xpp::events::AUTOX_INFO, xpp::events::AUTOX_SETTINGS, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_GRAB = {xpp::events::AUTOX_INFO, xpp::events::AUTOX_DIAGRAM, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_SETTINGS = {xpp::events::AUTOX_SETTINGS, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_DIAGRAM = {xpp::events::AUTOX_DIAGRAM, xpp::events::AUTOX_VIEW, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_INFO = {xpp::events::AUTOX_INFO, xpp::events::AUTOX_DIAGRAM, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_BROWSER = {xpp::events::SESSION_STATE, xpp::events::DONE};

const std::array ROWS = {
    Command{"autox.open", {}, XPP_KIND_VIEW, CommandCategory::Analysis, "Open autoX", "autoX",
            "Open the autoX window, the bifurcation package.", RESULT_OPEN, EVENTS_OPEN, true, open_autox},
    Command{"autox.run", RUN_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Run", "autoX",
            "Run autoX from the current state or a stored point: its kind (what is computed), switch for a branch switch, its start (a label, a point type and index, or the current state, optionally settled first), the settings of this run only (ds's sign is the direction) and the equilibria of a homoclinic start. The start point's type decides which kinds fit it: hello's autox_run table, and autoxinfo's start.",
            RESULT_RUN, EVENTS_RUN, true, run_autox, RUN_GROUPS, {}, "read", "*", "", "Start run"},
    Command{"autox.grab", GRAB_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Grab", "autoX",
            "Grab a stored point by label or type and index; omit both to pick it on the diagram.", RESULT_GRAB, EVENTS_GRAB, true, grab_point, GRAB_GROUPS, {}},
    Command{"autox.configure_numerics", CONFIGURE_NUMERICS_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Numerics", "autoX",
            "Set autoX's Numerics by their names (mesh_intervals, step_size, ...) and the codimension-two points it detects (special_points), all or nothing.",
            RESULT_SETTINGS, EVENTS_SETTINGS, true, configure_numerics, {}, {}, "read", "*", "continuation"},
    Command{"autox.configure_axes", PLOT_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Axes", "autoX",
            "Set what a view of the diagram plots: the plot type, the variable, the two parameters and the ranges, optionally fitted afterwards.",
            RESULT_PLOT, EVENTS_SETTINGS, true, configure_axes, {}, {}, "read", "*", "diagram+"},
    Command{"autox.configure_marks", MARKS_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Mark values", "autoX",
            "Set the Mark values: the parameters (or T, the period) and the values at which autoX labels a point.", RESULT_MARKS, EVENTS_SETTINGS, true, configure_marks, {}, {}},
    Command{"preferences.get", {}, XPP_KIND_CONTROL, CommandCategory::Analysis, "Get preferences", "",
            "The program's preferences as they stand: the thread count and whether it is automatic, the log level and the log file.", RESULT_THREADS, EVENTS_BROWSER, false, get_preferences, {}, {}},
    Command{"preferences.configure", THREADS_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Preferences", "File",
            "Set the program's preferences, kept in its preferences file: the thread count of autoX and of sweeps (a count, or automatic) and what the log shows and the file it goes to (empty: the terminal); the log applies from the next start.", RESULT_THREADS, EVENTS_SETTINGS, true, configure_preferences, THREADS_GROUPS, {}},
    Command{"autox.import_diagram", FILE_REQUIRED, XPP_KIND_DATA, CommandCategory::Analysis, "Import XPPAUT diagram", "autoX/File",
            "Import an XPPAUT diagram (.auto) into the open model.", RESULT_IMPORTED, EVENTS_RUN, true, import_diagram, {}, {}},
    Command{"autox.reset_diagram", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Reset diagram", "autoX/File",
            "Destroy the diagram and autoX's files.", RESULT_RESET, EVENTS_DIAGRAM, true, reset_diagram, {}, {}},
    Command{"autox.export_pdf", PICTURE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Export diagram PDF", "autoX/Export",
            "Write a view of the diagram as a PDF picture.", RESULT_FILE, EVENTS_BROWSER, true, export_pdf, {}, {}, "write", "*"},
    Command{"autox.export_svg", PICTURE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Export diagram SVG", "autoX/Export",
            "Write a view of the diagram as an SVG picture.", RESULT_FILE, EVENTS_BROWSER, true, export_svg, {}, {}, "write", "*"},
    Command{"autox.export_csv", FILE_OPTIONAL, XPP_KIND_DATA, CommandCategory::Analysis, "Export CSV", "autoX/Export",
            "Write the diagram and its eigenvalues as two CSV files.", RESULT_POINTS_FILE, EVENTS_BROWSER, true, export_csv, {}, {}, "write", "*"},
    Command{"autox.export_points", FILE_OPTIONAL, XPP_KIND_DATA, CommandCategory::Analysis, "Export points", "autoX/Export",
            "Write the diagram's points as the view plots them.", RESULT_POINTS_FILE, EVENTS_BROWSER, true, export_points, {}, {}, "write", "*"},
    Command{"autox.export_all_info", FILE_OPTIONAL, XPP_KIND_DATA, CommandCategory::Analysis, "Export all info", "autoX/Export",
            "Write every point with its parameters, extremes and eigenvalues.", RESULT_WRITTEN_FILE, EVENTS_BROWSER, true, export_all_info, {}, {}, "write", "*"},
    Command{"autox.export_initial_data", FILE_OPTIONAL, XPP_KIND_DATA, CommandCategory::Analysis, "Export initial data", "autoX/Export",
            "Write every point's main parameter and solution as initial data.", RESULT_WRITTEN_FILE, EVENTS_BROWSER, true, export_initial_data, {}, {}, "write", "*"},
    Command{"autox.show_orbit", LABEL_OPTIONAL, XPP_KIND_DATA, CommandCategory::Analysis, "Show orbit in table", "autoX/Branch",
            "Load the stored orbit of a labelled point (the grabbed one without label) into the data browser.", RESULT_ORBIT, EVENTS_BROWSER, true, load_orbit_into_browser, {}, {}},
    Command{"autox.show_branch", STRETCH_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Show branch in table", "autoX/Branch",
            "Load a stretch of a branch (its points first_point to last_point) into the data browser.", RESULT_BRANCH_ROWS, EVENTS_BROWSER, true, load_branch_into_browser, {}, {}},
    Command{"autox.range_branch", STRETCH_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Range over a branch", "autoX/Branch",
            "Run the sweep over a stretch of a branch (its points first_point to last_point), once for each.", RESULT_RANGE, EVENTS_BROWSER, true, range_branch, {}, {}},
    Command{"autox.draw_labeled_orbits", ORBIT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Draw labeled orbits", "autoX/Branch",
            "Draw the orbits of the labelled points when grabbed: off, draw (no erase) or clear_and_draw.", RESULT_ORBITS_DRAWN, EVENTS_BROWSER, true, draw_labeled_orbits, {}, {}},
    Command{"autox.select_2parameter_point", SELECT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Analysis, "Select two-parameter point", "autoX/Branch",
            "Set the two parameters to the point picked on a two-parameter plot, or to x and y.", RESULT_SELECTED, EVENTS_BROWSER, true, select_two_parameter_point, SELECT_GROUPS, {}},
    Command{"autox.clear_grab", {}, XPP_KIND_DATA, CommandCategory::Analysis, "Clear grab", "autoX/Branch",
            "Forget the grabbed point: a following run starts from the current state.", RESULT_CLEARED, EVENTS_GRAB, true, clear_grab, {}, {}},
    Command{"autox.set_auto_redraw", REDRAW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Automatic diagram redraw", "autoX/View",
            "Redraw the diagram automatically when enabled.", RESULT_REDRAW_MODE, EVENTS_BROWSER, true, set_auto_redraw, {}, {}},
    Command{"autox.restore_plot", LAST_PLOT_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Restore last plot", "autoX/View",
            "Go back to the last one-parameter or two-parameter plot's axes.", RESULT_PLOT_CHOICE, EVENTS_DIAGRAM, true, use_last_plot, {}, {}},
    Command{"autox.reset_view", VIEW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Reset diagram view", "autoX/View",
            "Give the view the model's own diagram ranges.", RESULT_VIEW, EVENTS_DIAGRAM, true, default_view, {}, {}},
    Command{"autox.zoom_in", BOX_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Zoom diagram in", "autoX/View",
            "Zoom the view in to a box of its plotted quantities.", RESULT_VIEW, EVENTS_DIAGRAM, true, zoom_in, {}, {}},
    Command{"autox.zoom_out", BOX_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Zoom diagram out", "autoX/View",
            "Zoom the view out so that what it shows now fills a box of its plotted quantities.", RESULT_VIEW, EVENTS_DIAGRAM, true, zoom_out, {}, {}},
    Command{"autox.scroll", SCROLL_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Scroll diagram", "autoX/View",
            "Move the view's visible ranges by dx and dy, in the plotted quantities.", RESULT_VIEW, EVENTS_DIAGRAM, true, scroll_view, {}, {}},
    Command{"autox.new_view", {}, XPP_KIND_VIEW, CommandCategory::Analysis, "New autoX view", "Windows",
            "Add a view of the diagram, with the axes of the active one; it becomes the active view.", RESULT_VIEW, EVENTS_DIAGRAM, true, new_view, {}, {}},
    Command{"autox.hide_earlier", {}, XPP_KIND_VIEW, CommandCategory::Analysis, "Hide earlier branches", "",
            "Hide the branches so far: new runs draw alone until set_display shows them again.", RESULT_HIDDEN, EVENTS_DIAGRAM, true, hide_earlier, {}, {}},
    Command{"autox.redraw", {}, XPP_KIND_VIEW, CommandCategory::Analysis, "Redraw diagram", "",
            "Draw the diagram again.", RESULT_REDRAWN, EVENTS_DIAGRAM, true, redraw, {}, {}},
    Command{"autox.pick", PICK_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Pick point", "",
            "A point of the diagram in its plotted quantities: shown, and stored for select_2parameter_point in a two-parameter plot.", RESULT_PICK, EVENTS_INFO, true, pick_point, {}, {}},
    Command{"autox.set_display", DISPLAY_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Set diagram display", "",
            "Set what the page shows of a view: its zoom (a [low, high] range, [] for the whole range of an axis) and whether the hidden branches are drawn.",
            RESULT_DISPLAY, EVENTS_DIAGRAM, true, set_display, {}, {}},
    Command{"autox.fit", VIEW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Analysis, "Fit diagram", "",
            "Fit the view to the diagram.", RESULT_VIEW, EVENTS_DIAGRAM, true, fit_view, {}, {}},};

} // namespace

std::span<const Command> autox_commands() { return ROWS; }

} // namespace xpp::command
