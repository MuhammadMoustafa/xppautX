/* The registry commands the JSON front end answers itself (W528): the ones whose state is the front end's own, the
   recording and the player, the user's keymap, the event subscriptions, the state and the model source. A row's
   handler is command::front_end, which comes here through XppUi.front_end_command; each case calls the function of the
   file that owns the state. */
#include "ui_json_internal.h"
#include "command_def.h"
#include "model_switch.h"
#include "session.h"
#include "xpp_ui.h"

namespace xpp::json {

namespace {

/* session.quit: save true saves the session (and a recording in progress) first, false quits at once, left out asks
   the leave question; a cancelled question leaves the session as it was */
xpp::Result<std::string> quit(xpp::Session &s, const xpp::command::Arguments &args)
{
    const std::optional<bool> save = args.optional<bool>("save");
    if (save && !*save) xpp::bye_bye();
    /* a recording playing waits: the question is the user's */
    player_hold();
    xpp_quit(s, save.value_or(false));
    return xpp::command::ResultObject().boolean("quitting", false).done();
}

} // namespace

xpp::Result<std::string> front_end_command(xpp::Session &s, const xpp::command::Command &command, const xpp::command::Arguments &args,
                                           const xpp::Place &place)
{
    const std::string_view name = command.name;
    if (name == "session.get_state") return session_get_state(s);
    if (name == "session.get_source") return session_get_source(s);
    if (name == "session.quit") return quit(s, args);
    if (name == "recording.start") return recording_start(s);
    if (name == "recording.stop") return recording_stop(s, args.optional<std::string>("file"), args.optional<bool>("confirm").value_or(false));
    if (name == "recording.add_note") return recording_add_note(args.get<std::string>("note"));
    if (name == "recording.open") return recording_open(s, args.get<std::string>("file"), args.get<bool>("play"));
    if (name == "recording.play") return recording_control("play", std::nullopt);
    if (name == "recording.pause") return recording_control("pause", std::nullopt);
    if (name == "recording.step") return recording_control("step", std::nullopt);
    if (name == "recording.set_speed") return recording_control("speed", args.get<double>("speed"));
    if (name == "recording.seek") return recording_seek(s, args.get<long long>("step"), args.optional<bool>("play"));
    if (name == "recording.close") return recording_close();
    if (name == "recording.set_step_note") return recording_set_step_note(args.get<long long>("step"), args.get<std::string>("note"), args.optional<bool>("confirm").value_or(false));
    if (name == "keymap.get") return keymap_get();
    if (name == "keymap.set") return keymap_set(args);
    if (name == "keymap.reset") return keymap_reset();
    if (name == "events.subscribe") return events_subscribe(s, args);
    if (name == "events.unsubscribe") return events_unsubscribe(s, args);
    return xpp::fail(command.name, "The front end has no such command", place);
}

} // namespace xpp::json
