/* The `keymap` command and hello's `keymap` and `settings` (docs/protocol.md "Keymap"): the protocol's side of xpp_keymap.h and
   xpp_settings.h, which own the keys and the file. Window and browser mode both come through here, so they read and write the one
   settings.json. */
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "ui_json_internal.h"
#include "command_def.h"
#include "xpp_keymap.h"
#include "xpp_settings.h"
#include "event_registry.h"
#include "xpp_error.h"
#include "xpp_fs.h"
#include "session.h"
#include "model.h"

namespace xpp::json {

namespace {

void send_keymap(xpp::Session &s, std::string_view op)
{
    std::string event = xpp::events::open(xpp::events::KEYMAP_CHANGED) + ",\"op\":";
    json_append_string(event, op);
    event += ",\"keymap\":" + hello_keymap(s.model()) + "}";
    data_emit(event);
}

/* the user's keymap as settings.json holds it: the error when the file is bad */
xpp::Result<xpp::keymap::Keymap> load_map(xpp::FileSystem &fs)
{
    const xpp::Result<xpp::settings::Settings> loaded = xpp::settings::load_settings(fs);
    if (!loaded) return std::unexpected(loaded.error());
    return loaded->keymap;
}

} // namespace

std::string hello_keymap(const xpp::Model &m) { return xpp::keymap::effective_json(xpp::settings::settings_path(*m.fs), load_map(*m.fs)); }

std::string hello_settings(const xpp::Model &m) { return xpp::settings::hello_json(*m.fs, xpp::settings::load_settings(*m.fs)); }

namespace {

/* the user's keymap as the keymap commands answer it: the keys of the commands they changed, and the pinned ones */
std::string keymap_result(const xpp::keymap::Keymap &map)
{
    std::string bindings = "{";
    for (const auto &[command, keys] : map.bindings) {
        if (bindings.size() > 1) bindings += ',';
        json_append_string(bindings, command);
        bindings += ":[";
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (i) bindings += ',';
            json_append_string(bindings, keys[i]);
        }
        bindings += ']';
    }
    std::string pinned = "[";
    for (std::size_t i = 0; i < map.pinned.size(); ++i) {
        if (i) pinned += ',';
        json_append_string(pinned, map.pinned[i]);
    }
    return xpp::command::ResultObject().raw("bindings", bindings + '}').raw("pinned", pinned + ']').done();
}

} // namespace

xpp::Result<std::string> keymap_get(xpp::Session &s)
{
    /* a file that cannot be read is shown to the page by the keymap event (ok false, the error's place); the result is then the defaults */
    const xpp::Result<xpp::keymap::Keymap> loaded = load_map(*s.model().fs);
    send_keymap(s, "get");
    return keymap_result(loaded ? *loaded : xpp::keymap::Keymap{});
}

xpp::Result<std::string> keymap_set(xpp::Session &s, const xpp::command::Arguments &args)
{
    /* a file that cannot be read is the error: the keymap is not replaced in a settings file that is bad */
    const xpp::Result<xpp::keymap::Keymap> current = load_map(*s.model().fs);
    if (!current) return std::unexpected(current.error());
    xpp::keymap::Keymap map = *current;
    if (args.has("bindings")) {
        map.bindings.clear();
        for (auto &[command, keys] : args.map<std::vector<std::string>>("bindings")) map.bindings.emplace(command, std::move(keys));
    }
    if (args.has("pinned")) map.pinned = args.list<std::string>("pinned");
    /* checked whole, then written: nothing of an unusable keymap is kept */
    if (const xpp::Result<> saved = xpp::settings::set_keymap(*s.model().fs, map); !saved) return std::unexpected(saved.error());
    send_keymap(s, "set");
    return xpp::command::ResultObject().boolean("saved", true).done();
}

xpp::Result<std::string> keymap_reset(xpp::Session &s)
{
    if (const xpp::Result<> reset = xpp::settings::set_keymap(*s.model().fs, xpp::keymap::Keymap{}); !reset) return std::unexpected(reset.error());
    send_keymap(s, "reset");
    return xpp::command::ResultObject().boolean("reset", true).done();
}

} // namespace xpp::json
