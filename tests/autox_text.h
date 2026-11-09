#pragma once
/* autoX's session state as text, for the unit tests: the settings, the views and the whole `autox` group of a
   session statement written and read as the session file's reader reads them (xpp_session_autox.h), each as the entries
   of a file of its own. */
#include <map>
#include <string>
#include <string_view>

#include "odex_block.h"
#include "xpp_session_autox.h"

namespace autox_text {

using namespace xpp::snapx::autox_members;

/* the group `settings` of s as an entry of a file */
inline std::string settings_text(const xpp::AutoxSettingsSet &s)
{
    xpp::odex::Out out;
    write_autox_settings(out, s);
    return *out.result("test");
}
inline xpp::Result<SettingsRead> parse_settings(std::string_view text, std::string file)
{
    return xpp::odex::read_block("autoX's settings", std::move(file), text, true, [](xpp::odex::Block &b) {
        xpp::odex::Block g = *b.group("settings");
        return read_autox_settings(g);
    });
}

/* the views and the active one as the entries of a file */
inline std::string views_text(const SavedViews &v)
{
    xpp::odex::Out out;
    write_views(out, v);
    return *out.result("test");
}
inline xpp::Result<ViewsRead> parse_views(std::string_view text, std::string file)
{
    return xpp::odex::read_block("autoX's views", std::move(file), text, true, [](xpp::odex::Block &b) { return read_views(b); });
}

/* the group `autox` of s as an entry of a file, and the members of it read for s */
inline std::string state_text(const xpp::Session &s)
{
    xpp::odex::Out out;
    write_state(out, s);
    return *out.result("test");
}
inline xpp::Result<Members> read_members(const xpp::Session &s, const std::map<std::string, std::string> &members, std::string_view prefix,
                                         const std::string &name, std::string_view autox, const std::string &file)
{
    return xpp::odex::read_block("session", file, autox, true, [&](xpp::odex::Block &b) { return members_read(s, members, prefix, name, b); });
}

} // namespace autox_text
