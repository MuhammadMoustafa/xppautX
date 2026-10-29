#pragma once
/* a model's text as its reader takes it: the format version first (format_version.h), so a test writes only what it
   tests. A text that has its own version statement (a converted model) is left as it is. The version is the text's first
   line, so every position a reader reports is one line further. */
#include "format_version.h"
#include <string>
#include <string_view>

inline std::string versioned(std::string_view text)
{
    const std::string word(xpp::version_word);
    if (text.starts_with(word + " ") || text.find("\n" + word + " ") != std::string_view::npos) return std::string(text);
    return word + " " + std::to_string(xpp::format_version) + "\n" + std::string(text);
}

/* an animation's: the version is a statement of its own, `version: 4` */
inline std::string versioned_anix(std::string_view text)
{
    const std::string word(xpp::version_word);
    if (text.starts_with(word + ":")) return std::string(text);
    return word + ": " + std::to_string(xpp::format_version) + "\n" + std::string(text);
}
