/* The `keymap` command and hello's `keymap` (docs/protocol.md "Keymap"): the
   protocol's side of xpp_keymap.h, which owns the file. Window and browser
   mode both come through here, so they read and write the one file. */
#include "ui_json_internal.h"
#include "command_def.h"
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

namespace {

/* the user's keymap as the keymap commands answer it: the keys of the commands they changed, the pinned ones, the preset */
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
    return xpp::command::ResultObject().raw("bindings", bindings + '}').raw("pinned", pinned + ']').text("preset", map.preset).done();
}

} // namespace

xpp::Result<std::string> keymap_get()
{
    /* a file that cannot be read is shown to the page by the keymap event (ok false, the error's place); the result is then the defaults */
    const xpp::Result<xpp::keymap::Keymap> loaded = xpp::keymap::load_keymap();
    send_keymap("get");
    return keymap_result(loaded ? *loaded : xpp::keymap::Keymap{});
}

xpp::Result<std::string> keymap_set(const xpp::command::Arguments &args)
{
    /* a file that cannot be read is replaced whole: what the arguments leave out is then the defaults */
    const xpp::Result<xpp::keymap::Keymap> current = xpp::keymap::load_keymap();
    xpp::keymap::Keymap map = current ? *current : xpp::keymap::Keymap{};
    if (args.has("bindings")) {
        map.bindings.clear();
        for (auto &[command, keys] : args.map<std::vector<std::string>>("bindings")) map.bindings.emplace(command, std::move(keys));
    }
    if (args.has("pinned")) map.pinned = args.list<std::string>("pinned");
    if (const std::optional<std::string> preset = args.optional<std::string>("preset")) map.preset = *preset;
    /* checked whole, then written: nothing of an unusable keymap is kept */
    xpp::Result<xpp::keymap::Keymap> checked = xpp::keymap::parse_keymap(xpp::keymap::serialize(map), std::string(SENT_KEYMAP));
    if (checked) {
        if (const xpp::Result<> saved = xpp::keymap::save(*checked); !saved) checked = std::unexpected(saved.error());
    }
    if (!checked) return std::unexpected(checked.error());
    send_keymap("set");
    return xpp::command::ResultObject().boolean("saved", true).done();
}

xpp::Result<std::string> keymap_reset()
{
    if (const xpp::Result<> reset = xpp::keymap::reset(); !reset) return std::unexpected(reset.error());
    send_keymap("reset");
    return xpp::command::ResultObject().boolean("reset", true).done();
}

} // namespace xpp::json
