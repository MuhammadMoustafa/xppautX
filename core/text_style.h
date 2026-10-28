#pragma once
/* A plot text's style (W532): normal, bold, italic or bold_italic. The words are the ones our files, the
   protocol and the page use (a choice is named, never a number); the page shows each word by its label
   (choiceLabel: underscores as spaces, a capital first letter). */

#include <array>
#include <optional>
#include <string_view>

#include "xpp_io.h"

namespace xpp {

enum class TextStyle { normal, bold, italic, bold_italic };

inline constexpr std::array<const char *, 4> text_style_names{"normal", "bold", "italic", "bold_italic"};

/* the style a word names, nothing for any other text */
inline std::optional<TextStyle> parse_text_style(std::string_view word)
{
    return enum_from_name<TextStyle>(text_style_names, word);
}

inline const char *text_style_word(TextStyle style) { return text_style_names[static_cast<std::size_t>(style)]; }

} // namespace xpp
