#pragma once
/* A session's settings read from the text of a file that holds them alone, for the unit tests: `version`, the par and
   init lines, the options block and a session statement of the groups session_settings.h describes. */
#include <string>
#include <string_view>

#include "odex.h"
#include "session_settings.h"

namespace settings_text {

inline xpp::Result<xpp::Settings> read(xpp::Session &s, std::string file, std::string_view text)
{
    xpp::Result<xpp::odex::StateFile> parsed = xpp::odex::read_state_file(xpp::odex::FileKind::session, "session", std::move(file), std::string(text));
    if (!parsed) return std::unexpected(parsed.error());
    try {
        xpp::odex::Block session = parsed->block(true, "the session");
        xpp::Settings f = xpp::read_settings(s, *parsed, session);
        session.done();
        return f;
    } catch (xpp::ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
}

} // namespace settings_text
