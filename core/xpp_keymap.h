#pragma once
/* The keys (W211, W790): what a key press is, the keys the browser and the
   system keep, the user's keymap as data (its bindings and its pinned
   commands), the effective keys of each command, and the `keymap` object the
   page gets (docs/protocol.md "Keymap"). The keymap is the `keymap` group of
   settings.json, whose one owner is xpp_settings.h: it reads, checks and
   writes the file, this module says what a key and a binding are. C++ only.

   A binding is a command id with its keys ([] = none); the table's defaults
   stand for every command the user leaves alone. */
#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp::command {
struct Command;
}

namespace xpp::keymap {

/* the longest key string: Ctrl+Alt+Shift+Meta+ArrowRight, with room */
inline constexpr std::size_t MAX_KEY_BYTES = 64;
/* keys per command: the command table gives two (a binding and one alternative), the user may add a few */
inline constexpr std::size_t MAX_KEYS_PER_COMMAND = 8;
/* how many commands the user may pin to the quick-access toolbar, and how many commands the user may bind:
   each is a few bytes, so these bounds are generous and stop a hostile file's count before any allocation */
inline constexpr std::size_t MAX_PINNED = 64;
inline constexpr std::size_t MAX_BINDINGS = 1024;
/* the steps of one pin and the bytes of a pin's name (W775): a pin is a handful of commands, and its name a label on a button */
inline constexpr std::size_t MAX_PIN_STEPS = MAX_KEYS_PER_COMMAND; /* the registry's limit on a list of texts inside a map (command_validate.cpp), as a command's keys */
inline constexpr std::size_t MAX_PIN_NAME_BYTES = 64;

/* Keys the page and the system keep (docs/command-design.md "Principles" 4):
   Alt+F4, Ctrl+W, Ctrl+Q, F11, F12 and the browser's own tab, window,
   address and reload keys. No command is bound to one, and the page never
   calls preventDefault on them; hello's `keymap.reserved` sends this list.
   Each is a key as key_problem writes it. */
inline constexpr std::array<std::string_view, 16> RESERVED_KEYS = {
    "Alt+F4", "Ctrl+W", "Ctrl+Q", "F11", "F12", "F5", "Ctrl+T", "Ctrl+N", "Ctrl+Shift+N", "Ctrl+Shift+T",
    "Ctrl+Tab", "Ctrl+Shift+Tab", "Alt+ArrowLeft", "Alt+ArrowRight", "Ctrl+L", "Ctrl+Shift+I"};

/* a pin (W775): a name and its steps, each a command value (command_def.h) in the order they run. One kind of pin: a pin of one step is
   the command a toolbar button runs, a pin of several is a sequence the page runs as a batch of ordinary commands */
struct Pin {
    std::string name;
    std::vector<std::string> steps;
    bool operator==(const Pin &) const = default;
};

/* the user's keymap: its differences from the table, as settings.json holds them */
struct Keymap {
    /* command id -> its keys instead of the table's ([] = none) */
    std::map<std::string, std::vector<std::string>> bindings;
    /* the quick-access toolbar's pins, in order */
    std::vector<Pin> pinned;
};

/* "" when `key` is a key string, else why not. The grammar, once: one key
   press, never a sequence: the modifiers that apply in the order Ctrl+, Alt+, Shift+, Meta+ and then one key: a
   capital letter, a digit or other printable ASCII symbol, F1 to F24, or one
   of Enter, Escape, Tab, Backspace, Delete, Insert, Home, End, PageUp,
   PageDown, ArrowLeft, ArrowRight, ArrowUp, ArrowDown, Space. One spelling
   per key ("Ctrl+Shift+S", never "shift+ctrl+s"). */
std::string key_problem(std::string_view key);
/* a key is one of RESERVED_KEYS */
bool is_reserved_key(std::string_view key);

/* the effective keys of command `row`: the user's when map binds it, else the table's defaults */
std::vector<std::string> effective_keys(const Keymap &map, const command::Command &row);

/* A pin of several steps is bound like a command, in `bindings`, by the id `pin "Name"` (the pin's name quoted as JSON text: user text is quoted,
   and no command id has a blank). The one binding table, key grammar, reserved keys and clash check serve commands and pins; a pin of one step is
   bound as its command is. */
inline constexpr std::string_view PIN_BINDING_PREFIX = "pin ";
/* the binding id of pin `name` */
std::string pin_binding_id(std::string_view name);
/* the pin name a binding id names (`pin "Name"`), or nothing when `id` is no pin's */
std::optional<std::string> pin_binding_name(std::string_view id);

/* "" when `name` can name a pin, else why not: not empty, at most MAX_PIN_NAME_BYTES, no control character */
std::string pin_name_problem(std::string_view name);

/* the keymap's two members as settings.json writes them: the bindings object and the pinned array, one line each */
std::string bindings_json(const Keymap &map);
std::string pinned_json(const Keymap &map);

/* The `keymap` object of hello and of the `keymap` event: the load's result
   as the page keeps it (docs/protocol.md "Keymap"). A good load gives
   {"ok":true, ...}; a bad file {"ok":false, "error", "file", "line", ...}
   and the effective keymap of no user differences (the table's defaults,
   nothing pinned), marked by the error and never presented as the user's.
   `path` is settings.json's path ("" when there is no config folder). */
std::string effective_json(std::string_view path, const Result<Keymap> &loaded);

} // namespace xpp::keymap
