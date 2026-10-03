#pragma once
#include <span>

namespace xpp {
struct WebAsset {
    const char *path, *type;
    std::span<const unsigned char> data;
};
extern const std::span<const WebAsset> web_assets;
extern const std::span<const unsigned char> icon_png;
extern const std::span<const unsigned char> window_lib;
}
