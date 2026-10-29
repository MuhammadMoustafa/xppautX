#pragma once

/* The heavy data events a client subscribes to (events.subscribe, W528), by name: hello's `features`, the events.*
   rows' choices and the front end's subscriptions all read this one list. */
#include <array>
#include <string_view>

namespace xpp::event_streams {

inline constexpr std::array<std::string_view, 11> NAMES = {"series", "plots", "nullclines", "dfield", "marks", "ani",
                                                           "autoinfo", "autosettings", "numerics", "windows", "player"};
/* how a subscribed event's value arrays are sent: numbers in the JSON, or base64 of little-endian float32 */
inline constexpr std::array<std::string_view, 2> ENCODINGS = {"json", "f32"};
inline constexpr std::string_view ENCODING_F32 = "f32";

} // namespace xpp::event_streams
