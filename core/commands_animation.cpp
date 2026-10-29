/* The animation's registry rows (W527): its window, the file it plays, Go (play, pause, faster, slower, the
   speed), the frame shown (reset, step, seek, the skip), live drawing, grabbing a point and writing the frames.
   Each handler calls the core function the animation window's keys call too (aniparse.h): one behaviour,
   whichever front end asks. The window itself is closed with window.destroy or window.hide. */
#include "aniparse.h"
#include "command_def.h"
#include "session.h"
#include "xpp_ui.h"
#include <array>
#include <limits>

namespace xpp::command {
namespace {

/* faster and slower change the delay between two frames of Go by this many ms, slower to at most
   MAX_SLOWER_MS (the animation window's buttons: XPPAUT's own steps) */
constexpr int SPEED_STEP_MS = 2, MAX_SLOWER_MS = 100;

std::string speed_result(const Session &s) { return ResultObject().integer("speed_ms", s.animation.speed).done(); }

Result<std::string> open(Session &s, const Command &, const Arguments &, const Place &)
{
    new_vcr(s);
    return ResultObject().boolean("open", s.animation.vcr.iexist == 1).done();
}

Result<std::string> load(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>("file");
    if (const Result<> loaded = load_chosen_animation(s, file); !loaded) return std::unexpected<Error>(loaded.error());
    ui.ani_slider(s);
    return ResultObject().text("file", file).boolean("loaded", s.animation.ncom > 0).done();
}

Result<std::string> play(Session &s, const Command &, const Arguments &, const Place &)
{
    if (const Result<int> shown = play_animation(s, {}); !shown) return std::unexpected<Error>(shown.error());
    return ResultObject().boolean("playing", false).done();
}

Result<std::string> reset(Session &s, const Command &command, const Arguments &, const Place &)
{
    if (const Result<> ready = animation_ready(s, command.name); !ready) return std::unexpected<Error>(ready.error());
    ani_reset(s);
    return ResultObject().integer("frame", s.animation.vcr.pos).done();
}

Result<std::string> set_frame_skip(Session &s, const Command &, const Arguments &args, const Place &)
{
    s.animation.vcr.inc = static_cast<int>(args.get<long long>("skip"));
    ui.ani_slider(s);
    return ResultObject().integer("skip", s.animation.vcr.inc).done();
}

Result<std::string> export_frames(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const bool ppm = args.optional<bool>("ppm").value_or(false), gif = args.optional<bool>("animated_gif").value_or(false);
    if (ppm == gif) return fail(command.name, "Choose PPM frames or an animated GIF: exactly one of ppm and animated_gif is true", place);
    const Result<int> written = play_animation(s, {gif ? AniExportKind::gif : AniExportKind::ppm, args.get<std::string>("file")});
    if (!written) return std::unexpected<Error>(written.error());
    return ResultObject().integer("frames_saved", *written).done();
}

Result<std::string> set_live_draw(Session &s, const Command &, const Arguments &args, const Place &)
{
    s.animation.options.on_the_fly = args.get<bool>("enabled") ? 1 : 0;
    ui.ani_slider(s);
    return ResultObject().boolean("live_draw", s.animation.options.on_the_fly != 0).done();
}

/* press at (x, y), move to (to_x, to_y) when given, release: what a drag on the picture does */
Result<std::string> grab_point(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (s.animation.ncom == 0) return fail(command.name, "No animation is loaded: load one first (animation.load)", place);
    if (s.animation.grabs.empty()) return fail(command.name, "The animation has no grab points: its file gives none", place);
    if (!args.has("x") && args.has("to_x")) return fail(command.name, "to_x and to_y need x and y", place);
    if (!s.animation.grab_flag) ani_grab_start(s);
    if (!args.has("x")) {ui.ani_slider(s); return ResultObject().boolean("grabbed", false).done();}
    const double x = args.get<double>("x"), y = args.get<double>("y");
    ani_grab_mouse(s, 1, x, y);
    const double to_x = args.optional<double>("to_x").value_or(x), to_y = args.optional<double>("to_y").value_or(y);
    if (args.has("to_x")) update_ani_motion_stuff(s, to_x, to_y);
    ani_grab_mouse(s, 0, to_x, to_y);
    ui.ani_slider(s);
    return ResultObject().boolean("grabbed", s.animation.who_was_grabbed >= 0).done();
}

Result<std::string> set_speed(Session &s, const Command &, const Arguments &args, const Place &)
{
    s.animation.speed = static_cast<int>(args.get<long long>("ms"));
    ui.ani_slider(s);
    return speed_result(s);
}

/* the frame shown after `move`, which needs an animation to show and data to show it from */
template <class Move> Result<std::string> move_frame(Session &s, const Command &command, Move move)
{
    if (const Result<> ready = animation_ready(s, command.name); !ready) return std::unexpected<Error>(ready.error());
    move();
    ui.ani_slider(s);
    return ResultObject().integer("row", s.animation.vcr.pos).done();
}

Result<std::string> step(Session &s, const Command &command, const Arguments &args, const Place &)
{
    return move_frame(s, command, [&] { ani_flip1(s, static_cast<int>(args.get<long long>("rows"))); });
}

Result<std::string> seek(Session &s, const Command &command, const Arguments &args, const Place &)
{
    return move_frame(s, command, [&] { ani_seek(s, static_cast<int>(args.get<long long>("row"))); });
}

/* Pause stops a Go that is running (the checkpoint that takes it sees its name, ui_json.cpp); with none it only says the speed */
Result<std::string> pause(Session &s, const Command &, const Arguments &, const Place &)
{
    return speed_result(s);
}

Result<std::string> faster(Session &s, const Command &, const Arguments &, const Place &)
{
    s.animation.speed = std::max(0, s.animation.speed - SPEED_STEP_MS);
    ui.ani_slider(s);
    return speed_result(s);
}

Result<std::string> slower(Session &s, const Command &, const Arguments &, const Place &)
{
    s.animation.speed = std::min(MAX_SLOWER_MS, s.animation.speed + SPEED_STEP_MS);
    ui.ani_slider(s);
    return speed_result(s);
}

constexpr long long MAX_INT = std::numeric_limits<int>::max();
constexpr std::array FILE_ARGUMENT = {Argument{"file", Type::file, true}};
/* a skip past the stored rows shows the first frame only: this keeps the row plus the skip a whole int */
constexpr long long MAX_FRAME_SKIP = 100000000;
constexpr std::array SKIP_ARGUMENTS = {Argument{"skip", Type::integer, true, MAX_TEXT_BYTES, {}, constant(1), constant(MAX_FRAME_SKIP)}};
constexpr std::array EXPORT_ARGUMENTS = {Argument{"ppm", Type::boolean, false}, Argument{"animated_gif", Type::boolean, false}, Argument{"file", Type::file, true}};
constexpr std::array LIVE_ARGUMENTS = {Argument{"enabled", Type::boolean, true}};
constexpr std::array GRAB_ARGUMENTS = {Argument{"x", Type::real, false}, Argument{"y", Type::real, false}, Argument{"to_x", Type::real, false}, Argument{"to_y", Type::real, false}};
constexpr std::array GRAB_TOGETHER = {"to_x", "to_y"};
constexpr std::array GRAB_ORIGIN = {"x", "y"};
constexpr std::array GRAB_GROUPS = {Group{GroupRule::together, GRAB_ORIGIN}, Group{GroupRule::together, GRAB_TOGETHER}};
constexpr std::array SPEED_ARGUMENTS = {Argument{"ms", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_ANI_SPEED)}};
constexpr std::array STEP_ARGUMENTS = {Argument{"rows", Type::integer, true, MAX_TEXT_BYTES, {}, constant(-MAX_INT), constant(MAX_INT)}};
constexpr std::array SEEK_ARGUMENTS = {Argument{"row", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_INT)}};
constexpr std::array OPEN_RESULTS = {ResultField{"open", "boolean"}};
constexpr std::array LOAD_RESULTS = {ResultField{"file", "file"}, ResultField{"loaded", "boolean"}};
constexpr std::array PLAY_RESULTS = {ResultField{"playing", "boolean"}};
constexpr std::array RESET_RESULTS = {ResultField{"frame", "integer"}};
constexpr std::array SKIP_RESULTS = {ResultField{"skip", "integer"}};
constexpr std::array EXPORT_RESULTS = {ResultField{"frames_saved", "integer"}};
constexpr std::array LIVE_RESULTS = {ResultField{"live_draw", "boolean"}};
constexpr std::array GRAB_RESULTS = {ResultField{"grabbed", "boolean"}};
constexpr std::array ROW_RESULTS = {ResultField{"row", "integer"}};
constexpr std::array SPEED_RESULTS = {ResultField{"speed_ms", "integer"}};
constexpr std::array WINDOW_EVENTS = {xpp::events::WINDOW_CREATED, xpp::events::WINDOW_SELECTED, xpp::events::WINDOW_DESTROYED, xpp::events::ANIMATION_STATE, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array ANI_EVENTS = {xpp::events::ANIMATION_STATE, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array GRAB_EVENTS = {xpp::events::ANIMATION_STATE, xpp::events::PLOT_SERIES, xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array ROWS = {
    Command{"animation.open", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Viewaxes/Toon",
            "Open the animation window.", OPEN_RESULTS, WINDOW_EVENTS, true, open},
    Command{"animation.load", FILE_ARGUMENT, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/File",
            "Load an animation file (.anix or .ani) of the model's folder; its first frame shows when there is data.",
            LOAD_RESULTS, ANI_EVENTS, true, load, {}, {}},
    Command{"animation.play", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Go",
            "Play the stored rows as frames from the one shown to the last.", PLAY_RESULTS, ANI_EVENTS, true, play, {}, {}},
    Command{"animation.reset", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Reset",
            "Back to the first frame.", RESET_RESULTS, ANI_EVENTS, true, reset, {}, {}},
    Command{"animation.set_frame_skip", SKIP_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Skip",
            "Show every skip-th stored row when playing.", SKIP_RESULTS, ANI_EVENTS, true, set_frame_skip, {}, {}},
    Command{"animation.export_frames", EXPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "File/Animation/Mpeg",
            "Play the animation writing every frame, as numbered PPM files from file (ppm) or as one animated GIF file (animated_gif), one of the two, into the model's folder.",
            EXPORT_RESULTS, ANI_EVENTS, true, export_frames, {}, {}, "write", "*"},
    Command{"animation.set_live_draw", LIVE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/On the fly",
            "Draw the animation while an integration runs.", LIVE_RESULTS, ANI_EVENTS, true, set_live_draw, {}, {}},
    Command{"animation.grab_point", GRAB_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Plot, "Windows/Animation/Grab",
            "Pick a grab point in the animation's coordinates and optionally move it; omit coordinates to begin pointer selection.",
            GRAB_RESULTS, GRAB_EVENTS, true, grab_point, GRAB_GROUPS, {}},
    Command{"animation.set_speed", SPEED_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Set speed",
            "Set the delay between two frames of Go in ms; it reaches a Go that is running.", SPEED_RESULTS, ANI_EVENTS, true, set_speed, {}, {}},
    Command{"animation.step", STEP_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Step",
            "Move the frame shown by rows stored rows (negative: back).", ROW_RESULTS, ANI_EVENTS, true, step, {}, {}},
    Command{"animation.seek", SEEK_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Seek",
            "Show the frame of stored row row (row 0 is the first).", ROW_RESULTS, ANI_EVENTS, true, seek, {}, {}},
    Command{"animation.pause", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Pause",
            "Stop a Go that is running at the frame it is on; with none running it changes nothing.", SPEED_RESULTS, ANI_EVENTS, true, pause, {}, {}},
    Command{"animation.faster", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Faster",
            "Shorten the delay between two frames of Go by a step; it reaches a Go that is running.", SPEED_RESULTS, ANI_EVENTS, true, faster, {}, {}},
    Command{"animation.slower", {}, XPP_KIND_VIEW, CommandCategory::Plot, "Windows/Animation/Slower",
            "Lengthen the delay between two frames of Go by a step; it reaches a Go that is running.", SPEED_RESULTS, ANI_EVENTS, true, slower, {}, {}}};

} // namespace

std::span<const Command> animation_commands() { return ROWS; }

} // namespace xpp::command
