/* The kinescope's registry rows (W527): the frames the client keeps (capture, reset), playing them back and
   saving them as files or one animated GIF. The captured pictures live in the front end; each handler is the
   core function the Kinescope menu's keys call too (commands.cpp's do_movie_com). */
#include "command_def.h"
#include "image_format.h"
#include "kinescope.h"
#include "session.h"
#include "xpp_ui.h"
#include <array>
#include <limits>

namespace xpp::command {
namespace {

Result<> need_frames(const Session &s, const Command &command, const Place &place)
{
    if (s.kinescope.frames == 0) return fail(command.name, "The kinescope holds no frames: capture one first", place);
    return {};
}

Result<std::string> capture(Session &s, const Command &, const Arguments &, const Place &)
{
    if (const Result<> clipped = ui.film_clip(s); !clipped) return std::unexpected<Error>(clipped.error());
    return ResultObject().integer("frame_count", s.kinescope.frames).done();
}

Result<std::string> reset(Session &s, const Command &, const Arguments &, const Place &)
{
    reset_film(s);
    return ResultObject().boolean("cleared", true).done();
}

Result<std::string> play(Session &s, const Command &command, const Arguments &, const Place &place)
{
    if (const Result<> ready = need_frames(s, command, place); !ready) return std::unexpected<Error>(ready.error());
    ui.movie_play_back(s);
    return ResultObject().integer("frames_played", s.kinescope.frames).done();
}

Result<std::string> autoplay(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<> ready = need_frames(s, command, place); !ready) return std::unexpected<Error>(ready.error());
    s.kinescope.cycles = static_cast<int>(args.optional<long long>("cycles").value_or(s.kinescope.cycles));
    s.kinescope.frame_ms = static_cast<int>(args.optional<long long>("frame_ms").value_or(s.kinescope.frame_ms));
    ui.movie_auto_play(s);
    return ResultObject().integer("cycles_completed", s.kinescope.cycles).done();
}

Result<std::string> export_frames(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<> ready = need_frames(s, command, place); !ready) return std::unexpected<Error>(ready.error());
    const std::string &file = args.get<std::string>("file");
    const Result<int> saved = ui.movie_save(s, file, IMAGE_FORMAT_GIF);
    if (!saved) return std::unexpected<Error>(saved.error());
    return ResultObject().text("file", file).integer("frames_saved", *saved).done();
}

Result<std::string> export_gif(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (const Result<> ready = need_frames(s, command, place); !ready) return std::unexpected<Error>(ready.error());
    const std::string &file = args.get<std::string>("file");
    if (const Result<> written = ui.movie_make_anigif(s, file); !written) return std::unexpected<Error>(written.error());
    const Result<unsigned long long> size = file_size(file, command, place);
    if (!size) return std::unexpected<Error>(size.error());
    return ResultObject().text("file", file).integer("bytes_written", static_cast<long long>(*size)).done();
}

constexpr std::array AUTOPLAY_ARGUMENTS = {
    Argument{"cycles", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(std::numeric_limits<int>::max())},
    Argument{"frame_ms", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0), constant(std::numeric_limits<int>::max())}};
constexpr std::array FILE_ARGUMENTS = {Argument{"file", Type::file, true}};
constexpr std::array CAPTURE_RESULTS = {ResultField{"frame_count", "integer"}};
constexpr std::array RESET_RESULTS = {ResultField{"cleared", "boolean"}};
constexpr std::array PLAY_RESULTS = {ResultField{"frames_played", "integer"}};
constexpr std::array AUTOPLAY_RESULTS = {ResultField{"cycles_completed", "integer"}};
constexpr std::array FRAMES_RESULTS = {ResultField{"file", "file"}, ResultField{"frames_saved", "integer"}};
constexpr std::array GIF_RESULTS = {ResultField{"file", "file"}, ResultField{"bytes_written", "integer"}};
constexpr std::array FILM_EVENTS = {"film", "state", "done"};
constexpr std::array DONE_EVENTS = {"state", "done"};

constexpr std::array ROWS = {
    Command{"kinescope.capture", {}, XPP_KIND_VIEW, CommandCategory::Plot, "File/Kinescope/Capture", "KC",
            "Capture the plot window as the next frame of the kinescope.", CAPTURE_RESULTS, FILM_EVENTS, true, capture},
    Command{"kinescope.reset", {}, XPP_KIND_VIEW, CommandCategory::Plot, "File/Kinescope/Reset", "KR",
            "Throw away every captured frame.", RESET_RESULTS, FILM_EVENTS, true, reset},
    Command{"kinescope.play", {}, XPP_KIND_VIEW, CommandCategory::Plot, "File/Kinescope/Playback", "KP",
            "Show the captured frames once.", PLAY_RESULTS, FILM_EVENTS, true, play},
    Command{"kinescope.autoplay", AUTOPLAY_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Plot, "File/Kinescope/Autoplay", "KA",
            "Show the captured frames cycles times, frame_ms apart; what is left out is the last autoplay's setting.",
            AUTOPLAY_RESULTS, FILM_EVENTS, true, autoplay},
    Command{"kinescope.export_frames", FILE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "File/Kinescope/Save", "KS",
            "Write every captured frame as a GIF file of the model's folder: file for the first, then its numbered names.",
            FRAMES_RESULTS, DONE_EVENTS, true, export_frames, {}, {}, "", "write", "*"},
    Command{"kinescope.export_gif", FILE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Plot, "File/Kinescope/Make AniGif", "KM",
            "Write the captured frames as one animated GIF file of the model's folder.", GIF_RESULTS, DONE_EVENTS, true, export_gif, {}, {}, "", "write", "*"}};

} // namespace

std::span<const Command> kinescope_commands() { return ROWS; }

} // namespace xpp::command
