/* The recording resource's registry rows (W528): recording the session's steps, and playing a recording back in the
   player. The recorder and the player are the front end's own state (json_record.cpp, json_player.cpp), so each row's
   handler is front_end; the rows name the commands, their arguments and results. */
#include "command_def.h"
#include "recx.h"
#include <array>

namespace xpp::command {
namespace {

constexpr Argument FILE_OPTIONAL = {"file", Type::file, false};
constexpr Argument STEP = {"step", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0)};
/* the longest note: a few paragraphs of explanation above a step (a note is shown whole in the recording bar and the player) */
constexpr std::size_t MAX_NOTE_BYTES = 4096;
constexpr Argument NOTE = {"note", Type::text, true, MAX_NOTE_BYTES, {}, {}, {}, NameSet::none, Type::text, MAX_ITEMS, true};

constexpr std::array STOP_ARGUMENTS = {FILE_OPTIONAL, Argument{"confirm", Type::boolean, false}};
constexpr std::array NOTE_ARGUMENTS = {NOTE};
constexpr std::array OPEN_ARGUMENTS = {Argument{"file", Type::path, true}, Argument{"play", Type::boolean, true}};
constexpr std::array SPEED_ARGUMENTS = {Argument{"speed", Type::real, true, MAX_TEXT_BYTES, {}, constant(recx::player_speed_min), constant(recx::player_speed_max)}};
constexpr std::array SEEK_ARGUMENTS = {STEP, Argument{"play", Type::boolean, false}};
constexpr std::array STEP_NOTE_ARGUMENTS = {STEP, NOTE, Argument{"confirm", Type::boolean, false}};

constexpr std::array START_RESULTS = {ResultField{"recording", "boolean"}};
constexpr std::array STOP_RESULTS = {ResultField{"file", "file"}, ResultField{"steps_recorded", "integer"}};
constexpr std::array NOTE_RESULTS = {ResultField{"note_added", "boolean"}};
constexpr std::array OPEN_RESULTS = {ResultField{"file", "file"}, ResultField{"total_steps", "integer"}};
constexpr std::array PLAY_RESULTS = {ResultField{"playing", "boolean"}};
constexpr std::array PAUSE_RESULTS = {ResultField{"paused", "boolean"}};
constexpr std::array STEP_RESULTS = {ResultField{"step_index", "integer"}};
constexpr std::array SPEED_RESULTS = {ResultField{"speed", "real"}};
constexpr std::array SEEK_RESULTS = {ResultField{"step", "integer"}};
constexpr std::array CLOSE_RESULTS = {ResultField{"closed", "boolean"}};
constexpr std::array STATE_EVENTS = {"state", "done"};
constexpr std::array PLAYER_EVENTS = {"player", "state", "done"};
constexpr std::array DONE_EVENTS = {"done"};

constexpr std::array ROWS = {
    Command{"recording.start", {}, XPP_KIND_DATA, CommandCategory::Panels, "File/Record steps",
            "Start recording the session's steps, from the session as it is now.", START_RESULTS, STATE_EVENTS, false, front_end},
    Command{"recording.stop", STOP_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Panels, "File/Record steps",
            "Stop recording and save the recording (.recx added); left out, the file is asked for; confirm true replaces an existing file without asking.",
            STOP_RESULTS, STATE_EVENTS, false, front_end, {}, {}, "write", "*.recx"},
    Command{"recording.add_note", NOTE_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Panels, "File/Recording/Add note",
            "Put a note on the next recorded step.", NOTE_RESULTS, DONE_EVENTS, false, front_end},
    Command{"recording.open", OPEN_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Panels, "File/Play recording",
            "Open a recording (.recx; its path, which only --server's own input may give) in the player: its model is loaded and its steps are ready; play says whether it starts playing at once.", OPEN_RESULTS, PLAYER_EVENTS, false,
            front_end, {}, {}, "read", "*.recx"},
    Command{"recording.play", {}, XPP_KIND_CONTROL, CommandCategory::Panels, "File/Recording/Play", "Play the open recording from where it is.", PLAY_RESULTS,
            DONE_EVENTS, false, front_end},
    Command{"recording.pause", {}, XPP_KIND_CONTROL, CommandCategory::Panels, "File/Recording/Pause", "Pause the playing recording.", PAUSE_RESULTS, DONE_EVENTS,
            false, front_end},
    Command{"recording.step", {}, XPP_KIND_CONTROL, CommandCategory::Panels, "File/Recording/Step", "Play the next step of the open recording.", STEP_RESULTS,
            DONE_EVENTS, false, front_end},
    Command{"recording.set_speed", SPEED_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Panels, "File/Recording/Set speed",
            "Set the speed the recording's paces are divided by (hello.player_speed gives the bounds).", SPEED_RESULTS, DONE_EVENTS, false, front_end},
    Command{"recording.seek", SEEK_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Panels, "File/Recording/Seek",
            "Jump to a step: the steps before it run at once, then the recording plays when play is true (left out: when the step is not the first).",
            SEEK_RESULTS, PLAYER_EVENTS, false, front_end},
    Command{"recording.close", {}, XPP_KIND_DATA, CommandCategory::Panels, "File/Recording/Close", "Leave the player; the model stays.", CLOSE_RESULTS,
            STATE_EVENTS, false, front_end},
    Command{"recording.set_step_note", STEP_NOTE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Panels, "File/Recording/Set step note",
            "Write a note into a step of the open recording's file; confirm true replaces the file without asking.", SEEK_RESULTS, PLAYER_EVENTS, false, front_end}};

} // namespace

std::span<const Command> recording_commands() { return ROWS; }

} // namespace xpp::command
