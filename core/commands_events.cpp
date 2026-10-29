/* The events resource's registry rows (W528): which of the heavy data events (the plots, the AUTO info, the numerics ...)
   a client gets after each command. The subscriptions are the front end's (json_state.cpp), so each row's handler is
   front_end. */
#include "command_def.h"
#include "event_streams.h"
#include <array>

namespace xpp::command {
namespace {

constexpr auto STREAMS = c_names(event_streams::NAMES);
constexpr auto ENCODINGS = c_names(event_streams::ENCODINGS);
constexpr std::array SUBSCRIBE_ARGUMENTS = {
    Argument{"events", Type::list, true, MAX_COMMAND_BYTES, STREAMS, {}, {}, NameSet::none, Type::choice},
    Argument{"encoding", Type::choice, false, MAX_TEXT_BYTES, ENCODINGS}, Argument{"diagram", Type::boolean, false}};
constexpr std::array UNSUBSCRIBE_ARGUMENTS = {
    Argument{"events", Type::list, true, MAX_COMMAND_BYTES, STREAMS, {}, {}, NameSet::none, Type::choice}};
constexpr std::array SUBSCRIBE_RESULTS = {ResultField{"subscribed", "list[text]"}};
constexpr std::array UNSUBSCRIBE_RESULTS = {ResultField{"unsubscribed", "list[text]"}};
constexpr std::array EVENTS = {"state", "done"};

constexpr std::array ROWS = {
    Command{"events.subscribe", SUBSCRIBE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Panels, "Help/Events/Subscribe", "",
            "Send the heavy data events named (hello.features lists them) from now on, each at the end of this command; encoding f32 sends their value arrays "
            "as base64 float32 (what is left out stays as it was); diagram says whether AUTO's diagram points are sent.",
            SUBSCRIBE_RESULTS, EVENTS, false, front_end},
    Command{"events.unsubscribe", UNSUBSCRIBE_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Panels, "Help/Events/Unsubscribe", "",
            "Stop sending the data events named.", UNSUBSCRIBE_RESULTS, EVENTS, false, front_end}};

} // namespace

std::span<const Command> events_commands() { return ROWS; }

} // namespace xpp::command
