#ifndef XPP_KEYMAP_H
#define XPP_KEYMAP_H
/* keymap.json (W211, docs/command-design.md "Settings file"): the user's
   own differences from the command table's defaults, in the per-user config
   folder (files::config_dir). The one owner: how a key is written, what a
   file may hold, loading it all or nothing (W125), writing it, and the
   effective keys the page gets in `hello` and in the `keymap` event
   (docs/protocol.md "Keymap"). C++ only.

   {"preset": "default" | "xppaut",
    "pinned": ["run_id", ...],                  ordered, pinnable commands
    "bindings": {"command_id": ["Ctrl+B"], "other": []}}   [] = no key

   A missing file is all defaults. A bad one is an error with its file, line
   and value, nothing of it applied, and the keymap shown to the page is the
   table's defaults marked as an error; there is no partial or older
   reading. The only path is the config folder's fixed file: nothing the
   file says names another. */
#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp::keymap {

inline constexpr std::string_view FILE_NAME = "keymap.json";
inline constexpr std::string_view PRESET_DEFAULT = "default";
inline constexpr std::string_view PRESET_XPPAUT = "xppaut"; /* the XPPAUT one-letter sequences (hello's legacy_keys) */

/* what a file may hold: a keymap of a user who changed every command is a few
   kilobytes, so this bound is generous and still stops a huge or hostile
   file before it is read whole */
inline constexpr std::size_t MAX_FILE_BYTES = 64 * 1024;
/* the longest key string: two parts of Ctrl+Alt+Shift+Meta+ArrowRight and the space */
inline constexpr std::size_t MAX_KEY_BYTES = 64;
/* keys per command: the command table gives two (a binding and one alternative), the user may add a few */
inline constexpr std::size_t MAX_KEYS_PER_COMMAND = 8;
/* the parts of one key (a chord): XPPAUT's sequences are a layer key then an item key, VS Code's chords two */
inline constexpr std::size_t MAX_CHORD_PARTS = 2;

/* Keys the page and the system keep (docs/command-design.md "Principles" 4):
   Alt+F4, Ctrl+W, Ctrl+Q, F11, F12 and the browser's own tab, window,
   address and reload keys. No command is bound to one, and the page never
   calls preventDefault on them; hello's `keymap.reserved` sends this list.
   Each is a key as key_problem writes it (one part). */
inline constexpr std::array<std::string_view, 16> RESERVED_KEYS = {
    "Alt+F4", "Ctrl+W", "Ctrl+Q", "F11", "F12", "F5", "Ctrl+T", "Ctrl+N", "Ctrl+Shift+N", "Ctrl+Shift+T",
    "Ctrl+Tab", "Ctrl+Shift+Tab", "Alt+ArrowLeft", "Alt+ArrowRight", "Ctrl+L", "Ctrl+Shift+I"};

/* the user's differences, as a file holds them */
struct Keymap {
  /* command id -> its keys instead of the table's ([] = none) */
  std::map<std::string, std::vector<std::string>> bindings;
  /* the quick-access toolbar's pinned commands, in order */
  std::vector<std::string> pinned;
  std::string preset{PRESET_DEFAULT};
};

/* "" when `key` is a key string, else why not. The grammar, once: one to
   MAX_CHORD_PARTS parts separated by one space; a part is the modifiers
   that apply in the order Ctrl+, Alt+, Shift+, Meta+ and then one key: a
   capital letter, a digit or other printable ASCII symbol, F1 to F24, or one
   of Enter, Escape, Tab, Backspace, Delete, Insert, Home, End, PageUp,
   PageDown, ArrowLeft, ArrowRight, ArrowUp, ArrowDown, Space. One spelling
   per key ("Ctrl+Shift+S", never "shift+ctrl+s"). */
std::string key_problem(std::string_view key);
/* a part of a key is one of RESERVED_KEYS (every part of a chord is checked) */
bool is_reserved_key(std::string_view key);

/* The keymap in `text`, the contents of `file` (the name its errors give),
   all or nothing: invalid JSON, a key the file does not define, an unknown
   or unbindable command, a malformed or reserved key, two commands on one
   key (or one key the start of another's chord), a pinned command twice or
   not pinnable, an unknown preset, or anything over a limit above is an
   error at its line, naming the value. */
Result<Keymap> parse_keymap(std::string_view text, std::string file);
/* the file's text for `map`: parse_keymap(serialize(map)) is map */
std::string serialize(const Keymap &map);

/* the effective keys of command `id` (a row of the table): the user's when
   map binds it, else the table's defaults */
std::vector<std::string> effective_keys(const Keymap &map, std::string_view id);

/* the settings file's path, "" when the system names no config folder */
std::string path();
/* the file's keymap: all defaults when the file does not exist; the error
   when it cannot be read or is not a good keymap */
Result<Keymap> load_keymap();
/* map written to the file (the folder made first): temp file, then rename */
Result<> save(const Keymap &map);
/* the file removed (no file is all defaults); an error when it stays */
Result<> reset();

/* The `keymap` object of hello and of the `keymap` event: the load's result
   as the page keeps it (docs/protocol.md "Keymap"). A good load gives
   {"ok":true, ...}; a bad file {"ok":false, "error", "file", "line", ...}
   and the effective keymap of no user differences (the table's defaults,
   preset "default", nothing pinned), marked by the error and never
   presented as the user's. */
std::string effective_json(const Result<Keymap> &loaded);

} // namespace xpp::keymap

#endif
