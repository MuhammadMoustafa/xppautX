#pragma once

/* The one format number (W529, Discussion #415 decision 9): the protocol (hello's `protocol`) and every file of ours
   carry it, and each reader accepts exactly this version: .odex (its first statement, `version 4`), .anix, .setx,
   .snapx, .recx, keymap.json and preferences.setx (a `version` key). Another version is an error naming both versions;
   there is no upgrader and no fallback. 4 is the command migration's; it is reset to 1 once everything settles. A macro
   so the protocol's hello and About can print it at compile time. */
#include "xpp_io.h"
#include <string>
#include <string_view>

#define XPP_FORMAT_VERSION 4

namespace xpp {

inline constexpr int format_version = XPP_FORMAT_VERSION;
/* the word that names the version in a file: a `version 4` statement of .odex and .anix, the key of the others */
inline constexpr std::string_view version_word = "version";

/* why a file of `kind` ("model files", "recordings") that says version `found` is refused */
inline std::string wrong_version(int found, std::string_view kind)
{
    return xpp::format("version {}: this program reads {} of version {}", found, kind, format_version);
}

} // namespace xpp
