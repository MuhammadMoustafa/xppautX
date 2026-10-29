#pragma once

/* How a subscribed event's value arrays are sent (events.subscribe, W528): numbers in the JSON, or base64 of
   little-endian float32. The events a client may subscribe to are event_registry.h's STREAMS. */
#include <array>
#include <string_view>

namespace xpp::event_streams {

inline constexpr std::array<std::string_view, 2> ENCODINGS = {"json", "f32"};
inline constexpr std::string_view ENCODING_F32 = "f32";

} // namespace xpp::event_streams
