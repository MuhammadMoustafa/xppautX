/* The keymap resource's registry rows (W528): the user's keyboard bindings (one key each) and the quick-access commands, kept in keymap.json (xpp_keymap.h). Each page and --server session reads and writes the one file
   through the front end (json_keymap.cpp), which sends the `keymap` event the page keeps its shortcuts from. */
#include "command_def.h"
#include "xpp_keymap.h"
#include <array>

namespace xpp::command {
namespace {

constexpr std::array SET_ARGUMENTS = {
    Argument{"bindings", Type::map, false, MAX_COMMAND_BYTES, {}, {}, {}, NameSet::none, Type::text_list},
    Argument{"pinned", Type::list, false, MAX_COMMAND_BYTES, {}, {}, {}, NameSet::none, Type::text}};
constexpr std::array GET_RESULTS = {ResultField{"bindings", "map[text, list[text]]"}, ResultField{"pinned", "list[text]"}};
constexpr std::array SET_RESULTS = {ResultField{"saved", "boolean"}};
constexpr std::array RESET_RESULTS = {ResultField{"reset", "boolean"}};
constexpr std::array GET_EVENTS = {"keymap", "done"};
constexpr std::array SET_EVENTS = {"keymap", "state", "done"};

constexpr std::array ROWS = {
    Command{"keymap.get", {}, XPP_KIND_CONTROL, CommandCategory::Panels, "Help/Keymap/Get",
            "The user's keymap: the keys of the commands they changed, and the pinned commands.", GET_RESULTS, GET_EVENTS, false, front_end},
    Command{"keymap.set", SET_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Panels, "Help/Keymap/Set",
            "Replace the bindings or the pinned commands given (what is left out stays); the keymap is checked whole, then written.",
            SET_RESULTS, SET_EVENTS, false, front_end},
    Command{"keymap.reset", {}, XPP_KIND_CONTROL, CommandCategory::Panels, "Help/Keymap/Reset", "Remove the user's keymap: every command has its default keys.",
            RESET_RESULTS, SET_EVENTS, false, front_end}};

} // namespace

std::span<const Command> keymap_commands() { return ROWS; }

} // namespace xpp::command
