/* The `keymap` command and hello's `keymap` (docs/protocol.md "Keymap"): the
   protocol's side of xpp_keymap.h, which owns the file. Window and browser
   mode both come through here, so they read and write the one file. */
#include "ui_json_internal.h"
#include "xpp_keymap.h"

namespace xpp::json {

namespace {

/* what `set` calls the text it checks, in an error's place: the keymap the
   page sent, not the settings file (its lines are the sent object's) */
constexpr std::string_view SENT_KEYMAP = "the keymap sent";

void send_keymap(std::string_view op)
{
    std::string event = "{\"ev\":\"keymap\",\"op\":";
    json_append_string(event, op);
    event += ",\"keymap\":" + hello_keymap() + "}";
    data_emit(event);
}

} // namespace

std::string hello_keymap() { return xpp::keymap::effective_json(xpp::keymap::load_keymap()); }

void keymap_command(xpp::Session &, const char *line)
{
    std::string op;
    get_string(line, "op", op);
    if (op == "get") {
        send_keymap(op);
    } else if (op == "set") {
        const char *map = js_find(line, "map");
        if (!map || *map != '{') {
            j_command_error("keymap", "set needs a map: the whole user keymap as an object");
            return;
        }
        auto parsed = xpp::keymap::parse_keymap(js_raw(map), std::string(SENT_KEYMAP));
        if (parsed) {
            if (auto saved = xpp::keymap::save(*parsed); !saved) parsed = std::unexpected(saved.error());
        }
        if (!parsed) {
            j_err_msg(parsed.error());
            return;
        }
        send_keymap(op);
    } else if (op == "reset") {
        if (auto reset = xpp::keymap::reset(); !reset) {
            j_err_msg(reset.error());
            return;
        }
        send_keymap(op);
    } else {
        j_command_error("keymap", "op is get, set or reset");
    }
}

} // namespace xpp::json
